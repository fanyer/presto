/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#ifndef _KEYWORD_MAN_H_
#define _KEYWORD_MAN_H_

#include "modules/util/simset.h"
#include "modules/util/str.h"
#include "modules/auth/auth_enum.h"

enum Keyword_ResolvePolicy {
	KEYWORD_NO_RESOLVE,
	KEYWORD_RESOLVE
};

/** The keyword IDs of recognized HTTP and MIME Header names */
enum HTTP_Header_Tags {
	HTTP_Header_Unknown = 0,
	HTTP_Header_Accept_Ranges,
	HTTP_Header_Age,
#ifdef HTTP_DIGEST_AUTH
	HTTP_Header_Authentication_Info,
#endif
	HTTP_Header_Boundary,
	HTTP_Header_Cache_Control,
	HTTP_Header_Connection,
	HTTP_Header_Content_Disposition,
	HTTP_Header_Content_Encoding,
	HTTP_Header_Content_ID,
	HTTP_Header_Content_Language,
	HTTP_Header_Content_Length,
	HTTP_Header_Content_Location,
	//HTTP_Header_Content_MD5,
	HTTP_Header_Content_Range,
	HTTP_Header_Content_Transfer_Encoding,
	HTTP_Header_Content_Type,
	HTTP_Header_Date,
	HTTP_Header_ETag,
	HTTP_Header_Expires,
	HTTP_Header_Keep_Alive,
	HTTP_Header_Last_Modified,
	HTTP_Header_Link,
	HTTP_Header_Default_Style,
	HTTP_Header_Location,
	HTTP_Header_Pragma,
	HTTP_Header_Powered_By,
	HTTP_Header_Proxy_Authenticate,
#ifdef HTTP_DIGEST_AUTH
	HTTP_Header_Proxy_Authentication_Info,
#endif
	HTTP_Header_Proxy_Connection,
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	HTTP_Header_Proxy_Support,
#endif
	HTTP_Header_Refresh,
	HTTP_Header_Retry_After,
	HTTP_Header_Server,
	HTTP_Header_Set_Cookie,
	HTTP_Header_Set_Cookie2,
#ifdef LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY
	HTTP_Header_Strict_Transport_security,
#endif
	HTTP_Header_Trailer,
	HTTP_Header_Transfer_Encoding,
	HTTP_Header_Version,
	//HTTP_Header_Via,
	//HTTP_Header_Warning,
	HTTP_Header_WWW_Authenticate,
	HTTP_Header_X_Frame_Options,
	HTTP_Header_X_Opera_Authenticate,
	HTTP_Header_X_Opera_ProxyConfig,
	HTTP_Header_X_Powered_By
};

#ifdef _ENABLE_AUTHENTICATE
/** The keyword IDs of recognized HTTP authentication attribute names */
enum HTTP_Authentication_Tags {
	HTTP_Authentication_Unknown,
	// Methods, hardcoded to the authscheme codes
	HTTP_Authentication_Method_Basic = AUTH_SCHEME_HTTP_BASIC,
#ifdef HTTP_DIGEST_AUTH
	HTTP_Authentication_Method_Digest=AUTH_SCHEME_HTTP_DIGEST,
#endif
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	HTTP_Authentication_Method_Negotiate = AUTH_SCHEME_HTTP_NEGOTIATE,
	HTTP_Authentication_Method_NTLM = AUTH_SCHEME_HTTP_NTLM,
#endif

	HTTP_Authentication_argument_start, // Dynamic
	// Parameters
	HTTP_Authentication_Realm
#ifdef HTTP_DIGEST_AUTH
	, HTTP_Authentication_Stale,
	HTTP_Authentication_Algorithm,
	HTTP_Authentication_Method_Auth_Int,
	HTTP_Authentication_Method_Auth,
	HTTP_Authentication_Cnonce,
	HTTP_Authentication_Domain,
	HTTP_Authentication_NC,
	HTTP_Authentication_Nextnonce,
	HTTP_Authentication_Nonce,
	HTTP_Authentication_Nonce_Count,
	HTTP_Authentication_Qop,
	HTTP_Authentication_Opaque,
	HTTP_Authentication_Rspauth
#endif
};
#endif

