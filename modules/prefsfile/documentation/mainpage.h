/** @mainpage Prefsfile module

This is an auto-generated documentation of the prefsfile module.
For more information about this module, please refer to the
<a href="http://wiki.oslo.opera.com/developerwiki/index.php/Prefsfile_module">Wiki</a>
page.

@section api API

@subsection prefsfile PrefsFile

PrefsFile is the interface describing how to read and write preferences,
storing preferences in files using PrefsAccessor. It is also be used for
similar formats, for instance for reading language files.

@subsection prefsaccessor PrefsAccessor

PrefsAccessor is an abstract interface describing a method to read and write
a file containing preference data.

IniAccessor implements this interface to support the extended Windows INI
file format normally used by Opera.

XmlAccessor implements support for reading and writing preferences in an
XML based format.

LangAccessor implements support for reading language files used by Opera's
localization support.

Other file formats can easily be implemented by subclassing PrefsAccessor.

@author Peter Karlsson <peter@opera.com>
*/
