/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MODULES_GADGET_OPGADGETUPDATE_H
#define MODULES_GADGET_OPGADGETUPDATE_H
#ifdef GADGET_SUPPORT
#include "modules/gadgets/OpGadgetClassProxy.h"

//////////////////////////////////////////////////////////////////////////
// OpGadgetUpdateObject
//////////////////////////////////////////////////////////////////////////

class OpGadgetUpdateObject
	: public MessageObject
	, public Link
{
public:
	/** Creates a new gadget update object.
	  *
	  * @param new_obj [out] The address of newly created object
	  * @param mh [in] Message handler to be use for data retrieval
	  * @param klass [in] The gadget class to be updated
	  * @param update_url_str [in] The URL to use for update (provided by the gadget manifest)
	  * @param additional_params [in] OPTIONAL additional parameters to be appended to the exisiting ones.
	  *                          They should be already concatenated with ampersands, which means they
	  *                          should also be properly encoded already. May be NULL.
	  */
	static OP_GADGET_STATUS Make(OpGadgetUpdateObject*& new_obj,
								 MessageHandler* mh,
								 OpGadgetClassProxy& klass,
								 const uni_char* additonal_params);

	virtual ~OpGadgetUpdateObject();

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	OP_GADGET_STATUS HandleFinished(BOOL modified);
	void HandleError();

	OP_GADGET_STATUS SetCallbacks();
	void UnsetCallbacks();

	OpGadgetClass* GetClass() { return m_class_proxy.GetGadgetClass(); }

private:
	/** Check if an update is newer than the installed gadget class.
	 *
	 * If gadget_class has no version then it is assumed older than the update.
	 */
	BOOL IsUpdateNewer(const OpGadgetUpdateInfo* update_info, const OpGadgetClass* gadget_class);
	OpGadgetUpdateObject(MessageHandler* mh, OpGadgetClassProxy& klass);

	OP_GADGET_STATUS Construct(const uni_char* update_url_str, const uni_char* additonal_params);
	OP_GADGET_STATUS ProcessUpdateXML(BOOL modified);

	MessageHandler* m_mh;
	OpGadgetClassProxy m_class_proxy;
	URL m_update_url;
	BOOL m_finished;
};

//////////////////////////////////////////////////////////////////////////
// OpGadgetDownloadObject
//////////////////////////////////////////////////////////////////////////

class OpGadgetDownloadObject
	: public MessageObject
	, public Link
{
public:
	static OP_GADGET_STATUS Make(OpGadgetDownloadObject*& new_obj, MessageHandler* mh, const uni_char* url, const uni_char* caption, void* userdata);

	virtual ~OpGadgetDownloadObject();

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	OP_GADGET_STATUS SetCallbacks();
	void UnsetCallbacks();

	void HandleFinished();
	void HandleError();

	URL& DownloadUrl() { return m_download_url; }
	const uni_char* DownloadUrlStr() { return m_url_str.CStr(); }
	const uni_char* Caption() { return m_caption.CStr(); }
	void* UserData() { return m_userdata; }

private:
	OpGadgetDownloadObject(MessageHandler* mh, void* userdata);

	OP_GADGET_STATUS Construct(const uni_char* url, const uni_char* caption);

	MessageHandler* m_mh;
	void* m_userdata;
	BOOL m_finished;
	URL m_download_url;
	OpString m_url_str;
	OpString m_caption;
};

//////////////////////////////////////////////////////////////////////////
// OpGadgetDownloadCallbackToken
//////////////////////////////////////////////////////////////////////////

class OpGadgetDownloadCallbackToken
	: public Link
	, public OpGadgetListener::GadgetDownloadPermissionCallback
{
public:
	static OP_GADGET_STATUS Make(OpGadgetDownloadCallbackToken*& new_token, const uni_char* url, const uni_char* caption, void* userdata);
	virtual ~OpGadgetDownloadCallbackToken();

	// from OpGadgetListener::GadgetDownloadPermissionCallback
	virtual void OnGadgetDownloadPermission(BOOL allow);

private:
	OpString url;
	OpString caption;
	void* userdata;
};

//////////////////////////////////////////////////////////////////////////
// OpGadgetUpdateInfo
//////////////////////////////////////////////////////////////////////////

class OpGadgetUpdateInfo : public Link
{
public:
	typedef enum
	{
		GADGET_UPDATE_UNKNOWN,
		GADGET_UPDATE_UPDATE,
		GADGET_UPDATE_DISABLE,
		GADGET_UPDATE_ACTIVATE,
		GADGET_UPDATE_DELETE,
	} UpdateType;

