/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef INTERNAL_SPELLCHECK_SUPPORT

#ifndef SPC_USE_HUNSPELL_ENGINE

#include "modules/spellchecker/src/opspellchecker.h"

DictionaryEncoding GetEncodingFromString(UINT8 *enc_str)
{
	if (str_eq(enc_str,(UINT8*)"UTF-8"))
		return UTF8_ENC;
	return BIT8_ENC;
}

FlagEncoding GetFlagEncodingFromString(UINT8 *enc_str)
{
	if (str_eq(enc_str,(UINT8*)"long"))
		return FLAG_LONG_ENC;
	else if (str_eq(enc_str,(UINT8*)"num"))
		return FLAG_NUM_ENC;
	else if (str_eq(enc_str,(UINT8*)"UTF-8"))
		return FLAG_UTF8_ENC;
	return FLAG_UNKNOWN_ENC;
}

AffixConditionData *OpSpellChecker::GetAffixConditionData(UINT8 *str, UINT8* buffer, int buf_len, int &cond_count)
{
	int str_len = op_strlen((const char*)str);
	if (!str_len || (int)(str_len*(sizeof(UINT32)+sizeof(AffixConditionData))) > buf_len)
		return NULL;
	UINT32 *chars = (UINT32*)(buffer+str_len*sizeof(AffixConditionData));
	AffixConditionData *cond_array = (AffixConditionData*)buffer;
	cond_count = 0;
	while (*str)
	{
		UINT32 ch;
		int len = 0;
		AffixConditionData *cond = cond_array + cond_count++;
		BOOL negative = FALSE;
		if (*str == ']')
			return NULL;
		if (*str == '^')
		{
			negative = TRUE;
			str++;
			if (*str == '^' || *str == ']')
				return NULL;
		}
		if (*str == '[')
		{
			str++;
			if (*str == '^')
			{
				negative = TRUE;
				str++;
			}
			while (*str && *str != ']') // accept un-terminated ([blabla\0) condition to fix the gsc_FR dictionary
			{
				SPC_NEXT_CHAR(ch,str);
				if (ch == '\0' || ch == '[')
					return NULL;
				chars[len++] = ch;
			}
			if (*str)
				str++;
			if (!len)
				return NULL;
		}
		else
		{
			SPC_NEXT_CHAR(ch,str);
			if (ch == '\0')
				return NULL;
			len = 1;
			*chars = ch;
		}
		cond->chars = chars;
		cond->len = len;
		cond->negative = negative;
		chars += len;
	}
	return cond_array;
}

CompoundRuleConditionData *OpSpellChecker::GetCompoundRuleConditionData(UINT8 *str, UINT8* buffer, int buf_len, FlagEncoding flag_encoding, int &cond_count)
{
	int str_len = op_strlen((const char*)str);
	if (!str_len || (int)(str_len*(sizeof(UINT16)+sizeof(CompoundRuleConditionData))) > buf_len)
		return NULL;
	UINT16 *indexes = (UINT16*)(buffer+str_len*sizeof(CompoundRuleConditionData));
	CompoundRuleConditionData *cond_array = (CompoundRuleConditionData*)buffer;
	cond_count = 0;
	BOOL num_flg = flag_encoding == FLAG_NUM_ENC;
	while (*str)
	{
		int flag_index;
		int len = 0;
		int bytes_used = 0;
		CompoundRuleConditionData *cond = cond_array + cond_count++;
		BOOL negative = FALSE;
		if (op_strchr("]*+", *str) || num_flg && *str == ',')
			return NULL;
		if (*str == '^')
		{
			negative = TRUE;
			str++;
			if (!*str || op_strchr("]*+^", *str) || num_flg && *str == ',')
				return NULL;
		}
		if (*str == '[')
		{
			str++;
			if (*str == '^')
			{
				negative = TRUE;
				str++;
			}
			while (*str != ']')
			{
				if (!*str || op_strchr("[*+^", *str) || num_flg && *str == ',')
					return NULL;
				flag_index = decode_flag(str,flag_encoding,bytes_used);
				if (flag_index < 0)
					return NULL;
				str += bytes_used;
				if (num_flg && *str == ',')
					str++;
				indexes[len++] = (UINT16)flag_index;
			}
			str++;
			if (!len)
				return NULL;
		}
		else
		{
			flag_index = decode_flag(str,flag_encoding,bytes_used);
			if (flag_index < 0)
				return NULL;
			str += bytes_used;
			if (num_flg && *str == ',')
				str++;
			len = 1;
			*indexes = flag_index;
		}
		cond->indexes = indexes;
		cond->len = len;
		cond->negative = negative;
		indexes += len;
		cond->multiple_type = 0;
		if (*str == '*' || *str == '+')
			cond->multiple_type = *(str++);
	}
	return cond_array;
}

