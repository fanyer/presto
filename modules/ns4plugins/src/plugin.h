/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _PLUGIN_H_INC_
#define _PLUGIN_H_INC_

#ifdef _PLUGIN_SUPPORT_

#include "modules/logdoc/elementref.h"
#include "modules/ns4plugins/src/pluginhandler.h"
#include "modules/ns4plugins/src/plugincommon.h"
#include "modules/ns4plugins/src/pluginscript.h"
#include "modules/ns4plugins/opns4plugin.h"
#include "modules/idle/idle_detector.h"
#include "modules/inputmanager/inputcontext.h"
#ifdef USE_PLUGIN_EVENT_API
#include "modules/pi/OpNS4PluginAdapter.h"
#include "modules/pi/OpKeys.h"
#endif // USE_PLUGIN_EVENT_API
#include "modules/pi/OpPluginWindow.h"
#if NS4P_INVALIDATE_INTERVAL > 0
#include "modules/hardcore/timer/optimer.h"
#endif // NS4P_INVALIDATE_INTERVAL > 0
#include "modules/util/simset.h"
#include "modules/url/url2.h"
#include "modules/url/url_docload.h"


class FramesDocument;
class EmbedBox;
class PluginStream;
class HTML_Element;
class VisualDevice;

typedef enum {
    NotAsked = 0,
    WantsAllNetworkStreams = 1,
	WantsOnlySucceededNetworkStreams = 2
} WantsAllNetworkStreamsType;

typedef enum {
	/** The handle event lock could not be acquired and events may not be delivered to the plug-in. */
	HandleEventNotAllowed,
	/** The handle event lock was acquired and a single event may be delivered to the plug-in.
	 * ReleaseHandleEventLock() must be called immediately after the event has been delivered.
	 * Before TryTakeHandleEventLock() nesting was allowed, ReleaseHandleEventLock() will restore
	 * the nesting permission correspondingly. */
	WindowProtectedNestingAllowed,
	/** The handle event lock was acquired and a single event may be delivered to the plug-in.
	 * ReleaseHandleEventLock() must be called immediately after the event has been delivered.
	 * Before TryTakeHandleEventLock() nesting was _NOT_ allowed, ReleaseHandleEventLock() will restore
	 * the nesting permission correspondingly. */
	WindowProtectedNestingBlocked,
} HandleEventLock;

/**
 * @brief An instance of a URLLink with an additional stream_count member variable
 * @author Hanne Elisabeth Larsen
 *
 *
 * Declaration :
 * Allocation  :
 * Deletion    :
 */

class StreamCount_URL_Link : public URLLink
{
public:
	int stream_count;

	/**
	 * StreamCount_URL_Link : constructed when a PluginStream is created.
	 *
	 * @param u URL to be streamed to the plugin
	 *
	 * @return when an URLLink instance has been constructed with initialized stream_count
	 */

	StreamCount_URL_Link(const URL &u): URLLink(u), stream_count(1) {}
};

/**
 * @brief An instance of a plugin
 * @author Hanne Elisabeth Larsen
 *
 *
 * Declaration :
 * Allocation  :
 * Deletion    :
 */

class Plugin
  : public Link
  ,	public OpNS4Plugin
#ifndef  USE_PLUGIN_EVENT_API
  , public OpInputContext
#endif // !USE_PLUGIN_EVENT_API
#if NS4P_INVALIDATE_INTERVAL > 0
  , public OpTimerListener
