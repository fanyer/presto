DOM_GADGET_FILE_API_SUPPORT/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 * Authors: Chris Pine, Huib Kleinhout, Patryk Obara
 */

#ifndef DESKTOP_GADGET_H
#define DESKTOP_GADGET_H

#ifdef GADGET_SUPPORT

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

#include "modules/dochand/win.h"
#include "modules/hardcore/timer/optimer.h"
#include "modules/widgets/OpWidget.h"

#ifdef WEBSERVER_SUPPORT
#include "modules/webserver/webserver_resources.h"
#endif // WEBSERVER_SUPPORT

#ifdef WIDGET_RUNTIME_SUPPORT
#ifdef DOM_GEOLOCATION_SUPPORT
#include "adjunct/widgetruntime/GadgetPermissionListener.h"
#endif // DOM_GEOLOCATION_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT

#ifdef WIDGETS_UPDATE_SUPPORT
#include "adjunct/autoupdate_gadgets/GadgetUpdateController.h"
#endif //WIDGETS_UPDATE_SUPPORT

class GadgetContainerWindow;
class GadgetTooltipHandler;
class Image;
class OpWindow;
class OpWindowCommander;
class WebserverRequest;

#ifdef WEBSERVER_SUPPORT
#define DESKTOP_GADGET_TRAFFIC_TIMEOUT			5000  // ms
#define DESKTOP_GADGET_TRAFFIC_SLOW_TIMEOUT		30000 // ms
#define DESKTOP_GADGET_TRAFFIC_QUEUE_LENGTH		5
#define DESKTOP_GADGET_NOTIFY_TIME_DELAY		899   // secs

