// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Patricia Aas
//

#ifndef __MIME_TYPE_CONTAINER_H__
#define __MIME_TYPE_CONTAINER_H__

#include "adjunct/desktop_util/handlers/DownloadManager.h"

class MimeTypeContainer :
	public DownloadManager::Container
{
public:

//  -------------------------
//  Public member functions:
//  -------------------------

	MimeTypeContainer()	: DownloadManager::Container() {}

    MimeTypeContainer(OpString * mime_type,
					  OpString * mime_type_name,
					  OpBitmap * mime_type_icon);

	void Init(OpString * mime_type,
			  OpString * mime_type_name,
			  OpBitmap * mime_type_icon);

	void Empty();

    const OpStringC& GetMimeType();
    BOOL IsOctetStream();
    BOOL IsTextPlain();
    BOOL IsUntrusted();
	BOOL HasMimeType(OpString & type)
		{ return m_mime_type.HasContent() && type.HasContent() && m_mime_type.Compare(type) == 0; }

	virtual ContainerType GetType() { return CONTAINER_TYPE_MIMETYPE; }

private:

//  ------------------------
//  Private member functions:
//  -------------------------

    void SetMimeType(OpString * mime_type);

//  -------------------------
//  Private member variables:
//  -------------------------

    OpString m_mime_type;
};

#endif //__MIME_TYPE_CONTAINER_H__
