/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef WAND_MANAGER_H
#define WAND_MANAGER_H

#ifdef WAND_SUPPORT

#include "modules/util/adt/opvector.h"
#include "modules/util/simset.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/util/opfile/opfile.h"

#include "modules/wand/wandsecuritywrapper.h"
#include "modules/wand/wandenums.h"
#include "modules/wand/suspendedoperation.h"
#include "modules/libcrypto/include/CryptoMasterPasswordHandler.h"

#ifdef SYNC_HAVE_PASSWORD_MANAGER
#include "modules/sync/sync_coordinator.h"
#include "modules/sync/sync_encryption_key.h"
#include "modules/wand/wand_sync_item.h"
#include "modules/wand/wand_sync_data_client.h"
#endif // SYNC_HAVE_PASSWORD_MANAGER

class FramesDocument;
class WandPage;
class WandProfile;
class WandManager;
class HTML_Element;
class OpWindowCommander;
class ES_Thread;
class OpFile;
class URL;
class OpWindow;
class FormValue;
class CryptoBlobEncryption;

#define WAND_HAS_MATCH_IN_STOREDPAGE 0x01
#define WAND_HAS_MATCH_IN_ECOMMERCE 0x02

BOOL HasPreparedSSLForCrypto();
OP_STATUS StoreWandInfo();
/**
 * Store a submit in progress, while waiting for the user to respond
 * to a policy question for instance. It saves quite some pointers to
 * things that better not go away while waiting for the user to
 * answer.
 */
class WandInfo : public Link
{
	friend class WandManager;
public:

	/**
	 * Constructor. Creates a WandInfo object.
	 *
	 * @param[in] wand_manager The wand manager. g_wand_manager except
	 * in selftests. This is the wand manager that will get the store
	 * and submit calls.
	 *
	 * @param[in] page The page that we might want to save in the
	 * database. It will be owned by the WandInfo object.
	 *
	 * @param[in] window The window that will be used in case
	 * someone needs to show dialogs for instance.
	 */
	WandInfo(WandManager* wand_manager,
			 WandPage* page, Window* window);
	virtual ~WandInfo();

	/**
	 * Will perform the action, submit the page and delete itself.
	 *
	 * @param[in] action The default action for fields on the page.
	 *
	 * @param[in] eCommerce_action How to treat eCommerce fields. This
	 * parameter only exists when build with WAND_ECOMMERCE_SUPPORT.
	 */
	void ReportAction(WAND_ACTION action
#ifdef WAND_ECOMMERCE_SUPPORT
		, WAND_ECOMMERCE_ACTION eCommerce_action = WAND_DONT_STORE_ECOMMERCE_VALUES
#endif // WAND_ECOMMERCE_SUPPORT
		);

	/**
	 * Returns the WindowCommander for the window with the document
	 * triggering the submit or NULL if no such object could be found.
	 *
	 * @returns NULL or a pointer to an OpWindowCommander.
	 */
	OpWindowCommander * GetOpWindowCommander();
public:
	WandPage* page;
private:
	void UnreferenceWindow(Window* window);

	Window*	m_window;
	WandManager* wand_manager;
};


/**
 * Sometimes a page gives several possible matches and we have to
 * present the user with the choice of which to use. This object
 * stores the relevant information for that choice.
 */
class WandMatchInfo : public Link
{
	friend class WandManager;
public:
	/**
	 * Constructor.
	 *
	 * @param[in] doc The document triggering the choice.
	 *
	 * @param[in] profile The profile that we found matches in.
	 */
	WandMatchInfo(FramesDocument* doc, WandProfile* profile);

	/**
	 * Destructor. Virtual since it inherits the virtual destructor in
	 * Link.
	 */
	virtual ~WandMatchInfo();

//#ifdef WAND_FUNCTIONS_NOT_MADE_ASYNC_YET
	/**
	 * To be called by the UI when the user has selected the one
	 * entry. This will delete the WandMatchInfo object. If the
	 * user cancelled the choice, Cancel() must be called.
	 *
	 * @param[in] index The selected entry.
	 * @param[in] submit Wether or not to submit the form.
	 *
	 * @see Cancel()
	 */
	void Select(INT32 index, BOOL3 submit = MAYBE);
//#endif // WAND_FUNCTIONS_NOT_MADE_ASYNC_YET

	/**
	 * To be called if the user will not make a choice and instead
	 * cancels the dialog. This will delete the WandMatchInfo object.
	 *
	 * <p>If the user makes a choice, Select should be called instead.
	 *
	 * @see Select()
	 */
	void Cancel();

	/**
	 * It's possible for the user to delete entries from the
	 * dialog. To do so, call this method and update the displayed
	 * list.
	 *
	 * @param[in] index The entry to delete.
	 */
	BOOL Delete(INT32 index);

	/**
	 * @returns The number of choices for the user.
	 */
	INT32 GetMatchCount() { return m_matches.GetCount(); }

	/**
	 * The title of the choice. Typically the user name.
	 *
	 * @param[in] index The entry.
	 */
	const uni_char* GetMatchString(INT32 index);

	/**
	 * Get the windowcommander of the document that this wand match relates to.
	 *
	 * @returns windowcommander of the document or NULL if there
	 * was no document
	 */
	OpWindowCommander* GetOpWindowCommander() const;

private:

	/**
	 * Continues the import of data from wand and deletes the
	 * WandMatchInfo object.
	 *
	 * This requires security to be setup.
	 *
	 * @param[in] index The selected entry.
	 * @param[in] submit Wether the form should be auto-submitted or not.
	 *
	 * @returns An error code if the import failed. Regardless the
	 * object will be deleted.
	 */
	OP_STATUS ProcessMatchSelectionAndDelete(INT32 index, BOOL3 submit = MAYBE);

	FramesDocument* m_doc;
	WandProfile* m_profile;

	struct WandMatch {
		OpString string;
		WandPage* page;
		INT32 index;
	};
	OpVector<WandMatch> m_matches;
};


/**
 * Listeners. Should probably be changed into 2 different classes or more.
 */
class WandListener
{
public:
	virtual ~WandListener() {}

