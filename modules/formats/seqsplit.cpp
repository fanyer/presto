/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/formats/seqsplit.h"
#include "modules/formats/argsplit.h"
#include "modules/formats/uri_escape.h"
#include "modules/mime/mimeutil.h"
#include "modules/url/tools/arrays.h"
#include "modules/encodings/utility/opstring-encodings.h"
#include "modules/encodings/detector/charsetdetector.h"


PREFIX_CONST_ARRAY(static, const char*, paramsep, formats)
	CONST_ENTRY("= \t\r\n")		// NVS_VAL_SEP_ASSIGN, *****************, *************  Also: NVS_VAL_SEP_ASSIGN | NVS_SEP_CRLF and NVS_VAL_SEP_NO_ASSIGN
	CONST_ENTRY("= \t\r\n,")	// NVS_VAL_SEP_ASSIGN, *****************, NVS_SEP_COMMA  Also: NVS_VAL_SEP_NO_ASSIGN
	CONST_ENTRY("= \t\r\n;")	// NVS_VAL_SEP_ASSIGN, NVS_SEP_SEMICOLON, *************  Also: NVS_VAL_SEP_NO_ASSIGN
	CONST_ENTRY("= \t\r\n,;")	// NVS_VAL_SEP_ASSIGN, NVS_SEP_SEMICOLON, NVS_SEP_COMMA  Also: NVS_VAL_SEP_NO_ASSIGN
	CONST_ENTRY(": \t\r\n")		// NVS_VAL_SEP_COLON,  *****************, *************  Also: NVS_VAL_SEP_COLON | NVS_SEP_CRLF 
	CONST_ENTRY(": \t\r\n,")	// NVS_VAL_SEP_COLON,  *****************, NVS_SEP_COMMA
	CONST_ENTRY(": \t\r\n;")	// NVS_VAL_SEP_COLON,  NVS_SEP_SEMICOLON, *************
	CONST_ENTRY(": \t\r\n,;")	// NVS_VAL_SEP_COLON,  NVS_SEP_SEMICOLON, NVS_SEP_COMMA
CONST_END(paramsep)

NameValue_Splitter::NameValue_Splitter()
{
	InternalInit();
}

void NameValue_Splitter::InternalInit()
{
	name= value =NULL;
	assigned = FALSE;
	is_invalid = FALSE;
	
	rfc2231_escape_mode = RFC_2231_Not_Escaped;
	rfc2231_index= 0;

	internal_buffer = NULL;
	parameters = NULL;
}


NameValue_Splitter::~NameValue_Splitter()
{
	Clean();
}

void NameValue_Splitter::Clean()
{
	rfc2231_Name.Empty();
	OP_DELETEA(internal_buffer); // Pointer NULLed later
	OP_DELETE(parameters);  // Pointer NULLed later

	InternalInit(); // NULLs pointers
}

void NameValue_Splitter::ConstructFromNameAndValueL(Sequence_Splitter &list, const char *aName, const char * aValue, int aValLen)
{	
	int nlen = 0;
	int vlen = 0;

	if(aName)
	{
		nlen = (int)op_strlen(aName)+1;
	}
	if(aValue)
			vlen = aValLen+1; 

	if(nlen + vlen)
	{
		internal_buffer = OP_NEWA_L(char, nlen + vlen);

		if(aValue)
		{
			value = internal_buffer;
			op_strncpy(internal_buffer, aValue, aValLen);
			internal_buffer[aValLen] = 0;
			assigned = TRUE;
		}

		if(aName && !name)
		{
			name = internal_buffer + vlen;
			op_strcpy(internal_buffer + vlen, aName);
		}
	}

	list.UpdateIndexID(this);
	this->Into(&list);
}

