/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/
 
#ifndef _ARG_SPLIT_H_
#define _ARG_SPLIT_H_

#include "modules/formats/seqsplit.h"

#define PARAM_SEP_COMMA			NVS_SEP_COMMA
#define PARAM_SEP_SEMICOLON		NVS_SEP_SEMICOLON
#define PARAM_SEP_WHITESPACE	NVS_SEP_WHITESPACE
#define PARAM_STRIP_ARG_QUOTES	NVS_STRIP_ARG_QUOTES
#define PARAM_CHECK_FOR_INVALID_QUOTES  NVS_CHECK_FOR_INVALID_QUOTES 
#define PARAM_STRIP_ARG_QUOTES_GENTLY	NVS_STRIP_ARG_QUOTES_GENTLY
#define PARAM_NO_ASSIGN_FIRST_ONLY		NVS_NO_ASSIGN_FIRST_ONLY

// No assignements in the parameters
#define PARAM_NO_ASSIGN			NVS_VAL_SEP_NO_ASSIGN
 // Separation chars are the only breakchars (trims leading/trailing whitespace)
#define PARAM_ONLY_SEP			NVS_ONLY_SEP
#define PARAM_WHITESPACE_FIRST_ONLY		NVS_WHITESPACE_FIRST_ONLY

#define PARAM_COPY_CONTENT		NVS_COPY_CONTENT	// Allocates memory and duplicates content
#define PARAM_TAKE_CONTENT		NVS_TAKE_CONTENT	// Uses buffer and Free's the memory during destruction
#define PARAM_BORROW_CONTENT	NVS_BORROW_CONTENT	// Do not free content. Free'd externally

#define PARAM_HAS_RFC2231_VALUES NVS_HAS_RFC2231_VALUES  // The parameters can be coded according to RFC 2231

//class ParameterList;

typedef Sequence_Splitter ParameterList;

typedef NameValue_Splitter Parameters;

/** Contains an allocated parameter string, if necessary with a charset and language */
class Allocated_Parameter : public Parameters
{
private:
	OpString8 m_name;
	OpString8 m_value;
	OpString8 m_charset;
	OpString8 m_language;

public:
	Allocated_Parameter(){};

	void InitializeL(const OpStringC8 &a_Name, const OpStringC8 &a_Value, const OpStringC8 &a_Charset, const OpStringC8 &a_Language);

	virtual const char *Charset()const{return m_charset.CStr();}
#ifdef FORMATS_NEED_LANGUAGE
	virtual const char *Language()const{return m_language.CStr();}
#endif


protected: 
	virtual NameValue_Splitter *DuplicateL() const;

};


class UniParameterList;

/** Unicode version of parameterlist, internal format is UTF-8, 
 *	Otherwise the same API  as Parameters/NameValue_Pair class 
 */
class UniParameters : public Parameters
{
protected:
	OpString uni_name;
	OpString uni_value;
	
public:	
	UniParameters(){};
	
	/** Internal format is UTF8 */
	virtual char *InitL(char *,int flags);
	
	const uni_char *Name() const {return uni_name.CStr();};	
	virtual void StripQuotes(BOOL gently=FALSE);
	const uni_char *Value() const {return assigned ? uni_value.CStr() : UNI_L("");};

	UniParameterList *SubParameters();
	UniParameterList *GetParameters(const KeywordIndex *keys, int count, unsigned flags=0, Prepared_KeywordIndexes index=KeywordIndex_None);
	UniParameterList *GetParametersL(const KeywordIndex *keys, int count, unsigned flags=0, Prepared_KeywordIndexes index=KeywordIndex_None);
	UniParameterList *GetParameters(unsigned flags=0, Prepared_KeywordIndexes index=KeywordIndex_None){return GetParameters(NULL, 0, flags, index);};
	UniParameterList *GetParametersL(unsigned flags=0, Prepared_KeywordIndexes index=KeywordIndex_None){return GetParametersL(NULL, 0, flags, index);};
	
	UniParameters *Suc(){return static_cast<UniParameters *>(Parameters::Suc());}	
	UniParameters *Pred(){return static_cast<UniParameters *>(Parameters::Pred());}	

protected: 
	void ConstructDuplicateL(const UniParameters *old);
	virtual NameValue_Splitter *DuplicateL() const;
	virtual Sequence_Splitter *CreateSplitterL() const;
};

/** Contains an allocated parameter string, if necessary with a charset and language */
class UniAllocated_Parameter : public UniParameters
{
private:
	OpString8 m_name;
	OpString8 m_value;
	OpString8 m_charset;
	OpString8 m_language;

public:
	UniAllocated_Parameter(){};

	void InitializeL(const OpStringC8 &a_Name, const OpStringC8 &a_Value, const OpStringC8 &a_Charset, const OpStringC8 &a_Language);

	virtual const char *Charset()const{return m_charset.CStr();}
#ifdef FORMATS_NEED_LANGUAGE
	virtual const char *Language()const{return m_language.CStr();}
#endif


protected: 
	virtual NameValue_Splitter *DuplicateL() const;
};

/** Unicode version of ParameterList
 *	
 *	The input string is converted to UTF-8 and processed as a 8 bit string
 *
 *	Keyword Indexes MUST be in UTF-8 encoding
 */
class UniParameterList : public ParameterList
{
private:
	OpString8 argument; // UTF8 representation

public:
	UniParameterList(){};
	UniParameterList(Prepared_KeywordIndexes index) : ParameterList(index){};
	UniParameterList(const KeywordIndex *keys, unsigned int count) : ParameterList(keys, count){}; // Use UTF8 strings!!
	
	virtual ~UniParameterList();
	
	OP_STATUS SetValue(const uni_char *value, unsigned flags=0);
	void SetValueL(const uni_char *value, unsigned flags=0);
	
	UniParameters *GetParameter(const uni_char *tag, 
		Parameter_UseAssigned assign = PARAMETER_ANY,
		UniParameters *after = NULL,
		Parameter_ResolveKeyword resolve=PARAMETER_NO_RESOLVE);	
	
	UniParameters *GetParameterByID(unsigned int tag_id, 
		Parameter_UseAssigned assign = PARAMETER_ANY,
		UniParameters *after = NULL){return static_cast<UniParameters *>(ParameterList::GetParameterByID(tag_id, assign, after));}

	
	UniParameters *First(){return static_cast<UniParameters *>(ParameterList::First());};
	UniParameters *Last(){return static_cast<UniParameters *>(ParameterList::Last());};
	
protected:
	virtual NameValue_Splitter *CreateNameValueL() const;
	virtual Parameters *CreateAllocated_ParameterL(const OpStringC8 &a_Name, const OpStringC8 &a_Value, const OpStringC8 &a_Charset, const OpStringC8 &a_Language); 
	virtual Sequence_Splitter *CreateSplitterL() const;

public:

	UniParameterList *DuplicateListL() const {return static_cast<UniParameterList *>(Sequence_Splitter::DuplicateL());};
};

inline UniParameterList *UniParameters::SubParameters()
{
	return static_cast<UniParameterList *>(Parameters::SubParameters());
}

inline UniParameterList *UniParameters::GetParameters(const KeywordIndex *keys, int count, unsigned flags, Prepared_KeywordIndexes index)
{
	return static_cast<UniParameterList *>(Parameters::GetParameters(keys, count, flags, index));
}

inline UniParameterList *UniParameters::GetParametersL(const KeywordIndex *keys, int count, unsigned flags, Prepared_KeywordIndexes index)
{
	return static_cast<UniParameterList *>(Parameters::GetParametersL(keys, count, flags, index));
}

#endif
