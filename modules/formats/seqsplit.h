/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#ifndef _SEQSPLIT_H_
#define _SEQSPLIT_H_

#include "modules/formats/keywordman.h"

/** Pairs are Comma separated */
#define NVS_SEP_COMMA			0x01
/** Pairs are Semicolon separated */
#define NVS_SEP_SEMICOLON		0x02
/** Pairs are Whitespace separated */
#define NVS_SEP_WHITESPACE		0x04
/** Pairs are CRLF separated. NOTE: NVS_SEP_COMMA, NVS_SEP_SEMICOLON and NVS_SEP_WHITESPACE are ignored*/
#define NVS_SEP_CRLF			0x10
/** Pairs are CRLF separated, but a value can be continued on the next line by using a whitespace first on the line.
 *	NOTE: NVS_SEP_COMMA, NVS_SEP_SEMICOLON and NVS_SEP_WHITESPACE are ignored*/
#define NVS_SEP_CRLF_CONTINUED	0x18
/** Strip quotes from value */
#define NVS_STRIP_ARG_QUOTES	0x20

/** Default: Pairs components are separated by assignment ("=") characters */
#define NVS_VAL_SEP_ASSIGN		0x00
/** No assignments in the parameters. The items will be treated as a values, e.g. quotes may be stripped */
#define NVS_VAL_SEP_NO_ASSIGN	0x40
/** Pair components are separated by colons, overrides NVS_VAL_SEP_NO_ASSIGN and NVS_VAL_SEP_ASSIGN, whitespace is trimmed
 *	The colon MUST be on the first line in the case of the NVS_SEP_CRLF_CONTINUED mode
 */
#define NVS_VAL_SEP_COLON		0x80
/** Separation chars are the only breakchars (trims leading/trailing whitespace) */
#define NVS_ONLY_SEP			0x100

/** First item in name/value pair sequence is followed by a whitespace, overrides NVS_SEP_* prefs */
#define NVS_WHITESPACE_FIRST_ONLY	0x200

/** Content is copied into an allocated buffer*/
#define NVS_COPY_CONTENT		0x000
/** Sequence splitter takes ownership of content, and deallocates ir after use*/
#define NVS_TAKE_CONTENT		0x400
/** The buffer is owned by caller, and will be freed by caller.
 *	NOTE: The buffer will be modified by the sequence splitter, and the buffer MUST be in writable memory!!
 *  NOTE: The buffer MUST NOT be deleted before the sequence splitter has been destroyed
 */
#define NVS_BORROW_CONTENT		0x800

#define NVS_SEP_MASK   (NVS_SEP_COMMA | NVS_SEP_SEMICOLON | NVS_SEP_WHITESPACE | NVS_SEP_CRLF | NVS_SEP_CRLF_CONTINUED)
#define NVS_VAL_SEP_MASK (NVS_VAL_SEP_ASSIGN | NVS_VAL_SEP_NO_ASSIGN | NVS_VAL_SEP_COLON | NVS_ONLY_SEP)
#define NVS_BUFFER_MASK (NVS_COPY_CONTENT | NVS_TAKE_CONTENT | NVS_BORROW_CONTENT)

/** The parameters may be using RFC 2231 escape syntax */
#define NVS_HAS_RFC2231_VALUES	0x1000

/** If the values contain invalid quotes, raise error (unclosed quotes are always discarded)*/
#define NVS_CHECK_FOR_INVALID_QUOTES  0x2000

/** If the _name_ of a name/value pair contain a quote, accept it even if it isn't legal, but discard afterwards */
#define NVS_ACCEPT_QUOTE_IN_NAME	0x4000

/** Strip qoutes from value. If a CJK character set is detected, only remove backslashes from \" and \\. */
#define NVS_STRIP_ARG_QUOTES_GENTLY	(0x8000+NVS_STRIP_ARG_QUOTES)

/** Special flag used to allow spaces in cookie names */
#define NVS_COOKIE_SEPARATION 0x10000

/** First item in name/value pair sequence has no assignment (i.e. NVS_VAL_SEP_NO_ASSIGN) */
#define NVS_NO_ASSIGN_FIRST_ONLY 0x20000

