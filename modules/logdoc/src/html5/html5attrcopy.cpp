/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/logdoc/src/html5/html5attrcopy.h"
#include "modules/logdoc/src/html5/html5attr.h"
#include "modules/logdoc/src/html5/html5tokenbuffer.h"


HTML5AttrCopy::~HTML5AttrCopy()
{
	OP_DELETEA(m_value);
}

void HTML5AttrCopy::CopyL(HTML5Attr *src)
{
	m_name.CopyL(src->GetName());

	unsigned dummy;
	src->GetValue().GetBufferL(m_value, dummy, TRUE);

	m_ns = Markup::HTML;
}

void HTML5AttrCopy::CopyL(HTML5AttrCopy *src)
{
	m_name.CopyL(src->GetName());

	uni_char *tmp = src->GetValue();
	if (tmp)
		LEAVE_IF_ERROR(UniSetStrN(m_value, tmp, uni_strlen(tmp)));
	else
		m_value = NULL;
	m_ns = src->m_ns;
}

void HTML5AttrCopy::SetNameL(const uni_char* name, unsigned length)
{
	m_name.Clear();
	m_name.AppendL(name, length);
}

void HTML5AttrCopy::SetNameL(HTML5TokenBuffer *name)
{
	m_name.CopyL(name);
}

void HTML5AttrCopy::SetValueL(const uni_char* value, unsigned length)
{
	LEAVE_IF_ERROR(UniSetStrN(m_value, value, length));
}

BOOL HTML5AttrCopy::IsEqual(HTML5Attr *attr)
{
	if (uni_strncmp(m_name.GetBuffer(), attr->GetNameStr(), m_name.Length()) != 0)
		return FALSE;

	return attr->GetValue().Matches(m_value, FALSE);
}
