/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _HTTP_METHODS_AND_RESPONSES_
#define _HTTP_METHODS_AND_RESPONSES_

// response status codes
#define HTTP_NO_RESPONSE    0
#define HTTP_CONTINUE		100
#define HTTP_SWITCHING_PROTOCOLS		101
#define HTTP_OK         200
#define HTTP_CREATED      201
#define HTTP_ACCEPTED     202
#define HTTP_PARTIAL_INFO   203
#define HTTP_NO_CONTENT     204
#define HTTP_RESET_CONTENT     205
#define HTTP_PARTIAL_CONTENT     206
#define HTTP_MULTIPLE_CHOICES 300
#define HTTP_MOVED        301
#define HTTP_FOUND        302
#define HTTP_SEE_OTHER     303
#define HTTP_NOT_MODIFIED   304
#define HTTP_USE_PROXY		305
#define HTTP_TEMPORARY_REDIRECT   307
#define HTTP_BAD_REQUEST    400
#define HTTP_UNAUTHORIZED   401
#define HTTP_PAYMENT_REQUIRED 402
#define HTTP_FORBIDDEN      403
#define HTTP_NOT_FOUND      404
#define HTTP_METHOD_NOT_ALLOWED      405
#define HTTP_NOT_ACCEPTABLE      406
#define HTTP_PROXY_UNAUTHORIZED   407
#define HTTP_TIMEOUT	408
#define HTTP_CONFLICT	409
#define HTTP_GONE	410
#define HTTP_LENGTH_REQUIRED	411
#define HTTP_PRECOND_FAILED	412
#define HTTP_REQUEST_ENT_TOO_LARGE	413
#define HTTP_REQUEST_URI_TOO_LARGE	414
#define HTTP_UNSUPPORTED_MEDIA_TYPE	415
#define HTTP_RANGE_NOT_SATISFIABLE	416
#define HTTP_EXPECTATION_FAILED		417
#define HTTP_INTERNAL_ERROR   500
#define HTTP_NOT_IMPLEMENTED  501
#define HTTP_BAD_GATEWAY  502
#define HTTP_SERVICE_UNAVAILABLE  503
#define HTTP_GATEWAY_TIMEOUT  504
#define HTTP_VERSION_NOT_SUPPORTED  505



#define HTTP_Method	unsigned short
#define HTTP_METHOD_GET		0
#define HTTP_METHOD_POST	1
#define HTTP_METHOD_HEAD	2
#define HTTP_METHOD_CONNECT	3
#define HTTP_METHOD_PUT		4
#define HTTP_METHOD_OPTIONS	5
#define HTTP_METHOD_DELETE	6
#define HTTP_METHOD_TRACE	7
#define HTTP_METHOD_SEARCH  8	
#define HTTP_METHOD_String  9	// Method specified in string
#define HTTP_METHOD_Invalid 10  // The specified method was invalid, fail any request.

// Max searchable
#define HTTP_METHOD_MAX  HTTP_METHOD_SEARCH

BOOL GetMethodHasRequestBody(HTTP_Method meth);
BOOL GetMethodIsSafe(HTTP_Method meth);
BOOL GetMethodIsRestartable(HTTP_Method meth);

const char *GetHTTPMethodString(HTTP_Method method);

#endif /* _HTTP_METHODS_ */
