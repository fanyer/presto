/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef IRC_SUPPORT

#include <ctype.h>

#include "irc-message.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/m2/src/util/str/strutil.h"
#include "modules/encodings/detector/charsetdetector.h"

//********************************************************************
//
//	CTCPInformation
//
//********************************************************************

OP_STATUS CTCPInformation::Init(const OpStringC& ctcp_message)
{
	StringTokenizer tokenizer(ctcp_message);

	RETURN_IF_ERROR(tokenizer.NextToken(m_type));
	m_type.MakeUpper();

	RETURN_IF_ERROR(tokenizer.RemainingString(m_parameters_string));

	while (tokenizer.HasMoreTokens())
	{
		OpString remaining_string;

		OpString *tok = OP_NEW(OpString, ());
		if (!tok)
			return OpStatus::ERR_NO_MEMORY;

		OpAutoPtr<OpString> token(tok);

		RETURN_IF_ERROR(tokenizer.RemainingString(remaining_string));
		RETURN_IF_ERROR(tokenizer.NextToken(*tok));

		// Support the fact that a parameter can contain spaces if it starts
		// with a quote marker.
		if (tok->HasContent() && ((*tok)[0] == '\"'))
		{
			// Find the end marker if it exist.
			const int end_pos = StringUtils::Find(remaining_string, UNI_L("\""), 1);
			if (end_pos != KNotFound)
			{
				RETURN_IF_ERROR(tok->Set(remaining_string.SubString(1), end_pos - 1));

				// Reinitialize the tokenizer with the remaining string.
				RETURN_IF_ERROR(tokenizer.Init(remaining_string.SubString(end_pos + 1)));
			}
		}

		RETURN_IF_ERROR(m_parameters.Add(tok));
		token.release();
	}

	return OpStatus::OK;
}


const OpStringC& CTCPInformation::Parameter(UINT index) const
{
	OP_ASSERT(index < ParameterCount());
	return *m_parameters.Get(index);
}


//********************************************************************
//
//	IRCMessage
//
//********************************************************************

IRCMessage::IRCMessage(IRCMessage const &other)
{
	m_prefix.Set(other.m_prefix);
	m_command.Set(other.m_command);
	m_parameters.Set(other.m_parameters);

	m_is_ctcp = other.m_is_ctcp;

	// Loop each parameter and create a new one.
	const UINT32 parameter_count = other.m_parameter_collection.GetCount();
	for (UINT32 index = 0; index < parameter_count; ++index)
	{
		OpString *cur_parameter = other.m_parameter_collection.Get(index);
		OP_ASSERT(cur_parameter != 0);

		AddParameter(*cur_parameter);
	}
}


OP_STATUS IRCMessage::FromRawLine(const OpStringC& raw_line)
{
	return Parse(raw_line, OpStringC8(), OpStringC8());
}


OP_STATUS IRCMessage::FromRawLine(const OpStringC8& raw_line,
	const OpStringC8& charset, const OpStringC8& server)
{
	OpString raw_line16;
	RETURN_IF_ERROR(raw_line16.Set(raw_line));

	RETURN_IF_ERROR(Parse(raw_line16, charset, server));
	return OpStatus::OK;
}


OP_STATUS IRCMessage::MessageSender(OpString &sender) const
{
	RETURN_IF_ERROR(sender.Set(m_prefix));

	// If there is a ! we must extract the nickname from the front.
	const int find_pos = sender.Find("!");
	if (find_pos != KNotFound)
		sender.Delete(find_pos);

	return OpStatus::OK;
}


OP_STATUS IRCMessage::UserInformation(OpString& user_id, OpString& host) const
{
	StringTokenizer tokenizer1(m_prefix, UNI_L("!"));
	OpString user_id_and_host;

	RETURN_IF_ERROR(tokenizer1.SkipToken()); // Skip the left part of '!'.
	RETURN_IF_ERROR(tokenizer1.NextToken(user_id_and_host));

	StringTokenizer tokenizer2(user_id_and_host, UNI_L("@"));

	RETURN_IF_ERROR(tokenizer2.NextToken(user_id));
	RETURN_IF_ERROR(tokenizer2.NextToken(host));

	return OpStatus::OK;
}


