/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef TRANSFERMANAGERDOWNLOAD_H
# define TRANSFERMANAGERDOWNLOAD_H
# ifdef WIC_USE_DOWNLOAD_CALLBACK

#include "modules/viewers/viewers.h"
#include "modules/windowcommander/OpWindowCommander.h"

class TransferManagerDownloadAPI
	:	public OpDocumentListener::DownloadCallback
{
public:
	/* class URLInformation methods
	virtual const uni_char*							URLName() = 0;
	virtual const uni_char*							SuggestedFilename() = 0;
	virtual const char *							MIMEType() = 0;
	virtual OP_STATUS								SetMIMETypeOverride(const char *mime_type) = 0;
	virtual long									Size() = 0;
	virtual const uni_char *						SuggestedSavePath() = 0;
	virtual URL_ID									GetURL_Id() = 0;
	virtual OP_STATUS								DownloadDefault(DownloadContext * context, const uni_char * downloadpath = NULL) = 0;
	virtual OP_STATUS								DownloadTo(DownloadContext * context, const uni_char * downloadpath = NULL) = 0;
	virtual void									URL_Information_Done() = 0;
	virtual OP_STATUS 								CreateSecurityInformationParser(OpSecurityInformationParser** parser) const = 0;
	virtual int										GetSecurityMode() = 0;
	virtual int										GetTrustMode() = 0;
	virtual UINT32									SecurityLowStatusReason() = 0;
	virtual const uni_char*							ServerUniName() const = 0;
	virtual OP_STATUS 								CreateTrustInformationResolver(OpTrustInformationResolver** resolver, OpTrustInfoResolverListener * listener) const = 0;

	OpDocumentListener::DownloadCallback methods:
	virtual OpDocumentListener::DownloadRequestMode	Mode() = 0;
	virtual void									SetDownloadContext(DownloadContext * dc) = 0;
	virtual DownloadContext *						GetDownloadContext() = 0;
	virtual BOOL									Save(uni_char * filename) = 0;
	virtual BOOL									Run(uni_char * filename) = 0;
	virtual BOOL									ReturnToSender(BOOL usePlugin) = 0;
	virtual BOOL									Abort() = 0;
	*/
	// New functions defined by this api:

	/** URLFullName will make sensitive information available */
	virtual const OpStringC8	URLFullName() = 0;

	/** GetOpWindow returns the corresponding OpWindow */
	virtual OpWindow * GetOpWindow() = 0;
	/** Set WAIT_FOR_USER */
	virtual void SetWaitForUser() = 0;
	/** SetViewer sets the viewer which will spill over to TransferItem. */
	virtual void SetViewer(Viewer& viewer) = 0;
	/** SetViewerAction is a wrapper for SetAction */
	virtual void SetViewerAction(ViewAction action) = 0;
	/** SetViewerApplication is a wrapper for SetApplication */
	virtual void SetViewerApplication(const uni_char *application) = 0;
	/** IsSaveDirect returns true if Viewer is set to mode SAVE_DIRECT and is not overridden by current flags */
	virtual BOOL IsSaveDirect() = 0;
	/** SetSaveDirectFileName will try to generate a unique filename in the direct download folder defined by the viewer
	 *  it will report success in not_success, and return OpStatus on more basic failure (OOM etc). */
	virtual OP_STATUS SetSaveDirectFileName(OpString& filename, OpString& direct_filename, BOOL& not_success) = 0;
	/** Stop is a wrapper */
	virtual void Stop() = 0;
	/** Force the mimetype to change */
	virtual void ForceMIMEType(URLContentType type) = 0;
	/** URLType */
	virtual URLType GetURLType() = 0;
	/** FileNameExtension  */
	virtual const uni_char * SuggestedExtension() = 0;
	/** FilePathName */
	virtual const uni_char * FilePathName() = 0;
	/** GetServerName */
	virtual const ServerName * GetServerName() = 0;
	/** GetHostName */
	virtual OP_STATUS GetHostName(OpString& name) = 0;
	/** GetContentSize */
	virtual OpFileLength GetContentSize() = 0;
	/** GetLoadedSize */
	virtual OpFileLength GetLoadedSize() = 0;
	virtual BOOL IsLoading() = 0;
	virtual BOOL IsLoaded() = 0;
	virtual BOOL GetUsedContentDispositionAttachment() = 0;
	virtual OP_STATUS GetApplicationToOpenWith(OpString& application, BOOL& pass_url) = 0;
	virtual BOOL PassURL() = 0;
	/** Informs if file was previously downloaded */
	virtual BOOL WasDownloaded() = 0;
};

