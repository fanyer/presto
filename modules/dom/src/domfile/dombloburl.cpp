/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef DOM_SUPPORT_BLOB_URLS

#include "modules/dom/src/domfile/dombloburl.h"

#include "modules/dom/src/domfile/domblob.h"

/* static */ OP_STATUS
DOM_BlobURL::Make(DOM_BlobURL*& bloburl, const uni_char* url_str, DOM_Blob* blob)
{
	OP_ASSERT(url_str);
	OP_ASSERT(blob);

	if ((bloburl = OP_NEW(DOM_BlobURL, (blob))) == NULL ||
	    (bloburl->m_url = UniSetNewStr(url_str)) == NULL)
	{
		OP_DELETE(bloburl);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

void
DOM_BlobURL::GCTrace()
{
	DOM_Object::GCMark(m_blob);
}

void trace_blob(const void* key, void* data)
{
	DOM_BlobURL* bloburl = static_cast<DOM_BlobURL*>(data);
	bloburl->GCTrace();
}


/* static */ void
DOM_BlobURL::GCTrace(OpStringHashTable<DOM_BlobURL>* blob_urls)
{
	blob_urls->ForEach(trace_blob);
}

#endif // DOM_SUPPORT_BLOB_URLS

