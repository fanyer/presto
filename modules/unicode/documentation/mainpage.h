/** @mainpage Unicode module

This is an auto-generated documentation of the Unicode module.
For more information about this module, please refer to the
<a href="http://wiki.oslo.opera.com/developerwiki/index.php/Modules/unicode">Wiki</a>
page.

@section api API

@subsection Unicode Namespace

The API of this module is exposed through the Unicode namespace.
The namespace is implemented as a class with several static methods
since not all compilers used to compile Opera can handle C++ namespaces.

APIs:

- Normalization: Unicode::Normalize()
- Case conversion: Unicode::Lower() and Unicode::Upper()
- Linebreaking: Unicode::IsLinebreakOpportunity()
- Character properties: Unicode::IsCSSPunctuation(),
  Unicode::IsUpper(), Unicode::IsLower(), Unicode::IsCntrl(),
  Unicode::IsSpace(), Unicode::IsAlpha(), Unicode::IsDigit(),
  Unicode::IsUniDigit(), Unicode::IsAlphaOrDigit() and
  Unicode::GetCharacterClass()
- Bidirectionality: Unicode::GetBidiCategory(),
  Unicode::IsMirrored(), Unicode::GetMirrorChar() and
  BidiCalculation
- Segmentation: UnicodeSegmenter and
  UnicodeSegmenter::IsGraphemeClusterBoundary()
- Scripts: Unicode::GetScriptType()
  

@section initdeinit Initialization and de-initalization

The data for the Unicode namespace resides in the program image, so no
special initialization or de-initialization needs to take place.

@author Peter Krefting <peter@opera.com>
*/
