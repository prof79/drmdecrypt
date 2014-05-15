/* drmdecrypt -- DRM decrypting tool for Samsung TVs
 *
 * Copyright (C) 2014 - Bernhard Froehlich <decke@bluelife.at>
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the GPL v2 license.  See the LICENSE file for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <limits.h>
#include <unistd.h>

#include "aes.h"
#include "trace.h"


unsigned char drmkey[0x10];


char *filename(char *path, char *newsuffix)
{
   char *end = path + strlen(path);

   while(*end != '.' && *end != '/')
      --end;

   if(newsuffix != NULL)
      strcpy(++end, newsuffix);
   else
      *end = '\0';

   return path;
}

int hasPESHeader(unsigned char *pBuf)
{
   unsigned char  *p;
   unsigned char  u8AFC;

   /*
    2  AFC   Adaption Field Control
     1. 01 - no adaptation field, payload only
     2. 10 - adaptation field only, no payload
     3. 11 - adaptation field followed by payload
     4. 00 - RESERVED for future use
   */
   u8AFC = pBuf[3] >> 4 & 0x03;
   p = &pBuf[4];

   switch (u8AFC) 
   {
      case 0x11:
         /* 188-4-1-3 */
         if (*p < 181) {
            /* skipping adapt field. points now to 1st payload byte */
            p += *p + 1;
         }
         else {
            trace(TRC_ERROR, "Adaptation Field is too long!. No space left for PES header.");
            return 0;
         }
         /* fall thru */

      case 0x01:
         trace(TRC_DEBUG, "FRAME HEADER: %02x %02x %02x", *p, *(p+1), *(p+2));
         if (memcmp(p, "\x00\x00\x01", 3)) {
            trace(TRC_WARN, "no PES header found");
            return 0;
         }
         else {
            /* PES header found in packet */
            return 1;
         }

      case 0x10:
         /* this should never happen as we have pusi packets here only */
         trace(TRC_ERROR, "Packet has adapt field but no payload!");
         return 0;

      default:
         /* 00 is reserved and 01 should never happen as we have pusi packets here only */
         trace(TRC_ERROR, "Illegal adaptation field value");
         return 0;
    }
}

int readdrmkey(char *mdbfile)
{
   char tmpbuf[64];
   unsigned int j;
   FILE *mdbfp;

   memset(tmpbuf, '\0', sizeof(tmpbuf));

   if((mdbfp = fopen(mdbfile, "rb")))
   {
      fseek(mdbfp, 8, SEEK_SET);
      for (j = 0; j < 0x10; j++){
         fread(&drmkey[(j&0xc)+(3-(j&3))], sizeof(unsigned char), 1, mdbfp);
      }
      fclose(mdbfp);

      for (j = 0; j < sizeof(drmkey); j++)
         sprintf(tmpbuf+strlen(tmpbuf), "%02X ", drmkey[j]);

      trace(TRC_INFO, "drm key successfully read from %s", basename(mdbfile));
      trace(TRC_INFO, "KEY: %s", tmpbuf);

      return 0;
   }
   else
      trace(TRC_ERROR, "mdb file %s not found", basename(mdbfile));

   return 1;
}

int genoutfilename(char *outfile, char *inffile)
{
   FILE *inffp;
   unsigned char inf[0x200];
   char tmpname[PATH_MAX];
   int i;

   if((inffp = fopen(inffile, "rb")))
   {
      fseek(inffp, 0, SEEK_SET);
      fread(inf, sizeof(unsigned char), 0x200, inffp);
      fclose(inffp);

      /* build base path */
      strcpy(tmpname, inffile);
      filename(tmpname, NULL);
      strcat(tmpname, "-");
      
      /* http://code.google.com/p/samy-pvr-manager/wiki/InfFileStructure */

      /* copy channel name and program title */
      for(i=1; i < 0x200; i += 2)
      {
         if (inf[i])
         {
            if((inf[i] >= 'A' && inf[i] <= 'z') || (inf[i] >= '0' && inf[i] <= '9'))
               strncat(tmpname, (char*)&inf[i], 1);
            else
               strcat(tmpname, "_");
         }
         if (i == 0xFF) {
            strcat(tmpname, "_-_");
         }
      }

      strcat(tmpname, ".ts");

      strcpy(outfile, tmpname);
   }
   else
      return 1;

   return 0;
}