	/**
	 * Called on submit of a page. Use to show for instance "Use wand
	 * to store formsinfo?".  When the user has done the choice you
	 * must call info.ReportAction to let the submit continue!
	 *
	 * <p>This will be called on all listeners so only *one* of
	 * wandmanagers listeners should have an implementation of this.
	 *
	 * @param[in] info The callback object. Exactly one of the
	 * listeners _must_ call info.ReportAction() or info.Cancel().
	 *
	 * @returns The status of the listener. OpStatus::ERR_NO_MEMORY if
	 * no memory, OpStatus::ERR_NOT_SUPPORTED if the listener doesn't
	 * handle this callback and OpStatus::OK if it took care of the
	 * event.
	 */
	virtual OP_STATUS OnSubmit(WandInfo* info) { return OpStatus::ERR_NOT_SUPPORTED; }

	/**
	 * Called when the wand is used and the page has been stored more
	 * than one time with different info.  A dialog which let the user
	 * choose which one to use should be shown. Call Select, Cancel or
	 * Delete.
	 *
	 * This will be called on all listeners so only *one* of
	 * wandmanagers listeners should have an implementation of this.
	*/
	virtual OP_STATUS OnSelectMatch(WandMatchInfo* info) { return OpStatus::OK; }

	/**
	 * Tell a listener that a login was added.
	 */
	virtual void OnWandLoginAdded(int index) {}

	/**
	 * Tell a listener that a login was removed.
	 */
	virtual void OnWandLoginRemoved(int index) {}

	/**
	 * Tell a listener that a page was added.
	 */
	virtual void OnWandPageAdded(int index) {}

	/**
	 * Tell a listener that a page was removed.
	 */
	virtual void OnWandPageRemoved(int index) {}

	/**
	 Tells a listener how a security state change operation ended.
	 * @param successful TRUE if a requested change was successfully
	 * completed or if the operation didn't require any change at all.
	 * FALSE if it wasn't completed, for instance because
	 * a user failed to a supply a master password.
	 *
	 * @param changed TRUE if an encryption/decryption
	 * of the database was performed.
	 *
	 * @param is_now_security_encrypted TRUE if the wand database is now encrypted
	 * with the master password. FASLE if it's now merely obfuscated.
	 * If successful was FALSE, then this is the state the database
	 * had before the requested change, and still has afterwards.
	 */
	virtual void OnSecurityStateChange(BOOL successful, BOOL changed, BOOL is_now_security_encrypted) {}
};

/**
 * Storage for a password. It will be encrypted in memory almost all
 * the time. Either with an obfuscation password or encrypted with the
 * master password.
 */
class WandPassword
{
public:
	WandPassword() : password(NULL), password_len(0), password_encryption_mode(NO_PASSWORD) {}
	~WandPassword();

	OP_STATUS	Save(OpFile &file);
	OP_STATUS	Open(OpFile &file, long version);

	OP_STATUS	Set(const uni_char* password, BOOL add_encrypted);

	OP_STATUS	GetPassword(OpString& str) const;
	OP_STATUS	ReEncryptPassword(const ObfuscatedPassword* pass, const ObfuscatedPassword* newpass);
	/**
	 * If the password is already correctly encrypted, this is a nop, otherwise
	 * it will encrypt with the master password. This must be called before
	 * storing the password for real in the wand database (memory and disk).
	 */
	OP_STATUS	Encrypt();
	OP_STATUS	Decrypt(OpString& str) const;

	// stuff for the changepassword code..
	OP_STATUS	Set(const unsigned char* data, UINT16 length);
	void		Switch(WandPassword* pwd);
	const unsigned char*	GetDirect() const { return password; }
	UINT16					GetLength() const { return password_len; }
	OP_STATUS	CopyTo(WandPassword* clone);
	BOOL		IsEncrypted() { return password_encryption_mode == PASSWORD_ENCRYPTED; }
private:

	OP_STATUS 	SetObfuscated(const uni_char* in_password);
	OP_STATUS 	DeObfuscate(OpString &plain_password) const;

	UINT8* password;
	int password_len;

	enum PasswordEncryptionMode
	{
		PASSWORD_TEMPORARY_OBFUSCATED,
		PASSWORD_ENCRYPTED,
		NO_PASSWORD
	} password_encryption_mode;
};

/**
 * Either OnPasswordRetrieved() or OnPasswordRetrievalFailed() will be called.
 */
class WandLoginCallback
{
public:
	virtual void OnPasswordRetrieved(const uni_char* password) = 0;
	virtual void OnPasswordRetrievalFailed() = 0;
	virtual ~WandLoginCallback() {}
};

/**
 * Storage for login-info used in authentication and password-dialogs
 * or any other resouce. This is used in the feature that allows
 * anyone to store such information in wand.
 */
class WandLogin
#ifdef SYNC_HAVE_PASSWORD_MANAGER
: public WandSyncItem
#endif // SYNC_HAVE_PASSWORD_MANAGER

{
public:
	WandLogin(){}
	virtual ~WandLogin(){}

	OP_STATUS	Set(const uni_char* id, const uni_char* username, const uni_char* password, BOOL encrypt_password);
	OP_STATUS	Save(OpFile &file);
	OP_STATUS	Open(OpFile &file, long version);
	/**
	 * Requires security to be enabled before being called. Will encrypt the password in case it
	 * wasn't already encrypted.
	 */
	OP_STATUS	EncryptPassword();

	/**
	 * Requires security to be enabled before being called. Password  in this object needs to be decrypted to
	 * be able to compare
	 */
	BOOL		IsEqualTo(WandLogin *login);
	/**
	 * Creates a copy of this WandLogin object or returns NULL if there wasn't enough memory.
	 */
	WandLogin*	Clone();

#ifdef SYNC_HAVE_PASSWORD_MANAGER
	virtual OP_STATUS 	InitAuthTypeSpecificElements(OpSyncItem *item, BOOL modify = FALSE);
	virtual OP_STATUS 	ConstructSyncItemAuthTypeSpecificElements(OpSyncItem *sync_item);
	virtual OpSyncDataItem::DataItemType GetAuthType() { return OpSyncDataItem::DATAITEM_PM_HTTP_AUTH; }
	virtual BOOL		PreventSync();
#endif // SYNC_HAVE_PASSWORD_MANAGER

public:
	OpString id, username;
	WandPassword password;
};

/**
 * The info for each stored formobject (Stores only name of the object
 * and the value)
 */
class WandObjectInfo
{
public:
	WandObjectInfo() : is_guessed_username (FALSE), is_password(FALSE), is_textfield_for_sure(FALSE), is_changed(FALSE) {}
	~WandObjectInfo();

