@@# -*- mode: conf -*-

@@ifdef FreeBSD
 @@define ETC /usr/local/etc
@@else
 @@define ETC /etc
@@endif

@@define TRADITIONAL_ARCH @@{CONF:arch}
@@ifdef x86_64
 @@ifdef deb
  @@define TRADITIONAL_ARCH amd64
 @@elifdef FreeBSD
  @@define TRADITIONAL_ARCH amd64
 @@endif
@@endif

@@ifdef deb
 @@define LICENSE_FILE copyright
@@else
 @@define LICENSE_FILE LICENSE
@@endif

@@define LIB64 lib
@@ifdef x86_64
 @@ifdef rpm
  @@define LIB64 lib64
 @@endif
@@endif

@@include doc/strings.inc
