/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#include "core/pch.h"

#include "platforms/viewix/src/nodes/MimeTypeNode.h"
#include "platforms/viewix/src/nodes/ApplicationNode.h"

MimeTypeNode::MimeTypeNode(const OpStringC & mime_type) :
    FileHandlerNode(0, 0)
{
	AddMimeType(mime_type);

    m_applications      = OP_NEW(OpHashTable, (&m_hash_function));
    m_default_app       = 0;
}

MimeTypeNode::~MimeTypeNode()
{
    OpHashIterator* it = m_applications->GetIterator();
    OP_STATUS ret_val = it->First();
    while (OpStatus::IsSuccess(ret_val))
    {
		uni_char * key = (uni_char *)it->GetKey();
		OP_DELETEA(key);
		ret_val = it->Next();
    }
	OP_DELETE(it);
    OP_DELETE(m_applications);
}

OP_STATUS MimeTypeNode::AddApplication(ApplicationNode * app)
{
    if(!app)
		return OpStatus::OK;

    const uni_char * key = app->GetKey();
	
	if(!key)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = m_applications->Add((void *) key, (void *) app);

	if(OpStatus::IsError(status))
		OP_DELETEA(key);

	return status;
}

OpString * MimeTypeNode::AddMimeType(const OpStringC & mime_type)
{
	for(unsigned int i = 0; i < m_mime_types.GetCount(); i++)
	{
		if(m_mime_types.Get(i)->Compare(mime_type) == 0)
			return m_mime_types.Get(i);
	}

	OpString * type = OP_NEW(OpString, ());

	if(!type)
		return 0;

	type->Set(mime_type);
    m_mime_types.Add(type);

	return type;
}

OpString * MimeTypeNode::AddSubclassType(const OpStringC & mime_type)
{
	for(unsigned int i = 0; i < m_subclass_types.GetCount(); i++)
	{
		if(m_subclass_types.Get(i)->Compare(mime_type) == 0)
			return m_subclass_types.Get(i);
	}

	OpString * type = OP_NEW(OpString, ());

	if(!type)
		return 0;

	type->Set(mime_type);
    m_subclass_types.Add(type);

	return type;
}


void MimeTypeNode::SetDefault(ApplicationNode * app)
{
    if(!app)
		return;

	// If the app is already in the applications list - remove it
	RemoveApplication(app);

	// If we have a default already - add it to the applications list
	if(m_default_app)
		AddApplication(m_default_app);

	// Set the default to be the new one
    m_default_app = app;
}

OP_STATUS MimeTypeNode::GetDefaultIconName(OpString & icon)
{
	return icon.Set("unknown.png");
}

// Unbelievably complicated remove caused by the need to delete
// the key aswell.
void MimeTypeNode::RemoveApplication(ApplicationNode * app)
{
	if(!app)
		return;

	OpHashIterator* it = m_applications->GetIterator();
    OP_STATUS ret_val = it->First();

    while (OpStatus::IsSuccess(ret_val))
    {
		ApplicationNode * node = (ApplicationNode *) it->GetData();

		if(node == app)
		{
			uni_char * key = (uni_char *) it->GetKey();
			void* data = 0;
			m_applications->Remove(key, &data);
			OP_DELETEA(key);
		}

		ret_val = it->Next();
    }

	OP_DELETE(it);
}