OP_STATUS IRCMessage::CTCPInfo(CTCPInformation& ctcp_info) const
{
	OP_ASSERT(IsCTCPMessage());

	if (ParameterCount() < 2)
		return OpStatus::ERR;

	RETURN_IF_ERROR(ctcp_info.Init(Parameter(1)));
	return OpStatus::OK;
}


UINT IRCMessage::NumericReplyValue() const
{
	OP_ASSERT(IsNumericReply());
	return uni_atoi(m_command.CStr());
}


BOOL IRCMessage::SenderIsServer() const
{
	BOOL is_server = FALSE;

	// A userhost will contain a @. Simple check which should work.
	if (m_prefix.Find("@") == KNotFound)
		is_server = TRUE;

	return is_server;
}


BOOL IRCMessage::IsNumericReply() const
{
	return uni_isdigit(m_command.CStr()[0]) > 0;
}


const OpStringC& IRCMessage::Parameter(UINT Index) const
{
	OP_ASSERT(Index < ParameterCount());
	return *m_parameter_collection.Get(Index);
}


OP_STATUS IRCMessage::SetPrefix(const OpStringC& prefix)
{
	RETURN_IF_ERROR(m_prefix.Set(prefix));
	return OpStatus::OK;
}


OP_STATUS IRCMessage::SetCommand(const OpStringC& command)
{
	RETURN_IF_ERROR(m_command.Set(command));
	m_command.MakeUpper();

	return OpStatus::OK;
}


OP_STATUS IRCMessage::AddParameter(const OpStringC& parameter)
{
	OpString *param_cpy = OP_NEW(OpString, ());
	if (!param_cpy)
		return OpStatus::ERR_NO_MEMORY;

	OpAutoPtr<OpString> parameter_copy(param_cpy);

	RETURN_IF_ERROR(param_cpy->Set(parameter));
	RETURN_IF_ERROR(m_parameter_collection.Add(param_cpy));

	parameter_copy.release();
	return OpStatus::OK;
}


