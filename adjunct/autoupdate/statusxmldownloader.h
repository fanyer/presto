/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Marius Blomli, Michal Zajaczkowski
 */

#ifndef _STATUSXMLDOWNLOADER_H_INCLUDED
#define _STATUSXMLDOWNLOADER_H_INCLUDED

#ifdef AUTO_UPDATE_SUPPORT

#include "modules/windowcommander/OpTransferManager.h"
#include "modules/xmlutils/xmlfragment.h"
#include "adjunct/autoupdate/updatableresource.h"

class AutoUpdater;
class StatusXMLDownloader;
class StatusXMLDownloaderListener;

class StatusXMLDownloaderHolder
{
public:
	static OP_STATUS AddDownloader(StatusXMLDownloader* downloader);
	static OP_STATUS RemoveDownloader(StatusXMLDownloader* downloader);
	static void ReplaceRedirectedFileURLs(const OpString& url, const OpString redirected_url);
private:
	static OpVector<StatusXMLDownloader>	downloader_list;
};

/**
 * Class that encapsulates the download and parsing of the status xml 
 * document. The class is only supposed to be used by the AutoUpdater
 * class, and you need an AutoUpdater instance to instantiate and
 * run the download and parsing.
 *
 * How to use: 
 * (1) From the AutoUpdater, instantiate using itself as 
 * argument to the constructor.
 * (2) Call StartDownload() using the download url with parameters
 * as parameter
 * (3) Release control and wait for callbacks to either 
 * StatusXMLDownloaded or StatusXMLDownloadFailed. 
 * (4) If success, fetch the downloaded information by calling the 
 * GetDownloadFileArray() and GetChangeSettingArray() functions.
 * (5) If error, fetch the error code through the GetErrorCode function.
 *
 * @todo The xml should be signed in some way, probably with a private key...

 */
class StatusXMLDownloader : public OpTransferListener
{
public:
	/**
	 * Enum of states, can be used to tell what went wrong if download or parsing
	 * fails
	 */
	enum DownloadStatus
	{
		READY,
		INPROGRESS,
		NO_TRANSFERITEM,
		NO_URL,
		LOAD_FAILED,
		DOWNLOAD_FAILED,
		DOWNLOAD_ABORTED,
		PARSE_ERROR,
		WRONG_XML,
		OUT_OF_MEMORY,
		SUCCESS
	};

	/**
	 * The StatusXMLDownloader requires a check type given when calling Init().
	 * The value is used to determine the storage location for the 
	 */
	enum CheckType
	{
		CheckTypeInvalid,
		CheckTypeUpdate,
		CheckTypeOther
	};

private:
	OpFileFolder	m_response_file_folder;
	OpString		m_response_file_name;
	CheckType		m_check_type;
	/**
	 * Pointer to the AutoUpdater that initated this download to use for 
	 * calling back when the download is finished.
	 */
	StatusXMLDownloaderListener* m_listener;
	/**
	 * A reference to the TransferItem that represents the transfer 
	 * initiated by this class.
	 */
	OpTransferItem* m_transferItem;
	/**
	 * List of resources for updating for the status xml.
	 */
	OpVector<UpdatableResource> m_resource_list;
	/**
	 * Iterator for going through list of resources that are requested 
	 * for update.
	 */
	UINT m_resource_iterator;
	/**
	 * Status of the xml download and parsing.
	 */
	DownloadStatus m_status;
	/**
	 * Download url string from the xml
	 */
	OpString m_download_url;
	/**
	 * Parse resource element (setting or file)
	 */
	OP_STATUS ParseResourceElement(const OpStringC& resource_type, XMLFragment& fragment);
	/**
	 * Parse resources element
	 */
	OP_STATUS ParseResourcesElement(XMLFragment& fragment);
	/**
	 * Internal function used in parsing the xml fragment. Enters the node to 
	 * fetch the text then closes it again. Note: this call modifies frag, it 
	 * passes the relevant node, so subsequent calls to GetSubnodeText() or 
	 * XMLFragment::EnterNode() with the same node name will enter the NEXT node.
	 * Also note: If the requested node doesen't exist, the function returns NULL
	 * 
	 * @param frag The fragment to work with. Will be modified by the call.
	 * @param node_name The name of the node to enter.
	 * @return pointer to uni_char buffer holding the text in the requested node. 
	 * NULL if requested node does not exist.
	 */
	OP_STATUS GetSubnodeText(XMLFragment* frag, const uni_char* node_name, OpString &text);

