/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/
#ifndef _URL_NAME_H_
#define _URL_NAME_H_

#ifdef YNP_WORK
//#define YNP_DEBUG_NAME 
#endif

//#include "url_enum.h"
#include "modules/url/url2.h"
#include "modules/url/url_sn.h"
#include "modules/url/url_scheme.h"

struct URL_Name_Components_st
{
public:
	const protocol_selentry *scheme_spec;
	URLType		url_type;
	const char	*username;
	const char	*password;
	ServerName_Pointer server_name;
	uint16		port;
	URL_Scheme_Authority_Components_pointer basic_components;
	const char	*path;
	const char	*query;

	const char	*full_url;

public:

	/**
	 * Export escaped string representation of url into a buffer of given length.
	 *
	 * @param password_hide Only escaped urls are allowed (KName_*_Escaped*)
	 * @param output_buffer The buffer where the url will be printed
	 * @param buffer_len 	Length of buffer. The buffer should be at least of length op_strlen(full_url) + 1;
	 * @param linkid		URL_NAME_LinkID_None, URL_NAME_LinkID or URL_NAME_LinkID_Unique
	 */
	void OutputString(URL::URL_NameStringAttribute password_hide, char *output_buffer, size_t buffer_len, int linkid) const;

	/**
	 * Export escaped string representation of url into a buffer of given length.
	 *
	 * @param password_hide Only escaped urls are allowed (KUniName_*_Escaped*)
	 * @param output_buffer The buffer where the url will be printed
	 * @param buffer_len 	Length of buffer. The buffer should be at least of length op_strlen(full_url) + 1;
	 */
	void OutputString(URL::URL_NameStringAttribute password_hide, uni_char *output_buffer, size_t buffer_len) const;

	void Clear(){
		scheme_spec = NULL;
		full_url = NULL; 
		url_type = URL_NULL_TYPE; 
		username = NULL; 
		password = NULL; 
		server_name = NULL;
		port = 0;
		path = NULL;
		query = NULL;
	};
	URL_Name_Components_st(){Clear();};
};



class URL_Name
#ifdef YNP_DEBUG_NAME
: public Link
#endif
{
	friend class URL_Manager;
	friend class CacheTester;
private:

	const protocol_selentry* GetProtocolSelentry() const;

	URL_Scheme_Authority_Components_pointer basic_components;
	OpString16			illegal_original_url;
	OpString8			path;
	OpString8			query;

#ifdef URL_FILE_SYMLINKS
	// Stores the target path if this URL is a symlink, i.e. it reads
	// data from this target path instead of the original path.
	// See the URL KUniName_For_Data, KUniPath_For_Data and
	// KUniPath_SymbolicLinkTarget attributes for more info.
	OpString8			symbolic_link_target_path;
#endif // URL_FILE_SYMLINKS

private:

	enum { TEMPBUFFER_SHRINK_LIMIT = (MEM_MAN_TMP_BUF_LEN + 255) & ~255 };

	OP_STATUS		CheckBufferSize(unsigned long templen) const;

#define URL_NAME_LinkID_None 0
#define URL_NAME_LinkID 1
#define URL_NAME_LinkID_Unique 2


public:

					URL_Name();
					~URL_Name();

	OP_STATUS		Set_Name(URL_Name_Components_st &url);


	/** Content from Name must always be copied immediately, as a temporary buffer is used.
	 *  The content of this buffer will be destroyed on next call to Name(), or on the next call to the constructor
	 */
	const char*		Name(URL::URL_NameStringAttribute password_hide, URL_RelRep* rel_rep = NULL, int linkid=URL_NAME_LinkID_None) const;

	/** Content from UniName must always be copied immediately, as a temporary buffer is used.
	 *  The content of this buffer will be destroyed on next call to Name(), or on the next call to the constructor
	 */
	const uni_char*	UniName(URL::URL_UniNameStringAttribute password_hide, unsigned short charsetID, URL_RelRep* rel_rep = NULL) const;

	OP_STATUS		GetSuggestedFileName(OpString &target, BOOL only_extension = FALSE) const;

	uint32 GetAttribute(URL::URL_Uint32Attribute attr) const;
	const OpStringC8 GetAttribute(URL::URL_StringAttribute attr, URL_RelRep* rel_rep = NULL) const;
	const OpStringC GetAttribute(URL::URL_UniStringAttribute attr, URL_RelRep* rel_rep = NULL) const;
	OP_STATUS		GetAttribute(URL::URL_StringAttribute attr, OpString8 &val, URL_RelRep* rel_rep = NULL) const;
	OP_STATUS		GetAttribute(URL::URL_UniStringAttribute attr, OpString &val, URL_RelRep* rel_rep = NULL) const;
	const OpStringC8 GetAttribute(URL::URL_NameStringAttribute attr, URL_RelRep* rel_rep = NULL) const;
	const OpStringC GetAttribute(URL::URL_UniNameStringAttribute attr, URL_RelRep* rel_rep = NULL) const;
	OP_STATUS		GetAttribute(URL::URL_NameStringAttribute attr, OpString8 &val, URL_RelRep* rel_rep = NULL) const{return val.Set(GetAttribute(attr,rel_rep));};
	OP_STATUS		GetAttribute(URL::URL_UniNameStringAttribute attr, unsigned short charsetID, OpString &val, URL_RelRep* rel_rep = NULL) const;
	const void *GetAttribute(URL::URL_VoidPAttribute  attr) const;

	void SetAttribute(URL::URL_Uint32Attribute attr, uint32 value);
	OP_STATUS SetAttribute(URL::URL_UniNameStringAttribute attr, const OpStringC &string);
	//void SetAttributeL(URL::URL_StringAttribute attr, const OpStringC8 &string);
	//void SetAttributeL(URL::URL_UniStringAttribute attr, const OpStringC &string);
	//void SetAttributeL(URL::URL_VoidPAttribute  attr, const void *param);

	/** Some internal buffers can grow quite big. Call this to make them small again. */
	static void ShrinkTempBuffers();

#ifdef _OPERA_DEBUG_DOC_
	unsigned long	GetMemUsed();
#endif

	BOOL operator==(const URL_Name &other) const;
};
#endif //_URL_NAME_H_
