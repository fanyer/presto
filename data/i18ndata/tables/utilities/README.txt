This directory contains utility scripts that are not used by the
chartables.bin generator scripts.


gettablelist.pl

  List (and possibly extract) the contents of a chartables.bin file.
  Can also be used to convert chartables.bin to stand-alone table files,
  C++ based tables used for the ROM table manager, and/or the new
  encoding.bin file format.

surrogate.pl

  Converts a Unicode codepoint into a pair of surrogates. Parameter is the
  codepoint expressed as a decimal or hexadecimal (with 0x prefix).

unpack_cns11643.pl

  Converts the packed representation of a CNS 11643 (EUC-TW) codepoint into
  its components. The packed representation is used in the conversion tables
  to reduce used space.

maketest.pl

  Creates test web pages for the encodings, cross-referencing them with
  their Unicode code points.