// Request per second timeouts
#define DESKTOP_GADGET_RPS_LOW					1.0
#define DESKTOP_GADGET_RPS_MEDIUM				5.0
#endif // WEBSERVER_SUPPORT

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum GadgetInstallationType
{
	  SystemSpaceGadget		// gadget shipped with Opera (in the Unite directory?)
    , UserSpaceGadget		// gadget located in the widgets directory
	, RemoteGadgetURL		// installation from a URL
	, LocalPackagedGadget	// drag & drop of a gadget stored locally. gadget should be copied to the widgets directory.
	, LocalGadgetConfigXML	// drag & drop of a config.xml file. gadget should be started from the location of the config.xml. no copying/moving.
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

/***********************************************************************************
 **
 **	GadgetVersion
 ** Class to process and compare the gadget's version sring
 ** selftests: run opera -test=quick.gadgetversion
 **
 ***********************************************************************************/
class GadgetVersion
{
public:
	/**
	 * Used if parts of the version are supposed to be treated as decimal places
	 * (i.e. version "10.10" equals "10.1") rather than separate integers
	 * (i.e. version "10.01" equals "10.1"). It specifies the number of decimal
	 * places supported. 6 means: highest spotted version part is'999999'
	 * 
	 * @see GetVersionPart
	 */
	const static UINT32 s_supported_default_len = 6;

public:
	// Constructors / Destructor
	GadgetVersion();
	GadgetVersion(const GadgetVersion & version);
	~GadgetVersion();

	// Setters
	OP_STATUS	Set(const OpStringC & version_string);
	void		Reset();

	OP_STATUS	SetMajor(UINT32 major);
	OP_STATUS	SetMinor(UINT32 minor);
	OP_STATUS	SetRevision(UINT32 revision);
	OP_STATUS	SetAdditionalInfo(const OpStringC & additional_info) { return m_additional_info.Set(additional_info.CStr()); }

	// Getters
	UINT32		GetMajor() const;
	UINT32		GetMinor() const;
	UINT32		GetRevision() const;
	OP_STATUS	GetAdditionalInfo(OpString & additional_info) const { return additional_info.Set(m_additional_info.CStr()); }

	// Operators
	BOOL operator ==(const GadgetVersion & version) const;
	BOOL operator  <(const GadgetVersion & version) const;
	BOOL operator  >(const GadgetVersion & version) const;
	BOOL operator !=(const GadgetVersion & version) const { return !(*this == version); }
	BOOL operator <=(const GadgetVersion & version) const { return !(*this > version); }
	BOOL operator >=(const GadgetVersion & version) const { return !(*this < version); }
	GadgetVersion operator =(const GadgetVersion & version);

private:
	/**
	 ** Reads the next part of the version, ie. the minor version part of "10.01.2003"
	 **
	 ** @param version_string	Input string to read the version part from
	 ** @param offset			Offset from where to start searching. In the example above, the position after the first dot.
	 ** @param supported_len	The supported lenght of the version part, ie how many positions are supported. 
	 **							e.g. if supported_len == 4, the minor version of "10.1" would be changed to "1000"
	 **							this is to differenciate between "10.01" and "10.1"
	 **							supported_len == -1 doesn't append anything
	 ** @param version_part		The actual retrieved number. e.g. version "10.23" with supported_len == 4 returns 2300 as minor version (offset == 3)
	 ** @param version_part_len	The string lengh of the number read. e.g. minor version in "10.23" returns 2 for the minor version
	 **
	 ** @return		If reading a number was successful or not. Returns an error if offset/supported_len are too big or if there is no number to be read.
	 */
	OP_STATUS	GetVersionPart(const OpStringC & version_string, UINT32 offset, INT32 supported_len, UINT32 * version_part, UINT32 * version_part_len); 

private:
	// Shifts & masks for the version data
	const static UINT32 s_major_ver_shift	= 40;
	const static UINT32 s_minor_ver_shift	= 20;
	const static UINT64 s_major_ver_mask	= 0x0000000000FFFFFF; // highest major version we can handle is 2^24 - 1
	const static UINT64 s_ver_mask			= 0x00000000000FFFFF; // highest minor version and revision we can handle is 2^20 - 1

private:
	UINT64		m_version;					// storing major (first 24 bits), minor (next 20 bits) and revision (next 20 bits)
	OpString	m_additional_info;			// any string that is not part of the version (comes after the version, if present)
};


struct GadgetNotificationCallbackWrapper
{
	GadgetNotificationCallbackWrapper() : m_callback(0) {}

	OpDocumentListener::GadgetShowNotificationCallback* m_callback;
};


/**
 * \brief DesktopGadget is representation of any OpGadget
 * (including UniteApplication), that we intent to initialize in quick.
 *
 * Ordinary gadgets and UAs share interface and most of implementation in this
 * class (mostly because we want DesktopGadget to be backwards compatible).
 * However, DesktopGadget initialized as Gadget will behave differently than
 * UA; gadgets have to create it's own structure of OpWindows to function
 * properly while UA have to register itself as WebSubServerEventListener.
 * 
 * When dealing with uknown DesktopGadget object, you can determine its
 * behaviour with GetOpGadget()->IsSubserver().
 *
 * When object is initialized as UA, no visible user interface is created.
 *
 * When object is initialized as gadget, it creates following structure
 * for itself:
 *
 *  +----------------------------------------------+
 *  | GadgetContainerWindow                        |
 *  | +----------------------------------------------+
 *  | | WidgetWindow (container for Control Buttons) |
 *  | +----------------------------------------------+
 *  | +----------------------------------------------+
 *  | | DesktopGadget (this object)                  |
 *  | |                                              |
 *  | | Gadget's document lives in this window       |
 *  +-|                                              |
 *    +----------------------------------------------+
 *
 * DesktopGadget aggregates its parent window because otherwise
 * backward compatibility with rest of quick code would be broken
 * (and gadget window would have to reimplement all of DesktopGadget's
 * base classes in such situation). This should be repaired when
 * possible.
 *
 * However, initialization of control buttons window is dependent on 
 * gadget's set of features - control buttons can be explicitly disabled
 * by widget's author; this can be determined through 
 * HasControlButtonsEnabled method.
 * 
 * GadgetContainerWindow is toplevel parent window aggregated in DesktopGadget;
 * it deals with all aspects of gadget's window from window manager POV
 * (placement, native decorations, outer size, application icon, etc); it
 * also deals with showing, hiding and blocking of Control Buttons.
 *
 * When resizing gadget's document through javascript all resize calls are
 * treated as SetInnerSize (Control Buttons size is treated as part of
 * decorations, thus all resizes change only size of document - size of
 * GadgetContainerWindow adjusts accordingly).
 *
 * TODO 
 * - In future this class should break away from DesktopWindow 
 * interface and spawn two separate classes.
 * - When Application mode is on, there are number of hacks, that
 * override new window structure to decorate DesktopGadget after all;
 * this is the case because otherwise gogi has problems with closing windows
 * when close originated from platform. To remove these hacks remove 
 * m_decorated flag from DesktopGadget.
 *
 */ 
class DesktopGadget : public DesktopWindow,
					  public OpDocumentListener,
					  public OpMenuListener, 
					  public OpLoadingListener
#ifdef WEBSERVER_SUPPORT
					  , public OpTimerListener
					  , public WebSubserverEventListener
#endif // WEBSERVER_SUPPORT
#ifdef DOM_GADGET_FILE_API_SUPPORT
					  , public OpFileSelectionListener
					  , public DesktopFileChooserListener					  
#endif
{
	public:

		enum GadgetStyle
		{
			GADGET_STYLE_NORMAL,
			GADGET_STYLE_ALWAYS_ON_TOP,
			GADGET_STYLE_ALWAYS_BELOW,
#ifdef WEBSERVER_SUPPORT
			GADGET_STYLE_HIDDEN
#endif
		};
		
#ifdef WEBSERVER_SUPPORT
		enum DesktopGadgetActivityState
		{
			DESKTOP_GADGET_STARTED,
			DESKTOP_GADGET_NO_ACTIVITY,
			DESKTOP_GADGET_LOW_ACTIVITY,
			DESKTOP_GADGET_MEDIUM_ACTIVITY,
			DESKTOP_GADGET_HIGH_ACTIVITY
		};
		
		class ActivityListener
		{
			public:
			virtual ~ActivityListener() {}

			virtual void	OnDesktopGadgetActivityStateChanged(DesktopGadgetActivityState act_state, time_t seconds_since_last_request) {};
			virtual void	OnGetAttention() {}
		};
		
#endif // WEBSERVER_SUPPORT

		/**
		 * @param commander the window commander commanding this DesktopWindow's
		 * 		OpWindow
		 */
		explicit DesktopGadget(OpWindowCommander* commander);
		virtual				~DesktopGadget();

		OP_STATUS Init();

	private:
		/**
		 * Initialize non-webserver/non-service gadget; this involves creating correct
		 * OpWindows structure through call to InitializaContainerWindow()).
		 *
		 * Webservers and ordinary gadgets share only slight ammount of code here:
		 * see InitializeOpGadget().
		 *
		 * @returns OK on succesfull initialization
		 */
		OP_STATUS			InitializeGadget();

#ifdef WEBSERVER_SUPPORT
		/**
		 * This is alternative initialization code to InitializaGadget.
		 *
		 * Webservers and ordinary gadgets share only slight ammount of code here:
		 * see InitializeOpGadget().
		 *
		 * @returns OK on succesfull initialization
		 */
		OP_STATUS			InitializeWebserver();
#endif // WEBSERVER_SUPPORT

		/**
		 * This method is used to initialize GadgetContainerWindow (parent window for
		 * this DesktopGadget);
		 *
		 * @returns OK on succesfull initialization
		 */
		OP_STATUS			InitializeContainerWindow();

		/**
		 * Configure the window commander commanding this DesktopWindow's
		 * OpWindow.  Initialization code, that gadgets and webservers share.
		 *
		 * @returns OK on succesfull initialization
		 */
		OP_STATUS			ConfigureWindowCommander();

		GadgetContainerWindow* m_container_window; /// Toplevel container window.
		bool m_decorated;
		
	public:
		BOOL				IsInitialized() { return m_initialized; }

		OpGadget*			GetOpGadget();
		const OpGadget*		GetOpGadget() const;

		void				CenterOnScreen();
		BOOL				SetGadgetStyle(GadgetStyle gadget_style, BOOL only_test_if_possible = FALSE, BOOL show = TRUE);
		GadgetStyle			GetGadgetStyle() { return m_gadget_style; }

		UINT32				GetScale()	{ return m_scale; };
		void				SetScale(const UINT32 new_scale);

		virtual OpWindowCommander*	GetWindowCommander() { return m_window_commander; }
		void				SetIcon(Image &icon);

#ifdef WIDGET_CONTROL_BUTTONS_SUPPORT
		BOOL				HasControlButtonsEnabled();
#endif // WIDGET_CONTROL_BUTTONS_SUPPORT

#ifdef WIDGETS_UPDATE_SUPPORT						   
		void SetUpdateController(WidgetUpdater& ctrl);
#endif //WIDGETS_UPDATE_SUPPORT 

		// is this an unite service?
#ifdef WEBSERVER_SUPPORT
	    OP_STATUS		    AddActivityListener(ActivityListener* listener)		{ return m_listeners.Add(listener); }
		void				RemoveActivityListener(ActivityListener* listener)	{ m_listeners.Remove(listener); }
#endif // WEBSERVER_SUPPORT

		// == DesktopWindow hooks ======================

		virtual Type		GetType() { return WINDOW_TYPE_GADGET; }
		virtual void		OnShow(BOOL show);
	    virtual DesktopWindow* GetToplevelDesktopWindow();

		virtual void		OnActivate(BOOL activate, BOOL first_time);
		virtual OP_STATUS	AddToSession(OpSession* session, INT32 parent_id, BOOL shutdown, BOOL extra_info = FALSE);

		void				SetOuterPos(INT32 x, INT32 y);
		void				GetOuterPos(INT32& x, INT32& y);
		void				OnResize(INT32 width, INT32 height);

		BOOL				Activate(BOOL activate = TRUE);

		virtual BOOL		RespectsGlobalVisibilityChange(BOOL visible) { return FALSE; }

        // == OpMenuListener ======================

		virtual void		OnPopupMenu(OpWindowCommander* commander, OpDocumentContext& context, const OpPopupMenuRequestMessage *);
		virtual void		OnPopupMenu(OpDocumentContext* context);
# ifdef WIC_MIDCLICK_SUPPORT
		virtual void		OnMidClick(OpWindowCommander * commander, OpDocumentContext& context, BOOL mousedown, ShiftKeyState modifiers);
# endif // WIC_MIDCLICK_SUPPORT
        virtual void		OnMenuDispose(OpWindowCommander* commander) {}

        // == OpInputContext ======================

        virtual BOOL			OnInputAction(OpInputAction* action);
        virtual const char*		GetInputContextName() { return "Desktop Gadget"; }

		BOOL					HandleGadgetActions(OpInputAction* action, OpWindowCommander* wc );
		BOOL					HandleLinkActions(OpInputAction* action, OpWindowCommander* wc );
		BOOL 					HandleImageActions(OpInputAction* action, OpWindowCommander* wc );

		// == OpLoadingListener ======================

		virtual void			OnUrlChanged(OpWindowCommander* commander, URL& url) {} // NOTE: This will be removed from OpLoadingListener soon (CORE-35579), don't add anything here!
	    virtual void			OnUrlChanged(OpWindowCommander* commander, const uni_char* url) {}
	    virtual void			OnStartLoading(OpWindowCommander* commander) {}
	    virtual void			OnLoadingProgress(OpWindowCommander* commander, const LoadingInformation* info){}
	    virtual void			OnLoadingFinished(OpWindowCommander* commander, LoadingFinishStatus status);
		virtual BOOL			OnLoadingFailed(OpWindowCommander* commander, int msg_id, const uni_char* url) { return FALSE; }
		virtual void			OnAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback);
		virtual void			OnAuthenticationCancelled(OpWindowCommander* commander, URL_ID authid) {}
	    virtual void            OnXmlParseFailure(OpWindowCommander*) {}
#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
		virtual void 			OnSearchSuggestionsRequested(OpWindowCommander* commander, const uni_char* url, OpSearchSuggestionsCallback* callback) {}
#endif
	    virtual void			OnUndisplay(OpWindowCommander* commander) {}
		virtual void			OnLoadingCreated(OpWindowCommander* commander) {}
	    virtual void			OnStartUploading(OpWindowCommander* commander) {}
	    virtual void			OnUploadingFinished(OpWindowCommander* commander, LoadingFinishStatus status){}

        // == OpDocumentListener ======================

		virtual void			OnScaleChanged(OpWindowCommander* commander, UINT32 newScale);
		virtual void			OnWindowAttachRequest(OpWindowCommander* commander, BOOL attached){}
		virtual void			OnGetWindowAttached(OpWindowCommander* commander, BOOL* attached){}
        virtual void			OnClose(OpWindowCommander* commander);
        virtual void			OnDownloadRequest(OpWindowCommander*, DownloadCallback *);
        virtual void			OnHover(OpWindowCommander* commander, const uni_char* url, const uni_char* link_title, BOOL is_image);
        virtual void			OnNoHover(OpWindowCommander* commander);
		virtual void			OnTabFocusChanged(OpWindowCommander* commander, OpDocumentContext& context) {}
		virtual void			OnDocumentScroll(OpWindowCommander* commander){ }
		virtual void			OnSetupInstall(OpWindowCommander* commander, const uni_char* url, URLContentType content_type) {}
		virtual void			OnUnexpectedComponentGone(OpWindowCommander* commander, OpComponentType type, const OpMessageAddress& address, const OpStringC& information) {}
		virtual void			OnGeolocationAccess(OpWindowCommander* commander, const OpVector<uni_char>* hosts) {}
		virtual void			OnCameraAccess(OpWindowCommander* commander, const OpVector<uni_char>* hosts) {}
		virtual void			OnKeyboardSelectionModeChanged(OpWindowCommander* commander, BOOL enabled) {}
		virtual OP_STATUS		OnJSFullScreenRequest(OpWindowCommander* commander, BOOL enable_fullscreen) { return OpStatus::OK; }

        virtual BOOL HasGadgetReceivedDragRequest	(OpWindowCommander* commander) { return m_drag_requested; }
        virtual void OnGadgetDragRequest			(OpWindowCommander* commander) { m_drag_requested = TRUE; }
        virtual void CancelGadgetDragRequest		(OpWindowCommander* commander) { m_drag_requested = FALSE;}
        virtual void OnGadgetDragStart				(OpWindowCommander* commander, INT32 x, INT32 y);
        virtual void OnGadgetDragMove				(OpWindowCommander* commander, INT32 x, INT32 y);
        virtual void OnGadgetDragStop				(OpWindowCommander* commander);
        virtual BOOL OnGadgetClick					(OpWindowCommander* commander, const OpPoint &point, MouseButton button, UINT8 nclicks) { return FALSE; }
        virtual void OnGadgetPaint					(OpWindowCommander* commander, VisualDevice* vis_dev) {}
		virtual void OnGadgetGetAttention			(OpWindowCommander* commander);
		virtual void OnGadgetShowNotification		(OpWindowCommander* commander, const uni_char* message, GadgetShowNotificationCallback* callback);
	    virtual void OnGadgetShowNotificationCancel	(OpWindowCommander*);
		virtual void OnGadgetInstall				(OpWindowCommander* commander, URLInformation* url_info) { url_info->URL_Information_Done(); }

#ifdef WIDGET_CONTROL_BUTTONS_SUPPORT
		virtual void OnMouseGadgetEnter				(OpWindowCommander* commander);
		virtual void OnMouseGadgetLeave				(OpWindowCommander* commander);
#endif // WIDGET_CONTROL_BUTTONS_SUPPORT

	    void DoNotificationCallback					(OpDocumentListener::GadgetShowNotificationCallback* callback, OpDocumentListener::GadgetShowNotificationCallback::Reply reply);

        virtual void OnInnerSizeRequest          (OpWindowCommander* commander, UINT32 width, UINT32 height);
        virtual void OnGetInnerSize              (OpWindowCommander* commander, UINT32* width, UINT32* height);
        virtual void OnOuterSizeRequest          (OpWindowCommander* commander, UINT32 width, UINT32 height);
        virtual void OnGetOuterSize              (OpWindowCommander* commander, UINT32* width, UINT32* height);
        virtual void OnMoveRequest               (OpWindowCommander* commander, INT32 x, INT32 y);
        virtual void OnGetPosition               (OpWindowCommander* commander, INT32* x, INT32* y);

		virtual void OnTitleChanged(OpWindowCommander* commander, const uni_char* title);
		virtual BOOL OnAnchorSpecial			 (OpWindowCommander * commander, const AnchorSpecialInfo & info);
		virtual void OnAskPluginDownload		 (OpWindowCommander * commander);
		virtual void OnVisibleRectChanged		 (OpWindowCommander* commander, const OpRect& visible_rect){}

		virtual void OnOnDemandStateChangeNotify(OpWindowCommander* commander, BOOL has_placeholders) {};

		virtual void OnStatusText				 (OpWindowCommander* commander, const uni_char* text);

		virtual void OnWebHandler(OpWindowCommander* commander, WebHandlerCallback* cb){};
		virtual void OnWebHandlerCancelled(OpWindowCommander* commander, WebHandlerCallback* cb){};
		virtual BOOL OnMailToWebHandlerRegister(OpWindowCommander* commander, const uni_char* url, const uni_char* description){return FALSE;}
		virtual void OnMailToWebHandlerUnregister(OpWindowCommander* commander, const uni_char* url) {}
		virtual void IsMailToWebHandlerRegistered(OpWindowCommander* commander, MailtoWebHandlerCheckCallback* cb) {}

#ifdef PLUGIN_AUTO_INSTALL
	virtual void NotifyPluginDetected(OpWindowCommander* commander, const OpStringC& mime_type);
	virtual void NotifyPluginMissing(OpWindowCommander* commander, const OpStringC& mime_type);
	virtual void RequestPluginInfo(OpWindowCommander* commander, const OpStringC& mime_type, OpString& plugin_name, BOOL& available);
	virtual void RequestPluginInstallDialog(OpWindowCommander* commander, const PluginInstallInformation& information);
#endif

#ifdef DOM_GADGET_FILE_API_SUPPORT
		// FileSelectionListener
		virtual BOOL OnRequestPermission(OpWindowCommander* commander){ return TRUE; }
		virtual void OnDomFilesystemFilesRequest(OpWindowCommander* commander, DomFilesystemCallback* callback);
		virtual void OnDomFilesystemFilesCancel(OpWindowCommander* commander);
		virtual void OnUploadFilesRequest(OpWindowCommander* commander, UploadCallback* callback){}
		virtual void OnUploadFilesCancel(OpWindowCommander* commander) {}

		// DesktopFileChooserListener
		void OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result);
		DesktopFileChooserRequest	m_request;