void NameValue_Splitter::ConstructDuplicateL(const NameValue_Splitter *old)
{
	Clean();
	if(old == NULL)
		return;

	int nlen = 0;
	int vlen = 0;

	const char *aName = old->Name();
	const char *aValue = old->Value();

	if(aName)
		nlen = (int)op_strlen(aName)+1;
	if(aValue)
		vlen = (int)op_strlen(aValue)+1;

	if(nlen + vlen)
	{
		internal_buffer = OP_NEWA_L(char, nlen + vlen);

		if(aValue)
		{
			value = internal_buffer;
			op_strcpy(internal_buffer, aValue);
		}

		if(aName && !name)
		{
			name = internal_buffer + vlen;
			op_strcpy(internal_buffer + vlen, aName);
		}
	}

	rfc2231_Name.SetL(old->GetRFC2231_Name());
	rfc2231_escape_mode = old->GetRFC2231_Escaped();
	rfc2231_index = old->GetRFC2231_Index();

	assigned = old->AssignedValue();
	SetNameID(old->GetNameID());
}

char *NameValue_Splitter::InitL(char *val,int flags)
{
	const char *sepstr_all,*sepstr_noassign,*sepstr_sep;
	char *current_pos;
	char tmp1;
	
	Clean();

	if(val == NULL || *val == '\0')
		return NULL;

	if((flags & NVS_SEP_CRLF) != 0)
		flags &= ~(NVS_SEP_COMMA | NVS_SEP_SEMICOLON | NVS_SEP_WHITESPACE); // Disables these
	if((flags & NVS_VAL_SEP_COLON) != 0)
		flags &= ~(NVS_VAL_SEP_NO_ASSIGN); // Disables this

	sepstr_all = g_paramsep[(flags & (NVS_SEP_SEMICOLON | NVS_SEP_COMMA)) | ((flags & NVS_VAL_SEP_COLON) != 0 ? 0x04 : 0)];
	sepstr_noassign = sepstr_all+1;
	sepstr_sep = (flags & NVS_SEP_WHITESPACE) ? sepstr_noassign : sepstr_noassign+2; 
	// CR and LF are considered end of argument, same as the real separators. This is to avoid linewrapped arguments.

	if((flags & NVS_VAL_SEP_COLON) != 0 && (flags & NVS_ONLY_SEP) != 0)
	{
		// In the case of colon separators and when they are the only separator, whitespace is part of the identifier.
		sepstr_all = ":\r\n";
		sepstr_sep = sepstr_noassign = sepstr_all+1; 
	}

	if (flags & NVS_COOKIE_SEPARATION)
	{
		sepstr_all = "=\r\n;";
	}

	current_pos = val + op_strspn(val,sepstr_noassign);	// skip all possible leading separators/whitespace
	if(*current_pos == '\0')
		return NULL;

	char item_sep = (flags & NVS_VAL_SEP_COLON) ? ':' : '=';

	if(!(flags & NVS_VAL_SEP_NO_ASSIGN) || (flags & NVS_VAL_SEP_COLON) != 0 )
	{
		name = current_pos;		// set name
		current_pos = current_pos + op_strcspn(current_pos,sepstr_all);	// find end of name
		tmp1 = *current_pos;
		if (flags & NVS_COOKIE_SEPARATION)
		{
			char *strip = current_pos;
			while (op_isspace(*(strip - 1)) && strip > name)
				strip--;

			while (op_isspace(*name) && name < strip)
				name++;

			*strip = '\0';	// and terminate it

		}
		else
			*current_pos = '\0';

		if(op_strchr(name,'\"') != NULL)
		{
			// something's fishy, a quote inside the name
			if((flags & NVS_ACCEPT_QUOTE_IN_NAME))
				is_invalid = TRUE;
			else
				LEAVE(OpStatus::ERR_PARSING_FAILED);
		}

		if(	((flags & NVS_ONLY_SEP)  || (flags & NVS_VAL_SEP_COLON)) && tmp1 && tmp1 != item_sep && !op_strchr(sepstr_sep, tmp1))
		{
			char *tmp_pos = current_pos+1 + op_strspn(current_pos+1, sepstr_noassign);

			if(*tmp_pos == item_sep)
			{
				tmp1 = item_sep;
				current_pos = tmp_pos;	// skip illegal whitespace between name and assignment
			}
		}
	}
	else
	{
		tmp1 = item_sep;		// fake an assignment to reuse quote processing
		current_pos --;
	}

	if(tmp1 == item_sep)
	{
		BOOL continuation_unfolding_needed = FALSE;

		current_pos++;
		if (!(flags & NVS_VAL_SEP_NO_ASSIGN) || (flags & NVS_VAL_SEP_COLON) != 0 )
		{
			assigned = TRUE;
			if ((flags & NVS_ONLY_SEP) && !(flags & NVS_VAL_SEP_COLON))
				while(op_isspace((unsigned char) *current_pos))	// skip illegal whitespace between assignment and value
					current_pos++;
			else if ((flags & NVS_VAL_SEP_COLON))
				while(op_isspace((unsigned char) *current_pos) && *current_pos != '\n'&& *current_pos != '\r')	// skip illegal whitespace between assignment and value, but stop on CRLF
					current_pos++;
		}
		value = current_pos;


		if (!(flags & NVS_SEP_CRLF) && *current_pos == '\"')		// is quoted
		{
			// Search for end quote
			for (current_pos++; *current_pos && *current_pos != '"'; current_pos++)
				if (*current_pos == '\\' && *(current_pos+1))
					current_pos ++; // Skip quoted-pairs

			if (!*current_pos)
				LEAVE(OpStatus::ERR_PARSING_FAILED); // something's fishy, no end quote found

			if (flags & NVS_STRIP_ARG_QUOTES) // strip preceding quote
				value ++;
			else
				current_pos ++;					// don't strip succeeding quote
		}
		else
		{
			char *searchstart = current_pos;
			
			while (TRUE)
			{
				current_pos = searchstart + op_strcspn(searchstart,(flags & NVS_ONLY_SEP) ? sepstr_sep : sepstr_noassign);	// find end of name resp. value
				if (!*current_pos)
					break;

				if((flags & NVS_SEP_CRLF_CONTINUED) == NVS_SEP_CRLF_CONTINUED)
				{
					// only looked for first CR or LF, now skip the CRLF (or LF or CR)  section and look at the first character on
					// the next line. If it is a space or tab we continue searching for the next CRLF, as the next line is part of this value.
					
					OP_ASSERT(*current_pos == '\r' || *current_pos == '\n');
		
					searchstart = current_pos;
					if(*searchstart == '\r')
						searchstart++;
					if(*searchstart == '\n')
						searchstart++;

					if(*searchstart == ' ' || *searchstart == '\t')
					{
						continuation_unfolding_needed = TRUE;
						searchstart ++;
						continue; // Line starts with a whitespace, Include the next line in value
					}
				}
				else
				{
					const char *quote_search = value;
					BOOL in_quote = FALSE;
					char tok;

					while((tok = *quote_search) != '\0' && quote_search != current_pos)
					{
						quote_search ++;
						if(tok == '"')
							in_quote = !in_quote;
						else if(tok == '\\' && in_quote)
						{
							if(quote_search == current_pos)
								break;
							quote_search ++;
							// ignore charachter after backslash inside quotes
						}
					}

					if(in_quote && *current_pos != '\0')
					{
						searchstart = current_pos+1;
						continue; // separator is inside a quote 
					}
				}
				break;
			}
		}

		if (flags & NVS_VAL_SEP_NO_ASSIGN)
			name = value;

		if (flags & NVS_ONLY_SEP)
			while(current_pos > value && op_isspace((unsigned char) *(current_pos-1)))	// skip illegal whitespace between value and end of argument
				current_pos--;

		tmp1 = *current_pos;
		if(tmp1)
		{
			*(current_pos++) = '\0';	// and terminate it

			if(*sepstr_sep && !op_strchr(sepstr_sep,tmp1))
			{
				char *post_val = current_pos;
				current_pos += op_strcspn(current_pos,sepstr_sep);	// skip until the next separator
				if(*current_pos)
				{
					*current_pos = '\0';
					current_pos ++;
				}

				if((flags & NVS_CHECK_FOR_INVALID_QUOTES) && (tmp1 == '\"' ||op_strchr(post_val,'\"') != NULL))
					LEAVE(OpStatus::ERR_PARSING_FAILED); // Something is fishy here, quotes in the middle of unquoted text after the value.
			}
		}

		if((flags & NVS_CHECK_FOR_INVALID_QUOTES) && value && *value != '\"' && op_strchr(value,'\"') != NULL)
			LEAVE(OpStatus::ERR_PARSING_FAILED); // Something is fishy here, quotes in the middle of unquoted text.

		if (continuation_unfolding_needed)
		{
			// Remove all CR/LF since there aren't supposed to be any in the output in case of continuations
			char *dst = const_cast<char*>(value);
			for (const char *src = value; *src; src++)
				if (*src != '\r' && *src != '\n')
					*dst++ = *src;
			*dst++ = '\0';
		}
	}
	else if(tmp1)	// no assignment
		current_pos++;
		

	if((flags & NVS_HAS_RFC2231_VALUES) != 0 && assigned && name && value)
	{
		do {
			OpStringC8 temp_name(name);
			
			int indicator = temp_name.FindFirstOf('*');
			int len = temp_name.Length();
			
			if(indicator == KNotFound || (len > indicator+1 && !op_isdigit((unsigned char) temp_name[indicator+1])))
				break; // Skip items witout the special character "*" or an illegal characters after "*"
			
			unsigned int read_number=0;
			char special_token=0, should_not_be_read_token=0;
			int read_items = op_sscanf(name+indicator+1,"%u%c%c", &read_number, &special_token, &should_not_be_read_token);

			if(read_items >=3 || (read_items == 2 && special_token != '*'))
				break; // illegal syntax, skip parameter

			rfc2231_Name.SetL(name, indicator);
			rfc2231_escape_mode = RFC_2231_Single;

			if(read_items > 0)
			{
				rfc2231_index = read_number;
				rfc2231_escape_mode = (read_items >1 ? RFC_2231_Fully_Escaped : RFC_2231_Numbered);
			}
		}while(0);
	}

	return current_pos;
}

