/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"


#include "modules/formats/argsplit.h"
#include "modules/util/str.h"

// argsplit.cpp

// Splitting parameters


void Allocated_Parameter::InitializeL(const OpStringC8 &a_Name, const OpStringC8 &a_Value, const OpStringC8 &a_Charset, const OpStringC8 &a_Language)
{
	m_name.SetL(a_Name);
	m_value.SetL(a_Value);
	m_charset.SetL(a_Charset);
	m_language.SetL(a_Language);

	name = m_name.CStr();
	value = m_value.CStr();
	assigned = TRUE;
}

NameValue_Splitter *Allocated_Parameter::DuplicateL() const
{
	OpStackAutoPtr<Allocated_Parameter> ret(OP_NEW_L(Allocated_Parameter, ()));

	ret->InitializeL(m_name, m_value, m_charset, m_language);

	ret->assigned = AssignedValue();
	ret->SetNameID(GetNameID());

	return ret.release();
}

void UniParameters::ConstructDuplicateL(const UniParameters *old)
{
	Parameters::ConstructDuplicateL(old);

	uni_name.SetFromUTF8L(Parameters::Name());
	uni_value.SetFromUTF8L(Parameters::Value());
}

char *UniParameters::InitL(char *val,int flags)
{
	char *ret = Parameters::InitL(val, flags);
	
	uni_name.SetFromUTF8L(Parameters::Name());
	uni_value.SetFromUTF8L(Parameters::Value());

	return ret;
}

void UniParameters::StripQuotes(BOOL gently)
{
	Parameters::StripQuotes(gently);

	if(!uni_value.IsEmpty())
		return;
	
	uni_char *trg = uni_value.DataPtr();
	const uni_char *src = uni_value.CStr();
	uni_char val;
	while((val = *(src++)) != '\0')
	{
		if(val != '\"')
		{
			if(val == '\\' && *src >= 0x20)
				val = *(src++);
			*(trg++) = val;
		}
	}
	*trg = '\0';
}

NameValue_Splitter *UniParameters::DuplicateL() const
{
	OpStackAutoPtr<UniParameters> ret(OP_NEW_L(UniParameters, ()));

	ret->ConstructDuplicateL(this);

	return ret.release();
}

Sequence_Splitter *UniParameters::CreateSplitterL() const
{
	return OP_NEW_L(UniParameterList, ());
}

void UniAllocated_Parameter::InitializeL(const OpStringC8 &a_Name, const OpStringC8 &a_Value, const OpStringC8 &a_Charset, const OpStringC8 &a_Language)
{
	uni_name.SetFromUTF8L(a_Name.CStr());
	uni_value.SetFromUTF8L(a_Value.CStr(), a_Value.Length()); //Only convert back from utf-8, user will do the next step based on charset value.
	m_name.SetL(a_Name);
	m_value.SetL(a_Value);
	m_charset.SetL(a_Charset);
	m_language.Set(a_Language);

	name = m_name.CStr();
	value = m_value.CStr();
	assigned = TRUE;
}

NameValue_Splitter *UniAllocated_Parameter::DuplicateL() const
{
	OpStackAutoPtr<UniAllocated_Parameter> ret(OP_NEW_L(UniAllocated_Parameter, ()));

	ret->InitializeL(m_name, m_value, m_charset, m_language);
	ret->assigned = AssignedValue();
	ret->SetNameID(GetNameID());

	return ret.release();
}

UniParameterList::~UniParameterList()
{
	Clear();
}

NameValue_Splitter *UniParameterList::CreateNameValueL() const
{
	return OP_NEW_L(UniParameters, ());
}

OP_STATUS UniParameterList::SetValue(const uni_char *value, unsigned flags)
{
	TRAPD(op_err, SetValueL(value, flags));

	return op_err;
}

void UniParameterList::SetValueL(const uni_char *value, unsigned flags)
{
	argument.SetUTF8FromUTF16L(value);

	if((flags & PARAM_TAKE_CONTENT) != 0)
	{
		uni_char *owned_value = (uni_char *) value;
		OP_DELETEA(owned_value); // As we are taking over the buffer, and have copied it, we delete it
		value = NULL;
	}

	flags = flags ^ (flags & (PARAM_TAKE_CONTENT | PARAM_BORROW_CONTENT | PARAM_COPY_CONTENT)); // strips caller specified buffer management
	flags |= PARAM_BORROW_CONTENT; // Parameters will only borrow this string (saves an allocation)

	ParameterList::SetValueL(argument.CStr(), flags);
}

UniParameters *UniParameterList::GetParameter(const uni_char *tag,
						Parameter_UseAssigned assign, UniParameters *after,
				Parameter_ResolveKeyword resolve)
{
	OpString8 temp_tag;

	if(OpStatus::IsError(temp_tag.SetUTF8FromUTF16(tag)))
		return NULL;

	return (UniParameters *) ParameterList::GetParameter(temp_tag.CStr(), assign, after, resolve);
}

Sequence_Splitter *UniParameterList::CreateSplitterL() const
{
	return OP_NEW_L(UniParameterList, ());
}

Parameters *UniParameterList::CreateAllocated_ParameterL(const OpStringC8 &a_Name, const OpStringC8 &a_Value, const OpStringC8 &a_Charset, const OpStringC8 &a_Language)
{
	OpStackAutoPtr<UniAllocated_Parameter> temp(OP_NEW_L(UniAllocated_Parameter, ()));

	temp->InitializeL(a_Name, a_Value, a_Charset, a_Language);

	return temp.release();
}