class WindowCommander;
class TransferManagerDownloadCallback
	:	public TransferManagerDownloadAPI,
		private MessageObject
{
private:
	DoneListener * 		done_listener;
	DocumentManager*	document_manager;
	URL					download_url;
	Viewer	 			viewer;
	ViewActionFlag		current_action_flag; // To overide flags on the viewer (which would be default)

	BOOL 				called, delayed, cancelled, is_download_to, download_to_started;

	BOOL                m_privacy_mode;

	enum DownloadActionMode_t {
		DOWNLOAD_ABORT,
		DOWNLOAD_RUN,
		DOWNLOAD_SAVE,
		DOWNLOAD_UNDECIDED,
	};

	DownloadActionMode_t 	download_action_mode;
	OpString				url_name;
	OpString				suggested_filename;
	OpString				suggested_extension;
	OpString				download_filename;
	OpString				file_path_name;
	OpString				host_name;
	OpString8				mime_type_override;
	DownloadContext *		download_context;
	BOOL					need_copy_when_downloaded;
	BOOL					keep_loading;
	BOOL					doc_man_can_close;
	WindowCommander *		m_window_commander;
	BOOL					dont_add_to_transfers;
	const BOOL				was_downloaded;

	static const OpMessage download_callback_messages[];
	/* =
		{
			MSG_URL_LOADING_FAILED,
			MSG_HEADER_LOADED,
		};
		*/
	void SetCallbacks(MH_PARAM_1 id);
	void UnsetCallbacks();
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

public:
	TransferManagerDownloadCallback(DocumentManager *	document_manager,
									URL					download_url,
			 						Viewer * 			viewer,
			 						ViewActionFlag		current_action_flag,
									BOOL                was_downloaded = FALSE);

	~TransferManagerDownloadCallback();

	/** Second-step initializer
	 *  Needed only if object was constructed with flag was_downloaded = TRUE
	 *  @param file_path path to file, which is already downloaded
	 *  @return OK if file_path was successfuly saved
	 */
	OP_STATUS Init(const uni_char * file_path);

	//URLInformation methods
	DoneListener * SetDoneListener(DoneListener * listener);
	const uni_char* URLName();

	const uni_char* SuggestedFilename();

	const char* MIMEType();

	OP_STATUS SetMIMETypeOverride(const char *mime_type);

	void ForceMIMEType(URLContentType type);

	long Size();

	const uni_char* SuggestedSavePath();

	URL_ID GetURL_Id();

	const uni_char* HostName();

	OP_STATUS DownloadDefault(DownloadContext * context, const uni_char * downloadpath);

	OP_STATUS DownloadTo(DownloadContext * context, const uni_char * downloadpath);


#ifdef SECURITY_INFORMATION_PARSER
	OP_STATUS CreateSecurityInformationParser(OpSecurityInformationParser** parser);
	int GetSecurityMode();
	int GetTrustMode();
	UINT32 SecurityLowStatusReason();
	const uni_char*	ServerUniName() const;
#endif // SECURITY_INFORMATION_PARSER
#ifdef TRUST_INFO_RESOLVER
	OP_STATUS CreateTrustInformationResolver(OpTrustInformationResolver** resolver, OpTrustInfoResolverListener * listener);
#endif // TRUST_INFO_RESOLVER
	// Called to not depend on the DocumentManager
	void ReleaseDocument();

	// Called if used as part of popupinfo->url_info
	void URL_Information_Done();

	// OpDocumentListener::DownloadCallback methods
	OpDocumentListener::DownloadRequestMode Mode();

	void SetDownloadContext(DownloadContext * dc);

	DownloadContext * GetDownloadContext();

	TransferManagerDownloadAPI * GetDownloadAPI();

	// TransferManagerDownloadAPI method
	const OpStringC8 URLFullName();
	
	const uni_char * URLUniFullName();

	OpWindow * GetOpWindow();

	void SetWaitForUser();

	void SetViewer(Viewer& viewer);

	void SetViewerAction(ViewAction action);

	void SetViewerApplication(const uni_char *application);

	BOOL IsSaveDirect();

	OP_STATUS SetSaveDirectFileName(OpString& filename, OpString& direct_filename, BOOL& not_success);

	void Stop();

	URLContentType GetURLContentType();

	GadgetContentType GetGadgetContentType();

	URLType GetURLType();

	const uni_char * SuggestedExtension();

	const uni_char * FilePathName();

	const ServerName* GetServerName();

	OP_STATUS GetHostName(OpString& name);

	OpFileLength GetContentSize();

	OpFileLength GetLoadedSize();

	BOOL IsLoading();
	BOOL IsLoaded();
	BOOL GetUsedContentDispositionAttachment();

	OP_STATUS GetApplicationToOpenWith(OpString& application, BOOL& pass_url);
	BOOL PassURL();

	// OpDocumentListener::DownloadCallback methods that will conclude the
	// interaction and ultimately free/delete the resources
	BOOL Save(uni_char * filename);

	BOOL Run(uni_char * filename);

	BOOL Abort();

	BOOL ReturnToSender(BOOL usePlugin);

	// Methods used by DocumentManager or self
	OP_BOOLEAN Execute();

	void Cancel();

	void SetKeepLoading() { keep_loading = TRUE; }

	BOOL WasDownloaded() { return was_downloaded; }
private:
	void		EnableWindowClose();

	void		DisableWindowClose();

	OP_STATUS	LoadToFile(uni_char * filename);

	OP_STATUS	DoLoadedCheck();

	BOOL		IsUrlInlined();

	BOOL		WaitForHeader(WindowCommander * window_commander);

	void		TryDelete();
};


#endif // WIC_USE_DOWNLOAD_CALLBACK

#endif /*TRANSFERMANAGERDOWNLOAD_H*/