const char *NameValue_Splitter::Charset() const
{
	return NULL;
}

#ifdef FORMATS_NEED_LANGUAGE
const char *NameValue_Splitter::Language() const
{
	return NULL;
}
#endif

void NameValue_Splitter::StripQuotes(BOOL gently)
{
	if(!value || *value != '\"')
		return;

	if (gently && GetRFC2231_Escaped() == RFC_2231_Not_Escaped)
	{
		OpString8 cset;
		CharsetDetector test;
		
		test.PeekAtBuffer(value, (unsigned long)op_strlen(value));
		cset.Set(test.GetDetectedCharset());
		gently = cset.Compare("iso-2022",8)==0;
	}
	else
		gently = FALSE;
	
	char *trg = (char *)value;
	const char *src = value;
	char val;
	while((val = *(src++)) != '\0')
	{
		if(val != '\"')
		{
			if(val == '\\' && *src >= 0x20 && (!gently || *src=='\\' || *src=='\"'))
				val = *(src++);
			*(trg++) = val;
		}
	}
	*trg = '\0';
}

const char* NameValue_Splitter::LinkId()
{
	return Name();
}

unsigned long NameValue_Splitter::GetUnsignedValue(int base)
{
	StripQuotes();

	const char *val = Value();

	return (val ? op_strtoul(val, NULL, base) : 0);
}

