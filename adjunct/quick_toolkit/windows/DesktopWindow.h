/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DESKTOP_WINDOW_H
#define DESKTOP_WINDOW_H

class WidgetContainer;
class OpWindowListener;
class OpWorkspace;
class DesktopOpWindow;
class OpSession;
class OpSessionWindow;
class OpWindowCommander;
class Dialog;
class PagebarButton;
class DesktopGroupModelItem;

#include "modules/util/OpTypedObject.h"
#include "modules/util/adt/oplisteners.h"
#include "adjunct/desktop_util/settings/SettingsListener.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#include "adjunct/desktop_pi/DesktopOpWindow.h"
#include "adjunct/quick/application/PageLoadDispatcher.h"
#include "adjunct/quick/models/DesktopWindowModelItem.h"

#include "modules/inputmanager/inputcontext.h"

#include "modules/pi/OpWindow.h"
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
# include "modules/accessibility/opaccessibleitem.h"
#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/skin/OpWidgetImage.h"
#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
#include "adjunct/quick_toolkit/windows/DesktopWindowListener.h"

#include "modules/hardcore/mh/messobj.h"

#include "modules/widgets/OpWidget.h"
#include "modules/widgets/OpSlider.h"

#include "adjunct/quick_toolkit/widgets/OpToolTipListener.h"

#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
#include "modules/content_filter/content_filter_util.h"
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL

#define DEFAULT_SIZEPOS		INT_MAX

#ifdef MSWIN
#undef IsRestored
#undef IsMinimized
#undef IsMaximized
#endif

// defines what data should be updated on OnTrigger is called
#define UPDATE_TYPE_TITLE 0x01
#define UPDATE_TYPE_IMAGE 0x02

/***********************************************************************************
**
**	DesktopWindow
**
***********************************************************************************/