#endif // DOM_GADGET_FILE_API_SUPPORT

		virtual void OnQueryGoOnline(OpWindowCommander* commander, const uni_char *message, DialogCallback *callback);

#ifdef DRAG_SUPPORT
		virtual void OnFileDropConfirm(OpWindowCommander* commander, const uni_char *server_name, DialogCallback *callback) {}
#endif // DRAG_SUPPORT

		virtual void OnCancelDialog(OpWindowCommander* commander, DialogCallback* callback) {}

		virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
		
	private:
        BOOL				m_drag_requested;

#ifdef DOM_GADGET_FILE_API_SUPPORT
	   //  Callback object for file choosing
	   OpFileSelectionListener::DomFilesystemCallback* m_file_choosing_callback;
#endif // DOM_GADGET_FILE_API_SUPPORT

		BOOL				m_initialized;
		BOOL				m_has_forced_focus;
		BOOL				m_stand_alone;
		GadgetStyle			m_gadget_style;
		UINT32				m_scale;
        INT32				m_drag_client_x;
        INT32				m_drag_client_y;
		OpPoint 			m_last_mouse_position;
		OpWindowCommander*	m_window_commander;
	    OpString			m_last_notification;
	    BOOL				m_last_notification_valid;
	    DesktopFileChooser*	m_chooser;