	/**
	 * @param add_encrypted If TRUE encrypt any
	 * password for real (assumes that master password is already input or not
	 * needed). If FALSE, only obfuscate any password.
	 */
	OP_STATUS Init(const uni_char* name, const uni_char* value, BOOL is_password, BOOL is_textfield, BOOL is_changed, BOOL add_encrypted);

	OP_STATUS	Save(OpFile &file);
	OP_STATUS	Open(OpFile &file, long version);

public:
	OpString name;			///< Name of formobject
	OpString value;			///< Value if not password
	BOOL is_guessed_username; ///< This form field is probably the username
	BOOL is_password;		///< Is password. Use .password instead of .value.
	BOOL is_textfield_for_sure;	///< Is a textfield (if FALSE, may still be a textfield from an old database)
	BOOL is_changed;		///< Value was different than the defaultvalue when storing.
	WandPassword password;	///< Value if password
};

/**
 * Handles storing of formsfields on a webpage (or webpages on the
 * server)
 */
class WandPage
#ifdef SYNC_HAVE_PASSWORD_MANAGER
: public WandSyncItem
#endif // SYNC_HAVE_PASSWORD_MANAGER
{
	friend class WandManager;
	friend class WandProfile;
public:
	WandPage() : formnr(0), flags(0), offset_x(0), offset_y(0), document_x(0), document_y(0) {}
	virtual ~WandPage();

#ifdef SYNC_HAVE_PASSWORD_MANAGER
	/* Implementation of WandSyncItem */
	OP_STATUS 			InitAuthTypeSpecificElements(OpSyncItem *item, BOOL modify = FALSE);
	OP_STATUS			ConstructSyncItemAuthTypeSpecificElements(OpSyncItem *sync_item);
	OpSyncDataItem::DataItemType GetAuthType() { return OpSyncDataItem::DATAITEM_PM_FORM_AUTH; }
	virtual BOOL PreventSync() { return FALSE; }
#endif // SYNC_HAVE_PASSWORD_MANAGER

	OP_STATUS			Save(OpFile &file);
	OP_STATUS			Open(OpFile &file, long version);

	OP_STATUS			CollectDataFromDocument(FramesDocument* doc, HTML_Element* he_submit, HTML_Element* he_form,
		long offset_x, long offset_y, long document_x, long document_y)
	{
		return CollectFromDocumentInternal(doc, he_submit, he_form, offset_x, offset_y, document_x, document_y, TRUE);
	}

	/**
	 * Saves the data non encrypted (since encryption isn't available) for later encryption.
	 */
	OP_STATUS			CollectPlainTextDataFromDocument(FramesDocument* doc, HTML_Element* he_submit, HTML_Element* he_form,
								long offset_x, long offset_y, long document_x, long document_y)
	{
		return CollectFromDocumentInternal(doc, he_submit, he_form, offset_x, offset_y, document_x, document_y, FALSE);
	}
	/**
	 * Fetches the data found in the wand database and inserts it into
	 * the page.
	 *
	 * @param[in] doc The document to use wand on.
	 *
	 * @param[in] helm The html element to fill wand data in - or NULL if whole
	 *            document should be used.
	 *
	 * @param[out] num_matching The number of form controls that got a
	 * value from wand.
	 *
	 * @param[in] only_if_matching_username Only fetch the data if a
	 * "username" found in the form matches what is stored in
	 * wand. See Use() for further documentation.
	 */
	OP_STATUS			Fetch(FramesDocument* doc, HTML_Element* helm, int& num_matching, BOOL only_if_matching_username);

	INT32				CountMatches(FramesDocument* doc);

	/**
	 * Searches this WandPage for a WandObjectInfo that matches
	 * the element he. This is slow (O(n) n = document size) so be nice.
	 *
	 * @param doc The document that owns he.
	 * @param he The element to find a match for.
	 * @param[inout] he_form Optional pointer to he's form. Providing it will make the
	 * operation faster. If a form for |he| was found in the process and the form wasn't
	 * previously set, this reference variable will be updated.
	 */
	WandObjectInfo*		FindMatch(FramesDocument* doc, HTML_Element* he, HTML_Element*& he_form);

	/**
	 * Only compare name. To be used for the global WandPage that has no url (but has ECommerce info)
	 */
	WandObjectInfo*		FindNameMatch(const uni_char* name);
	BOOL				HasECommerceMatch(HTML_Element* he);

private:
	/**
	 * @param add_encrypted In case of a password, should it be stored
	 * encrypted or just obfuscated? Encryption might still mean merely
	 * "obfuscated" depending on user settings.
	 */
	OP_STATUS			AddObjectInfo(const uni_char* name, const uni_char* value, BOOL is_password, BOOL is_textfield, BOOL is_changed, BOOL add_encrypted);
public:
#ifdef SELFTEST
	OP_STATUS			SelftestConstruct(const uni_char* url, const uni_char* topdoc_url, const uni_char *url_action, const uni_char* submitname, UINT32 formnr, UINT32 flags);
	OP_STATUS			SelftestAddObjectInfo(const uni_char* name, const uni_char* value, BOOL is_password, BOOL is_textfield, BOOL is_changed, BOOL add_encrypted = FALSE)
	{
		return AddObjectInfo(name, value, is_password, is_textfield, is_changed, add_encrypted);
	}
#endif // SELFTEST

#ifdef WAND_ECOMMERCE_SUPPORT
	OP_STATUS			AddEComObjectInfo(const uni_char* name, const uni_char* value, BOOL is_changed)
	{
		return AddObjectInfo(name, value, FALSE, TRUE, is_changed, FALSE);
	}
#endif // WAND_ECOMMERCE_SUPPORT

	INT32				CountObjects()				{ return objects.GetCount(); }
	WandObjectInfo*		GetObject(INT32 nr)			{ return (WandObjectInfo*) objects.Get(nr); }

	OP_STATUS			SetUrl(FramesDocument* doc, HTML_Element* he_form);
	const OpString&		GetUrl() { return url; }

	BOOL				IsMatching(FramesDocument* doc);
	BOOL				IsMatching(WandPage* new_page);

	BOOL				GetNeverOnThisPage();
	BOOL				GetOnThisServer();
	INT32				CountPasswords();
	BOOL				IsSameFormAndUsername(WandPage* page, FramesDocument* doc);

	/**
	 * Replaces the wand data in this page with other_page.
	 *
	 * Note that when syncing, the sync data is not replaced.
	 *
	 * @param other_page Replace with this page
	 * @return OK or ERR_NO_MEMORY
	 */
	OP_STATUS			ReplaceWithOtherPage(WandPage* other_page);
	/** To be used when we were not able to initially encrypt the passwords. Such a page can only be stored a short time and seperate from the rest */
	OP_STATUS			EncryptPasswords();

	/**
	 * @deprecated See FindUserName(const uni_char*& user_name)
	 */
	DEPRECATED(const uni_char*		 FindUserName());

	/**
	 * Return values from the FindUserName function.
	 */
	enum FindUserNameResult
	{
		FIND_USER_OK,
		FIND_USER_ERR_NOTHING_STORED_ON_SERVER,
		FIND_USER_ERR_NOTHING_STORED_ON_PAGE,
		FIND_USER_ERR_NO_USERNAME
	};

	/**
	 * Returns the username on the page. This is a "best guess", and might not be
	 * correct. Providing a FramesDocument will improve the accuracy.
	 *
	 * @param[out] result_user_name Will return a pointer to the user name data or NULL.
	 *
	 * @param[in] doc A document with a form that matches this WandPage. This is used to
	 * refined the result from FindUserName so that it doesn't return names of
	 * checkboxes or such.
	 *
	 * @param result If there was a user_name, then FIND_USER_OK, otherwise an
	 * error code. user_name may still be NULL potentially if it was empty.
	 */
	FindUserNameResult	FindUserName(const uni_char*& result_user_name, FramesDocument* doc = NULL);

	WandPassword*		FindPassword();

	/**
	 * If the passwords are already correctly encrypted, this
	 * is bascially a nop, otherwise
	 * it will encrypt them with the master password. This
	 * must be called before
	 * storing the page for real in the wand database (memory and disk).
	 */
	OP_STATUS	Encrypt();

#ifdef WAND_ECOMMERCE_SUPPORT
	/**
	 * Checks if there are things on this page that doesn't match
	 * what's stored in the personal profile.
	 *
	 * This function is O(n*m) where n is the number of objects
	 * in the current page and m is the number of stored objects
	 * in the personal profile.
	 *
	 * @return TRUE if there are something that might hava changed.
	 */
	BOOL 				HasChangedECommerceData();
	/**
	 * Fetches a stored ECommerce value from the personal profile (regardless
	 * of which page this is.
	 *
	 * @param name The form field's name.
	 * @param out_val The stored value.
	 *
	 * @returns OpStatus::OK if a value was found, else an error.
	 */
	OP_STATUS			GetStoredECommerceValue(const OpString& name, OpString& out_val);
#endif // WAND_ECOMMERCE_SUPPORT

	HTML_Element*		FindTargetSubmit(FramesDocument* doc);
	HTML_Element*		FindTargetForm(FramesDocument* doc);

	/**
	 * Only for use from WandInfo::ReportAction, and only once.
	 */
	void				SetUserChoiceFlags(UINT32 new_flags);
private:
	/**
	 * Saves the data non encrypted (since encryption isn't available) for later encryption.
	 */
	OP_STATUS			CollectFromDocumentInternal(FramesDocument* doc, HTML_Element* he_submit, HTML_Element* he_form,
								long offset_x, long offset_y, long document_x, long document_y, BOOL encrypted);

	/**
	 * Returns the WandObjectInfo most likely to represent the username field.
	 * This is a "best guess", and might not be correct. Providing a FramesDocument will improve 
	 * the accuracy.
	 *
	 * @param[in] doc A document with a form that matches this WandPage. This is used to
	 * refine the result so that it doesn't return names of checkboxes or such.
	 *
	 * @param result If there was a username field, the WandObjectInfo associated with it, 
	 * NULL otherwise.
	 */
	WandObjectInfo* FindAndMarkUserNameFieldCandidate(FramesDocument* doc = NULL);

	/**
	 * @param[in] doc May be NULL.
	 */
	BOOL MightBeUserName(FramesDocument* doc, WandObjectInfo* object_info);
	void Clear();

	OpVector<WandObjectInfo> objects;
	OpString url;
	OpString m_topdoc_url;
	OpString url_action;
	OpString submitname;
	UINT32 formnr;
	UINT32 flags;
	long offset_x, offset_y;
	long document_x, document_y;
};