OP_STATUS IRCMessage::RawMessages(OpAutoVector<OpString8> &raw_messages, const OpStringC8& charset) const
{
	OpString8 temp_message;

	// Append the prefix. You should never have a prefix if composing an IRC
	// message yourself, but I'm adding it here non the less, in case you have
	// an incoming IRC message and, for some reason, want to convert back to
	// the raw message.
	if (m_prefix.HasContent())
	{
		OpString8 prefix8;
		prefix8.Set(m_prefix.CStr());

		temp_message.Append(":");
		temp_message.Append(prefix8);
		temp_message.Append(" ");
	}

	// Append the command.
	{
		OpString8 command8;
		command8.Set(m_command.CStr());

		temp_message.Append(command8);
	}

	UINT max_bytes_for_word = m_max_byte_length - temp_message.Length() - 1;

	// Make a copy of the charset, since MessageEngine::ConvertToBestChar8()
	// may change the charset value.
	OpString8 preferred_charset;
	preferred_charset.Set(charset);

	const unsigned int parameter_count = ParameterCount();

	// Create a string for the raw message.
	OpString8 *raw_message = 0;
	RETURN_IF_ERROR(CreateNewRawMessage(&raw_message, temp_message));

	if (parameter_count == 0)
		temp_message.Empty();

	// Append the parameters.
	for (UINT32 index = 0; index < parameter_count; ++index)
	{
		OpString current_parameter;
		current_parameter.Set(Parameter(index));

		const BOOL is_first_parameter = (index == 0);
		const BOOL is_last_parameter = ((index + 1) == parameter_count);

		if (is_first_parameter && m_command == "USER")
		{
			// Some IRC servers, like FreeNode, doesn't like periods in the
			// user id, so replace those if existing.
			OpStatus::Ignore(StringUtils::Replace(current_parameter, UNI_L("."), UNI_L("_")));
		}

		if (is_last_parameter && (m_command == "PRIVMSG" || m_command == "NOTICE"))
		{
			// Always add a leading colon in this case.
			raw_message->Append(" :");

			// Add a ctcp marker if needed.
			if (m_is_ctcp)
			{
				CTCPInformation ctcp_info;
				RETURN_IF_ERROR(CTCPInfo(ctcp_info));

				OpString8 ctcp_request8;
				RETURN_IF_ERROR(ctcp_request8.Set(ctcp_info.Type().CStr()));

				const char ctcp_marker[2] = { m_ctcp_marker, 0 };
				RETURN_IF_ERROR(raw_message->Append(ctcp_marker));
				RETURN_IF_ERROR(raw_message->Append(ctcp_request8));

				RETURN_IF_ERROR(current_parameter.Set(ctcp_info.Parameters()));

				if (current_parameter.HasContent())
					raw_message->Append(" ");
			}

			// Store the content in a temporary. This will always be the bare
			// minimum of what we need from now on.
			temp_message.Set(*raw_message);
			max_bytes_for_word = m_max_byte_length - temp_message.Length() - 1;

			// Add word by word.
			BOOL done = current_parameter.IsEmpty();
			while (!done)
			{
				if (current_parameter[0] == '\r' || current_parameter[0] == '\n')
				{
					if (current_parameter[0] == '\n')
						current_parameter.Delete(0, 1);
					else
					{
						if (current_parameter.Length() > 1 && current_parameter[1] == '\n')
							current_parameter.Delete(0, 2);
					}

					if (raw_message->Compare(temp_message) != 0)
					{
						RETURN_IF_ERROR(AddRawMessage(raw_messages, raw_message));
					}

					RETURN_IF_ERROR(CreateNewRawMessage(&raw_message, temp_message));
				}

				OpString word;
				const int break_pos = FindFirstWord(current_parameter, word);

				UINT characters_added = 0;
				if (!AddWordToRawMessage(*raw_message, preferred_charset, word, max_bytes_for_word, characters_added))
				{
					current_parameter.Delete(0, characters_added);

					// Ok, we have exhausted this message. Add it to the vector.
					RETURN_IF_ERROR(AddCTCPMarkerIfNeeded(*raw_message));

					RETURN_IF_ERROR(AddRawMessage(raw_messages, raw_message));
					RETURN_IF_ERROR(CreateNewRawMessage(&raw_message, temp_message));

					// In case we have to continue with another message we want to
					// strip out a single leading space, if any. This is so we avoid
					// to start a new sentence with a space.
					if ((current_parameter.Length() >= 2) &&
						(uni_isspace(current_parameter[0])) && (!uni_isspace(current_parameter[1])))
					{
						current_parameter.Delete(0, 1);
					}
				}
				else
					current_parameter.Delete(0, break_pos);

				// We're done if the parameter content is all deleted.
				done = current_parameter.IsEmpty();
			}
		}
		else
		{
			if (is_last_parameter && (
				current_parameter.FindFirstOf(UNI_L(" :")) != KNotFound ||
				current_parameter.IsEmpty() ||
				m_command.CompareI("USER") == 0))
			{
				current_parameter.Insert(0, UNI_L(" :"));
			}
			else
				current_parameter.Insert(0, UNI_L(" "));

			// If AddWordToRawMessage() returns FALSE, there was not enough
			// room for the parameter.
			UINT characters_added = 0;
			if (!AddWordToRawMessage(*raw_message, preferred_charset, current_parameter, max_bytes_for_word, characters_added))
			{
				// Not much to do if it fails allready on the first parameter.
				if (index == 0)
				{
					OP_DELETE(raw_message);
					raw_message = 0;

					return OpStatus::ERR;
				}
				else
				{
					// Ok, add what we have so far to the vector of raw
					// messages.
					RETURN_IF_ERROR(AddCTCPMarkerIfNeeded(*raw_message));

					RETURN_IF_ERROR(AddRawMessage(raw_messages, raw_message));
					RETURN_IF_ERROR(CreateNewRawMessage(&raw_message, temp_message));
				}
			}
		}
	}

	RETURN_IF_ERROR(AddCTCPMarkerIfNeeded(*raw_message));

	if ((raw_message != 0) && (raw_message->Compare(temp_message) != 0))
		RETURN_IF_ERROR(AddRawMessage(raw_messages, raw_message));

	return OpStatus::OK;
}


