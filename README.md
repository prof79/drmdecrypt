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


## Usage

```
drmdecrypt [-x] [-o outdir] infile.srf
```


## Building

```
make
```


## Installing

```
make install
```


## TODO

- test AESNI support
- flag to specify verbosity level
- test resyncing
- MPEG packet size could also be 204 or 208 byte

