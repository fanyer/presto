/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Roar Lauritzsen
**
*/


#ifndef _MIME_ENUM_H_
#define _MIME_ENUM_H_

enum MIME_encoding {
	ENCODE_UNKNOWN, 
	ENCODE_NONE, 
	ENCODE_7BIT, 
	ENCODE_8BIT, 
	ENCODE_BINARY,
	ENCODE_QP, 
	ENCODE_BASE64, 
	ENCODE_UUENCODE
};

enum MIME_ContentTypeID {
	MIME_Other,
	MIME_Alternative,
	MIME_MultiPart,
	MIME_Mime,
	MIME_Plain_text,
	MIME_HTML_text,
	MIME_Javascript_text,
	MIME_CSS_text,
	MIME_Text,
	MIME_audio,
	MIME_video,
	MIME_image,
	MIME_ExternalBody,
	MIME_Message,
	MIME_Binary,
	MIME_XML_text,
	MIME_GIF_image,
	MIME_JPEG_image,
	MIME_PNG_image,
	MIME_BMP_image,
	MIME_Flash_plugin,
	MIME_MS_TNEF_data,
	MIME_SMIME_Signed,
	MIME_SMIME_pkcs7,
	MIME_PostSecEncrypted,
	MIME_PostSecEncrypted_Email,
	MIME_Multipart_Related,
	MIME_PGP_File,
	MIME_PGP_Signed,
	MIME_PGP_Encrypted,
	MIME_PGP_Keys,
	MIME_Multipart_Signed,
	MIME_Multipart_Encrypted,
	MIME_Multipart_Binary,
	MIME_SVG_image,
	MIME_ignored_body,
	MIME_calendar_text
};

enum MIME_ExtBody_Accesstype {
	MIME_URI,
#ifndef NO_FTP_SUPPORT
	MIME_FTP,
	MIME_TFTP,
	MIME_AnonFTP,
#endif // NO_FTP_SUPPORT
	MIME_LocalFile,
	MIME_Other_Access
};

#endif // _MIME_ENUM_H_

