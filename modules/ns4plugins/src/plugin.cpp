/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef _PLUGIN_SUPPORT_

#include "modules/pi/OpSystemInfo.h"
#include "modules/console/opconsoleengine.h"
#include "modules/display/vis_dev.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/dom/domenvironment.h"
#include "modules/dom/domutils.h"
#include "modules/encodings/decoders/utf8-decoder.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/libgogi/pi_impl/mde_opview.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/locale/locale-enum.h"
#include "modules/probetools/probepoints.h"
#include "modules/ns4plugins/src/plugin.h"
#include "modules/ns4plugins/src/pluginexceptions.h"
#include "modules/ns4plugins/src/pluginlib.h"
#include "modules/ns4plugins/src/pluginlibhandler.h"
#include "modules/ns4plugins/src/pluginmemoryhandler.h"
#include "modules/ns4plugins/src/pluginstream.h"
#include "modules/ns4plugins/src/pluginfunctions.h"
#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/util/filefun.h"
#include "modules/util/gen_str.h"
#include "modules/util/handy.h"
#include "modules/util/OpHashTable.h"
#include "modules/windowcommander/src/WindowCommander.h"

#ifdef OPERA_CONSOLE
#include "modules/locale/oplanguagemanager.h"
#endif // OPERA_CONSOLE

#ifdef _MACINTOSH_
#include "platforms/mac/pi/plugins/MacOpPluginWindow.h"
#undef macintosh	// macintosh shouldn't be defined for MachO targets
#endif // _MACINTOSH_

#ifdef EPOC
#include <coecntrl.h>
#include "platforms/symbian/bridge/inc/epocabstractplugin.h"
#include "platforms/symbian/engine/inc/epocutils.h"
#endif // EPOC

/** ConvertFromLocalToScreen converts from rendering coordinates
 * to screen coordinates.
 *
 * @param[in] vis_dev visual device doing the conversion
 * @param[in, out] x local x coordinate
 * @param[in, out] y local y coordinate
 */
static void ConvertFromLocalToScreen(VisualDevice* vis_dev, int& x, int& y)
{
	vis_dev->view->ConvertToContainer(x, y);
#ifdef VEGA_OPPAINTER_SUPPORT
	static_cast<MDE_OpView*>(vis_dev->view->GetOpView())->GetMDEWidget()->ConvertToScreen(x, y);
#endif // VEGA_OPPAINTER_SUPPORT
}