/** First bitvalue not reserved by the Sequence Splitter, All bits with value above or equal to this bit is ignored */
#define NVS_USER_SETTING		0x40000

class Sequence_Splitter;

enum RFC_2231_Escape_Mode {
	RFC_2231_Not_Escaped,
	RFC_2231_Single,
	RFC_2231_Numbered,
	RFC_2231_Fully_Escaped,
	RFC_2231_Decoded
};

enum Parameter_UseAssigned {
	PARAMETER_ANY, // e.g domain = "" or domain = "something"
	PARAMETER_ASSIGNED_ONLY, // e.g. domain = "somthing"
	PARAMETER_NO_ASSIGNED // detecting presence or absence of parameter e.g. secure
};


enum Parameter_ResolveKeyword {
	PARAMETER_NO_RESOLVE = KEYWORD_NO_RESOLVE,
	PARAMETER_RESOLVE = KEYWORD_RESOLVE
};

/** The basic container for a name value pair
 *	Normally the input is a string which is formatted according to the NVS_* flags
 *	The object breaks off the first name and value pair according to those rules
 */
class NameValue_Splitter : public KeywordIndexed_Item
{
	friend class Sequence_Splitter;
protected:
	/** The name of the argument pair */
	const char *name;

	/** The value of the argument pair */
	const char *value;

	/** Is this an assigned object? (name=value) */
	BOOL assigned;

	/** Is this object invalid? Used to discard invalid items */
	BOOL is_invalid;

	/** index of this attribute in an rfc2231 sequence */
	unsigned int rfc2231_index;

	/** What kind of RFC 2231 escaping is done in the value part */
	RFC_2231_Escape_Mode	rfc2231_escape_mode;

	/** The actual name of the RFC 2231 attribute this attribute is a part of */
	OpStringS8	rfc2231_Name;

	/** Used to store copied name and value string for copied sequences.
	 *	Both the name and the value strings are stored in this buffer,
	 *	separated by a null termination
	 */
	char * internal_buffer;

	/** The sequence splitter initialized with the parameter value of this object */
	Sequence_Splitter *parameters;

public:
	/** Constructor */
	NameValue_Splitter();

protected:

	/** Create a duplicate of the argument splitter */
	void ConstructDuplicateL(const NameValue_Splitter *);

	/** Internal initialization */
	void InternalInit();

private:

	/** Reset the object, delete all allocated data */
	void Clean();

public:

	/** Destructor */
	virtual ~NameValue_Splitter();

	/** Initialization of the object by extracting the name and optionally the value from
	 *	the argument string, according to the rules specified by the flags argument.
	 *
	 *	If the input data are using the RFC 2231 format the necessary data will be extracted.
	 *
	 *	@param	val		source string, the function will insert null terminations in this
	 *					string at the places that it finds the terminating characters specified by the flag
	 *	@param flags	ORed bitmask of NVS_* flags which determines the syntax of the name/value pairs
	 *
	 *	@return	char *	pointer to fist character after the name/value pair.
	 */
	virtual char *InitL(char *val, int flags);

	/** Construct this NameValue_Splitter object from an existing name and value pair
	 *	the object is added into the specified Sequence_Splitter object.
	 *	@param aList the Sequence_Splitter to add this  Header to
	 *	@param aName the name to be used
	 *	@param aValue the value of the object
	 *	@param aValLen the length of aValue
	 */
	void ConstructFromNameAndValueL(Sequence_Splitter &aList, const char *aName, const char * aValue, int aValLen);

	/** Returns the name of the name/value pair */
	const char *Name() const {return name;};
	/** Is this name assigned a value? */
	BOOL AssignedValue() const {return assigned;};
	/** Return value */
	const char *Value() const {return assigned ? value : "";};

	/** Return an OpStringC8 object for the name string */
	const OpStringC8 GetName() const{OpStringC8 temp(Name()); return temp;}
	/** Return an OpStringC8 object for the value string */
	const OpStringC8 GetValue() const{OpStringC8 temp(Value()); return temp;}