int decode_frame(unsigned char *data , unsigned char *outdata)
{
   unsigned char iv[0x10];
   unsigned char *inbuf;
   unsigned int i, n;
   unsigned char *outbuf;

   memcpy(outdata, data, 188);
   if((data[3]&0xC0) == 0x00)
   {
      trace(TRC_DEBUG, "frame not scrambled");
      return 0;
   }
   if(!((data[3]&0xC0) == 0xC0 || (data[3]&0xC0) == 0x80))
   {
      trace(TRC_ERROR, "frame seems to be invalid!");
      return 0;
   }

   int offset=4;

   /* remove scrambling bits */
   outdata[3] &= 0x3f;

   inbuf  = data + offset;
   outbuf = outdata + offset;
		
   int rounds = (188 - offset) / 0x10;
   /* AES CBC */
   memset(iv, 0, 16);
   for (i =  0; i < rounds; i++)
   {
      unsigned char *out = outbuf + i* 0x10;

      for(n = 0; n < 16; n++)
         out[n] ^= iv[n];

      aes_decrypt_128(inbuf + i* 0x10, outbuf + i * 0x10, drmkey);
      memcpy(iv, inbuf + i * 0x10, 16);
   }

   /* is this a pusi packet? */
   if ((outdata[1] & 0x40) == 0x40)
   {
      /* yes, and it should start with the mpeg pes header 0x00 0x00 0x01 ... */
      if (!hasPESHeader(outdata))
         trace(TRC_DEBUG, "expected packet payload 00 00 01 not found. drm key wrong?");
   }

   return 1;		
}

void usage(void)
{
   fprintf(stderr, "Usage: drmdecrypt [-o outfile] infile.srf\n");
}

int main(int argc, char *argv[])
{
   char mdbfile[PATH_MAX];
   char inffile[PATH_MAX];
   char srffile[PATH_MAX];
   char outfile[PATH_MAX];
   FILE *srffp, *outfp;
   int ch, retries;

   int sync_find = 0;
   unsigned long filesize = 0, foffset = 0;
   unsigned long i;
   unsigned char buf[1024];
   unsigned char outdata[1024];

   memset(outfile, '\0', sizeof(outfile));

   while ((ch = getopt(argc, argv, "o:")) != -1)
   {
      switch (ch)
      {
         case 'o':
            strcpy(outfile, optarg);
            break;
         default:
            usage();
            exit(EXIT_FAILURE);
      }
   }

   if(argc == optind)
   {
      usage();
      exit(EXIT_FAILURE);
   }

   strcpy(srffile, argv[optind]);

   strcpy(inffile, srffile);
   filename(inffile, "inf");

   strcpy(mdbfile, srffile);
   filename(mdbfile, "mdb");

   /* read drm key from .mdb file */
   if(readdrmkey(mdbfile) != 0)
      return 1;

   if(strlen(outfile) == 0)
   {
      /* generate outfile name based on title from .inf file */
      if(genoutfilename(outfile, inffile) != 0)
      {
         strcpy(outfile, srffile);
         filename(outfile, "ts");
      }
   }

   trace(TRC_INFO, "Writing to %s", outfile);

   if((outfp = fopen(outfile, "wb")) == NULL)
   {
      trace(TRC_ERROR, "Cannot open %s for writing", outfile);
      return 1;
   }

   if((srffp = fopen(srffile, "rb")) == NULL)
   {
      trace(TRC_ERROR, "Cannot open %s for reading", srffile);
   }
	

   /* calculate filesize */
   fseek(srffp, 0, 2); 
   filesize = ftell(srffp); 
   rewind(srffp);

   trace(TRC_INFO, "Filesize %ld", filesize);

resync:

   /* try to sync */
   sync_find = 0;
   retries = 10;
   fseek(srffp, foffset, SEEK_SET);

   while(sync_find == 0 && retries-- > 0)
   {
      fread(buf, sizeof(unsigned char), sizeof(buf), srffp);

      /* search 188byte frames starting with 0x47 */
      for(i=0; i < (sizeof(buf)-188-188); i++)
      {
         if (buf[i] == 0x47 && buf[i+188] == 0x47 && buf[i+188+188] == 0x47)
         {
            sync_find = 1;
            foffset += i;
            fseek(srffp, foffset, SEEK_SET);

            trace(TRC_INFO, "synced at offset %ld", foffset);

            break;
         }
      }
   }

   if (sync_find)
   {
      for(i=0; foffset+i < filesize; i+= 188)
      {
         fread(buf, sizeof(unsigned char), 188, srffp);

         if (buf[0] == 0x47)
         {
            decode_frame(buf, outdata);
            fwrite(outdata, sizeof(unsigned char), 188, outfp);
         }
         else
         {
            foffset += i;
            trace(TRC_WARN, "lost sync at %ld", foffset);

            goto resync;
         }
      }
   }

   fclose(srffp);
   fclose(outfp);

   return 0;
}

