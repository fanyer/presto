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

#ifndef URLLOADHANDLER_H
#define URLLOADHANDLER_H

// URL Load Handlers

#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"

#include "modules/util/str.h"
#include "modules/util/opfile/opfile.h"
#include "modules/formats/hdsplit.h"

class URL_Rep;
class AuthElm;
class HTTP_Request;
class OpRemoteFile;
class URL_HTTP_ProtocolData;
struct HeaderInfo;
class OpFolderListing;
class HeaderList;
class OperaCacheReaderInterface;

enum HTTPAuth {
	HTTP_AUTH_NOT = 0,
	HTTP_AUTH_NEED,
	HTTP_AUTH_HAS
} ;

class File_Info : public Link
{
public:
	BOOL processed;
	OpFileInfo::Mode attrib;	// Attribute for matched path: Attributes
	time_t wr_time;			// Time of last file write
	OpFileLength size;		// Length of file in bytes
	OpString name;			// name
	OpString8 rawname;		// Raw name (only used for ftp)
	char type;

	File_Info();
	virtual ~File_Info();

	File_Info *Suc(){return (File_Info *) Link::Suc();}
	File_Info *Pred(){return (File_Info *) Link::Pred();}

};

class File_Info_List : public AutoDeleteHead
{
public:
	void Sort_List(int (*compare)(const File_Info *, const File_Info *));

	File_Info *First(){return (File_Info *) Head::First();}
	File_Info *Last(){return (File_Info *) Head::Last();}
};

#define FILE_INFO_ATTR_FILE   0
#define FILE_INFO_ATTR_DIR	  1
#define FILE_INFO_ATTR_SYMBLINK 2

/** Basic interface between protocol stacks and the URL_DataStorage class
 *
 *	Messages usually sent by URL_LoadHandler implementations (specific implementations may add others)
 *
 *		MSG_COMM_LOADING_FINISHED:
 *			par1 : sender Id
 *			par2 : unused
 *
 *			Indicates that the sink received a normal close from the server (or have decided to treat it as such)
 *
 *		MSG_COMM_LOADING_FAILED:
 *			par1 : sender Id
 *			par2 : error code
 *
 *			An error occured in the sink, par2 contains the languagecode for the error.
 *			The sink is usually closed when these messages are sent, the exception being
 *			Str::SI_ERR_HTTP_100CONTINUE_ERROR, which is non-fatal
 *
 *			The receiver may act on the error code, or send it on tho the next parent.
 *
 *			The error code ERR_SSL_ERROR_HANDLED indicates that an error occured, but
 *			has been handled, and the parent/document should silently end loading.
 *
 *		MSG_COMM_DATA_READY
 *			par1 : sender Id
 *			par2 : unused
 *
 *			Fallback message, used in some cases by ProcessReceivedData when an SComm does not
 *			have an SComm derived parent, to indicate that there are data waiting to be read.
 *
 *
 *	Implementations should implement the following functions:
 *
 *		SetCallbacks()
 *		Load()
 *		SetProgressInformation()
 *		Stop()
 *		HandleCallback()
 *
 *
 *	And may implement, as needed:
 *
 *		ReadData()
 *		SendData()
 *		ConnectionEstablished()
 *		RequestMoreData()
 *		ProcessReceivedData()
 *		SecondaryProcessReceivedData()
 *		GetSecurityLevel()
 *		GetSecurityText()
 *		SafeToDelete()
 *
 *	SetProgressInformation should handle the following progress_levels
 *
 *		STOP_FURTHER_REQUESTS : Should not send further requests on this connection
 *			(sent by SSL for some servers that do not handle renegotiation gracefully)
 *		RESTART_LOADING: The request should be resent, Progress_info2 (if non-NULL)
 *			is a pointer to a BOOL that must be set to TRUE if the operation is permitted.
 *			(Used by SSL in case the secure connection has to be restarted, during initial handshake)
 *		HANDLE_SECONDARY_DATA: Used by HTTPS through proxy to indicate that the real source
 *			of data is the CONNECT request to the proxy, not the actual request
 *		HEADER_LOADED : Inidicates that the header has been loaded, or the connection is
 *			ready to receive payload data.
 *		GET_APPLICATIONINFO : Request by sink (usually SSL) to get the current URL for
 *			display purposes. The Password part of the URL MUST be hidden. progress_info2
 *			is a pointer to a OpString pointer.
 *
 *		The remaining updates MUST be sent to URL_LoadHandler::SetProgressInformation, unless there are special reasons why not.
 */
class URL_LoadHandler : public ProtocolComm
{
	protected:
		URL_Rep *url;	 // Does NOT update reference count
		//MessageHandler *mh;

