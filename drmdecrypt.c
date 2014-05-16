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

/*
 * Decode a MPEG packet
 *
 * Transport Stream Header:
 * ========================
 *
 * Name                    | bits | byte msk | Description
 * ------------------------+------+----------+-----------------------------------------------
 * sync byte               | 8    | 0xff     | Bit pattern from bit 7 to 0 as 0x47
 * Transp. Error Indicator | 1    | 0x80     | Set when a demodulator cannot correct errors from FEC data
 * Payload Unit start ind. | 1    | 0x40     | Boolean flag with a value of true means the start of PES
 *                         |      |          | data or PSI otherwise zero only.
 * Transport Priority      | 1    | 0x20     | Boolean flag with a value of true means the current packet
 *                         |      |          | has a higher priority than other packets with the same PID.
 * PID                     | 13   | 0x1fff   | Packet identifier
 * Scrambling control      | 2    | 0xc0     | 00 = not scrambled
 *                         |      |          | 01 = Reserved for future use (DVB-CSA only)
 *                         |      |          | 10 = Scrambled with even key (DVB-CSA only)
 *                         |      |          | 11 = Scrambled with odd key (DVB-CSA only)
 * Adaptation field exist  | 1    | 0x20     | Boolean flag
 * Contains payload        | 1    | 0x10     | Boolean flag
 * Continuity counter      | 4    | 0x0f     | Sequence number of payload packets (0x00 to 0x0F)
 *                         |      |          | Incremented only when a playload is present
 *
 * Adaptation Field:
 * ========================
 *
 * Name                    | bits | byte msk | Description
 * ------------------------+------+----------+-----------------------------------------------
 * Adaptation Field Length | 8    | 0xff     | Number of bytes immediately following this byte
 * Discontinuity indicator | 1    | 0x80     | Set to 1 if current TS packet is in a discontinuity state
 * Random Access indicator | 1    | 0x40     | Set to 1 if PES packet starts a video/audio sequence
 * Elementary stream prio  | 1    | 0x20     | 1 = higher priority
 * PCR flag                | 1    | 0x10     | Set to 1 if adaptation field contains a PCR field
 * OPCR flag               | 1    | 0x08     | Set to 1 if adaptation field contains a OPCR field
 * Splicing point flag     | 1    | 0x04     | Set to 1 if adaptation field contains a splice countdown field
 * Transport private data  | 1    | 0x02     | Set to 1 if adaptation field contains private data bytes
 * Adapt. field extension  | 1    | 0x01     | Set to 1 if adaptation field contains extension
 * Below fields optional   |      |          | Depends on flags
 * PCR                     | 33+6+9 |        | Program clock reference
 * OPCR                    | 33+6+9 |        | Original Program clock reference
 * Splice countdown        | 8    | 0xff     | Indicates how many TS packets from this one a splicing point
 *                         |      |          | occurs (may be negative)
 * Stuffing bytes          | 0+   |          |
 *
 *
 * See: http://en.wikipedia.org/wiki/MPEG_transport_stream
 */
int decode_packet(unsigned char *data, unsigned char *outdata)
{
   unsigned char iv[0x10];
   unsigned char *inbuf;
   unsigned int i, n;
   unsigned char *outbuf;
   int offset, rounds;
   int scrambling, adaptation;

   if(data[0] != 0x47)
   {
      trace(TRC_ERROR, "Not a valid MPEG packet!");
      return 0;
   }

   memcpy(outdata, data, 188);

   scrambling = data[3] & 0xC0;
   adaptation = data[3] & 0x20;

   if(scrambling == 0x80)
   {
      trace(TRC_DEBUG, "packet scrambled with even key");
   }
   else if(scrambling == 0xC0)
   {
      trace(TRC_DEBUG, "packet scrambled with odd key");
   }
   else if(scrambling == 0x00)
   {
      trace(TRC_DEBUG, "packet not scrambled");
      return 0;
   }
   else
   {
      trace(TRC_ERROR, "scrambling info seems to be invalid!");
      return 0;
   }

   offset=4;

   /* skip adaption field */
   if(adaptation)
      offset += (data[4]+1);

   /* remove scrambling bits */
   outdata[3] &= 0x3f;

   inbuf  = data + offset;
   outbuf = outdata + offset;
		
   rounds = (188 - offset) / 0x10;
   /* AES CBC */
   memset(iv, 0, 16);
   for (i = 0; i < rounds; i++)
   {
      unsigned char *out = outbuf + i * 0x10;

      for(n = 0; n < 16; n++)
         out[n] ^= iv[n];

      aes_decrypt_128(inbuf + i * 0x10, outbuf + i * 0x10, drmkey);
      memcpy(iv, inbuf + i * 0x10, 16);
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

      /* search 188byte packets starting with 0x47 */
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
            decode_packet(buf, outdata);
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

