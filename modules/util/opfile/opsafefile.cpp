/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

// Implementation of OpSafeFile, a class for overwriting a file in a safe manner.

#include "core/pch.h"

#include "modules/util/opfile/opsafefile.h"

OpSafeFile::~OpSafeFile()
{
	if (m_file)
	{
		if (m_file->IsOpen())
		{
			m_file->Close();
		}
		m_file->Delete();
	}

	if (m_original_file_is_copy)
	{
		// Avoid deletion of copied OpLowLevelFile
		m_original_file.m_file = NULL;
	}
}
OP_STATUS
OpSafeFile::Construct(const OpStringC& path, OpFileFolder folder, int flags)
{
	RETURN_IF_ERROR(m_original_file.Construct(path, folder, flags));
	m_file = m_original_file.m_file->CreateTempFile(UNI_L("opr"));

	if (m_file == NULL)
	{
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}
OP_STATUS
OpSafeFile::Construct(OpFile* file)
{
	m_original_file.m_file = file->m_file;
	m_original_file_is_copy = TRUE;
	m_file = m_original_file.m_file->CreateTempFile(UNI_L("opr"));

	if (m_file == NULL)
	{
		return OpStatus::ERR; 
	}

	return OpStatus::OK;
}

OP_STATUS
OpSafeFile::SafeClose()
{
	if (m_original_file.m_file->IsOpen())
	{
		m_original_file.m_file->SafeClose();
	}

	OP_STATUS res = OpStatus::OK;

    if (m_file)
    {
    	if (m_file->IsOpen())
    	{
    		m_file->SafeClose();
    	}

    	res = m_original_file.m_file->SafeReplace(m_file);
    	m_file->Delete();
    	OP_DELETE(m_file);
    	m_file = NULL;
    }

	return res;
}

OP_STATUS
OpSafeFile::Close()
{
	if (m_original_file.m_file->IsOpen())
	{
		m_original_file.m_file->Close();
	}

	OP_STATUS res = OpStatus::OK;

	if (m_file)
	{
		if (m_file->IsOpen())
		{
			m_file->Close();
		}
		res = m_original_file.m_file->SafeReplace(m_file);
		m_file->Delete();
		OP_DELETE(m_file);
		m_file = NULL;
	}
	return res;
}

