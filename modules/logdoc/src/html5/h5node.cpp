/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/logdoc/src/html5/h5node.h"
#include "modules/logdoc/src/html5/html5attrcopy.h"
#include "modules/logdoc/src/html5/html5tokenwrapper.h"
#include "modules/logdoc/src/html5/html5buffer.h"

#include "modules/stdlib/src/thirdparty_printf/uni_printf.h"

/*virtual*/ H5Node::~H5Node()
{
	// gcc has problems with inline destructors
}

H5Node* H5Node::Next() const
{
	if (FirstChild())
		return FirstChild();

	for (const H5Node *leaf = this; leaf; leaf = leaf->Parent())
		if (leaf->Suc())
			return leaf->Suc();

	return NULL;
}

void H5Node::Under(H5Element *parent)
{
	m_parent = parent;

	if (H5Node *last_child = parent->LastChild())
	{
		m_pred = last_child;
		last_child->m_suc = this;
	}
	else
		m_parent->m_first_child = this;

	m_parent->m_last_child = this;
}

void H5Node::Precede(H5Node *node_after)
{
	m_parent = node_after->m_parent;

	m_pred = node_after->m_pred;
	if (m_pred)
		m_pred->m_suc = this;
	else if (m_parent)
		m_parent->m_first_child = this;

	m_suc = node_after;
	node_after->m_pred = this;
}

void H5Node::Out()
{
	if (m_suc)
		// If we've a successor, unchain us
		m_suc->m_pred = m_pred;
	else if (m_parent)
		// If we don't, we're the last in the list
		static_cast<H5Element*>(m_parent)->m_last_child = m_pred;

	if (m_pred)
		// If we've a predecessor, unchain us
		m_pred->m_suc = m_suc;
	else if (m_parent)
			// If we don't, we're the first in the list
		static_cast<H5Element*>(m_parent)->m_first_child = m_suc;

	m_pred = NULL;
	m_suc = NULL;
	m_parent = NULL;
}

/*virtual*/ H5Element::~H5Element()
{
	OP_DELETEA(m_name);
	OP_DELETEA(m_attrs);
}

/*virtual*/ BOOL H5Element::Clean()
{
	H5Node *child = m_first_child;
	while (child)
	{
		child->Clean();
		child = m_first_child;
	}

	Out();
	
	return TRUE;
}

void H5Element::SetNameL(const uni_char *name, unsigned len)
{
	LEAVE_IF_ERROR(UniSetStrN(m_name, name, len));
}

BOOL H5Element::IsNamed(const uni_char *name, Markup::Type type)
{
	if (m_elm_type != Markup::HTE_UNKNOWN)
		return type == m_elm_type;

	return uni_str_eq(name, m_name);
}

void H5Element::InitAttributesL(HTML5TokenWrapper *token)
{
	unsigned new_attr_count = token->GetAttrCount();
	if (new_attr_count > 0)
	{
		HTML5AttrCopy* new_attrs = OP_NEWA(HTML5AttrCopy, new_attr_count);
		LEAVE_IF_NULL(new_attrs);

		ANCHOR_ARRAY(HTML5AttrCopy, new_attrs);
		if (token->HasLocalAttrCopies())
		{
			for (unsigned i = 0; i < new_attr_count; i++)
				new_attrs[i].CopyL(token->GetLocalAttrCopy(i));
		}
		else
		{
			for (unsigned i = 0; i < new_attr_count; i++)
				new_attrs[i].CopyL(token->GetAttribute(i));
		}
		ANCHOR_ARRAY_RELEASE(new_attrs);
		m_attrs = new_attrs;
		m_attr_count = new_attr_count;
	}
}

const uni_char* H5Element::GetAttribute(const uni_char *name) const
{
	for (unsigned i = 0; i < m_attr_count; i++)
	{
		if (uni_str_eq(name, m_attrs[i].GetName()->GetBuffer()))
			return m_attrs[i].GetValue();
	}

	return NULL;
}

