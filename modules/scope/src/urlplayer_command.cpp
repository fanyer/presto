/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SCOPE_URL_PLAYER

#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/urlplayer_command.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/windowcommander/src/WindowCommanderUrlPlayerListener.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"

class OpUrlPlayerWindow;

class OpUrlPlayerLoadingListener : public OpLoadingListener
{
public:
	OpUrlPlayerLoadingListener(int window, OpScopeUrlPlayer* cmd);

	virtual void OnUrlChanged(OpWindowCommander*, const uni_char* /* url */) {}
	virtual void OnStartLoading(OpWindowCommander*) {}
	virtual void OnLoadingProgress(OpWindowCommander*, const LoadingInformation*) {}
	virtual void OnAuthenticationRequired(OpWindowCommander*, OpAuthenticationCallback*) {}
	virtual void OnUndisplay(OpWindowCommander*) {}
	virtual void OnUploadingFinished(OpWindowCommander*, LoadingFinishStatus) {}
	virtual void OnStartUploading(OpWindowCommander*) {}
	virtual void OnLoadingCreated(OpWindowCommander*) {}

	virtual void OnLoadingFinished(OpWindowCommander* commander, LoadingFinishStatus status);
	virtual BOOL OnLoadingFailed(OpWindowCommander*, int /* msg_id */, const uni_char* /* url */) { return FALSE; }
	void OnXmlParseFailure(OpWindowCommander* commander) {}
#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	void OnSearchSuggestionsRequested(OpWindowCommander* commander, const uni_char* url, OpSearchSuggestionsCallback* callback) {}
#endif

#ifdef XML_AIT_SUPPORT
	virtual OP_STATUS OnAITDocument(OpWindowCommander*, AITData*) { return OpStatus::OK; }
#endif // XML_AIT_SUPPORT

private:
	int window;
	OpScopeUrlPlayer* cmd;
};

class OpUrlPlayerErrorListener : public OpErrorListener
{
public:
	OpUrlPlayerErrorListener(int window, OpScopeUrlPlayer* cmd);

	virtual void OnOperaDocumentError(OpWindowCommander*, OperaDocumentError) {}
	virtual void OnFileError(OpWindowCommander*, FileError) {}
	virtual void OnHttpError(OpWindowCommander*, HttpError) {}
	virtual void OnDsmccError(OpWindowCommander*, DsmccError) {}
#ifdef _SSL_SUPPORT_
	virtual void OnTlsError(OpWindowCommander*, TlsError) {}
#endif // _SSL_SUPPORT_
	virtual void OnNetworkError(OpWindowCommander*, NetworkError) {}
	virtual void OnBoundaryError(OpWindowCommander*, BoundaryError) {}
	virtual void OnRedirectionRegexpError(OpWindowCommander*) {}
	virtual void OnOomError(OpWindowCommander*) {}
	virtual void OnSoftOomError(OpWindowCommander*) {}

private:
	int window;
	OpScopeUrlPlayer* cmd;
};

class OpUrlPlayerWindow
{
public:
	OpUrlPlayerWindow();
	~OpUrlPlayerWindow();

	OpUrlPlayerLoadingListener*	m_loading_listener;
	OpUrlPlayerErrorListener*	m_error_listener;
	OpWindowCommander*			m_windowcommander;
	/** Have we sent a reply to the command? This state is needed to avoid
	    sending multiple responses to one command, since we might get 
	    LOADING_SUCCESS many times on one document. */
	BOOL						m_sent_response;
};

OpUrlPlayerLoadingListener::OpUrlPlayerLoadingListener(int window, OpScopeUrlPlayer* cmd)
	: window(window)
	, cmd(cmd)
{
}

OpUrlPlayerErrorListener::OpUrlPlayerErrorListener(int window, OpScopeUrlPlayer* cmd)
	: window(window)
	, cmd(cmd)
{
}

OpUrlPlayerWindow::OpUrlPlayerWindow()
	: m_loading_listener(NULL)
	, m_error_listener(NULL)
	, m_windowcommander(NULL)
	, m_sent_response(TRUE)
{
}

OpUrlPlayerWindow::~OpUrlPlayerWindow()
{
	OP_DELETE(m_loading_listener);
	OP_DELETE(m_error_listener);
}

OpScopeUrlPlayer::OpScopeUrlPlayer() 
	: OpScopeUrlPlayer_SI()
	, m_num_windows(0)
	, m_windows(NULL)
{
}

OpScopeUrlPlayer::~OpScopeUrlPlayer()
{
	OP_DELETEA(m_windows);
}

void 
OpScopeUrlPlayer::OnDeleteWindowCommander(OpWindowCommander* wc)
{
	//Lets iterate trough our list, and NULLIFY the windowcommanders that are 
	//being closed closed. We also need to send "done" message!
	if(m_windows)
	{
		for (int i=0; i < m_num_windows; i++ ) 
		{
			if (m_windows[i].m_windowcommander == wc)
			{
				//Send finish messages
				if(m_windows[i].m_loading_listener != NULL)
					m_windows[i].m_loading_listener->OnLoadingFinished(m_windows[i].m_windowcommander, OpLoadingListener::LOADING_UNKNOWN);
				
				//Delete loading listener
				if(m_windows[i].m_loading_listener != NULL)
				{
					OP_DELETE(m_windows[i].m_loading_listener);
					m_windows[i].m_loading_listener = NULL;
				}

				//Delete error listener
				if(m_windows[i].m_error_listener != NULL)
				{
					OP_DELETE(m_windows[i].m_error_listener);
					m_windows[i].m_error_listener = NULL;
				}
					
				//Remove us from listener
				m_windows[i].m_windowcommander->SetUrlPlayerListener(NULL);
				m_windows[i].m_windowcommander = NULL;
			}
		}
	}
}