int NameValue_Splitter::GetSignedValue()
{
	StripQuotes();

	const char *val = Value();

	return (val ? op_atoi(val) : 0);
}

unsigned long NameValue_Splitter::GetUnsignedName(int base) const
{
	const char *val = Name();

	return (val ? op_strtoul(val, NULL, base) : 0);
}

#ifdef _MIME_SUPPORT_
void NameValue_Splitter::GetValueStringL(OpString &target, const OpStringC8 &default_charset)
{
	const char *val = Value();
	if(val && *val)
	{
		if(Charset())
		{
			TRAPD(status, SetFromEncodingL(&target, Charset(), val, (int)op_strlen(val)));
			if (OpStatus::IsMemoryError(status))
				LEAVE(status);
			if (OpStatus::IsSuccess(status))
				return;
		}

		RemoveHeaderEscapes(target, val, (int)op_strlen(val), TRUE, TRUE, default_charset.CStr());
	}
}
#endif

Sequence_Splitter *NameValue_Splitter::CreateSplitterL() const
{
	return OP_NEW_L(Sequence_Splitter, ());
}

Sequence_Splitter *NameValue_Splitter::GetParameters(const KeywordIndex *keys, int count, unsigned flags, Prepared_KeywordIndexes index)
{
	Sequence_Splitter * OP_MEMORY_VAR ret = NULL;
	TRAPD(op_err, ret = (Sequence_Splitter *) GetParametersL(keys, count, flags, index));

	return ret;
}