#endif // NS4P_INVALIDATE_INTERVAL > 0
  , public URL_DocumentLoader
  , public ElementRef
{
public:

	/**
	 *
	 */
	Plugin();

	/**
	 *
	 */
	virtual ~Plugin();

	/**
	 *
	 *
	 * @param plugin_dll
	 * @param component_type Component type to use for this plugin
	 * @param id
	 *
	 * @return
	 */
	OP_STATUS Create(const uni_char* plugin_dll,
					 OpComponentType component_type,
					 int id);

	/**
	 * Windowless API : called when a ui event was detected in the
	 * portion of the browserwindow that belongs to the plugin. The
	 * event will have to be translated to a platform plugin event
	 * and passed on to the plugin.
	 *
	 * @param event that occurred
	 * @param point where it occurred
	 * @param button_or_key_or_delta which button/key it concerns or scroll delta
	 * @param modifiers any modifiers that were present at the time
	 *
	 * @return TRUE if the event was successfully passed to the plugin
	 */
	BOOL HandleEvent(DOM_EventType event,
					 const OpPoint& point,
					 int button_or_key_or_delta,
					 ShiftKeyState modifiers);

#ifdef USE_PLUGIN_EVENT_API
	virtual bool DeliverKeyEvent(OpKey::Code key, const uni_char *key_value, OpPlatformKeyEventData *key_event_data, const OpPluginKeyState key_state, OpKey::Location location, const ShiftKeyState modifiers);
	virtual bool DeliverFocusEvent(bool focus, FOCUS_REASON reason);

	/**
	 *
	 *
	 * @param event
	 * @param event_type
	 *
	 * @return
	 */
	BOOL SendEvent(OpPlatformEvent * event,
				   OpPluginEventType event_type);

#endif // USE_PLUGIN_EVENT_API

	/**
	 *
	 *
	 * @param frames_doc
	 * @param plugin_window
	 * @param mimetype
	 * @param mode
	 * @param argc
	 * @param argn
	 * @param argv
	 * @param url
	 * @param embed_url_changed
	 *
	 * @return
	 */
    OP_STATUS New(FramesDocument* frames_doc,
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
				  BOOL embed_url_changed);

#ifdef USE_PLUGIN_EVENT_API
	/** Display the plug-in. Tell it about changes in its window.
	 *
	 * Can be either called during painting, reflowing or at arbitrary time
	 * in case plugin window has not been created yet.
	 *
	 * When called first time and NPP_New() has returned, this will create plugin
	 * window. Otherwise plugin window will be created at soonest opportunity.
	 * First call should be called with 'show' argument set to FALSE to ensure
	 * that a plug-in that e.g. produces sound, but is not within the visible
	 * view, will not start playing the sound even if it's not visible.
	 *
	 * During reflowing some arguments (namely show and is_fixed_positioned)
	 * have hardcoded FALSE value which does not necessarily reflect reality
	 * thus plugin window is not being updated during that phase as that
	 * would cause flickering if visibility would change for example.
	 *
	 * Can also be called from HandleBusyState() in case plugin window has not
	 * been created yet. That is similar to reflowing in a sense that both
	 * show and is_fixed_positioned arguments are FALSE.
	 *
	 * For windowed plug-ins, this will update the position and dimension of
	 * the plug-in window, and show it (unless 'show' is FALSE), and notify the
	 * plug-in library about the new values.
	 *
	 * For windowless plug-ins, this will notify the plug-in library about the
	 * position and dimension, and then paint it (unless 'show' is
	 * FALSE). Whatever the plug-in wants to paint must be painted now, to get
	 * layout box stacking order right.
	 *
	 * In some cases, when the plug-in is busy or not yet initialized, the
	 * plug-in cannot be displayed immediately. If this is the case, Display()
	 * will make sure that a paint request will be issued when the plug-in
	 * becomes ready (which in turn will cause this method to be called again).
	 *
	 * @param plugin_rect Position and size of the plug-in. Coordinates are
	 * relative to the viewport. All values are unscaled ("document" values).
	 * @param paint_rect Area to paint. This is only used for windowless
	 * plug-ins. Coordinates are relative to the viewport. All values are
	 * unscaled ("document" values). This rectangle will always be fully
	 * contained by 'plugin_rect', i.e. so that
	 * paint_rect.IntersectWith(plugin_rect) has no effect.
	 * @param show TRUE if the plug-in is to be shown / painted, FALSE if the
	 * plug-in is only to be notified about its dimensions and whereabouts,
	 * without being shown or painted. Some plug-ins are defined as hidden (in
	 * the HTML markup), typically because the plug-in is only needed for its
	 * ability to produce sound, or to perform other non-visual operations, in
	 * which case this parameter will be FALSE.
	 * @param fixed_position_subtree The first ancestor fixed positioned element
	 * of the plug-in (including the element owning the plugin)
	 * or NULL if no such exists.
	 *
	 * @return OK if successful (this also includes busy or not initialized
	 * plug-ins, since this does not require the caller to take any actions),
	 * ERR_NO_MEMORY if OOM, ERR for other errors.
	 */
	OP_STATUS Display(const OpRect& plugin_rect,
					  const OpRect& paint_rect,
					  BOOL show,
					  HTML_Element* fixed_position_subtree);
#else
	OP_STATUS SetWindow(int x,
						int y,
						unsigned int width,
						unsigned int height,
						BOOL show,
						const OpRect* rect = NULL);
#endif // USE_PLUGIN_EVENT_API

# ifdef _PRINT_SUPPORT_
	/**
	 *
	 *
	 * @param vd
	 * @param x
	 * @param y
	 * @param width
	 * @param height
	 */
	void Print(const VisualDevice* vd,
			   const int x,
			   const int y,
			   const unsigned int width,
			   const unsigned int height);
# endif // _PRINT_SUPPORT_

	/**
	 * Schedule the destruction of a plugin. Its window will be detached
	 * immediately (if it has one), and all resources will be released
	 * on the next revolution of the message cycle.
	 *
	 * @return OpStatus::OK on success.
	 */
	OP_STATUS ScheduleDestruction();

	/**
	 * Destroy the plugin instance. Will call NPP_Destroy and release
	 * all resources, including the plugin window.
	 *
	 * Calling this haphazardly may have terrible consequences, so
	 * unless you know exactly what you're doing, please call
	 * ScheduleDestruction() instead.
	 *
	 * @return OpStatus::ERR_NO_MEMORY on OOM, else OpStatus::OK.
	 */
	OP_STATUS Destroy();

#ifdef USE_PLUGIN_EVENT_API
	/**
	 * Passes the platform plugin event on to the plugin, will inform
	 * the platform with SaveState(event_type) and RestoreState(event_type)
	 * before and after the call respectivly.
	 * Will query the OpPluginWindow with IsAcceptingEvents(event_type)
	 * and will only perform the call if this returns true.
	 *
	 * @param event to be passed to the plugin
	 * @param event_type of the event
	 *
	 * @return the return value of the NPP_HandleEvent call to the plugin
	 */
	int16 HandleEvent(OpPlatformEvent* event, OpPluginEventType event_type, BOOL lock = TRUE);
#else
	int16 HandleEvent(void *event, BOOL lock = TRUE);
#endif // USE_PLUGIN_EVENT_API

	/**
	 * Try to acquire the lock needed to be able to deliver an event to the plug-in.
	 *
	 * @param event_type the type of event that the caller plans to deliver to the plug-in
	 * @param window_protection_needed true if the plug-in window needs to be protected during
	 *                                 event delivery (i.e. calls to NPP_SetWindow blocked).
	 * @return if the return value is != HandleEventNotAllowed, then the locking was
	 *         successful and an event may be delivered to the plug-in via one of the event
	 *         handling functions in PluginFunctions:: (i.e. HandleFocusEvent,
	 *         HandleWindowlessKeyEvent, HandleWindowlessPaint or HandleWindowlessMouseEvent).
	 *         After the event has been delivered to the plug-in, the caller must pass
	 *         this return value into ReleaseHandleEventLock() which will unprotect the
	 *         window and restore the old nesting permission as returned by
	 *         g_pluginscriptdata->AllowNestedMessageLoop() before the lock was acquired.
	 */
	HandleEventLock TryTakeHandleEventLock(OpPluginEventType event_type, BOOL window_protection_needed);

	/**
	 * Release lock needed to be able to deliver an event to the plug-in.
	 *
	 * This function must be called immediately after event delivery.
	 * See docs for TryTakeHandleEventLock().
	 *
	 * @param lock return value from a previous call to TryTakeHandleEventLock(), must be
	 *             either WindowProtectedNestingAllowed or WindowProtectedNestingBlocked
	 *             because if TryTakeHandleEventLock() returned HandleEventNotAllowed the
	 *             lock was not succesfully acquired and thus it's not necessary to release it.
	 * @param event_type type of the single event that was delivered to the plug-in between
	 *                   the calls to TryTakeHandleEventLock() and ReleaseHandleEventLock().
	 */
	void ReleaseHandleEventLock(HandleEventLock lock, OpPluginEventType event_type);

	/**
	 * Set that this plugin should be windowless, note that this
	 * is only meaningful before the first SetWindow, since this
	 * will define what should be in the NPWindow.type field.
	 *
	 * @param winless whether the plugin should be windowless
	 */
	void SetWindowless(BOOL winless);

	/** Return TRUE if the plug-in is windowless, FALSE otherwise. */
	BOOL IsWindowless() const { return windowless; }

	Plugin*				Suc() { return (Plugin*) Link::Suc(); }
	INT32				GetID() const { return ID; }

	/** Get plugin's clip rectangle.
	 * Check that the plugin is visible on screen, compensate for scrolling
	 * and find the intersect rectangle between the visual area and the plugin.
	 * @param[in] x plugin's x position relative to parent OpView in scaled units
	 * @param[in] y plugin's y position relative to parent OpView in scaled units
	 * @param[in] width plugin's width, in scaled units
	 * @param[in] height plugin's height, in scaled units
	 * @returns clipped rectangle relative to the plugin rect, in scaled units
	 */
	OpRect				GetClipRect(int x, int y, int width, int height);

#ifdef USE_PLUGIN_EVENT_API
	/** Call NPP_SetWindow() and possibly window position changed event
	 * unless the NPP_SetWindow() information is unchanged or
	 * if plugin is already processing a NPP_ call.
	 */
	void				CallSetWindow();
#else
	OP_STATUS			SetWindow();
#endif // USE_PLUGIN_EVENT_API

#ifdef _DEBUG
	int m_loading_streams;
	int	m_loaded_streams;
#endif // _DEBUG

	/** Adds a new stream to the plugin.
	 * @param new_stream Will contain a pointer to the newly created stream if successful.
	 * @param url The URL to stream data from.
	 * @param notify If TRUE, a notification will be sent to the plugin.
	 * @param notify_data Pointer to the plugin specific data to be sent along with the notification.
	 * @param loadInTarget VERIFY: Set to TRUE if loaded in another window or something.
	 * @returns Normal OOM values. */
	OP_STATUS			AddStream(PluginStream*& new_stream, URL& url, BOOL notify, void* notify_data, BOOL loadInTarget);
	/** Adds a new stream to the plugin. Implements OpNS4Plugin::AddPluginStream()
	 * @param url The URL to stream data from.
	 * @param mime_type The mime type specified for the plugin. Can be NULL.
	 * @returns Normal OOM values. */
	OP_STATUS			AddPluginStream(URL& url, const uni_char* mime_type);

	/** Must be called when there is more data available for the stream of the plugin itself.
	 * @return Normal OOM values */
	OP_STATUS			OnMoreDataAvailable();

	/** Implements OpNS4Plugin::StopLoadingAllStreams() */
	void				StopLoadingAllStreams() { URL_DocumentLoader::StopLoadingAll(); }

	OP_STATUS			InterruptStream(NPStream* pstream, NPReason reason);

#ifdef _PRINT_SUPPORT_
	BOOL				HaveSameSourceURL(const uni_char* src_url);
#endif // _PRINT_SUPPORT_

	uni_char*			GetName() const { return plugin_name; };


	OP_STATUS			GetScriptableObject(ES_Runtime* runtime, BOOL allow_suspend, ES_Value* value);

	OP_STATUS			GetFormsValue(uni_char*& string_value);
	OP_STATUS			GetPluginDescriptionStringValue();

	/* Checks if the plugin is interested in receiving the http body of all http requests
	 * including failed ones, http status != 200.
	 *
	 * @return the returned value from the plugin, where default value is OpBoolean::IS_FALSE.
	 */
	OP_BOOLEAN			GetPluginWantsAllNetworkStreams();

#if defined(_PLUGIN_NAVIGATION_SUPPORT_)
	BOOL NavigateInsidePlugin(INT32 direction);
#endif
    PluginStream*		FindStream(int id);
	FramesDocument*		GetDocument() const { return m_document; }

	NPPluginFuncs*		GetPluginfuncs() const { return pluginfuncs; }
	NPP					GetInstance() { return &instance_struct; }
	NPWindow*			GetNPWindow() { return &npwin; }

	ServerName*			GetHostName() { return m_document_url.GetServerName(); }

	void				SetTransparent(BOOL transp) { transparent = transp; }
	BOOL				IsTransparent() { return transparent; }

	void*				GetWindow() const { return m_plugin_window ? m_plugin_window->GetHandle() : NULL; }

#ifdef USE_PLUGIN_EVENT_API
	/** Returns plugin rect relative to parent OpView, in scaled units. */
	int					GetWindowX() const {return m_view_rect.x;}
	int					GetWindowY() const {return m_view_rect.y;}
#else
	int					GetWindowX() const {return win_x;}
	int					GetWindowY() const {return win_y;}
#endif // USE_PLUGIN_EVENT_API

// For the Plugin Event API the input context is in layout on ExternalContent
#ifndef  USE_PLUGIN_EVENT_API

         virtual const char * GetInputContextName() {return "Plugin Instance";}

	void                OnFocus(BOOL focus, FOCUS_REASON reason);

#endif // !USE_PLUGIN_EVENT_API

#ifdef NS4P_CHECK_PLUGIN_NAME
	OP_BOOLEAN			CheckPluginName(const uni_char* plugin_name);
#endif // NS4P_CHECK_PLUGIN_NAME

	OP_STATUS			DestroyAllStreams();

	/** DestroyAllLoadingStreams() destroys all loading streams. */
	void				DestroyAllLoadingStreams();

	void				RemoveLinkedUrl(URL_ID url_id);
	NPMIMEType			GetMimeType() const { return m_mimetype; }
	StreamCount_URL_Link* GetEmbedSrcUrl() { return static_cast<StreamCount_URL_Link *>(url_list.First()); }
	HTML_Element*       GetHtmlElement() const { return m_htm_elm; }
	void                SetHtmlElement(HTML_Element* htm_elm) { m_htm_elm = htm_elm; }

	/* ElementRef implementation. */

	/** Called when the element is about to be deleted. */
	virtual	void		OnDelete(FramesDocument* document);

	void				SetSaved(NPSavedData* sav) { saved = sav; }
	NPSavedData*		GetSaved() { return saved; }
	char**				GetArgs8() { return m_args8; }
	int16				GetArgc() { return m_argc; }

	void				PushLockEnabledState();
	void				PopLockEnabledState();
	BOOL				GetLockEnabledState() { return m_lock_stack != NULL; }

	/**
	 * Check if the plug-in can handle painting or NPP_SetWindow.
	 *
	 * Display() will be post-poned if the plug-in has not been initialized
	 * (NPP_SetWindow cannot precede NPP_New) or we are busy processing another
	 * paint event or SetWindow call.
	 *
	 * @return TRUE if the plug-in can process Plugin::Display().
	 */
	BOOL				IsDisplayAllowed();

	/**
	 * Check if we are already processing NPP_SetWindow or NPP_HandleEvent(paint).
	 *
	 * See Plugin::SetWindowProtected().
	 */
	BOOL				IsWindowProtected() { return m_is_window_protected; }

	/**
	 * Prevent or allow calls to Plugin::Display().
	 *
	 * Set TRUE before entering NPP_SetWindow or NPP_HandleEvent (when painting)
	 * to ensure we do not nest window related calls. Set FALSE afterwards.
	 *
	 * @param is_window_protected TRUE to protect window, FALSE otherwise.
	 */
	void				SetWindowProtected(BOOL is_window_protected = TRUE);

	/**
	 * Check if Display() was called while IsDisplayAllowed() was false, and
	 * if that was the case, then perform the post-poned Display.
	 *
	 * This method may only be called if IsDisplayAllowed().
	 */
	void				HandlePendingDisplay();

	void				SetContextMenuActive(BOOL c) { m_context_menu_active = c; }
	BOOL				GetContextMenuActive() { return m_context_menu_active; }

#if defined(NS4P_WMP_STOP_STREAMING) || defined(NS4P_WMP_CONVERT_CODEBASE) || defined(NS4P_WMP_LOCAL_FILES)
	BOOL				IsWMPMimetype() { return m_is_wmp_mimetype; }
#endif // NS4P_WMP_STOP_STREAMING || NS4P_WMP_CONVERT_CODEBASE || NS4P_WMP_LOCAL_FILES

	OpNPObject*         GetScriptableObject() const { return m_scriptable_object; }
	ES_Object*          GetDOMElement() const { return m_domelement; }
	uint16              GetMode() { return m_mode; }
	void                SetPluginUrlRequestsDisplayed(BOOL stat) { m_plugin_url_requests_displayed = stat; }
	BOOL                GetPluginUrlRequestsDisplayed() { return m_plugin_url_requests_displayed; }

	void				PushPopupsEnabledState(NPBool enabled);
	void				PopPopupsEnabledState();
	NPBool				GetPopupEnabledState() { return m_popup_stack && m_popup_stack->GetPopupState(); }

	NPObject*			GetWindowNPObject();
	NPObject*			GetPluginElementNPObject();

	BOOL				SameWindow(NPWindow npwin);

#ifdef USE_PLUGIN_EVENT_API
	OP_STATUS			AddParams(URL* url, BOOL embed_url_changed, BOOL add_allowscriptaccess);
	OP_STATUS			AddParams(int argc, const uni_char* argn[], const uni_char* argv[], URL* url, BOOL embed_url_changed, BOOL is_flash);
#else
	OP_STATUS			AddParams(int16 argc, const uni_char* argn[], const uni_char* argv[], URL* url, BOOL embed_url_changed, BOOL is_flash);
#endif // USE_PLUGIN_EVENT_API


#ifdef USE_PLUGIN_EVENT_API
	/** Register a listener that will receive plugin events for this plugin.
	 *  If the listener object is destroyed before the plugin object, the
	 *  listener should be detach using SetWindowListener(NULL). If the plugin
	 *  is destroyed first, it will implicitly stop delivering events. */
	void                SetWindowListener(OpPluginWindowListener* listener);
#endif // USE_PLUGIN_EVENT_API
	OpPluginWindow*		GetPluginWindow() const { return m_plugin_window; }
#ifdef USE_PLUGIN_EVENT_API
	OpNS4PluginAdapter*	GetPluginAdapter() const { return m_plugin_adapter; }
#endif // USE_PLUGIN_EVENT_API
#ifndef USE_PLUGIN_EVENT_API
	void				RemovePluginWindow() { m_plugin_window = NULL; }
#endif // !USE_PLUGIN_EVENT_API

	void				SetPopupsEnabled(BOOL enabled);
	BOOL				IsPopupsEnabled() { return m_popups_enabled; }

	void				StopLoading(URL& url);

    OP_BOOLEAN          HandlePostRequest(BYTE type, const char* url_name, const char* window, uint32 len, const char* buf, NPBool file, void* notifyData = NULL);
    OP_STATUS           InterruptStream(int url_id, NPReason reason);

	OP_STATUS			Redirect(URL_ID orig_url_id);

	OP_STATUS			SetWindowPos(int x, int y);

	void				HideWindow();

	/**
	 * Detach plugin window from its parents and siblings in the window
	 * system. The window will remain alive and fully intact.
	 *
	 * Intended to allow the UI's destruction of windows and tabs to be
	 * decoupled from plugin instance termination.
	 */
	void				DetachWindow();

    URL                 DetermineURL(BYTE type, const char* url_name, BOOL unique);
    URL					GetBaseURL();
	void				DeterminePlugin(BOOL& is_flash, BOOL& is_acrobat_reader);
	OP_STATUS           SetFailure(BOOL crashed, const OpMessageAddress& address = OpMessageAddress());
	BOOL                IsFailure() { return m_is_failure; }
#ifdef NS4P_WINDOWLESS_MACROMEDIA_WORKAROUND
	BOOL				SpoofUA() const { return m_spoof_ua; }
	OP_STATUS			DetermineSpoofing();
#endif // NS4P_WINDOWLESS_MACROMEDIA_WORKAROUND
#ifdef NS4P_TRY_CATCH_PLUGIN_CALL
	OP_STATUS           SetException() { m_is_exception = TRUE; return SetFailure(FALSE); }
	BOOL                IsException() { return m_is_exception; }
#endif // NS4P_TRY_CATCH_PLUGIN_CALL
	virtual BOOL		IsPluginFullyInited();

	/**
	 * Get the origin of the plugin's document URL.
	 *
	 * @param[out] The origin of the document on success.
	 * @return OpStatus::OK on success
	 *         OpStatus::ERR_NO_SUCH_RESOURCE if document was detached.
	 *         OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS           GetOrigin(char** origin);

	void				Activate();
	void				DeleteStream(PluginStream* ps);
	void				EnterSynchronousLoop() { m_in_synchronous_loop++; }
	void				LeaveSynchronousLoop() { m_in_synchronous_loop--; }
	BOOL				IsInSynchronousLoop() { return m_in_synchronous_loop > 0; }
	void				SetScriptException(BOOL e) { m_script_exception = e; }
	BOOL				IsScriptException() { return m_script_exception; }
	OP_STATUS			SetExceptionMessage(const NPUTF8 *message) { return m_script_exception_message.SetFromUTF8(message); }
	const uni_char*		GetExceptionMessage() { return m_script_exception_message.CStr(); }

#ifdef _MACINTOSH_
	NPDrawingModel		GetDrawingModel() { return m_drawing_model; }
	void				SetDrawingModel(NPDrawingModel drawing_model) { m_drawing_model = drawing_model; }

	/**
	 * Enable/disable use of Core Animation.
	 * @param ca  Whether to use Core Animation.
	 */
	void				SetCoreAnimation(BOOL ca) { m_core_animation = ca; }
#endif // _MACINTOSH_
	/*
	 * When NS4P_INVALIDATE_INTERVAL is set to 0 this method simply invokes InvalidateInternal().
	 * When NS4P_INVALIDATE_INTERVAL > 0 this method assures that InvalidateInternal() isn't called more often then NS4P_INVALIDATE_INTERVAL,
	 * so the region to invalidate is stored in m_invalid_rect and the timer is set.
	 * For flash video InvalidateInternal() is called independently of NS4P_INVALIDATE_INTERVAL - according to one of CORE-23977 goals
	 * it is not throttled.
	 * @param invalidRect the region to invalidate
	 */
	void				Invalidate(NPRect *invalidRect);

#if NS4P_INVALIDATE_INTERVAL > 0
	/*
	 * Invokes dalayed InvalidateInternal().
	 */
	void				OnTimeOut(OpTimer* timer);

	/*
	 * This method is called from the PluginStream to mark plugin as the flash video content.
	 * According to one of CORE-23977 goals flesh videos should not be throttled.
	 * @param isFlashVideo new value for m_is_flash_video member
	 */
	void				SetIsFlashVideo(BOOL isFlashVideo) { m_is_flash_video = isFlashVideo; }
#endif // NS4P_INVALIDATE_INTERVAL > 0

	void				OnStreamDeleted(PluginStream* stream) { if (stream == m_main_stream) m_main_stream = NULL; }


	/* Checks if an equal url is already streaming (by being one of the predecessors of ps).
	 * Note that when a plugin stream is finished, it is removed from the list.
	 * @param ps PluginStream object representing the url to be streamed to the plugin
	 *
	 * @return	TRUE ps has a predecessor PluginStream object with the same url id
	 *          FALSE ps has no predecessor PluginStream objects with the same url id
	 */
	BOOL				IsEqualUrlAlreadyStreaming(PluginStream* ps);


#if NS4P_STARVE_WHILE_LOADING_RATE>0
	/*
	 * Starving the plugin delays processing of the data by the plugin decreasing the CPU consumption during page loading.
	 * The value of NS4P_STARVE_WHILE_LOADING_RATE defines how often plugin will be starved (not fed with data) when the page is being loaded.
	 * @return  TRUE plugin should be starved in this cycle
	 *          FALSE plugin should not be starved in this cycle
	 */
	BOOL				IsStarving();
#endif // NS4P_STARVE_WHILE_LOADING_RATE

	/**
	 * Order is important - allows us to check states with range comparisons.
	 */
	enum LifeCycleState
	{
		FAILED_OR_TERMINATED = 0, ///< Broken
		UNINITED = 1, ///< Start state
		PRE_NEW, ///< Inside the New call
		NEW_WITHOUT_WINDOW, ///< Between New but before any SetWindow
		WITH_WINDOW, ///< After a SetWindow but before READYFORSTREAMING
		RUNNING ///< Fully started, everything goes
	};

	void				SetLifeCycleState(LifeCycleState new_state)
	{
		OP_ASSERT(new_state == FAILED_OR_TERMINATED || m_life_cycle_state != FAILED_OR_TERMINATED); // Can never leave FAILED_OR_TERMINATED
		OP_ASSERT(new_state == FAILED_OR_TERMINATED || new_state == m_life_cycle_state + 1); // Can only move forward one step at the time
		m_life_cycle_state = new_state;
	}

	LifeCycleState		GetLifeCycleState() const { return m_life_cycle_state; }

	/** Add an ES_Object -> OpNPObject to the list of active bindings.
	 *
	 * @param internal Key.
	 * @param object Value.
	 *
	 * @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY on memory allocation failure.
	 */
	OP_STATUS           AddBinding(const ES_Object* internal, OpNPObject* object);

	/** Remove an ES_Object -> OpNPObject from the list of active bindings.
	 *
	 * @param object Object to be removed.
	 */
	void                RemoveBinding(const ES_Object* internal);

	/** Invalidate OpNPObject.
	 *
	 * @param object Object that should no longer be referenced.
	 */
	void                InvalidateObject(OpNPObject* object);

	/** Look up an OpNPObject based on its internal ES_Object.
	 *
	 * @param internal The internal ES_Object to match against.
	 *
	 * @return A uniquely matching OpNPObject or NULL if none exist.
	 */
	OpNPObject*         FindObject(const ES_Object* internal);

	/** Take ownership of an OpNPObject.
	 *
	 * @param object The object now belonging to this plug-in instance.
	 */
	void                AddObject(OpNPObject* object) { object->Into(&m_objects); }

private:
	/*
	 * Does the actual work of invalidating the specified drawing area prior to repainting or refreshing.
	 * @param invalidRect the region to invalidate
	 * @param currentRuntimeMs this parameter is used to pass the actual value of GetRuntimeMS in order to reduce the number of calls to this method.
	 *							If 0. is passed in this parameter GetRuntimeMS is called internally in order to obtain the current runtime.
	 */
	void				InvalidateInternal(OpRect *invalidRect, double currentRuntimeMs = 0.);

private:
#if NS4P_INVALIDATE_INTERVAL > 0
	/** The time of last InvalidateInternal execution */
	double				m_last_invalidate_time;

	/** The rect to invalidate in next InvalidateInternal execution */
	OpRect				m_invalid_rect;

	/** Indicates that the last invalidate is already delayed, so the next InvalidateInternal should invalidate the union of previous and current rects. */
	BOOL				m_invalidate_timer_set;

	/** Timer for delaying plugin invalidates */
	OpTimer				*m_invalidate_timer;

	/** See SetIsFlashVideo() method */
	BOOL				m_is_flash_video;
#endif // NS4P_INVALIDATE_INTERVAL > 0
#if NS4P_STARVE_WHILE_LOADING_RATE>0
	/** See IsStarving() method  */
	int					m_starvingStep;
#endif // NS4P_STARVE_WHILE_LOADING_RATE
    Head				stream_list;
	Head				url_list; /* Make sure files aren't deleted while the plug-in runs */
    uni_char*			plugin_name;
    INT32				ID;
    int					stream_counter;
    FramesDocument*		m_document;
	URL					m_document_url;
	PluginStream*		m_main_stream;
	LifeCycleState		m_life_cycle_state;


#ifdef USE_PLUGIN_EVENT_API
	OpPluginWindowListener* m_window_listener;
	OpRect				m_view_rect; ///< plug-in window rectangle relative to the OpView, in scaled units
#endif // USE_PLUGIN_EVENT_API
	OpPluginWindow*		m_plugin_window;
	BOOL				m_plugin_window_detached; ///< plug-in window has been detached after creation.
#ifdef USE_PLUGIN_EVENT_API
	OpNS4PluginAdapter*	m_plugin_adapter;
#ifdef NS4P_COMPONENT_PLUGINS
	OpMessageAddress    m_plugin_adapter_channel_address;
#endif // NS4P_COMPONENT_PLUGINS
#endif // USE_PLUGIN_EVENT_API

    NPPluginFuncs*		pluginfuncs;
    NPP_t				instance_struct;
    NPSavedData*		saved;
    NPWindow			npwin;
#ifndef USE_PLUGIN_EVENT_API
    int					win_x;
    int					win_y;
#endif // !USE_PLUGIN_EVENT_API

	BOOL				windowless;
	BOOL				transparent;
	BOOL				m_core_animation;

	NPMIMEType			m_mimetype;
	HTML_Element*       m_htm_elm;

	PluginMsgType		m_lastposted;

	char**				m_args8;
	int16				m_argc;
#ifdef USE_PLUGIN_EVENT_API
	/** Display was called while !IsDisplayAllowed() and must be re-started. @See HandlePendingDisplay(). */
	BOOL				m_pending_display;
	/** Initial display was called while !IsDisplayAllowed() and must be re-started. @See HandlePendingDisplay(). */
	BOOL				m_pending_create_window;
#endif // USE_PLUGIN_EVENT_API
#ifdef _MACINTOSH_
	NPDrawingModel		m_drawing_model;
#endif // _MACINTOSH_
	class LockStack
	{
		private:
			LockStack*		m_previous_lock;
		public:
							LockStack(LockStack* previous_lock) { m_previous_lock = previous_lock;}
							~LockStack() {}
			LockStack*		Pred() { return m_previous_lock; }
	};
	LockStack*			m_lock_stack;
	/** Are we performing NPP_SetWindow or NPP_HandleEvent(paint)? @See Plugin::SetWindowProtected(). */
	BOOL				m_is_window_protected;
	BOOL				m_context_menu_active;

#if defined(NS4P_WMP_STOP_STREAMING) || defined(NS4P_WMP_CONVERT_CODEBASE)
	BOOL				m_is_wmp_mimetype;
#endif // NS4P_WMP_STOP_STREAMING || NS4P_WMP_CONVERT_CODEBASE
	void				SetLastPosted( const PluginMsgType ty ) { m_lastposted = ty; }
	PluginMsgType		GetLastPosted() const { return m_lastposted; }
	OP_STATUS			CreatePluginWindow(const OpRect& scaled_rect);

	/** ES_Object -> OpNPObject map. */
	OpPointerHashTable<ES_Object, OpNPObject> m_internal_objects;

	/** Objects belonging to this instance. */
	List<OpNPObject>    m_objects;

	/** Cached plugin scriptable object. May be NULL. Included in m_objects. */
	OpNPObject*         m_scriptable_object;

	ES_Object*          m_domelement;
	uint16              m_mode;
	BOOL                m_plugin_url_requests_displayed;
	OpComponentType		m_component_type;

	class PopupStack
	{
		private:
			NPBool			m_popup_enabled;
			PopupStack*		m_previous_popup;
		public:
							PopupStack(NPBool popup_enabled, PopupStack* previous_popup) { m_popup_enabled = popup_enabled; m_previous_popup = previous_popup;}
							~PopupStack() {}
			PopupStack*		Pred() { return m_previous_popup; }
			NPBool			GetPopupState() { return m_popup_enabled; }
	};
	PopupStack*			m_popup_stack;

	NPWindow			m_last_npwin;

	OP_STATUS			SetMimeType(const uni_char* mime_type);
	BOOL				m_popups_enabled;
	BOOL                m_is_failure;
# ifdef NS4P_WINDOWLESS_MACROMEDIA_WORKAROUND
	BOOL				m_spoof_ua;
# endif // NS4P_WINDOWLESS_MACROMEDIA_WORKAROUND

#ifdef NS4P_TRY_CATCH_PLUGIN_CALL
	BOOL                m_is_exception;
#endif // NS4P_TRY_CATCH_PLUGIN_CALL

	INT					m_in_synchronous_loop;
	BOOL				m_script_exception;
	OpString			m_script_exception_message;

	WantsAllNetworkStreamsType m_wants_all_network_streams_type;


	/** Check if any additional NPP_New() argn parameters should be added and
	 * assign the extra parameters to Plugin::m_args8 and Plugin::m_argc.
	 * @param[in, out] count The number of parameters added so far
	 * @param[in] add_restricted_allowscriptaccess If TRUE, ALLOWSCRIPTACCESS=SAMEDOMAIN is added
	 * @param[in] add_baseurl If TRUE, BASEURL=<baseurl> is added
	 * @param[in] baseurl Name of the baseurl
	 * @param[in] add_srcurl If TRUE, SRC=<srcurl> is added
	 * @param[in] srcurl Name of the srcurl
	 * @returns Normal OOM values.
	 */

	OP_STATUS			AddExtraParams(int& count, BOOL add_restricted_allowscriptaccess, BOOL add_baseurl, OpString baseurl, BOOL add_srcurl, OpString srcurl);

	/** Keeps a local 'is active'-state for all plugins. Used to detect if opera is idle, important for testing */
	OpAutoActivity activity_plugin;

// -----------------------------------------------------------
//
// DEPRECATED API :
//
// -----------------------------------------------------------
#ifndef USE_PLUGIN_EVENT_API
	DEPRECATED(OP_STATUS OldSetWindow());
	DEPRECATED(int16 OldHandleEvent(void *event, BOOL lock = TRUE));
	DEPRECATED(BOOL	OldHandleEvent(DOM_EventType event, const OpPoint& point, int button_or_key, ShiftKeyState modifiers));
	DEPRECATED(OP_STATUS OldNew(FramesDocument* frames_doc, void* plugin_window, const uni_char *mimetype, uint16 mode, int16 argc, const uni_char *argn[], const uni_char *argv[], URL* url, BOOL embed_url_changed));
	DEPRECATED(OP_STATUS OldSetWindow(int x, int y, unsigned int width, unsigned int height, BOOL show, const OpRect* rect = NULL));
	DEPRECATED(void	OldPrint(const VisualDevice* vd, const int x, const int y, const unsigned int width, const unsigned int height));
	DEPRECATED(OP_STATUS OldDestroy());
	DEPRECATED(void OldPluginDestructor());
#endif // USE_PLUGIN_EVENT_API
// -----------------------------------------------------------
};

