/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef _PLUGIN_SUPPORT_

#include "modules/ns4plugins/src/pluginhandler.h"
#include "modules/ns4plugins/src/plugin.h"
#include "modules/ns4plugins/src/pluginlibhandler.h"
#include "modules/ns4plugins/src/pluginlib.h"
#include "modules/ns4plugins/opns4plugin.h"
#include "modules/ns4plugins/src/pluginstream.h"
#include "modules/doc/frm_doc.h"
#include "modules/pi/OpThreadTools.h"
#include "modules/pi/OpLocale.h"

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/cpuusagetracker/cpuusagetrackeractivator.h"

#ifdef _MACINTOSH_
# include "platforms/mac/pi/plugins/MacOpPluginWindow.h"
#endif // _MACINTOSH_

#include "modules/dochand/win.h"
#include "modules/viewers/viewers.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/layout/box/box.h"
#include "modules/layout/content/content.h"

#include "modules/ns4plugins/src/pluginexceptions.h"
#include "modules/prefs/prefsmanager/collections/pc_app.h"

PluginHandler::PluginHandler()
	: plugincount(0)
	, pluginlibhandler(NULL)
	, last_handled_msg(NULL)
#ifdef NS4P_QUICKTIME_CRASH_WORKAROUND
	, m_number_of_active_quicktime_plugins(0)
#endif // NS4P_QUICKTIME_CRASH_WORKAROUND
#ifdef NS4P_WINDOWLESS_MACROMEDIA_WORKAROUND
	, m_spoof_ua(FALSE)
#endif // NS4P_WINDOWLESS_MACROMEDIA_WORKAROUND
	, m_nextTimerID(1)
#ifdef NS4P_COMPONENT_PLUGINS
	, m_scripting_context(0)
#endif // NS4P_COMPONENT_PLUGINS
{
	g_main_message_handler->SetCallBack(this, MSG_PLUGIN_CALL_ASYNC, 0);
}

PluginHandler::~PluginHandler()
{
	// clean up the remaining plugins before destruction of the plugin handler
	Plugin* plugin = NULL;
	while ((plugin = (Plugin*)plugin_list.First()) != NULL)
	{
		plugin->Destroy();
		OP_DELETE(plugin);
		plugin = NULL;
	}

	PluginHandlerRestartObject* p = NULL;
	while ((p = restart_object_list.First()) != NULL)
	{
		p->Out();
		OP_DELETE(p);
		p = NULL;
	}
	if (pluginlibhandler)
		OP_DELETE(pluginlibhandler);
	pluginlibhandler = NULL;
}