OP_STATUS OpSpellChecker::PostProcessPass(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	if (state->GetPass() == 0)
	{
		RETURN_IF_ERROR(fd->PostProcessCircumfixes(state));
		RETURN_IF_ERROR(fd->ConvertAliasIndexes());
		fd->SetTypeIdBits();
		RETURN_IF_ERROR(SetupCharConverters());
		RETURN_IF_ERROR(CreateUTF16FromBuffer(fd->GetWordChars(), m_word_chars, &m_allocator));
		RETURN_IF_ERROR(CreateUTF16FromBuffer(fd->GetBreakChars(), m_break_chars, &m_allocator));
	}
	else if (state->GetPass() == 1)
	{
		RETURN_IF_ERROR(fd->PostProcessCircumfixes(state));
	}

	if ( state->IsLastPass() )
	{
		UINT32 existing_radix_flags = 0;
		int i;
		for (i=0;i<SPC_RADIX_FLAGS;i++)
		{
			if (fd->GetFlagTypeCount((AffixFlagType)i))
				existing_radix_flags |= 1<<i;
		}
		m_has_radix_flags = !!existing_radix_flags;
		m_has_compounds = (existing_radix_flags & SPC_RADIX_COMPOUND_FLAGS || m_has_compound_rule_flags) && m_compoundwordmax > 1;
	}

	return OpStatus::OK;
}

