// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Patricia Aas
//

#include "core/pch.h"
#include "adjunct/desktop_util/handlers/MimeTypeContainer.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
MimeTypeContainer::MimeTypeContainer(OpString * mime_type,
									 OpString * mime_type_name,
									 OpBitmap * mime_type_icon)
    : DownloadManager::Container(mime_type_name,
								 mime_type_icon)
{
    SetMimeType(mime_type);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void MimeTypeContainer::Init(OpString * mime_type,
							 OpString * mime_type_name,
							 OpBitmap * mime_type_icon)
{
	InitContainer(mime_type_name, mime_type_icon);
	SetMimeType(mime_type);
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void MimeTypeContainer::Empty()
{
	EmptyContainer();
	m_mime_type.Empty();
}


/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
const OpStringC& MimeTypeContainer::GetMimeType()         
{ 
    return m_mime_type; 
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void MimeTypeContainer::SetMimeType(OpString * mime_type)
{
    if(mime_type)
	{
		m_mime_type.Set(mime_type->CStr());
	}
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL MimeTypeContainer::IsOctetStream()
{
	return m_mime_type.CompareI(UNI_L("APPLICATION/OCTET-STREAM")) == 0;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL MimeTypeContainer::IsTextPlain()
{
	return m_mime_type.CompareI(UNI_L("TEXT/PLAIN")) == 0;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL MimeTypeContainer::IsUntrusted()
{
	BOOL is_octetstream_or_plain = m_mime_type.HasContent() && (IsOctetStream() || IsTextPlain());
	return 
		!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::TrustServerTypes) && 
		is_octetstream_or_plain;
}