#ifdef DATABASE_MODULE_MANAGER_SUPPORT
		OpFileLength		m_previous_widgetprefs_quota;
		OpFileLength		m_previous_localstorage_quota;
		OpFileLength		m_previous_webdatabases_quota;
#endif // DATABASE_MODULE_MANAGER_SUPPORT
		GadgetTooltipHandler* m_tooltip_handler;

#ifdef WEBSERVER_SUPPORT
		OpListeners<ActivityListener>	m_listeners;
		OpTimer							m_traffic_timer;
		INT32							m_request_count[DESKTOP_GADGET_TRAFFIC_QUEUE_LENGTH];
		INT32							m_current_request_queue_num;
		DesktopGadgetActivityState		m_act_state;
		BOOL							m_slow_timer;
		time_t							m_last_request_time;
		BOOL							m_websubserver_listener_set;
#endif // WEBSERVER_SUPPORT

#ifdef DOM_GEOLOCATION_SUPPORT
		GadgetPermissionListener*		m_permission_listener;
#endif //DOM_GEOLOCATION_SUPPORT

	public:

        // == Unused OpDocumentListener methods ======================

        virtual void			OnImageModeChanged(OpWindowCommander* commander, ImageDisplayMode mode) {}
        virtual void			OnSecurityModeChanged(OpWindowCommander* commander, SecurityMode mode, BOOL inline_elt)  {}
