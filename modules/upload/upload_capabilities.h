/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef UPLOAD_CAPABILITIES_H
#define UPLOAD_CAPABILITIES_H

#define UPLOAD_MODULE_NAME envelope_filler
#define UPLOAD_MODULE_VERSION 1
#define UPLOAD_MODULE_FINAL_VERSION 1

// The Upload module is included
#define UPLOAD_CAP_MODULE

// This module has the capability to upload data as XML forms instead of MIME
#define URL_CAP_XML_UPLOAD_MODE

// Upload_URL::Init can handle file names with percent characters so
// others can stop escaping the "file names". This is only true if
// both this define and FORMS_CAP_NOT_ESCAPE_UPLOAD_NAMES is set
#define UPLOAD_CAP_CAN_HANDLE_NAMES_WITH_PERCENT

// Header_Item::GetValue exists
#define UPLOAD_CAP_HEADER_ITEM_GET_VALUE

// Header_List::RemoveHeader exists
#define UPLOAD_CAP_HEADER_LIST_REMOVE_HEADER

// Header_List::InsertHeaders can remove existing header items
#define UPLOAD_CAP_HEADER_LIST_INSERTHEADERS_CAN_REMOVE_EXISTING

// Header_Item can be temporarily disabled
#define UPLOAD_CAP_HEADER_ITEM_TEMP_DISABLE

#endif // UPLOAD_CAPABILITIES_H