Sequence_Splitter *NameValue_Splitter::GetParametersL(const KeywordIndex *keys, int count, unsigned flags, Prepared_KeywordIndexes index)
{
	OP_DELETE(parameters);
	parameters = NULL;

	OpStackAutoPtr<Sequence_Splitter> item(CreateSplitterL());

	if(item.get())
	{
		item->SetKeywordList(keys, count);
		if(index != KeywordIndex_None)
			item->SetKeywordList(index);
		item->SetValueL((assigned ? Value() : Name()), flags);
	}

	return (parameters = item.release());
}

NameValue_Splitter *NameValue_Splitter::DuplicateL() const
{
	OpStackAutoPtr<NameValue_Splitter> ret(OP_NEW_L(NameValue_Splitter, ()));

	ret->ConstructDuplicateL(this);

	return ret.release();
}

#ifdef FORMATS_ENABLE_DUPLICATE_INTO
void NameValue_Splitter::DuplicateIntoL(Sequence_Splitter *list) const
{
	if(!list)
		return;

	OpStackAutoPtr<NameValue_Splitter> item(DuplicateL());

	item->Into(list);
	list->UpdateIndexID(item.get());
	item.release();
}
#endif

Sequence_Splitter::Sequence_Splitter()
{
	argument = NULL;
	external = FALSE;
}

Sequence_Splitter::Sequence_Splitter(Prepared_KeywordIndexes index)
: Keyword_Manager(index)
{
	argument = NULL;
	external = FALSE;
}


/*
Sequence_Splitter::Sequence_Splitter(const char* const *keys, unsigned count) 
: Keyword_Manager(keys, count)
{
	argument = NULL;
	external = FALSE;
}
*/

Sequence_Splitter::Sequence_Splitter(const KeywordIndex *keys, unsigned int count) 
: Keyword_Manager(keys, count)
{
	argument = NULL;
	external = FALSE;
}

Sequence_Splitter::~Sequence_Splitter()
{
	Clear();
}

void Sequence_Splitter::Clear()
{
	Keyword_Manager::Clear();
	if(!external)
		OP_DELETEA(argument);
	argument = NULL;
	external = FALSE;
}

OP_STATUS Sequence_Splitter::SetValue(const char *value, unsigned flags)
{
	TRAPD(op_err, SetValueL(value,flags));

	return op_err;
}

