/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve N. Pettersen
 */

#ifndef _URL_DOC_TESTMAN_H_
#define _URL_DOC_TESTMAN_H_

#ifdef SELFTEST

#include "modules/url/url2.h"
#include "modules/url/url_docload.h"
#include "modules/locale/locale-enum.h"

// If required, SELFTEST_USERNAME should be defined by the compiler
#ifdef SELFTEST_USERNAME
#define URL_NAME "~" SELFTEST_USERNAME "/"
#define FTP_NAME SELFTEST_USERNAME "/"
#else
#define URL_NAME "netcore/"
#define FTP_NAME "netcore/"
#endif

#define URLSelftestBaseHost(host, path) "http://" host "/" URL_NAME "selftests/urltests/" path
#define FTP_URLSelftestBaseHost(host, path) "ftp://" host "/" FTP_NAME "selftests/urltests/" path

#define URLSelftestBase(path) URLSelftestBaseHost("netcore3.oslo.osa", path)
#define URLSpdy2SelftestBase(path) URLSelftestBaseHost("netcore3.oslo.osa:82", path)
#define FTP_URLSelftestBase(path) FTP_URLSelftestBaseHost("netcore3.oslo.osa", path)

#define URLSelftestBaseIP(path) URLSelftestBaseHost("10.20.11.156", path)
#define FTP_URLSelftestBaseIP(path) FTP_URLSelftestBaseHost("10.20.11.156", path)

/** Events for the URL selftests */
enum URL_DocSelfTest_Event {
	URLDocST_Event_Unknown,
	URLDocST_Event_None,
	URLDocST_Event_Header_Loaded,
	URLDocST_Event_Data_Received,
	URLDocST_Event_Redirect,
	URLDocST_Event_LoadingFailed,
	URLDocST_Event_LoadingFinished,
	URLDocST_Event_LoadingRestarted,
	URLDocST_Event_Item_Finished,
	URLDocST_Event_Item_Failed,
	URLDocST_Event_Batch_Finished,
	URLDocST_Event_Batch_Failed
};

class URL_DocSelfTest_Item;
class URL_DocSelfTest_Batch;
class URL_DocSelfTest_Manager;

BOOL CompareFileAndURL(URL_DocSelfTest_Item *reporter, URL &url, const OpStringC& test_file);
BOOL CompareURLAndURL(URL_DocSelfTest_Item *reporter, URL &url, URL& test_source);

/**	Each item contains one loading URL and its referrer
 *	and can handle individual events from the URL
 *	Implementations may add more actions, or different actions.
 *
 *	Procedure:
 *		Create individual testcases, and add them to a batch
 *		Batches are then added to a manager
 *		When all batches are configured the sequence can be started.
 */
class URL_DocSelfTest_Item : public ListElement<URL_DocSelfTest_Item>, public URL_DocumentHandler, public MessageObject
{
private:
	friend class URL_DocSelfTest_Batch;
	/** Owner of this item */
	URL_DocSelfTest_Batch *owner;

	BOOL had_action;

public:
	/** The loading URL */
	URL url;
	URL_InUse url_use;
	/** The referrer */
	URL referer;

public:
	URL_DocSelfTest_Item();
	/** Constructor. Takes a URL to load and the referrer. DO NOT use Construct() when using this */
	URL_DocSelfTest_Item(URL &a_url, URL &a_referer, BOOL unique=TRUE);
	
	/** Destructor */
	virtual ~URL_DocSelfTest_Item();

	/** Construct the item. Only used of the empty constructor was used */
	void Construct(URL &a_url, URL &ref, BOOL unique= TRUE);

	URL_DocSelfTest_Batch *GetBatch(){return owner;}

	/** Handle callbacks */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/** Verify function. called for each event and with a status code for error messages
	 *	Must be implemented by subclass.
	 *	Verified MUST call ST_failed() if the test fails
	 *	Implementations MUST NOT call ST_passed()
	 *	Return TRUE if success.
	 */
	virtual BOOL Verify_function(URL_DocSelfTest_Event event, Str::LocaleString status_code=Str::NOT_A_STRING);

	void ReportFailure(const char *format, ...);

	virtual BOOL OnURLRedirected(URL &url, URL &redirected_to);
	virtual BOOL OnURLHeaderLoaded(URL &url);
	virtual BOOL OnURLDataLoaded(URL &url, BOOL finished, OpAutoPtr<URL_DataDescriptor> &stored_desc);
	virtual BOOL OnURLRestartSuggested(URL &url, BOOL &restart_processing, OpAutoPtr<URL_DataDescriptor> &stored_desc);
	virtual BOOL OnURLNewBodyPart(URL &url, OpAutoPtr<URL_DataDescriptor> &stored_desc);
	virtual void OnURLLoadingFailed(URL &url, Str::LocaleString error, OpAutoPtr<URL_DataDescriptor> &stored_desc);
	virtual void OnURLLoadingStopped(URL &url, OpAutoPtr<URL_DataDescriptor> &stored_desc);
};

/** A batch of testcases
 *	Can handle individual events from the testcases
 *	Implementations may add more actions, or different actions.
 *
 *	Procedure:
 *		Create individual testcases, and add them to a batch
 *		Batches are then added to a manager
 *		When all batches are configured the sequence can be started.
 */
