/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND

#include "modules/database/src/webstorage_data_abstraction_simple_impl.h"
#include "modules/database/src/filestorage.h"
#include "modules/xmlutils/xmltoken.h"
#include "modules/xmlutils/xmldocumentinfo.h"
#include "modules/formats/encoder.h"
#include "modules/formats/base64_decode.h"

/**
 * XML tokens
 */
#define FSS_EL_ROOT "ws"
#define FSS_EL_ENTRY "e"
#define FSS_EL_KEY "k"
#define FSS_EL_VALUE "v"

#define FSS_HEADER "<" FSS_EL_ROOT ">\n"
#define FSS_ENTRY_START "<" FSS_EL_ENTRY"><" FSS_EL_KEY ">"
#define FSS_ENTRY_MID "</" FSS_EL_KEY ">\n<" FSS_EL_VALUE ">"
#define FSS_ENTRY_END "</" FSS_EL_VALUE "></" FSS_EL_ENTRY ">\n"
#define FSS_FOOTER "</" FSS_EL_ROOT ">"

#define FSS_STR_LEN(s) (sizeof(s)-1)
#define FSS_STR_LEN_PAIR(s) s,FSS_STR_LEN(s)

#define WEBSTORAGE_ENABLE_SIMPLE_BACKEND_DATA_CHUNK 4096

//////////////////////////////////////////////////////////////////////////
// Implementation of the FileStorageLoader class
//////////////////////////////////////////////////////////////////////////

void FileStorageLoader::SetState(FslState new_state)
{
	OP_ASSERT(new_state != FSL_STATE_IN_ENTRIES || m_state == FSL_STATE_ROOT || m_state == FSL_STATE_IN_ENTRY);
	OP_ASSERT(new_state != FSL_STATE_IN_ENTRY || m_state == FSL_STATE_IN_ENTRIES || m_state == FSL_STATE_IN_KEY || m_state == FSL_STATE_IN_VALUE);
	OP_ASSERT(new_state != FSL_STATE_IN_KEY || m_state == FSL_STATE_IN_ENTRY);
	OP_ASSERT(new_state != FSL_STATE_IN_VALUE || m_state == FSL_STATE_IN_ENTRY);
	OP_ASSERT(new_state != FSL_STATE_ROOT || m_state == FSL_STATE_IN_ENTRIES);
	OP_ASSERT(new_state != FSL_STATE_FAILED || m_state != FSL_STATE_FAILED);

	m_state = new_state;
}

void FileStorageLoader::Finish(OP_STATUS err_val)
{
	if (!m_caller_notified)
	{
		m_caller_notified = TRUE;
		if (OpStatus::IsError(err_val))
			m_callback->OnLoadingFailed(err_val);
		else
			m_callback->OnLoadingFinished();
	}
}

FileStorageLoader::~FileStorageLoader()
{
	OP_DELETE(m_parser);
	// Value will be here if parsing is aborted.
	OP_DELETE(m_current_value);
	OP_DELETE(m_mh);
}

OP_STATUS FileStorageLoader::Load(const uni_char* file_path)
{
	m_caller_notified = FALSE;

	OP_ASSERT(OpDbUtils::IsFilePathAbsolute(file_path));

	if (!m_mh)
	{
		m_mh = OP_NEW(MessageHandler, (NULL));
		RETURN_OOM_IF_NULL(m_mh);
	}

	OpString full_url;
	RETURN_IF_MEMORY_ERROR(full_url.AppendFormat(UNI_L("file://localhost/%s"), file_path));

	DB_DBG(("FileStorageLoader::Load(\"%S\")\n", file_path));

	URL file_url = g_url_api->GetURL(full_url);
	if (file_url.IsEmpty())
		return OpStatus::ERR_FILE_NOT_FOUND;
#ifdef URL_FILTER
	RETURN_IF_ERROR(file_url.SetAttribute(URL::KSkipContentBlocker, TRUE));
#endif // URL_FILTER

	XMLParser* parser;
	RETURN_IF_ERROR(XMLParser::Make(parser, this, m_mh, this, file_url));

	URL dummy_referrer;
	OP_STATUS status = parser->Load(dummy_referrer, FALSE, 0, TRUE);
	if (OpStatus::IsSuccess(status))
		m_parser = parser;
	else
		OP_DELETE(parser);
	return status;
}