		ProgressState stored_progress_level;
		unsigned long stored_progress_info1;
		OpStringS stored_progress_info2;
	public:
		URL_LoadHandler(URL_Rep *url_rep, MessageHandler *msgh);
		virtual ~URL_LoadHandler();

#ifdef _ENABLE_AUTHENTICATE
		/** Update Authentication credentials with the provided authentication element */
		virtual void Authenticate(AuthElm *auth_elm){};
		/** Did we use the same authentication credentials last time? */
		virtual BOOL CheckSameAuthorization(AuthElm *auth_elm, BOOL proxy){return TRUE;};
#endif
		/* Is only needed for URL_HTTP_LoadHandler */
		virtual OP_STATUS CopyExternalHeadersToUrl(URL &url){ return OpStatus::OK; }

		virtual CommState Load() = 0;

		virtual unsigned ReadData(char *buffer, unsigned buffer_len)=0;

		virtual void EndLoading(BOOL force=FALSE)=0;

		OP_STATUS GenerateDirectoryHTML(OpFolderListing *, File_Info_List &finfo1,
										BOOL &finished,BOOL delete_finfo);

		/*
		unsigned GenerateDirectoryHTML(char *buffer, unsigned buffer_len,
							unsigned &pos, unsigned &count, BOOL &started,
							const char *dirname, file_info **finfo,
							BOOL &finished,BOOL delete_finfo = TRUE);
							*/

		virtual void		ProcessReceivedData();
		virtual void		SetProgressInformation(ProgressState progress_level, unsigned long progress_info1=0, const void *progress_info2 = NULL);
		void				RefreshProgressInformation();

		virtual void		DisableAutomaticRefetching();

#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
	/** Pauses the loading of an URL. Only implemented by some load handlers. */
		virtual void PauseLoading() {};
		virtual OP_STATUS ContinueLoading() { return OpStatus::OK; };
#endif

#ifdef URL_NOTIFY_FILE_DOWNLOAD_SOCKET
		/**
		 * Notifies the comms stack if it is currently handling a file download.
		 * Only implemented by some load handlers.
		*/
		virtual void SetIsFileDownload(BOOL value) {}
#endif
		/* Implemented to avoid warning about the other EndLoading() function in
		   this class this one inherited from SComm (via ProtocolComm). */
		virtual void EndLoading() { SComm::EndLoading(); }
};

struct HTTP_request_st;

//class HTTP_Request;

class URL_HTTP_LoadHandler : public URL_LoadHandler
{
	private:
		HTTP_Request *req;

		struct http_load_info
		{
			unsigned waiting:1;
			unsigned use_name_completion:1;
			unsigned name_completion_modus:1;
#ifdef HTTP_DIGEST_AUTH
			unsigned check_auth_info:1;
			unsigned check_proxy_auth_info:1;
#endif
#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
			unsigned load_direct:1;
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
#ifdef _EMBEDDED_BYTEMOBILE
			unsigned predicted:1;
#endif // _EMBEDDED_BYTEMOBILE
#ifdef WEBSERVER_SUPPORT
			UINT unite_connection:1;
			UINT unite_admin_connection:1;
#endif // WEBSERVER_SUPPORT
		} info;

		HTTP_request_st *GetServerAndRequestL();
		void HandleHeaderLoadedL();
		void HandleLoadingFailed(MH_PARAM_1 par1, MH_PARAM_2 par2);
		void HandleLoadingFinished();
#if defined _ASK_COOKIE || defined COOKIE_USE_DNS_LOOKUP || defined PUBSUFFIX_ENABLED
		void CheckForCookiesHandledL();
#endif
#ifdef LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY
		void HandleStrictTransportSecurityHeader(HeaderList *header_list);
#endif // LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY

#ifdef STRICT_CACHE_LIMIT
        int ramcacheextra;  // Keep track of how much memory we have added to urlMan::rc_u_extra
        BOOL tryagain; // State variable for retrying loading of file
#endif

	protected:
		void HandleHeaderLoadedStage2L(URL_HTTP_ProtocolData *hptr, HeaderInfo *hinfo);

	public:
		URL_HTTP_LoadHandler(URL_Rep *url_rep, MessageHandler *mh);
		virtual ~URL_HTTP_LoadHandler();

		virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
#ifdef _ENABLE_AUTHENTICATE
		virtual void Authenticate(AuthElm *auth_elm);
		void AuthenticateL(AuthElm *auth_elm);
		virtual BOOL CheckSameAuthorization(AuthElm *auth_elm, BOOL proxy);
#endif

#if defined(IMODE_EXTENSIONS)
		URL_Rep* GetURL(void) { return url; } // we need to reach the url from http_req
#endif //IMODE_EXTENSIONS
		virtual CommState Load();
				 CommState LoadL();
		virtual void ProcessReceivedData();
		virtual unsigned ReadData(char *buffer, unsigned buffer_len);
		virtual void EndLoading(BOOL force=FALSE);
		virtual void SetProgressInformation(ProgressState progress_level,
											 unsigned long progress_info1=0,
											 const void *progress_info2=NULL);
#ifdef SCOPE_RESOURCE_MANAGER
		void ReportLoadStatus(unsigned long progress_info1, const void *progress_info2);
#endif // SCOPE_RESOURCE_MANAGER