enum WAND_TYPE {
	WAND_LOG_PROFILE = 0,		///< It's a profile that contains all the stored pages
	WAND_PERSONAL_PROFILE = 1,	///< It's a profile that contains userinfo and should match to any page

	WAND_TYPE_HIGH_LIMIT
};

/**
 * Wand data is divided into profiles, so that we can have several
 * sets of data. Normally a user has two sets, custom saved data and
 * the personal data.
 */
class WandProfile
#ifdef SYNC_HAVE_PASSWORD_MANAGER
	: public WandSyncDataClient
#endif // SYNC_HAVE_PASSWORD_MANAGER
{
public:
	WandProfile(WandManager* wand_manager, WAND_TYPE type=WAND_PERSONAL_PROFILE)
		: m_wand_manager(wand_manager), type(type)
	{OP_ASSERT(wand_manager);}

	virtual ~WandProfile();

	WAND_TYPE			GetType() { return type; }

	OP_STATUS			Save(OpFile &file);
	OP_STATUS			Open(OpFile &file, long version);

	/**
	 * Takes ownership of the submitted page. May be deleted if this method returns an error code.
	 */
	OP_STATUS			Store(WandPage* submitted_page);

#ifdef WAND_ECOMMERCE_SUPPORT
	/**
	 * Store all eCommerce fields in this profile which must be the
	 * personal profile.
	 *
	 * @param submitted_page The WandPage with the
	 * ecommerce form fields (and other stuff).
	 *
	 * @returns A status code telling how the operation succeeded.
	 */
	OP_STATUS			StoreECommerce(WandPage* submitted_page);
#endif // WAND_ECOMMERCE_SUPPORT

	WandObjectInfo*		FindMatch(FramesDocument* doc, HTML_Element* he);
	WandPage*			FindPage(WandPage* new_page, INT32 index = 0);
	WandPage*			FindPage(FramesDocument* doc, INT32 index = 0);
	void				DeletePage(FramesDocument* doc, INT32 index);
	void				DeleteAllPages();

	OP_STATUS			SetName(const uni_char* newname) { return name.Set(newname); }
	const OpString&		GetName() { return name; }

	WandPage*			GetWandPage(INT32 index) { return pages.Get(index); }
	UINT32				GetWandPageCount() const { return pages.GetCount(); }
	void				DeletePage(int index);

#ifdef SYNC_HAVE_PASSWORD_MANAGER
	virtual OP_STATUS 		SyncDataFlush(OpSyncDataItem::DataItemType item_type, BOOL first_sync, BOOL is_dirty);

	virtual WandSyncItem* 	FindWandSyncItem(const uni_char* sync_id, int &index);
	virtual WandSyncItem* 	FindWandSyncItemWithSameUser(WandSyncItem *item, int &index);
	virtual void 		 	DeleteWandSyncItem(int index);
	virtual OP_STATUS 		StoreWandSyncItem(WandSyncItem *item);
	virtual void	 		SyncWandItemsSyncStatuses();
#endif // SYNC_HAVE_PASSWORD_MANAGER

private:
	friend class WandManager;
	WandManager*		m_wand_manager;
	WandPage* 			CreatePage(FramesDocument* doc, HTML_Element* he_form);
	OpVector<WandPage> pages;
	OpString name;
	WAND_TYPE type;
};