/** The keyword IDs of recognized HTTP cookie attribute names */
enum HTTP_Cookie_Tags {
	HTTP_Cookie_Unknown = 0,
	HTTP_Cookie_Comment,
	HTTP_Cookie_CommentURL,
	HTTP_Cookie_Discard,
	HTTP_Cookie_Domain,
	HTTP_Cookie_Expires,
	HTTP_Cookie_Max_Age,
	HTTP_Cookie_Path,
	HTTP_Cookie_Port,
	HTTP_Cookie_Secure,
	HTTP_Cookie_Version
};

/** The keyword IDs of recognized general attribute names */
enum HTTP_General_Tags {
	HTTP_General_Tag_Unknown = 0,
	HTTP_General_Tag_Access_Type,
	HTTP_General_Tag_Attachment,
	HTTP_General_Tag_Base64,
	HTTP_General_Tag_Boundary,
	HTTP_General_Tag_Bytes,
	HTTP_General_Tag_Charset,
	HTTP_General_Tag_Chunked,
	HTTP_General_Tag_Close,
	HTTP_General_Tag_Compress,
	HTTP_General_Tag_Deflate,
	HTTP_General_Tag_Delsp,
	HTTP_General_Tag_Directory,
	HTTP_General_Tag_Filename,
	HTTP_General_Tag_Format,
	HTTP_General_Tag_FWD,
	HTTP_General_Tag_Gzip,
	HTTP_General_Tag_Identity,
#ifdef LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY
	HTTP_General_Include_SubDomains,
#endif
	HTTP_General_Tag_Inline,
	HTTP_General_Tag_Keep_Alive,
	HTTP_General_Tag_Max,
	HTTP_General_Tag_Max_Age,
#if defined(_SUPPORT_SMIME_) || defined(_SUPPORT_OPENPGP_)
	HTTP_General_Tag_Micalg,
	HTTP_General_Tag_Protocol,
#endif
	HTTP_General_Tag_Must_Revalidate,
#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && !defined __OEM_OPERATOR_CACHE_MANAGEMENT
	HTTP_General_Tag_Never_Flush,
#endif
	HTTP_General_Tag_No_Cache,
	HTTP_General_Tag_No_Store,
	HTTP_General_Tag_None,
	HTTP_General_Tag_Private,
	HTTP_General_Tag_Name,
	HTTP_General_Tag_Site,
	HTTP_General_Tag_Start,
	HTTP_General_Tag_URL
};

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
/** The keyword IDs of recognized Automatic Proxy Configuration keywords */
enum APC_Keyword_Type
{
	APC_Keyword_Type_None = 0,
	APC_Keyword_Type_Direct,
	APC_Keyword_Type_Proxy
#ifdef SOCKS_SUPPORT
	, APC_Keyword_Type_Socks
#endif
};
#endif

/** The enumlist of predefined keyword lists
 *	MUST match the numbers of corresponding
 *	items in Keyword_Index_List array
 */
enum Prepared_KeywordIndexes {
	/** No list is selected */
	KeywordIndex_None = 0,
	/** The HTTP and MIME header keyword list */
	KeywordIndex_HTTP_MIME ,
	/** The general HTTP and MIME attribute keyword list */
	KeywordIndex_HTTP_General_Parameters
#ifdef _ENABLE_AUTHENTICATE
	/** The authentication keyword list */
	, KeywordIndex_Authentication
#endif
	/** The cookie keyword list */
	, KeywordIndex_Cookies
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	/** The Automatic Proxy Configuration Keyword list */
	, KeywordIndex_Automatic_Proxy_Configuration
#endif
};