	/** Return the charset used for the value (RFC 2231) */
	virtual const char *Charset() const;
#ifdef FORMATS_NEED_LANGUAGE
	/** Return the language used for the value (RFC 2231).
	 *	Calling module must import API_FORMATS_NEED_LANGUAGE */
	virtual const char *Language() const;
#endif

	/** remove qoutes from the value
	 *  @param gently If true and a CJK character set is detected, only remove backslashes from \" and \\. */
	virtual void StripQuotes(BOOL gently=FALSE);

	/** Convert the value string to an unsigned number using the given base */
	unsigned long GetUnsignedValue(int base=10);
	/** Convert the name string to an unsigned number of the given base */
	unsigned long GetUnsignedName(int base=10) const;
	/** Convert the value string to a signed integer, uses base = 10 */
	int GetSignedValue();

#ifdef _MIME_SUPPORT_
	/** Convert the value to a unicode OpString using the contained charset or the provided default charset */
	void GetValueStringL(OpString &target, const OpStringC8 &default_charset);
#endif

	/** LinkId function, returns the name value for the keyword parser */
	virtual const char* LinkId();

#ifdef FORMATS_ENABLE_DUPLICATE_INTO
	/** Create a duplicate of this object and insert it in the argument list.
	 *	Calling module must import API_FORMATS_DUPLICATE_INTO */
	void DuplicateIntoL(Sequence_Splitter *) const;
#endif

	/** Get the parameters list created earlier based on the contained value
	 *	NOTE: This sequence is owned by this object and MUST NOT be deleted
	 */
	Sequence_Splitter *SubParameters(){return parameters;};

	/** Create a new sequence list from the value string, based on the given keywords and flags.
	 *
	 *	@param	keys	list of sorted keywords and associated integers. The first element
	 *					in the list is the default element, the rest of the list must be
	 *					alphabetically sorted. The total length must be at lest count elements
	 *	@param	count	length of keys list
	 *	@param	flags	ORed bitmask of NVS_* flags which determines the syntax of the name/value pairs in the new sequence
	 *	@param	index	Enumed identificator of a predefined list of keywords
	 *
	 *	@return	Sequence_Splitter *		Pointer to the new sequence. NOTE: This sequence is owned by this object and MUST NOT be deleted
	 */
	Sequence_Splitter *GetParametersL(const KeywordIndex *keys, int count, unsigned flags=0, Prepared_KeywordIndexes index=KeywordIndex_None);

	/** Create a new sequence list from the value string, based on the given keywords and flags.
	 *
	 *	@param	flags	ORed bitmask of NVS_* flags which determines the syntax of the name/value pairs in the new sequence
	 *	@param	index	Enumed identificator of a predefined list of keywords
	 *
	 *	@return	Sequence_Splitter *		Pointer to the new sequence. NOTE: This sequence is owned by this object and MUST NOT be deleted
	 */
	Sequence_Splitter *GetParametersL(unsigned flags=0, Prepared_KeywordIndexes index=KeywordIndex_None){return GetParametersL(NULL, 0, flags, index);};

	/** Create a new sequence list from the value string, based on the given keywords and flags.
	 *
	 *	@param	keys	list of sorted keywords and associated integers. The first element
	 *					in the list is the default element, the rest of the list must be
	 *					alphabetically sorted. The total length must be at lest count elements
	 *	@param	count	length of keys list
	 *	@param	flags	ORed bitmask of NVS_* flags which determines the syntax of the name/value pairs in the new sequence
	 *	@param	index	Enumed identificator of a predefined list of keywords
	 *
	 *	@return	Sequence_Splitter *		Pointer to the new sequence. NOTE: This sequence is owned by this object and MUST NOT be deleted
	 */
	Sequence_Splitter *GetParameters(const KeywordIndex *keys, int count, unsigned flags=0, Prepared_KeywordIndexes index=KeywordIndex_None);

	/** Create a new sequence list from the value string, based on the given keywords and flags.
	 *
	 *	@param	flags	ORed bitmask of NVS_* flags which determines the syntax of the name/value pairs in the new sequence
	 *	@param	index	Enumed identificator of a predefined list of keywords
	 *
	 *	@return	Sequence_Splitter *		Pointer to the new sequence. NOTE: This sequence is owned by this object and MUST NOT be deleted
	 */
	Sequence_Splitter *GetParameters(unsigned flags=0, Prepared_KeywordIndexes index=KeywordIndex_None){return GetParameters(NULL, 0, flags, index);};

