
const UINT gDTDlenmin = 19;
const UINT gDTDlenmax = 78;

const UINT gDTDlengths[] =
{
	0, // len = 19
	0, // len = 20
	2, // len = 21
	3, // len = 22
	6, // len = 23
	7, // len = 24
	8, // len = 25
	9, // len = 26
	13, // len = 27
	15, // len = 28
	17, // len = 29
	18, // len = 30
	23, // len = 31
	25, // len = 32
	25, // len = 33
	30, // len = 34
	33, // len = 35
	37, // len = 36
	37, // len = 37
	40, // len = 38
	40, // len = 39
	42, // len = 40
	42, // len = 41
	46, // len = 42
	47, // len = 43
	47, // len = 44
	48, // len = 45
	50, // len = 46
	50, // len = 47
	52, // len = 48
	52, // len = 49
	52, // len = 50
	53, // len = 51
	55, // len = 52
	57, // len = 53
	57, // len = 54
	57, // len = 55
	57, // len = 56
	57, // len = 57
	58, // len = 58
	59, // len = 59
	59, // len = 60
	59, // len = 61
	59, // len = 62
	59, // len = 63
	59, // len = 64
	59, // len = 65
	59, // len = 66
	59, // len = 67
	59, // len = 68
	60, // len = 69
	60, // len = 70
	60, // len = 71
	60, // len = 72
	60, // len = 73
	60, // len = 74
	60, // len = 75
	60, // len = 76
	60, // len = 77
	61, // len = 78
	61 // len > 78
};

CONST_ARRAY(gDTDStrings, char*)
	CONST_ENTRY("-//IETF//DTD HTML//"), // len = 19, idx = 0
	CONST_ENTRY("-//IETF//DTD HTML 3//"), // len = 21, idx = 1
	CONST_ENTRY("-//W3C//DTD W3 HTML//"),
	CONST_ENTRY("-//W3C//DTD HTML 3.2//"), // len = 22, idx = 3
	CONST_ENTRY("-//IETF//DTD HTML 2.0//"), // len = 23, idx = 4
	CONST_ENTRY("-//IETF//DTD HTML 3.0//"),
	CONST_ENTRY("-//IETF//DTD HTML 3.2//"),
	CONST_ENTRY("-//IETF//DTD HTML 2.1E//"), // len = 24, idx = 7
	CONST_ENTRY("-//W3O//DTD W3 HTML 3.0//"), // len = 25, idx = 8
	CONST_ENTRY("-//IETF//DTD HTML Strict//"), // len = 26, idx = 9
	CONST_ENTRY("-//IETF//DTD HTML Level 0//"), // len = 27, idx = 10
	CONST_ENTRY("-//IETF//DTD HTML Level 1//"),
	CONST_ENTRY("-//IETF//DTD HTML Level 2//"),
	CONST_ENTRY("-//IETF//DTD HTML Level 3//"),
	CONST_ENTRY("-//W3C//DTD HTML 3.2 Draft//"), // len = 28, idx = 14
	CONST_ENTRY("-//W3C//DTD HTML 3.2 Final//"),
	CONST_ENTRY("-//W3C//DTD HTML 3.2S Draft//"), // len = 29, idx = 16
	CONST_ENTRY("-//IETF//DTD HTML 3.2 Final//"),
	CONST_ENTRY("-//IETF//DTD HTML 2.0 Strict//"), // len = 30, idx = 18
	CONST_ENTRY("-//W3C//DTD HTML 3 1995-03-24//"), // len = 31, idx = 19
	CONST_ENTRY("-//W3C//DTD HTML 4.0 Frameset//"),
	CONST_ENTRY("-//WebTechs//DTD Mozilla HTML//"),
	CONST_ENTRY("-//IETF//DTD HTML 2.0 Level 1//"),
	CONST_ENTRY("-//IETF//DTD HTML 2.0 Level 2//"),
	CONST_ENTRY("-//W3C//DTD HTML 4.01 Frameset//"), // len = 32, idx = 24
	CONST_ENTRY("-//W3C//DTD XHTML 1.0 Frameset//"),
	CONST_ENTRY("-//IETF//DTD HTML Strict Level 0//"), // len = 34, idx = 26
	CONST_ENTRY("-//IETF//DTD HTML Strict Level 1//"),
	CONST_ENTRY("-//IETF//DTD HTML Strict Level 2//"),
	CONST_ENTRY("-//IETF//DTD HTML Strict Level 3//"),
	CONST_ENTRY("-/W3C/DTD HTML 4.0 Transitional/EN"),
	CONST_ENTRY("-//Netscape Comm. Corp.//DTD HTML//"), // len = 35, idx = 31
	CONST_ENTRY("-//WebTechs//DTD Mozilla HTML 2.0//"),
	CONST_ENTRY("-//W3C//DTD HTML 4.0 Transitional//"),
	CONST_ENTRY("-//Spyglass//DTD HTML 2.0 Extended//"), // len = 36, idx = 34
	CONST_ENTRY("-//W3C//DTD HTML 4.01 Transitional//"),
	CONST_ENTRY("-//W3C//DTD XHTML 1.0 Transitional//"),
	CONST_ENTRY("-//W3O//DTD W3 HTML Strict 3.0//EN//"),
	CONST_ENTRY("-//W3C//DTD HTML Experimental 970421//"), // len = 38, idx = 38
	CONST_ENTRY("-//IETF//DTD HTML 2.0 Strict Level 1//"),
	CONST_ENTRY("-//IETF//DTD HTML 2.0 Strict Level 2//"),
	CONST_ENTRY("-//Metrius//DTD Metrius Presentational//"), // len = 40, idx = 41
	CONST_ENTRY("-//W3C//DTD HTML Experimental 19960712//"),
	CONST_ENTRY("+//Silmaril//dtd html Pro v0r11 19970101//"), // len = 42, idx = 43
	CONST_ENTRY("-//AS//DTD HTML 3.0 asWedit + extensions//"),
	CONST_ENTRY("-//Netscape Comm. Corp.//DTD Strict HTML//"),
	CONST_ENTRY("-//O'Reilly and Associates//DTD HTML 2.0//"),
	CONST_ENTRY("-//SQ//DTD HTML 2.0 HoTMetaL + extensions//"), // len = 43, idx = 47
	CONST_ENTRY("-//Sun Microsystems Corp.//DTD HotJava HTML//"), // len = 45, idx = 48
	CONST_ENTRY("-//Microsoft//DTD Internet Explorer 2.0 HTML//"), // len = 46, idx = 49
	CONST_ENTRY("-//Microsoft//DTD Internet Explorer 3.0 HTML//"),
	CONST_ENTRY("-//Microsoft//DTD Internet Explorer 2.0 Tables//"), // len = 48, idx = 51
	CONST_ENTRY("-//Microsoft//DTD Internet Explorer 3.0 Tables//"),
	CONST_ENTRY("-//O'Reilly and Associates//DTD HTML Extended 1.0//"), // len = 51, idx = 53
	CONST_ENTRY("-//Sun Microsystems Corp.//DTD HotJava Strict HTML//"), // len = 52, idx = 54
	CONST_ENTRY("-//AdvaSoft Ltd//DTD HTML 3.0 asWedit + extensions//"),
	CONST_ENTRY("-//Microsoft//DTD Internet Explorer 2.0 HTML Strict//"), // len = 53, idx = 56
	CONST_ENTRY("-//Microsoft//DTD Internet Explorer 3.0 HTML Strict//"),
	CONST_ENTRY("http://www.ibm.com/data/dtd/v11/ibmxhtml1-transitional.dtd"), // len = 58, idx = 58
	CONST_ENTRY("-//O'Reilly and Associates//DTD HTML Extended Relaxed 1.0//"), // len = 59, idx = 59
	CONST_ENTRY("-//SoftQuad//DTD HoTMetaL PRO 4.0::19971010::extensions to HTML 4.0//"), // len = 69, idx = 60
	CONST_ENTRY("-//SoftQuad Software//DTD HoTMetaL PRO 6.0::19990601::extensions to HTML 4.0//") // len = 78, idx = 61