/** This object contains the index of the keyword in the subclass */
class KeywordIndexed_Item : public Link
{
private:
	/** The keyword index number */
	unsigned int keyword_index;

protected:
	/** Constructor */
	KeywordIndexed_Item();

	/** Destructor */
	virtual ~KeywordIndexed_Item();

public:

	/** Set the Name ID */
	void SetNameID(unsigned int val){keyword_index = val;};

	/** Get the Name ID */
	unsigned int GetNameID() const{return keyword_index;}
};


/** The keyword manager that will assign integer indexes to each element in the list
 *	The keyword indexes can be obtained either by their index in a sorted list of char pointers, or
 *	a sorted keyword index list (where the first item contains the default index value)
 */
class Keyword_Manager : public AutoDeleteHead
{
private:
	/** @deprecated Alphabethically sorted list of pointers to keywords  */
	//const char* const *keywords1; // Must be alphabetically sorted
	/** List of keywords. The first item is the default element, and the rest must be alphabetically sorted */
	const KeywordIndex *keywords2; // Must be alphabetically sorted
	/** length of keyword list */
	unsigned int keyword_count;

public:
	/** Constructor */
	Keyword_Manager();

	/** Constructor, selects the given enumed keyword list */
	Keyword_Manager(Prepared_KeywordIndexes index);

	/** Copy constructor, only copies the keyword list, not the elements */
	Keyword_Manager(const Keyword_Manager &old_keys);

	/* @deprecated Constructor, use the provided alphapbtically sorted list of strings as a keyword list */
	//DEPRECATED(Keyword_Manager(const char* const *keys, unsigned int count));

	/** Constructor, use the provided keyword to index list as a keyword list. The first
	 *	element is the default index number, the remainding elements MUST be alphabetically sorted
	 */
	Keyword_Manager(const KeywordIndex *keys, unsigned int count);

	/** Selects the given enumed keyword list */
	void SetKeywordList(Prepared_KeywordIndexes index);

	/** Copies the keyword list of the old_keys list */
	void SetKeywordList(const Keyword_Manager &old_keys);

	/* @deprecated Use the provided alphapbtically sorted list of strings as a keyword list */
	//void DEPRECATED(SetKeywordList(const char* const *keys, unsigned int count));

	/** Use the provided keyword to index list as a keyword list. The first
	 *	element is the default index number, the remainding elements MUST be alphabetically sorted
	 */
	void SetKeywordList(const KeywordIndex *keys, unsigned int count);

	/** Iterate through the list, and update the keyword index for all the elements */
	void UpdateListIndexes();

	/** Retrieve the keyword index of the given item based on its LinkID and update the item's index ID */
	void UpdateIndexID(KeywordIndexed_Item *);

	/** Get the keyword ID from the current keyword list for the given name */
	unsigned int GetKeywordID(const OpStringC8 &name) const;


	/** Retrieve an indexed item by searcing for it by name
	 *
	 *	@param	tag		Name of the attribute, If NULL the name of the after attribute will be used.
	 *	@param	after	Start looking for the attribute after this object. If NULL start at the beginning
	 *	@param	resolve	Should the name be looked up in the keyword list first?
	 *
	 *	@return	KeywordIndexed_Item *	Pointer to the found object, NULL if no matching object was found
	 */
	KeywordIndexed_Item *GetItem(const char *tag, KeywordIndexed_Item *after = NULL, Keyword_ResolvePolicy resolve = KEYWORD_NO_RESOLVE);

	/** Retrieve an indexed item by searcing for it by its integer keyword id
	*
	*	@param	tag_id	The ID of the attribute's name. If the ID is 0 (zero) the id of the after attribute will be used
	*	@param	after	Start looking for the attribute after this object. If NULL start at the beginning
	*
	*	@return	KeywordIndexed_Item *	Pointer to the found object, NULL if no matching object was found
	*/
	KeywordIndexed_Item *GetItemByID(unsigned int tag_id, const KeywordIndexed_Item *after= NULL) const;
};


#endif // _KEYWORD_MAN_H_
