#ifndef MACHO_GLUE_PREFIX
#define MACHO_GLUE_PREFIX

/**
* Get a small subset of uni_char stuff from Opera
*/

#include <Carbon.h>
#include <wchar.h>
#include <stdlib.h>

#define _UNI_L_FORCED_WIDE(s)			L ## s
#define UNI_L(s)						_UNI_L_FORCED_WIDE(s)	//	both needed
#define uni_strncpy 	wcsncpy
#define uni_strncmp 	wcsncmp
#define uni_strnicmp 	wcsnicmp
#define uni_strlen		wcslen
#define uni_strchr		wcschr
#define uni_strcpy		wcscpy
typedef wchar_t			uni_char;

#define MAX_PATH		_MAX_PATH

#endif // MACHO_GLUE_PREFIX