	virtual void OnProgress(OpTransferItem* transferItem, TransferStatus status);					///< OpTransferListener implementation
	virtual void OnReset(OpTransferItem* transferItem) {}											///< OpTransferListener implementation
	virtual void OnRedirect(OpTransferItem* transferItem, URL* redirect_from, URL* redirect_to) {}	///< OpTransferListener implementation

	OP_STATUS	GenerateResponseFilename();
	OP_STATUS	DeleteResponseFile();

public:
	/**
	 * Create the downloader for the xml status document. Supply
	 * a pointer to the AutoUpdater that will receive the callback 
	 * when the download is done.
	 */
	StatusXMLDownloader();
	~StatusXMLDownloader();

	OP_STATUS Init(CheckType check_type, StatusXMLDownloaderListener* listener);

	/**
	 * Call this to start downloading from the location pointed to by 
	 * xml_url. 
	 *
	 * @param xml_url Full url to download the status xml including 
	 * parameters.
	 *
	 */
	OP_STATUS StartDownload(const OpString &xml_url);

	/**
	 * Call this function to start posting the xml to the autoupdate server
	 *
	 * @param post_address Url to post xml to
	 * @param xml          String containing the xml to post
	 *
	 */
	OP_STATUS StartXMLRequest(const OpString& post_address, const OpString8 &xml);

	/**
	 * Function that parses the downloaded document and stores the information 
	 * from the xml in the download_file_array and change_setting_array.
	 *
	 * @return TRUE if the xml was successfully parsed, FALSE if not.
	 */
	DownloadStatus ParseDownloadedXML();
	
	/**
	 * Call this function to stop the request
	 *
	 */
	OP_STATUS StopRequest();	
	
	/**
	 * Get first resource
	 */
	UpdatableResource* GetFirstResource() { m_resource_iterator = 0; return GetNextResource(); }	
	/**
	 * Call this to traverse the list of resources requested for update found
	 * in the xml. Each call returns a new element until the list has been
	 * traversed, then it returns NULL, and is reset.
	 */
	UpdatableResource* GetNextResource();

	/**
	 * Remove resource
	 */
	OP_STATUS RemoveResource(UpdatableResource* res);

	UINT32 GetResourceCount();
	
	/**
	 * Get the download page url string (www.opera.com/download)
	 */
	OP_STATUS GetDownloadURL(OpString& download_url) { return download_url.Set(m_download_url); }
	
	/**
	 * Replace redirected url in xml file.
	 */
	OP_STATUS ReplaceRedirectedFileURL(const OpString& url, const OpString& redirected_url);

};

class StatusXMLDownloaderListener
{
public:

	virtual ~StatusXMLDownloaderListener() {};

	/**
	 * Callback that signals that the status XML document has been downloaded
	 * into the supplied url.
	 */
	virtual void StatusXMLDownloaded(StatusXMLDownloader* downloader) = 0;

	/**
	 * Callback used from the StatusXMLDownloader that signals that the 
	 * status XML document couldn't be downloaded.
	 */
	virtual void StatusXMLDownloadFailed(StatusXMLDownloader* downloader, StatusXMLDownloader::DownloadStatus status) = 0;
};

#endif // AUTO_UPDATE_SUPPORT

#endif // _STATUSXMLDOWNLOADER_H_INCLUDED