#ifndef USE_PLUGIN_EVENT_API
inline OP_STATUS Plugin::OldSetWindow()
{
    return SetWindow();
}
#endif // !USE_PLUGIN_EVENT_API

/** Class for linked parameters, built with removed duplicates of name, as a temporary solution for bug #253601 */

class PluginParameter
	: public Link
{
private:

	/** The parameter's name, unique in the linked list. */
	OpString m_parameter_name;

	/** The parameter's matched (first) value. */
	OpString m_parameter_value;

public:

	OP_STATUS SetNameAndValue(const uni_char* name, const uni_char* value) { RETURN_IF_ERROR(m_parameter_name.Set(name)); return m_parameter_value.Set(value); }
	const uni_char* GetName() const { return m_parameter_name.CStr(); }
	const uni_char* GetValue() const { return m_parameter_value.CStr(); }

	/** Contain() checks if the parameter's name matches a given string */
	BOOL Contain(const uni_char* name) { return uni_stri_eq(m_parameter_name.CStr(), name); }

	/** Suc() returns the next parameter in the list, built with removed duplicates */
	PluginParameter* Suc() const { return (PluginParameter*)Link::Suc(); }

	/** FindInValue() checks if the parameter's value starts with a given string */
	BOOL FindInValue(const uni_char* name) { return (m_parameter_value.Find(name) == 0) ? TRUE : FALSE; }
};