static void ConvertFromLocalToScreen(VisualDevice* vis_dev, NPWindow& npwin)
{
	int x = npwin.x;
	int y = npwin.y;

	ConvertFromLocalToScreen(vis_dev, x, y);

	npwin.x = x;
	npwin.y = y;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
Plugin::Plugin()
	:
	URL_DocumentLoader(),
#ifdef _DEBUG
	m_loading_streams(0),
	m_loaded_streams(0),
#endif // _DEBUG
#if NS4P_INVALIDATE_INTERVAL > 0
	m_last_invalidate_time(0),
	m_invalidate_timer_set(FALSE),
	m_invalidate_timer(NULL),
	m_is_flash_video(FALSE),
#endif // NS4P_INVALIDATE_INTERVAL > 0
#if NS4P_STARVE_WHILE_LOADING_RATE>0
	m_starvingStep(0),
#endif // NS4P_STARVE_WHILE_LOADING_RATE
	plugin_name(NULL)
	, ID(0)
	, stream_counter(0)
	, m_document(NULL)
	, m_main_stream(NULL)
	, m_life_cycle_state(UNINITED)
#ifdef USE_PLUGIN_EVENT_API
	, m_window_listener(NULL)
#endif // USE_PLUGIN_EVENT_API
	, m_plugin_window(NULL)
	, m_plugin_window_detached(TRUE)
#ifdef USE_PLUGIN_EVENT_API
	, m_plugin_adapter(NULL)
#endif // USE_PLUGIN_EVENT_API
	, pluginfuncs(NULL)
	, saved(NULL)
#ifndef USE_PLUGIN_EVENT_API
	, win_x(0)
	, win_y(0)
#endif // !USE_PLUGIN_EVENT_API
	, windowless(FALSE)
	, transparent(FALSE)
	, m_core_animation(FALSE)
	, m_mimetype(NULL)
	, m_htm_elm(NULL)
	, m_lastposted(INIT)
	, m_args8(NULL)
	, m_argc(0)
#ifdef USE_PLUGIN_EVENT_API
	, m_pending_display(FALSE)
	, m_pending_create_window(FALSE)
#endif // USE_PLUGIN_EVENT_API
#ifdef _MACINTOSH_
# ifdef NP_NO_QUICKDRAW
	, m_drawing_model(NPDrawingModelCoreGraphics)
# else
	, m_drawing_model(NPDrawingModelQuickDraw)
# endif
#endif // _MACINTOSH_
	, m_lock_stack(NULL)
	, m_is_window_protected(FALSE)
	, m_context_menu_active(FALSE)
#if defined(NS4P_WMP_STOP_STREAMING) || defined(NS4P_WMP_CONVERT_CODEBASE) || defined(NS4P_WMP_LOCAL_FILES)
	, m_is_wmp_mimetype(FALSE)
#endif // NS4P_WMP_STOP_STREAMING || NS4P_WMP_CONVERT_CODEBASE || NS4P_WMP_LOCAL_FILES
	, m_scriptable_object(NULL)
	, m_domelement(NULL)
	, m_mode(0)
	, m_plugin_url_requests_displayed(TRUE)
	, m_popup_stack(NULL)
	, m_popups_enabled(FALSE)
	, m_is_failure(FALSE)
#ifdef NS4P_WINDOWLESS_MACROMEDIA_WORKAROUND
	, m_spoof_ua(FALSE)
#endif // NS4P_WINDOWLESS_MACROMEDIA_WORKAROUND
#ifdef NS4P_TRY_CATCH_PLUGIN_CALL
	, m_is_exception(FALSE)
#endif // NS4P_TRY_CATCH_PLUGIN_CALL
	, m_in_synchronous_loop(0)
	, m_script_exception(FALSE)
	, m_wants_all_network_streams_type(NotAsked)
	, activity_plugin(ACTIVITY_PLUGIN)
{
	npwin.x = 0;
	npwin.y = 0;
	npwin.width = 0;
	npwin.height = 0;
	npwin.window = NULL;
	npwin.clipRect.left = 0;
	npwin.clipRect.top = 0;
	npwin.clipRect.right = 0;
	npwin.clipRect.bottom = 0;
#if defined(XP_UNIX) && !defined(XP_MACOSX)
	npwin.ws_info = NULL;
#endif /* XP_UNIX */
	npwin.type = NPWindowTypeWindow;

	m_last_npwin.x = 0;
	m_last_npwin.y = 0;
	m_last_npwin.width = 0;
	m_last_npwin.height = 0;
	m_last_npwin.window = NULL;
	m_last_npwin.clipRect.left = 0;
	m_last_npwin.clipRect.top = 0;
	m_last_npwin.clipRect.right = 0;
	m_last_npwin.clipRect.bottom = 0;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OP_STATUS Plugin::Create(const uni_char* plugin_dll, OpComponentType component_type, int id)
{
#if NS4P_INVALIDATE_INTERVAL > 0
	m_invalidate_timer = OP_NEW(OpTimer, ());
	RETURN_OOM_IF_NULL(m_invalidate_timer);
	m_invalidate_timer->SetTimerListener(this);
#endif // NS4P_INVALIDATE_INTERVAL > 0
	plugin_name = uni_stripdup(plugin_dll);

	if (!plugin_name)
		return OpStatus::ERR_NO_MEMORY;

	if (!g_pluginhandler || !g_pluginhandler->GetPluginLibHandler())
		return OpStatus::ERR;

	OP_STATUS stat = g_pluginhandler->GetPluginLibHandler()->CreateLib(plugin_name, component_type, &pluginfuncs);

	m_component_type = component_type;

	if (stat == OpStatus::OK)
	{
		ID = static_cast<INT32>(id);

		saved = g_pluginhandler->GetPluginLibHandler()->GetSavedDataPointer(plugin_name);

#ifndef USE_PLUGIN_EVENT_API
		win_x = 0;
		win_y = 0;
#endif // !USE_PLUGIN_EVENT_API

		instance_struct.pdata = NULL;
		instance_struct.ndata = (void*)(INTPTR) id;

#ifdef NS4P_NDATA_REALPLAYER_WORKAROUND
		if (uni_stristr(plugin_dll, UNI_L("nppl3260.dll"))) // avoid RealPlayer 10 crash:
			instance_struct.ndata = g_pluginhandler->GetPluginLibHandler()->GetNdatadummy();
#endif // NS4P_NDATA_REALPLAYER_WORKAROUND

#ifndef STATIC_NS4PLUGINS
		stat = g_pluginhandler->PostPluginMessage(INIT, GetInstance(), id);
#endif // !STATIC_NS4PLUGINS
	}
	return stat;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
Plugin::~Plugin()
{
	// Delete all OpNPObjects first; their destructors require access to members of this object.
	m_objects.Clear();

	// Remove instance from the plugin list maintained by the PluginHandler.
	OP_ASSERT(InList() || !"Only to be removed from the list in the destructor!");
	Out();

#ifdef USE_PLUGIN_EVENT_API
	// Should already be deleted (in Destroy()).
	OP_ASSERT(!m_plugin_window);
	OP_ASSERT(!m_plugin_adapter);

	PluginStream* p = (PluginStream *)stream_list.First();
	while (p)
	{
		PluginStream* ps = p->Suc();

		p->Out();
		OP_DELETE(p);
		p = ps;
	}

	StreamCount_URL_Link* u = static_cast<StreamCount_URL_Link *>(url_list.First());
	while (u)
	{
		StreamCount_URL_Link* us = static_cast<StreamCount_URL_Link *>(u->Suc());

		u->Out();
		OP_DELETE(u);
		u=us;
	};

	if (ID && g_pluginhandler && g_pluginhandler->GetPluginLibHandler())
		g_pluginhandler->GetPluginLibHandler()->DeleteLib(plugin_name);

	OP_DELETEA(plugin_name);
	if (m_mimetype)
		OP_DELETEA(m_mimetype);

	while (m_popup_stack)
	{
		PopupStack* tmp = m_popup_stack->Pred();
		OP_DELETE(m_popup_stack);
		m_popup_stack = tmp;
	}

	while (m_lock_stack)
	{
		LockStack* tmp = m_lock_stack->Pred();
		OP_DELETE(m_lock_stack);
		m_lock_stack = tmp;
	}

	if (m_argc && m_args8)
	{
		for (int c=0; c<m_argc; c++)
		{
			OP_DELETEA(m_args8[c]);
			OP_DELETEA(m_args8[c+m_argc]);
		}
		OP_DELETEA(m_args8);
	}

#ifdef _MACINTOSH_
# ifndef NP_NO_QUICKDRAW
	if (m_drawing_model == NPDrawingModelQuickDraw)
		OP_DELETE(static_cast<NP_Port*>(m_last_npwin.window));
# endif // !NP_NO_QUICKDRAW
	if (m_drawing_model == NPDrawingModelCoreGraphics)
		OP_DELETE(static_cast<NP_CGContext*>(m_last_npwin.window));
#endif // _MACINTOSH_

#else // !USE_PLUGIN_EVENT_API

	// Call DEPRECATED function.
	OldPluginDestructor();

#endif // !USE_PLUGIN_EVENT_API

#if NS4P_INVALIDATE_INTERVAL > 0
	OP_DELETE(m_invalidate_timer);
#endif // NS4P_INVALIDATE_INTERVAL > 0
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OP_STATUS Plugin::ScheduleDestruction()
{
	/*
	 * While we won't destroy it yet, the window needs to be detached from the
	 * View system so that the UI components can be dismantled immediately.
	 */
	if (!IsFailure()) // In the case of an exception, HandleFailure() must be able to clean up.
		DetachWindow();

	if (g_pluginhandler)
	{
		RETURN_IF_ERROR(g_pluginhandler->PostPluginMessage(DESTROY, GetInstance(), GetID()));
		SetLastPosted(DESTROY);
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OP_STATUS Plugin::New(FramesDocument* frames_doc,
#ifndef USE_PLUGIN_EVENT_API
					  void* plugin_window,
#endif // !USE_PLUGIN_EVENT_API
					  const uni_char *mimetype,
					  uint16 mode,
#ifndef USE_PLUGIN_EVENT_API
					  int16 argc,
					  const uni_char *argn[],
					  const uni_char *argv[],
#endif // !USE_PLUGIN_EVENT_API
					  URL* url,
					  BOOL embed_url_changed)
{
	if (!frames_doc)
		return OpStatus::ERR;

	RETURN_IF_MEMORY_ERROR(URL_DocumentLoader::Construct(frames_doc->GetMessageHandler()));

#if defined(USE_PLUGIN_EVENT_API)

	m_document        = frames_doc;
	m_document_url  = m_document->GetURL();
	// Plugin cached content should be deleted after the window, opened in privacy mode, is closed.
	if (m_document->GetWindow() && m_document->GetWindow()->GetPrivacyMode())
	{
		PluginLib *lib = g_pluginhandler->GetPluginLibHandler()->FindPluginLib(plugin_name);
		if (lib)
			RETURN_IF_ERROR(lib->AddURLIntoCleanupTaskList(url));
	}
#ifdef NS4P_ALL_PLUGINS_ARE_WINDOWLESS
	SetWindowless(TRUE);
#endif // NS4P_ALL_PLUGINS_ARE_WINDOWLESS

	OP_ASSERT(!m_plugin_adapter);
	RETURN_IF_ERROR(OpNS4PluginAdapter::Create(&m_plugin_adapter, m_component_type));

#ifdef NS4P_COMPONENT_PLUGINS
	if (m_plugin_adapter->GetChannel())
	{
		m_plugin_adapter_channel_address = m_plugin_adapter->GetChannel()->GetAddress();
		RETURN_IF_ERROR(g_pluginhandler->WhitelistChannel(m_plugin_adapter->GetChannel()));
	}
#endif // NS4P_COMPONENT_PLUGINS

	RETURN_IF_ERROR(SetMimeType(mimetype));

	BOOL is_flash = FALSE;
	BOOL is_acrobat_reader = FALSE;
	DeterminePlugin(is_flash, is_acrobat_reader);

	m_mode =
#ifdef NS4P_ACROBAT_CHANGE_MODE
		is_acrobat_reader ? NP_FULL :
#endif // NS4P_ACROBAT_CHANGE_MODE
		mode;

	RETURN_IF_ERROR(AddParams(url, embed_url_changed, is_flash));

#ifdef NS4P_NPSAVEDDATA_IS_BASE_URL
	URL base = GetBaseURL();
	if (!base.IsEmpty() && saved)
	{
		URL u = g_url_api->GetURL(base, ".");
		OpString8 baseUrl;
		RETURN_IF_ERROR(u.GetAttribute(URL::KName, baseUrl));
		saved->buf = SetNewStr_NotEmpty(baseUrl.CStr());
		if (!saved->buf)
			return OpStatus::ERR_NO_MEMORY;
		saved->len = baseUrl.Length();
	}
#endif // NS4P_NPSAVEDDATA_IS_BASE_URL

	if (GetLastPosted() != NEW && GetLifeCycleState() == UNINITED && OpNS4PluginHandler::GetHandler())
	{
		RETURN_IF_ERROR(((PluginHandler*)OpNS4PluginHandler::GetHandler())->PostPluginMessage(NEW, GetInstance(), GetID()));
		SetLastPosted(NEW);
	}

	return OpStatus::OK;

#else

	// Call DEPRECATED function:
	return OldNew(frames_doc, plugin_window, mimetype, mode, argc, argn, argv, url, embed_url_changed);

#endif // USE_PLUGIN_EVENT_API
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OP_STATUS Plugin::Destroy()
{
#if defined(USE_PLUGIN_EVENT_API)
	OP_ASSERT(!GetLockEnabledState());

	DetachWindow();
	OP_ASSERT(!m_document);

#ifdef NS4P_NPSAVEDDATA_IS_BASE_URL
	if (saved && saved->buf)
	{
		OP_DELETEA(static_cast<char *>(saved->buf));
		saved->buf = NULL;
		saved->len = 0;
	}
#endif // NS4P_NPSAVEDDATA_IS_BASE_URL


	NPError ret = NPERR_NO_ERROR;

	int old_life_cycle_state = GetLifeCycleState();
	SetLifeCycleState(Plugin::FAILED_OR_TERMINATED);

	BOOL old_allow_nested_message_loop = g_pluginscriptdata->AllowNestedMessageLoop();
	g_pluginscriptdata->SetAllowNestedMessageLoop(FALSE);

	OpStatus::Ignore(DestroyAllStreams());

	if (old_life_cycle_state >= NEW_WITHOUT_WINDOW && !IsFailure()) // Make sure that NPP_New() has been called and no exception was thrown
	{
		PushLockEnabledState();
		m_plugin_adapter->SaveState(PLUGIN_OTHER_EVENT);
		TRY
			ret = pluginfuncs->destroy(GetInstance(), &saved);
		CATCH_PLUGIN_EXCEPTION(ret = NPERR_GENERIC_ERROR;SetException();)
		m_plugin_adapter->RestoreState(PLUGIN_OTHER_EVENT);
		PopLockEnabledState();
	}

#ifdef NS4P_COMPONENT_PLUGINS
	if (m_plugin_adapter_channel_address.IsValid())
		g_pluginhandler->DelistChannel(m_plugin_adapter_channel_address);
#endif // NS4P_COMPONENT_PLUGINS
	OP_DELETE(m_plugin_adapter);
	m_plugin_adapter = 0;

	OP_DELETE(m_plugin_window);
	m_plugin_window = 0;

	g_pluginscriptdata->SetAllowNestedMessageLoop(old_allow_nested_message_loop);

	if (ret == NPERR_OUT_OF_MEMORY_ERROR)
		return OpStatus::ERR_NO_MEMORY;
	else if (ret != NPERR_NO_ERROR)
		return OpStatus::ERR;
	else
		return OpStatus::OK;

#else

	// Call DEPRECATED function:
	return OldDestroy();

#endif // USE_PLUGIN_EVENT_API
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OP_STATUS Plugin::AddStream(PluginStream*& new_stream, URL& url, BOOL notify, void* notify_data, BOOL loadInTarget/*=FALSE*/)
{
	PluginStream* stream = OP_NEW(PluginStream, (this, ++stream_counter, url, notify, loadInTarget));
	if (stream)
	{
		OP_STATUS ret = stream->CreateStream(notify_data);
		if (ret != OpStatus::OK)
		{
			OP_DELETE(stream);
			return ret;
		}
		stream->Into(&stream_list);

		// insert unknown/unstreamed urls only
		StreamCount_URL_Link* u = static_cast<StreamCount_URL_Link *>(url_list.First());
		while (u && u->url.Id() != url.Id())
			u = static_cast<StreamCount_URL_Link *>(u->Suc());
		if (u)
		{
			u->stream_count++;
		}
		else
		{
			StreamCount_URL_Link *streamurl=OP_NEW(StreamCount_URL_Link, (url));
			if (streamurl)
				streamurl->Into(&url_list);
			else
			{
				stream->Out();
				OP_DELETE(stream);
				OP_DELETE(streamurl);
				return OpStatus::ERR_NO_MEMORY;
			}
		}
		new_stream = stream;
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

OP_STATUS Plugin::AddPluginStream(URL& url, const uni_char* mime_type)
{
	if (m_main_stream == NULL)
	{
		RETURN_IF_MEMORY_ERROR(AddStream(m_main_stream, url, FALSE, NULL, FALSE));
		if (mime_type)
			return m_main_stream->New(this, mime_type, NULL, 0);
	}
	else
	{
		OP_ASSERT(!"Should probably not happen");
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OP_STATUS Plugin::OnMoreDataAvailable()
{
	OP_PROBE7(OP_PROBE_PLUGIN_LOADDATA);

	if (m_main_stream)
	{
		PluginMsgType msgtype = m_main_stream->GetLastCalled();
		if (!m_main_stream->IsFinished())
		{
			switch (msgtype)
			{
			case INIT:
				if (m_life_cycle_state > UNINITED) // The plugin has called NPP_New()
				{
					if (m_main_stream->IsLoadingInTarget()) // data is loaded without streaming
					{
						if (m_main_stream->GetURL().Status(TRUE) != URL_LOADING)
							RETURN_IF_ERROR(m_main_stream->Notify(this));
					}
					else
						RETURN_IF_ERROR(m_main_stream->New(this, NULL, NULL, 0));
				}
				break;

			case NEWSTREAM:
			case WRITE:
				RETURN_IF_ERROR(m_main_stream->WriteReady(this));
				break;

			case WRITEREADY:
				RETURN_IF_ERROR(m_main_stream->Write(this));
				break;

			case STREAMASFILE:
			case DESTROYSTREAM:
			case URLNOTIFY:
				OP_ASSERT(FALSE);
				break;

			default:
				return OpStatus::ERR;
			}
		}
		else
		{
			switch (msgtype)
			{
			case INIT:
				if (m_main_stream->GetReason() != NPRES_DONE && m_main_stream->GetNotify())
					RETURN_IF_ERROR(m_main_stream->Notify(this));
				break;

			case NEWSTREAM:
				OP_ASSERT(m_main_stream->Type() == NP_ASFILEONLY);
				break;

			case WRITE:
				RETURN_IF_ERROR(m_main_stream->StreamAsFile(this));
				break;

			case STREAMASFILE:
				RETURN_IF_ERROR(m_main_stream->Destroy(this));
				break;

			case DESTROYSTREAM:
				RETURN_IF_ERROR(m_main_stream->Notify(this));
				break;

			case WRITEREADY:
			case URLNOTIFY:
				OP_ASSERT(FALSE);
				break;

			default:
				return OpStatus::ERR;
			}
		}
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OP_STATUS Plugin::InterruptStream(NPStream* pstream, NPReason reason)
{
	PluginStream *ostream = (PluginStream*)stream_list.First();
	while (ostream && ostream->Stream() != pstream)
		ostream = ostream->Suc();

	if (ostream)
		return ostream->Interrupt(this, reason);
	else
		return OpStatus::ERR;
}

#ifdef _PRINT_SUPPORT_
/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL Plugin::HaveSameSourceURL(const uni_char* src_url)
{
	StreamCount_URL_Link* u = static_cast<StreamCount_URL_Link *>(url_list.First());

	if (u == NULL || src_url == NULL)
		return FALSE;

	OpString s;
	OP_STATUS ret = u->url.GetAttribute(URL::KUniName_Username_Password_Hidden, s, TRUE);
	return OpStatus::IsSuccess(ret) && s.Compare(src_url) == 0;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void Plugin::Print(const VisualDevice* vd,
				   const int x,
				   const int y,
				   const unsigned int width,
				   const unsigned int height)
{
#if defined(USE_PLUGIN_EVENT_API)
	OpRect scaled_rect = vd->ScaleToScreen(OpRect(x, y, width, height));

	NPPrint npprint;
	npprint.mode = NP_EMBED;

	NPWindow &tmp_npwindow = npprint.print.embedPrint.window;

	tmp_npwindow.x               = scaled_rect.x;
	tmp_npwindow.y               = scaled_rect.y;
	tmp_npwindow.width           = scaled_rect.width;
	tmp_npwindow.height          = scaled_rect.height;
	tmp_npwindow.window          = 0;
	tmp_npwindow.clipRect.top    = 0;
	tmp_npwindow.clipRect.left   = 0;
	tmp_npwindow.clipRect.bottom = 0;
	tmp_npwindow.clipRect.right  = 0;
	tmp_npwindow.type            = npwin.type;

	RETURN_VOID_IF_ERROR(m_plugin_adapter->SetupNPPrint(&npprint, scaled_rect));

	m_plugin_adapter->SaveState(PLUGIN_PRINT_EVENT);
	TRY
		pluginfuncs->print(GetInstance(), &npprint);
	CATCH_PLUGIN_EXCEPTION(SetException();)
	m_plugin_adapter->RestoreState(PLUGIN_PRINT_EVENT);

#else

	// Call DEPRECATED function:
	OldPrint(vd, x, y, width, height);

#endif // USE_PLUGIN_EVENT_API
}

#endif // _PRINT_SUPPORT_

/***********************************************************************************
**
**
**
***********************************************************************************/
#if defined(USE_PLUGIN_EVENT_API)
BOOL Plugin::SendEvent(OpPlatformEvent * event,
					   OpPluginEventType event_type)
{
	if(!event || !m_plugin_window)
		return FALSE;

	if (windowless)
	{
		// TODO : Not sure what lock should be
		return HandleEvent(event, event_type, FALSE) != 0;
	}
	else
	{
		return m_plugin_window->SendEvent(event);
	}
}
#endif // USE_PLUGIN_EVENT_API

/***********************************************************************************
**
**
**
***********************************************************************************/
#if defined(USE_PLUGIN_EVENT_API)
int16 Plugin::HandleEvent(OpPlatformEvent *event, OpPluginEventType event_type, BOOL lock)
#else
int16 Plugin::HandleEvent(void *event, BOOL lock)
#endif // USE_PLUGIN_EVENT_API
{
	if (GetLifeCycleState() == FAILED_OR_TERMINATED)
		return FALSE;

#if defined(USE_PLUGIN_EVENT_API)

	int16 ret = 0;

	if (!IsWindowProtected() && pluginfuncs->event && m_plugin_adapter)
	{
		m_plugin_adapter->SaveState(event_type);

		SetWindowProtected(lock);

		TRY
			ret = pluginfuncs->event(GetInstance(), event->GetEventData());
		CATCH_PLUGIN_EXCEPTION(ret=0;SetException();)

		SetWindowProtected(FALSE);

		if (m_plugin_adapter)
		{
			// The plugin adapter might be deleted after a call to CallNPP_HandleEventProc
			m_plugin_adapter->RestoreState(event_type);
		}

		/* Key events return value can't be trusted. We have to return true regardless
		   of what plugin returns as we want default browser action to be prevented, 
           but we cannot ignore the kNPEventStartIME or key handling just won't work */
		if (event_type == PLUGIN_KEY_EVENT && ret != kNPEventStartIME)
			ret = 1;
	}

	return ret;

#else

	// Call DEPRECATED function:
	return OldHandleEvent(event, lock);

#endif // USE_PLUGIN_EVENT_API
}

#ifdef USE_PLUGIN_EVENT_API
bool Plugin::DeliverKeyEvent(OpKey::Code key, const uni_char *key_value, OpPlatformKeyEventData *key_event_data, const OpPluginKeyState key_state, OpKey::Location location, const ShiftKeyState modifiers)
{
	OP_NEW_DBG("Plugin::DeliverKeyEvent", "ns4p.input.keys");
	if (!m_plugin_window)
		return false;

#ifdef NS4P_COMPONENT_PLUGINS
	if (windowless)
	{
		// FIXME (CORE-41057): Pre-OOPP code had a comment that said:
		// "TODO : Not sure what lock should be"
		// Thus we should double check the second param to TryTakeHandleEventLock() below.
		HandleEventLock lock;
		if ((lock = TryTakeHandleEventLock(PLUGIN_KEY_EVENT, FALSE)) != HandleEventNotAllowed)
		{
			if (!m_plugin_adapter->IsIMEActive())
			{
				/* Retrieve any platform-specific data needed to re-create platform events in the plug-in wrapper. */
				UINT64 platform_data_1 = 0, platform_data_2 = 0;
				if (key_event_data)
					key_event_data->GetPluginPlatformData(/* out */ platform_data_1, /* out */ platform_data_2);

				INT32 is_event_handled = kNPEventNotHandled;
				OP_STATUS status = PluginFunctions::HandleWindowlessKeyEvent(
					GetInstance(), key, key_value, key_state, modifiers, platform_data_1, platform_data_2, &is_event_handled);

				if (OpStatus::IsSuccess(status) && is_event_handled == kNPEventStartIME)
					m_plugin_adapter->StartIME();

				ReleaseHandleEventLock(lock, PLUGIN_KEY_EVENT);
				RETURN_VALUE_IF_ERROR(status, false);
			}
		}

		/* Key events return value can't be trusted. We have to return true regardless
		   of what plugin returns via is_event_handled, as we want default browser
		   action to be prevented. */
		return true;
	}
#endif // NS4P_COMPONENT_PLUGINS

	OpPlatformEvent* event = NULL;
	OP_STATUS s = m_plugin_window->CreateKeyEvent(&event, key, key_value, key_event_data, key_state, location, modifiers);
	if (OpStatus::IsMemoryError(s))
		g_memory_manager->RaiseCondition(s);
	/* Treat as handled. */
	if (s == OpStatus::ERR_NOT_SUPPORTED)
		return true;
	RETURN_VALUE_IF_ERROR(s, false);

	OP_DBG((UNI_L("DeliverKeyEvent(%c, %s, ...)"), key, (key_state==PLUGIN_KEY_UP?UNI_L("PLUGIN_KEY_UP"):(key_state==PLUGIN_KEY_DOWN?UNI_L("PLUGIN_KEY_DOWN"):UNI_L("PLUGIN_KEY_PRESSED")))));
	bool retval = !!SendEvent(event, PLUGIN_KEY_EVENT);
	OP_DELETE(event);
	return retval;
}

bool Plugin::DeliverFocusEvent(bool focus, FOCUS_REASON reason)
{
	RETURN_VALUE_IF_NULL(m_plugin_window, false);

#ifdef NS4P_COMPONENT_PLUGINS
	if (windowless)
	{
		bool is_event_handled = false;
		HandleEventLock lock;
		if ((lock = TryTakeHandleEventLock(PLUGIN_KEY_EVENT, FALSE)) != HandleEventNotAllowed)
		{
			OP_STATUS status = PluginFunctions::HandleFocusEvent(GetInstance(), focus, reason, &is_event_handled);
			m_plugin_adapter->NotifyFocusEvent(focus, reason);
			ReleaseHandleEventLock(lock, PLUGIN_KEY_EVENT);

			RETURN_VALUE_IF_ERROR(status, false);
		}
		return is_event_handled;
	}
#endif // NS4P_COMPONENT_PLUGINS

	OpPlatformEvent* event = NULL;
	OP_STATUS s = m_plugin_window->CreateFocusEvent(&event, focus);
	if (OpStatus::IsMemoryError(s))
		g_memory_manager->RaiseCondition(s);
	RETURN_VALUE_IF_ERROR(s, false);

	bool ret = !!SendEvent(event, PLUGIN_FOCUS_EVENT);
	OP_DELETE(event);
	return ret;
}
#endif // USE_PLUGIN_EVENT_API

/***********************************************************************************
**
**
**
***********************************************************************************/
PluginStream *Plugin::FindStream(int id)
{
    PluginStream *p = (PluginStream*)stream_list.First();
    while (p)
    {
       if (p->Id() == id)
           return p;
       p = p->Suc();
    }

	return 0;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OpRect Plugin::GetClipRect(int x, int y, int width, int height) {
	OP_ASSERT(m_document);
	VisualDevice* vd = m_document->GetVisualDevice();
	int sx = x, sy = y, sw=width, sh=height;
	// Cut away plugin rect that is outside screen
	if (x <0) {
		sx=0;
		sw+=x;
	}
	if (y <0) {
		sy=0;
		sh+=y;
	}

	OpRect prect(sx+vd->GetRenderingViewX(), sy+vd->GetRenderingViewY(), sw, sh);
	// Check if plugin is visible on screen
	if (sw<0 || sh<0) {
		prect.Empty();
	}
	else {
		OpRect vrect;
		vrect = vd->GetView()->GetVisibleRect();

		if (vrect.x < 0) {
			vrect.width += vrect.x;
			vrect.x=0;
		}
		if (vrect.y < 0) {
			vrect.height += vrect.y;
			vrect.y=0;
		}

		OpPoint op(0,0);
		op = vd->OffsetToContainer(op);
		vrect.x -= op.x;
		vrect.y -= op.y;

		// Change the offset to the scrolling position
		vrect.x += vd->GetRenderingViewX();
		vrect.y += vd->GetRenderingViewY();

		// Get the intersect rect between the visual area and the plugin
		prect.IntersectWith(vrect);

		// Compensate for the scrolling
		if (!prect.IsEmpty()) {
			prect.x -= (x+vd->GetRenderingViewX());
			prect.y -= (y+vd->GetRenderingViewY());
		}

	}
	return prect;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void Plugin::SetWindowless(BOOL winless)
{
	windowless = winless;

	if(windowless)
		npwin.type = NPWindowTypeDrawable;
	else
		npwin.type = NPWindowTypeWindow;
}

#ifdef USE_PLUGIN_EVENT_API

/***********************************************************************************
**
**
**
***********************************************************************************/
void Plugin::SetWindowListener(OpPluginWindowListener* listener)
{
	m_window_listener = listener;

	if(m_plugin_window)
		m_plugin_window->SetListener(m_window_listener);
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OP_STATUS Plugin::CreatePluginWindow(const OpRect& scaled_rect)
{
	// This prevents creating windows of invalid sizes.
	if (scaled_rect.width > 65535 || scaled_rect.height > 65535)
	{
#ifdef OPERA_CONSOLE
		OpConsoleEngine::Message m(OpConsoleEngine::Plugins, OpConsoleEngine::Error);
		ANCHOR(OpConsoleEngine::Message, m);
		RETURN_IF_ERROR(m.context.Set("Plug-in error"));
		OpString e_msg;
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_PLUGIN_INVALID_WINDOW_SIZE_ERROR, e_msg));
		RETURN_IF_ERROR(m.message.AppendFormat(e_msg, scaled_rect.width, scaled_rect.height));
		RETURN_IF_LEAVE(g_console->PostMessageL(&m));
#endif //  OPERA_CONSOLE
		return OpStatus::ERR;
	}

	OP_ASSERT(!m_plugin_window);
	OP_ASSERT(m_document);

	VisualDevice* visual_device = m_document->GetVisualDevice();

#ifdef NS4P_USE_PLUGIN_NATIVE_WINDOW	
	RETURN_IF_ERROR(visual_device->GetNewPluginWindow(m_plugin_window, scaled_rect.x, scaled_rect.y, scaled_rect.width, scaled_rect.height, NULL, windowless, this, m_htm_elm));
#else  // NS4P_USE_PLUGIN_NATIVE_WINDOW
	RETURN_IF_ERROR(visual_device->GetNewPluginWindow(m_plugin_window, scaled_rect.x, scaled_rect.y, scaled_rect.width, scaled_rect.height, NULL, windowless, m_htm_elm));
#endif  // NS4P_USE_PLUGIN_NATIVE_WINDOW
	m_plugin_window_detached = FALSE;

	// Set the windowlistener on the window
	SetWindowListener(m_window_listener);

	m_plugin_window->SetPluginObject(this);

#ifdef MANUAL_PLUGIN_ACTIVATION
	// block mouse input to this plugin until it has been activated?
	m_plugin_window->BlockMouseInput(!m_htm_elm->GetPluginExternal() && !m_htm_elm->GetPluginActivated());
#endif // MANUAL_PLUGIN_ACTIVATION

	m_plugin_adapter->SetPluginWindow(m_plugin_window);
	m_plugin_adapter->SetVisualDevice(visual_device);

	return OpStatus::OK;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void Plugin::CallSetWindow()
{
	OP_ASSERT(m_plugin_adapter);
	if (IsFailure() || !m_plugin_adapter || GetLockEnabledState() || IsWindowProtected() || SameWindow(npwin))
		return;

	BOOL saved_allow_nested_message_loop = g_pluginscriptdata->AllowNestedMessageLoop();
	g_pluginscriptdata->SetAllowNestedMessageLoop(FALSE);

	m_plugin_adapter->SaveState(PLUGIN_OTHER_EVENT);
	NPError ret = NPERR_NO_ERROR;
	SetWindowProtected(TRUE);

#ifdef NS4P_COMPONENT_PLUGINS
	npwin.window = m_plugin_window->GetHandle();
	ret = PluginFunctions::SetWindow(GetInstance(), &npwin, m_plugin_adapter->GetChannel());
#else // !NS4P_COMPONENT_PLUGINS
	TRY
		ret = pluginfuncs->setwindow(GetInstance(), &npwin);
	CATCH_PLUGIN_EXCEPTION(ret = NPERR_GENERIC_ERROR;SetException();)
#endif // !NS4P_COMPONENT_PLUGINS
	SetWindowProtected(FALSE);
	m_plugin_adapter->RestoreState(PLUGIN_OTHER_EVENT);

#ifndef NS4P_COMPONENT_PLUGINS
	if (ret == NPERR_NO_ERROR && windowless)
	{
		OpPlatformEvent* poschanged_event = NULL;
		if (OpStatus::IsSuccess(m_plugin_window->CreateWindowPosChangedEvent(&poschanged_event)))
		{
			m_plugin_adapter->SaveState(PLUGIN_WINDOWPOSCHANGED_EVENT);
			HandleEvent(poschanged_event, PLUGIN_WINDOWPOSCHANGED_EVENT);
			m_plugin_adapter->RestoreState(PLUGIN_WINDOWPOSCHANGED_EVENT);
			OP_DELETE(poschanged_event);
		}
	}
#endif // NS4P_COMPONENT_PLUGINS

	g_pluginscriptdata->SetAllowNestedMessageLoop(saved_allow_nested_message_loop);

	if(ret != NPERR_NO_ERROR)
		return;

	if (GetLifeCycleState() < WITH_WINDOW)
		SetLifeCycleState(Plugin::WITH_WINDOW);
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OP_STATUS Plugin::Display(const OpRect& plugin_rect, const OpRect& paint_rect, BOOL show, HTML_Element* fixed_position_subtree)
{
	if (!m_document || IsFailure())
		return OpStatus::ERR;

	VisualDevice* visual_device = m_document->GetVisualDevice();

	// Scale and store plugin coordinates relative to parent OpView.
	m_view_rect = visual_device->ScaleToScreen(plugin_rect);

	if (!IsDisplayAllowed())
	{
		m_pending_display = TRUE;
		m_pending_create_window = m_life_cycle_state == UNINITED;
		return OpStatus::OK;
	}

	// Get offset of view for cases when plugin is inside iframe.
	OpPoint view_offset(0, 0);
	if (windowless && !m_core_animation)
		visual_device->view->ConvertToContainer(view_offset.x, view_offset.y);

	// Get offset of CoreViewContainer which might be non-0 due to toolbars.
	int container_x = 0, container_y = 0;
#ifdef VEGA_OPPAINTER_SUPPORT
	static_cast<MDE_OpView*>(visual_device->view->GetOpView())->GetMDEWidget()->ConvertToScreen(container_x, container_y);
#endif

	OpRect scaled_rect(visual_device->ScaleToScreen(plugin_rect));
	scaled_rect.OffsetBy(view_offset.x, view_offset.y);

	OpRect scaled_paint_rect(visual_device->ScaleToScreen(paint_rect));
	scaled_paint_rect.OffsetBy(view_offset.x, view_offset.y);
	// Restrict painted area using cliprect from painter. Painter is only available when called during paint.
	if (visual_device->painter)
	{
		OpRect painter_clip_rect;
		visual_device->painter->GetClipRect(&painter_clip_rect);
		if (m_core_animation)
		{
			int x = 0, y = 0;
			visual_device->view->ConvertToContainer(x, y);
			painter_clip_rect.OffsetBy(-x, -y);
		}
		scaled_paint_rect.IntersectWith(painter_clip_rect);
	}

	if (!m_plugin_window)
		RETURN_IF_ERROR(CreatePluginWindow(scaled_rect));

	/* During reflow we can't truthfully tell whether plugin is visible, fixed positioned
	   or what position it really has. Cases when plugin is moved out of the screen or when
	   its visibility gets changed are handled by CheckCoreViewVisibility method in VisualDevice. */
	if (visual_device->painter)
	{
		visual_device->SetPluginFixedPositionSubtree(m_plugin_window, fixed_position_subtree);
		visual_device->ShowPluginWindow(m_plugin_window, show);
	}

	visual_device->ResizePluginWindow(m_plugin_window, scaled_rect.x, scaled_rect.y, scaled_rect.width, scaled_rect.height, show, TRUE);

	// Set up default values for npwin. Coordinates are relative to window whether plugin is windowless or not.
	npwin.x = m_view_rect.x;
	npwin.y = m_view_rect.y;
	npwin.width = m_view_rect.width;
	npwin.height = m_view_rect.height;
	ConvertFromLocalToScreen(visual_device, npwin);

	// Set clip rect to visible region of plugin.
	OpRect clip_rect = GetClipRect(m_view_rect.x, m_view_rect.y, m_view_rect.width, m_view_rect.height);
	npwin.clipRect.left   = clip_rect.x;
	npwin.clipRect.top    = clip_rect.y;
	npwin.clipRect.right  = clip_rect.Right();
	npwin.clipRect.bottom = clip_rect.Bottom();

	RETURN_IF_ERROR(m_plugin_adapter->SetupNPWindow(&npwin, scaled_rect, scaled_paint_rect, m_view_rect, OpPoint(container_x,container_y), show, IsTransparent()));

	CallSetWindow();

	if (IsFailure())
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	if (GetLifeCycleState() < RUNNING
		&& OpStatus::IsSuccess(((PluginHandler*)OpNS4PluginHandler::GetHandler())->PostPluginMessage(
										SETREADYFORSTREAMING,
										GetInstance(),
										GetID())))
		SetLastPosted(SETREADYFORSTREAMING);

	if (show && windowless)
	{
		if (m_plugin_window->UsesDirectPaint())
		{
			return m_plugin_window->PaintDirectly(scaled_paint_rect);
		}
		else
		{
#ifdef NS4P_COMPONENT_PLUGINS
			HandleEventLock lock;
			if ((lock = TryTakeHandleEventLock(PLUGIN_PAINT_EVENT, TRUE)) != HandleEventNotAllowed)
			{
				/* Calculate the dirty rect relative to the plug-in rectangle. */
				OpRect scaled_dirty_rect(scaled_paint_rect);
				scaled_dirty_rect.OffsetBy(-scaled_rect.x, -scaled_rect.y);
				OP_ASSERT(visual_device->GetWindow() != NULL && "GetWindow() should not return NULL here because we already did doc_manager->GetCurrentDoc() in VisualDevice::OnPaint()");
				OP_STATUS status = PluginFunctions::HandleWindowlessPaint(GetInstance(), visual_device->painter, scaled_rect, scaled_dirty_rect, !!transparent, visual_device->GetWindow()->GetOpWindow());
				ReleaseHandleEventLock(lock, PLUGIN_PAINT_EVENT);
				RETURN_IF_ERROR(status);
			}
#else // NS4P_COMPONENT_PLUGINS
			OP_ASSERT(visual_device->painter);
			unsigned int number_of_events = m_plugin_window->CheckPaintEvent();
			OP_ASSERT(number_of_events <= 2);
			while (number_of_events--)
			{
				OpPlatformEvent* paint_event = NULL;
				OP_STATUS status = m_plugin_window->CreatePaintEvent(&paint_event, visual_device->painter, scaled_paint_rect);
				RETURN_IF_ERROR(status);

				// Don't allow nested message loops during paints when we have too much vulnerable data on the stack.
				BOOL saved_allow_nested_message_loop = g_pluginscriptdata->AllowNestedMessageLoop();
				g_pluginscriptdata->SetAllowNestedMessageLoop(FALSE);
				HandleEvent(paint_event, PLUGIN_PAINT_EVENT);
				OP_DELETE(paint_event);
				g_pluginscriptdata->SetAllowNestedMessageLoop(saved_allow_nested_message_loop);
			}
#endif // NS4P_COMPONENT_PLUGINS
		}
	}
	return OpStatus::OK;
}
#else
OP_STATUS Plugin::SetWindow(int x, int y, unsigned int width, unsigned int height, BOOL show, const OpRect* rect)
{
	if (!m_document)
		return OpStatus::ERR;

	// Call DEPRECATED function:
	return OldSetWindow(x, y, width, height, show, 0);
}
#endif // USE_PLUGIN_EVENT_API

#ifdef NS4P_COMPONENT_PLUGINS
HandleEventLock Plugin::TryTakeHandleEventLock(OpPluginEventType event_type, BOOL window_protection_needed)
{
	if (GetLifeCycleState() != FAILED_OR_TERMINATED && !IsWindowProtected() && pluginfuncs->event && m_plugin_adapter)
	{
		m_plugin_adapter->SaveState(event_type);

		SetWindowProtected(window_protection_needed);
		BOOL saved_allow_nested_message_loop = g_pluginscriptdata->AllowNestedMessageLoop();

		/* Paint events currently occur on a very fragile stack. Historical note: Previously, only
		 * mouse click handling was allowed to nest. When lifting that restriction, paint events
		 * still seemed risky, so it was left in place to err on the side of caution. */
		if (event_type == PLUGIN_PAINT_EVENT)
			g_pluginscriptdata->SetAllowNestedMessageLoop(FALSE);

		return saved_allow_nested_message_loop ? WindowProtectedNestingAllowed : WindowProtectedNestingBlocked;
	}
	return HandleEventNotAllowed;
}

void Plugin::ReleaseHandleEventLock(HandleEventLock lock, OpPluginEventType event_type)
{
	OP_ASSERT(lock != HandleEventNotAllowed);

	g_pluginscriptdata->SetAllowNestedMessageLoop(lock == WindowProtectedNestingAllowed);
	SetWindowProtected(FALSE);

	// adapter might have been deleted during NPP_HandleEvent()
	if (m_plugin_adapter)
		m_plugin_adapter->RestoreState(event_type);
}
#endif // NS4P_COMPONENT_PLUGINS

#if defined(_PLUGIN_NAVIGATION_SUPPORT_)
/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL Plugin::NavigateInsidePlugin(INT32 direction)
{
#if defined(EPOC)
    MOperaPlugin *epocPlugin = (MOperaPlugin*)npwin.window;
    return epocPlugin->Navigate(direction);
#endif // EPOC
    return FALSE;
}
#endif // _PLUGIN_NAVIGATION_SUPPORT_

/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL Plugin::HandleEvent(DOM_EventType dom_event, const OpPoint& point, int button_or_key_or_delta, ShiftKeyState modifiers)
{
#if defined(USE_PLUGIN_EVENT_API)

	if (!m_plugin_window)
		return FALSE;

#ifdef MANUAL_PLUGIN_ACTIVATION
	HTML_Element* elm = GetHtmlElement();

	if (elm && !elm->GetPluginExternal() && !elm->GetPluginActivated())
	{
		// Activate plugin on left click.
		if (button_or_key_or_delta == MOUSE_BUTTON_1)
			if (dom_event == ONMOUSEDOWN)
				m_window_listener->OnMouseDown();
			else if (dom_event == ONMOUSEUP)
				m_window_listener->OnMouseUp();

		// Trigger hover for tooltip.
		m_window_listener->OnMouseHover();
		return FALSE;
	}
#endif // MANUAL_PLUGIN_ACTIVATION

	OpPluginEventType event_type;

	switch (dom_event)
	{
		case ONMOUSEDOWN:
		{
			event_type = PLUGIN_MOUSE_DOWN_EVENT;
			m_window_listener->OnMouseDown();
			if (button_or_key_or_delta == MOUSE_BUTTON_2)
			{
				// When plugins, notably Flash on Linux, open their context menu they sometimes create a new
				// top-level window for it and immediately focuses that window. This means that Opera does
				// not receive any "X11 ButtonRelease event" but rather just a "X11 FocusOut event". This means that
				// the browser viewport CoreView gets stuck in "mouse capture" mode unless we manually releases it.
				m_document->GetVisualDevice()->GetView()->ReleaseMouseCapture();
				// Plugin might prevent mouse up from being seen by us (due to
				// triggering its own context menu) which ends up with gesture
				// UI being shown. We have to cancel it here.
				g_input_manager->ResetInput();
			}
			break;
		}
		case ONMOUSEUP:
		{
			event_type = PLUGIN_MOUSE_UP_EVENT;
			break;
		}
		case ONMOUSEMOVE:
		{
			event_type = PLUGIN_MOUSE_MOVE_EVENT;
			break;
		}
		case ONMOUSEOVER:
		{
			event_type = PLUGIN_MOUSE_ENTER_EVENT;
			break;
		}
		case ONMOUSEOUT:
		{
			event_type = PLUGIN_MOUSE_LEAVE_EVENT;
			break;
		}
		case ONMOUSEWHEELV:
		{
			event_type = PLUGIN_MOUSE_WHEELV_EVENT;
			break;
		}
		case ONMOUSEWHEELH:
		{
			event_type = PLUGIN_MOUSE_WHEELH_EVENT;
			break;
		}
		default:
		{
			return FALSE;
		}
	}

	BOOL retval = FALSE;
	/* Don't lock on right click to allow plugin to paint when context menu is open. */
	BOOL lock_needed = !(button_or_key_or_delta == MOUSE_BUTTON_2 && (dom_event == ONMOUSEDOWN || dom_event == ONMOUSEUP));
#ifdef NS4P_COMPONENT_PLUGINS
	HandleEventLock lock;
	if ((lock = TryTakeHandleEventLock(event_type, lock_needed)) != HandleEventNotAllowed)
	{
		bool is_event_handled = false;
		OP_STATUS status = PluginFunctions::HandleWindowlessMouseEvent(GetInstance(), event_type, OpPoint(point.x - m_view_rect.x, point.y - m_view_rect.y), button_or_key_or_delta, modifiers, &is_event_handled);
		ReleaseHandleEventLock(lock, event_type);
		if (OpStatus::IsError(status))
			retval = FALSE;
		else
			retval = !!is_event_handled;
	}
#else  // NS4P_COMPONENT_PLUGINS
	OpPlatformEvent * event = NULL;
	OP_STATUS status = m_plugin_window->CreateMouseEvent(&event, event_type, point, button_or_key_or_delta, modifiers);
	if (status != OpStatus::OK)
		return FALSE;

	retval = HandleEvent(event, event_type, lock_needed) != 0;

	OP_DELETE(event);
#endif // NS4P_COMPONENT_PLUGINS

	return retval;
#else

	// Call DEPRECATED function:
	return OldHandleEvent(dom_event, point, button_or_key_or_delta, modifiers);

#endif // USE_PLUGIN_EVENT_API
}

/***********************************************************************************
**
**
**
***********************************************************************************/
#ifndef USE_PLUGIN_EVENT_API
void Plugin::OnFocus(BOOL focus, FOCUS_REASON reason)
{
}
#endif // !USE_PLUGIN_EVENT_API

/***********************************************************************************
**
**
**
***********************************************************************************/
void Plugin::SetPopupsEnabled(BOOL enabled)
{
	m_popups_enabled = enabled;
}

/***********************************************************************************
**
**
**
***********************************************************************************/

void Plugin::DestroyAllLoadingStreams()
{
	if (!GetLockEnabledState())
	{
		PluginStream *plugin_stream = (PluginStream*)stream_list.First();
		while (plugin_stream)
		{
			PluginStream *tmpStream =  (PluginStream *)plugin_stream->Suc();

			URLStatus st = plugin_stream->GetURL().Status(TRUE);

			if ( st > URL_LOADED &&
				!plugin_stream->IsLoadingInTarget())
			{
				URL tmp = plugin_stream->GetURL();
				OpStatus::Ignore(plugin_stream->UpdateStatusRequest(this, TRUE));
				URL_DocumentLoader::StopLoading(tmp);

				plugin_stream->NonPostingInterrupt(this);
				int id = plugin_stream->IsLoadingInTarget() ? 0 : plugin_stream->UrlId();

				plugin_stream->Out();
				OP_DELETE(plugin_stream);
				RemoveLinkedUrl(id);
			}
			plugin_stream = tmpStream;
		}
	}
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/

/* virtual */
void Plugin::OnDelete(FramesDocument* document)
{
	SetHtmlElement(NULL);
	if (!document || document->IsBeingDeleted())
		m_document = NULL;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OP_STATUS Plugin::DestroyAllStreams()
{
	if (!GetLockEnabledState())
	{
		PluginStream *plugin_stream;
		while ((plugin_stream = (PluginStream*)stream_list.First()) != 0)
		{
			if (plugin_stream->GetURL().Status(TRUE) == URL_LOADING &&
				!plugin_stream->IsLoadingInTarget())
			{
				URL tmp = plugin_stream->GetURL();
				OpStatus::Ignore(plugin_stream->UpdateStatusRequest(this, TRUE));
				URL_DocumentLoader::StopLoading(tmp);
			}
			plugin_stream->NonPostingInterrupt(this);
			int id = plugin_stream->IsLoadingInTarget() ? 0 : plugin_stream->UrlId();
			plugin_stream->Out();
			OP_DELETE(plugin_stream);
			plugin_stream = NULL;
			RemoveLinkedUrl(id);
		}
	}
	return OpStatus::OK;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void Plugin::RemoveLinkedUrl(URL_ID url_id)
{
	StreamCount_URL_Link* u = static_cast<StreamCount_URL_Link *>(url_list.First());
	while (u && u->url.Id(TRUE) != url_id)
		u = static_cast<StreamCount_URL_Link *>(u->Suc());
	if (u && u->url.Id(TRUE) == url_id)
	{
		if (u->stream_count)
			u->stream_count--;
		if (u->stream_count == 0)
		{
			if (u->url.Status(TRUE) == URL_LOADING)
			{
				if (m_document && GetHtmlElement())
				{
					m_document->StopLoadingInline(&(u->url), GetHtmlElement(), EMBED_INLINE);
					m_document->StopLoadingInline(&(u->url), GetHtmlElement(), GENERIC_INLINE);
				}
			}

			if (u->url.GetAttribute(URL::KUnique))
			{
				u->Out();
				OP_DELETE(u);
				u = NULL;
			}
		}
	}
}

#ifdef NS4P_CHECK_PLUGIN_NAME
/***********************************************************************************
**
**
**
***********************************************************************************/
OP_BOOLEAN Plugin::CheckPluginName(const uni_char* plugin_name)
{
	OP_STATUS stat = OpStatus::OK;
	OpString src;
	OpString filename;
	RETURN_IF_ERROR(src.Set(GetName()));
	TRAP_AND_RETURN_VALUE_IF_ERROR(stat, SplitFilenameIntoComponentsL(src, 0, &filename, 0), stat);
	return uni_strni_eq_lower(filename.CStr(), plugin_name, uni_strlen(plugin_name)) ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}
#endif // NS4P_CHECK_PLUGIN_NAME

/***********************************************************************************
**
**
**
***********************************************************************************/
OP_STATUS Plugin::GetScriptableObject(ES_Runtime* runtime, BOOL allow_suspend, ES_Value* value)
{
	// Get JS_JavaObject for LiveConnect or OpNPExternalObject for the new scripting api.
	// In some cases new plugins supporting the new scripting api has presented themselves with version 11
	// (should have been >= 14), so the test on versioning has been replaced with a test on getvalue.

	if (IsFailure())
		return OpStatus::ERR;

	value->type = VALUE_UNDEFINED;
	if (m_scriptable_object)
	{
		if (!m_scriptable_object->Import(runtime))
			return OpStatus::ERR_NO_MEMORY;

		value->value.object = m_scriptable_object->GetInternal();
		value->type = VALUE_OBJECT;
	}
	else
	{
		if (pluginfuncs->getvalue)
		{
			if (m_life_cycle_state < WITH_WINDOW)
			{
				if (m_life_cycle_state == UNINITED)
				{
					// suspend and wait for plugin initialization (completion of NPP_New() call)
					if (allow_suspend)
						g_pluginhandler->Suspend(GetDocument(), GetHtmlElement(), runtime, value, TRUE);
					return OpStatus::OK;
				}
				else if (m_life_cycle_state == FAILED_OR_TERMINATED || !IsWindowless()) // let scripts access plugin DOM node
					return OpStatus::OK;
			}
			OP_ASSERT(m_life_cycle_state >= WITH_WINDOW || (IsWindowless() && m_life_cycle_state >= PRE_NEW)); // allow scripting

			BOOL saved_allow_nested_message_loop = g_pluginscriptdata->AllowNestedMessageLoop();
			g_pluginscriptdata->SetAllowNestedMessageLoop(FALSE); // avoid freeze by being in RunNow already
			NPError ret = NPERR_NO_ERROR;
			NPObject* object = 0;
			TRY
				ret = pluginfuncs->getvalue(GetInstance(), NPPVpluginScriptableNPObject, &object);
			CATCH_PLUGIN_EXCEPTION(ret = NPERR_GENERIC_ERROR;SetException();)
			g_pluginscriptdata->SetAllowNestedMessageLoop(saved_allow_nested_message_loop);

			if (ret == NPERR_NO_ERROR)
			{
				if (object)
				{
					m_scriptable_object = g_pluginscriptdata->FindObject(object);
					if (!m_scriptable_object) // create an internal representation of the object if it's not found
					{ // happens when scripting the QuakeLive plugin, see CORE-29702
						m_scriptable_object = OpNPObject::Make(this, object);
					}
					if (m_scriptable_object)
					{
						if (!m_scriptable_object->Import(runtime))
							return OpStatus::ERR_NO_MEMORY;

						value->value.object = m_scriptable_object->GetInternal();
						value->type = VALUE_OBJECT;
						return OpStatus::OK;
					}
				}
				else if (!object && pluginfuncs->version >= NP_VERSION_MINOR_14 && m_life_cycle_state < RUNNING)
				{
					// If there is an indication the scriptable object might appear once we're done with the full
					// plugin init sequence, then suspend. This might be too hard for windowless plugins that might
					// be stuck in NEW_WITHOUT_WINDOW once we've stopped calling SetWindow so much
					if (allow_suspend)
						g_pluginhandler->Suspend(GetDocument(), GetHtmlElement(), runtime, value, FALSE);
					GetHtmlElement()->MarkDirty(GetDocument()); // Trigger a reflow so it will be resumed
					return OpStatus::OK;
				}
			}
			else if (ret == NPERR_OUT_OF_MEMORY_ERROR)
				return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OP_STATUS Plugin::GetFormsValue(uni_char*& string_value)
{
	// Get the plugin value (as \0-terminated UTF-8 string data)
	// for form submission if the plugin is part of a form.
	string_value = NULL;
	if (pluginfuncs->getvalue && pluginfuncs->version >= NP_VERSION_MINOR_14)
	{
		void* value = NULL;
		NPError ret = NPERR_NO_ERROR;
		TRY
			ret = pluginfuncs->getvalue(GetInstance(), NPPVformValue, &value);
		CATCH_PLUGIN_EXCEPTION(ret = NPERR_GENERIC_ERROR;SetException();)
		if (ret == NPERR_OUT_OF_MEMORY_ERROR)
			return OpStatus::ERR_NO_MEMORY;

		if (value && ret == NPERR_NO_ERROR)
		{
			OP_STATUS stat = OpStatus::OK;
			NPUTF8 *utf8 = static_cast<NPUTF8 *>(value);
			string_value = OP_NEWA(uni_char, op_strlen(utf8) + 1);
			if (string_value)
			{
				UTF8toUTF16Converter converter;
				int read, written = converter.Convert(utf8, op_strlen(utf8), string_value, UNICODE_SIZE(op_strlen(utf8)), &read);
				string_value[written / 2] = 0;
			}
			else
				stat = OpStatus::ERR_NO_MEMORY;

			PluginMemoryHandler::GetHandler()->Free(utf8);
			return stat;
		}
	}
	return OpStatus::ERR;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OP_STATUS Plugin::GetPluginDescriptionStringValue()
{
	// Get the plugin description string value as UTF-8 string data
	if (pluginfuncs->getvalue)
	{
		void* value = NULL;
		NPError ret = NPERR_NO_ERROR;
		TRY
			ret = pluginfuncs->getvalue(GetInstance(), NPPVpluginDescriptionString, &value);
		CATCH_PLUGIN_EXCEPTION(ret = NPERR_GENERIC_ERROR;SetException();)
		if (ret == NPERR_OUT_OF_MEMORY_ERROR)
			return OpStatus::ERR_NO_MEMORY;

		if (value && ret == NPERR_NO_ERROR)
		{
			OP_STATUS stat = OpStatus::OK;
			NPUTF8 *utf8 = (NPUTF8*)value;
			PluginMemoryHandler::GetHandler()->Free(utf8);
			return stat;
		}
	}
	return OpStatus::ERR;
}

/***********************************************************************************
**	Checks if the plugin is interested in receiving the http body of all http requests
**	including failed ones, http status != 200.
**
***********************************************************************************/
OP_BOOLEAN Plugin::GetPluginWantsAllNetworkStreams()
{
	OP_STATUS stat = OpStatus::OK;
	if (m_wants_all_network_streams_type == NotAsked && pluginfuncs->getvalue)
	{
		m_wants_all_network_streams_type = WantsOnlySucceededNetworkStreams; // default value, old behaviour
		void* value = NULL;
		NPError ret = NPERR_NO_ERROR;
		TRY
			ret = pluginfuncs->getvalue(GetInstance(), NPPVpluginWantsAllNetworkStreams, &value);
		CATCH_PLUGIN_EXCEPTION(ret = NPERR_GENERIC_ERROR;SetException();)
		if (value && ret == NPERR_NO_ERROR)
		{
			if (value)
				m_wants_all_network_streams_type = WantsAllNetworkStreams;
		}
		else if (ret == NPERR_OUT_OF_MEMORY_ERROR)
			stat = OpStatus::ERR_NO_MEMORY;
	}
	if (OpStatus::IsSuccess(stat))
		return m_wants_all_network_streams_type == WantsAllNetworkStreams ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
	else
		return stat;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void Plugin::PushPopupsEnabledState(NPBool enabled)
{
	PopupStack* tmp = OP_NEW(PopupStack, (enabled, m_popup_stack));
	m_popup_stack = tmp;
	return;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void Plugin::PopPopupsEnabledState()
{
	if (m_popup_stack)
	{
		PopupStack* tmp = m_popup_stack->Pred();
		OP_DELETE(m_popup_stack);
		m_popup_stack = tmp;
	}
	return;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
NPObject* Plugin::GetWindowNPObject()
{
	if (!m_document || OpStatus::IsError(m_document->ConstructDOMEnvironment()))
		return NULL;

	ES_Object* window_object = DOM_Utils::GetES_Object(m_document->GetJSWindow());
	OpNPObject* object = FindObject(window_object);

	if (!object)
		object = OpNPObject::Make(this, m_document->GetESRuntime(), DOM_Utils::GetES_Object(m_document->GetJSWindow()));

	if (object)
	{
		object->Retain();
		return object->GetExternal();
	}
	else
		return NULL;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
NPObject* Plugin::GetPluginElementNPObject()
{
	if (!m_document || !GetHtmlElement() || OpStatus::IsError(m_document->ConstructDOMEnvironment()))
		return NULL;

	DOM_Object *node;

	if (OpStatus::IsError(m_document->GetDOMEnvironment()->ConstructNode(node, GetHtmlElement())))
		return NULL;

	ES_Object* node_object = DOM_Utils::GetES_Object(node);
	OpNPObject* object = FindObject(node_object);

	if (!object)
		object = OpNPObject::Make(this, m_document->GetESRuntime(), DOM_Utils::GetES_Object(node));

	if (object)
	{
		object->Retain();
		m_domelement = object->GetInternal();
		return object->GetExternal();
	}
	else
	{
		m_domelement = NULL;
		return NULL;
	}
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void Plugin::PushLockEnabledState()
{
	LockStack* tmp = OP_NEW(LockStack, (m_lock_stack));
	m_lock_stack = tmp;
	return;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void Plugin::PopLockEnabledState()
{
	if (m_lock_stack)
	{
		LockStack* tmp = m_lock_stack->Pred();
		OP_DELETE(m_lock_stack);
		m_lock_stack = tmp;
	}
	return;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL Plugin::IsDisplayAllowed()
{
	return GetLifeCycleState() != UNINITED && !m_is_window_protected;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void Plugin::SetWindowProtected(BOOL is_window_protected /* = TRUE */)
{
	OP_ASSERT(!(is_window_protected && m_is_window_protected) || !"Window is already protected.");
	m_is_window_protected = is_window_protected;

#ifdef USE_PLUGIN_EVENT_API
	if (IsDisplayAllowed())
		HandlePendingDisplay();
#endif // USE_PLUGIN_EVENT_API
}

#ifdef USE_PLUGIN_EVENT_API
/***********************************************************************************
**
**
**
***********************************************************************************/
void Plugin::HandlePendingDisplay()
{
	OP_ASSERT(IsDisplayAllowed());

	if (m_document)
	{
		/*
		 * Plugin::Display was called while Display was not allowed. Hence we have
		 * to invalidate the plug-in rectangle and thereby schedule a paint call to
		 * Display from VisualDevice.
		 */
		if (m_pending_display)
		{
			VisualDevice* vd = m_document->GetVisualDevice();
			OpRect plugin_rect(vd->ScaleToDoc(m_view_rect));
			vd->Update(plugin_rect.x + vd->GetRenderingViewX(), plugin_rect.y + vd->GetRenderingViewY(), plugin_rect.width, plugin_rect.height);
			m_pending_display = FALSE;
		}

		/*
		 * If we had to abort the first Display (where the window is created), then
		 * we'll have to create the window now. The plug-in will not enter running
		 * state until it has a window, and if it is hidden or occluded, the paint
		 * request above may not result in a call to Display.
		 */
		if (m_pending_create_window && GetLifeCycleState() > UNINITED)
		{
			m_pending_create_window = FALSE;
			VisualDevice* vd = m_document->GetVisualDevice();
			OpRect plugin_rect(-m_view_rect.width, -m_view_rect.height, m_view_rect.width, m_view_rect.height);
			plugin_rect = vd->ScaleToDoc(plugin_rect);
			Display(plugin_rect, plugin_rect, FALSE, NULL);
		}

	}
}
#endif // USE_PLUGIN_EVENT_API

/***********************************************************************************
**
**
**
***********************************************************************************/
BOOL Plugin::SameWindow(NPWindow npwin)
{
	if (m_last_npwin.x == npwin.x &&
		m_last_npwin.y == npwin.y &&
		m_last_npwin.width == npwin.width &&
		m_last_npwin.height == npwin.height &&
		m_last_npwin.clipRect.left == npwin.clipRect.left &&
		m_last_npwin.clipRect.top == npwin.clipRect.top &&
		m_last_npwin.clipRect.right == npwin.clipRect.right &&
		m_last_npwin.clipRect.bottom == npwin.clipRect.bottom
#ifdef _MACINTOSH_
# ifndef NP_NO_QUICKDRAW
		/* For QuickDraw model we must not consider window equal if NP_Port's CGrafPtr changed. */
		&& (m_drawing_model != NPDrawingModelQuickDraw ||
			(m_last_npwin.window
			 && static_cast<NP_Port*>(m_last_npwin.window)->port == static_cast<NP_Port*>(npwin.window)->port))
# endif // !NP_NO_QUICKDRAW
		&& (m_drawing_model != NPDrawingModelCoreGraphics || (
			m_last_npwin.window
			&& static_cast<NP_CGContext *>(m_last_npwin.window)->context == static_cast<NP_CGContext *>(npwin.window)->context
			&& static_cast<NP_CGContext *>(m_last_npwin.window)->window == static_cast<NP_CGContext *>(npwin.window)->window))
#endif // _MACINTOSH_
		)
		 return TRUE;

	m_last_npwin.x = npwin.x;
	m_last_npwin.y = npwin.y;
	m_last_npwin.width = npwin.width;
	m_last_npwin.height = npwin.height;
	m_last_npwin.clipRect.left = npwin.clipRect.left;
	m_last_npwin.clipRect.top = npwin.clipRect.top;
	m_last_npwin.clipRect.right = npwin.clipRect.right;
	m_last_npwin.clipRect.bottom = npwin.clipRect.bottom;
#ifdef _MACINTOSH_
# ifndef NP_NO_QUICKDRAW
	if (m_drawing_model == NPDrawingModelQuickDraw)
	{
		if (!m_last_npwin.window)
			m_last_npwin.window = OP_NEW(NP_Port, ());
		if (m_last_npwin.window && npwin.window)
			static_cast<NP_Port*>(m_last_npwin.window)->port = static_cast<NP_Port*>(npwin.window)->port;
	}
# endif // !NP_NO_QUICKDRAW
	if (m_drawing_model == NPDrawingModelCoreGraphics)
	{
		if (!m_last_npwin.window)
			m_last_npwin.window = OP_NEW(NP_CGContext, ());
		if (m_last_npwin.window && npwin.window)
		{
			static_cast<NP_CGContext *>(m_last_npwin.window)->context = static_cast<NP_CGContext *>(npwin.window)->context;
			static_cast<NP_CGContext *>(m_last_npwin.window)->window = static_cast<NP_CGContext *>(npwin.window)->window;
		}
	}
#endif // _MACINTOSH_

	return FALSE;
}

#ifdef USE_PLUGIN_EVENT_API
/***********************************************************************************
**
**
**
***********************************************************************************/
OP_STATUS Plugin::AddParams(URL* url,
							BOOL embed_url_changed,
							BOOL is_flash)
{
	const uni_char** argn = NULL;
	const uni_char** argv = NULL;
	int argc = 0;

	RETURN_IF_ERROR(GetHtmlElement()->GetEmbedAttrs(argc, argn, argv));

	OP_STATUS result = AddParams(argc, argn, argv, url, embed_url_changed, is_flash);

	OP_DELETEA(argn);
	OP_DELETEA(argv);

	char** new_args8 = NULL;
	int new_argc = 0;
	RETURN_IF_ERROR(m_plugin_adapter->ModifyParameters(this, m_args8, m_argc, &new_args8, &new_argc));
	if (new_args8 != NULL && new_argc != 0)
	{
		OP_DELETEA(m_args8);
		m_argc = new_argc;
		m_args8 = new_args8;
	}

	return result;
}
#endif // USE_PLUGIN_EVENT_API

/***********************************************************************************
**
**
**
***********************************************************************************/
#ifdef USE_PLUGIN_EVENT_API
OP_STATUS Plugin::AddParams(int argc,
#else
OP_STATUS Plugin::AddParams(int16 argc,
#endif // USE_PLUGIN_EVENT_API
							const uni_char* argn[],
							const uni_char* argv[],
							URL* url,
							BOOL embed_url_changed,
							BOOL is_flash)
{
	OP_ASSERT(argc);
	OP_ASSERT(GetMimeType());

	/* m_argc keeps track of the number of parameters sent to the plugin */
	m_argc = argc;

	/* Build a list of parameters */
	/* Go through the list of existing parameters */
	AutoDeleteHead parameter_list;
	BOOL add_baseurl = TRUE;
	BOOL add_restricted_allowscriptaccess = FALSE;
	BOOL param_found = FALSE;
	BOOL src_found = FALSE;
	BOOL add_extra_params_called = FALSE;
	OpString dataurl;

	/* The default PluginScriptAccess setting is TRUE, meaning that no extra "AllowScriptAccess" parameter is added.
	   When disabled, the extra parameter AllowScriptAccess=SameDomain is added to Flash Player, if not already present. */
	add_restricted_allowscriptaccess = is_flash && !g_pcapp->GetIntegerPref(PrefsCollectionApp::PluginScriptAccess);

	int i;
#ifdef NS4P_FORCE_FLASH_WINDOWLESS
	BOOL wmodeFound = FALSE;
#endif //NS4P_FORCE_FLASH_WINDOWLESS
	for (i=0; i<argc; i++)
	{
#ifdef NS4P_WINDOWLESS_MACROMEDIA_WORKAROUND
		if (
# ifndef _UNIX_DESKTOP_
			is_flash &&
# endif // _UNIX_DESKTOP_
			argn[i] && argv[i] &&
			uni_stri_eq(argn[i], "WMODE") &&
			(uni_stri_eq(argv[i], "TRANSPARENT") || uni_stri_eq(argv[i], "OPAQUE")))
		{
			RETURN_IF_ERROR(DetermineSpoofing());
		}
#endif // NS4P_WINDOWLESS_MACROMEDIA_WORKAROUND

		if ((argn[i] && !*argn[i]) // avoid crash on empty parameter name
#ifdef NS4P_REMOVE_NULL_PARAMETERS
			|| !argn[i] || !argv[i]
#endif // NS4P_REMOVE_NULL_PARAMETERS
			)
		{
			m_argc--;
			continue;
		}

		PluginParameter* param = OP_NEW(PluginParameter, ());
		if (!param)
			return OpStatus::ERR_NO_MEMORY;

#ifdef NS4P_FORCE_FLASH_WINDOWLESS
		if (is_flash && (wmodeFound=uni_stri_eq(argn[i], "WMODE")) && uni_stri_eq(argv[i], "window"))
			RETURN_IF_ERROR(param->SetNameAndValue(argn[i], UNI_L("opaque")));
		else
#endif //NS4P_FORCE_FLASH_WINDOWLESS
			RETURN_IF_ERROR(param->SetNameAndValue(argn[i], argv[i]));
		param->Into(&parameter_list);

		/* Check if param, src, data, allowscriptaccess and baseurl are among the existing parameters */
		if (uni_stri_eq(argn[i], "PARAM") && !argv[i])
			param_found = TRUE;
		else if (!param_found && uni_stri_eq(argn[i], "SRC"))
			src_found = TRUE;
		else if (!src_found && !param_found && uni_stri_eq(argn[i], "DATA"))
			RETURN_IF_ERROR(dataurl.Set(argv[i]));
		else if (add_restricted_allowscriptaccess && uni_stri_eq(argn[i], "ALLOWSCRIPTACCESS"))
			add_restricted_allowscriptaccess = FALSE;
		else if (uni_stri_eq(argn[i], "BASEURL")
#ifdef NS4P_WMP_CONVERT_CODEBASE
			|| (m_is_wmp_mimetype && uni_stri_eq(argn[i], "CODEBASE"))
#endif // NS4P_WMP_CONVERT_CODEBASE
			)
			add_baseurl = FALSE;
	}
#ifdef NS4P_FORCE_FLASH_WINDOWLESS
	if (is_flash && !wmodeFound)
	{
		PluginParameter* param = OP_NEW(PluginParameter, ());
		if (!param)
			return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(param->SetNameAndValue(UNI_L("WMODE"), UNI_L("opaque")));
		param->Into(&parameter_list);
		m_argc++;
	}
#endif //NS4P_FORCE_FLASH_WINDOWLESS

	/* Prepare the additional parameters */
	if (add_restricted_allowscriptaccess)
		m_argc++;

	OpString baseurl;
	if (add_baseurl)
	{
#if defined(_LOCALHOST_SUPPORT_) || !defined(RAMCACHE_ONLY)
		URL burl = GetBaseURL();
		if (burl.GetAttribute(URL::KType) == URL_FILE)
		{
			RETURN_IF_ERROR(burl.GetAttribute(URL::KFilePathName_L, baseurl, TRUE));
			// We shall only save the directory part of the filename
			OpFile file;
			file.Construct(baseurl.CStr());
			RETURN_IF_ERROR(file.GetDirectory(baseurl));
			m_argc++;
		}
		else
#endif // _LOCALHOST_SUPPORT_ || !RAMCACHE_ONLY
			add_baseurl = FALSE;
	}

	/* Add a src url only when OBJECT/APPLET's DATA attribute is present and no OBJECT/APPLET SRC attribute is present.
	   OBJECT/APPLET PARAM DATA/SRC attribute and EMBED DATA/SRC attributes do not matter here. */
	if (param_found && !src_found && dataurl.HasContent())
		m_argc++;

#ifdef NS4P_SUPPORT_PROXY_PLUGIN
	m_argc++;
#endif // NS4P_SUPPORT_PROXY_PLUGIN

	/* The final number of parameters has been decided and is kept in m_argc */
	m_args8 = OP_NEWA(char*, 2*m_argc);
	if (m_args8 == NULL)
		return OpStatus::ERR_NO_MEMORY;
	op_memset(m_args8, 0, 2*m_argc*sizeof(char*));

	/* for each parameter, allocate the new name and value into tmpstr and m_args8 */
	int count = 0;
	OpString tmpstr;
	PluginParameter* p = (PluginParameter*) parameter_list.First();
	while (p)
	{
		/* before copying any PARAM attributes, check if extra parameters should be added */
		if (!add_extra_params_called && param_found && p->Contain(UNI_L("PARAM")) && !p->GetValue())
		{
			RETURN_IF_ERROR(AddExtraParams(count, add_restricted_allowscriptaccess, add_baseurl, baseurl, !src_found, dataurl));
			add_extra_params_called = TRUE;
		}

		/* copy name */
		OP_ASSERT(p->GetName());
#ifdef NS4P_WMP_CONVERT_CODEBASE
		if (m_is_wmp_mimetype && p->Contain(UNI_L("URL")))
			RETURN_IF_ERROR(tmpstr.Set(UNI_L("SRC")));
		else if (m_is_wmp_mimetype && p->Contain(UNI_L("CODEBASE")) && !p->FindInValue(UNI_L("http://activex.microsoft.com/activex/controls/mplayer/")))
			RETURN_IF_ERROR(tmpstr.Set(UNI_L("BASEURL")));
		else
#endif // NS4P_WMP_CONVERT_CODEBASE
			RETURN_IF_ERROR(tmpstr.Set(p->GetName()));

		RETURN_IF_ERROR(tmpstr.UTF8Alloc(&m_args8[count]));

		/* copy value */
		if (p->Contain(UNI_L("SRC")) && url && GetMode() == NP_FULL &&
			((p->GetValue() && uni_stri_eq(p->GetValue(), "")) || embed_url_changed))
		{
			// change source url if either the src value is empty or if the object's url value should be used instead (because embed source url failed to load)
#if defined(_LOCALHOST_SUPPORT_) || !defined(RAMCACHE_ONLY)
			if (url->GetAttribute(URL::KType) == URL_FILE)
				RETURN_IF_ERROR(url->GetAttribute(URL::KFilePathName_L, tmpstr, TRUE));
			else
#endif // _LOCALHOST_SUPPORT_ || !RAMCACHE_ONLY
				RETURN_IF_ERROR(url->GetAttribute(URL::KUniName_Username_Password_Hidden, tmpstr, TRUE));
		}
		else if (p->Contain(UNI_L("SRC")) && p->GetValue() && uni_stri_eq(p->GetValue(), ""))
			RETURN_IF_ERROR(tmpstr.Set(UNI_L(" ")));
#if defined(NS4P_WMP_STOP_STREAMING) || defined(NS4P_WMP_CONVERT_CODEBASE)
		else if (IsWMPMimetype() && p->Contain(UNI_L("WINDOWLESSVIDEO")))
			RETURN_IF_ERROR(tmpstr.Set(UNI_L("FALSE")));
#endif // NS4P_WMP_STOP_STREAMING || NS4P_WMP_CONVERT_CODEBASE
		else
			RETURN_IF_ERROR(tmpstr.Set(p->GetValue()));

		RETURN_IF_ERROR(tmpstr.UTF8Alloc(&m_args8[m_argc+count]));

		p = p->Suc();
		count++;
	}

	/* Add any extra parameters for EMBED */
	if (!add_extra_params_called)
		RETURN_IF_ERROR(AddExtraParams(count, add_restricted_allowscriptaccess, add_baseurl, baseurl, FALSE, dataurl));

	OP_ASSERT(m_argc == count);

	return OpStatus::OK;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OP_STATUS Plugin::SetMimeType(const uni_char* mime_type)
{
	OpString8 sb_mime_type;
	RETURN_IF_ERROR(sb_mime_type.SetUTF8FromUTF16(mime_type));
	m_mimetype = SetNewStr_NotEmpty(sb_mime_type.CStr());

	return m_mimetype ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
extern NPError PluginGetURL(BYTE type, NPP instance, const char* url_name, const char* url_target, uint32 len, const char* buf, NPBool file, void* notifyData, NPBool unique, const char* headers);

/***********************************************************************************
**
**
**
***********************************************************************************/
class HandlePluginPostCallback
    : public OpDocumentListener::PluginPostCallback
{
protected:
    Plugin* m_plugin;
    BYTE m_type;
    URL m_post_url;
    char* m_window;
    uint32 m_len;
	unsigned char* m_buf;
    BOOL m_file;
    void* m_notifyData;
    BOOL m_called, m_delayed, m_post, m_cancelled;

public:
    HandlePluginPostCallback(Plugin* plugin, BYTE type, URL post_url, uint32 len, BOOL file, void* notifyData)
        : m_plugin(plugin),
          m_type(type),
          m_post_url(post_url),
          m_window(NULL),
          m_len(len),
          m_buf(NULL),
          m_file(file),
          m_notifyData(notifyData),
          m_called(FALSE),
          m_delayed(FALSE),
          m_post(FALSE),
          m_cancelled(FALSE)
    {
    }

    ~HandlePluginPostCallback()
    {
        OP_DELETEA(m_window);
        OP_DELETEA(m_buf);
    }

    virtual const uni_char *PluginServerName()
    {
        if (ServerName *sn = (ServerName *) m_post_url.GetAttribute(URL::KServerName, (void*)0))
            return sn->UniName();
        else
            return UNI_L("");
    }

    virtual BOOL PluginServerIsHTTP()
    {
        return m_post_url.Type() == URL_HTTP;
    }

    virtual OpDocumentListener::SecurityMode ReferrerSecurityMode()
    {
		if (FramesDocument* doc = m_plugin->GetDocument())
			return doc->GetWindow()->GetWindowCommander()->GetSecurityModeFromState(doc->GetURL().GetAttribute(URL::KSecurityStatus));
		else
			return OpDocumentListener::UNKNOWN_SECURITY;
    }

    virtual void Continue(BOOL do_post)
    {
        m_called = TRUE;
        m_post = do_post;

        if (!m_delayed) // Someone will call Execute and then we will go on
            return;

        if (!m_cancelled)
            OpStatus::Ignore(Execute());

        OP_DELETE(this);
    }

    OP_STATUS SetWindowName(const char *window)
    {
        return SetStr(m_window, window);
    }

	OP_STATUS SetBuf(const unsigned char *binary_buffer, const uint32 buf_len)
	{
		if (buf_len > 0)
		{
			m_buf = OP_NEWA(unsigned char, buf_len);
			if (m_buf)
				op_memcpy(m_buf, binary_buffer, buf_len);
			else
				return OpStatus::ERR_NO_MEMORY;
		}
		return OpStatus::OK;
	}

    OP_BOOLEAN Execute()
    {
		if (g_pluginhandler && m_plugin && g_pluginhandler->FindPlugin(m_plugin->GetInstance()))
		{
			if (m_called)
			{
				if (m_post)
				{
					OpString8 url_name;
					RETURN_IF_ERROR(m_post_url.GetAttribute(URL::KName_Username_Password_Hidden, url_name, TRUE));
					NPError ret = PluginGetURL(m_type, m_plugin->GetInstance(), url_name.CStr(), m_window, m_len, (char*)m_buf, m_file, m_notifyData, TRUE, NULL);
					if (ret == NPERR_OUT_OF_MEMORY_ERROR)
						return OpStatus::ERR_NO_MEMORY;
					else if (ret != NPERR_NO_ERROR)
						return OpStatus::ERR;
				}
				else if (m_type & PLUGIN_NOTIFY)
				{
					// Opera should call NPP_URLNotify() upon unsuccessful completion of the request
					PluginStream* new_stream;
					RETURN_IF_ERROR(m_plugin->AddStream(new_stream, m_post_url, (m_type & PLUGIN_NOTIFY) != 0, m_notifyData, FALSE));
					RETURN_IF_ERROR(new_stream->Interrupt(m_plugin, NPRES_USER_BREAK));
				}
			}
			else
			{
				m_delayed = TRUE;
				return OpBoolean::IS_FALSE;
			}
		}
		return OpBoolean::IS_TRUE; // tells the caller that we are finished and should be deleted
    }

    void Cancel()
    {
        m_cancelled = TRUE;
    }
};

/***********************************************************************************
**
**
**
***********************************************************************************/
OP_BOOLEAN Plugin::HandlePostRequest(BYTE type, const char* url_name, const char* win_name, uint32 len, const char* buf, NPBool file, void* notifyData)
{
    // This method is called when handling post data requests from the plugin.
    // NPAPI call methods: NPN_PostURL() and NPN_PostURLNotify()

	if (!m_document)
		return OpStatus::ERR;

    URL url = DetermineURL(type, url_name, TRUE);
    if (url.IsEmpty())
        return OpStatus::ERR;

    HandlePluginPostCallback *callback = OP_NEW(HandlePluginPostCallback, (this, type, url, len, !!file, notifyData));
    if (!callback ||
        (win_name && *win_name && OpStatus::IsMemoryError(callback->SetWindowName(win_name))) ||
		(buf && *buf && OpStatus::IsMemoryError(callback->SetBuf((unsigned char*)buf, len))))
    {
        OP_DELETE(callback);
        return OpStatus::ERR_NO_MEMORY;
    }
    WindowCommander *wc = m_document->GetWindow()->GetWindowCommander();
    wc->GetDocumentListener()->OnPluginPost(wc, callback);
    OP_BOOLEAN finished = callback->Execute();
    if (finished == OpBoolean::IS_TRUE)
    {
        OP_DELETE(callback);
        return OpBoolean::IS_TRUE;
    }
    else if (finished == OpBoolean::IS_FALSE)
        return OpBoolean::IS_TRUE;
    else
        return finished;
}

OP_STATUS Plugin::Redirect(URL_ID orig_url_id)
{
	// search for original url in plugin's list of streamed urls
	StreamCount_URL_Link* u = static_cast<StreamCount_URL_Link *>(url_list.First());
	while (u && u->url.Id() != orig_url_id)
		u = static_cast<StreamCount_URL_Link *>(u->Suc());

	if (u)
	{
		URL redir_url = u->url.GetAttribute(URL::KMovedToURL);
		if (redir_url.Id() != orig_url_id)
		{
			// original url must be replaced
			if (u->stream_count)
				u->stream_count--;
			if (u->stream_count == 0)
			{
				u->Out();
				OP_DELETE(u);
			}
			// search for redirected url
			u = static_cast<StreamCount_URL_Link *>(url_list.First());
			while (u && u->url.Id() != redir_url.Id())
				u = static_cast<StreamCount_URL_Link *>(u->Suc());
			// insert unknown/unstreamed redirected urls only
			if (u)
				u->stream_count++;
			else
			{
				u = OP_NEW(StreamCount_URL_Link, (redir_url));
				if (u)
					u->Into(&url_list);
				else
				{
					OP_DELETE(u);
					return OpStatus::ERR_NO_MEMORY;
				}
			}
		}
	}
	return OpStatus::OK;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
OP_STATUS Plugin::SetWindowPos(int x, int y)
{
	if (m_plugin_window && m_life_cycle_state == RUNNING)
	{
		VisualDevice* visual_device = m_document->GetVisualDevice();
		OpPoint scaled_pos(x, y);

#ifdef USE_PLUGIN_EVENT_API
		scaled_pos = visual_device->ScaleToScreen(scaled_pos);
		m_view_rect.x = scaled_pos.x;
		m_view_rect.y = scaled_pos.y;

		if (!IsDisplayAllowed())
		{
			m_pending_display = TRUE;
			return OpStatus::OK;
		}
#else
		win_x = x;
		win_y = y;
#endif // USE_PLUGIN_EVENT_API

		npwin.x = scaled_pos.x;
		npwin.y = scaled_pos.y;
		ConvertFromLocalToScreen(visual_device, npwin);

		// Set clip rect to visible region of plugin.
		OpRect clip = GetClipRect(scaled_pos.x, scaled_pos.y, npwin.width, npwin.height);
		npwin.clipRect.left = clip.x;
		npwin.clipRect.top = clip.y;
		npwin.clipRect.right = clip.Right();
		npwin.clipRect.bottom = clip.Bottom();

#if defined(USE_PLUGIN_EVENT_API)
		// Add offset of view for cases when plugin is inside iframe and scale to screen coordinates.
		int offset_x = 0, offset_y = 0;
		if (IsWindowless())
			visual_device->view->ConvertToContainer(offset_x, offset_y);

		int container_x = 0, container_y = 0;
#ifdef VEGA_OPPAINTER_SUPPORT
		static_cast<MDE_OpView*>(visual_device->view->GetOpView())->GetMDEWidget()->ConvertToScreen(container_x, container_y);
#endif
		OpRect scaled_rect(offset_x + scaled_pos.x, offset_y + scaled_pos.y, m_view_rect.width, m_view_rect.height);

		visual_device->ResizePluginWindow(m_plugin_window, scaled_rect.x, scaled_rect.y, scaled_rect.width, scaled_rect.height, TRUE, TRUE);
		m_plugin_adapter->SetupNPWindow(&npwin, scaled_rect, scaled_rect, m_view_rect, OpPoint(container_x,container_y), TRUE, IsTransparent());
		CallSetWindow();
#else

		OP_ASSERT(m_life_cycle_state != UNINITED);
		OP_ASSERT(m_life_cycle_state >= NEW_WITHOUT_WINDOW);
		if (OpNS4PluginHandler::GetHandler())
		{
			if (OpStatus::IsSuccess(((PluginHandler*)OpNS4PluginHandler::GetHandler())->PostPluginMessage(
									SETWINDOW,
									GetInstance(),
									GetID(),
									0,
									&npwin)))
			{
				SetLastPosted(SETWINDOW);
			}
		}
#endif // USE_PLUGIN_EVENT_API
	}

	return OpStatus::OK;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void Plugin::HideWindow()
{
	OP_ASSERT(m_document);
	if (m_plugin_window)
		m_document->GetVisualDevice()->ShowPluginWindow(m_plugin_window, FALSE);
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void Plugin::DetachWindow()
{
#ifdef USE_PLUGIN_EVENT_API
	/* Inform platforms that there is no longer anyone
	 * listening to plugin events for this plugin. */
	SetWindowListener(NULL);
	if (m_document && m_plugin_window && !m_plugin_window_detached)
	{
		m_document->GetVisualDevice()->DetachPluginWindow(m_plugin_window);
		m_plugin_window_detached = TRUE;
	}
#else // USE_PLUGIN_EVENT_API
	SetParentInputContext(NULL);
#endif // !USE_PLUGIN_EVENT_API
	m_document = NULL;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
URL Plugin::DetermineURL(BYTE type, const char* url_name, BOOL unique)
{
    OP_ASSERT(url_name);

    URL ret_url = URL();
    BOOL javascript_in_use = !op_strnicmp("javascript:", url_name, 11);

    if (!m_document)
        return ret_url; // return empty url

    URL base_url = GetBaseURL();

    /* determine url */

    char *rel_start =  javascript_in_use ? NULL : (char*) op_strchr(url_name, '#');
    if (rel_start)
        *rel_start++ = 0;

    unique = unique || javascript_in_use; // avoid cached javascript urls

	const char* szLFToRemove = (javascript_in_use && op_strchr(url_name, '\n')) ? "\n" : NULL;
	const char* szTABToRemove = (op_strchr(url_name, '\t')) ? "\t" : NULL;

	if (szLFToRemove || szTABToRemove)
	{   // remove LF characters from JS URL and TAB characters from URL
        char *tmp_url_name = NULL;
        if (OpStatus::IsError(SetStr(tmp_url_name, url_name)))
            return ret_url;

		if (szLFToRemove)
			StrFilterOutChars(tmp_url_name, szLFToRemove);
		if (szTABToRemove)
			StrFilterOutChars(tmp_url_name, szTABToRemove);

        ret_url = g_url_api->GetURL(base_url, tmp_url_name, rel_start, unique);

        SetStr(tmp_url_name, NULL);
    }
    else
    {
        ret_url = g_url_api->GetURL(base_url, url_name, rel_start, unique);
    }
    return ret_url;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
URL Plugin::GetBaseURL()
{
    URL base_url = URL();
	FramesDocument* frames_doc = m_document;
	if (frames_doc)
	{
		if (frames_doc->GetLogicalDocument() && frames_doc->GetLogicalDocument()->GetBaseURL())
			base_url = *(frames_doc->GetLogicalDocument()->GetBaseURL());

		if (base_url.IsEmpty())
			base_url = frames_doc->GetURL();
	}
#if defined(WEB_TURBO_MODE)
	// Plugins should not use Turbo
	if( base_url.GetAttribute(URL::KUsesTurbo) )
	{
		const OpStringC8 url_str = base_url.GetAttribute(URL::KName_With_Fragment_Username_Password_NOT_FOR_UI, URL::KNoRedirect);
		base_url = g_url_api->GetURL(url_str);
		base_url.SetAttribute(URL::KTurboBypassed, TRUE);
	}
#endif // WEB_TURBO_MODE

	return base_url;
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void Plugin::DeterminePlugin(BOOL& is_flash, BOOL& is_acrobat_reader)
{
	NPMIMEType mt = GetMimeType();
	if (mt)
	{
		/* Shockwave Flash? */
		if (!op_stricmp(mt, "application/x-shockwave-flash") ||
			!op_stricmp(mt, "application/futuresplash"))
		{
			is_flash = TRUE;
			return;
		}
		/* Acrobat Reader? */
		if (!op_stricmp(mt, "application/pdf"))
		{
			is_acrobat_reader = TRUE;
			return;
		}
#if defined(NS4P_WMP_STOP_STREAMING) || defined(NS4P_WMP_CONVERT_CODEBASE) || defined(NS4P_WMP_LOCAL_FILES)
		/* Windows Media Player? */
		if (!op_stricmp(mt, "application/x-ms-wmp") ||
			!op_stricmp(mt, "application/asx") ||
			!op_stricmp(mt, "video/x-ms-asf-plugin") ||
			!op_stricmp(mt, "video/x-ms-wm") ||
			!op_stricmp(mt, "audio/x-ms-wax") ||
			!op_stricmp(mt, "video/x-ms-wmv") ||
			!op_stricmp(mt, "video/x-ms-asf") ||
			!op_stricmp(mt, "audio/x-ms-wma") ||
			!op_stricmp(mt, "application/x-mplayer2") ||
			!op_stricmp(mt, "video/x-ms-wvx"))
		{
			m_is_wmp_mimetype = TRUE;
		}
#endif // NS4P_WMP_STOP_STREAMING || NS4P_WMP_CONVERT_CODEBASE || NS4P_WMP_LOCAL_FILES
	}
	return;
}

OP_STATUS Plugin::SetFailure(BOOL crashed, const OpMessageAddress& address)
{
#ifdef NS4P_COMPONENT_PLUGINS
	if (crashed)
	{
		if (m_document)
		{
			WindowCommander* commander = m_document->GetWindow()->GetWindowCommander();
			commander->GetDocumentListener()->OnUnexpectedComponentGone(
						commander, m_component_type, address, GetName());
		}

		if (m_window_listener)
			m_window_listener->OnCrash();
	}
#endif // NS4P_COMPONENT_PLUGINS

	m_is_failure = TRUE;
	RETURN_IF_ERROR(ScheduleDestruction());

	return OpStatus::OK;
}

#ifndef USE_PLUGIN_EVENT_API

OP_STATUS Plugin::SetWindow()
{
	OP_NEW_DBG("Plugin::SetWindow", "ns4p");
	OP_DBG((" ( instance: %p, IsWindowProtected: %d )", GetInstance(), IsWindowProtected()));
	BOOL send_set_window_message= TRUE;
	if (m_life_cycle_state == UNINITED || IsWindowProtected() || SameWindow(npwin))
		send_set_window_message = FALSE; // Plugin has only been initialized or NPP_HandleEvent() has not returned



	NPError ret = NPERR_NO_ERROR;
	OP_STATUS status = OpStatus::OK;
	if (send_set_window_message)
	{
		TRY
			ret = pluginfuncs->setwindow(GetInstance(),
							  GetNPWindow());
		CATCH_PLUGIN_EXCEPTION(ret=NPERR_GENERIC_ERROR;SetException();)
		// The plugin might have called back and changed all kinds of
		// states so be very careful with assumptions about the current
		// state from here on.
		OP_DBG(("Plugin [%d] SETWINDOW returned", GetID()));
		if (GetLifeCycleState() < WITH_WINDOW)
		{
			SetLifeCycleState(Plugin::WITH_WINDOW);
			if (PluginHandler* handler = static_cast<PluginHandler*>(OpNS4PluginHandler::GetHandler()))
			{
				if (OpStatus::IsError(handler->PostPluginMessage(SETREADYFORSTREAMING,
				                                                 GetInstance(),
				                                                 GetID())))
					status = OpStatus::ERR_NO_MEMORY;
			}
		}
		if (ret == NPERR_OUT_OF_MEMORY_ERROR)
			status = OpStatus::ERR_NO_MEMORY;

		if (OpStatus::IsSuccess(status) && !m_document)
			status = OpStatus::ERR;
	}

	if (PluginHandler* handler = static_cast<PluginHandler*>(OpNS4PluginHandler::GetHandler()))
		handler->Resume(GetDocument(), GetHtmlElement(), FALSE, TRUE);

	return status;
}

int16 Plugin::OldHandleEvent(void *event, BOOL lock)
{
	int16 ret = 0;

	if (!IsWindowProtected() && pluginfuncs->event)
	{
		SetWindowProtected(lock);

		BOOL saved_allow_nested_message_loop = g_pluginscriptdata->AllowNestedMessageLoop();
		g_pluginscriptdata->SetAllowNestedMessageLoop(FALSE);

		TRY
			ret = pluginfuncs->event(GetInstance(), event);
		CATCH_PLUGIN_EXCEPTION(ret=0; SetException();)

		g_pluginscriptdata->SetAllowNestedMessageLoop(saved_allow_nested_message_loop);

		SetWindowProtected(FALSE);
	}
	return ret;
}

// -----------------------------------------------------------
//
// DEPRECATED : (on if !USE_PLUGIN_EVENT_API)
//
// -----------------------------------------------------------

BOOL Plugin::OldHandleEvent(DOM_EventType dom_event, const OpPoint& point, int button_or_key, ShiftKeyState modifiers)
{
	OP_ASSERT(m_document);

#if defined(EPOC)
    BOOL handled = FALSE;
	switch (dom_event)
	{
		case ONFOCUS:
        case ONBLUR:
            {
            MOperaPlugin *epocPlugin = (MOperaPlugin*)npwin.window;
            epocPlugin->Control()->SetFocus(dom_event==ONFOCUS);
            handled=TRUE;
            break;
            }
        default:
            break;
    }
    return handled;
#else
	return FALSE;
#endif // MANY DIFFERENT PLATFORMS
}

// -----------------------------------------------------------
//
// DEPRECATED : (on if !USE_PLUGIN_EVENT_API)
//
// -----------------------------------------------------------

OP_STATUS Plugin::OldNew(FramesDocument* frames_doc, void* plugin_window, const uni_char *mimetype, uint16 mode, int16 argc, const uni_char *argn[], const uni_char *argv[], URL* url, BOOL embed_url_changed)
{
	m_document = frames_doc;
	OP_ASSERT(m_document);
	m_document_url = m_document->GetURL();

	// Plugin cached content should be deleted after the window, opened in privacy mode, is closed.
	if (m_document->GetWindow() && m_document->GetWindow()->GetPrivacyMode())
	{
		PluginLib *lib = g_pluginhandler->GetPluginLibHandler()->FindPluginLib(plugin_name);
		if (lib)
			RETURN_IF_ERROR(lib->AddURLIntoCleanupTaskList(url));
	}

	if (m_document->GetVisualDevice())
		SetParentInputContext(m_document->GetVisualDevice());

	m_plugin_window = (OpPluginWindow*) plugin_window;
	if (m_plugin_window->GetHandle())
		npwin.window = m_plugin_window->GetHandle();

	OP_STATUS stat = OpStatus::OK;
	RETURN_IF_ERROR(SetMimeType(mimetype));

	BOOL is_flash = FALSE;
	BOOL is_acrobat_reader = FALSE;
	DeterminePlugin(is_flash, is_acrobat_reader);

	m_mode =
#ifdef NS4P_FORCE_FLASH_WINDOWLESS
		is_flash ? NP_EMBED :
#endif // NS4P_FORCE_FLASH_WINDOWLESS
#ifdef NS4P_ACROBAT_CHANGE_MODE
		(is_acrobat_reader ? NP_FULL : mode);
#else
		mode;
#endif // NS4P_ACROBAT_CHANGE_MODE

	RETURN_IF_ERROR(AddParams(argc, argn, argv, url, embed_url_changed, is_flash));

	if (OpStatus::IsSuccess(stat))
	{
		if (GetLastPosted() != NEW &&
			m_life_cycle_state == UNINITED &&
			OpNS4PluginHandler::GetHandler())

			if (OpStatus::IsSuccess(stat = ((PluginHandler*)OpNS4PluginHandler::GetHandler())->PostPluginMessage(
									NEW,
									GetInstance(),
									GetID())))
				SetLastPosted(NEW);
	}

#ifdef NS4P_ALL_PLUGINS_ARE_WINDOWLESS
	SetWindowless(TRUE);
#endif // !NS4P_ALL_PLUGINS_ARE_WINDOWLESS

	return stat;
}

// -----------------------------------------------------------
//
// DEPRECATED : (on if !USE_PLUGIN_EVENT_API)
//
// -----------------------------------------------------------

OP_STATUS Plugin::OldSetWindow(int x, int y, unsigned int width, unsigned int height, BOOL show, const OpRect* rect)
{
	if (m_life_cycle_state == UNINITED || IsWindowProtected())
	{ // Plugin has only been initialized
		if (!windowless)
		{ // save the calculated layout values for later usage
			npwin.width = width;
			npwin.height = height;
			if (m_plugin_window)
				m_document->GetVisualDevice()->ResizePluginWindow(m_plugin_window, x, y, width, height, show, FALSE);
		}
		return OpStatus::OK; // NPP_New() has not been called yet or NPP_HandleEvent() has not returned
	}

	PluginHandler *plugin_handler = static_cast<PluginHandler*>(OpNS4PluginHandler::GetHandler());

	if (windowless)
	{
		if (!show)
			return OpStatus::OK;

		VisualDevice* vd = m_document->GetVisualDevice();
		if (vd
#ifdef _PRINT_SUPPORT_
			&& !vd->IsPrinter()
#endif // _PRINT_SUPPORT_
			)
		{
			npwin.x = win_x = x;
			npwin.y = win_y = y;
			npwin.width = width;
			npwin.height = height;

			ConvertFromLocalToScreen(vd, npwin);

			// Set clipRect to visible region of plugin.
			OpRect clip = GetClipRect(x,y,width,height);
			npwin.clipRect.left = clip.x;
			npwin.clipRect.top = clip.y;
			npwin.clipRect.right = clip.Right();
			npwin.clipRect.bottom = clip.Bottom();
			npwin.type = NPWindowTypeDrawable;

			if (plugin_handler)
			{
				if (m_life_cycle_state < RUNNING)
					if (OpStatus::IsSuccess(plugin_handler->PostPluginMessage(
												SETREADYFORSTREAMING,
												GetInstance(),
												GetID())))
						SetLastPosted(SETREADYFORSTREAMING);
			}
			SetWindow();
			return OpStatus::OK;
		}
	}

	int			doc_x = x + m_document->GetVisualDevice()->GetRenderingViewX();
	int			doc_y = y + m_document->GetVisualDevice()->GetRenderingViewY();
	if (win_x != doc_x || win_y != doc_y || npwin.width != width || npwin.height != height)
	{
		win_x = doc_x;
		win_y = doc_y;

		npwin.width = width;
		npwin.height = height;

		npwin.clipRect.left = 0;
		npwin.clipRect.top = 0;
		npwin.clipRect.right = width;
		npwin.clipRect.bottom = height;

		npwin.type = NPWindowTypeWindow;


		if (npwin.window)
			if (!IsInSynchronousLoop())
				m_document->GetVisualDevice()->ResizePluginWindow(m_plugin_window, x, y, width, height, show);
			else
				plugin_handler->PostPluginMessage(RESIZEMOVE, GetInstance(), GetID(), 0, &npwin);

		if (plugin_handler)
		{
			// Workaround for shockwave in bug CORE-26385. It seems to move it's window to whatever is passed as npwin.x, npwin.y
			npwin.x = x;
			npwin.y = y;
			ConvertFromLocalToScreen(m_document->GetVisualDevice(), npwin);

			if (OpStatus::IsSuccess(plugin_handler->PostPluginMessage(
									SETWINDOW,
									GetInstance(),
									GetID(),
									0,
									&npwin)))
			{
				SetLastPosted(SETWINDOW);
				if (m_life_cycle_state < RUNNING)
					if (OpStatus::IsSuccess(plugin_handler->PostPluginMessage(
												SETREADYFORSTREAMING,
												GetInstance(),
												GetID())))
						SetLastPosted(SETREADYFORSTREAMING);
			}
		}
	}
	return OpStatus::OK;
}

// -----------------------------------------------------------
//
// DEPRECATED : (on if !USE_PLUGIN_EVENT_API)
//
// -----------------------------------------------------------

void Plugin::OldPrint(const VisualDevice* vd, const int x, const int y, const unsigned int width, const unsigned int height)
{
	NPPrint npprint;
	npprint.mode = NP_EMBED;

	NPWindow &tmp_npwindow = npprint.print.embedPrint.window;
	tmp_npwindow.x = x;
	tmp_npwindow.y = y;
	tmp_npwindow.width = width;
	tmp_npwindow.height = height;
	tmp_npwindow.window = 0;
	tmp_npwindow.clipRect.top = 0;
	tmp_npwindow.clipRect.left = 0;
	tmp_npwindow.clipRect.bottom = 0;
	tmp_npwindow.clipRect.right = 0;
	tmp_npwindow.type = npwin.type;

	TRY
		pluginfuncs->print(GetInstance(), &npprint);
	CATCH_PLUGIN_EXCEPTION(SetException();)
}

// -----------------------------------------------------------
//
// DEPRECATED : (on if !USE_PLUGIN_EVENT_API)
//
// -----------------------------------------------------------

OP_STATUS Plugin::OldDestroy()
{
	OP_ASSERT(!m_document);


	OpStatus::Ignore(DestroyAllStreams());

	NPError ret = NPERR_NO_ERROR;

	if (GetLifeCycleState() > UNINITED && !IsFailure()) // Make sure that NPP_New() has been called and no exception was thrown
	{
		PushLockEnabledState();
		TRY
			ret = pluginfuncs->destroy(GetInstance(), &saved);
		CATCH_PLUGIN_EXCEPTION(ret = NPERR_GENERIC_ERROR;SetException();)
		PopLockEnabledState();
	}

	if (ret == NPERR_OUT_OF_MEMORY_ERROR)
		return OpStatus::ERR_NO_MEMORY;
	else if (ret != NPERR_NO_ERROR)
		return OpStatus::ERR;
	else
		return OpStatus::OK;
}

// -----------------------------------------------------------
//
// DEPRECATED : (on if !USE_PLUGIN_EVENT_API)
//
// -----------------------------------------------------------

void Plugin::OldPluginDestructor()
{
    PluginStream* p = (PluginStream *)stream_list.First();

    while (p)
    {
    	PluginStream* ps = p->Suc();

    	p->Out();
      	OP_DELETE(p);
      	p = ps;
    }

	StreamCount_URL_Link* u = static_cast<StreamCount_URL_Link *>(url_list.First());
	while (u)
	{
		StreamCount_URL_Link* us = static_cast<StreamCount_URL_Link *>(u->Suc());

		u->Out();
		OP_DELETE(u);
		u=us;
	};

	if (ID && g_pluginhandler && g_pluginhandler->GetPluginLibHandler())
		g_pluginhandler->GetPluginLibHandler()->DeleteLib(plugin_name);


	OP_DELETEA(plugin_name);
	if (m_mimetype)
		OP_DELETEA(m_mimetype);

	while (m_popup_stack)
	{
		PopupStack* tmp = m_popup_stack->Pred();
		OP_DELETE(m_popup_stack);
		m_popup_stack = tmp;
	}

	while (m_lock_stack)
	{
		LockStack* tmp = m_lock_stack->Pred();
		OP_DELETE(m_lock_stack);
		m_lock_stack = tmp;
	}

	if (m_argc && m_args8)
	{
		for (int c=0; c<m_argc; c++)
		{
			OP_DELETEA(m_args8[c]);
			OP_DELETEA(m_args8[c+m_argc]);
		}
		OP_DELETEA(m_args8);
	}
}

#endif // !USE_PLUGIN_EVENT_API

void Plugin::DeleteStream(PluginStream* ps)
{
	int id = ps->UrlId();
	ps->Out();
	OP_DELETE(ps);
	RemoveLinkedUrl(id);
}

#ifdef NS4P_WINDOWLESS_MACROMEDIA_WORKAROUND
OP_STATUS Plugin::DetermineSpoofing()
{
	// spoof ua if Flash version is old
	m_spoof_ua = FALSE;
	PluginViewer* pv = g_plugin_viewers->FindPluginViewerByPath(GetName());
	if (pv)
	{
		OpString plugin_description;
		RETURN_IF_ERROR(pv->GetDescription(plugin_description));
		if (plugin_description.HasContent())
		{
			// find plugin_description's version number, if any, and check if it's lower than 10
			uni_char* tmp;
			for (tmp=plugin_description.CStr(); !uni_isdigit(*tmp); tmp++)
				if (!*tmp)
					return OpStatus::OK;

			int version = uni_strtol(tmp, NULL, 10);
			m_spoof_ua = version < 10;
		}
	}
	g_pluginhandler->SetFlashSpoofing(m_spoof_ua); // NPN_UserAgent() might be called without plugin instance
	return OpStatus::OK;
}
#endif // NS4P_WINDOWLESS_MACROMEDIA_WORKAROUND

BOOL Plugin::IsEqualUrlAlreadyStreaming(PluginStream* ps)
{
	PluginStream* p = static_cast<PluginStream *>(stream_list.First());
	while (p && p != ps)
	{
		if (p->UrlId(TRUE) == ps->UrlId(TRUE))
		{
			p->WriteReady(this); // don't delay current stream without ping of previous stream
			return TRUE;
		}
		else
			p = p->Suc();
	}
	return FALSE;
}

void Plugin::InvalidateInternal(OpRect *invalidRect, double currentRuntimeMs /*= 0.*/)
{
#if NS4P_INVALIDATE_INTERVAL > 0
	m_last_invalidate_time = currentRuntimeMs!=0. ? currentRuntimeMs : g_op_time_info->GetRuntimeMS();
#endif // NS4P_INVALIDATE_INTERVAL > 0
	VisualDevice* vis_dev = NULL;
	if (GetDocument())
		vis_dev = m_document->GetVisualDevice();

	if (!vis_dev)
		return;

	if (IsWindowless() && vis_dev->GetView())
	{
		OpRect rect(invalidRect->x + GetWindowX(), invalidRect->y + GetWindowY(),
			invalidRect->width, invalidRect->height);

		if (GetPluginWindow())
		{
			vis_dev->GetView()->Invalidate(rect);
			return;
		}
		rect = vis_dev->ScaleToDoc(rect);
		vis_dev->Update(rect.x + vis_dev->GetRenderingViewX(), rect.y + vis_dev->GetRenderingViewY(), rect.width, rect.height);
	}
#ifdef NS4P_SILVERLIGHT_WORKAROUND
	else if (GetPluginWindow())
		GetPluginWindow()->InvalidateWindowed(vis_dev->ScaleToScreen(*invalidRect));
#endif // NS4P_SILVERLIGHT_WORKAROUND
}

void Plugin::Invalidate(NPRect *invalidRect)
{
	if (m_core_animation)
		return;

	OpRect rect;
	if (invalidRect)
	{
		rect.x = invalidRect->left;
		rect.y = invalidRect->top;
		rect.width = invalidRect->right-invalidRect->left;
		rect.height = invalidRect->bottom-invalidRect->top;
	}
	else
	{
		rect.x = 0;
		rect.y = 0;
		rect.width = npwin.width;
		rect.height = npwin.height;
	}
#if NS4P_INVALIDATE_INTERVAL > 0
	if (m_invalidate_timer_set)
		m_invalid_rect.UnionWith(rect);
	else if (!m_is_flash_video && m_last_invalidate_time)
	{
		double currentRuntimeMs = g_op_time_info->GetRuntimeMS();
		int timeFromLastInvalidate = static_cast<int>(currentRuntimeMs-m_last_invalidate_time);
		if (timeFromLastInvalidate > NS4P_INVALIDATE_INTERVAL)
			InvalidateInternal(&rect, currentRuntimeMs);
		else if (m_document)
		{
			m_invalid_rect = rect;
			m_invalidate_timer->Start(NS4P_INVALIDATE_INTERVAL - timeFromLastInvalidate);
			m_invalidate_timer_set = TRUE;
		}
	}
	else
#endif
		InvalidateInternal(&rect);
}

#if NS4P_INVALIDATE_INTERVAL > 0
void Plugin::OnTimeOut(OpTimer* timer)
{
	m_invalidate_timer_set = FALSE;
	InvalidateInternal(&m_invalid_rect);
}
#endif // NS4P_INVALIDATE_INTERVAL > 0

#if NS4P_STARVE_WHILE_LOADING_RATE>0

BOOL Plugin::IsStarving()
{
	if (m_main_stream && m_document_url.Id() == m_main_stream->GetURL().Id())
		return FALSE;

	FramesDocument *fd = m_document;
	while (fd->GetParentDoc())
		fd = fd->GetParentDoc();
	if (fd->GetLogicalDocument() && fd->GetLogicalDocument()->IsLoaded())
		return FALSE;
	else
	{
		m_starvingStep = (m_starvingStep + 1) % (NS4P_STARVE_WHILE_LOADING_RATE+1);
		return m_starvingStep;
	}
}

#endif // NS4P_STARVE_WHILE_LOADING_RATE

/*	virtual */
BOOL Plugin::IsPluginFullyInited()
{
	return m_life_cycle_state >= NEW_WITHOUT_WINDOW;
}

OP_STATUS Plugin::AddExtraParams(int &count, BOOL add_restricted_allowscriptaccess, BOOL add_baseurl, OpString baseurl, BOOL add_srcurl, OpString srcurl)
{
	OpString tmpstr;

	/* allocate and copy the special attributes before the PARAM attributes, following Mozilla */
	if (add_restricted_allowscriptaccess)
	{
		RETURN_IF_ERROR(tmpstr.Set(UNI_L("ALLOWSCRIPTACCESS")));
		RETURN_IF_ERROR(tmpstr.UTF8Alloc(&m_args8[count]));
		RETURN_IF_ERROR(tmpstr.Set(UNI_L("SAMEDOMAIN")));
		RETURN_IF_ERROR(tmpstr.UTF8Alloc(&m_args8[m_argc+count]));
		count++;
	}
	if (add_baseurl)
	{
		OP_ASSERT(baseurl.HasContent());
		RETURN_IF_ERROR(tmpstr.Set(UNI_L("BASEURL")));
		RETURN_IF_ERROR(tmpstr.UTF8Alloc(&m_args8[count]));
		RETURN_IF_ERROR(tmpstr.Set(baseurl.CStr()));
		RETURN_IF_ERROR(tmpstr.UTF8Alloc(&m_args8[m_argc+count]));
		count++;
	}
	if (add_srcurl && srcurl.HasContent())
	{
		RETURN_IF_ERROR(tmpstr.Set(UNI_L("SRC")));
		RETURN_IF_ERROR(tmpstr.UTF8Alloc(&m_args8[count]));
		RETURN_IF_ERROR(tmpstr.Set(srcurl.CStr()));
		RETURN_IF_ERROR(tmpstr.UTF8Alloc(&m_args8[m_argc+count]));
		count++;
	}

#ifdef NS4P_SUPPORT_PROXY_PLUGIN
	RETURN_IF_ERROR(tmpstr.Set(UNI_L("plugin_lib")));
	RETURN_IF_ERROR(tmpstr.UTF8Alloc(&m_args8[count]));
	// TODO : should use a pi interface to translate the string to
	//        a local encoded filename - depends on bug : 295809
	RETURN_IF_ERROR(tmpstr.Set(GetName()));
	RETURN_IF_ERROR(tmpstr.UTF8Alloc(&m_args8[m_argc+count]));
	count++;
#endif // NS4P_SUPPORT_PROXY_PLUGIN

	return OpStatus::OK;
}

OP_STATUS Plugin::AddBinding(const ES_Object* internal, OpNPObject* object)
{
	OP_ASSERT(!m_internal_objects.Contains(internal));

	return m_internal_objects.Add(internal, object);
}

void Plugin::RemoveBinding(const ES_Object* internal)
{
	OpNPObject* ignored;
	m_internal_objects.Remove(internal, &ignored);
}

void Plugin::InvalidateObject(OpNPObject* object)
{
	if (object == m_scriptable_object)
		m_scriptable_object = 0;
}

OpNPObject* Plugin::FindObject(const ES_Object* internal)
{
	OpNPObject* object;
	if (m_internal_objects.GetData(internal, &object) == OpStatus::OK)
		return object;

	return NULL;
}

OP_STATUS Plugin::GetOrigin(char** ret_origin)
{
	if (!m_document)
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	OpString8 origin;
	RETURN_IF_LEAVE(m_document->GetURL().GetAttributeL(URL::KProtocolName, origin, FALSE));
	RETURN_IF_ERROR(origin.Append("://"));

	OpString8 host;
	RETURN_IF_LEAVE(m_document->GetURL().GetAttributeL(URL::KHostNameAndPort_L, host, FALSE));
	RETURN_IF_ERROR(origin.Append(host));

	*ret_origin = static_cast<char*>(NPN_MemAlloc(origin.Length() + 1));
	RETURN_OOM_IF_NULL(*ret_origin);
	op_strncpy(*ret_origin, origin.CStr(), origin.Length() + 1);

	return OpStatus::OK;
}

#endif // _PLUGIN_SUPPORT_