void H5Element::AddAttributeL(HTML5Token *token, unsigned attr_idx)
{
	unsigned new_count = m_attr_count + 1;
	HTML5AttrCopy *new_attrs = OP_NEWA(HTML5AttrCopy, new_count);
	LEAVE_IF_NULL(new_attrs);

	ANCHOR_ARRAY(HTML5AttrCopy, new_attrs);
	for (unsigned i = 0; i < m_attr_count; i++)
		new_attrs[i].CopyL(&m_attrs[i]);
	new_attrs[new_count - 1].CopyL(token->GetAttribute(attr_idx));
	ANCHOR_ARRAY_RELEASE(new_attrs);

	OP_DELETEA(m_attrs);
	m_attrs = new_attrs;
	m_attr_count = new_count;
}

void H5Element::AddAttributeL(const uni_char *name, unsigned name_len, const uni_char *value, unsigned value_len)
{
	unsigned new_count = m_attr_count + 1;
	HTML5AttrCopy *new_attrs = OP_NEWA(HTML5AttrCopy, new_count);
	LEAVE_IF_NULL(new_attrs);

	ANCHOR_ARRAY(HTML5AttrCopy, new_attrs);
	for (unsigned i = 0; i < m_attr_count; i++)
		new_attrs[i].CopyL(&m_attrs[i]);
	new_attrs[new_count - 1].SetNameL(name, name_len);
	new_attrs[new_count - 1].SetValueL(value, value_len);
	ANCHOR_ARRAY_RELEASE(new_attrs);

	OP_DELETEA(m_attrs);
	m_attrs = new_attrs;
	m_attr_count = new_count;
}

void H5Element::SortAttributes()
{
	// Bubble sort FTW!!!!!!
	HTML5AttrCopy tmp;
	for (unsigned i = 0; i < m_attr_count; i++)
		for (unsigned j = i + 1; j < m_attr_count; j++)
		{
			if (m_attrs[j].GetNs() != Markup::HTML && m_attrs[j].GetNs() < m_attrs[i].GetNs()
				|| uni_strcmp(m_attrs[j].GetName()->GetBuffer(), m_attrs[i].GetName()->GetBuffer()) < 0)
			{
				tmp = m_attrs[i];
				m_attrs[i] = m_attrs[j];
				m_attrs[j] = tmp;
			}
		}
	// Clear tmp to avoid deletion of the strings
	tmp.m_name.ReleaseBuffer();
	tmp.m_value = NULL;
}

/*virtual*/ H5Comment::~H5Comment()
{
	OP_DELETEA(m_data);
}

/*virtual*/ BOOL H5Comment::Clean()
{
	Out();
	
	return TRUE;
}

void H5Comment::InitDataL(HTML5Token *token)
{
	unsigned dummy;
	token->GetData().GetBufferL(m_data, dummy, TRUE);
}

void H5Comment::SetDataL(const uni_char *data, unsigned len)
{
	LEAVE_IF_ERROR(UniSetStrN(m_data, data, len));
}

/*virtual*/ H5Doctype::~H5Doctype()
{
	// gcc has problems with inline destructors
}

/*virtual*/ BOOL H5Doctype::Clean()
{
	Out();
	
	return TRUE;
}

void H5Doctype::InitL(const uni_char *name, const uni_char *public_id, const uni_char *system_id)
{
	LEAVE_IF_ERROR(m_logdoc->SetDoctype(name, public_id, system_id));
}

/*virtual*/ H5Text::~H5Text()
{
	// gcc has problems with inline destructors
}

/*virtual*/ BOOL H5Text::Clean()
{
	Out();
	
	return TRUE;
}

void H5Text::SetTextL(const uni_char *text, unsigned len)
{
	m_text.Clear();
	m_text.AppendL(text, len);
}

void H5Text::AppendTextL(const uni_char *text, unsigned len)
{
	m_text.AppendL(text, len);
}
