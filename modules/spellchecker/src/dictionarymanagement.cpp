/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef INTERNAL_SPELLCHECK_SUPPORT

#include "modules/spellchecker/opspellcheckerapi.h"
#include "modules/spellchecker/src/opspellchecker.h"

OP_STATUS OwnFileFromDicPath(const uni_char * dictionary_path, OpFile& own_file)
{
	if (!dictionary_path)
		return SPC_ERROR(OpStatus::ERR);

	const uni_char * str = uni_strrchr(dictionary_path, PATHSEPCHAR);
	if (str == NULL)
		return SPC_ERROR(OpStatus::ERR);

	str++;

	OpString own_path;
	OP_STATUS status = OpStatus::OK;
	status = own_path.Set(str);
	if (OpStatus::IsError(status))
		return SPC_ERROR(status);

	int len = own_path.Length();
	if (len < 5)
		return SPC_ERROR(OpStatus::ERR);

	uni_strcpy(own_path.CStr()+len-3, UNI_L("oow"));

	status = own_file.Construct(own_path.CStr(), OPFILE_DICTIONARY_FOLDER);
	if (OpStatus::IsError(status))
		return SPC_ERROR(status);

	return OpStatus::OK;
}


OP_STATUS OpSpellChecker::RemoveStringsFromOwnFile(const uni_char *str, BOOL in_added_words, BOOL &removed)
{
	if (!m_own_path || *m_own_path == '\0')
		return OpStatus::OK;
	OpFile own_file;
	int file_len = 0;
	RETURN_IF_ERROR(SCOpenFile(m_own_path, own_file, file_len, OPFILE_UPDATE));
	UINT8 *file_data = OP_NEWA(UINT8, file_len);
	if (!file_data)
	{
		own_file.Close();
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	}
	OpFileLength bytes_read = 0;
	OP_STATUS status = own_file.Read(file_data, (OpFileLength)file_len, &bytes_read);
	if (OpStatus::IsError(status) || bytes_read != (OpFileLength)file_len)
	{
		own_file.Close();
		OP_DELETEA(file_data);
		return SPC_ERROR(OpStatus::IsError(status) ? status : OpStatus::ERR);
	}
	UINT8 *str_buf = TempBuffer();
	to_utf8((char*) str_buf, str, SPC_TEMP_BUFFER_BYTES);

	int ofs;
	UINT8 *end;
	for (ofs = 0; ofs < file_len && file_data[ofs]; ofs++);
	if (in_added_words)
	{
		end = file_data + ofs;
		ofs = 0;
	}
	else
	{
		end = file_data + file_len;
		ofs++;
	}
	UINT8 *start = file_data + ofs;
	removed = FALSE;
	while (start < end)
	{
		UINT8 *ptr = start;
		while (ptr != end && !is_ascii_new_line(*ptr))
			ptr++;
		BOOL removed_new_line = FALSE;
		if (ptr != end)
		{
			ptr++;
			removed_new_line = TRUE;
		}
		int len = (int)(ptr-start);
		if (len && str_equal(start, str_buf, len - (removed_new_line ? 1 : 0)))
		{
			int i;
			int to_copy = file_len - (int)(ptr-file_data);
			for (i=0;i<to_copy;i++)
				start[i] = start[i+len];
			removed = TRUE;
			end -= len;
			file_len -= len;
		}
		else
			start = ptr;
		while (start < end && is_ascii_new_line(*start))
			start++;
	}
	ReleaseTempBuffer();
	own_file.Close();
	status = OpStatus::OK;
	if (removed)
	{
		// Write down the changed file
		int dummy = 0;
		status = SCOpenFile(m_own_path, own_file, dummy, OPFILE_WRITE);
		if (OpStatus::IsSuccess(status))
		{
			status = own_file.SetFileLength(0);
			if (OpStatus::IsSuccess(status))
				status = own_file.Write(file_data, (OpFileLength)file_len);
			else
			{
				OP_ASSERT(!"We just destroyed the own word database. Doh. Should be using OpSafeFile or something");
			}
			own_file.Close();
		}
	}
	OP_DELETEA(file_data);
	return status;
}