#ifdef TRUST_RATING
		virtual void			OnTrustRatingChanged(OpWindowCommander* commander, TrustRating new_rating) {}
#endif // TRUST_RATING
        virtual void			OnCssModeChanged(OpWindowCommander* commander, CssDisplayMode mode)  {}
        virtual void			OnHandheldChanged(OpWindowCommander* commander, BOOL handheld) {}
#ifdef _PRINT_SUPPORT_
        virtual void			OnPrintPreviewModeChanged(OpWindowCommander* commander, PrintPreviewMode mode) {}
#endif // _PRINT_SUPPORT_
        virtual void			OnLinkClicked(OpWindowCommander* commander) {}
		virtual void			OnLinkNavigated(OpWindowCommander* commander, const uni_char* url, const uni_char* link_title, BOOL is_image, const OpRect* link_rect = NULL){ }
        virtual void			OnNoNavigation(OpWindowCommander* commander) {}
        virtual void			OnSearchReset(OpWindowCommander* commander)  {}
		virtual void			OnSearchHit(OpWindowCommander* commander) {}
        virtual void			OnDocumentIconAdded(OpWindowCommander* commander) {}
        virtual void			OnDocumentIconRemoved(OpWindowCommander* commander) {}
        virtual void            OnRaiseRequest(OpWindowCommander* commander) {}
        virtual void            OnLowerRequest(OpWindowCommander* commander) {}
        virtual void            OnDrag(OpWindowCommander* commander, const OpRect rect, const uni_char* link_url, const uni_char* link_title, const uni_char* image_url) {}