OP_STATUS IRCMessage::AddCTCPMarkerIfNeeded(OpString8 &raw_message) const
{
	if (m_is_ctcp)
	{
		const char ctcp_marker[2] = { m_ctcp_marker, 0 };
		RETURN_IF_ERROR(raw_message.Append(ctcp_marker));
	}

	return OpStatus::OK;
}


OP_STATUS IRCMessage::AddRawMessage(OpAutoVector<OpString8> &raw_messages, OpString8 *raw_message) const
{
	OP_ASSERT(raw_message != 0);

	RETURN_IF_ERROR(raw_messages.Add(raw_message));
	return OpStatus::OK;
}


OP_STATUS IRCMessage::CreateNewRawMessage(OpString8 **new_message, const OpStringC8& initial_value) const
{
	if (!(*new_message = OP_NEW(OpString8, ())))
		return OpStatus::ERR_NO_MEMORY;

	(*new_message)->Set(initial_value);
	return OpStatus::OK;
}


BOOL IRCMessage::AddWordToRawMessage(OpString8 &raw_message, OpString8 &preferred_charset,
	const OpStringC& word, UINT max_bytes_for_word, UINT &characters_added) const
{
	characters_added = 0;

	const UINT bytes_remaining = m_max_byte_length - raw_message.Length() - 1; // -1 because there may be need for a ctcp prefix at the end.
	const UINT characters_to_be_converted = word.Length();

	OpString8 word8;
	int characters_converted = 0;
	MessageEngine::GetInstance()->ConvertToBestChar8(preferred_charset, TRUE, word, word8, bytes_remaining, &characters_converted);

	if (UINT(characters_converted) < characters_to_be_converted)
	{
		// Ok, the word was too large to fit.
		const UINT characters_we_could_fit = characters_converted;

		// Will it fit into the an "empty" message?
		BOOL will_fit_in_empty = FALSE;

		if (bytes_remaining == max_bytes_for_word)
			will_fit_in_empty = FALSE;
		else
		{
			// Try the conversion again, this time with the maximum buffer.
			OpString8 temp_word8;
			MessageEngine::GetInstance()->ConvertToBestChar8(preferred_charset, TRUE, word, temp_word8, max_bytes_for_word, &characters_converted);

			if (UINT(characters_converted) < characters_to_be_converted)
				will_fit_in_empty = FALSE;
			else
				will_fit_in_empty = TRUE;
		}

		if (will_fit_in_empty)
			return FALSE; // Let it fit in the next message.

		// Ok, just chop it.
		raw_message.Append(word8);
		characters_added = characters_we_could_fit;

		return FALSE;
	}
	else
	{
		raw_message.Append(word8);
		characters_added = characters_converted;
	}

	return TRUE;
}


int IRCMessage::FindFirstWord(const OpStringC& find_in, OpString &word) const
{
	int break_pos = KNotFound;
	BOOL found_start = FALSE;

	const int input_length = find_in.Length();
	for (int index = 0; index < input_length; ++index)
	{
		const uni_char cur_char = find_in[index];

		if (cur_char == '\n')
		{
			if (index > 0 && find_in[index - 1] == '\r')
				break_pos = index - 1;
			else
				break_pos = index;
			break;
		}
		else
		{
			if (found_start)
			{
				if (uni_isspace(cur_char))
				{
					break_pos = index;
					break;
				}
			}
			else
			{
				if (!uni_isspace(cur_char))
					found_start = TRUE;
			}
		}
	}

	if (break_pos != -1)
	{
		OpString input_copy;
		input_copy.Set(find_in);

		word.Set(input_copy.CStr(), break_pos);
	}
	else
		word.Set(find_in);

	return break_pos;
}