	/** Set the RFC 2231 index value */
	void SetRFC2231_Index(unsigned int i){rfc2231_index = i;}
	/** Get the RFC 2231 index value */
	unsigned int GetRFC2231_Index() const{return rfc2231_index;}
	/** Set the RFC 2231 escape mode */
	void SetRFC2231_Escaped(RFC_2231_Escape_Mode mode){rfc2231_escape_mode = mode;}
	/** Get the RFC 2231 escape mode */
	RFC_2231_Escape_Mode GetRFC2231_Escaped() const{return rfc2231_escape_mode;}
	/** Set the RFC 2231 attribute real name*/
	const OpStringC8 &GetRFC2231_Name() const{return rfc2231_Name;}

	BOOL Valid() const{return !is_invalid;}

	NameValue_Splitter *Suc() const {return (NameValue_Splitter *)Link::Suc();}
	NameValue_Splitter *Pred() const {return (NameValue_Splitter *)Link::Pred();}

protected:

	/** Create a duplicate of this object */
	virtual NameValue_Splitter *DuplicateL() const;

	/** Create custom Sequence_Splitter based object */
	virtual Sequence_Splitter *CreateSplitterL() const;
};


/** Splits a ext string containing any number of name/value pairs into a list of separate name/value pairs a
 *	according to several parsing rules that can be iterated through or searched.
 *
 *	The class can either copy the input string, take ownership of it, or just work on an externally owned string
 *
 *	In case the input string is borrowed the string MUST exists for the entire lifetime of the object using it, and
 *	the string will be updated by the splitter routine.
 */
class Sequence_Splitter : public Keyword_Manager
{
private:
	/** pointer to argument string (work buffer) */
	char *argument;

	/** Is the argument string owned by the caller? */
	BOOL external;

public:
	/** "Copy" constructor. Takes over (DOES NOT copy) the argument sequence list */
	Sequence_Splitter(Sequence_Splitter& other): Keyword_Manager(other) {argument=NULL;external=FALSE; TakeOver(other);};
	/** Default constructor */
	Sequence_Splitter();
	/** Use the specified prepared keyword list */
	Sequence_Splitter(Prepared_KeywordIndexes index);
	/* @deprecated Use the sorted list of keywords to index the list */
	//DEPRECATED(Sequence_Splitter(const char* const *keys, unsigned int count)); // inlined below
	/** Use the specified list of sorted keywords to index the items
	 *	The first element in the list is the default element, the rest of the list must be
	 *	alphabetically sorted. The total length must be at lest count elements
	 */
	Sequence_Splitter(const KeywordIndex *keys, unsigned int count);

	/** destructor */
	virtual ~Sequence_Splitter();


	/** Initialization of the object by extracting the name and optionally the value from
	 *	the argument string, according to the rules specified by the flags argument.
	 *
	 *	Depending on the flags the value will be copied, or taken over by the object. It may use the
	 *	buffer directly, but not assume ownership, in which case it will update the string in buffer,
	 *	and add null terminations.
	 *
	 *	If the input data are using the RFC 2231 format the necessary data will be extracted.
	 *
	 *	@param	value	source string, the function will insert null terminations in this
	 *					string at the places that it finds the terminating characters specified by the flag
	 *	@param flags	ORed bitmask of NVS_* flags which determines the syntax of the name/value pairs
	 *
	 *	@return	OP_STATUS	status of operation
	 */
	OP_STATUS SetValue(const char *value, unsigned flags=0); // inputs string containg parameters

	/** Initialization of the object by extracting the name and optionally the value from
	 *	the argument string, according to the rules specified by the flags argument.
	 *
	 *	Depending on the flags the value will be copied, or taken over by the object. It may use the
	 *	buffer directly, but not assume ownership, in which case it will update the string in buffer,
	 *	and add null terminations.
	 *
	 *	If the input data are using the RFC 2231 format the necessary data will be extracted.
	 *
	 *	@param	value	source string, the function will insert null terminations in this
	 *					string at the places that it finds the terminating characters specified by the flag
	 *	@param flags	ORed bitmask of NVS_* flags which determines the syntax of the name/value pairs
	 */
	void SetValueL(const char *value, unsigned flags=0); // inputs string containg parameters