#ifdef WAND_EXTERNAL_STORAGE

class WandDataStorageEntry
{
public:
	~WandDataStorageEntry();
	OP_STATUS Set(const uni_char* username, const uni_char* password);
public:
	OpString username;
	OpString password;
};

class WandDataStorage
{
public:
	virtual ~WandDataStorage() {}

	/**
	 * Store the entrydata. The data should be associated with the
	 * serverstring so it can be found with FetchData.  The
	 * serverstring is either a url, or a mix of servername:port
	 * depending of the type of login that is stored.
	 */
	virtual void StoreData(const uni_char* server, WandDataStorageEntry* entry) = 0;

	/**
	 * Return TRUE and set the info in entry if there was a match.
	 * index is which match to return if there is several.
	 */
	virtual BOOL FetchData(const uni_char* server, WandDataStorageEntry* entry, INT32 index) = 0;

	/** Delete data from external storage (if exact match is found) */
	virtual OP_STATUS DeleteData(const uni_char* server, const uni_char* username) = 0;
};

#endif // WAND_EXTERNAL_STORAGE

/**
 * The one and only (global) wand manager (except if used through selftests)
 */
class WandManager
	: private MessageObject, private OpPrefsListener,
	  public CryptoMasterPasswordHandler::MasterPasswordRetrivedCallback
#ifdef SYNC_HAVE_PASSWORD_MANAGER
	, public WandSyncDataClient
	, private SyncEncryptionKeyManager::EncryptionKeyListener
