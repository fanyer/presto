/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

// ----------------------------------------------------

#ifndef UTIL_H
#define UTIL_H

// ----------------------------------------------------

#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/util/buffer.h"

#include "modules/url/uamanager/ua.h"

class OpTransferItem;
class Message;
class URL_DataDescriptor;
class OpTimer;
class OpTimerListener;

// ----------------------------------------------------

/*! A class encapsulating incremental reading of data retrieved from an URL
	that is finished loading. Can be used to avoid reading a large chunk of
	data into the memory at once (if you know, or suspect, that the URL
	contains lot of data).
*/

class BrowserUtilsURLStream
{
public:
	// Construction / destruction.
	BrowserUtilsURLStream();
	~BrowserUtilsURLStream();

	OP_STATUS Init(URL& url);

	// Methods.
	OP_STATUS NextChunk(OpString& chunk);
	BOOL HasMoreChunks() const { return m_has_more; }
	OP_STATUS NextChunk(OpByteBuffer& chunk);

private:
	// No copy or assignment.
	BrowserUtilsURLStream(const BrowserUtilsURLStream& stream);
	BrowserUtilsURLStream& operator=(const BrowserUtilsURLStream& stream);

	// Methods.
	OP_STATUS MarkURLAsGenerated(URL& url) const;

	// Members.
	OpAutoPtr<URL_DataDescriptor> m_data_descriptor;
	unsigned long m_source_len;
	BOOL m_has_more;
};

// ----------------------------------------------------

class BrowserUtils
{
public:
    virtual ~BrowserUtils() {}
	// put exported stuff here until better place is found

	// moved here from pcomm

	virtual const char* GetLocalHost() = 0;
	virtual const char* GetLocalFQDN() = 0;
    virtual OP_STATUS ResolveUrlName(const OpStringC& szNameIn, OpString& szResolved) = 0;
    virtual OP_STATUS GetURL(URL*& url, const uni_char* url_string) = 0;
	virtual OP_STATUS WriteStartOfMailUrl(URL* url, const Message* message) = 0;
	virtual OP_STATUS WriteEndOfMailUrl(URL* url, const Message* message) = 0;
	virtual OP_STATUS WriteToUrl(URL* url, const char* data, int length) const = 0;
	virtual OP_STATUS SetUrlComment(URL* url, const OpStringC& comment) = 0;

	virtual OP_STATUS CreateTimer(OpTimer** timer, OpTimerListener* listener = NULL) = 0;

#ifndef HAVE_REAL_UNI_TOLOWER
	virtual const void* GetTable(const char* table_name, long &byte_count) = 0;
#endif

	virtual OP_STATUS GetUserAgent(OpString8& useragent, UA_BaseStringId override_useragent = UA_NotOverridden) = 0;

	virtual OP_STATUS PathDirFileCombine(OUT OpString& dest, IN const OpStringC& directory, IN const OpStringC& file) const = 0;
	virtual OP_STATUS PathAddSubdir(OUT OpString& dest, IN const OpStringC& subdir) = 0;

	virtual time_t    CurrentTime() = 0;
	virtual long	  CurrentTimezone() = 0;
	virtual BOOL      OfflineMode() = 0;
	virtual BOOL	  TLSAvailable() = 0;
	virtual double    GetWallClockMS() = 0;

	virtual OP_STATUS ConvertFromIMAAAddress(const OpStringC8& imaa_address, OpString16& address, BOOL& is_mailaddress) const = 0;

	virtual OP_STATUS GetSelectedText(OUT OpString16& text) const = 0;

	virtual OP_STATUS FilenamePartFromFullFilename(const OpStringC& full_filename, OpString &filename_part) const = 0;

    virtual OP_STATUS GetContactName(const OpStringC &mail_address, OpString &full_name) = 0;
	virtual OP_STATUS GetContactImage(const OpStringC &mail_address, const char*& image) = 0;
	virtual OP_STATUS AddContact(const OpStringC& mail_address, const OpStringC& full_name, BOOL is_mailing_list = FALSE, int parent_folder_id = 0) = 0;
	virtual OP_STATUS AddContact(const OpStringC& mail_address, const OpStringC& full_name, const OpStringC& notes, BOOL is_mailing_list = FALSE, int parent_folder_id = 0) = 0;
	virtual BOOL      IsContact(const OpStringC &mail_address) = 0;
	virtual INT32     GetContactCount() = 0;
	virtual OP_STATUS GetContactByIndex(INT32 index, INT32& id, OpString& nick) = 0;

	virtual OP_STATUS PlaySound(OpString& path) = 0;
	virtual int       NumberOfCachedHeaders() = 0;

    virtual OP_STATUS ShowIndex(index_gid_t id, BOOL force_download = TRUE) = 0;

	virtual OP_STATUS SetFromUTF8(OpString* string, const char* aUTF8str, int aLength = KAll) = 0;
	virtual OP_STATUS ReplaceEscapes(OpString& string) = 0;

	// Needed for fetching RSS feeds
	virtual OP_STATUS GetTransferItem(OpTransferItem** item, const OpStringC& url_string, BOOL* already_created = 0) = 0;
	virtual OP_STATUS StartLoading(URL* url) = 0;
	virtual OP_STATUS ReadDocumentData(URL& url, BrowserUtilsURLStream& url_stream) = 0;
	virtual void      DebugStatusText(const uni_char* message) = 0;

	virtual void	  UpdateAllInputActionsButtons() = 0;


#ifdef MSWIN
	virtual OP_STATUS OpRegReadStrValue(IN HKEY hKeyRoot, IN LPCTSTR szKeyName, IN LPCTSTR szValueName, OUT LPCTSTR szValue, IO DWORD *pdwSize) = 0;

	virtual LONG	  _RegEnumKeyEx(HKEY, DWORD, LPWSTR, LPDWORD, LPDWORD, LPWSTR, LPDWORD, PFILETIME) = 0;
	virtual LONG	  _RegOpenKeyEx(HKEY, LPCWSTR, DWORD, REGSAM, PHKEY) = 0;
	virtual LONG	  _RegQueryValueEx(HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD) = 0;
	virtual LONG	  _RegCloseKey(HKEY) = 0;
	virtual	DWORD	  _ExpandEnvironmentStrings(LPCWSTR, LPWSTR, DWORD) = 0;
#endif //MSWIN

    //Dialogs requested opened from M2
    virtual void      SubscribeDialog(UINT16 account_id) = 0;
	virtual BOOL      MatchRegExp(const OpStringC& regexp, const OpStringC& string) = 0;
};

// ----------------------------------------------------

#endif // UTIL_H

// ----------------------------------------------------