#if defined(NS4P_TRY_CATCH_PLUGIN_CALL) || defined(MEMTOOLS_ENABLE_STACKGUARD)
class InCallFromPlugin
{
#ifdef MEMTOOLS_ENABLE_STACKGUARD
	OpStackTraceGuard stop_stack_trace_extraction;  // Don't venture into calling plugin
#endif // MEMTOOLS_ENABLE_STACKGUARD
#ifdef 	NS4P_TRY_CATCH_PLUGIN_CALL
	BOOL m_knew_that_it_was_plugin;
public:
	InCallFromPlugin()
	{
		// Sometimes the plugin that is calling us is coming
		// directly from the platform message loop and in that
		// case we haven't registered that we were in a plugin
		// and should not set the flag either because nobody will
		// clear it when the plugin returns.
		m_knew_that_it_was_plugin = g_opera->ns4plugins_module.is_executing_plugin_code;
		g_opera->ns4plugins_module.is_executing_plugin_code = FALSE;
	}
	~InCallFromPlugin() { g_opera->ns4plugins_module.is_executing_plugin_code = m_knew_that_it_was_plugin; }
#endif // NS4P_TRY_CATCH_PLUGIN_CALL
};

# define IN_CALL_FROM_PLUGIN InCallFromPlugin in_call_from_plugin; // sets global state
#else
# define IN_CALL_FROM_PLUGIN
#endif // NS4P_TRY_CATCH_PLUGIN_CALL || MEMTOOLS_ENABLE_STACKGUARD

#endif // _PLUGIN_SUPPORT_
#endif // !_PLUGIN_H_INC_