#ifdef DND_DRAGOBJECT_SUPPORT
		virtual void            OnDragLeave(OpWindowCommander* commander, class DesktopDragObject* drag_object) {}
		virtual void            OnDragMove(OpWindowCommander* commander, class DesktopDragObject* drag_object, const OpPoint& point) {}
		virtual void            OnDragDrop(OpWindowCommander* commander, class DesktopDragObject* drag_object, const OpPoint& point) {}
#endif // DND_DRAGOBJECT_SUPPORT

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
		virtual void            OnSearchSuggestionsUsed(OpWindowCommander* commander, BOOL suggestions) {}
#endif // DOC_SEARCH_SUGGESTIONS_SUPPORT

#ifdef SUPPORT_VISUAL_ADBLOCK
		virtual void			OnContentBlocked(OpWindowCommander* commander, const uni_char *image_url) {}
		virtual void			OnContentUnblocked(OpWindowCommander* commander, const uni_char *image_url) {}
#endif // SUPPORT_VISUAL_ADBLOCK

		virtual void			OnAccessKeyUsed(OpWindowCommander* commander) {}
        virtual void			OnConfirm(OpWindowCommander* commander, const uni_char *message, DialogCallback *callback) {}
		virtual void			OnGenericError(OpWindowCommander* commander, const uni_char *title, const uni_char *message, const uni_char *additional = NULL) {}
        virtual void			OnJSAlert(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, JSDialogCallback *callback);
        virtual void			OnJSConfirm(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, JSDialogCallback *callback);
        virtual void			OnJSPrompt(OpWindowCommander* commander, const uni_char *hostname, const uni_char *message, const uni_char *default_value, JSDialogCallback *callback);
        virtual void			OnJSPopup(OpWindowCommander* commander,	const uni_char *url, const uni_char *name, int left, int top, int width, int height, BOOL3 scrollbars, BOOL3 location, BOOL refuse, BOOL unrequested, JSPopupCallback *callback) { callback->Continue(); }
