#
# This Makefile requires GNU Make
#

CC	?= cc
CFLAGS	+= -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -maes

INSTALL	= install -c
STRIP	= strip
PREFIX	= /usr/local
BINDIR	= $(PREFIX)/bin
VERSION	= 1.0-beta

# add git revision if .git exists
ifeq (,$(wildcard .git))
CFLAGS	+= -DREVISION="unknown"
else
CFLAGS	+= -DREVISION="$(shell git rev-parse --short HEAD)"
endif

ifeq ($(DEBUG),1)
# for debugging
CFLAGS	+= -g -Wall -Werror
LDFLAGS	+= -pg
else
# for release
CFLAGS	+= -O2
endif

# we need to link to libc/msvcrt
ifeq ($(OS),Windows_NT)
CFLAGS += -DHAVE__ALIGNED_MALLOC
LDFLAGS	+= -lmsvcrt
RELDIR	= drmdecrypt-$(VERSION)-win
else
CFLAGS += -DHAVE_POSIX_MEMALIGN
LDFLAGS	+= -lc
RELDIR	= drmdecrypt-$(VERSION)
endif

##########################

SRC	= AES.c AESNI.c drmdecrypt.c
OBJS	= AES.o AESNI.o drmdecrypt.o

all:	drmdecrypt

drmdecrypt:	$(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

install:	all
	$(STRIP) drmdecrypt
	$(INSTALL) drmdecrypt $(BINDIR)/drmdecrypt

release-win:	all
	rm -rf $(RELDIR)
	mkdir $(RELDIR)
	cp LICENSE README.md drmdecrypt.exe $(RELDIR)
	$(STRIP) $(RELDIR)/*.exe

clean:
	rm -f *.o *.core drmdecrypt drmdecrypt.exe
	rm -rf $(RELDIR)