OP_STATUS OpSpellChecker::CreateOwnFileIfNotExists()
{
	if (m_own_path && *m_own_path)
		return OpStatus::OK;

	OP_STATUS status = OpStatus::OK;
	OpFile own_file;

	status = OwnFileFromDicPath(m_dictionary_path, own_file);
	if (OpStatus::IsError(status))
		return SPC_ERROR(status);

	status = own_file.Open(OPFILE_OVERWRITE);
	if (OpStatus::IsError(status))
		return SPC_ERROR(status);
	status = own_file.Write("\0",1);
	if (OpStatus::IsError(status))
		return SPC_ERROR(status);
	OpString own_path;
	status = own_path.Set(own_file.GetFullPath());
	if (OpStatus::IsError(status))
		return SPC_ERROR(status);
	status = m_language->SetOwnFilePath(own_path.CStr());
	if (OpStatus::IsError(status))
		return SPC_ERROR(status);
	return SPC_ERROR(UniSetStr(m_own_path, own_path.CStr()));
}

BOOL OpSpellChecker::CanRepresentInDictionaryEncoding(const uni_char *str)
{
	int i;
	if (!m_8bit_enc || !str || !*str)
		return TRUE;
	UINT8 *buffer = (UINT8*)TempBuffer();
	int was_read = 0;
	int to_write = uni_strlen(str)+1;
	int to_read = to_write*sizeof(uni_char);
	int was_written = m_output_converter->Convert(str, to_read, buffer, SPC_TEMP_BUFFER_BYTES, &was_read);
	if (was_written != to_write || was_read != to_read)
	{
		ReleaseTempBuffer();
		return FALSE;
	}
	int qm = 0;
	for (i=0;i<to_write-1;i++)
		qm += str[i] == '\?' ? 1 : 0;
	for (i=0;i<to_write-1;i++)
		qm -= buffer[i] == '\?' ? 1 : 0;
	ReleaseTempBuffer();
	return !qm;
}


#ifndef SPC_USE_HUNSPELL_ENGINE