# if defined WIC_CAP_JAVASCRIPT_PRINT && defined _PRINT_SUPPORT_
		virtual void			OnJSPrint(OpWindowCommander * commander, PrintCallback * callback){ callback->Cancel(); } // TODO: implement if support for js print is desired
# endif // WIC_CAP_JAVASCRIPT_PRINT && _PRINT_SUPPORT_
# if defined (DOM_GADGET_FILE_API_SUPPORT) && defined (WIC_CAP_HAS_MOUNTPOINT_API)  // TODO: implement if to by used for Gadgets
		virtual void			OnMountpointFolderRequest(OpWindowCommander* commander, MountPointFolderCallback* callback, MountPointFolderCallback::FolderType folder_type) { callback->MountPointCallbackCancel(); }
		virtual void			OnMountpointFolderCancel(OpWindowCommander* commander, MountPointFolderCallback* callback) {}
# endif // DOM_GADGET_FILE_API_SUPPORT && WIC_CAP_HAS_MOUNTPOINT_API
# if defined WIC_CAP_USER_PRINT && defined _PRINT_SUPPORT_
		virtual void			OnUserPrint(OpWindowCommander * commander, PrintCallback * callback){ callback->Cancel(); } // TODO: implement if support for user print is desired
# endif // WIC_CAP_JAVASCRIPT_PRINT && _PRINT_SUPPORT_

        virtual void			OnContentResized(OpWindowCommander* commander, INT32 new_width, INT32 new_height) {}
		virtual void			OnContentDisplay(OpWindowCommander* commander){};

        virtual void			OnContentMagicFound(OpWindowCommander* commander, INT32 x, INT32 y) {}
        virtual void			OnContentMagicLost(OpWindowCommander* commander) {}
        virtual void			OnFormSubmit(OpWindowCommander* commander, FormSubmitCallback *callback) { callback->Continue(TRUE); }

        virtual void			OnMailTo(OpWindowCommander* commander, const uni_char* url) {}
    	virtual void			OnMailTo(OpWindowCommander*, const OpStringC8&, BOOL, BOOL, const OpStringC8&) {}

        virtual void			OnTel(OpWindowCommander* commander, const uni_char* url) {}
        virtual BOOL			OnUnknownProtocol(OpWindowCommander* commander, const uni_char* url) { return FALSE; }
        virtual void			OnUnhandledFiletype(OpWindowCommander* commander, const uni_char* url) {}

