/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _URL_TAGS_H_
#define _URL_TAGS_H_

// Cache File Tags
// !!!NOTE!!! DO NOT ENTER ANY NEW TAGS HERE. NEW TAGS !!!MUST!!! BE ADDED BY MODULE OWNER !!!NOTE!!!

#define TAG_CACHE_FILE_ENTRY				0x0001 // Record sequence
#define TAG_VISITED_FILE_ENTRY				0x0002 // Record sequence
#define TAG_DOWNLOAD_RESCUE_FILE_ENTRY		0x0041 // Record sequence
#define TAG_CACHE_PARAMETERS_ONLY			0x0042 // Record sequence

#define TAG_CACHE_LASTFILENAME_ENTRY		0x0040 // String 5 characters
// cacheentry tags withing cache file entries
#define TAG_URL_NAME						0x0003 // String
#define TAG_LAST_VISITED					0x0004 // 32bit unsigned, expandable
#define TAG_LOCALTIME_LOADED				0x0005 // unsigned 32bit
#define TAG_SECURITY_STATUS					0x0006 // unsigned, truncated
#define TAG_STATUS							0x0007 // unsigned, truncated
#define TAG_CONTENT_SIZE					0x0008 // unsigned, truncated
#define TAG_CONTENT_TYPE					0x0009 // string
#define TAG_CHARSET							0x000a // string
#define TAG_FORM_REQUEST				   (0x000b | MSB_VALUE) // No Content, True if present
#define TAG_CACHE_DOWNLOAD_FILE			   (0x000c | MSB_VALUE) // No Content, True if present
#define TAG_CACHE_FILENAME					0x000d // string
#define TAG_PROXY_NO_CACHE				   (0x000e | MSB_VALUE) // No Content, True if present
#define TAG_CACHE_ALWAYS_VERIFY			   (0x000f | MSB_VALUE) // No Content, True if present
#define TAG_HTTP_PROTOCOL_DATA				0x0010 // record of TAG_HTTP_* tags
#define TAG_HTTP_SEND_CONTENT_TYPE			0x0011 // string
#define TAG_HTTP_SEND_DATA					0x0012 // string // Only used by windows history
#define TAG_HTTP_SEND_ONLY_IF_MODIFIED	   (0x0013 | MSB_VALUE) // No Content, True if present
#define TAG_HTTP_SEND_REFERER				0x0014 // string
#define TAG_HTTP_KEEP_LOAD_DATE				0x0015 // string
#define TAG_HTTP_KEEP_EXPIRES				0x0016 // unsigned, truncated
#define TAG_HTTP_KEEP_LAST_MODIFIED			0x0017 // string
#define TAG_HTTP_KEEP_MIME_TYPE				0x0018 // string
#define TAG_HTTP_KEEP_ENTITY_TAG			0x0019 // string
#define TAG_HTTP_MOVED_TO_URL				0x001a // string
#define TAG_HTTP_RESPONSE_TEXT				0x001b // string
#define TAG_HTTP_RESPONSE					0x001c // unsigned, truncated
#define TAG_HTTP_REFRESH_URL				0x001d // string
#define TAG_HTTP_REFRESH_INT				0x001e // unsigned, truncated
#define TAG_HTTP_SUGGESTED_NAME				0x001f // string
#define TAG_HTTP_KEEP_ENCODING				0x0020 // string
#define TAG_HTTP_CONTENT_LOCATION			0x0021 // string
#define TAG_RELATIVE_ENTRY					0x0022  // Sequence of TAG_RELATIVEentries
#define TAG_RELATIVE_NAME					0x0023 // string
#define TAG_RELATIVE_VISITED				0x0024 // unsigned, truncated
#define TAG_USERAGENT_ID					0x0025 // unsigned, truncated
#define TAG_USERAGENT_VER_ID				0x0026 // int, truncated
#define TAG_CACHE_ACPO_CODE					0x0027 // unsigned int
#define TAG_DOWNLOAD_START_TIME				0x0028 // time_t
#define TAG_DOWNLOAD_STOP_TIME				0x0029 // time_t
#define TAG_DOWNLOAD_SEGMENTSIZE			0x002A // time_t
#define TAG_DOWNLOAD_FTP_MDTM_DATE			0x002B // string 
#define TAG_CACHE_NEVER_FLUSH			   (0x002B | MSB_VALUE) // No Content, True if present // Collision, but not dangerous //Reason : From symbian v6 code
// TAG_DOWNLOAD_RESUMABLE_STATUS values
// 0 Not resumable
// 1 May not be resumable (or field not present)
// 2 Probably resumable
#define TAG_DOWNLOAD_RESUMABLE_STATUS		0x002C // Resumability indication of the downloaded URL
#define TAG_HTTP_LINK_RELATION				0x002D // string
#define TAG_HTTP_CONTENT_LANGUAGE			0x002E // string
#define TAG_CACHE_MUST_VALIDATE			   (0x002F | MSB_VALUE) // True if present
#define TAG_HTTP_SEND_METHOD_GET		   (0x0030 | MSB_VALUE) // Default, No Content, True if present
#define TAG_HTTP_SEND_METHOD_POST		   (0x0031 | MSB_VALUE) // No Content, True if present

