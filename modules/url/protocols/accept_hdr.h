/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */

#ifndef _HTTP_ACCEPT_HEADER_H_
#define _HTTP_ACCEPT_HEADER_H_

static const char HTTP_Accept_str[] =

	"text/html, "
	"application/xml;q=0.9, "
	"application/xhtml+xml, "

#ifdef URL_ADD_MULTIPART_MIXED_ACCEPT
	"multipart/mixed, "
#endif

#ifdef ALLOW_WML_IN_ACCEPT_HEADER
#ifdef WBMULTIPART_MIXED_SUPPORT
	"application/vnd.wap.multipart.mixed, "
#endif // WBMULTIPART_MIXED_SUPPORT
#endif // ALLOW_WML_IN_ACCEPT_HEADER

#ifdef _PNG_SUPPORT_
	"image/png, "
#endif	//PNG_SUPPORT

#ifdef WEBP_SUPPORT
	"image/webp, "
#endif // WEBP_SUPPORT

	"image/jpeg, image/gif, image/x-xbitmap, "

	// ******* NOTE: ALL additions above this line
	"*/*;q=0.1"; /* comment to fix editors getting confused by the string */

#endif