#ifdef _WML_SUPPORT_
		virtual void			OnWmlDenyAccess(OpWindowCommander* commander) {}
#endif // _WML_SUPPORT_
#ifdef WIC_CAP_UPLOAD_FILE_CALLBACK
		virtual BOOL			OnUploadConfirmationNeeded(OpWindowCommander* commander);
		virtual void			OnUploadRequest(OpWindowCommander* commander, UploadCallback* callback) {callback->Cancel();}
		virtual void			OnUploadCancel(OpWindowCommander* commander, UploadCallback* callback) {}
#endif // WIC_CAP_UPLOAD_FILE_CALLBACK
		virtual BOOL			OnRefreshUrl(OpWindowCommander* commander, const uni_char* url) { return FALSE; }
#ifdef RSS_SUPPORT
		virtual void			OnRss(OpWindowCommander* commander, const uni_char* url) {}
		virtual void			OnSubscribeFeed(OpWindowCommander* commander, const uni_char* url) {}
#endif // RSS_SUPPORT
        virtual void            OnPluginPost(OpWindowCommander* commander, PluginPostCallback *callback) { callback->Continue(TRUE); }
		virtual void			OnAskAboutUrlWithUserName(OpWindowCommander* commander, const uni_char* url, const uni_char* hostname, const uni_char* username, DialogCallback* callback) { callback->OnDialogReply(DialogCallback::REPLY_CANCEL); }
		virtual void			OnAskAboutFormRedirect(OpWindowCommander* commander, const uni_char* source_url, const uni_char* target_url, DialogCallback* callback) { callback->OnDialogReply(DialogCallback::REPLY_CANCEL); }
#ifdef WIC_CAP_SSL_WARNING
		virtual void			OnSSLWarning(OpWindowCommander* commander, uni_char* text, DialogCallback* callback)  {callback->OnDialogReply(DialogCallback::REPLY_CANCEL);}
#endif
		virtual void			OnFormRequestTimeout(OpWindowCommander* commander, const uni_char* url) {}
#ifdef DATABASE_MODULE_MANAGER_SUPPORT
		virtual void 			OnIncreaseQuota(OpWindowCommander* commander, const uni_char* db_name, const uni_char* db_domain, const uni_char* db_type, OpFileLength current_quota_size, OpFileLength quota_hint, QuotaCallback *callback);
#endif // DATABASE_MODULE_MANAGER_SUPPORT
#ifdef WEBSERVER_SUPPORT
#ifdef M2_SUPPORT
	virtual void GetExternalEmbedPermission(OpWindowCommander* commander, BOOL& has_permission) {};
	virtual void IsSpamMessage(OpWindowCommander* commander, BOOL& is_spam) {};
	virtual void OnExternalEmbendBlocked(OpWindowCommander* commander) {};
#endif // M2_SUPPORT
    // == WebSubserverEventListener ======================
	virtual void OnNewRequest(WebserverRequest *request);
	virtual void OnTransferProgressIn(double transfer_rate, UINT bytes_transfered,  WebserverRequest *request, UINT tcp_connections);
	virtual void OnTransferProgressOut(double transfer_rate, UINT bytes_transfered, WebserverRequest *request, UINT tcp_connections, BOOL request_finished);

    // == OpTimerListener ======================
	virtual void OnTimeOut(OpTimer *timer);

	void BroadcastDesktopGadgetActivityChange(DesktopGadgetActivityState act_state, time_t seconds_since_last_request = 0);
	void BroadcastGetAttention();

	void SetWebsubServerListener();
	void RemoveWebsubServerListener();
#endif // WEBSERVER_SUPPORT

	void ShowGadget();
};

#endif // GADGET_SUPPORT

#endif // DESKTOP_GADGET_H