CONST_END(gDTDStrings)

enum
{
	DTD_NO_MATCH = 0,
	DTD_MATCH = 1
};

enum
{
	DTD_PREFIX_MATCH = 0,
	DTD_EXACT_MATCH = 2
};

enum
{
	DTD_PUBLIC_IDENT = 0,
	DTD_SYSTEM_IDENT = 4
};

enum
{
	DTD_TRIGGERS_QUIRKS = 0,
	DTD_TRIGGERS_LIMITED_QUIRKS = 8
};

enum
{
	DTD_SYSTEM_DONT_CARE = 0,
	DTD_NEED_SYSTEM_ID = 16
};

const UINT gDTDtokens[] =
{
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//IETF//DTD HTML//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//IETF//DTD HTML 3//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//W3C//DTD W3 HTML//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//W3C//DTD HTML 3.2//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//IETF//DTD HTML 2.0//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//IETF//DTD HTML 3.0//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//IETF//DTD HTML 3.2//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//IETF//DTD HTML 2.1E//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//W3O//DTD W3 HTML 3.0//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//IETF//DTD HTML Strict//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//IETF//DTD HTML Level 0//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//IETF//DTD HTML Level 1//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//IETF//DTD HTML Level 2//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//IETF//DTD HTML Level 3//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//W3C//DTD HTML 3.2 Draft//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//W3C//DTD HTML 3.2 Final//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//W3C//DTD HTML 3.2S Draft//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//IETF//DTD HTML 3.2 Final//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//IETF//DTD HTML 2.0 Strict//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//W3C//DTD HTML 3 1995-03-24//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//W3C//DTD HTML 4.0 Frameset//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//WebTechs//DTD Mozilla HTML//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//IETF//DTD HTML 2.0 Level 1//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//IETF//DTD HTML 2.0 Level 2//"
	DTD_MATCH | DTD_EXACT_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS | DTD_NEED_SYSTEM_ID, // "-//W3C//DTD HTML 4.01 Frameset//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_LIMITED_QUIRKS, // "-//W3C//DTD XHTML 1.0 Frameset//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//IETF//DTD HTML Strict Level 0//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//IETF//DTD HTML Strict Level 1//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//IETF//DTD HTML Strict Level 2//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//IETF//DTD HTML Strict Level 3//"
	DTD_MATCH | DTD_EXACT_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-/W3C/DTD HTML 4.0 Transitional/EN"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//Netscape Comm. Corp.//DTD HTML//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//WebTechs//DTD Mozilla HTML 2.0//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//W3C//DTD HTML 4.0 Transitional//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//Spyglass//DTD HTML 2.0 Extended//"
	DTD_MATCH | DTD_EXACT_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS | DTD_NEED_SYSTEM_ID, // "-//W3C//DTD HTML 4.01 Transitional//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_LIMITED_QUIRKS, // "-//W3C//DTD XHTML 1.0 Transitional//"
	DTD_MATCH | DTD_EXACT_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//W3O//DTD W3 HTML Strict 3.0//EN//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//W3C//DTD HTML Experimental 970421//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//IETF//DTD HTML 2.0 Strict Level 1//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//IETF//DTD HTML 2.0 Strict Level 2//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//Metrius//DTD Metrius Presentational//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//W3C//DTD HTML Experimental 19960712//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "+//Silmaril//dtd html Pro v0r11 19970101//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//AS//DTD HTML 3.0 asWedit + extensions//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//Netscape Comm. Corp.//DTD Strict HTML//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//O'Reilly and Associates//DTD HTML 2.0//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//SQ//DTD HTML 2.0 HoTMetaL + extensions//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//Sun Microsystems Corp.//DTD HotJava HTML//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//Microsoft//DTD Internet Explorer 2.0 HTML//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//Microsoft//DTD Internet Explorer 3.0 HTML//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//Microsoft//DTD Internet Explorer 2.0 Tables//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//Microsoft//DTD Internet Explorer 3.0 Tables//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//O'Reilly and Associates//DTD HTML Extended 1.0//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//Sun Microsystems Corp.//DTD HotJava Strict HTML//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//AdvaSoft Ltd//DTD HTML 3.0 asWedit + extensions//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//Microsoft//DTD Internet Explorer 2.0 HTML Strict//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//Microsoft//DTD Internet Explorer 3.0 HTML Strict//"
	DTD_MATCH | DTD_EXACT_MATCH | DTD_SYSTEM_IDENT | DTD_TRIGGERS_QUIRKS, // "http://www.ibm.com/data/dtd/v11/ibmxhtml1-transitional.dtd"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//O'Reilly and Associates//DTD HTML Extended Relaxed 1.0//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS, // "-//SoftQuad Software//DTD HoTMetaL PRO 6.0::19990601::extensions to HTML 4.0//"
	DTD_MATCH | DTD_PREFIX_MATCH | DTD_PUBLIC_IDENT | DTD_TRIGGERS_QUIRKS // "-//SoftQuad//DTD HoTMetaL PRO 4.0::19971010::extensions to HTML 4.0//"
};

