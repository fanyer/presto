/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#include "core/pch.h"

#include "modules/forms/src/formiterator.h"

#include "modules/logdoc/htm_elm.h"

FormIterator::FormIterator(FramesDocument* frames_doc,
						   const HTML_Element* form_element) :
	m_frames_doc(frames_doc),
	m_form(const_cast<HTML_Element*>(form_element)),
	m_current(m_form),
	m_include_output(FALSE)
#ifdef _PLUGIN_SUPPORT_
	, m_include_object(FALSE)
#endif // _PLUGIN_SUPPORT_
{
	OP_ASSERT(frames_doc);
	OP_ASSERT(form_element);
	OP_ASSERT(form_element->IsMatchingType(HE_FORM, NS_HTML) || form_element->IsMatchingType(HE_ISINDEX, NS_HTML));
}

HTML_Element* FormIterator::GetNext()
{
	if (!m_current)
	{
		return NULL;
	}

	// In Web Forms 2, elements can be anywhere in the document,
	// not only after/in the <form> element. If we're starting at the <form>
	// tag we move to the beginning of the document and scan from there. We
	// will never again be at the form tag because we don't stop at form tags.
	if (m_current == m_form)
	{
		while (m_current->Parent())
		{
			m_current = m_current->Parent();
		}
	}
	else
	{
		m_current = static_cast<HTML_Element*>(m_current->NextActualStyle());
	}

	while (m_current)
	{
		// All the default types have FormValue. If something is included 
		// here by default without FormValue everything using the iterator has to be fixed/checked.
		HTML_ElementType he_type = m_current->GetNsType() == NS_HTML ? m_current->Type() : HE_ANY;
		if (he_type == HE_TEXTAREA ||
			he_type == HE_SELECT ||
			he_type == HE_INPUT ||
			he_type == HE_BUTTON
#ifdef FORMS_KEYGEN_SUPPORT
			|| he_type == HE_KEYGEN
#endif // FORMS_KEYGEN_SUPPORT
			|| (m_include_output && he_type == HE_OUTPUT)
#ifdef _PLUGIN_SUPPORT_
			|| (m_include_object && he_type == HE_OBJECT)
#endif // _PLUGIN_SUPPORT_
			)
		{
			if (FormManager::BelongsToForm(m_frames_doc, m_form, m_current))
			{
#ifdef _PLUGIN_SUPPORT_
				OP_ASSERT(m_include_object && m_current->IsMatchingType(HE_OBJECT, NS_HTML) || 
					FormManager::IsFormElement(m_current));
#else
				OP_ASSERT(FormManager::IsFormElement(m_current));
#endif // _PLUGIN_SUPPORT
				return m_current;
			}
		}

		m_current = static_cast<HTML_Element*>(m_current->NextActualStyle());
	}

	return m_current;
}
