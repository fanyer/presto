/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#ifdef HC_MESSAGE_LOGGING

#include "modules/hardcore/logging/OpFileMessageLogger.h"

#include "modules/hardcore/logging/OpReadableLogFormatter.h"
#include "modules/util/opfile/opfile.h"
#include "modules/stdlib/include/opera_stdlib.h"
#include "modules/hardcore/base/opstatus.h"

OpFileMessageLogger::OpFileMessageLogger(
	OpSharedPtr<OpFile> logFile,
	OpSharedPtr<OpLogFormatter> logFormatter)
	: m_logFile(logFile), m_logFormatter(logFormatter),
	  m_ableToLog(true)
{
	if(m_logFile.get() == NULL || m_logFormatter.get() == NULL)
	{
		m_ableToLog = false;
		return;
	}
	if(m_logFile->IsOpen() && !m_logFile->IsWritable())
		CheckOpStatus(m_logFile->Close());
	if(!m_logFile->IsOpen())
		CheckOpStatus(m_logFile->Open(OPFILE_WRITE | OPFILE_TEXT));
	OpUniStringStream ss;
	CheckOpStatus(m_logFormatter->FormatBeginLogging(ss));
	WriteToFile(ss);
}

OpFileMessageLogger::~OpFileMessageLogger()
{
	if(m_ableToLog)
	{
		OpUniStringStream ss;
		m_logFormatter->FormatEndLogging(ss);
		WriteToFile(ss);
	}
}

bool OpFileMessageLogger::IsAbleToLog() const
{
	return m_ableToLog;
}

void OpFileMessageLogger::CheckOpStatus(OP_STATUS in)
{
	if(!m_ableToLog) return;
	if (!OpStatus::IsSuccess(in))
		m_ableToLog = false;
}

OpSharedPtr<OpFileMessageLogger> OpFileMessageLogger::Create(const uni_char* logFileNameBase)
{
	OpUniStringStream fileName(logFileNameBase);
	fileName << g_component_manager->GetAddress().cm << UNI_L(".txt");
	OpSharedPtr<OpFile> logFile = make_shared<OpFile>();
	if( logFile.get() &&
		OpStatus::IsSuccess(logFile->Construct(fileName.Str())) &&
		OpStatus::IsSuccess(logFile->Open(OPFILE_WRITE | OPFILE_TEXT)))
	{
		OpSharedPtr<OpLogFormatter> logFormatter =
				make_shared<OpReadableLogFormatter>(g_component_manager);
		return make_shared<OpFileMessageLogger>(logFile, logFormatter);
	}
	return OpSharedPtr<OpFileMessageLogger>();
}

void OpFileMessageLogger::WriteToFile(const OpUniStringStream& ss)
{

	OpString8 utf8;
	CheckOpStatus(utf8.SetUTF8FromUTF16(ss.Str()));
	if(m_ableToLog)
	{
		CheckOpStatus(m_logFile->Write(utf8.CStr(), utf8.Length()));
	}
}

void OpFileMessageLogger::BeforeDispatch(const OpTypedMessage* msg)
{
	if(!m_ableToLog || !msg) return;
	OpUniStringStream ss;
	CheckOpStatus(m_logFormatter->FormatStartedDispatching(ss, *msg));
	WriteToFile(ss);
}

void OpFileMessageLogger::AfterDispatch(const OpTypedMessage* msg)
{
	if(!m_ableToLog || !msg) return;
	OpUniStringStream ss;
	CheckOpStatus(m_logFormatter->FormatFinishedDispatching(ss, *msg));
	WriteToFile(ss);
}

void OpFileMessageLogger::Log(const OpTypedMessage* msg, const uni_char* action)
{
	if(!m_ableToLog || !msg) return;
	OpUniStringStream ss;
	CheckOpStatus(m_logFormatter->FormatLog(ss, *msg, action));
	WriteToFile(ss);
}

#endif // HC_MESSAGE_LOGGING