void Sequence_Splitter::SetValueL(const char *value, unsigned flags)
{
	Clear();
	if(external)
		argument = NULL;
	if(flags & (NVS_TAKE_CONTENT | NVS_BORROW_CONTENT))
	{
		argument = (char*)value; // Must not be deleted!
		external = (flags & NVS_BORROW_CONTENT) ? TRUE : FALSE;
	}
	else
	{
		SetStrL(argument, value);
		external = FALSE;
	}

	if(argument == NULL)
		return;

	char *current = argument;
	OpStackAutoPtr<NameValue_Splitter> current_parameter(NULL);

	BOOL do_unquote = FALSE;
	BOOL unquote_gently = (flags & PARAM_STRIP_ARG_QUOTES_GENTLY) == PARAM_STRIP_ARG_QUOTES_GENTLY;
	if((flags & PARAM_HAS_RFC2231_VALUES) != 0 && (flags & PARAM_STRIP_ARG_QUOTES) != 0)
	{
		do_unquote = TRUE;
		flags &= ~PARAM_STRIP_ARG_QUOTES; // remove this parameter, will do it later, need to check the syntax
	}

	BOOL first = TRUE;

	while(current && *current)
	{
		current_parameter.reset(CreateNameValueL());

		unsigned current_flags = flags;
		if (first && (flags & NVS_NO_ASSIGN_FIRST_ONLY) != 0)
			current_flags = (current_flags | NVS_VAL_SEP_NO_ASSIGN) & ~NVS_VAL_SEP_COLON;
		if (first && (flags & NVS_WHITESPACE_FIRST_ONLY) != 0)
			current_flags = NVS_SEP_WHITESPACE;

		current = current_parameter->InitL(current, current_flags);

		if(current == NULL)
		{
			current_parameter.reset();
			break;
		}

		if(current_parameter->Valid())
		{
			UpdateIndexID(current_parameter.get());

			current_parameter->Into(this);
			current_parameter.release();
		}
		else
			current_parameter.reset();

		first = FALSE;
	}

	if((flags & PARAM_HAS_RFC2231_VALUES) != 0)
	{
		AutoDeleteHead value_list;
		ANCHOR(AutoDeleteHead, value_list);

		NameValue_Splitter *next_item = First(); 
		NameValue_Splitter *current_item = NULL;

		while(next_item)
		{
			if(current_item && do_unquote)
				current_item->StripQuotes(unquote_gently); // strip qoutes on the previous element

			current_item = next_item;
			next_item = next_item->Suc();

			if(current_item->GetRFC2231_Escaped() != RFC_2231_Not_Escaped && 
				current_item->GetRFC2231_Name().HasContent())
			{
				unsigned long collected_len = 0;
				OpStringC8 name(current_item->GetRFC2231_Name());

				// move to temporary list;
				current_item->Out();
				current_item->Into(&value_list);
				collected_len += current_item->GetValue().Length();

				if(current_item->GetRFC2231_Escaped() != RFC_2231_Single)
				{
					NameValue_Splitter *next_item2 = next_item;
					while(next_item2)
					{
						current_item = next_item2;
						next_item2 = next_item2->Suc();

						if(current_item->GetRFC2231_Escaped() != RFC_2231_Not_Escaped &&
							name.CompareI(current_item->GetRFC2231_Name()) == 0)
						{
							NameValue_Splitter *item2 = (NameValue_Splitter *) value_list.Last(); // Assume that in most cases the list will be sequential, and ordered numerically
							while(item2 && current_item->GetRFC2231_Index() < item2->GetRFC2231_Index())
							{
								item2 = item2->Pred(); // otherwise step backwards in the list
							}
							if(item2 && current_item->GetRFC2231_Index() == item2->GetRFC2231_Index()) 
								continue; // Same name, skip this one

							current_item->Out();
							if(item2)
								current_item->Follow(item2);
							else
								current_item->IntoStart(&value_list);

							collected_len += current_item->GetValue().Length();

							if(current_item == next_item)
								next_item = next_item2;
						}
					}
				}

				OpString8 real_name;
				ANCHOR(OpString8, real_name);
				OpString8 collected_value;
				ANCHOR(OpString8, collected_value);
				OpString8 charset;
				ANCHOR(OpString8, charset);
				OpString8 language;
				ANCHOR(OpString8, language);
				OpString8 temp_val;
				ANCHOR(OpString8, temp_val);
				unsigned int	current_number=0;
				BOOL finished = FALSE;
				BOOL convert_to_utf8 = FALSE;

				collected_value.ReserveL(collected_len+10);

				real_name.SetL(name);

				current_item = (NameValue_Splitter *) value_list.First();
				while(!finished && current_item)
				{
					BOOL is_escaped = FALSE;
					BOOL is_numbered = TRUE;
					if(current_number == 0 && current_item->GetRFC2231_Escaped() == RFC_2231_Single)
					{
						is_escaped = TRUE;
						is_numbered = FALSE;
						finished = TRUE;
					}

					if(is_numbered)
					{
						if(current_item->GetRFC2231_Index() != current_number)
						{
							finished = TRUE;
							break;
						}

						is_escaped = (current_item->GetRFC2231_Escaped() == RFC_2231_Fully_Escaped);
					}

					const char *val_pos = current_item->Value();
					if(is_escaped)
					{						
						if(*val_pos == '"')
						{
							finished = TRUE;
							break;
						}

						if(current_number == 0)
						{
							char *lang_start = (char *)op_strchr(val_pos, '\'');
							if(lang_start == NULL)
							{
								finished = TRUE;
								continue;
							}

							charset.SetL(val_pos, (int)(lang_start- val_pos));
							lang_start++;
							val_pos = op_strchr(lang_start, '\'');
							if(val_pos == NULL)
							{
								finished = TRUE;
								continue;
							}

							language.SetL(lang_start, (int)(val_pos - lang_start));
							val_pos ++;

							if(charset.CompareI("utf-16") == 0)
							{
								charset.SetL("utf-8");
								convert_to_utf8 = TRUE;
							}
						}

						int origin_len = UriUnescape::ReplaceChars((char *) val_pos, UriUnescape::NonCtrlAndEsc);

						if(convert_to_utf8)
						{
							temp_val.SetUTF8FromUTF16L((uni_char *) val_pos,UNICODE_DOWNSIZE(origin_len));
							val_pos = temp_val.CStr();
						}
					}
					else
					{
						current_item->StripQuotes();

						val_pos = current_item->Value();
					}

					collected_value.AppendL(val_pos);

					current_item = current_item->Suc();
					current_number ++;
				}

				OpStackAutoPtr<NameValue_Splitter> new_param(CreateAllocated_ParameterL(real_name, collected_value, charset, language));

				UpdateIndexID(new_param.get());
				new_param->SetRFC2231_Escaped(RFC_2231_Decoded);

				// Put this before any non-RFC2231 parameters of the same name, so RFC2231 parameters takes precedence
				for (NameValue_Splitter *item = First(); item != next_item; item = item->Suc())
					if (item->GetRFC2231_Escaped() == RFC_2231_Not_Escaped &&
						real_name.CompareI(item->Name()) == 0)
					{
						new_param->Precede(item);
						break;
					}

				if (!new_param->InList())
				{
					if(next_item)
						new_param->Precede(next_item);
					else
						new_param->Into(this);
				}
				new_param.release();

				value_list.Clear(); // remove all items;
				current_item = NULL;
				
			}
		}
		if(current_item && do_unquote)
			current_item->StripQuotes(unquote_gently); // strip qoutes on the previous element
	}
}

