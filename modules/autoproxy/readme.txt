TO-DO list for the autoproxy module.

- Several reported bugs in the BTS, search on [AUTOPROXY] in summary

- Performance assessment: how do we measure the impact of the
  autoproxy functionality relative to not using autoproxy, and is the
  difference acceptable?  The APC may be slower at the first page
  loaded because the fetching of the script (especially from HTTP)
  creates a bottleneck in the system.  We wish to verify that this
  bottleneck is acceptable.

- User-friendliness: It ought to be possible to restart the autoproxy
  facility after an error that disables it.

- Coding standards:
   * GCC warnings
   * Documentation requirements

- The footprint with the linear_b_1 ECMAScript module is large because
  of the string representations for the libraries.  This has been
  fixed for linear_b_2 by including the bytecoded program
  representation in the program instead; for newer versions of the
  ECMAScript module than linear_b_2, separate compiled representations
  may have to be created.

- General structure: lth believes the autoproxy module should be
  removed from Opera, or nearly so.  The URL_LoadHandler specific
  functionality should be moved into the URL module, and the script
  handling (exclusive of script downloading) should be supported
  directly by the ecmascript_util module, probably through the use of
  the ES-Async interface in that module.  The support libraries
  (written in ECMAScript could either be moved into the URL code or
  would exist as the last remains of the autoproxy module.)

$Id$
