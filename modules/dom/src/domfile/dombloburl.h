/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DOM_BLOBURL_H
#define DOM_BLOBURL_H

#ifdef DOM_SUPPORT_BLOB_URLS

#include "modules/dom/src/domobj.h"

#include "modules/util/OpHashTable.h"

class DOM_Blob;

/**
 * The connection between an URL and its blob.
 */
class DOM_BlobURL
{
	DOM_Blob* m_blob;
	const uni_char* m_url;

	DOM_BlobURL(DOM_Blob* blob) : m_blob(blob), m_url(NULL) {}
public:
	~DOM_BlobURL() { OP_DELETEA(m_url); }
	static OP_STATUS Make(DOM_BlobURL*& bloburl, const uni_char* url_str, DOM_Blob* blob);
	void GCTrace();
	const uni_char* GetURL()  { return m_url; }

	static void GCTrace(OpStringHashTable<DOM_BlobURL>* blob_urls);
};

#endif // DOM_SUPPORT_BLOB_URLS
#endif // DOM_BLOBURL_H
