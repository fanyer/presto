/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef MIME_CAPABILITIES_H
#define MIME_CAPABILITIES_H

#define MIME_MODULE_NAME letter_reader
#define MIME_MODULE_VERSION 1
#define MIME_MODULE_FINAL_VERSION 1

// The MIME module is included
#define MIME_CAP_MODULE

// URL will call listeners when decoder has an attachment ready
#define URL_CAP_CAN_REPORT_DECODED_ATTACHMENTS

// The MIME module contains a multipart parser
#define MIME_CAP_MULTIPART_PARSER

// The MIME module contains its own enums
#define MIME_CAP_MIME_ENUMS

// This module knows about the network selftest utility module
#define MIME_CAP_KNOW_NETWORK_SELFTEST

// The MIME module has GetOriginalContentType
#define MIME_CAP_GET_ORIGINAL_CONTENT_TYPE

#endif // MIME_CAPABILITIES_H
