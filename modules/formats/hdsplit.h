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

#ifndef _HEADER_SPLIT_H_
#define _HEADER_SPLIT_H_

#include "modules/formats/seqsplit.h"

#define HEADER_COPY_CONTENT NVS_COPY_CONTENT	// Allocates memory and duplicates content
#define HEADER_TAKE_CONTENT NVS_TAKE_CONTENT		// Uses buffer and Free's the memory during destruction
#define HEADER_BORROW_CONTENT NVS_BORROW_CONTENT		// Do not free content. Free'd externally

class HeaderList;
class Sequence_Splitter;
typedef Sequence_Splitter ParameterList;
typedef NameValue_Splitter HeaderEntry;

enum Header_ResolveKeyword {
	HEADER_NO_RESOLVE = KEYWORD_NO_RESOLVE, 
	HEADER_RESOLVE = KEYWORD_RESOLVE
};

/** Creates a sequence of HTTP/MIME (RFC 2616/RFC 2045) headername/value pairs based on a 
 *	CRLF separated sequence of lines
 *	This is a specialized implementation of Sequence_Splitter
 */
class HeaderList : public Sequence_Splitter
{
public:
	/** Constructor */
	HeaderList();
	/** Constructor, Use the specified prepared keyword list */
	HeaderList(Prepared_KeywordIndexes index);
	/** "Copy" constructor. Takes over (DOES NOT copy) the argument header list */
	HeaderList(HeaderList &);
	/** Use the specified list of sorted keywords to index the items
	 *	The first element in the list is the default element, the rest of the list must be 
	 *	alphabetically sorted. The total length must be at lest count elements
	 */
	HeaderList(const KeywordIndex *keys, int count);
	
	/** Take over all the elements in the input sequence splitter, including its buffers */
	void TakeOver(HeaderList &old_hdrs){Sequence_Splitter::TakeOver(old_hdrs);}
	
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
	 *	@param flags	ORed bitmask of NVS_* flags which determines the syntax of the name/value pairs. 
	 *					Only relevant flags will be forwarded to the splitter, and new flags added
	 *
	 *	@return	OP_STATUS	status of operation
	 */
	OP_STATUS SetValue(const char *value, unsigned flags=0){return Sequence_Splitter::SetValue(value, (flags & NVS_BUFFER_MASK) | NVS_SEP_CRLF_CONTINUED | NVS_VAL_SEP_COLON | NVS_ONLY_SEP);}

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
	 *					Only relevant flags will be forwarded to the splitter, and new flags added
	 */
	void SetValueL(const char *value, unsigned flags=0){Sequence_Splitter::SetValueL(value, (flags & NVS_BUFFER_MASK) | NVS_SEP_CRLF_CONTINUED | NVS_VAL_SEP_COLON | NVS_ONLY_SEP | NVS_ACCEPT_QUOTE_IN_NAME);}

	/** Retrieve a header by searcing for it by name
	 *
	 *	@param	tag		Name of the header. If NULL the name of the after attribute will be used.
	 *	@param	after	Start looking for the attribute after this object. If NULL start at the beginning
	 *	@param	resolve	Should the name be looked up in the keyword list first?
	 *
	 *	@return	HeaderEntry *	Pointer to the found object, NULL if no matching object was found
	 */
	HeaderEntry *GetHeader(const char *tag,
		HeaderEntry *after = NULL,
		Header_ResolveKeyword resolve=HEADER_NO_RESOLVE){
		return (HeaderEntry *) GetParameter(tag, after, (Parameter_ResolveKeyword) resolve);
	};	

	/** Retrieve a header by searcing for it by its integer keyword id
	 *
	 *	@param	tag		The ID of the attribute's name. If the ID is 0 (zero) the id of the after attribute will be used
	 *	@param	after	Start looking for the attribute after this object. If NULL start at the beginning
	 *
	 *	@return	HeaderEntry *	Pointer to the found object, NULL if no matching object was found
	 */
	HeaderEntry *GetHeaderByID(unsigned int tag,
		HeaderEntry *after = NULL){
		return (HeaderEntry *) GetParameterByID(tag, after);
	};	
	
	/** Create a duplicate of this sequence */
	HeaderList *DuplicateL() const {return (HeaderList *) Sequence_Splitter::DuplicateL();}

	/** Duplicate all elements of this list that matches the criteria into the argument list
	 *
	 *	@param	list			All new elements are inserted into this list
	 *	@param	hdr_name_id		Keyword ID of the elements to be copied, if 0 (zero) all elements will be copied
	 *	@param	after			Start duplication after this element in the list
	 */
	void DuplicateHeadersL(HeaderList *list, int hdr_name_id, HeaderEntry *after = NULL) const {DuplicateIntoListL(list, hdr_name_id, after);}


protected:
	
	/** Create a new sequence splitter */
	virtual Sequence_Splitter *CreateSplitterL() const;

};



#endif