#define TAG_DOWNLOAD_TRANSFERTYPE           0x0032 // unsigned, download type (needed for download mngr rescue file on non-http downloads)
#define TAG_DOWNLOAD_METAFILE               0x0033 // string, pointer to the meta data file used to initiate this download (used for BitTorrent)
#define TAG_DOWNLOAD_LOCATION               0x0034 // string, pointer to the last directory used as root for this download (used for BitTorrent)
#define TAG_DOWNLOAD_COMPLETION_STATUS		0x0035 // unsigned, used for BitTorrent to signify if a archive has been completely downloaded and CRC checked.
#define TAG_DOWNLOAD_FRIENDLY_NAME          0x0036 // string, pointer to the "friendly" name (used for BitTorrent)

#define TAG_ASSOCIATED_FILES                0x0037 // unsigned int, bitfield for associated files

#define TAG_DOWNLOAD_HIDE_TRANSFER          0x0038 // hide from transfer panel

#define TAG_HTTP_AGE						0x003A

#define TAG_HTTP_RESPONSE_HEADERS			0x003B // Sequence of TAG_HTTP_RESPONSE_ENTRY record (all headers from a HTTP response)
#define TAG_HTTP_RESPONSE_ENTRY				0x003C // Sequence of a single TAG_HTTP_RESPONSE_HEADER_NAME and a single TAG_HTTP_RESPONSE_HEADER_VALUE
#define TAG_HTTP_RESPONSE_HEADER_NAME		0x003D // String: Name of HTTP header
#define TAG_HTTP_RESPONSE_HEADER_VALUE		0x003E // String: Value of HTTTP header
#define TAG_CACHE_MULTIMEDIA			   (0x003F | MSB_VALUE) // No Content, True if present; it notify if the file is a "Multimedia Cache", with its header and logic

#define TAG_CACHE_APPLICATION_CACHE_ENTRY	0x003F // // Record sequence

#define TAG_CACHE_EMBEDDED_CONTENT			0x0050 // Sequence of (TAG_CONTENT_SIZE) bytes. It is the actual content of the file (it is stored in the index, not outside as a normal cache file)
#define TAG_CACHE_CONTAINER_INDEX			0x0051 // byte. It is the index on the container file (several cache files will sharing the same phisical file)
#define TAG_CACHE_LOADED_FROM_NETTYPE		0x0052 // unsigned int, value of KLoadedFromNetType attribute
#define TAG_CACHE_CONTENT_TYPE_WAS_NULL			   (0x0053 | MSB_VALUE) // TRUE if the MIME type was NULL; this is used for XHR, as MIME sniffing created problems

#define TAG_CACHE_SERVER_CONTENT_TYPE		0x0054 // string

// Must be sequential and long form must have LSB set
#define TAG_CACHE_DYNATTR_FLAG_ITEM			0x0060  // Flag dynamic attribute, short form, TRUE if present. 
													// A sequence of two integers module number and in-module tag, both 8-bits 
#define TAG_CACHE_DYNATTR_FLAG_ITEM_Long	0x0061  // Flag dynamic attribute, long form, TRUE if present. 
													// A sequence of two integers module number and in-module tag, both 16-bits 
#define TAG_CACHE_DYNATTR_INT_ITEM			0x0062  // Integer dynamic attribute, 32 bit unsigned integer, short
													// First a sequence of two integers module number and in-module tag, both 8 bits
													//Then the rest of the record is an integer, endianness determined by fileformat.
#define TAG_CACHE_DYNATTR_INT_ITEM_Long		0x0063  // Integer dynamic attribute, 32 bit unsigned integer, long form
													// First a sequence of two integers module number and in-module tag, both 16 bits
													//Then the rest of the record is an integer, endianness determined by fileformat.
#define TAG_CACHE_DYNATTR_STRING_ITEM		0x0064  // String dynamic attribute, short form
													// First a sequence of two integers module number and in-module tag, both 8 bits
													//Then the rest of the record is a string
#define TAG_CACHE_DYNATTR_STRING_ITEM_Long	0x0065  // String dynamic attribute, long form
													// First a sequence of two integers module number and in-module tag, both 16 bits
													//Then the rest of the record is a string

#define TAG_CACHE_DYNATTR_URL_ITEM			0x0066	// URL string form dynamic attribute, short form
													// First a sequence of two integers module number and in-module tag, both 8 bits
													//Then the rest of the record is a string that MUST be an absolute URL.
#define TAG_CACHE_DYNATTR_URL_ITEM_Long		0x0067	// URL string form dynamic attribute, long form
													// First a sequence of two integers module number and in-module tag, both 16 bits
													//Then the rest of the record is a string that MUST be an absolute URL.

// Last item
#define TAG_CACHE_DYNATTR_LAST_ITEM TAG_CACHE_DYNATTR_URL_ITEM_Long

// end of sequential 

#endif // URL_TAGS_H