#endif // SYNC_HAVE_PASSWORD_MANAGER
{
public:
	WandManager();
	virtual ~WandManager();

	/** SubmitPage should be called when subitting a page */
	OP_STATUS				SubmitPage(FramesDocument* doc,
									   HTML_Element* he_submit,
									   HTML_Element* he_form,
							           int offset_x, long offset_y,
							           int document_x, long document_y,
							           ShiftKeyState modifiers,
							           BOOL handle_directly = FALSE,
							           ES_Thread *interrupt_thread = NULL,
									   BOOL synthetic_submit_event = FALSE);

	/**
	 * Use should be called when Wand should be used, normally
	 * triggered by the user activating the wand feature. It
	 * will try to find a form to fill in and possibly submit.
	 *
	 * @param doc The document to use wand on.
	 *
	 * @param submit If a filled in form should be submitted
	 * as well. MAYBE = Use the user's preference. YES = Always
	 * submit if possible, not recommended. NO = Don't submit.
	 *
	 * @param only_if_matching_username If set to TRUE, only fill out
	 * the form if the current username in the form matches what is
	 * stored in the wand database. When going back in history and
	 * calling Use, this flag prevents overwriting a form where the
	 * user edited the username to something else than was stored in
	 * wand.
	 * NOTE: Pending operations, and several WandPage for one URL,
	 * do not yet support this flag.
	 *
	 * @returns OpStatus::OK if the action was performed
	 * successfully or had nothing to do, OpStatus::ERR_NO_MEMORY if OOM.
	 */
	OP_STATUS				Use(FramesDocument* doc, BOOL3 submit = MAYBE, BOOL only_if_matching_username = FALSE) { return InsertWandDataInDocument(doc, NULL, submit, only_if_matching_username); }
	/**
	 * InsertWandDataInDocument should be called when Wand should be used, normally
	 * triggered by the user activating the wand feature. 
	 *
	 * @param doc The document to use wand on.
	 * @param he  The html element to fill wand data in - or NULL if whole
	 * document should be used.
	 * NOTE: If operation can't be done synchronously (e.g. due to security
	 * reasons) and is suspended, he parameter is ignored and inserting
	 * wand data is applied to whole document.
	 *
	 * @param submit If a filled in form should be submitted
	 * as well. MAYBE = Use the user's preference. YES = Always
	 * submit if possible, not recommended. NO = Don't submit.
	 *
	 * @param only_if_matching_username If set to TRUE, only fill out
	 * the form if the current username in the form matches what is
	 * stored in the wand database. When going back in history and
	 * calling Use, this flag prevents overwriting a form where the
	 * user edited the username to something else than was stored in
	 * wand.
	 * NOTE: Pending operations, and several WandPage for one URL,
	 * do not yet support this flag.
	 *
	 * @returns OpStatus::OK if the action was performed
	 * successfully or had nothing to do, OpStatus::ERR_NO_MEMORY if OOM.
	 */
	OP_STATUS				InsertWandDataInDocument(FramesDocument* doc, HTML_Element* he, BOOL3 submit = MAYBE, BOOL only_if_matching_username = FALSE);

	/**
	 * If Use has been called on this document and eventual
	 * asyncronous UI hasn't been answered yet.
	 */
	BOOL					IsUsing(FramesDocument* doc);

	/**
	 * If SubmitPage has been called on this document and eventual
	 * asyncronous UI hasn't been answered yet.
	 */
	BOOL					IsWandOperationInProgress(Window* window);
	/**
	 * Call if doc is being removed some way, so eventual asyncronous
	 * UI will be safely removed.
	 */
	void					UnreferenceDocument(FramesDocument* doc);

	/**
	 * Call if Window is being removed some way, so eventual
	 * asyncronous UI will be safely removed.
	 */
	void					UnreferenceWindow(Window* window);

	/**
	 * Tells if Wand can be applied to a document
	 */
	BOOL					Usable(FramesDocument* doc,
								   BOOL unused = FALSE);

	/**
	 * Adds a listener. Typically a UI listener for showing the UI to
	 * allow or disallow wand operations. The listener will not be
	 * owned by the WandManager.
	 */
	void					AddListener(WandListener* listener);

	/**
	 * Remove a previously added listener.
	 */
	void					RemoveListener(WandListener* listener);

	/**
	 * Activates or deactivates the wand feature. WandManager always
	 * starts in a disabled state.
	 *
	 * @param[in] active TRUE to activate, FALSE to disable.
	 */
	void				SetActive(BOOL active) { is_active = active; }

	OP_STATUS			Save(OpFile &file);

	/**
	 * Reads the contents of a wand database into wand. Will
	 * return OpStatus::ERR_YIELD if now was not a good time,
	 * but it will in that case try again very soon.
	 *
	 * @param[in] during_startup TRUE if this is during
	 * startup. Used to determine if it's possible to
	 * popup a dialog right now.
	 */
	OP_STATUS			Open(OpFile &file, BOOL during_startup, BOOL store_converted_file = FALSE);

#ifdef WAND_EXTERNAL_STORAGE
	OP_STATUS			UseExternalStorage(FramesDocument* doc, INT32 index);
	OP_STATUS			DeleteFromExternalStorage(FramesDocument* doc, const uni_char* id);
	void				SetDataStorage(WandDataStorage* data_storage) { this->data_storage = data_storage; }
	void				UpdateMatchingFormsFromExternalStorage(FramesDocument* doc);
#endif

	/**
	 * Clears all wand data, used by Delete Private Data dialog
	 *
	 * @param include_opera_logins In the wand login database we store
	 * logins with an id that has the prefix "opera:". it's possible
	 * to keep those even if we delete the rest by setting this
	 * parameter to FALSE. Those are typically mail passwords and such
	 * in the desktop client.
	 * @param also_clear_passwords_on_server	If TRUE and password sync is on
	 * 											the passwords will also be cleared
	 * 											on the server.
	 * @param stop_password_sync				If TRUE the password sync preference will be
	 * 											turned off.
	 * @param reset_sync_state					If TRUE reset_sync_state will be set to 0.
	 * 											Next time, sync is run, all passwords will
	 * 											be downloaded again.
	 */
#ifdef SYNC_HAVE_PASSWORD_MANAGER
	OP_STATUS				ClearAll(BOOL include_opera_logins = TRUE, BOOL also_clear_passwords_on_server = FALSE, BOOL stop_password_sync = TRUE, BOOL reset_sync_state = TRUE );
#else
	OP_STATUS				ClearAll(BOOL include_opera_logins = TRUE);
#endif // SYNC_HAVE_PASSWORD_MANAGER
	OP_STATUS				CreateProfile();

	void					SetCurrentProfile(INT32 profile)	{ curr_profile = profile; }
	INT32					GetCurrentProfile()					{ return curr_profile; }

	/**
	 * Returns the number of custom profiles, normally 1 (the personal
	 * info profile). The logged profile isn't included in the count.
	 */
	INT32					CountProfiles()						{ return profiles.GetCount(); }

	/**
	 * Retrieves a custom profile (typically the personal info
	 * profile). You can not access the logged profile this way. For
	 * that, check GetWandPage() and GetWandPageCount()
	 *
	 * @param[in] nr The index of the profile. Less than what
	 * CountProfiles() returned.
	 */
	WandProfile*			GetProfile(INT32 nr)				{ return (WandProfile*) profiles.Get(nr); }

	/**
	 * Stores the changed formobjects in doc in the log profile. Takes
	 * ownership over page, which might be deleted when the function
	 * returns. This is done regardless of return value.
	 */
	OP_STATUS				Store(Window* window, WandPage* page);

	/**
	 * Fetches the stored values if the page has been stored. If
	 * not, it tries with the current personal profile.
	 * If it's a typical User+Password form, it can also submit.
	 *
	 * @param The document.
	 *
	 * @param he  The html element to fill wand data in - or NULL if whole
	 * document should be used. Restricting wand operation to only one html
	 * element is for performance reasons when we know that other elements
	 * are going to be fetched later (or will remain unchanged).
	 *
	 * @param page The wand page to enter into the document.
	 *
	 * @param submit If YES, submits directly, NO don't submit,
	 * MAYBE, submit if the user hasn't blocked it with a pref.
	 *
	 * @param[in] only_if_matching_username Only fetch the data if a
	 * "username" found in the form matches what is stored in
	 * wand. See Use() for further documentation.
	 */
	OP_STATUS				Fetch(FramesDocument* doc, HTML_Element* he, WandPage* page, BOOL3 submit, BOOL only_if_matching_username);

	/**
	 *	Returns 0 if there was no match for he, or
	 *	WAND_HAS_MATCH_IN_STOREDPAGE or WAND_HAS_MATCH_IN_ECOMMERCE.
	 */
	int						HasMatch(FramesDocument* doc, HTML_Element* he);

	/**
	 * Stores a username/password in the wand database. The action might not be immediate in case we have to
	 * ask the user for a master password and if the user fails to enter the master password the
	 * save will fail silently.
	 *
	 * @param window The Window that is the source of the operation. Used among other things to show
	 * dialogs on top of the right tab.
	 */
	OP_STATUS				StoreLogin(Window* window, const uni_char* id, const uni_char* username, const uni_char* password) { OP_NEW_DBG("WandManager::StoreLogin()", "wand"); OP_ASSERT(window); return StoreLoginCommon(window, id, username, password); }

	/**
	 * For use when there is no Window object responsible for the StoreLogin.
	 *
	 * @see StoreLogin.
	 */
	OP_STATUS				StoreLoginWithoutWindow(const uni_char* id, const uni_char* username, const uni_char* password) { OP_NEW_DBG("WandManager::StoreLoginWithoutWindow()", "wand"); return StoreLoginCommon(NULL, id, username, password); }

	void					DeleteLogin(const uni_char* id, const uni_char* username);
	void					DeleteLogin(int index);
	/**
	 * Some logins have special prefixes, or at least there is one
	 * special prefix, "opera:...". That is used for logins not directly
	 * connected to web pages, but rather for mail, feed or irc servers.
	 *
	 * @returns An error if the updated wand database couldn't be stored. The login will
	 * still be deleted so a subsequent successful store will delete it permanently.
	 */
	OP_STATUS				DeleteLoginsWithIdPrefix(const char* id_prefix);
	WandLogin*				FindLogin(const uni_char* id, INT32 index, BOOL include_external_data = TRUE);

	WandLogin*				FindLogin(const uni_char* id, const uni_char *username);
	WandLogin*				GetWandLogin(int index);

	/**
	 * The callback will be called with the password either directly (while this method is called) or after a while.
	 * If the password couldn't be accessed, callback->OnPasswordRetrievalFailed() will be called.
	 *
	 * @param[in] window The window that will act as parent to any dialogs (master password dialog that is).
	 *
	 * @param callback The callback. It must be a live object until the callback is called. There is no
	 * way to unregister a callback once this method has returned.
	 *
	 * @returns An error code if the process couldn't be initiated, otherwise OpStatus::OK. If OpStatus::OK is returned,
	 * the callback is guaranteed to be called.
	 */
	OP_STATUS	GetLoginPassword(Window* window, WandLogin* wand_login, WandLoginCallback* callback) { OP_ASSERT(window); return GetLoginPasswordInternal(window, wand_login, callback); }

	/**
	 * See GetLoginPassword. This is used when there is no Window available and it will cause any
	 * dialogs to be positioned randomly or not at all.
	 */
	OP_STATUS	GetLoginPasswordWithoutWindow(WandLogin* wand_login, WandLoginCallback* callback) {return GetLoginPasswordInternal(NULL, wand_login, callback); }
	/**
	 * Checks the amount of data in the database.
	 *
	 * @returns The number of pages in the logged profile (the one
	 * with data from the web.
	 */
	UINT32					GetWandPageCount();

	/**
	 * Get the WandPage.
	 *
	 * @param[in] index, less than what is returned from GetWandPageCount().
	 *
	 * @returns The page object. Not NULL.
	 */
	WandPage*				GetWandPage(int index);
	void					DeleteWandPage(int index);

	/**
	 * Deletes any wandpages stored for the given document.
	 */
	void					DeleteWandPages(FramesDocument* doc);

	/**
	 * Re-encrypt data in the database.
	 */
	void					ReEncryptDatabase(const ObfuscatedPassword* old_password, const ObfuscatedPassword* new_password);

	/**
	 * Will either encrypt all data with the security/master password or
	 * decrypt all data, all depending on the current database state and
	 * the preference. Might also do nothing if everything is right already.
	 *
	 * WandListener::OnSecurityStateChange will be called with information
	 * of how the operation completed. It can be called synchronously or
	 * asynchronously.
	 *
	 * @param[in] parent_window The Window that will form the base for
	 * the operation. This window will be
	 * the parent of any dialogs that the user has to be shown, and the
	 * operation will abort if that window is closed before the database
	 * is re-encrypted.
	 */
	void					UpdateSecurityState(Window* parent_window) { UpdateSecurityStateInternal(parent_window, FALSE); }

	/**
	 * Will either encrypt all data with the security/master password or
	 * decrypt all data, all depending on the current database state and
	 * the preference. Might also do nothing if everything is right already.
	 *
	 * @see UpdateSecurityState
	 */
	void					UpdateSecurityStateWithoutWindow() { UpdateSecurityStateInternal(NULL, FALSE); }

	/**
	 * Checks if the database is encrypted for real with a master password
	 * or simply obscurified.
	 *
	 * @returns TRUE if it's encrypted for real.
	 */
	BOOL					GetIsSecurityEncrypted() { return is_security_encrypted; }

	// stuff for the changepassword code..
	const WandPassword*		RetrieveDataItem(int index);

//#ifdef WAND_FUNCTIONS_NOT_MADE_ASYNC_YET
	BOOL					PreliminaryStoreDataItem(int index, const unsigned char* data, UINT16 length);
//#endif WAND_FUNCTIONS_NOT_MADE_ASYNC_YET
	void					CommitPreliminaryDataItems();
	void					DestroyPreliminaryDataItems();

	/* From OpPrefsListener */
	virtual void 			PrefChanged(enum OpPrefsCollection::Collections id, int pref, int newvalue);

#ifdef WAND_ECOMMERCE_SUPPORT
	/**
	 * Help function to check if a name if one of the
	 * predefined names in the eCommerce specification.
	 *
	 * @param name The name to check.
	 *
	 * @returns TRUE if the name was one of the specified names.
	 */
	static BOOL				IsECommerceName(const uni_char* name);

	static BOOL				CanImportValueFromPrefs(HTML_Element* he, const uni_char* field_name);

	/**
	 * Make user info data into ECommerce data.
	 */
	static OP_STATUS		ImportValueFromPrefs(HTML_Element* he, const uni_char* field_name, OpString& str);
#endif // WAND_ECOMMERCE_SUPPORT

#ifdef SYNC_HAVE_PASSWORD_MANAGER
	/* Encryption key for link synchronization of wand.
	 * Must be exactly 16 bytes (128 bit AES key)
	 *
	 * @param key 16 bytes long encryption key
	 * @return OpStatus::ERR_NO_MEMORY or OpStatus::OK
	 */
	OP_STATUS 				SetSyncPasswordEncryptionKey(const UINT8 *key);
	const UINT8*			GetSyncPasswordEncryptionKey() const { return m_key; }
	BOOL					HasSyncPasswordEncryptionKey() const { return m_key != 0; }

	OP_STATUS 				InitSync();
	BOOL 		 			GetBlockOutgoingSyncMessages() { return m_sync_block; }

	/* From WandSyncDataClient */
	virtual OP_STATUS 		SyncDataFlush(OpSyncDataItem::DataItemType item_type, BOOL first_sync, BOOL is_dirty);
	virtual WandSyncItem* 	FindWandSyncItem(const uni_char* sync_id, int &index);
	virtual WandSyncItem* 	FindWandSyncItemWithSameUser(WandSyncItem *item, int &index);
	virtual void 		 	DeleteWandSyncItem(int index);
	virtual OP_STATUS 		StoreWandSyncItem(WandSyncItem *item);

	void 					SyncWandItemsSyncStatuses();

#ifdef SELFTEST
	WandProfile &SelftestGetLogProfile() { return log_profile; }
	OP_STATUS  	SelftestMaybeStorePageInDatabase(Window* window, WandPage* page) { return MaybeStorePageInDatabase(window, page); }
#endif // #SELFTEST

	/**
	 * Used to prevent sync messages back to the server while receiving sync messages from the server.
	 */
	class SyncBlocker
	{
	public:
		SyncBlocker() : m_prev_state(g_wand_manager->m_sync_block) { if (g_wand_manager) g_wand_manager->m_sync_block = TRUE; }
		~SyncBlocker() { ResetBlocker(); }
		void ResetBlocker(){ if (g_wand_manager) g_wand_manager->m_sync_block = m_prev_state; }
	private:
		BOOL m_prev_state;
	};

private:

	/**
	 * @name Implementation of SyncEncryptionKeyManager::EncryptionKeyListener
	 *
	 * This instance registers itself on initialisation InitSync() as
	 * SyncEncryptionKeyManager::EncryptionKeyListener. The
	 * SyncEncryptionKeyManager instance informs this instance when an
	 * encryption-key is available or deleted.
	 *
	 * WandSyncItem::ConstructSyncItem() will not generate a sync-item if there is no key
	 * available.
	 *
	 * @{
	 */
	virtual void OnEncryptionKey(const SyncEncryptionKey& key);
	virtual void OnEncryptionKeyDeleted();
	void 		 SyncPasswordItemSyncStatuses();
	/** @} */ // Implementation of SyncEncryptionKeyManager::EncryptionKeyListener

#endif // SYNC_HAVE_PASSWORD_MANAGER

private:

	OpFile		m_source_database_file;

	OpAutoVector<SuspendedWandOperation> m_suspended_operations;

	OpAutoVector<WandProfile> profiles;		///< Personal profiles	(contains one page with common matches)
	WandProfile log_profile;			///< Log profile		(contains all URL specific pages)
	OpAutoVector<WandLogin> logins;			///< Logins.			(contains logins for authentication-dialogs)
	INT32 curr_profile;

#ifdef SYNC_HAVE_PASSWORD_MANAGER
	BOOL m_sync_block;
	UINT8 *m_key;
#endif // SYNC_HAVE_PASSWORD_MANAGER

	OpAutoVector<WandListener> listeners;
	BOOL is_active;
	BOOL is_security_encrypted;
//	INT32 num_matching;

	Head wand_infos;
	Head wand_match_infos;

#ifdef WAND_EXTERNAL_STORAGE
	WandDataStorage* data_storage;
	WandLogin tmp_wand_login;
#endif

	friend class WandInfo;
	friend class WandMatchInfo;
	friend class WandPage;
	friend class WandProfile;
	friend class WandPassword;
	friend class WandSyncDataClient;
	friend class SyncBlocker;

	void OnWandLoginAdded(int index);
	void OnWandLoginRemoved(int index, WandLogin *removed_login);
	void OnWandPageAdded(int index);
	void OnWandPageRemoved(int index, WandPage *removed_page);


	void OnMasterPasswordRetrieved(CryptoMasterPasswordHandler::PasswordState state);
	void RunSuspendedOperation();

	/**
	 * From the MessageObject interface.
	 */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	OP_STATUS SetSuspendedPageOperation(SuspendedWandOperation::WandOperation type,
			Window* window,
			FramesDocument* doc,
			BOOL3 bool3,
			WandPage* wand_page,
			BOOL owns_wand_page);
	OP_STATUS SetSuspendedLoginOperation(SuspendedWandOperation::WandOperation type,
			Window* window,
			FramesDocument* doc,
			BOOL3 bool3,
			WandLogin* wand_login,
			WandLoginCallback* callback);
	OP_STATUS SetSuspendedInfoOperation(SuspendedWandOperation::WandOperation type,
			Window* window,
			FramesDocument* doc,
			BOOL3 bool3,
			WandInfo* wand_info,
			WAND_ACTION generic_action, int int2);
	OP_STATUS SetSuspendedOperation(SuspendedWandOperation::WandOperation type,
			Window* window,
			FramesDocument* doc,
			BOOL3 bool3);
#ifdef SYNC_HAVE_PASSWORD_MANAGER
	OP_STATUS SetSuspendedSyncOperation(SuspendedWandOperation::WandOperation type,
	        BOOL first_sync = FALSE,
	        BOOL dirty_sync = FALSE);
#endif // SYNC_HAVE_PASSWORD_MANAGER
	/**
	 * Takes ownership of the page and if suitable, and after asking the UI, stores the page
	 * in the database.
	 *
	 * <b> For wand internal use only. </b>
	 *
	 * @param window The window related to the submit.
	 *
	 * @param page The wand page that might be suitable to store.
	 */
	OP_STATUS MaybeStorePageInDatabase(Window* window, WandPage* page);


	/**
	 * <b> For wand internal use only. </b>
	 */
	void ResumeSubmitPage(Window* window, WandPage* plain_text_wand_page);

	/**
	 * <b> For wand internal use only. </b>
	 *
	 * Do not access login after using this method as it might be deleted
	 */
	OP_STATUS StoreLoginInternal(Window* window, WandLogin* login);

	/**
	 * <b> For wand internal use only. </b>
	 */
	OP_STATUS	GetLoginPasswordInternal(Window* window, WandLogin* wand_login, WandLoginCallback* callback);

	/**
	 * <b> For wand internal use only. </b> Between StoreLogin and StoreLoginInternal
	 */
	OP_STATUS StoreLoginCommon(Window* window, const uni_char* _id, const uni_char* username, const uni_char* password);

	/**
	 * Removes a login and sends remove event
	 * @param index	The index in login list
	 */
	void RemoveLogin(int index);

	int	FindLoginInternal(const uni_char* id, INT32 index, BOOL include_external_data);

	/**
	 * Will either encrypt all data with the security/master password or
	 * decrypt all data, all depending on the current database state and
	 * the preference. Might also do nothing if everything is right already.
	 *
	 * @param[in] possible_parent_window If it exists, the Window that will form the base for
	 * the operation. This window will be
	 * the parent of any dialogs that the user has to be shown, and the
	 * operation will abort if that window is closed before the database
	 * is re-encrypted.
	 *
	 * @param silent_about_nops If TRUE, the listener won't be called if there was nothing to
	 * do. this is used for "check if something has changed" calls so that the listener isn't
	 * spammaed with "nothing has changed" calls.
	 */
	void	UpdateSecurityStateInternal(Window* possible_parent_window, BOOL silent_about_nops);

	/**
	 * Like FindLogin but removes the login from the internal arrays.
	 */
	WandLogin*	ExtractLogin(const uni_char* id, INT32 index, BOOL include_external_data = TRUE);

	/**
	 * Helper function to put a string value into a form control.
	 *
	 * @returns TRUE if the form control received the value, FALSE otherwise.
	 */
	static BOOL SetFormValue(FramesDocument* doc, HTML_Element* he, const uni_char* value);
};

#endif // WAND_SUPPORT

#endif // WAND_MANAGER_H
