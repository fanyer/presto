/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef GADGET_SUPPORT

#include "modules/gadgets/OpGadgetClassProxy.h"
#include "modules/gadgets/OpGadgetClass.h"
#include "modules/gadgets/OpGadgetUpdate.h"

OpGadgetClassProxy::OpGadgetClassProxy(OpGadgetClass* gadget_class)
	: m_gadget_class(gadget_class),
	  m_update_description_document_url(NULL),
	  m_user_data(NULL)
{}

OpGadgetClassProxy::OpGadgetClassProxy(const uni_char* update_description_document_url,
				   const void* const user_data)
	: m_gadget_class(NULL),
	  m_update_description_document_url(update_description_document_url),
	  m_user_data(user_data)
{}


const uni_char* OpGadgetClassProxy::GetUpdateDescriptionDocumentUrl() const
{
	return m_gadget_class ? m_gadget_class->GetGadgetUpdateUrl() :
							m_update_description_document_url;
}

OpGadgetUpdateInfo* OpGadgetClassProxy::GetUpdateInfo() const
{
	return  m_gadget_class ? m_gadget_class->GetUpdateInfo() : m_update_info.get();
}

void OpGadgetClassProxy::SetUpdateInfo(OpGadgetUpdateInfo* update_info)
{
	if(m_gadget_class)
		m_gadget_class->SetUpdateInfo(update_info);
	else
		m_update_info = OpAutoPtr<OpGadgetUpdateInfo>(update_info);
}

#endif // GADGET_SUPPORT
