drmdecrypt
==========

## Synopsis

drmdecrypt is a tool to decrypt recorded files from Samsung TVs
into standard transport stream format. There are multiple similar
versions out there based on code from SamyGO and various people
but they are all either slow, Windows specific, full of bugs or
even all together.

This version is fast, POSIX compliant, has a sane codebase and is
focused on a small number of useful features.


## Usage

drmdecrypt [-o <outfile.ts>] <infile.srf>


## Building

make


## Installing

make install


## TODO

- specify outdir instead of outfile
- flag to specify verbosity level
- test resyncing
- build on windows (low priority)

