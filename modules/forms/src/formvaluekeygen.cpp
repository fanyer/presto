/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef FORMS_KEYGEN_SUPPORT

#include "modules/forms/formvaluekeygen.h"

#include "modules/forms/piforms.h"
#include "modules/logdoc/htm_elm.h"
# include "modules/libssl/sslv3.h"
# include "modules/libssl/ssl_api.h"

#define SSL_GET_KEYGEN_SIZE(x) g_ssl_api->SSL_GetKeygenSize(SSL_RSA, (x))

/* static */
OP_STATUS FormValueKeyGen::ConstructFormValueKeyGen(HTML_Element* he,
													FormValue*& out_value)
{
	OP_ASSERT(he->Type() == HE_KEYGEN);
	out_value = OP_NEW(FormValueKeyGen, ());
	if (!out_value)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

/* virtual */
FormValue* FormValueKeyGen::Clone(HTML_Element *he)
{
	FormValueKeyGen* clone = OP_NEW(FormValueKeyGen, ());
	if (clone)
	{
		if (he && IsValueExternally())
		{
			FormObject* form_object = he->GetFormObject();
			SelectionObject* sel_obj = static_cast<SelectionObject*>(form_object);
			m_keysize_idx = sel_obj->GetSelectedIndex();
		}

		clone->m_keysize_idx = m_keysize_idx;
		OP_ASSERT(!clone->IsValueExternally());
	}
	return clone;
}

/* virtual */
OP_STATUS FormValueKeyGen::GetValueAsText(HTML_Element* he,
									 OpString& out_value) const
{
	OP_ASSERT(!"Go away. No text value here");
	return OpStatus::OK;
}

unsigned int FormValueKeyGen::GetSelectedKeySize(HTML_Element* he) const
{
	int key_idx = m_keysize_idx;
	if (IsValueExternally())
	{
		FormObject* form_object = he->GetFormObject();
		SelectionObject* sel_obj = static_cast<SelectionObject*>(form_object);
		key_idx = sel_obj->GetSelectedIndex();
	}
	return key_idx == -1 ? 0 : SSL_GET_KEYGEN_SIZE(key_idx+1);
}

/* virtual */
OP_STATUS FormValueKeyGen::SetValueFromText(HTML_Element* he,
											const uni_char* in_value)
{
	// Kidding me?
	OP_ASSERT(!"What are you doing?");
	return OpStatus::OK;
}

/* virtual */
OP_STATUS FormValueKeyGen::ResetToDefault(HTML_Element* he)
{
	// XXX How?
	return OpStatus::OK;
}

/* virtual */
BOOL FormValueKeyGen::Externalize(FormObject* form_object_to_seed)
{
	if (!FormValue::Externalize(form_object_to_seed))
	{
		return FALSE; // It was wrong to call Externalize
	}
	SelectionObject* sel_obj;
	sel_obj = static_cast<SelectionObject*>(form_object_to_seed);
	if (sel_obj->GetElementCount() == 0)
	{
		// Not inited yet. Let's insert all the elements

		// The old form implementation requires this!
		sel_obj->StartAddElement();

		BOOL need_default_selection = m_keysize_idx == -1;
		unsigned int keygen_size = SSL_GET_KEYGEN_SIZE(1);

		const unsigned int default_keygen_size = g_ssl_api->SSL_GetDefaultKeyGenSize(SSL_RSA);

		int i = 0;
		while (keygen_size > 0)
		{
#if UINT_MAX > 0xffffffffu
# error "Code assumes ints are at most 4 bytes"
#endif
			uni_char buf[12]; // ARRAY OK 2009-01-26 bratell - An int fits
			uni_itoa(keygen_size, buf, 10); // radix 10
			sel_obj->AddElement(buf, FALSE, FALSE); // Not selected, not disabled
			if (need_default_selection && keygen_size >= default_keygen_size)
			{
				// Select this as default
				m_keysize_idx = i;
				need_default_selection = FALSE;
			}
			i++;
			keygen_size = SSL_GET_KEYGEN_SIZE(i+1);
		}

		sel_obj->EndAddElement(); // Changes widget size
	}
	// This is only until the layout module is fixed to not insert keygen options
	else if (m_keysize_idx == -1)
	{
		m_keysize_idx = 0;
	}
	sel_obj->SetSelectedIndex(m_keysize_idx);
	return TRUE;
}

/* virtual */
void FormValueKeyGen::Unexternalize(FormObject* form_object_to_replace)
{
	if (IsValueExternally())
	{
		SelectionObject* sel_obj;
		sel_obj = static_cast<SelectionObject*>(form_object_to_replace);
		m_keysize_idx = sel_obj->GetSelectedIndex();
		FormValue::Unexternalize(form_object_to_replace);
	}
}

#endif // FORMS_KEYGEN_SUPPORT