inline OP_STATUS OpSpellChecker::ParseIntValue(int *result, HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	int dummy;
	HunspellAffixFileLine *line = state->GetCurrentLine();
	if (!line->GetWordCount())
		return SPC_ERROR(OpStatus::ERR);
	*result = pos_num_to_int(line->WordAt(0),0xFFFF,dummy);
	return *result <= 0 ? SPC_ERROR(OpStatus::ERR) : OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseSFX(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return ParseAffix(fd,state,TRUE);
}

OP_STATUS OpSpellChecker::ParsePFX(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return ParseAffix(fd,state,FALSE);
}

OP_STATUS OpSpellChecker::ParsePHONE(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	state->SetDicCharsUsed();
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseKEY(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	state->SetDicCharsUsed();
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseAF(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	HunspellAffixFileLine *line = state->GetCurrentLine();
	if (!state->GetAliasesDetected())
	{
		state->SetAliasesDetected();
		return OpStatus::OK;
	}
	if (!line->GetWordCount())
		return SPC_ERROR(OpStatus::ERR);
	state->SetFlagsUsed();
	UINT8 *str = line->WordAt(0);
	int bytes_used = 0;
	UINT16 *flags = (UINT16*)fd->Allocator()->Allocate16(SPC_MAX_FLAGS_PER_LINE*sizeof(UINT16));
	if (!flags)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	int i = 0;
	int f;
	while ((f = decode_flag(str,fd->GetFlagEncoding(),bytes_used)) >= 0 && i < SPC_MAX_FLAGS_PER_LINE)
	{
		flags[i++] = f;
		str += bytes_used;
		while (*str == ',')
			str++;
	}
	fd->Allocator()->OnlyUsed(i*sizeof(UINT16));
	if (!i)
		return SPC_ERROR(OpStatus::ERR);
	FlagAlias alias = FlagAlias(flags,i);
	RETURN_IF_ERROR(fd->AddAlias(alias));
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseTRY(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseSET(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	HunspellAffixFileLine *line = state->GetCurrentLine();
	UINT8 *enc_str = line->WordAt(0);
	if (!enc_str)
		return OpStatus::OK;
	if (state->GetDicCharsUsed() || m_enc_string)
		return SPC_ERROR(OpStatus::ERR); // Can't use two different encodings
	DictionaryEncoding enc = GetEncodingFromString(enc_str);
	if (enc == UNKNOWN_ENC)
		return SPC_ERROR(OpStatus::ERR);
	m_encoding = enc;
	m_8bit_enc = IS_8BIT_ENCODING(enc);
	if (!m_8bit_enc)
	{
		OP_DELETEA(m_existing_chars.linear);
		m_existing_chars.utf = UTFMapping<UINT32>::Create();
		if (!m_existing_chars.utf)
			return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	}
	RETURN_IF_ERROR(fd->SetDictionaryEncoding(enc));
	OP_DELETEA(m_enc_string);
	int str_len = op_strlen((const char*)enc_str);
	m_enc_string = OP_NEWA(char, str_len+1);
	if (!m_enc_string)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	op_strcpy(m_enc_string, (const char*)enc_str);
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseFLAG(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	HunspellAffixFileLine *line = state->GetCurrentLine();
	UINT8 *enc_str = line->WordAt(0);
	if (!enc_str)
		return OpStatus::OK;
	FlagEncoding enc = GetFlagEncodingFromString(enc_str);
	if (enc == FLAG_UNKNOWN_ENC || state->GetFlagsUsed() && enc != fd->GetFlagEncoding()) // Can't use different flag encodings...
		return SPC_ERROR(OpStatus::ERR);
	RETURN_IF_ERROR(fd->SetFlagEncoding(enc));
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseCOMPLEXPREFIXES(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseCOMPOUNDFLAG(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return ParseFlagGeneral(fd,state,AFFIX_TYPE_COMPOUNDFLAG);
}

OP_STATUS OpSpellChecker::ParseCOMPOUNDBEGIN(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return ParseFlagGeneral(fd,state,AFFIX_TYPE_COMPOUNDBEGIN);
}

OP_STATUS OpSpellChecker::ParseCOMPOUNDMIDDLE(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return ParseFlagGeneral(fd,state,AFFIX_TYPE_COMPOUNDMIDDLE);
}

OP_STATUS OpSpellChecker::ParseCOMPOUNDEND(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return ParseFlagGeneral(fd,state,AFFIX_TYPE_COMPOUNDEND);
}

OP_STATUS OpSpellChecker::ParseCOMPOUNDWORDMAX(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	RETURN_IF_ERROR(ParseIntValue(&m_compoundwordmax,fd,state));
	m_compoundwordmax = MIN(m_compoundwordmax, SPC_MAX_ALLOWED_COMPOUNDWORDMAX);
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseCOMPOUNDROOT(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseCOMPOUNDPERMITFLAG(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return ParseFlagGeneral(fd,state,AFFIX_TYPE_COMPOUNDPERMITFLAG);
}

OP_STATUS OpSpellChecker::ParseCOMPOUNDFORBIDFLAG(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return ParseFlagGeneral(fd,state,AFFIX_TYPE_COMPOUNDFORBIDFLAG);
}

OP_STATUS OpSpellChecker::ParseCHECKCOMPOUNDDUP(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseCHECKCOMPOUNDREP(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseCHECKCOMPOUNDTRIPLE(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseCHECKCOMPOUNDCASE(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	m_checkcompoundcase = TRUE;
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseNOSUGGEST(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return ParseFlagGeneral(fd,state,AFFIX_TYPE_NOSUGGEST);
}

OP_STATUS OpSpellChecker::ParseFORBIDDENWORD(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseLEMMA_PRESENT(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseCIRCUMFIX(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	int dummy;
	HunspellAffixFileLine *line = state->GetCurrentLine();
	if (!line->GetWordCount())
		return SPC_ERROR(OpStatus::ERR);
	state->SetFlagsUsed();
	int flag_index = decode_flag(line->WordAt(0),fd->GetFlagEncoding(),dummy);
	if (flag_index < 0)
		return SPC_ERROR(OpStatus::ERR);
	RETURN_IF_ERROR(fd->AddCircumfixFlagIndex(flag_index));
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseONLYINCOMPOUND(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return ParseFlagGeneral(fd,state,AFFIX_TYPE_ONLYINCOMPOUND);
}

OP_STATUS OpSpellChecker::ParsePSEUDOROOT(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return ParseFlagGeneral(fd,state,AFFIX_TYPE_NEEDAFFIX);
}

OP_STATUS OpSpellChecker::ParseNEEDAFFIX(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return ParseFlagGeneral(fd,state,AFFIX_TYPE_NEEDAFFIX);
}

OP_STATUS OpSpellChecker::ParseCOMPOUNDMIN(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return ParseIntValue(&m_compoundmin,fd,state);
}

OP_STATUS OpSpellChecker::ParseCOMPOUNDSYLLABLE(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseSYLLABLENUM(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseCHECKNUM(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseWORDCHARS(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	state->SetDicCharsUsed();
	return fd->AddWordChars(state->GetCurrentLine()->WordAt(0));
}

OP_STATUS OpSpellChecker::ParseIGNORE(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseCHECKCOMPOUNDPATTERN(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseCOMPOUNDRULE(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	if (state->GetPass() == 0)
		return ParseCompoundRulePass0(fd,state);
	OP_ASSERT(state->GetPass() == 1);
	return ParseCompoundRulePass1(fd,state);
}

OP_STATUS OpSpellChecker::ParseCompoundRulePass0(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	int i,j;
	HunspellAffixFileLine *line = state->GetCurrentLine();
	if (!fd->GetFlagTypeCount(AFFIX_TYPE_COMPOUNDRULEFLAG)) // first COMPOUNDRULE is number of compoundrules
	{
		fd->IncTypeIdFor(AFFIX_TYPE_COMPOUNDRULEFLAG); // reserve 0 for when there are no compoundrule flag
		return OpStatus::OK;
	}
	state->SetFlagsUsed();
	UINT8 *str = line->WordAt(0);
	if (!str)
		return OpStatus::OK;
	int cond_count = 0;
	CompoundRuleConditionData *rule_data = GetCompoundRuleConditionData(str,TempBuffer(),SPC_TEMP_BUFFER_BYTES,fd->GetFlagEncoding(),cond_count);
	if (!rule_data)
		return SPC_ERROR(OpStatus::ERR);
	
	OP_ASSERT(cond_count);
	int plus_count = 0;
	for (i=0;i<cond_count;i++)
	{
		if (rule_data[i].multiple_type == '+')
			plus_count++;
		for (j=0;j<rule_data[i].len;j++)
		{
			int flag_index = (int)rule_data[i].indexes[j];
			FlagClass *flag_class = fd->GetFlagClass(flag_index);
			if (!flag_class)
			{
				flag_class = OP_NEW(FlagClass, (AFFIX_TYPE_COMPOUNDRULEFLAG,flag_index));
				if (!flag_class)
					return SPC_ERROR(OpStatus::ERR);
				RETURN_IF_ERROR(fd->AddFlagClass(flag_class,state));
			}
			else if (flag_class->GetType() != AFFIX_TYPE_COMPOUNDRULEFLAG)
				return SPC_ERROR(OpStatus::ERR);
		}
	}
	ReleaseTempBuffer();
	cond_count += plus_count; // a+ == aa*
	if (cond_count > SPC_COMPUNDRULE_MAX_CONDITIONS)
		return SPC_ERROR(OpStatus::ERR);
	state->IterateInNextPass(line);
	line->SetParserData((void*)cond_count);
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseCompoundRulePass1(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	int i,j;
	HunspellAffixFileLine *line = state->GetCurrentLine();
	UINT8 *str = line->WordAt(0);
	int total_cond_count = (int)(long)line->GetParserData();
	CompoundRule *rule = OP_NEW(CompoundRule, ());
	if (!rule)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	if (m_compound_rules.AddElement(rule) < 0)
	{
		OP_DELETE(rule);
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	}
	RETURN_IF_ERROR(rule->Initialize(total_cond_count,fd->GetFlagTypeCount(AFFIX_TYPE_COMPOUNDRULEFLAG),&m_allocator));

	int cond_count = 0;
	CompoundRuleConditionData *rule_data = GetCompoundRuleConditionData(str,TempBuffer(),SPC_TEMP_BUFFER_BYTES,fd->GetFlagEncoding(),cond_count);
	if (!rule_data)
	{
		OP_ASSERT(!"This should work as it worked in pass 0.");
		return SPC_ERROR(OpStatus::ERR);
	}

	for (i=0;i<cond_count;i++)
	{
		for (j=0;j<rule_data->len;j++)
		{
			FlagClass *flag_class = fd->GetFlagClass(rule_data->indexes[j]);
			OP_ASSERT(flag_class && flag_class->GetType() == AFFIX_TYPE_COMPOUNDRULEFLAG);
			rule_data->indexes[j] = flag_class->GetFlagTypeId();
		}
		int multiple_type = rule_data->multiple_type;
		int loop = multiple_type == '+' ? 2 : 1;
		if (multiple_type == '+')
			multiple_type = 0;
		for (j=0;j<loop;j++)
		{
			rule->AddCondition(rule_data->indexes,rule_data->len,rule_data->negative,!!multiple_type);
			multiple_type = '*';
		}
		rule_data++;
	}
	ReleaseTempBuffer();
	OP_ASSERT(rule->GetAddedConditionCount() == rule->GetTotalConditionCount());
	m_has_compound_rule_flags = TRUE;
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseMAP(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	HunspellAffixFileLine *line = state->GetCurrentLine();
	if (!state->GetMapDetected()) // first MAP is number of MAP entries that follows
	{
		state->SetMapDetected();
		return OpStatus::OK;
	}
	UINT8 *str = line->WordAt(0);
	if (!str)
		return OpStatus::OK;
	state->SetDicCharsUsed();
	int str_len = SPC_STRLEN(str);
	if (str_len > SPC_MAX_PER_MAPPING)
		return SPC_ERROR(OpStatus::ERR);
	if (str_len <= 1)
		return OpStatus::OK;
	int mapping_idx = m_mappings.GetSize();
	if (mapping_idx == SPC_MAX_INDEXES)
		return SPC_ERROR(OpStatus::ERR);
	MapMapping map_mapping;

	int i, ch, map_count = 0;
	for (i=0;i<str_len;i++)
	{
		SPC_NEXT_CHAR(ch,str);
		if (ch <= 0)
			return SPC_ERROR(OpStatus::ERR);
		if (!GetCharMapping(ch).IsPointer())
			map_count++;
	}
	str = line->WordAt(0);
	if (map_count < 2)
		return OpStatus::OK;

	UINT32 *map = (UINT32*)m_allocator.Allocate32(sizeof(UINT32)*(map_count)*2); // + additional space for BIG/small chars
	UINT32 *freq = (UINT32*)m_allocator.Allocate32(sizeof(UINT32)*(map_count)*2); // + additional space for BIG/small chars
	if (!map || !freq)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	
	int pos = 0;
	for (i=0;i<str_len;i++)
	{
		SPC_NEXT_CHAR(ch,str);
		if (GetCharMapping(ch).IsPointer())
			continue;
		
		map[pos] = (UINT32)ch;
		RETURN_IF_ERROR(AddToCharMappings(ch,mapping_idx,pos));
		pos++;
	}
	map_mapping.char_count = map_count;
	map_mapping.bit_representation = bits_to_represent(map_count-1); // exclude "most frequent char"
	map_mapping.chars = map;
	map_mapping.frequencies = freq;
	if (m_mappings.AddElement(map_mapping) < 0)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseBREAK(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	state->SetDicCharsUsed();
	return fd->AddBreakChars(state->GetCurrentLine()->WordAt(0));
}

OP_STATUS OpSpellChecker::ParseLANG(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseVERSION(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseMAXNGRAMSUGS(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseNOSPLITSUGS(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseSUGSWITHDOTS(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseKEEPCASE(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return ParseFlagGeneral(fd,state,AFFIX_TYPE_KEEPCASE);
}

OP_STATUS OpSpellChecker::ParseSUBSTANDARD(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseCHECKSHARPS(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	m_checksharps = TRUE;
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseAffixPass0(HunspellAffixFileData *fd, HunspellAffixFileParseState *state, BOOL is_suffix)
{
	UINT8 *str;
	HunspellAffixFileLine *line = state->GetCurrentLine();
	if (line->GetWordCount() < 3)
		return SPC_ERROR(OpStatus::ERR);
	int dummy = 0;
	state->SetFlagsUsed();
	int flag_index = decode_flag(line->WordAt(0),fd->GetFlagEncoding(),dummy);
	if (flag_index < 0)
		return SPC_ERROR(OpStatus::ERR); // invalid name
	BOOL can_combine = *(line->WordAt(1)) == 'Y';
	int rule_count = pos_num_to_int(line->WordAt(2),65000,dummy);
	if (rule_count < 0)
		return SPC_ERROR(OpStatus::ERR);
	if (rule_count == 0)
		return OpStatus::OK;
	
	OpSpellCheckerAffix *affix = OP_NEW(OpSpellCheckerAffix, (flag_index,is_suffix,can_combine,rule_count));
	if (!affix)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	RETURN_IF_ERROR(fd->AddFlagClass(affix,state));
	line->SetParserData(affix);
	state->IterateInNextPass(line);
	
	int actual_rule_count = 0;
	int line_index = line->GetLineIndex()+1;
	HunspellAffixFileLine *rule_line = fd->LineAt(line_index);
	while (rule_line && rule_line->Type() == line->Type() && decode_flag(rule_line->WordAt(0),fd->GetFlagEncoding(),dummy) == flag_index)
	{
		OpSpellCheckerAffixRule rule;
		UINT32 *res = NULL;
		actual_rule_count++;
		if (rule_line->GetWordCount() < 4)
			return SPC_ERROR(OpStatus::ERR);
		UINT8 *strip_chars = rule_line->WordAt(1);
		UINT8 *affix_chars = rule_line->WordAt(2);
		UINT8 *cond_str = rule_line->WordAt(3);
		if (*strip_chars != '0')
		{
			RETURN_IF_ERROR(GetWideStrPtr(strip_chars,res,fd->Allocator()));
			rule.SetStripChars(res);
		}
		str = affix_chars;
		BOOL has_affix_chars = *str != '0' && *str != '/';

		while (*str && *str != '/')
				str++;

		if (*str == '/') // there are flags...
		{
			*str = '\0';
			if (str[1])
				rule.SetFlags(str+1); // wait with flag-parsing to next pass for the possibility of aliases
		}
		res = NULL;
		if (has_affix_chars)
			RETURN_IF_ERROR(GetWideStrPtr(affix_chars,res,fd->Allocator()));
		rule.SetAffixChars(res);
		RETURN_IF_ERROR(AddExistingCharsAtAffix(rule.GetAffixChars(), rule.GetAffixCharLength()));
		if (*cond_str != '.' && has_any_not(cond_str, "[]^")) // there are conditions...
			rule.SetConditionString(cond_str);
		RETURN_IF_ERROR(affix->AddRule(rule));
		rule_line = fd->LineAt(++line_index);
	}
	state->SetCurrentLine(line_index);
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseAffixPass1(HunspellAffixFileData *fd, HunspellAffixFileParseState *state, BOOL is_suffix)
{
	int i,j,k;
	OpSpellCheckerAffix *affix = (OpSpellCheckerAffix*)state->GetCurrentLine()->GetParserData();
	if (!affix)
		return OpStatus::OK; // this affix has been replaced by another one with the same flag-name, hack in order to support error (I guess) in en_GB
	int char_count = 1; // zero is the "unknown" character (for chars not present in this affix's condition strings)
	int max_cond_len = 0;
	for (i=0;i<affix->GetRuleCount();i++) // assign indexes, count condition characters + set flags
	{
		OpSpellCheckerAffixRule *rule = affix->GetRulePtr(i);
		if (rule->GetFlags())
		{
			UINT16 *flags = NULL;
			RETURN_IF_ERROR(GetFlagPtr(rule->GetFlagsUnParsed(), flags, fd, fd->Allocator()));
			rule->SetFlags(flags);
		}
		UINT8 *cond = rule->GetConditionString();
		if (!cond)
			continue;

		int cond_len = 0;
		AffixConditionData *cond_data = GetAffixConditionData(cond,TempBuffer(),SPC_TEMP_BUFFER_BYTES,cond_len);
		if (!cond_data || cond_len > SPC_MAX_AFFIX_RULE_CONDITION_LENGTH)
			return SPC_ERROR(OpStatus::ERR);
		for (j=0;j<cond_len;j++)
		{
			for (k=0;k<cond_data[j].len;k++)
			{
				int ch = (int)(cond_data[j].chars[k]);
				UINT16 *ch_idx = fd->GetCharIndexPtrForAffix(ch,affix);
				if (!ch_idx)
					return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
				if (!*ch_idx)
				{
					*ch_idx = (UINT16)(char_count++);
					if (char_count&0x10000)
						return SPC_ERROR(OpStatus::ERR); // Too many different chars in the same affix's conditions
				}
			}
		}
		ReleaseTempBuffer();
		OP_ASSERT(cond_len);
		rule->SetConditionLength(cond_len);
		max_cond_len = MAX(cond_len,max_cond_len);
	}
	if (char_count == 1)
		char_count = 0; // only unconditional rules in this affix
	RETURN_IF_ERROR(affix->SetConditionProperties(max_cond_len,char_count));
	for (i=0;i<affix->GetRuleCount();i++) 
	{
		OpSpellCheckerAffixRule *rule = affix->GetRulePtr(i);
		UINT8 *cond = rule->GetConditionString();
		if (!cond)
			continue;
		
		int cond_len = 0;
		int str_len = op_strlen((const char*)cond);
		int *cond_chars = (int*)TempBuffer();
		UINT8 *buffer = (UINT8*)cond_chars; buffer += str_len * sizeof(int); // use TempBuffer for cond_data as well
		int buf_size = SPC_TEMP_BUFFER_BYTES - str_len*sizeof(int);
		AffixConditionData *cond_data = GetAffixConditionData(cond,buffer,buf_size,cond_len);
		if (!cond_data)
			return SPC_ERROR(OpStatus::ERR);
		OP_ASSERT(cond_len == rule->GetConditionLength());
		for (j=0;j<cond_len;j++)
		{
			int cond_char_count = cond_data[j].len;
			for (k=0;k<cond_char_count;k++)
			{
				cond_chars[k] = fd->GetCharIndexForAffix(cond_data[j].chars[k],affix);
				OP_ASSERT(cond_chars[k]);
			}
			int pos = affix->IsSuffix() ? cond_len - j - 1 : j;
			affix->SetRuleConditions(rule,cond_chars,cond_char_count,pos,cond_data[j].negative);
		}
		ReleaseTempBuffer();
	}
	return OpStatus::OK;
}

OP_STATUS OpSpellChecker::ParseAffix(HunspellAffixFileData *fd, HunspellAffixFileParseState *state, BOOL is_suffix)
{
	state->SetDicCharsUsed();
	if (state->GetPass() == 0)
		return ParseAffixPass0(fd,state,is_suffix);
	if (state->GetPass() == 1)
		return ParseAffixPass1(fd,state,is_suffix);
	OP_ASSERT(FALSE);
	return SPC_ERROR(OpStatus::ERR);
}

OP_STATUS OpSpellChecker::ParseREP(HunspellAffixFileData *fd, HunspellAffixFileParseState *state)
{
	HunspellAffixFileLine *line = state->GetCurrentLine();
	UINT8 *to_rep = line->WordAt(0);
	if (!to_rep)
		return OpStatus::OK;
	int to_rep_len = SPC_STRLEN(to_rep);
	UINT8 *rep = line->WordAt(1);
	if (!rep && !m_rep.GetSize() && *to_rep > '0' && *to_rep < '9')
		return OpStatus::OK; // this is probably just a number telling how many REP entries there are...
	int rep_len = rep ? SPC_STRLEN(rep) : 0;
	UINT32 *rep_data = (UINT32*)m_allocator.Allocate32(sizeof(UINT32)*(to_rep_len+rep_len+3));
	if (!rep_data)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	UINT32 *rep_start = rep_data;
	*(rep_data++) = to_rep_len;
	RETURN_IF_ERROR(GetWideStrPtr(to_rep,rep_data,NULL));
	rep_data += to_rep_len;
	(*rep_data++) = rep_len;
	if (rep_len)
		RETURN_IF_ERROR(GetWideStrPtr(rep,rep_data,NULL));
	if (m_rep.AddElement(rep_start) < 0)
		return SPC_ERROR(OpStatus::ERR_NO_MEMORY);
	return OpStatus::OK;
}

#endif

#endif // INTERNAL_SPELLCHECK_SUPPORT