CONST_ARRAY(g_handheld_html_doctypes, char*)
	CONST_ENTRY("-//W3C//DTD Compact HTML 1.0 Draft//EN"),
	CONST_ENTRY("-//WAPFORUM//DTD XHTML Mobile 1.0//EN"),
	CONST_ENTRY("-//WAPFORUM//DTD XHTML Mobile 1.1//EN"),
	CONST_ENTRY("-//WAPFORUM//DTD WML 2.0//EN"),
	CONST_ENTRY("-//OMA//DTD XHTML Mobile 1.0//EN"),
	CONST_ENTRY("-//W3C//DTD XHTML Basic 1.0//EN"), // http://www.w3.org/TR/2000/REC-xhtml-basic-20001219/
	CONST_ENTRY("-//W3C//DTD XHTML Basic 1.1//EN"), // http://www.w3.org/TR/2008/PR-xhtml-basic-20080611/
	CONST_ENTRY("-//WAPFORUM//DTD XHTML Mobile 1.2//EN"), // Released March 2008, can't find the spec except for a wikipedia article
	CONST_ENTRY(NULL) // NULL Termination
CONST_END(g_handheld_html_doctypes)

CONST_ARRAY(g_handheld_wml_doctypes, char*)
	CONST_ENTRY("-//WAPFORUM//DTD WML 1.3//EN"),
	CONST_ENTRY("-//WAPFORUM//DTD WML 1.2//EN"),
	CONST_ENTRY("-//WAPFORUM//DTD WML 1.1//EN"),
	CONST_ENTRY(NULL) // NULL Termination
CONST_END(g_handheld_wml_doctypes)