class DesktopWindow
	: public OpInputContext, public Link, public MessageObject
	, public OpWidgetListener, public OpWidgetImageListener, public OpToolTipListener
	, public SettingsListener, public OpDelayedTriggerListener
	, public DispatchableObject
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	, public OpAccessibleItem
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
{
	public:

		friend class DesktopWindowListener;
		friend class InternalDesktopWindowListener;

									DesktopWindow();
		virtual						~DesktopWindow();

		/** Initialize the desktop window
		  * @param style
		  * @param parent_window
		  * @param parent_workspace
		  * @param effects See OpWindow::Effects
		  */
		// A parent-less DesktopWindow
		OP_STATUS					Init(OpWindow::Style style, UINT32 effects = 0);
		// A DesktopWindow with a parent DesktopWindow
		OP_STATUS					Init(OpWindow::Style style, DesktopWindow* parent_window, UINT32 effects = 0);
		// A DesktopWindow with a parent OpWorkspace
		OP_STATUS					Init(OpWindow::Style style, OpWorkspace* parent_workspace, UINT32 effects = 0);
		// A DesktopWindow with a parent OpWindow
		OP_STATUS					Init(OpWindow::Style style, OpWindow* parent_opwindow, UINT32 effects = 0);
		// A DesktopWindow with a parent DesktopWindow and a parent OpWindow
		// that might not be the parent's OpWindow.
		OP_STATUS					Init(OpWindow::Style style, DesktopWindow* parent_window, OpWindow* parent_opwindow, UINT32 effects = 0);

		virtual void				Close(BOOL immediately = FALSE, BOOL user_initiated = FALSE, BOOL force = TRUE);

		void						Show(BOOL show, const OpRect* preferred_rect = NULL, OpWindow::State preferred_state = OpWindow::RESTORED, BOOL behind = FALSE, BOOL force_preferred_rect = FALSE, BOOL inner = FALSE);

		void						SetTitle(const uni_char* title);
		virtual const uni_char*		GetTitle(BOOL no_extra_info = TRUE) {return m_title.CStr();}

		void						SetStatusText( const OpString &text, DesktopWindowStatusType type = STATUS_TYPE_ALL) {SetStatusText(text.CStr(), type);}
		void						SetStatusText( const uni_char* text, DesktopWindowStatusType type = STATUS_TYPE_ALL );
		const uni_char*				GetStatusText(DesktopWindowStatusType type = STATUS_TYPE_ALL);
	    void						SetUnderlyingStatusText( const OpString &text, DesktopWindowStatusType type = STATUS_TYPE_ALL) {SetUnderlyingStatusText(text.CStr(), type);}
	    void                        SetUnderlyingStatusText( const uni_char* text, DesktopWindowStatusType type );
	    const uni_char*	            GetUnderlyingStatusText( DesktopWindowStatusType type  );




		void						Enable(BOOL enable);
		void						Raise();
		void						Lower();

		void						Restore();
		void						Minimize();
		void						Maximize();
		BOOL						Fullscreen(BOOL fullscreen);

		/** Set current visibility of window
		  * @param hidden Whether this window is completely hidden from view (ie.
		  *               not visible on the screen)
		  */
		virtual void				SetHidden(BOOL hidden) {}

		BOOL						IsActive() {return m_active;}

		/** Return TRUE if this window is currently visible.
			This will return FALSE if the window is covered by other windows/views (that is not children to this window). */
		BOOL						IsVisible() const {return m_visible;}

		/** Return TRUE if this window is currently shown (By f.ex calling Show() or SetDesktopPlacement).
			This may return TRUE even if the window is covered by other windows/views, as it's only reflecting the show status. */
		BOOL						IsShowed() {return m_showed;}
		/**
		 * @return @c TRUE iff this window has been shown at least once
		 */
		virtual BOOL				HasBeenShown() const { return FALSE; }

		BOOL						IsRestored();
		BOOL						IsMinimized();
		BOOL						IsMaximized();
		BOOL						IsFullscreen();

		// Some platforms/configurations do not have compositing. We need to know when
		BOOL 						SupportsExternalCompositing();

#ifdef DEVTOOLS_INTEGRATED_WINDOW
		virtual void				AttachDevToolsWindow(BOOL attach) {}
		virtual BOOL				HasDevToolsWindowAttached() { return FALSE; }
		virtual BOOL				IsDevToolsOnly() { return FALSE; }
		virtual void				UpdateWindowIcon() {}
#endif // DEVTOOLS_INTEGRATED_WINDOW
		//determines if this window is a dialog
		BOOL						IsDialog() {return (GetType() >= DIALOG_TYPE && GetType() < DIALOG_TYPE_LAST); }
		BOOL						IsLocked() {return m_lock_count > 0;}

		void						LockUpdate(BOOL lock);

		void						GetOuterSize(UINT32& width, UINT32& height);
		void						SetOuterSize(UINT32 width, UINT32 height);

		void						GetInnerSize(UINT32& width, UINT32& height);
		void						SetInnerSize(UINT32 width, UINT32 height);

		void						GetOuterPos(INT32& x, INT32& y);
		void						SetOuterPos(INT32 x, INT32 y);

		void						GetInnerPos(INT32& x, INT32& y);
		void						SetInnerPos(INT32 x, INT32 y);

		void						SetMinimumInnerSize(UINT32 width, UINT32 height);
		void						SetMaximumInnerSize(UINT32 width, UINT32 height);

		INT32						GetOuterWidth();
		INT32						GetOuterHeight();
		INT32						GetInnerWidth();
		INT32						GetInnerHeight();

		/** Set the icon associated with this window
		  * @param image Icon to set
		  * @param propagate_to_native_window Whether to set this icon on the native (OS) window as well
		  */
		void						SetIcon(Image& image, BOOL propagate_to_native_window = TRUE);
		void						SetIcon(const OpStringC8& image, BOOL propagate_to_native_window = TRUE);

		const OpWidgetImage*		GetWidgetImage() const {return &m_image;}
		void						SetWidgetImage(const OpWidgetImage* widget_image);

		void						GetBounds(OpRect& rect);
		void						GetRect(OpRect& rect);
		// Gets the rect in screen coordinate
		void						GetAbsoluteRect(OpRect& rect);
		void						GetScreenRect(OpRect& rect, BOOL workspace = FALSE);
		/*	Adjust a rect to make it fit on the screen.
			@param rect				The rect that needs to be adjusted
			@param fully_visible	Set TRUE if the rect needs to be complete visible on the screen
									If FALSE, the rect will not be modified if a substantial amount of the rect is visible
			@param full_screen		Set as false if the rect needs to be adjusted to the size of the workspace	*/
		void						FitRectToScreen(OpRect& rect, BOOL fully_visible = TRUE, BOOL fullscreen = FALSE);

		void						SetSkin(const char* skin, const char* fallback_skin = NULL);
		const char*					GetSkin() { return GetSkinImage()->GetImage(); }
		OpWidgetImage*				GetSkinImage();

		virtual	const char*			GetFallbackSkin() {return "Window Skin";}
		virtual void				EnableTransparentSkins(BOOL enabled, BOOL top_only, BOOL force_update = FALSE) {}

		void						BroadcastDesktopWindowChanged();
		void						BroadcastDesktopWindowStatusChanged(DesktopWindowStatusType type);
		void						BroadcastDesktopWindowClosing(BOOL user_initiated);
		void						BroadcastDesktopWindowContentChanged();
		void						BroadcastDesktopWindowBrowserViewChanged();
		void						BroadcastDesktopWindowShown(BOOL shown);
		void						BroadcastDesktopWindowPageChanged();
		void						BroadcastDesktopWindowAnimationRectChanged(const OpRect &source, const OpRect &target);

		void						SetIsClosing(BOOL closing) { m_is_closing = closing; }
		BOOL						IsClosing() { return m_is_closing; }

		virtual void				SetAttention(BOOL attention);
		BOOL						HasAttention() {return m_attention;}

		virtual BOOL				Activate(BOOL activate = TRUE, BOOL parent_too = TRUE);

		DEPRECATED(OpWindow* GetWindow()); // Inlined below
		OpWindow*					GetOpWindow() const {return m_window;}
		OpWindow*					GetParentWindow()	{return m_window->GetParentWindow();}
		OpView*						GetParentView()		{return m_window->GetParentView();}
		OpWindow::Style				GetStyle() const	{return m_style;}

		PagebarButton*				GetPagebarButton();

		// Implement this for the window if it needs a specific name so the Automated UI testing 
		// framework can identify and find the window, otherwise use the generic name
		// (e.g. All simple dialogs now have a name so we can confirm the correct dialog is shown)
	    virtual const char*         GetSpecificWindowName() { return GetWindowName(); }
		// Generic name of the window (e.g. "Simple Dialog", "Desktop Window"
		virtual const char*			GetWindowName() {return NULL;}
		virtual const uni_char*		GetWindowDescription() {return UNI_L("");}
		virtual WidgetContainer*	GetWidgetContainer() { return m_widget_container; }
		virtual OpWorkspace*		GetWorkspace() { return m_parent_workspace; }
		virtual OpWindowCommander*	GetWindowCommander() {return NULL;}
		virtual OpBrowserView*		GetBrowserView() {return NULL;}
		virtual	OpWidget*			GetWidgetByTypeAndId(Type type, INT32 id = -1);
		virtual OpWidget*			GetWidgetByType(Type type) {return GetWidgetByTypeAndId(type, -1);}
		virtual OpWidget*			GetWidgetByName(const OpStringC8 & name);

		/**
		 * Gets a widget that matches both a name and type.
		 * @param T target OpWidget (sub)class
		 * @param name widget name
		 * @param type type constant of @a T
		 * @return widget pointer or @c NULL if no match was found
		 */
		template <typename T>
		T* GetWidgetByName(const OpStringC8& name, Type type)
		{
			OpWidget* widget = GetWidgetByName(name);
			T* typed_widget = widget != NULL && widget->IsOfType(type) ? static_cast<T*>(widget) : NULL;
			OP_ASSERT(typed_widget == NULL || typed_widget->T::IsOfType(type) || !"type <-> type constant mismatch");
			return typed_widget;
		}

		virtual DesktopWindow*		GetParentDesktopWindow();
	    virtual DesktopWindow*      GetToplevelDesktopWindow();
		virtual OpWorkspace*		GetParentWorkspace() {return m_parent_workspace; }
		virtual void				SetParentWorkspace(OpWorkspace* workspace, BOOL activate = TRUE);
		virtual void				ResetParentWorkspace();

		virtual double				GetWindowScale() { return GetWindowCommander() ? GetWindowCommander()->GetScale() * 0.01 : 1.0 ; }
		virtual void				QueryZoomSettings(double &min, double &max, double &step, const OpSlider::TICK_VALUE* &tick_values, UINT8 &num_tick_values) { }

		DesktopGroupModelItem*		GetParentGroup();

		virtual void				SetFocus(FOCUS_REASON reason);
		virtual void				RestoreFocus(FOCUS_REASON reason);

		virtual void				Relayout();
		virtual void				SyncLayout();

		/*
		* This function exist here as a temporary hack, that should be removed 
		* as soon as possible. 
		* Long story: close window messages comes from platform
		* for widgets it goes to GadgetContainerWindow, and has to be passed to DesktopGadget
		* This is because of incorrect implementation of window / object connetion 
		* caused by the need of support unite and widgets by DesktopGadget. 
		* this functionality should be splited, and will allow to correct implementation. 
		*
		* Short story: function called while InternalDesktopWindowListener::OnClose performs operations
		* and breaks it, if return value is TRUE
		* used to handle redirection of close operation to different window
		*/
		virtual BOOL	            PreClose(){return FALSE;}

		// BrowserDesktopWindow and DocumentDesktopWindow support private browsing
		virtual BOOL				PrivacyMode(){return FALSE;}
		virtual void				SetPrivacyMode(BOOL mode){}

	    OP_STATUS			        AddListener(DesktopWindowListener* listener) { return m_listeners.Add(listener); }
		void						RemoveListener(DesktopWindowListener* listener) {m_listeners.Remove(listener);}

		void						SetSavePlacementOnClose(BOOL save) {m_save_placement_on_close = save;}
		BOOL						GetSavePlacementOnClose() {return m_save_placement_on_close;}

		void						AddDialog(Dialog* dialog);
		void						RemoveDialog(Dialog* dialog);
		INT32						GetDialogCount() {return m_dialogs.GetCount();}
		Dialog*						GetDialog(INT32 index) {return m_dialogs.Get(index);}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		// accessibility methods
		virtual BOOL				AccessibilityIsReady() const {return TRUE;}
		virtual OpWindow*			AccessibilityGetWindow() const {return m_window;}
		virtual OP_STATUS			AccessibilityClicked();
		virtual OP_STATUS			AccessibilitySetValue(int value);
		virtual OP_STATUS			AccessibilitySetText(const uni_char* text);
		virtual OP_STATUS			AccessibilityGetAbsolutePosition(OpRect &rect);
		virtual OP_STATUS			AccessibilityGetValue(int &value);
		virtual OP_STATUS			AccessibilityGetText(OpString& str);
		virtual OP_STATUS			AccessibilityGetDescription(OpString& str);
		virtual OP_STATUS			AccessibilityGetKeyboardShortcut(ShiftKeyState* shifts, uni_char* kbdShortcut);
		virtual Accessibility::State AccessibilityGetState();
		virtual Accessibility::ElementKind AccessibilityGetRole() const;
		virtual int					GetAccessibleChildrenCount();
		virtual OpAccessibleItem*	GetAccessibleParent();
		virtual OpAccessibleItem*	GetAccessibleChild(int);
		virtual int GetAccessibleChildIndex(OpAccessibleItem* child);
		virtual OpAccessibleItem*	GetAccessibleChildOrSelfAt(int x, int y);
		virtual OpAccessibleItem*	GetNextAccessibleSibling();
		virtual OpAccessibleItem*	GetPreviousAccessibleSibling();
		virtual OpAccessibleItem*	GetAccessibleFocusedChildOrSelf();

		virtual OpAccessibleItem*	GetLeftAccessibleObject();
		virtual OpAccessibleItem*	GetRightAccessibleObject();
		virtual OpAccessibleItem*	GetDownAccessibleObject();
		virtual OpAccessibleItem*	GetUpAccessibleObject();

#endif //ACCESSIBILITY_EXTENSION_SUPPORT

		// Settings listener
		
		// if you override this function, you should still call it, to make sure all toolbars and widgets that want this message get it
		virtual void				OnSettingsChanged(DesktopSettings* settings);

		// Hooks that subclasses can override

		virtual void				OnRelayout() {}
		virtual void				OnLayout() {}
		virtual void				OnResize(INT32 width, INT32 height) {}
		virtual void                OnLayoutAfterChildren() {}
		virtual void				OnClose(BOOL user_initiated) {}
		virtual void				OnMove() {}
		virtual void				OnActivate(BOOL activate, BOOL first_time) {}
		virtual void				OnFullscreen(BOOL fullscreen) {}
		virtual void				OnShow(BOOL show) {}
		virtual void				OnDropFiles(const OpVector<OpString>& file_names);
		virtual void				OnSessionReadL(const OpSessionWindow* session_window) {}
		virtual void				OnSessionWriteL(OpSession* session, OpSessionWindow* session_window, BOOL shutdown) {}
		virtual OP_STATUS			AddToSession(OpSession* session, INT32 parent_id, BOOL shutdown, BOOL extra_info = FALSE);
		// Used for dialogs if you want a dialog to cascade above its parent rather than centre
		virtual BOOL				IsCascaded() { return FALSE; }
		// Overload to specify if the window should be disabled for a few milliseconds when activated to protect from doubleclicking the page
		virtual BOOL				GetProtectAgainstDoubleClick() { return FALSE; }
		virtual BOOL				IsMouseGestureAllowed() { return TRUE; }
		// Whether window should be visible in the window list / panel
		virtual BOOL				VisibleInWindowList() { return GetParentWorkspace() || m_style == OpWindow::STYLE_DESKTOP; }
#ifdef PLUGIN_AUTO_INSTALL
		virtual void				OnPluginAvailable(const OpStringC& mime_type) {}
		virtual void				OnPluginInstalled(const OpStringC& mime_type) {}
		virtual void				OnPluginDownloadStarted(const OpStringC& mime_type) {}
#endif // PLUGIN_AUTO_INSTALL

		// Specify whether window should be shown / hidden on global show / hide commands
		virtual BOOL				RespectsGlobalVisibilityChange(BOOL visible);

		/** Called when the UI language changes.  By default, propagates the
		  * RTL setting to all the OpWidgets in this window.
		  */
		virtual void				UpdateLanguage();

		// OpWidgetListener

		virtual void				OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		virtual void				OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		virtual void				OnRelayout(OpWidget* widget);
		virtual void				OnLayout(OpWidget* widget);
		virtual void                OnLayoutAfterChildren(OpWidget* widget);

		// Implementing OpWidgetImageListener interface

		virtual void				OnImageChanged(OpWidgetImage* widget_image) {BroadcastDesktopWindowChanged();}

		// OpInputContext

		virtual BOOL				IsInputContextAvailable(FOCUS_REASON reason) {return IsActive();}
		virtual BOOL				IsParentInputContextAvailabilityRequired() {return m_parent_workspace != NULL;}
		virtual BOOL				IsInputDisabled() {return m_disabled_count > 0;}
		virtual BOOL				OnInputAction(OpInputAction* action);

		// Implementing DesktopWindowModelItem interface

		Type						GetType() {return WINDOW_TYPE_UNKNOWN;}
		INT32						GetID() {return m_id;}
		OP_STATUS					GetItemData(ItemData* item_data);
		DesktopWindowModelItem&		GetModelItem() { return m_model_item; }

		/** @return Type used to initialize the OpWindow for this DesktopWindow
		  */
		virtual Type				GetOpWindowType() { return GetType(); }

		// HandleCallback

		virtual void				HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

		virtual BOOL				RestoreFocusWhenActivated() {return TRUE;}

		/* Sets whether the user has "pinned" the window/tab.
		 *
		 * When the window/tab is "pinned", many ways of closing the
		 * window/tab should no longer work (e.g. from the UI or from
		 * javascript).
		 *
		 * This does not block the window/tab from being closed in all
		 * cases, e.g. when opera is exiting.
		 */
		virtual void				SetLockedByUser(BOOL locked);
		/* Returns whether the user has "pinned" the window/tab.
		 */
		virtual BOOL				IsLockedByUser() { return m_locked_by_user; }

		/** Updates the visual style of the pagebar button associated
		 *  with this window, if any.
		 */
		virtual void				UpdateUserLockOnPagebar();

		/** Block this window from being closed in (almost) any way.
		 *
		 * Until UnblockWindowClosing() is called, this window will
		 * not be closed.  This is not entirely true, so can not be
		 * trusted completely.  E.g. if opera is exiting, this block
		 * will probably be ignored.
		 *
		 * The effects of this method are somewhat similar to
		 * SetLockedByUser(TRUE).  However, that method indicates that
		 * the user does not want to lose the window/tab, whereas this
		 * method indicates that there is some piece of opera's code
		 * that wants to ensure that the window remains available.
		 * Thus the effects may be subtly different.
		 *
		 * It is necessary to call UnblockWindowClosing() the same
		 * number of times as BlockWindowClosing() has been called
		 * before the window can be closed again.
		 */
		virtual void				BlockWindowClosing()
		{ m_windowclosingblocked += 1; OP_ASSERT(m_windowclosingblocked > 0); };
		/** Undo one call to BlockWindowClosing().
		 *
		 * See BlockWindowClosing() for details, but note that each
		 * call to this method only counters a single call to
		 * BlockWindowClosing().
		 */
		virtual void				UnblockWindowClosing()
		{ OP_ASSERT(m_windowclosingblocked > 0); m_windowclosingblocked -= 1; };

		virtual	Image				GetThumbnailImage() { return DesktopWindow::GetThumbnailImage(THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT); }
		virtual	Image				GetThumbnailImage(UINT32 width, UINT32 height, BOOL high_quality = FALSE, BOOL use_skin_image_if_present = TRUE);
		virtual	Image				GetThumbnailImage(OpPoint &logo);
		
		// is the thumbnail image returned above a fixed (skin) image or a dynamically created image
		virtual BOOL				HasFixedThumbnailImage() { return m_uses_fixed_thumbnail_image; }

		// if the callee needs to know if a fixed image will be used, it can call this method
		void						UpdateUseFixedImage();

		// get a full sized snapshot of the window
		virtual Image				GetSnapshotImage();

		/* NOTE: I believe this also blocks (some/most)
		 * non-user-initiated closings as well.  Should it be renamed
		 * to IsClosable()?  (And documented.  That would be nice...)
		 *
		 * Attempt at documentation (but I don't know if I've got it
		 * right):
		 *
		 * As long as this method returns true, most attempts at
		 * closing the window/tab will fail.  In particular, closing
		 * the window/tab through the UI or from javascript will fail.
		 * However, this does not guarantee that the window/tab will
		 * not be closed, e.g. when opera exits (and it doesn't
		 * currently block the user from exiting opera (even through
		 * the UI), but such details may change and should not be
		 * depended upon).
		 */
		virtual BOOL				IsClosableByUser()
		{ return !IsLockedByUser() && m_windowclosingblocked == 0; }

		// == OpToolTipListener =============================
		virtual BOOL			HasToolTipText(OpToolTip* tooltip) {return TRUE;}
		virtual void			GetToolTipText(OpToolTip* tooltip, OpInfoText& text);
		virtual void			GetToolTipThumbnailText(OpToolTip* tooltip, OpString& title, OpString& url_string, URL& url);

		// == OpDelayedTriggerListener ======================

		virtual void			OnTrigger(OpDelayedTrigger*);

		OP_STATUS					init_status;

		// == OpScopeDesktopWindowManager =======
		virtual OP_STATUS			ListWidgets(OpVector<OpWidget> &widgets);
		OP_STATUS					ListWidgets(OpWidget* widget, OpVector<OpWidget> &widgets);

		//
		// DispatchableObject interface
		//

		virtual int					GetPriority() const { return -1; }
		virtual OP_STATUS			InitObject();

    protected:

        // bad hack.. core should stop using OpWindows

        void				        ResetWindowListener();

	private:
		OP_STATUS					Init(OpWindow::Style style, OpWindow* parent_opwindow, DesktopWindow* parent_window, OpWorkspace* parent_workspace, UINT32 effects);

		void						UpdateSkin();
		void						SetWindowIconFromIcon();
		BOOL						MatchText(const uni_char* string);
		bool						IsModalDialog() const { return m_style == OpWindow::STYLE_MODAL_DIALOG || m_style == OpWindow::STYLE_BLOCKING_DIALOG; }

	// ===================================================

		OpVector<Dialog>			m_dialogs;
		OpWindow::Style				m_style;
		DesktopOpWindow*			m_window;
		OpWindowListener*			m_window_listener;
		OpWorkspace*				m_parent_workspace;
		OpListeners<DesktopWindowListener>		m_listeners;
		OpWindow::State				m_old_state;
		BOOL						m_fullscreen;
		BOOL						m_save_placement_on_close;
		BOOL						m_active;
		BOOL						m_visible;
		BOOL						m_showed;
		BOOL						m_attention;
		BOOL						m_locked_by_user;
		unsigned int				m_windowclosingblocked;
		BOOL						m_is_closing;
		OpWidgetImage				m_image;
		INT32						m_id;
		INT32						m_disabled_count;
		INT32						m_lock_count;
		WidgetContainer*			m_widget_container;
		OpString					m_title;
		OpString8	   				m_skin;
		OpString	   				m_status_text[STATUS_TYPE_TOTAL];
	    OpString                    m_underlying_status_text[STATUS_TYPE_TOTAL];
		OpDelayedTrigger			m_delayed_trigger;
		OpString8					m_delayed_icon_image;
		BOOL						m_delayed_propagate_to_native_window;
		BOOL                        m_delayed_has_image_data;
		UINT32                      m_delayed_update_flags;
		BOOL						m_uses_fixed_thumbnail_image;			// if TRUE, the image supplied with GetThumbnailImage() is a fixed skin image
		DesktopWindowModelItem		m_model_item;
};