		void DeleteComm();

		CommState UpdateCookieL();

		virtual void		DisableAutomaticRefetching();
#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
		virtual void PauseLoading();
		virtual OP_STATUS ContinueLoading();
#endif
#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
		void SetLoadDirect(){info.load_direct = TRUE;}
		BOOL GetLoadDirect(){return info.load_direct;}
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
#ifdef _EMBEDDED_BYTEMOBILE
		int predicted_depth;
		void SetPredicted(int depth){info.predicted = TRUE;predicted_depth=depth;}
		BOOL GetPredicted(){return info.predicted;}
		int GetPredictedDepth(){return predicted_depth;}
#endif // _EMBEDDED_BYTEMOBILE
#ifdef WEBSERVER_SUPPORT
		void SetUniteConnection(){info.unite_connection = TRUE;}
		BOOL GetUniteConnection(){return info.unite_connection;}
		void SetUniteAdminConnection(){info.unite_connection = TRUE;}
		BOOL GetUniteAdminConnection(){return info.unite_connection;}
#endif // WEBSERVER_SUPPORT
#ifdef URL_NOTIFY_FILE_DOWNLOAD_SOCKET
		virtual void SetIsFileDownload(BOOL value);
#endif
		OP_STATUS CopyExternalHeadersToUrl(URL &url);
	private:
		BOOL SetupAuthenticationDataL(HeaderList *headerlist, int header_name_id, URL_HTTP_ProtocolData *hptr, HeaderInfo *hinfo, HTTPAuth &auth_status, int &resp);
#ifdef HTTP_DIGEST_AUTH
		BOOL CheckAuthentication(HeaderList	*headerlist, int header_name_id, BOOL proxy, BOOL finished, UINT error_code, BOOL terminate_on_fail, BOOL &failed);
#endif

		BOOL GenerateRedirectHTML(BOOL terminate_if_failed);
};

#ifndef NO_FTP_SUPPORT
class FTP_Request;

class URL_FTP_LoadHandler : public URL_LoadHandler
{
	private:
		FTP_Request *ftp;

		File_Info_List finfo_list;
		char *dir_buffer;
		OpString8  dir_message;
		unsigned unconsumed, dir_buffer_size;
		//unsigned count, pos;
		//unsigned finfo_len;
		InputConverter *converter;

		UINT started:1;
		UINT sending:1;
		UINT authenticating:1;
        UINT authentication_done:1;
		UINT is_directory:1;
		OpFolderListing *generator;
		OpFileLength bytes_downloaded;

	private:
		void HandleHeaderLoadedL( MH_PARAM_2 par2);
		void HandleLoadingFailed(MH_PARAM_1 par1, MH_PARAM_2 par2);
		void HandleLoadingFinished();

		void GenerateDirListing();
		BOOL ParseFtpLine(char* buf, int buf_len);

	public:
		URL_FTP_LoadHandler(URL_Rep *url_rep, MessageHandler *mh);
		virtual ~URL_FTP_LoadHandler();

		virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

#ifdef _ENABLE_AUTHENTICATE
		virtual void Authenticate(AuthElm *auth_elm);
		virtual BOOL CheckSameAuthorization(AuthElm *auth_elm, BOOL proxy);
#endif
		virtual CommState Load();
		virtual unsigned ReadData(char *buffer, unsigned buffer_len);
		virtual void EndLoading(BOOL force=FALSE);
		virtual void SetProgressInformation(ProgressState progress_level,
											 unsigned long progress_info1=0,
											 const void *progress_info2=NULL);

		void DeleteComm();

#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
		virtual void PauseLoading();
		virtual OP_STATUS ContinueLoading();
#endif
};
#endif // NO_FTP_SUPPORT

#ifdef _LOCALHOST_SUPPORT_
class URL_FILE_DIR_LoadHandler : public URL_LoadHandler
{
	private:
		uni_char *dirname;

		File_Info_List finfo_list;
		unsigned count, pos;
		BOOL started;
		OpFolderListing *generator;

	public:
		URL_FILE_DIR_LoadHandler(URL_Rep *url_rep, MessageHandler *mh,
												   const uni_char *filename);
		virtual ~URL_FILE_DIR_LoadHandler();

		virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

		virtual CommState Load();
		virtual unsigned ReadData(char *buffer, unsigned buffer_len);
		virtual void EndLoading(BOOL force=FALSE);

		void		SetFileName(const uni_char *dname) { SetStr(dirname, dname); };//FIXME:OOM (unable to report) // Never used?
		unsigned	LoadDirListHead(char *buf, unsigned buf_len);
		void	CreateDirList();

};
#endif // _LOCALHOST_SUPPORT_

#endif
