@@ifdef x86_64
 @@define REQSUFFIX ()(64bit)
@@else
 @@define REQSUFFIX
@@endif
%define _use_internal_dependency_generator 0
%define __find_requires @@{CONF:temp}/rpmbuild/find-requires
%define _binary_payload w9.lzdio
Name: @@{CONF:product}
Summary: @@{S_ONELINE_DESC}
Version: @@{CONF:version}
Release: @@{CONF:build}
Epoch: 2
Group: Applications/Internet
Vendor: @@{S_VENDOR}
Packager: @@{S_MAINTAINER}
URL: @@{S_HOMEPAGE}
License: Proprietary
Icon: opera@@{SUFFIX}-browser.xpm
Requires: libgstautodetect.so@@{REQSUFFIX}
Requires: libgstogg.so@@{REQSUFFIX}
Requires: libgsttheora.so@@{REQSUFFIX}
Requires: libgstvorbis.so@@{REQSUFFIX}
Requires: libgstwavparse.so@@{REQSUFFIX}
%description
@@include ../doc/description.inc
%files
%defattr(-,root,root)
@@{FILES:subtree:UNIX_BIN}
@@{FILES:subtree:UNIX_DESKTOP}
@@{FILES:subtree:UNIX_ICONS}
@@{FILES:subtree:UNIX_MIME}
@@{ABS:UNIX_DOC}
@@{ABS:UNIX_LIB}
@@{ABS:RESOURCES}
%doc
@@{FILES:subtree:UNIX_MAN}

%changelog
* @@{TIME:%a %b %d %Y} @@{S_MAINTAINER} 2:@@{CONF:version}-@@{CONF:build}
- @@{S_CHANGELOG_URL}

%build
# This empty section says there's nothing to be done to build this package from source
%clean
# This empty section prevents rpmbuild from deleting the buildroot directory and causing a race condition with tar

%post
@@include ../scripts/rpm.post
%postun
@@include ../scripts/rpm.postun
