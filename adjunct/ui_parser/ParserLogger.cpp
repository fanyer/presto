/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#include "adjunct/ui_parser/ParserLogger.h"

ParserLogger::AutoIndenter::AutoIndenter(ParserLogger& logger, const OpStringC8& message, const OpStringC8& param)
	: m_logger(logger)
	, m_success(false)
{
	OpStatus::Ignore(m_message.AppendFormat(message.CStr(), param.CStr()));
	if (m_message.HasContent())
		m_logger.OutputEntry("%s...", m_message);
	m_logger.IncreaseIndentation();
}

ParserLogger::AutoIndenter::~AutoIndenter()
{
	m_logger.DecreaseIndentation();
	if (m_message.HasContent())
		m_logger.OutputEntry(m_success ? "Done %s" : "ERROR %s", m_message);
}

////////// OutputEntryInternal
OP_STATUS ParserLogger::OutputEntryInternal(const OpStringC8& entry)
{
	if (m_indentation == 0)
		return FileLogTarget::OutputEntry(entry);

	OpString8 indented;
	int entry_length = entry.Length();
	char* indented_ptr = indented.Reserve(entry_length + m_indentation * 2);
	RETURN_OOM_IF_NULL(indented_ptr);
	
	op_memset(indented_ptr, ' ', m_indentation * 2);
	indented_ptr += m_indentation * 2;
	if (entry_length > 0)
		op_memcpy(indented_ptr, entry.CStr(), entry_length);
	indented_ptr[entry_length] = '\0';

	return FileLogTarget::OutputEntry(indented);
}

////////// OutputEntryInternal
OP_STATUS ParserLogger::OutputEntryInternal(const OpStringC8& entry, const OpStringC8 & param)
{
	OpString8 full_entry;
	RETURN_IF_ERROR(full_entry.AppendFormat(entry.CStr(), param.CStr()));

	return OutputEntry(full_entry);
}
