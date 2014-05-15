CC	?= cc
CFLAGS	+= -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
LDFLAGS	+= -lc

INSTALL	= install -c
STRIP	= strip
PREFIX	= /usr/local
BINDIR	= $(PREFIX)/bin

# for debugging
#CFLAGS	+= -g -Wall

# for release
CFLAGS	+= -O2

##########################

SRC	= aes.c drmdecrypt.c
OBJS	= aes.o drmdecrypt.o

all:	drmdecrypt

drmdecrypt:	$(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

install:	all
	$(STRIP) drmdecrypt
	$(INSTALL) drmdecrypt $(BINDIR)/drmdecrypt

clean:
	rm -f *.o *.core drmdecrypt