/* virtual */ void FileStorageLoader::Continue(XMLParser *parser)
{
	OP_ASSERT(!"TODO(stighal): Implement me!");
}

/* virtual */ void FileStorageLoader::Stopped(XMLParser *parser)
{
	OP_STATUS err = OpStatus::OK;
	if (parser->IsFailed())
		err = PS_Status::ERR_CORRUPTED_FILE;
	else if (parser->IsOutOfMemory())
		err = OpStatus::ERR_NO_MEMORY;
	else {
		OP_ASSERT(m_state == FSL_STATE_ROOT);
	}
	Finish(err);
}

/* virtual */ void FileStorageLoader::ParsingStopped(XMLParser *parser)
{
	OP_STATUS err = OpStatus::OK;
	if (parser->IsFailed())
		err = PS_Status::ERR_CORRUPTED_FILE;
	else if (parser->IsOutOfMemory())
		err = OpStatus::ERR_NO_MEMORY;

	Finish(err);
}

BOOL NamesAreEqual(const XMLCompleteNameN &name_tok, const char *name, unsigned name_len)
{
	return name_tok.GetLocalPartLength() == name_len && uni_strncmp(name_tok.GetLocalPart(), name, name_len) == 0;
}

/* virtual */ XMLTokenHandler::Result FileStorageLoader::HandleToken(XMLToken &token)
{
	XMLToken::Type type = token.GetType();
	if (type == XMLToken::TYPE_STag || type == XMLToken::TYPE_ETag)
	{
		const XMLCompleteNameN& name = token.GetName();
		if (NamesAreEqual(name, FSS_STR_LEN_PAIR(FSS_EL_KEY)))
		{
			if (m_state == FSL_STATE_IN_ENTRY && type == XMLToken::TYPE_STag)
				SetState(FSL_STATE_IN_KEY);
			else if (m_state == FSL_STATE_IN_KEY && type == XMLToken::TYPE_ETag)
				SetState(FSL_STATE_IN_ENTRY);
		}
		else if (NamesAreEqual(name, FSS_STR_LEN_PAIR(FSS_EL_VALUE)))
		{
			if (m_state == FSL_STATE_IN_ENTRY && type == XMLToken::TYPE_STag)
				SetState(FSL_STATE_IN_VALUE);
			else if (m_state == FSL_STATE_IN_VALUE && type == XMLToken::TYPE_ETag)
				SetState(FSL_STATE_IN_ENTRY);
		}
		else if (NamesAreEqual(name, FSS_STR_LEN_PAIR(FSS_EL_ENTRY)))
		{
			if (m_state == FSL_STATE_IN_ENTRIES && type == XMLToken::TYPE_STag)
			{
				if (m_current_value == NULL) {
					m_current_value = OP_NEW(WebStorageValueInfo, ());
					if (m_current_value == NULL)
					{
						SetState(FSL_STATE_FAILED);
						return XMLTokenHandler::RESULT_OOM;
					}
				}
				SetState(FSL_STATE_IN_ENTRY);
			}
			else if (m_state == FSL_STATE_IN_ENTRY && type == XMLToken::TYPE_ETag)
			{
				if (m_current_value)
				{
					OP_STATUS oom = m_callback->OnValueFound(m_current_value);
					m_current_value = NULL;
					if (OpStatus::IsMemoryError(oom))
						return XMLTokenHandler::RESULT_OOM;
				}
				SetState(FSL_STATE_IN_ENTRIES);
			}
		}
		else if (NamesAreEqual(name, FSS_STR_LEN_PAIR(FSS_EL_ROOT)))
		{
			if (m_state == FSL_STATE_ROOT && type == XMLToken::TYPE_STag)
				SetState(FSL_STATE_IN_ENTRIES);
			else if(m_state == FSL_STATE_IN_ENTRIES && type == XMLToken::TYPE_ETag)
				SetState(FSL_STATE_ROOT);
		}
	}
	else if (type == XMLToken::TYPE_Text && (m_state == FSL_STATE_IN_KEY || m_state == FSL_STATE_IN_VALUE))
	{
		// In our debug builds, this assert ensures a <e> element wraped <k> and <v> therefore the file was well created.
		// Also asserts that state switching was well managed.
		OP_ASSERT(m_current_value);

		XMLToken::Literal literal;
		if (OpStatus::IsMemoryError(token.GetLiteral(literal)))
			return XMLTokenHandler::RESULT_OOM;

		unsigned parts_count = literal.GetPartsCount();
		unsigned final_size = 0;
		unsigned i;
		for (i = 0; i < parts_count; i++)
			final_size += literal.GetPartLength(i);

		uni_char *data = OP_NEWA(uni_char, final_size + 1);
		if (!data)
			return XMLTokenHandler::RESULT_OOM;

		unsigned last_part_end = 0;
		for (i = 0; i < parts_count; i++)
		{
			unsigned part_len = literal.GetPartLength(i);
			uni_strncpy(data + last_part_end, literal.GetPart(i), part_len);
			last_part_end += part_len;
		}
		data[last_part_end] = 0;

		OpString8 data8;
		if (OpStatus::IsMemoryError(data8.SetUTF8FromUTF16(data, last_part_end)))
		{
			OP_DELETEA(data);
			return XMLTokenHandler::RESULT_OOM;
		}

		unsigned long read_pos;
		BOOL warning;
		OP_ASSERT((data8.Length() & 0x3) == 0);//multiple of four, else file is bogus
		unsigned long written = GeneralDecodeBase64((const unsigned char*)data8.CStr(), data8.Length(), read_pos, (unsigned char *)data, warning, 0, NULL, NULL);
		OP_ASSERT(!warning);

		unsigned long new_len = UNICODE_DOWNSIZE(written);
		uni_char *data_copy = OP_NEWA(uni_char, new_len + 1);
		if (!data_copy)
		{
			OP_DELETEA(data);
			return XMLTokenHandler::RESULT_OOM;
		}
		op_memcpy(data_copy, data, written);
		data_copy[new_len] = 0;

		OP_DELETEA(data);

		if (m_state == FSL_STATE_IN_KEY)
		{
			// This assert checks for the proper structure of the xml file
			OP_ASSERT(m_current_value->m_key.m_value == NULL );
			m_current_value->m_key.Set(data_copy, new_len);
		}
		else if (m_state == FSL_STATE_IN_VALUE)
		{
			// This assert checks for the proper structure of the xml file
			OP_ASSERT(m_current_value->m_value.m_value == NULL);
			m_current_value->m_value.Set(data_copy, new_len);
		}
	}

	return XMLTokenHandler::RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
// Implementation of the FileStorageSaver class
//////////////////////////////////////////////////////////////////////////

void FileStorageSaver::SetState(FssState new_state)
{
	OP_ASSERT(new_state != FSS_STATE_BEFORE_KEY || m_state == FSS_STATE_PRE_START || m_state == FSS_STATE_AFTER_VALUE);
	OP_ASSERT(new_state != FSS_STATE_KEY_VALUE || m_state == FSS_STATE_BEFORE_KEY);
	OP_ASSERT(new_state != FSS_STATE_AFTER_KEY || m_state == FSS_STATE_KEY_VALUE);
	OP_ASSERT(new_state != FSS_STATE_VALUE_VALUE || m_state == FSS_STATE_AFTER_KEY);
	OP_ASSERT(new_state != FSS_STATE_AFTER_VALUE || m_state == FSS_STATE_VALUE_VALUE);
	OP_ASSERT(new_state != FSS_STATE_AFTER_ENTRIES || m_state == FSS_STATE_BEFORE_KEY || m_state == FSS_STATE_AFTER_ENTRIES);
	OP_ASSERT(new_state != FSS_STATE_FINISHED || m_state == FSS_STATE_AFTER_ENTRIES);
	OP_ASSERT(new_state != FSS_STATE_FAILED);

	m_state = new_state;
}

BOOL FileStorageSaver::Append(const char *str, unsigned str_len, BOOL apply_base64)
{
	int chunk_len;
	const char *source_buffer;

	if (apply_base64)
	{
		if (m_base64_len == 0) // not converted yet
		{
			MIME_Encode_Error error = MIME_Encode_SetStr(m_base64_buffer, m_base64_len, str, str_len, NULL, GEN_BASE64_ONELINE);
			if (error != MIME_NO_ERROR)
			{
				Finish(error == MIME_FAILURE ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR);
				return FALSE;
			}
		}

		source_buffer = m_base64_buffer;
		chunk_len = m_base64_len - m_data_offset;
	}
	else
	{
		source_buffer = str;
		chunk_len = str_len - m_data_offset;
	}

	int source_offset = m_data_offset;
	if ((unsigned)chunk_len >= m_buffer_len - m_used_len)
	{
		chunk_len = m_buffer_len - m_used_len;
		m_data_offset += chunk_len;
	}
	else
		m_data_offset = 0;

	op_memcpy(m_buffer + m_used_len, source_buffer + source_offset, chunk_len);

	m_used_len += chunk_len;

	if (m_data_offset > 0)
	{
		Commit();
		return FALSE;
	}
	else if (apply_base64 && m_base64_len > 0)
	{
		OP_DELETEA(m_base64_buffer);
		m_base64_buffer = NULL;
		m_base64_len = 0;
	}

	return TRUE;
}

void FileStorageSaver::Commit()
{
#ifdef WEBSTORAGE_SIMPLE_BACKEND_ASYNC
	if (!m_synchronous)
	{
		m_out_file.WriteAsync(m_buffer, m_used_len);
		return;
	}
#endif // WEBSTORAGE_SIMPLE_BACKEND_ASYNC

#ifdef DEBUG_ENABLE_OPASSERT
	// Check to help prevent DSK-380695 regressing.
	if (m_commit_count++ == 0)
		OP_ASSERT(m_used_len != 0 && op_memcmp(m_buffer, FSS_HEADER, FSS_STR_LEN(FSS_HEADER)) == 0);
	else
		OP_ASSERT(m_used_len != 0 && op_memcmp(m_buffer, FSS_HEADER, FSS_STR_LEN(FSS_HEADER)) != 0);
#endif // DEBUG_ENABLE_OPASSERT

	OP_STATUS err_val = m_out_file.Write(m_buffer, m_used_len);
	if (OpStatus::IsError(err_val))
		SetState(FSS_STATE_FAILED);

	if (IsStateFinished())
		Finish(err_val);
#ifndef WEBSTORAGE_SIMPLE_BACKEND_ASYNC
	else if (!m_synchronous)
		m_mh->PostMessage(MSG_FSS_CONTINUE_WRITING, reinterpret_cast<MH_PARAM_1>(&m_out_file), 0);
#endif // WEBSTORAGE_SIMPLE_BACKEND_ASYNC
	else
		m_used_len = 0;
}

void FileStorageSaver::Finish(OP_STATUS err_val)
{
	if (!m_caller_notified)
	{
		m_caller_notified = TRUE;
		if (OpStatus::IsError(err_val))
		{
			m_out_file.Close(); // will close the temp file and delete it
			m_callback->OnSavingFailed(err_val);
		}
		else
		{
			m_out_file.SafeClose(); // will close the temp file and replace the real file
			m_callback->OnSavingFinished();
		}
	}
	Clean();
}

FileStorageSaver::~FileStorageSaver()
{
	Clean();
}

void
FileStorageSaver::Clean()
{
	if (m_out_file.IsOpen())
		OpStatus::Ignore(m_out_file.SafeClose());

	m_vector = NULL;
	m_vector_index = 0;
	OP_DELETEA(m_buffer);
	m_buffer = NULL;

#ifndef WEBSTORAGE_SIMPLE_BACKEND_ASYNC
	m_mh->UnsetCallBacks(this);
#endif // WEBSTORAGE_SIMPLE_BACKEND_ASYNC
}

OP_STATUS FileStorageSaver::Save(const uni_char* file_path, const OpVector<WebStorageValueInfo>* vector)
{
	m_caller_notified = FALSE;

	RETURN_IF_ERROR(m_out_file.Construct(file_path, OPFILE_ABSOLUTE_FOLDER));
	RETURN_IF_ERROR(m_out_file.Open(OPFILE_WRITE | OPFILE_SHAREDENYWRITE));

	m_buffer_len = WEBSTORAGE_ENABLE_SIMPLE_BACKEND_DATA_CHUNK;
	RETURN_OOM_IF_NULL(m_buffer = OP_NEWA(char, m_buffer_len));

	m_vector = vector;
	m_vector_index = 0;

#ifndef WEBSTORAGE_SIMPLE_BACKEND_ASYNC
	if (!m_synchronous)
		m_mh->SetCallBack(this, MSG_FSS_CONTINUE_WRITING, reinterpret_cast<MH_PARAM_1>(&m_out_file));
#else // WEBSTORAGE_SIMPLE_BACKEND_ASYNC
	m_out_file.SetListener(this);
#endif // WEBSTORAGE_SIMPLE_BACKEND_ASYNC

	if (m_synchronous)
		Flush();
	else
		WriteNextBlock();

	return OpStatus::OK;
}

void FileStorageSaver::Flush()
{
	if (m_out_file.IsOpen())
		// This flushes pending data and forces the buffer
		// index to move forward.
		OnDataWritten(&m_out_file, m_out_file.Flush(), 0);

	m_synchronous = TRUE;
	do
		WriteNextBlock();
	while (!HasCompleted());
}

void FileStorageSaver::WriteNextBlock()
{
	if (IsStateFinished())
		return;

	// 1st run.
	if (m_state == FSS_STATE_PRE_START)
	{
		if (!Append(FSS_STR_LEN_PAIR(FSS_HEADER), FALSE))
			return;
		SetState(FSS_STATE_BEFORE_KEY);
	}

	if (m_state != FSS_STATE_AFTER_ENTRIES)
	{
		// NULL check because of crash http://crashlog/show_stacktrace?id=11697967
		// Solved elsewhere, but check left as safeguard.
		OP_ASSERT(m_vector);
		if (m_vector == NULL)
		{
			Finish(OpStatus::ERR);
			return;
		}

		while (m_vector_index < m_vector->GetCount())
		{
			WebStorageValueInfo *value = m_vector->Get(m_vector_index);

			switch (m_state)
			{
			case FSS_STATE_BEFORE_KEY:
				if (!Append(FSS_STR_LEN_PAIR(FSS_ENTRY_START), FALSE))
					return;

				SetState(FSS_STATE_KEY_VALUE);

				// fall through
			case FSS_STATE_KEY_VALUE:
				if (value->m_key.m_value_length != 0 && !Append((char*)value->m_key.m_value, UNICODE_SIZE(value->m_key.m_value_length), TRUE))
					return;

				SetState(FSS_STATE_AFTER_KEY);

				// fall through
			case FSS_STATE_AFTER_KEY:
				if (!Append(FSS_STR_LEN_PAIR(FSS_ENTRY_MID), FALSE))
					return;

				SetState(FSS_STATE_VALUE_VALUE);

				// fall through
			case FSS_STATE_VALUE_VALUE:
				if (value->m_value.m_value_length != 0 && !Append((char*)value->m_value.m_value, UNICODE_SIZE(value->m_value.m_value_length), TRUE))
					return;

				SetState(FSS_STATE_AFTER_VALUE);

				// fall through
			case FSS_STATE_AFTER_VALUE:
				if (!Append(FSS_STR_LEN_PAIR(FSS_ENTRY_END), FALSE))
					return;

				SetState(FSS_STATE_BEFORE_KEY);
				break;

			default:
				OP_ASSERT(!"Bad state");
				Finish(OpStatus::ERR);
				return;
			}

			m_vector_index++;
		}
		// Loop ended.
		SetState(FSS_STATE_AFTER_ENTRIES);
	}

	if (!Append(FSS_STR_LEN_PAIR(FSS_FOOTER), FALSE))
		return;

	SetState(FSS_STATE_FINISHED);
	Commit();
}

/* virtual */ void FileStorageSaver::OnDataWritten(OpFile* file, OP_STATUS result, OpFileLength len)
{
	if (OpStatus::IsError(result))
		SetState(FSS_STATE_FAILED);

	if (IsStateFinished())
		Finish(result);
	else
	{
		m_used_len = 0;
		WriteNextBlock();
	}
}

#ifndef WEBSTORAGE_SIMPLE_BACKEND_ASYNC
/* virtual */ void FileStorageSaver::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_FSS_CONTINUE_WRITING && par1 == reinterpret_cast<MH_PARAM_1>(&m_out_file))
		OnDataWritten(&m_out_file, OpStatus::OK, 0);
}
#endif // WEBSTORAGE_SIMPLE_BACKEND_ASYNC

#endif // WEBSTORAGE_ENABLE_SIMPLE_BACKEND
