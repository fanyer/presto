/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/windows_common/utils/fileinfo.h"
#include "modules/opdata/UniString.h"

WindowsCommonUtils::FileInformation::FileInformation()
	: m_versioninfo(NULL),
	m_data(NULL)
{
	m_langid[0] = 0;
}

WindowsCommonUtils::FileInformation::~FileInformation()
{
	if (m_data)
		op_free(m_data);
}

OP_STATUS WindowsCommonUtils::FileInformation::Construct(const uni_char* process_path)
{
	UINT version_len;

	if (!process_path || *process_path == 0)
	{
		OP_ASSERT(false);
		return OpStatus::ERR_NULL_POINTER;
	}

	DWORD fileinfo_len = GetFileVersionInfoSize((LPTSTR)process_path, NULL);
	RETURN_VALUE_IF_NULL(fileinfo_len, OpStatus::ERR);

	m_data = (LPVOID)op_malloc(fileinfo_len);
	RETURN_OOM_IF_NULL(m_data);

	if (!GetFileVersionInfo((LPTSTR)process_path, NULL, fileinfo_len, m_data))
	{
		op_free(m_data);
		m_data = NULL;
		return OpStatus::ERR;
	}

	// Get root block.
	if (!VerQueryValue(m_data, L"\\", (void **)&m_versioninfo, (UINT *)&version_len))
	{
		op_free(m_data);
		m_data = NULL;
		return OpStatus::ERR;
	}

	SetDefaultLanguage();

	return OpStatus::OK;
}

void WindowsCommonUtils::FileInformation::SetDefaultLanguage()
{
	UINT translation_len = 0;
	// Structure of language identifier.
	struct TRANSL { WORD lang; WORD cp; } *translation;

	// Use first language that was found.
	if (VerQueryValue(m_data, L"\\VarFileInfo\\Translation", (LPVOID*)&translation, (UINT*)&translation_len)
		&& translation_len && translation)
	{
		uni_sprintf(m_langid, UNI_L("%04x%04x"), translation[0].lang, translation[0].cp);
	}
	else
	{
		// 041904b0 is a very common one, because it means:
		//   US English/Russia, Windows MultiLingual characterset
		// Or to pull it all apart:
		// 04------        = SUBLANG_ENGLISH_USA
		// --09----        = LANG_ENGLISH
		// --19----        = LANG_RUSSIA
		// ----04b0 = 1200 = Codepage for Windows:Multilingual
		uni_strcpy(m_langid, UNI_L("041904b0"));
	}
}

OP_STATUS WindowsCommonUtils::FileInformation::GetInfoItem(const uni_char* info_item, UniString& result, const uni_char* lang_override)
{
	UniString sub_block;
	// String pointer to item text.
	uni_char* item_value = NULL;
	// Length of string pointing to item text.
	UINT item_len = 0;

	if (lang_override)
		RETURN_IF_ERROR(sub_block.AppendFormat(UNI_L("\\StringFileInfo\\%s\\%s"), lang_override, info_item));
	else
		RETURN_IF_ERROR(sub_block.AppendFormat(UNI_L("\\StringFileInfo\\%s\\%s"), m_langid, info_item));

	const uni_char* sub_block_ptr;
	RETURN_IF_ERROR(sub_block.CreatePtr(&sub_block_ptr, TRUE));

	if (!(VerQueryValue(m_data, sub_block_ptr, (void**)&item_value, (UINT*)&item_len) && item_len && item_value))
		return OpStatus::ERR;

	return result.SetCopyData(item_value);
}