/***********************************************************************************
 **
 **	DesktopWindowSpy
 **
 **	A base helper class to inherit from if one wants "spy"
 **	on the currently active window and its main OpBrowserView, if any
 **
 ***********************************************************************************/

class DesktopWindowSpy : public DesktopWindowListener, public OpPageListener
{
	public:

								DesktopWindowSpy(BOOL prefer_windows_with_browser_view);
		virtual					~DesktopWindowSpy();

		void					SetSpyInputContext(OpInputContext* input_context, BOOL send_change = TRUE);
		OpInputContext*			GetSpyInputContext() {return m_input_context;}

		void					UpdateTargetDesktopWindow();

		DesktopWindow*			GetTargetDesktopWindow() {return m_desktop_window;}
		OpBrowserView*			GetTargetBrowserView() {return m_desktop_window ? m_desktop_window->GetBrowserView() : NULL;}

		// Overload this hook to know when a new target is set, which may be NULL (= no target)

		virtual void			OnTargetDesktopWindowChanged(DesktopWindow* target_window) {}

		// DesktopWindowListener

		virtual void			OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);

		virtual void			OnDesktopWindowContentChanged(DesktopWindow* desktop_window);
		
		virtual void			OnDesktopWindowBrowserViewChanged(DesktopWindow* desktop_window);

	private:

		class SpyInputStateListener : public OpInputStateListener
		{
			public:

				SpyInputStateListener(DesktopWindowSpy* spy) : m_spy(spy)
				{
					m_input_state.SetInputStateListener(this);
					m_input_state.SetWantsOnlyFullUpdate(TRUE);
				}
				virtual ~SpyInputStateListener() { }

				void Enable(BOOL enable)
				{
					m_input_state.SetEnabled(enable);
				}

				// OpInputStateListener

				virtual void OnUpdateInputState(BOOL full_update)
				{
					m_spy->UpdateTargetDesktopWindow();
				}

			private:

				DesktopWindowSpy*			m_spy;
				OpInputState				m_input_state;
		};

		void					SetTargetDesktopWindow(DesktopWindow* desktop_window, BOOL send_change = TRUE);

		SpyInputStateListener	m_input_state_listener;
		DesktopWindow*			m_desktop_window;
		OpInputContext*			m_input_context;
		BOOL					m_prefer_windows_with_browser_view;
};


/***********************************************************************************
 **
 **	ContentBlockTreeModel
 **
 **
 ***********************************************************************************/

#ifdef SUPPORT_VISUAL_ADBLOCK
#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
class ContentBlockTreeModel : public TemplateTreeModel<ContentFilterItem>
#else
class ContentBlockTreeModel : public SimpleTreeModel
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL
{
	public:
#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
		ContentBlockTreeModel(INT32 column_count = 1) : TemplateTreeModel<ContentFilterItem>(column_count) {}
#else
		ContentBlockTreeModel(INT32 column_count = 1) : SimpleTreeModel(column_count) {}
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL

		SimpleTreeModelItem* Find(const uni_char *url)
		{
			UINT32 cnt;

			for(cnt = 0; cnt < (UINT32)GetCount(); cnt++)
			{
				const uni_char *this_url = GetItemString(cnt);

				if(uni_strcmp(this_url, url) == 0)
				{
					return GetItemByIndex(cnt);
				}
			}
			return NULL;
		}
};
#endif // SUPPORT_VISUAL_ADBLOCK

// Inlined here to allow both inlining and deprecation
inline OpWindow* DesktopWindow::GetWindow() {return GetOpWindow();}

#endif