class URL_DocSelfTest_Batch : public ListElement<URL_DocSelfTest_Batch>, public URL_DocumentLoader
{
private:
	friend class URL_DocSelfTest_Manager;
	/** The manager of this batch */
	URL_DocSelfTest_Manager *manager;
	/** The list of loading testcases */
	AutoDeleteList<URL_DocSelfTest_Item> loading_items;
	/** The list of completed testcases */
	AutoDeleteList<URL_DocSelfTest_Item> loaded_items;

	int call_count;

	OpString8 batch_id;

	BOOL ignore_load_failures;

public:

	/** Constructor */
	URL_DocSelfTest_Batch();
	/** Destructor */
	virtual ~URL_DocSelfTest_Batch();

	void Construct(MessageHandler *mh);

	/** Set the batch ID */
	void SetBatchID(const OpStringC8 &id){OpStatus::Ignore(batch_id.Set(id));}

	const OpStringC8 &GetBatchID(){return batch_id;}

	URL_DocSelfTest_Manager *GetManager(){return manager;}
	void SetManager(URL_DocSelfTest_Manager *man){manager=man;}

	/** Verify function. called for each event and with a status code for error messages
	 *	Must be implemented by subclass.
	 *	Verified MUST call ST_failed() if the test fails
	 *	Implementations MUST NOT call ST_passed()
	 *	Return TRUE if success.
	 */
	virtual BOOL Verify_function(URL_DocSelfTest_Event event, URL_DocSelfTest_Item *source);

	/** Start Loading the batch. Implementations may override. Return TRUE if success */
	virtual BOOL StartLoading();
	/** Stop Loading the batch. Implementations may override. Return TRUE if success */
	virtual BOOL StopLoading();
	/** Is this batch finished? */
	virtual BOOL Finished();

	virtual BOOL Empty(){return loading_items.Empty() && loaded_items.Empty();}
	virtual int Cardinal(){return loading_items.Cardinal() + loaded_items.Cardinal();}

	void ReportFailure(const char *format, ...);
	void ReportTheFailure(const char *report);

	/** Add a new testcase item */
	BOOL AddTestCase(URL_DocSelfTest_Item *);

	void SetIgnoreLoadFailures(BOOL flag){ignore_load_failures = flag;}
};

/** A sequence of testbatches
 *	Only one batch loaded at a time;
 *	Can handle individual events from the batches
 *	Implementations may add more actions, or different actions.
 *
 *	Procedure:
 *		Create individual testcases, and add them to a batch
 *		Batches are then added to a manager
 *		When all batches are configured the sequence can be started.
 */
class URL_DocSelfTest_Manager : public MessageObject
{
private:
	AutoDeleteList<URL_DocSelfTest_Batch> batch_items;
	AutoDeleteList<URL_DocSelfTest_Batch> delete_items;
	BOOL last_batch_added;
	BOOL reported_status;

protected:

	unsigned int items_added;
	unsigned int items_completed;

public:
	/** Constructor */
	URL_DocSelfTest_Manager();
	/** Destructor */
	virtual ~URL_DocSelfTest_Manager();

	/** Verify function. called for each event and with a status code for error messages
	 *	Must be implemented by subclass.
	 *	Verified MUST call ST_failed() if the test fails
	 *	Implementations MUST NOT call ST_passed()
	 *	Return TRUE if success.
	 */
	virtual BOOL Verify_function(URL_DocSelfTest_Event event, URL_DocSelfTest_Batch *source);

	/** Add another batch */
	BOOL AddBatch(URL_DocSelfTest_Batch *);

	/** Starts the first batch */
	BOOL SetLastBatch();

	/** Handle callbacks */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	void ReportFailure(const char *format, ...);
	void ReportTheFailure(const char *report);

	BOOL ReportedStatus() const{return reported_status;}

	void ReportItemCompleted(){items_completed++;}

private:

	/** Start loading the first batch of testcases */
	BOOL LoadFirstBatch();

	/** Clear used items */
	void DeletePendingBatches();
};


class URL_DocSimpleTester : public URL_DocSelfTest_Item
{
protected:
	OpString	test_file;
	BOOL	header_loaded;

public:
	URL_DocSimpleTester() : header_loaded(FALSE) {};
	URL_DocSimpleTester(URL &a_url, URL &ref, BOOL unique=TRUE) : URL_DocSelfTest_Item(a_url, ref, unique), header_loaded(FALSE) {};
	virtual ~URL_DocSimpleTester(){}

	OP_STATUS Construct(const OpStringC &filename);

	OP_STATUS Construct(URL &a_url, URL &ref, const OpStringC &filename, BOOL unique=TRUE);

	OP_STATUS Construct(const OpStringC &source_url, URL &ref, const OpStringC &filename, BOOL unique=TRUE);

	OP_STATUS Construct(const OpStringC8 &filename);

	OP_STATUS Construct(URL &a_url, URL &ref, const OpStringC8 &filename, BOOL unique=TRUE);

	OP_STATUS Construct(const OpStringC8 &source_url, URL &ref, const OpStringC8 &filename, BOOL unique=TRUE);

	OP_STATUS Construct(const OpStringC8 &source_url, URL &ref, const OpStringC &filename, BOOL unique=TRUE);

	virtual BOOL Verify_function(URL_DocSelfTest_Event event, Str::LocaleString status_code=Str::NOT_A_STRING);
};

class URL_DocVerySimpleTest : public URL_DocSimpleTester
{
public:
	URL_DocVerySimpleTest() {}

	OP_STATUS Construct(const OpStringC8 &baseurl_format, URL &ref, const OpStringC8 &filename, BOOL unique=TRUE);
};

#endif // _URL_DOC_TESTMAN_H_
#endif // SELFTEST
