/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/xmlutils/xmltoken.h"
#include "modules/xmlutils/xmltokenbackend.h"
#include "modules/xmlutils/xmlutils.h"
#include "modules/xmlutils/src/xmlparserimpl.h"
#include "modules/xmlparser/xmlinternalparser.h"
#include "modules/util/str.h"

BOOL
XMLToken::Attribute::TakeOverValue()
{
	if (owns_value)
	{
		owns_value = 0;
		return TRUE;
	}
	else
		return FALSE;
}

void
XMLToken::Attribute::SetName(const uni_char *new_name, unsigned new_name_length)
{
	XMLCompleteNameN temporary(new_name, new_name_length);
	name = temporary;
}

void
XMLToken::Attribute::SetName(const XMLCompleteNameN &new_name)
{
	name = new_name;
}

void
XMLToken::Attribute::SetValue(const uni_char *new_value, unsigned new_value_length, BOOL need_copy, BOOL new_specified)
{
	value = new_value;
	value_length = new_value_length;
	owns_value = !need_copy;
	specified = new_specified;
	id = 0;
}

void
XMLToken::Attribute::SetId()
{
	id = 1;
}

void
XMLToken::Attribute::Reset()
{
	if (owns_value)
	{
		uni_char *non_const = const_cast<uni_char *>(value);
		OP_DELETEA(non_const);
		owns_value = FALSE;
	}
}

XMLToken::Literal::Literal()
	: parts(NULL),
	  parts_count(0)
{
}

XMLToken::Literal::~Literal()
{
	OP_DELETEA(parts);
}

BOOL
XMLToken::Literal::TakeOverPart(unsigned index)
{
	Part &part = parts[index];

	if (part.owns_value)
	{
		if (backend)
			backend->ReleaseLiteralPart(index);

		part.owns_value = FALSE;
		return TRUE;
	}
	else
		return FALSE;
}

OP_STATUS
XMLToken::Literal::SetPartsCount(unsigned count)
{
	parts = OP_NEWA(Part, count);

	if (parts)
	{
		op_memset(parts, 0, count * sizeof parts[0]);
		parts_count = count;
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

OP_STATUS
XMLToken::Literal::SetPart(unsigned index, const uni_char *data, unsigned data_length, BOOL need_copy)
{
	Part &part = parts[index];

	if (need_copy)
		if (!(part.data = UniSetNewStrN(data, data_length)))
			return OpStatus::ERR_NO_MEMORY;
		else
			part.owns_value = TRUE;
	else
		part.data = data;

	part.data_length = data_length;
	return OpStatus::OK;
}

XMLToken::Literal::Part::~Part()
{
	if (owns_value)
	{
		uni_char *non_const = const_cast<uni_char *>(data);
		OP_DELETEA(non_const);
	}
}

XMLToken::XMLToken(XMLParser *parser, XMLTokenBackend *backend)
	: type(TYPE_Unknown),
	  attributes(NULL),
	  attributes_count(0),
	  attributes_total(0),
	  parser(parser),
	  backend(backend),
	  docinfo(NULL)
{
}

XMLToken::~XMLToken()
{
	Reset();

	OP_DELETEA(attributes);
}

BOOL
XMLToken::GetLiteralIsWhitespace() const
{
	if (backend)
		return backend->GetLiteralIsWhitespace();

	return ((XMLParserImpl *) parser)->GetInternalParser()->GetLiteralIsWhitespace();
}

const uni_char *
XMLToken::GetLiteralSimpleValue() const
{
	if (backend)
		return backend->GetLiteralSimpleValue();

	return ((XMLParserImpl *) parser)->GetInternalParser()->GetLiteralSimpleValue();
}

uni_char *
XMLToken::GetLiteralAllocatedValue() const
{
	if (backend)
		return backend->GetLiteralAllocatedValue();

	return ((XMLParserImpl *) parser)->GetInternalParser()->GetLiteralAllocatedValue();
}

unsigned
XMLToken::GetLiteralLength() const
{
	if (backend)
		return backend->GetLiteralLength();

	return ((XMLParserImpl *) parser)->GetInternalParser()->GetLiteralLength();
}

OP_STATUS
XMLToken::GetLiteral(Literal &literal) const
{
	if (backend)
		return backend->GetLiteral(literal);

	XMLInternalParser *internalparser = ((XMLParserImpl *) parser)->GetInternalParser();
	unsigned parts_count = internalparser->GetLiteralPartsCount();

	RETURN_IF_ERROR(literal.SetPartsCount(parts_count));

	for (unsigned index = 0; index < parts_count; ++index)
	{
		uni_char *data;
		unsigned data_length;
		BOOL need_copy;

		internalparser->GetLiteralPart(index, data, data_length, need_copy);

		RETURN_IF_ERROR(literal.SetPart(index, data, data_length, need_copy));
	}

	return OpStatus::OK;
}

XMLToken::Attribute *
XMLToken::GetAttribute(const uni_char *qname, unsigned qname_length) const
{
	if (qname_length == ~0u)
		qname_length = uni_strlen(qname);

	XMLCompleteNameN completename(qname, qname_length);

	for (unsigned index = 0; index < attributes_count; ++index)
		if (completename.SameQName(attributes[index].GetName()))
			return &attributes[index];

	return NULL;
}

void
XMLToken::Initialize()
{
	Reset();
}

void
XMLToken::SetName(const XMLCompleteNameN &new_name)
{
	name = new_name;
}

void
XMLToken::SetData(const uni_char *new_data, unsigned new_data_length)
{
	data = new_data;
	data_length = new_data_length;
}

OP_STATUS
XMLToken::AddAttribute(Attribute *&attribute)
{
	if (attributes_count == attributes_total)
	{
		unsigned new_attributes_total = attributes_total == 0 ? 8 : attributes_total + attributes_total;
		Attribute *new_attributes = OP_NEWA(Attribute, new_attributes_total);

		if (!new_attributes)
			return OpStatus::ERR_NO_MEMORY;

		if (attributes)
		{
			op_memcpy(new_attributes, attributes, attributes_count * sizeof attributes[0]);
			op_memset(new_attributes + attributes_count, 0, attributes_total * sizeof attributes[0]);
			OP_DELETEA(attributes);
		}
		else
			op_memset(new_attributes, 0, new_attributes_total * sizeof attributes[0]);

		attributes = new_attributes;
		attributes_total = new_attributes_total;
	}

	attribute = &attributes[attributes_count++];
	return OpStatus::OK;
}

void
XMLToken::RemoveAttribute(unsigned index)
{
	while (++index < attributes_count)
		attributes[index - 1] = attributes[index];

	--attributes_count;
}

void
XMLToken::Reset()
{
	for (unsigned index = 0; index < attributes_count; ++index)
		attributes[index].Reset();

	attributes_count = 0;
}

#ifdef XML_ERRORS

BOOL
XMLToken::GetTokenRange(XMLRange &range)
{
	if (backend)
		return backend->GetTokenRange(range);
	else
		return FALSE;
}

BOOL
XMLToken::GetAttributeRange(XMLRange &range, unsigned index)
{
	if (backend && index < attributes_count && attributes[index].GetSpecified())
		return backend->GetAttributeRange(range, index);
	else
		return FALSE;
}

#endif // XML_ERRORS

/* virtual */
XMLTokenBackend::~XMLTokenBackend()
{
}