OP_STATUS OpSpellChecker::RemoveWordList(UINT8 *data, int len)
{
	if (!data || !len || !*data)
		return OpStatus::OK;
	BOOL finished = FALSE;
	uni_char *buf = (uni_char*)TempBuffer();
	UINT8 *str;
	while (!finished)
	{
		str = data;
		while (!is_ascii_new_line(*data) && *data) // data is '\0' terminated
			data++;
		if (!*data)
			finished = TRUE;
		*data = '\0';
		if (data != str)
		{
			RETURN_IF_ERROR(ConvertStringToUTF16(str, buf, SPC_TEMP_BUFFER_BYTES));
			RETURN_IF_ERROR(m_removed_words.AddString(buf,FALSE));
		}
		while (!finished && is_ascii_new_line(*(++data)));
	}
	ReleaseTempBuffer();
	RETURN_IF_ERROR(m_removed_words.Sort());
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::AddWord(const uni_char *word, BOOL permanently)
{
	OP_STATUS status;
	if (!CanAddWord(word) || m_added_words.HasString(word))
		return OpStatus::OK;
	if (m_removed_words.HasString(word))
	{
		BOOL dummy = FALSE;
		m_removed_words.RemoveString(word);
		if (permanently)
			RETURN_IF_ERROR(RemoveStringsFromOwnFile(word,FALSE,dummy));
		BOOL has_word = FALSE;
		RETURN_IF_ERROR(IsWordInDictionary(word, has_word));
		if (has_word && !m_added_from_file_and_later_removed.HasString(word))
			return OpStatus::OK;
	}
	else
		RETURN_IF_ERROR(m_added_words.AddString(word));
	if (!permanently)
		return OpStatus::OK;
	RETURN_IF_ERROR(CreateOwnFileIfNotExists());
	OpFile own_file;
	int file_len = 0;
	RETURN_IF_ERROR(SCOpenFile(m_own_path, own_file, file_len, OPFILE_READ));
	UINT8 *buf = TempBuffer();
	status = ConvertStringFromUTF16(word, buf, SPC_TEMP_BUFFER_BYTES);
	int word_bytes = op_strlen((const char*)buf) + 1;
	UINT8 *new_data = OP_NEWA(UINT8, file_len+word_bytes);
	if (OpStatus::IsError(status) || !new_data)
	{
		own_file.Close();
		ReleaseTempBuffer();
		OP_DELETEA(new_data);
		return SPC_ERROR(OpStatus::IsError(status) ? status : OpStatus::ERR_NO_MEMORY);
	}
	op_strcpy((char*)new_data, (const char*)buf);
	new_data[word_bytes-1] = '\n';
	OpFileLength bytes_read = 0;
	status = own_file.Read(new_data+word_bytes, (OpFileLength)file_len, &bytes_read);
	if (OpStatus::IsSuccess(status))
	{
		own_file.Close();
		int dummy = 0;
		status = SCOpenFile(m_own_path, own_file, dummy, OPFILE_WRITE);
		if (OpStatus::IsSuccess(status))
			status = own_file.SetFileLength(0);
		if (OpStatus::IsSuccess(status))
			status = own_file.SetFilePos(0); // not so necessary I guess...
		if (OpStatus::IsSuccess(status))
			status = own_file.Write(new_data, (OpFileLength)(file_len+word_bytes));
	}
	own_file.Close();
	ReleaseTempBuffer();
	OP_DELETEA(new_data);
	return OpStatus::IsError(status) ? SPC_ERROR(status) : OpStatus::OK;
}

OP_STATUS OpSpellChecker::RemoveWord(const uni_char *word, BOOL permanently)
{
	OP_STATUS status;
	const uni_char* word_end = word + uni_strlen(word);
	if (!CanSpellCheck(word, word_end) || m_removed_words.HasString(word))
		return OpStatus::OK;
	BOOL removed_from_file = FALSE;
	if (permanently)
		RETURN_IF_ERROR(RemoveStringsFromOwnFile(word,TRUE,removed_from_file));
	if (m_added_words.HasString(word))
	{
		m_added_words.RemoveString(word);
		BOOL has_word = FALSE;
		RETURN_IF_ERROR(IsWordInDictionary(word, has_word));
		if (!has_word)
			return OpStatus::OK;
	}
	else if (removed_from_file && !m_added_from_file_and_later_removed.HasString(word))
		RETURN_IF_ERROR(m_added_from_file_and_later_removed.AddString(word));
	RETURN_IF_ERROR(m_removed_words.AddString(word));
	if (!permanently || removed_from_file)
		return OpStatus::OK;
	RETURN_IF_ERROR(CreateOwnFileIfNotExists());
	OpFile own_file;
	int file_len = 0;
	RETURN_IF_ERROR(SCOpenFile(m_own_path, own_file, file_len, OPFILE_APPEND));
	UINT8 *buf = TempBuffer();
	status = ConvertStringFromUTF16(word, buf, SPC_TEMP_BUFFER_BYTES);
	int word_bytes = op_strlen((const char*)buf);
	if (OpStatus::IsSuccess(status))
		status = own_file.Write(buf,(OpFileLength)word_bytes);
	if (OpStatus::IsSuccess(status))
		status = own_file.Write("\n",1);
	own_file.Close();
	ReleaseTempBuffer();
	return OpStatus::IsError(status) ? SPC_ERROR(status) : OpStatus::OK;
}
#endif // !SPC_USE_HUNSPELL_ENGINE

#endif // INTERNAL_SPELLCHECK_SUPPORT