void Sequence_Splitter::TakeOver(Sequence_Splitter &oldlist)
{
	SetKeywordList(oldlist);
	argument = oldlist.argument;
	oldlist.argument = NULL;
	external = oldlist.external;
	oldlist.external = FALSE;
	Append(&oldlist);
}

Sequence_Splitter *Sequence_Splitter::DuplicateL() const
{
	OpStackAutoPtr<Sequence_Splitter> new_list(CreateSplitterL());
	
	DuplicateIntoListL(new_list.get(),0);

	return new_list.release();
}


void Sequence_Splitter::DuplicateIntoListL(Sequence_Splitter *list, int hdr_name_id, NameValue_Splitter *after) const
{
	if(!list)
		return;

	list->SetKeywordList(*this);
	const NameValue_Splitter* cur_head = (const NameValue_Splitter *) (hdr_name_id ? GetItemByID(hdr_name_id, after) : First());
	while (cur_head)
	{
		OpStackAutoPtr<NameValue_Splitter> item(cur_head->DuplicateL());
	
		if(item.get())
		{
			item->Into(list);
			item.release();
		}

		cur_head = (const NameValue_Splitter *) (hdr_name_id ? GetItemByID(0, cur_head) : cur_head->Suc());
	}
}

NameValue_Splitter *Sequence_Splitter::CreateAllocated_ParameterL(const OpStringC8 &a_Name, const OpStringC8 &a_Value, const OpStringC8 &a_Charset, const OpStringC8 &a_Language)
{
	OpStackAutoPtr<Allocated_Parameter> temp(OP_NEW_L(Allocated_Parameter, ()));

	temp->InitializeL(a_Name, a_Value, a_Charset, a_Language);

	return temp.release();
}

