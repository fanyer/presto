/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Allowed #if-ery: none.
 * Sort order: alphabetic
 */
#ifndef POSIX_SYS_NONIX_CHOICE_H
#define POSIX_SYS_NONIX_CHOICE_H __FILE__

#define SYSTEM_BLOCK_CON_FILE			NO
#define SYSTEM_COLORREF					NO /* COLORREF type */
#define SYSTEM_DRIVES					NO
#define SYSTEM_DTOA						NO /* dtoa() */
#define SYSTEM_ETOA						NO /* etoa() */
#define SYSTEM_GETPHYSMEM				NO /* OpSystemInfo::GetPhysicalMemorySizeMB */
#define SYSTEM_GUID						NO
#define SYSTEM_ITOA						NO /* itoa() */
#define SYSTEM_LTOA						NO /* ltoa() */
#define SYSTEM_MAKELONG					NO /* MAKELONG(,) macro */
#define SYSTEM_MULTIPLE_DRIVES			NO
#define SYSTEM_NETWORK_BACKSLASH_PATH	NO
#define SYSTEM_OWN_OP_STATUS_VALUES		NO
#define SYSTEM_POINT					NO /* POINT struct */
#define SYSTEM_RECT						NO /* RECT struct */
#define SYSTEM_RGB						NO /* RGB macro */
#define SYSTEM_SIZE						NO /* SIZE struct */
#define SYSTEM_STRCASE					NO /* strlwr, strupr */
#define SYSTEM_STRREV					NO /* at least, not on Linux */
#define SYSTEM_UNI_ITOA					NO /* itow */
#define SYSTEM_UNI_LTOA					NO /* ltow */
#define SYSTEM_UNI_MEMCHR				NO
#define SYSTEM_UNI_SNPRINTF				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_SPRINTF				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_SSCANF				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_STRCAT				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_STRCHR				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_STRCMP				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_STRCPY				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_STRCSPN				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_STRDUP				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_STRICMP				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_STRISTR				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_STRLCAT				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_STRLCPY				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_STRLEN				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_STRNCAT				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_STRNCMP				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_STRNCPY				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_STRNICMP				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_STRPBRK				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_STRRCHR				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_STRREV				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_STRSPN				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_STRSTR				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_STRTOK				POSIX_SYS_USE_UNI
#define SYSTEM_UNI_VSNPRINTF			POSIX_SYS_USE_UNI
#define SYSTEM_UNI_VSPRINTF				POSIX_SYS_USE_UNI
#define SYSTEM_YIELD					NO

#endif /* POSIX_SYS_NONIX_CHOICE_H */