	/** Reset the sequence, deleting all allocated data */
	void Clear();

	/** Retrieve a Name/value pair by searcing for it by name
	 *
	 *	@param	tag		Name of the attribute. If NULL the name of the after attribute will be used.
	 *	@param	assign	What kind of assignments are accepted?
	 *	@param	after	Start looking for the attribute after this object. If NULL start at the beginning
	 *	@param	resolve	Should the name be looked up in the keyword list first?
	 *
	 *	@return	NameValue_Splitter *	Pointer to the found object, NULL if no matching object was found
	 */
	NameValue_Splitter *GetParameter(const char *tag,
		Parameter_UseAssigned assign,
		NameValue_Splitter *after = NULL,
		Parameter_ResolveKeyword resolve=PARAMETER_NO_RESOLVE);

	/** Retrieve a Name/value pair by searcing for it by its integer keyword id
	 *
	 *	@param	tag_id	The ID of the attribute's name. If the ID is 0 (zero) the id of the after attribute will be used
	 *	@param	assign	What kind of assignments are accepted?
	 *	@param	after	Start looking for the attribute after this object. If NULL start at the beginning
	 *
	 *	@return	NameValue_Splitter *	Pointer to the found object, NULL if no matching object was found
	 */
	NameValue_Splitter *GetParameterByID(unsigned int tag_id,
		Parameter_UseAssigned assign,
		NameValue_Splitter *after = NULL);


	/** Retrieve a Name/value pair by searcing for it by name
	 *
	 *	@param	tag		Name of the attribute, If NULL the name of the after attribute will be used.
	 *	@param	after	Start looking for the attribute after this object. If NULL start at the beginning
	 *	@param	resolve	Should the name be looked up in the keyword list first?
	 *
	 *	@return	NameValue_Splitter *	Pointer to the found object, NULL if no matching object was found
	 */
	NameValue_Splitter *GetParameter(const char *tag,
		NameValue_Splitter *after = NULL,
		Parameter_ResolveKeyword resolve=PARAMETER_NO_RESOLVE){ return (NameValue_Splitter *) GetItem(tag, after, (Keyword_ResolvePolicy) resolve);}

	/** Retrieve a Name/value pair by searcing for it by its integer keyword id
	 *
	 *	@param	tag_id	The ID of the attribute's name. If the ID is 0 (zero) the id of the after attribute will be used
	 *	@param	after	Start looking for the attribute after this object. If NULL start at the beginning
	 *
	 *	@return	NameValue_Splitter *	Pointer to the found object, NULL if no matching object was found
	 */
	NameValue_Splitter *GetParameterByID(unsigned int tag_id,
		NameValue_Splitter *after = NULL){ return (NameValue_Splitter *) GetItemByID(tag_id, after);}

	/** Does an argument with the given keyword ID exist?
	 *
	 *	@param	parameter_id	The ID of the attribute's name. If the ID is 0 (zero) the id of the after attribute will be used
	 *	@param	after	Start looking for the attribute after this object. If NULL start at the beginning
	 *
	 *	@return	BOOL	TRUE if a matching element was found
	 */
	BOOL ParameterExists(int parameter_id, NameValue_Splitter *after=NULL){
			return (GetParameterByID(parameter_id, after) ? TRUE : FALSE);
		}