OP_STATUS IRCMessage::Parse(const OpStringC& raw_line,
	const OpStringC8& charset, const OpStringC8& server)
{
	int find_pos = -1;

	OpString work_string;
	RETURN_IF_ERROR(work_string.Set(raw_line));

	if (work_string[0] == ':')
	{
		// Retrieve prefix.
		find_pos = work_string.Find(" ");
		if (find_pos == KNotFound)
			return OpStatus::ERR;

		OpString8 prefix8;
		RETURN_IF_ERROR(prefix8.Set(work_string.CStr()+1, find_pos-1));

		// Decode it.
		RETURN_IF_ERROR(ConvertToChar16(prefix8, charset, server, m_prefix));

		work_string.Delete(0, find_pos + 1);
	}

	// Retrieve the command.
	find_pos = work_string.Find(" ");

	if (find_pos != KNotFound)
	{
		work_string[find_pos] = '\0';
		SetCommand(work_string);
		work_string[find_pos] = ' ';
		work_string.Delete(0, find_pos + 1);
	}
	else
	{
		SetCommand(work_string);
		m_parameters.Empty();

		return OpStatus::OK;
	}

	// Store all parameters in a string, just for convenience.
	RETURN_IF_ERROR(m_parameters.Set(work_string));

	// Find parameter list.
	BOOL done = FALSE;
	do
	{
		OpString8 parameter8;

		if (work_string[0] == ':') // This is the last parameter.
		{
			OpStringC param(work_string.CStr()+1);
			
			const int parameter_length = param.Length();

			if (param[0] == m_ctcp_marker &&
				param[parameter_length - 1] == m_ctcp_marker)
			{
				m_is_ctcp = TRUE;

				RETURN_IF_ERROR(parameter8.Set(param.CStr()+1, parameter_length-2));
			}
			else
			{
				RETURN_IF_ERROR(parameter8.Set(param.CStr()));
			}

			done = TRUE;
		}
		else
		{
			find_pos = work_string.Find(" ");
			if (find_pos == KNotFound)
			{
				done = TRUE;
				RETURN_IF_ERROR(parameter8.Set(work_string.CStr()));
			}
			else
			{
				RETURN_IF_ERROR(parameter8.Set(work_string.CStr(), find_pos));
				work_string.Delete(0, find_pos + 1);
			}
		}

		// Convert to 16 bit unicode and add the parameter to the parameter collection.
		OpString parameter;
		RETURN_IF_ERROR(ConvertToChar16(parameter8, charset, server, parameter));
		RETURN_IF_ERROR(AddParameter(parameter));
	}
	while (!done);

	return OpStatus::OK;
}


OP_STATUS IRCMessage::ConvertToChar16(const OpStringC8& input,
	const OpStringC8& charset, const OpStringC8& server, OpString& output) const
{
	// Try to detect the charset used in the input.
#ifdef ENCODINGS_CAP_UTF8_THRESHOLD
	CharsetDetector charset_detector(server.CStr(), 0, 0, 0, 0);
#else
	CharsetDetector	charset_detector(server.CStr());
#endif
	charset_detector.PeekAtBuffer(input.CStr(), input.Length());

	const char* detected_charset = charset_detector.GetDetectedCharset();
	if (detected_charset == 0)
	{
		// Special utf-8 vs. iso-latin handling.
		if (charset.CompareI("utf-8") == 0)
			detected_charset = g_op_system_info->GetSystemEncodingL();
		else
			detected_charset = charset.CStr();
	}

	if (detected_charset != 0)
		return MessageEngine::GetInstance()->GetInputConverter().ConvertToChar16(detected_charset, input, output, TRUE);
	else
		return output.Set(input);
}


const UINT IRCMessage::m_max_byte_length = 470; // Should really be 512.
const char IRCMessage::m_ctcp_marker = '\001';

#endif // IRC_SUPPORT
