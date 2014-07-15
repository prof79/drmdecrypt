drmdecrypt
==========

## Synopsis

drmdecrypt is a tool to decrypt recorded files from Samsung TVs
into standard transport stream format. There are multiple similar
versions out there based on code from SamyGO and various people
but they are all either slow, Windows specific, full of bugs or
even all together.

This version is fast, POSIX compliant (Linux, FreeBSD, Windows
(MinGW)) and is focused on a small number of useful features.

## Features
- Reading title and channel from .inf file
- Bulk decoding multiple files
- AES-NI support (5x faster)


## Usage

```
Usage: drmdecrypt [-dqvx][-o outdir] infile.srf ...
Options:
   -d         Show debugging output
   -o outdir  Output directory
   -q         Be quiet. Only error output.
   -v         Version information
   -x         Disable AES-NI support
```


## Building / Installing

```
make
make install
```

## Tested with
- Samsung Series C, D

## TODO

- Test with Samsung Series E, F