OP_STATUS
OpScopeUrlPlayer::DoCreateWindows(const WindowCount &in, WindowCount &out)
{
	int i;

	if (m_windows != NULL) 
	{
		// reset and restart
		for ( i=0; i < m_num_windows; i++ ) 
		{
			if (m_windows[i].m_windowcommander == NULL)
				break;//Since we break in setup, we also need to break in teardown!

			//remove url player listener!
			m_windows[i].m_windowcommander->SetUrlPlayerListener(NULL);

			//close window
			g_windowCommanderManager->
				GetUiWindowListener()->
					CloseUiWindow(m_windows[i].m_windowcommander);
		}
		OP_DELETEA(m_windows);	// deletes the listeners
		m_windows = NULL;
	}

	m_num_windows = in.GetWindowCount();
	if (m_num_windows == 0)
		return OpStatus::ERR_OUT_OF_RANGE;

	m_windows = OP_NEWA(OpUrlPlayerWindow, m_num_windows);
	RETURN_OOM_IF_NULL(m_windows);

	OpString url_string; 

	if (OpStatus::IsError(url_string.Set("")))
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_STATUS status = OpStatus::OK;
	for ( i=0; i < m_num_windows; i++ ) 
	{
		OP_STATUS initStatus = InitSingleWindow(i, url_string);
		if(OpStatus::IsError(initStatus))
		{
			if(OpStatus::IsMemoryError(initStatus))
			{
				status = initStatus;
			}
			break;
		}
	} 

	// Reset to safe state.  This should probably be done more thoroughly.
	if (i < m_num_windows)
		m_windows[i].m_windowcommander = NULL;

	out.SetWindowCount(m_num_windows); // TODO: See if this is even necessary, the spec says to return this value but the code does not seem to reflect that.
	return status;
}

OP_STATUS
OpScopeUrlPlayer::DoLoadUrl(const Request &in, Window &out)
{
	int window_number = in.GetWindowNumber();

	if ( window_number < 0 || window_number >= m_num_windows )
		return OpStatus::ERR_OUT_OF_RANGE; // TODO: Perhaps introduce a SetError to set STP errors directly?

	const OpString &url_string = in.GetUrl();

	if (m_windows[window_number].m_windowcommander == NULL)
		RETURN_IF_ERROR(InitSingleWindow(window_number, url_string));

	OpWindowCommander::OpenURLOptions options;
	m_windows[window_number].m_windowcommander->OpenURL(url_string.CStr(), options);

	out.SetWindowID(window_number);
	return OpStatus::OK;
}

OP_STATUS
OpScopeUrlPlayer::InitSingleWindow(int i, const OpString& url_string)
{
	OpUrlPlayerWindow *window_to_init = &m_windows[i];

	OP_STATUS status = OpStatus::OK;
	if ( OpStatus::IsError(status = g_windowCommanderManager->GetWindowCommander(&window_to_init->m_windowcommander)) )
	{
		status = OpStatus::ERR;
		return false;
	}

	if ( OpStatus::IsError(status = 
			g_windowCommanderManager->
				GetUiWindowListener()->
					CreateUiWindow(window_to_init->m_windowcommander, 
								   NULL,
								   2000, 
								   2000)) )
	{
		status = OpStatus::ERR;
		return status;
	}

	RETURN_IF_ERROR(window_to_init->m_windowcommander->OpenURL(url_string.CStr(), FALSE));

	window_to_init->m_loading_listener = OP_NEW(OpUrlPlayerLoadingListener, ( i, this ));
	RETURN_OOM_IF_NULL(window_to_init->m_loading_listener);

	window_to_init->m_error_listener = OP_NEW(OpUrlPlayerErrorListener, (i, this ));
	RETURN_OOM_IF_NULL(window_to_init->m_error_listener);

	window_to_init->m_windowcommander->SetLoadingListener(window_to_init->m_loading_listener);
	window_to_init->m_windowcommander->SetErrorListener(window_to_init->m_error_listener);
	window_to_init->m_windowcommander->SetUrlPlayerListener(this);
	return status;
}

void 
OpUrlPlayerLoadingListener::OnLoadingFinished(OpWindowCommander*, LoadingFinishStatus status)
{
	if (!cmd->IsEnabled())
		return;

	switch (status)
	{
	case OpLoadingListener::LOADING_SUCCESS:
	case OpLoadingListener::LOADING_UNKNOWN:	// normally the translation of URL_LOADING?
	case OpLoadingListener::LOADING_REDIRECT:	// apparently not sent by anyone in core
	{
		OpScopeUrlPlayer::Window msg;
		msg.SetWindowID(window);
		cmd->SendOnUrlLoaded(msg);
		break;
	}

	case OpLoadingListener::LOADING_COULDNT_CONNECT:
	{
		OpScopeUrlPlayer::Window msg;
		msg.SetWindowID(window);
		cmd->SendOnConnectionFailed(msg);
	}
		break;

	default:
		OP_ASSERT(!"Should not happen");
	}
}

#endif // SCOPE_URL_PLAYER