NameValue_Splitter *Sequence_Splitter::CreateNameValueL() const
{
	return OP_NEW_L(NameValue_Splitter, ());
}

Sequence_Splitter *Sequence_Splitter::CreateSplitterL() const
{
	return OP_NEW_L(Sequence_Splitter, ());
}


NameValue_Splitter *Sequence_Splitter::GetParameter(const char *tag,
						Parameter_UseAssigned assign, NameValue_Splitter *after,
				Parameter_ResolveKeyword resolve)
{
	NameValue_Splitter *current;

	while((current = GetParameter(tag, after, resolve)) != NULL)
	{
		if(assign == PARAMETER_ANY || 
			(assign == PARAMETER_ASSIGNED_ONLY && current->AssignedValue()) || 
			(assign == PARAMETER_NO_ASSIGNED && !current->AssignedValue()))
			break;
		after = current;
	}

	return current;
}

NameValue_Splitter *Sequence_Splitter::GetParameterByID(unsigned int tag_id,
						Parameter_UseAssigned assign, NameValue_Splitter *after)
{
	NameValue_Splitter *current;

	while((current = GetParameterByID(tag_id, after)) != NULL)
	{
		if(assign == PARAMETER_ANY || 
			(assign == PARAMETER_ASSIGNED_ONLY && current->AssignedValue()) || 
			(assign == PARAMETER_NO_ASSIGNED && !current->AssignedValue()))
			break;
		after = current;
	}

	return current;
}

#ifdef FORMATS_GETVALUE8_ASSIGN
void Sequence_Splitter::GetValueStringFromParameterL(OpStringS8 &target, int parameter_id, 
							Parameter_UseAssigned assign, NameValue_Splitter *after)
{
	NameValue_Splitter *item = GetParameterByID(parameter_id, assign, after);
	
	if(item)
		target.SetL(item->Value());
}
#endif

#ifdef FORMATS_GETVALUE8
void Sequence_Splitter::GetValueStringFromParameterL(OpStringS8 &target, int parameter_id, 
							NameValue_Splitter *after)
{
	NameValue_Splitter *item = GetParameterByID(parameter_id, after);
	
	if(item)
		target.SetL(item->Value());
}
#endif

#ifdef FORMATS_GETVALUE16
void Sequence_Splitter::GetValueStringFromParameterL(OpString &target, const OpStringC8 &default_charset, int parameter_id, 
							 Parameter_UseAssigned assign, NameValue_Splitter *after)
{
	NameValue_Splitter * param = GetParameterByID(parameter_id, assign, after);
	if(param)
		param->GetValueStringL(target,default_charset);
}
#endif