	/** Does an argument with the given keyword ID exist?
	 *
	 *	@param	parameter_id	The ID of the attribute's name. If the ID is 0 (zero) the id of the after attribute will be used
	 *	@param	assign	What kind of assignments are accepted?
	 *	@param	after	Start looking for the attribute after this object. If NULL start at the beginning
	 *
	 *	@return	BOOL	TRUE if a matching element was found
	 */
	BOOL ParameterExists(int parameter_id, Parameter_UseAssigned assign, NameValue_Splitter *after=NULL){
			return (GetParameterByID(parameter_id, assign, after) ? TRUE : FALSE);
		}

#ifdef FORMATS_GETVALUE8
	/** Retrieve the valuestring of an argument with the given keyword ID.
	 *	Calling module must import API_FORMATS_GETVALUE8
	 *
	 *	@param	target	Reference to the target string where the resulting string will be put (empty if not found)
	 *	@param	parameter_id	The ID of the attribute's name. If the ID is 0 (zero) the id of the after attribute will be used
	 *	@param	after	Start looking for the attribute after this object. If NULL start at the beginning
	 */
	void GetValueStringFromParameterL(OpStringS8 &target, int parameter_id, NameValue_Splitter *after=NULL);
#endif

#ifdef FORMATS_GETVALUE8_ASSIGN
	/** Retrieve the valuestring of an argument with the given keyword ID
	 *	Calling module must import API_FORMATS_GETVALUE8_ASSIGN
	 *
	 *	@param	target	Reference to the target string where the resulting string will be put (empty if not found)
	 *	@param	parameter_id	The ID of the attribute's name. If the ID is 0 (zero) the id of the after attribute will be used
	 *	@param	assign	What kind of assignments are accepted?
	 *	@param	after	Start looking for the attribute after this object. If NULL start at the beginning
	 */
	void GetValueStringFromParameterL(OpStringS8 &target, int parameter_id,
		Parameter_UseAssigned assign, NameValue_Splitter *after=NULL);
#endif

#ifdef FORMATS_GETVALUE16
	/** Retrieve the unicode valuestring of an argument with the given keyword ID
	 *	Calling module must import API_FORMATS_GETVALUE16
	 *
	 *	@param	target	Reference to the target string where the resulting string will be put (empty if not found)
	 *	@param	default_charset	Reference to the default charset that will be used when converting the string
	 *	@param	parameter_id	The ID of the attribute's name. If the ID is 0 (zero) the id of the after attribute will be used
	 *	@param	assign	What kind of assignments are accepted?
	 *	@param	after	Start looking for the attribute after this object. If NULL start at the beginning
	 */
	void GetValueStringFromParameterL(OpString &target, const OpStringC8 &default_charset, int parameter_id,
							 Parameter_UseAssigned assign = PARAMETER_ANY, NameValue_Splitter *after=NULL);
#endif

	NameValue_Splitter *First() const {return (NameValue_Splitter *)Keyword_Manager::First();}
	NameValue_Splitter *Last() const {return (NameValue_Splitter *)Keyword_Manager::Last();}


protected:
	/** Take over all the elements in the input sequence splitter, including its buffers */
	void TakeOver(Sequence_Splitter &);

	/** Create a duplicate of this sequence */
	Sequence_Splitter *DuplicateL() const ;

	/** Duplicate all elements of this list that matches the criteria into the argument list
	 *
	 *	@param	list			All new elements are inserted into this list
	 *	@param	hdr_name_id		Keyword ID of the elements to be copied, if 0 (zero) all elements will be copied
	 *	@param	after			Start duplication after this element in the list
	 */
	void DuplicateIntoListL(Sequence_Splitter *list, int hdr_name_id, NameValue_Splitter *after = NULL) const ;

	/** Create a new Name/value pair object, if necessary customized */
	virtual NameValue_Splitter *CreateNameValueL() const;
	/** Create a new sequence splitter, if necessary a customized class*/
	virtual Sequence_Splitter *CreateSplitterL() const;

	/** Create an allocation splitter object that will copy the given  name, value, charset and language
	 *
	 *	@param	a_Name		name of the new name/value pair
	 *	@param	a_Value		value of the new name/value pair
	 *	@param	a_Charset	the charset of the value (from RFC 2231)
	 *	@param	a_Language	the language of the value (from RFC 2231)
	 */
	virtual NameValue_Splitter *CreateAllocated_ParameterL(const OpStringC8 &a_Name, const OpStringC8 &a_Value, const OpStringC8 &a_Charset, const OpStringC8 &a_Language);

public:

	/** Duplicates the parameters in this list  */
	Sequence_Splitter *DuplicateListL() const {return DuplicateL();};
};

#endif // _SEQSPLIT_H_