OP_STATUS PluginHandler::Init()
{
	OP_ASSERT(pluginlibhandler == NULL);
	pluginlibhandler = OP_NEW(PluginLibHandler, ());
	return pluginlibhandler ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

#ifdef USE_PLUGIN_EVENT_API
OpNS4Plugin* PluginHandler::New(const uni_char *plugin_dll, OpComponentType component_type, FramesDocument* frames_doc, OpStringC &mimetype, uint16 mode, HTML_Element* htm_elm, URL* url, BOOL embed_url_changed)
#else
OpNS4Plugin* PluginHandler::New(const uni_char *plugin_dll, OpComponentType component_type, FramesDocument* frames_doc, void* embed, OpStringC &mimetype, uint16 mode, int16 argc, const uni_char *argn[], const uni_char *argv[], HTML_Element* htm_elm, URL* url, BOOL embed_url_changed)
#endif // USE_PLUGIN_EVENT_API
{
#ifdef NS4P_QUICKTIME_CRASH_WORKAROUND
	if ((mimetype.CompareI("audio/wav") == 0 ||
		 mimetype.CompareI("audio/x-wav") == 0 ||
		 mimetype.CompareI("video/quicktime") == 0 ||
		 mimetype.CompareI("application/sdp") == 0 ||
		 mimetype.CompareI("application/x-sdp") == 0 ||
		 mimetype.CompareI("application/x-rtsp") == 0 ||
		 mimetype.CompareI("video/flc") == 0))
	{
		if (m_number_of_active_quicktime_plugins < NS4P_DEFAULT_MAX_NUMBER_QUICKTIME_PLUGINS)
			m_number_of_active_quicktime_plugins++;
		else
			return NULL;
	}
#endif // NS4P_QUICKTIME_CRASH_WORKAROUND

#if defined(NS4P_FLASH_LIMITATION_TOTAL) || defined(NS4P_FLASH_LIMITATION_PER_PAGE)
	if ((mimetype.CompareI("application/x-shockwave-flash") == 0 ||
		 mimetype.CompareI("application/futuresplash") == 0))
	{
		int flash_in_total = 1;
		int flash_per_document = 1;
		for (Plugin* plug = (Plugin*)plugin_list.First(); plug; plug = plug->Suc())
		{
			NPMIMEType mt = plug->GetMimeType();
			if (mt &&
				(!op_stricmp(mt, "application/x-shockwave-flash") ||
				!op_stricmp(mt, "application/futuresplash")))
			{
				flash_in_total++;
				if (plug->GetDocument() == frames_doc)
					flash_per_document++;
			}
		}
#ifdef NS4P_FLASH_LIMITATION_TOTAL
		if (flash_in_total > NS4P_MAX_FLASH_IN_TOTAL)
			return NULL;
#endif // NS4P_FLASH_LIMITATION_TOTAL
#ifdef NS4P_FLASH_LIMITATION_PER_PAGE
		if (flash_per_document > NS4P_MAX_FLASH_PER_DOCUMENT)
			return NULL;
#endif // NS4P_FLASH_LIMITATION_PER_PAGE
	}
#endif // defined(NS4P_FLASH_LIMITATION_TOTAL) || defined(NS4P_FLASH_LIMITATION_PER_PAGE)

	Plugin* plug = OP_NEW(Plugin, ());
	if (!plug)
		return NULL;

	OP_STATUS ret = plug->Create(plugin_dll, component_type, ++plugincount);

	if (OpStatus::IsError(ret))
	{
		OP_DELETE(plug);
		return NULL;
	}

	if (!plug->GetID())
	{
		OP_DELETE(plug);
		return NULL;
	}
	else
	{
		plug->Into(&plugin_list);
		plug->SetHtmlElement(htm_elm);
		// Calling the virtual method of the ElementRef class.
		plug->SetElm(htm_elm);

#ifdef USE_PLUGIN_EVENT_API
		if (OpStatus::IsError(plug->New(frames_doc, mimetype.CStr(), mode, url, embed_url_changed)))
#else
		if (OpStatus::IsError(plug->New(frames_doc, embed, mimetype.CStr(), mode, argc, argn, argv, url, embed_url_changed)))
#endif // USE_PLUGIN_EVENT_API
		{
			OpStatus::Ignore(plug->Destroy());
			OP_DELETE(plug);
			return NULL;
		}

		return plug;
	}
}

void PluginHandler::Destroy(OpNS4Plugin* plugin)
{
	OP_NEW_DBG("PluginHandler::Destroy", "ns4p");

	Plugin* plug = (Plugin*)plugin;
	if (plug)
	{
		OP_DBG(("Plugin [%d]", plug->GetID()));

#ifdef NS4P_QUICKTIME_CRASH_WORKAROUND
		NPMIMEType mt = plug->GetMimeType();
		if (mt &&
			(!op_stricmp(mt, "audio/wav") ||
			!op_stricmp(mt, "audio/x-wav") ||
			!op_stricmp(mt, "video/quicktime") ||
			!op_stricmp(mt, "application/sdp") ||
			!op_stricmp(mt, "application/x-sdp") ||
			!op_stricmp(mt, "application/x-rtsp") ||
			!op_stricmp(mt, "video/flc")))
		{
			m_number_of_active_quicktime_plugins--;
		}
#endif // NS4P_QUICKTIME_CRASH_WORKAROUND

		plug->ScheduleDestruction();
	}
}

Plugin* PluginHandler::FindPlugin(NPP instance, BOOL return_deleted)
{
	if (instance)
	{
		// First check if one of the active plugins matches the instance
		Plugin *p = static_cast<Plugin*>(plugin_list.First());
		while (p)
		{
		   if (p->GetInstance() == instance)
			   return p;
		   p = p->Suc();
		}
		// If this method is called from PluginHandler::HandleMessage (return_deleted is true)
		// it's not necessary to check instance's ndata since instance will never change.
		// It's dangerous to do it because the plugin might have been deleted
		// and the instance pointer no longer points to a valid instance.
		if (!return_deleted)
		{
			// Then check if one of the active plugins matches the instance's ndata, Shockwave Player requires this
			p = static_cast<Plugin*>(plugin_list.First());
			while (p)
			{
			   if (p->GetInstance()->ndata == instance->ndata)
				   return p;
			   p = p->Suc();
			}
		}
	}
	return 0;
}

OpNS4Plugin* PluginHandler::FindOpNS4Plugin(OpPluginWindow* oppw, BOOL unused)
{
	if (oppw)
	{
		// First check if one of the active plugins matches the instance
		Plugin* p = static_cast<Plugin*>(plugin_list.First());
		while (p)
		{
			if (oppw == p->GetPluginWindow())
				return p;
			p = p->Suc();
		}
	}
	return 0;
}

FramesDocument*	PluginHandler::GetScriptablePluginDocument(NPP instance, Plugin *&plugin)
{
	plugin = FindPlugin(instance);
	if (plugin && plugin->GetLifeCycleState() >= Plugin::PRE_NEW)
	{
		return plugin->GetDocument();
	}
	return 0;
}

ES_Object* PluginHandler::GetScriptablePluginDOMElement(NPP instance)
{
	Plugin *plugin = FindPlugin(instance);
	return plugin ? plugin->GetDOMElement() : 0;
}

#ifdef _PRINT_SUPPORT_
void PluginHandler::Print(const VisualDevice* vd, const uni_char* src_url, const int x, const int y, const unsigned int width, const unsigned int height)
{
	for (Plugin *plug = (Plugin*)plugin_list.First(); plug; plug = plug->Suc())
		if (plug->HaveSameSourceURL(src_url))
			plug->Print(vd, x, y, width, height);
}
#endif // _PRINT_SUPPORT_

/* virtual */ void PluginHandler::HandleCallback(OpMessage msg, MH_PARAM_1 wParam, MH_PARAM_2 lParam)
{
	RAISE_AND_RETURN_VOID_IF_ERROR(HandleMessage(msg, wParam, lParam));
}

OP_STATUS PluginHandler::HandleMessage(OpMessage message_id, MH_PARAM_1 wParam, MH_PARAM_2 lParam)
{
	OP_NEW_DBG("PluginHandler::HandleMessage", "ns4p");

	OP_STATUS stat = OpStatus::OK;

	if (message_id == MSG_PLUGIN_CALL_ASYNC)
	{
		NPP instance = (NPP)wParam;
		PluginAsyncCall* call = (PluginAsyncCall*)lParam;
		Plugin* plugin = FindPlugin(instance);
		TRACK_CPU_PER_TAB((plugin && plugin->GetDocument()) ? plugin->GetDocument()->GetWindow()->GetCPUUsageTracker() : NULL);
		if (plugin)
		{
			TRY
			call->function(call->userdata);
			CATCH_PLUGIN_EXCEPTION(plugin->SetException();)
		}
		g_thread_tools->Free(call);
		return OpStatus::OK;
	}

	OP_ASSERT(message_id == MSG_PLUGIN_CALL);
	OP_ASSERT(wParam);
	OP_ASSERT(!lParam);

	if (message_id == MSG_PLUGIN_CALL)
	{
		PluginMsgStruct *call = reinterpret_cast<PluginMsgStruct*>(wParam);
		Plugin* plugin = FindPlugin(call->id.instance, TRUE);
		TRACK_CPU_PER_TAB((plugin && plugin->GetDocument()) ? plugin->GetDocument()->GetWindow()->GetCPUUsageTracker() : NULL);
		if (plugin)
		{
			// check if exception handling is needed or if the plugin was deleted while locked
			if (plugin->IsFailure())
			{
				HandleFailure();
				if (call->msg_type != DESTROY)
					plugin = NULL; // ignore further handling of all other messages
			}
		}
		if (plugin && plugin->GetID() == call->id.pluginid)
		{
			if (plugin->GetLockEnabledState() || plugin->IsWindowProtected())
			{
				unsigned int delay_ms = NS4P_DEFAULT_POST_DELAY;
				OP_DBG(("Plugin [%d] msg %d reposted (delay=%dms)", call->id.pluginid, call->msg_type, delay_ms));
				g_main_message_handler->PostMessage(MSG_PLUGIN_CALL, wParam, 0, delay_ms);
				return OpStatus::OK;
			}
			NPError ret = NPERR_NO_ERROR;
			last_handled_msg = call;
			switch (call->msg_type)
			{
				case INIT:
				{
#ifndef STATIC_NS4PLUGINS
					PluginLib *lib = pluginlibhandler ? pluginlibhandler->FindPluginLib(plugin->GetName()) : NULL;
					if (lib && !lib->Initialized())
					{
						plugin->PushLockEnabledState();
						OP_DBG(("Plugin [%d] INIT", call->id.pluginid));
						ret = lib->Init();
						OP_DBG(("Plugin [%d] INIT returned", call->id.pluginid));
						plugin->PopLockEnabledState();
						if (ret != NPERR_NO_ERROR)
							stat = plugin->SetFailure(FALSE);
					}
#endif // !STATIC_NS4PLUGINS
					break;
				}
				case NEW:
				{
					FramesDocument* frames_doc = plugin->GetDocument();
					PluginLib* lib = pluginlibhandler->FindPluginLib(plugin->GetName());
					if (!lib || !lib->Initialized())
						stat = plugin->SetFailure(FALSE);
					else
#ifdef STATIC_NS4PLUGINS
					if (!plugin->GetPluginfuncs()->newp) // the static plugin was not initialized properly
						stat = plugin->SetFailure(FALSE);
					else
#endif // STATIC_NS4PLUGINS
                    if (frames_doc)
					{
						plugin->PushLockEnabledState();
						OP_DBG(("Plugin [%d] NEW", call->id.pluginid));
						NPSavedData* saved = plugin->GetSaved();
						char** args8 = plugin->GetArgs8();
						int16 argc = plugin->GetArgc();
						plugin->SetLifeCycleState(Plugin::PRE_NEW);
						TRY
						ret = plugin->GetPluginfuncs()->newp(		plugin->GetMimeType(),
												call->id.instance,
												plugin->GetMode(),
												argc,
												args8,
												&args8[argc],
												saved);
						CATCH_PLUGIN_EXCEPTION(ret = NPERR_GENERIC_ERROR;plugin->SetException();)
						OP_DBG(("Plugin [%d] NEW returned %d (%s)", call->id.pluginid, ret, ret ? "failure" : "success"));
						plugin->PopLockEnabledState();
						if ((ret == NPERR_NO_ERROR
#ifdef NS4P_IGNORE_LOAD_ERROR
							|| ret == NPERR_MODULE_LOAD_FAILED_ERROR
#endif // NS4P_IGNORE_LOAD_ERROR
							))
						{
							// NOTE: CallNPP_NewProc might have run a nested message
							//       loop and everything might have changed. The only
							//       thing we know that we have is the Plugin object
							//       and some things it referred to.
							plugin->SetLifeCycleState(Plugin::NEW_WITHOUT_WINDOW);

#ifdef USE_PLUGIN_EVENT_API
							if (plugin->IsDisplayAllowed())
								plugin->HandlePendingDisplay();
#else
							if (!plugin->IsWindowless())
								plugin->SetWindow();
#endif // USE_PLUGIN_EVENT_API

							plugin->SetSaved(saved);

							// As initialised as it will be for now, resume scripts
							Resume(frames_doc, plugin->GetHtmlElement(), FALSE, TRUE);
							if (plugin->GetDocument() && plugin->GetHtmlElement())
								plugin->GetHtmlElement()->MarkDirty(frames_doc);
						}
						else
						{
							// If any scripts were waiting for the plugin, tell them to stop waiting, the plugin is a dead fish
							Resume(frames_doc, plugin->GetHtmlElement(), TRUE, TRUE);
							stat = plugin->SetFailure(FALSE);
						}
					}
					break;
				}
				case DESTROY:
				{
					OP_ASSERT(g_in_synchronous_loop >= 0);
					if (g_in_synchronous_loop || plugin->GetContextMenuActive())
					{
						// One of the plugins is in nested message loop and we don't dare destroying plugins
						// during that time (see DSK-312375). Destroying plugin that is in nested message loop
						// would be even worse so this branch caters for both cases.
						// Instead we're going to try destroying again in NS4P_DEFAULT_POST_DELAY milliseconds,
						// hoping that we're unnested by then. This is kind of a nasty busy loop because we
						// don't really know when we'll be able to unnest.
						OP_DBG(("Plugin [%d] DESTROY reposted because of synchronous loop", call->id.pluginid));
						unsigned long delay_ms = NS4P_DEFAULT_POST_DELAY;
						g_main_message_handler->PostDelayedMessage(MSG_PLUGIN_CALL, wParam, 0, delay_ms);
						return stat;
					}
					else
					{
						plugin->Destroy();
						OP_DELETE(plugin);
						plugin = NULL;
					}
					break;
				}
				case RESIZEMOVE:
				{
					if (plugin->IsInSynchronousLoop())
					{
						OP_DBG(("Plugin [%d] RESIZEMOVE reposted because of synchronous loop", call->id.pluginid));
						g_main_message_handler->PostMessage(message_id, wParam, lParam);
						return stat;
					}
					else
					{
						OP_DBG(("Plugin [%d] RESIZEMOVE", call->id.pluginid));

						FramesDocument* frames_doc = plugin->GetDocument();
						if (frames_doc && plugin->GetHtmlElement())
							plugin->GetHtmlElement()->MarkDirty(frames_doc);
					}
					break;
				}
				case SETWINDOW:
				{
					if (plugin->IsInSynchronousLoop())
						OP_DBG(("Plugin [%d] SETWINDOW ignored because plugin is in synchronous script handling", call->id.pluginid));
					else if (plugin->GetLifeCycleState() != Plugin::FAILED_OR_TERMINATED)
					{
						OP_DBG(("Plugin [%d] SETWINDOW", call->id.pluginid));

#ifdef USE_PLUGIN_EVENT_API
						// A script waiting for plugin init might have waited all up to the
						// first SetWindow so now we need to resume it.
						if (plugin->GetDocument() && plugin->GetHtmlElement())
							Resume(plugin->GetDocument(), plugin->GetHtmlElement(), FALSE, TRUE);
#else
						plugin->PushLockEnabledState();
						stat = plugin->SetWindow();
						plugin->PopLockEnabledState();
#endif // USE_PLUGIN_EVENT_API
					}
					break;
				}
				case SETREADYFORSTREAMING:
				{
					OP_DBG(("Plugin [%d] SETREADYFORSTREAMING", call->id.pluginid));

					if (plugin->GetLifeCycleState() != Plugin::FAILED_OR_TERMINATED)
					{
						if (plugin->GetLifeCycleState() < Plugin::RUNNING)
						{
							if (plugin->IsWindowless() &&
								plugin->GetLifeCycleState() == Plugin::NEW_WITHOUT_WINDOW)
							{
								// Windowless plugins don't need SetWindow
								// to start streaming so skip past that state
								plugin->SetLifeCycleState(Plugin::WITH_WINDOW);
							}
							plugin->SetLifeCycleState(Plugin::RUNNING);
						}
#ifdef USE_PLUGIN_EVENT_API
						if (plugin->IsDisplayAllowed())
							plugin->HandlePendingDisplay();
#endif // USE_PLUGIN_EVENT_API
					}
					break;
				}
				case EMBEDSTREAMFAILED:
				case NEWSTREAM:
				case WRITEREADY:
				case WRITE:
				case STREAMASFILE:
				case DESTROYSTREAM:
				case URLNOTIFY:
				{
					if (plugin->GetLifeCycleState() <= Plugin::UNINITED)
						break;

					if (PluginStream* ps = plugin->FindStream(call->id.streamid))
					{
						switch (call->msg_type)
						{
							case EMBEDSTREAMFAILED:
							{
								plugin->PushLockEnabledState();
								OP_STATUS plugin_wants_all_streams = plugin->GetPluginWantsAllNetworkStreams();
								plugin->PopLockEnabledState();
								if (OpStatus::IsError(plugin_wants_all_streams))
									stat = plugin_wants_all_streams;
								else if (plugin_wants_all_streams == OpBoolean::IS_FALSE)
								{
									ps->SetFinished();
									ps->SetReason(NPRES_NETWORK_ERR);
									if (ps->GetNotify())
										stat = ps->Notify(plugin);
								}
								else
								{
									ps->SetLastCalled(EMBEDSTREAMFAILED);
									stat = ps->New(plugin, NULL, NULL, 0);
								}
								break;
							}
							case NEWSTREAM:
							{
								// Check if the streaming should be delayed
								if (!ps->IsJSUrl() && // do not delay a javascript url
									(plugin->GetLifeCycleState() < Plugin::RUNNING || // check if the plugin is ready for streaming
									  plugin->IsEqualUrlAlreadyStreaming(ps))) // check if an equal plugin stream is already streaming (first in list, stream first)
								{
									OP_DBG(("Plugin [%d] stream [%d] delay NEWSTREAM", call->id.pluginid, call->id.streamid));
									// If the plug-in isn't ready for streaming yet, wait n/1000th of a second and try again.
									g_main_message_handler->RemoveDelayedMessage(message_id, wParam, lParam);
#ifdef NS4P_POST_DELAYED_NEWSTREAM
                                    g_main_message_handler->PostDelayedMessage(message_id, wParam, lParam, NS4P_DEFAULT_POST_DELAY);
#else
                                    g_main_message_handler->PostDelayedMessage(message_id, wParam, lParam, 10);
#endif // NS4P_POST_DELAYED_NEWSTREAM
									return stat;
								}
								OP_DBG(("Plugin [%d] stream [%d] NEWSTREAM", call->id.pluginid, call->id.streamid));
								plugin->PushLockEnabledState();
								uint16 stype = ps->Type();
								TRY
									ret = plugin->GetPluginfuncs()->newstream(call->id.instance,
														  ps->GetMimeType(),
														  ps->Stream(),
														  FALSE,
														  &stype);
								CATCH_PLUGIN_EXCEPTION(ret=NPERR_GENERIC_ERROR;plugin->SetException();)
								OP_DBG(("Plugin [%d] stream [%d] NEWSTREAM returned", call->id.pluginid, call->id.streamid));
								plugin->PopLockEnabledState();
								ps->SetLastCalled(call->msg_type);
								if (ret == NPERR_NO_ERROR)
								{
									ps->SetType(stype);
									ps->WriteReady(plugin);
								}
								else if (ret == NPERR_OUT_OF_MEMORY_ERROR)
									stat = OpStatus::ERR_NO_MEMORY;
								else
									plugin->DeleteStream(ps);
								break;
							}
							case WRITEREADY:
							{
								if ((ps->GetMsgFlushCounter() > call->msg_flush_counter) || ps->GetLastCalled() == DESTROYSTREAM)
									break; // Since the plugin has called NPN_RequestRead(), the streaming should be halted, positioned and restarted

								OP_DBG(("Plugin [%d] stream [%d] WRITEREADY", call->id.pluginid, call->id.streamid));
								int32 write_ready = 0;
#if NS4P_STARVE_WHILE_LOADING_RATE>0
								if (!plugin->IsStarving())
#endif // NS4P_STARVE_WHILE_LOADING_RATE
								{
									plugin->PushLockEnabledState();
									TRY
										write_ready = plugin->GetPluginfuncs()->writeready(call->id.instance, ps->Stream());
									CATCH_PLUGIN_EXCEPTION(write_ready=0;plugin->SetException();)
									OP_DBG(("Plugin [%d] stream [%d] WRITEREADY returned", call->id.pluginid, call->id.streamid));
									plugin->PopLockEnabledState();
#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
									URL url=ps->GetURL();
									URL redir = url.GetAttribute(URL::KMovedToURL);
									if(!redir.IsEmpty())
										redir.SetAttribute(URL::KPauseDownload, write_ready <= 0);
									else
										url.SetAttribute(URL::KPauseDownload, write_ready <= 0);
#endif // TCP_PAUSE_DOWNLOAD_EXTENSION

									ps->SetLastCalled(call->msg_type);
								}
								ps->SetWriteReady(write_ready);
								ps->Write(plugin);
								break;
							}
							case WRITE:
							{
								if (ps->GetMsgFlushCounter() > call->msg_flush_counter || ps->GetLastCalled() == DESTROYSTREAM)
									break; // Since the plugin has called NPN_RequestRead(), the streaming should be halted, positioned and restarted

								plugin->PushLockEnabledState();
								OP_DBG(("Plugin [%d] stream [%d] WRITE", call->id.pluginid, call->id.streamid));
								int32 bytes_written = 0;
								TRY
									bytes_written = plugin->GetPluginfuncs()->write(call->id.instance,
															ps->Stream(),
															ps->GetOffset(),
															ps->GetBufLength(),
															(void*)ps->GetBuf());
								CATCH_PLUGIN_EXCEPTION(bytes_written = -1; plugin->SetException();)
								OP_DBG(("Plugin [%d] stream [%d] WRITE returned", call->id.pluginid, call->id.streamid));
								plugin->PopLockEnabledState();
								ps->SetLastCalled(call->msg_type);
								ps->SetBytesWritten(static_cast<int32>(ps->GetBufLength()) < bytes_written ? ps->GetBufLength() : bytes_written);

								if (ps->GetBytesWritten() > 0)
								{
									OP_ASSERT(ps->GetURLDataDescr() || ps->GetJSEval());
									if (ps->GetURLDataDescr())
									{
										ps->GetURLDataDescr()->ConsumeData(static_cast<unsigned>(ps->GetBytesWritten()));
										if (!ps->GetMore() && (static_cast<unsigned long>(ps->GetBytesWritten()) >= ps->GetBytesLeft()))
											ps->SetFinished();
									}
									else if (ps->GetJSEval() && (ps->GetOffset() + ps->GetBytesWritten() >= ps->GetJSEvalLength()))
										ps->SetFinished();
								}
								else
								{
									ps->SetFinished();
									if (ps->GetBytesWritten() < 0) // CallNPP_WriteProc was unsuccessful
										ps->SetReason(NPRES_USER_BREAK);
								}
								if (ps->IsFinished())
								{
									if (ps->Type() == NP_NORMAL)
										// experimental fix for flash video problems seen on youtube when there
										// is a delay between the last NPP_Write() and NPP_DestroyStream().
										// Fix is to call NPP_DestroyStream() directly instead of posting a message
										stat = DestroyStream(plugin, ps);
									else
										ps->StreamAsFile(plugin);
								}
								else
									ps->WriteReady(plugin);

								break;
							}
							case STREAMASFILE:
							{
								OP_DBG(("Plugin [%d] stream [%d] STREAMASFILE", call->id.pluginid, call->id.streamid));
#if defined(_LOCALHOST_SUPPORT_) || !defined(RAMCACHE_ONLY)
								OpString fname;
								OpString8 converted_fname;
#ifdef NS4P_STREAM_FILE_WITH_EXTENSION_NAME
								if (OpStatus::IsSuccess(stat = fname.Set(ps->GetUrlCacheFileName())))
#else
								if (OpStatus::IsSuccess(stat = ps->GetURL().GetAttribute(URL::KFilePathName_L, fname, TRUE)))
#endif // NS4P_STREAM_FILE_WITH_EXTENSION_NAME
								{
									stat = g_oplocale->ConvertToLocaleEncoding(&converted_fname, fname.CStr());
									if (OpStatus::IsSuccess(stat))
									{
										plugin->PushLockEnabledState();
										TRY
											plugin->GetPluginfuncs()->asfile(call->id.instance,
															 ps->Stream(),
															 converted_fname.CStr());
										CATCH_PLUGIN_EXCEPTION(plugin->SetException();)
										OP_DBG(("Plugin [%d] stream [%d] STREAMASFILE returned", call->id.pluginid, call->id.streamid));
										plugin->PopLockEnabledState();
										ps->SetLastCalled(call->msg_type);
										ps->Destroy(plugin);
									}
								}
#endif // _LOCALHOST_SUPPORT_ || !RAMCACHE_ONLY
								break;
							}
							case DESTROYSTREAM:
							{
								stat = DestroyStream(plugin, ps);
								break;
							}
							case URLNOTIFY:
							{
								ps->CallProc(URLNOTIFY, plugin);
								ps->SetLastCalled(call->msg_type);
								plugin->DeleteStream(ps);
								break;
							}
							default:
								OP_DBG(("[%p] ERROR: MSG_PLUGIN_CALL was not handled for plugin [%d](unknown message type)", wParam, call->id.pluginid));
						}
					}
					else
						OP_DBG(("[%p] ERROR: MSG_PLUGIN_CALL was not handled for plugin [%d](unknown stream id)", wParam, call->id.pluginid));

					break;
				}
				default:
				{
					OP_DBG(("[%p] ERROR: MSG_PLUGIN_CALL was not handled for plugin [%d](unknown message type)", wParam, call->id.pluginid));
					stat = OpStatus::ERR;
				}
			}
		}
		else
			OP_DBG(("[%p] ERROR: MSG_PLUGIN_CALL was not handled (unknown plugin id)", wParam));

		call->Out();
		OP_DELETE(call);
	}
	else
	{
		OP_DBG(("[%p] ERROR: not MSG_PLUGIN_CALL message type", wParam));
		stat = OpStatus::ERR;
	}

	return stat;
}

OP_STATUS PluginHandler::PostPluginMessage(PluginMsgType msgty, NPP inst, int plugin_id, int stream_id, NPWindow* npwin, int msg_flush_counter, int delay /*= 0*/)
{
	OP_ASSERT(g_main_message_handler);

	PluginMsgStruct *plugin_msg = OP_NEW(PluginMsgStruct, ());
	if (!plugin_msg)
		return OpStatus::ERR_NO_MEMORY;

	plugin_msg->msg_type = msgty;
	plugin_msg->id.instance = inst;
	plugin_msg->id.pluginid = plugin_id;
	plugin_msg->id.streamid = stream_id;
	plugin_msg->id.npwin = npwin;
	plugin_msg->msg_flush_counter = msg_flush_counter;
	plugin_msg->Into(&messages);

	if (delay)
		g_main_message_handler->PostDelayedMessage(MSG_PLUGIN_CALL, reinterpret_cast<MH_PARAM_1>(plugin_msg), 0, delay);
	else
		g_main_message_handler->PostMessage(MSG_PLUGIN_CALL, reinterpret_cast<MH_PARAM_1>(plugin_msg), 0);

	return OpStatus::OK;
}

BOOL PluginHandler::HasPendingPluginMessage(PluginMsgType msgty, NPP inst, int plugin_id, int stream_id, NPWindow* npwin, int msg_flush_counter)
{
	PluginMsgStruct *msg = (PluginMsgStruct *)messages.First();
	while(msg)
	{
		if (msg != last_handled_msg && msg->msg_type == msgty && msg->id.instance == inst
			&& msg->id.pluginid == plugin_id && msg->id.streamid == stream_id
			&& msg->id.npwin == npwin && msg->msg_flush_counter == msg_flush_counter)
			return TRUE;
		msg = (PluginMsgStruct *)msg->Suc();
	}
	return FALSE;
}


#ifdef NS4P_SUPPORT_PROXY_PLUGIN
OP_STATUS PluginHandler::LoadStaticLib()
{
#if defined STATIC_NS4PLUGINS && defined OPSYSTEMINFO_STATIC_PLUGIN
	// Get the spec for the proxy plugin :
	StaticPlugin plugin_spec;
    RETURN_IF_ERROR(g_op_system_info->GetStaticPlugin(plugin_spec));

	// Add it to the list :
	RETURN_IF_ERROR(pluginlibhandler->AddStaticLib(plugin_spec.plugin_name.CStr(),
												   plugin_spec.init_function,
												   plugin_spec.shutdown_function));
#endif // STATIC_NS4PLUGINS && OPSYSTEMINFO_STATIC_PLUGIN
	return OpStatus::OK;
}
#endif // NS4P_SUPPORT_PROXY_PLUGIN

OP_STATUS PluginHandler::HandleFailure()
{
	Plugin* plugin = static_cast<Plugin*>(plugin_list.First());
	while (plugin)
	{
		FramesDocument* frames_doc = plugin->GetDocument();
		if (plugin->IsFailure() && frames_doc)
		{
			if (HTML_Element* elm = plugin->GetHtmlElement())
			{
				Box* box = elm->GetLayoutBox();
				if (box && box->GetContent() && box->GetContent()->IsEmbed())
					static_cast<EmbedContent*>(box->GetContent())->SetRemoved();

				elm->DisableContent(frames_doc);
			}

			frames_doc->GetDocManager()->EndProgressDisplay(TRUE);
			plugin = NULL;
		}
		else
			plugin = plugin->Suc();
	}
	return OpStatus::OK;
}

void* PluginHandler::FindPluginHandlerRestartObject(FramesDocument* frames_doc, HTML_Element* element)
{
	PluginHandlerRestartObject *p = NULL;
	if (!restart_object_list.Empty())
	{
		p = restart_object_list.First();
		while (p && p->html_element != element)
		{
			p = p->Suc();
		}
	}
	OP_ASSERT(!p || p->frames_doc == frames_doc);
	return static_cast<void*>(p);
}

BOOL PluginHandler::IsSuspended(FramesDocument* frames_doc, HTML_Element* element)
{
	/* Check if this element's scripting has been suspended */
	PluginHandlerRestartObject *p = static_cast<PluginHandlerRestartObject*>(FindPluginHandlerRestartObject(frames_doc, element));
	return p && !p->been_blocked_but_plugin_failed;
}

OP_STATUS PluginHandler::Suspend(FramesDocument* frames_doc, HTML_Element* element, ES_Runtime* runtime, ES_Value* value, BOOL wait_for_full_plugin_init)
{
	OP_NEW_DBG("PluginHandler::Suspend", "ns4p");
	OP_ASSERT(frames_doc);
	OP_ASSERT(element);

	OP_ASSERT(frames_doc->GetLogicalDocument());
	OP_ASSERT(frames_doc->GetLogicalDocument()->GetRoot());
	OP_ASSERT(frames_doc->GetLogicalDocument()->GetRoot()->IsAncestorOf(element));
	OP_DBG(("Asked to suspend waiting for element %p (wait_for_full_plugin_init = %d)", element, wait_for_full_plugin_init));

	/* Initially check if this element's scripting has been suspended and then resumed because of an error */
	PluginHandlerRestartObject *p = static_cast<PluginHandlerRestartObject*>(FindPluginHandlerRestartObject(frames_doc, element));
	if (p && p->been_blocked_but_plugin_failed)
		return OpStatus::ERR;

	/* When resuming without errors, the PluginHandlerRestartObject will be removed from restart_object_list.
	   Scripts can be suspended, resumed and then suspended again, for the same element.
	   There can also be several scripts (from different documents) blocked on the same plugin. */

	/* Suspend this element's scripting */
	BOOL create_new_p = !p;
	if (create_new_p)
	{
		p = OP_NEW(PluginHandlerRestartObject, ());
		if (!p)
			return OpStatus::ERR_NO_MEMORY;
		p->html_element = element;
		p->frames_doc = frames_doc;
		p->been_blocked_but_plugin_failed = FALSE;
		p->restart_object_list = NULL;
	}
	PluginRestartObject* restart_object = NULL;

	if (OpStatus::IsError(frames_doc->AddBlockedPlugin(element)))
	{
		if (create_new_p)
			OP_DELETE(p);
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_STATUS stat;
	if (OpStatus::IsError(stat = PluginRestartObject::Suspend(restart_object, runtime)))
	{
		if (create_new_p)
			OP_DELETE(p);
		// We will leave it in the doc list, in case there are other scripts also blocked
		// at that plugin.
		return stat;
	}

	restart_object->type = wait_for_full_plugin_init ? PluginRestartObject::PLUGIN_RESTART_AFTER_INIT : PluginRestartObject::PLUGIN_RESTART;
	restart_object->next_restart_object = p->restart_object_list;
	p->restart_object_list = restart_object;

	if (create_new_p)
		p->Into(&restart_object_list);

	value->value.object = *restart_object;
	value->type = VALUE_OBJECT;

	return OpStatus::OK;
}

void PluginHandler::Resume(FramesDocument* frames_doc, HTML_Element *element, BOOL plugin_failed, BOOL include_threads_waiting_for_init)
{
	OP_NEW_DBG("PluginHandler::Resume", "ns4p");
	OP_DBG(("Asked to resume plugin blocked script for element %p (plugin_failed = %d, include_threads_waiting_for_init = %d)", element, plugin_failed, include_threads_waiting_for_init));

	PluginHandlerRestartObject *p = static_cast<PluginHandlerRestartObject*>(FindPluginHandlerRestartObject(frames_doc, element));
	if (p && !p->been_blocked_but_plugin_failed)
	{
		PluginRestartObject** prev_pointer = &p->restart_object_list;
		while (*prev_pointer)
		{
			PluginRestartObject* restart_object = *prev_pointer;
			BOOL resume = plugin_failed || include_threads_waiting_for_init || restart_object->type != PluginRestartObject::PLUGIN_RESTART_AFTER_INIT;

			if (resume)
			{
				restart_object->Resume();
				// Unlink
				*prev_pointer = restart_object->next_restart_object;
			}
			else
			{
				// Leave in list
				prev_pointer = &restart_object->next_restart_object;
			}
		}

		BOOL some_thread_still_blocked = p->restart_object_list != NULL;

		if (!some_thread_still_blocked)
		{
			if (plugin_failed)
			{
				p->been_blocked_but_plugin_failed = TRUE;
				p->restart_object_list = NULL;
			}
			else
			{
				p->Out();
				OP_DELETE(p);
			}
			if (frames_doc)
				frames_doc->RemoveBlockedPlugin(element);
		}
	}
	else
	{
		OP_ASSERT(!(p && p->restart_object_list));
	}
}

BOOL PluginHandler::IsPluginStartupRestartObject(ES_Object* object)
{
	OP_ASSERT(object);
	if (IsSupportedRestartObject(object))
	{
		PluginRestartObject *restart_object = static_cast<PluginRestartObject*>(ES_Runtime::GetHostObject(object));
		return (restart_object->type == PluginRestartObject::PLUGIN_RESTART ||
		        restart_object->type == PluginRestartObject::PLUGIN_RESTART_AFTER_INIT);
	}
	return FALSE;
}

BOOL PluginHandler::IsSupportedRestartObject(ES_Object* object)
{
	OP_ASSERT(object);
	EcmaScript_Object* host_object = ES_Runtime::GetHostObject(object);
	return host_object && host_object->IsA(NS4_TYPE_RESTART_OBJECT);
}

OP_STATUS PluginHandler::DestroyStream(Plugin* plugin, PluginStream* ps)
{
	if (ps->GetURL().Status(TRUE) == URL_LOADED)
	{ // cache request
		if (ps->GetURL().PrepareForViewing(TRUE) == MSG_OOM_CLEANUP)
			return OpStatus::ERR_NO_MEMORY;
	}
	NPError ret = ps->CallProc(DESTROYSTREAM, plugin);
	ps->SetLastCalled(DESTROYSTREAM);

	if (ret == NPERR_OUT_OF_MEMORY_ERROR)
		return OpStatus::ERR_NO_MEMORY;

	if (ps->GetNotify())
		ps->Notify(plugin);
	else
		plugin->DeleteStream(ps);

	return OpStatus::OK;
}

void PluginHandler::DestroyPluginRestartObject(PluginRestartObject* object)
{
	PluginHandlerRestartObject* p = restart_object_list.First();
	while (p)
	{
		PluginRestartObject* restart_object = p->restart_object_list;
		while (restart_object && restart_object != object)
			restart_object = restart_object->next_restart_object;

		if (restart_object)
			break;
		p = p->Suc();
	}
	if (p)
	{
		if (p->frames_doc)
			p->frames_doc->RemoveBlockedPlugin(p->html_element);
		p->Out();
		OP_DELETE(p);
	}
}

void PluginHandler::DestroyAllLoadingStreams()
{
	Plugin* plugin = static_cast<Plugin*>(plugin_list.First());
	while (plugin)
	{
		plugin->DestroyAllLoadingStreams();
		plugin = static_cast<Plugin*>(plugin->Suc());
	}
}

uint32_t PluginHandler::ScheduleTimer(NPP instance, uint32_t interval, BOOL repeat, void (*timerFunc)(NPP npp, uint32_t timerID))
{
	OP_NEW_DBG("PluginHandler::ScheduleTimer", "plugintimer");

	PluginTimer* pluginTimer = OP_NEW(PluginTimer, ());
	if (!pluginTimer)
		return 0;

	pluginTimer->instance  = instance;
	pluginTimer->timerID   = m_nextTimerID++;
	pluginTimer->interval  = interval;
	pluginTimer->repeat    = repeat;
	pluginTimer->timerFunc = timerFunc;
	pluginTimer->Into(&m_timer_list);

	pluginTimer->SetTimerListener(this);
	pluginTimer->Start(interval);

	OP_DBG(("pluginTimer[%p], instance[%p], timerID[%d], interval[%d], repeat[%d], f[%p]", pluginTimer, pluginTimer->instance, pluginTimer->timerID, pluginTimer->interval, pluginTimer->repeat, pluginTimer->timerFunc));

	return pluginTimer->timerID;
}

void PluginHandler::UnscheduleTimer(uint32_t timerID)
{
	OP_NEW_DBG("PluginHandler::UnscheduleTimer","plugintimer");

	PluginTimer* pluginTimer = m_timer_list.First();
	for (;pluginTimer; pluginTimer=pluginTimer->Suc())
	{
		if (pluginTimer->timerID == timerID)
		{
			OP_DBG(("pluginTimer[%p], instance[%p], timerID[%d], interval[%d], repeat[%d], f[%p], unscheduled[%d], running[%d]", pluginTimer, pluginTimer->instance, pluginTimer->timerID, pluginTimer->interval, pluginTimer->repeat, pluginTimer->timerFunc, pluginTimer->is_unscheduled, pluginTimer->is_running));
			pluginTimer->is_unscheduled = TRUE;
			break;
		}
	}
}

void PluginHandler::UnscheduleAllTimers(NPP instance)
{
    OP_NEW_DBG("PluginHandler::UnscheduleAllTimers", "plugintimer");
    OP_DBG(("instance[%p]", instance));

	PluginTimer* pluginTimer = m_timer_list.First();
	for (;pluginTimer;pluginTimer=pluginTimer->Suc())
	{
		if (pluginTimer->instance == instance)
			pluginTimer->is_unscheduled = TRUE;
	}
}

void PluginHandler::OnTimeOut(OpTimer* timer)
{
	OP_NEW_DBG("PluginHandler::OnTimeOut", "plugintimer");

	PluginTimer* pluginTimer = static_cast<PluginTimer*>(timer);
	OP_ASSERT(pluginTimer);

	PluginTimer* it = m_timer_list.First();
	while (it && it!=pluginTimer) it=it->Suc();
	if (it!=pluginTimer) return;

	// No recursion allowed
	if (pluginTimer->is_running)
		return;

	if (!FindPlugin(pluginTimer->instance))
		pluginTimer->is_unscheduled = TRUE;

	if (!pluginTimer->is_unscheduled)
	{
		if (pluginTimer->repeat)
			pluginTimer->Start(pluginTimer->interval);

		pluginTimer->is_running = TRUE;
		pluginTimer->timerFunc(pluginTimer->instance, pluginTimer->timerID);
		pluginTimer->is_running = FALSE;
	}

	if (!pluginTimer->repeat || pluginTimer->is_unscheduled)
	{
		OP_DBG(("Deleting: pluginTimer[%p], instance[%p], timerID[%d], interval[%d], repeat[%d], f[%p], unscheduled[%d], running[%d]", pluginTimer, pluginTimer->instance, pluginTimer->timerID, pluginTimer->interval, pluginTimer->repeat, pluginTimer->timerFunc, pluginTimer->is_unscheduled, pluginTimer->is_running));

		if (pluginTimer->IsStarted())
			pluginTimer->Stop();

		pluginTimer->Out();
		OP_DELETE(pluginTimer);
	}
}

void PluginHandler::OnLibraryError(const uni_char* library_name, const OpMessageAddress& address)
{
	for (Plugin* p = static_cast<Plugin*>(plugin_list.First()); p; p = static_cast<Plugin*>(p->Suc()))
		if (!uni_strcmp(p->GetName(), library_name))
		{
			p->SetFailure(TRUE, address);
			Destroy(p);
		}
}

#ifdef NS4P_COMPONENT_PLUGINS

OP_STATUS PluginHandler::WhitelistChannel(OpChannel* channel)
{
	return m_channel_whitelist.RegisterAddress(channel->GetAddress());
}

void PluginHandler::DelistChannel(const OpMessageAddress& channel_address)
{
	m_channel_whitelist.DeregisterAddress(channel_address);
}

PluginWhitelist::PluginWhitelist()
	: m_component_address(g_opera->GetAddress())
	, m_scripting_context(0)
{
}

BOOL PluginWhitelist::IsMember(const OpTypedMessage* message) const
{
	const OpMessageAddress& address = message->GetDst();

	/* The purpose of this filter is to block all non-plug-in-related messages to one
	 * specific component. If the message is addressed to another component, then we
	 * let the message pass. */
	if (address.cm != m_component_address.cm ||
		address.co != m_component_address.co)
		return TRUE;

	/* If the scripting context is restricted, and this is a script request made in
	 * a larger context, reject it. See PluginHandler::PushScriptingContext(). */
#define REJECT_IF_NOT_IN_CONTEXT(type) \
	case type::Type: \
	{ \
		type* m = type::Cast(message); \
		if (!m->HasContext() || m->GetContext() < m_scripting_context) \
			return FALSE; \
		break; \
	}

	if (m_scripting_context)
		switch (message->GetType())
		{
			REJECT_IF_NOT_IN_CONTEXT(OpPluginEvaluateMessage);
			REJECT_IF_NOT_IN_CONTEXT(OpPluginHasMethodMessage);
			REJECT_IF_NOT_IN_CONTEXT(OpPluginHasPropertyMessage);
			REJECT_IF_NOT_IN_CONTEXT(OpPluginGetPropertyMessage);
			REJECT_IF_NOT_IN_CONTEXT(OpPluginSetPropertyMessage);
			REJECT_IF_NOT_IN_CONTEXT(OpPluginRemovePropertyMessage);
			REJECT_IF_NOT_IN_CONTEXT(OpPluginInvokeMessage);
			REJECT_IF_NOT_IN_CONTEXT(OpPluginInvokeDefaultMessage);
			REJECT_IF_NOT_IN_CONTEXT(OpPluginObjectEnumerateMessage);
			REJECT_IF_NOT_IN_CONTEXT(OpPluginObjectConstructMessage);
		}

	/* Finally, check if the destination is whitelisted. */
	return m_whitelist.Count(address) > 0 ? TRUE : FALSE;
}

OP_STATUS PluginWhitelist::RegisterAddress(const OpMessageAddress& address)
{
	Whitelist::Iterator found = m_whitelist.Find(address);
	if (found != m_whitelist.End())
	{
		found->second++;
		return OpStatus::OK;
	}

	return m_whitelist.Insert(address, 1);
}

void PluginWhitelist::DeregisterAddress(const OpMessageAddress& address)
{
	Whitelist::Iterator found = m_whitelist.Find(address);
	if (found != m_whitelist.End())
	{
		if (found->second > 1)
			/* Can only fail if the address is not present, and we've established that it is. */
			found->second--;
		else
			m_whitelist.Erase(address);
	}
}

/* virtual */ OP_STATUS PluginSyncMessenger::SendMessage(OpTypedMessage* message, bool allow_nesting /* = false */,
														 unsigned int timeout /* = 0 */, int context /* = 0 */)
{
	if (!m_peer)
	{
		OP_DELETE(message);
		return OpStatus::ERR_NO_MEMORY;
	}

	if (!timeout)
	{
		/* Experimental. All synchronous plug-in communication ought to have a timeout, since we
		 * have little control over the code run on the wrapper side, and its nesting has caused
		 * very complicated and brittle platform PIs. */
		timeout = g_pcapp->GetIntegerPref(PrefsCollectionApp::PluginSyncTimeout) * 1000;
	}

	if (!allow_nesting)
	{
		PluginWhitelist* whitelist = static_cast<PluginWhitelist*>(g_pluginhandler->GetChannelWhitelist());
		OpMessageAddress address = m_peer->GetAddress();

		PluginChannelWhitelistToggle enabled(true);

		RETURN_IF_ERROR(whitelist->RegisterAddress(address));
		int old_scripting_context = whitelist->GetScriptingContext();
		whitelist->SetScriptingContext(context);

		OP_STATUS s = OpSyncMessenger::ExchangeMessages(message, timeout, OpComponentPlatform::PROCESS_IPC_MESSAGES);

		whitelist->SetScriptingContext(old_scripting_context);
		whitelist->DeregisterAddress(address);

		return s;
	}

	return OpSyncMessenger::SendMessage(message, true, timeout);
}

PluginChannelWhitelistToggle::PluginChannelWhitelistToggle(bool enable)
{
	OpTypedMessageSelection* whitelist = g_pluginhandler->GetChannelWhitelist();
	m_whitelist_was_enabled = g_component_manager->HasMessageFilter(whitelist);

	if (m_whitelist_was_enabled != enable)
	{
		if (enable)
			g_component_manager->AddMessageFilter(whitelist);
		else
			g_component_manager->RemoveMessageFilter(whitelist);
	}
}

PluginChannelWhitelistToggle::~PluginChannelWhitelistToggle()
{
	OpTypedMessageSelection* whitelist = g_pluginhandler->GetChannelWhitelist();
	bool whitelist_is_enabled = g_component_manager->HasMessageFilter(whitelist);

	if (m_whitelist_was_enabled != whitelist_is_enabled)
	{
		if (m_whitelist_was_enabled)
			g_component_manager->AddMessageFilter(whitelist);
		else
			g_component_manager->RemoveMessageFilter(whitelist);
	}
}

#endif // NS4P_COMPONENT_PLUGINS

#endif // _PLUGIN_SUPPORT_