	typedef enum
	{
		GADGET_HASH_UNKNOWN,
		GADGET_HASH_MD5,
		GADGET_HASH_SHA1
	} HashType;

	OpGadgetUpdateInfo(OpGadgetClassProxy& klass);
	OpGadgetUpdateInfo(OpGadgetClass* klass);
	virtual ~OpGadgetUpdateInfo();

	OP_GADGET_STATUS	SaveToFile(PrefsFile* prefsfile, const OpStringC& section, const uni_char* prefix);
	void		SaveToFileL(PrefsFile* prefsfile, const OpStringC& section, const uni_char* prefix);
	OP_GADGET_STATUS	LoadFromFile(PrefsFile* prefsfile, const OpStringC& section, const uni_char* prefix);
	void		LoadFromFileL(PrefsFile* prefsfile, const OpStringC& section, const uni_char* prefix);

	void		SetType(UpdateType type) { m_type = type; }
	OP_GADGET_STATUS	SetID(const uni_char* id) { return m_id.Set(id); }
	OP_GADGET_STATUS	SetSrc(const uni_char* src) { return m_src.Set(src); }
	OP_GADGET_STATUS	SetVersion(const uni_char* version) { return m_version.Set(version); }
	void		SetByteCount(UINT32 bytes) { m_bytes = bytes; }
	void		SetIsMandatory(BOOL mandatory) { m_mandatory = mandatory; }
	OP_GADGET_STATUS	SetDetails(const uni_char* details) { return m_details.Set(details); }
	OP_GADGET_STATUS	SetHref(const uni_char* href) { return m_href.Set(href); }
	DEPRECATED(OP_GADGET_STATUS SetHash(HashType type, const uni_char* hash));
	void		SetLastModified(time_t last_modified) { m_last_modified = last_modified; }
	void		SetExpires(time_t expires) { m_expires = expires; }

	UpdateType      GetType() const { return m_type; }
	const uni_char*	GetID() const { return m_id.CStr(); }
	const uni_char* GetSrc() const { return m_src.CStr(); }
	const uni_char* GetVersion() const { return m_version.CStr(); }
	UINT32          GetByteCount() const { return m_bytes; }
	BOOL            IsMandatory() const { return m_mandatory; }
	const uni_char* GetDetails() const { return m_details.CStr(); }
	const uni_char* GetHref() const { return m_href.CStr(); }
	DEPRECATED(HashType GetHashType() const);
	DEPRECATED(const uni_char* GetHash() const);
	time_t			GetLastModified() const { return m_last_modified; }

	/** Check if the update information is valid.
	 *
	 * According to the W3C Widget Updates specification the fields
	 * version and src are mandatory.
	 */
	BOOL IsValid()
	{
		return GetVersion() != NULL
		    && GetSrc() != NULL;
	}

	void NotifyUpdate()    { NotifyUpdate(ToGadgetListenerUpdateType(m_type)); }
	void NotifyNoUpdate()  { NotifyUpdate(OpGadgetListener::GADGET_NO_UPDATE); }

private:
	void NotifyUpdate(OpGadgetListener::GadgetUpdateType gadget_listener_update_type);
	OpGadgetListener::GadgetUpdateType ToGadgetListenerUpdateType(UpdateType update_type);

	time_t     m_last_modified;
	time_t     m_expires;

	UpdateType m_type;				//< What type of update this is (update, disable, activate, delete)
	OpString   m_id;				//< Widget id (not the same as widget uid, which is called id...), specified in config.xml
	OpString   m_src;				//< URI pointing to the updated widget.
	OpString   m_version;			//< The new version string.
	UINT32     m_bytes;				//< Size of the widget archive (or 0).
	BOOL       m_mandatory;			//< This is a mandatory update.

	OpString   m_details;			//< Detail text as returned by the update server.
	OpString   m_href;				//< URI that could point to an additional information document.

	OpGadgetClassProxy m_class;			//< The OpGadgetClass being updated.
};

inline OP_GADGET_STATUS OpGadgetUpdateInfo::SetHash(OpGadgetUpdateInfo::HashType type, const uni_char* hash) { return OpStatus::ERR; }
inline OpGadgetUpdateInfo::HashType OpGadgetUpdateInfo::GetHashType() const { return GADGET_HASH_UNKNOWN; }
inline const uni_char* OpGadgetUpdateInfo::GetHash() const { return NULL; }

#endif // GADGET_SUPPORT
#endif // !MODULES_GADGET_OPGADGETUPDATE_H
