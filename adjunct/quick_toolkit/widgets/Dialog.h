/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DIALOG_H
#define DIALOG_H

#include "adjunct/quick_toolkit/widgets/DialogListener.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick/windows/DocumentWindowListener.h"
#include "adjunct/quick/quick-widget-names.h"

// Let the Dialog superclass have the first 100 widget IDs for itself

#define FIRST_FREE_DIALOG_WIDGET_ID		100
#define	YNC_CANCEL						1

class OpGroup;
class OpProgressBar;
class OpTreeView;
class SimpleTreeModel;
class OpExpand;
class OpOverlayLayoutWidget;
class OpLabel;
class OpTabs;
class OpButtonStrip;


/***********************************************************************************
**
**	Dialog
**
***********************************************************************************/

class Dialog : public DesktopWindow, public DesktopWindowListener, public OpTimerListener, public DocumentWindowListener
{
	public:

		enum DialogType
		{
			TYPE_OK,
			TYPE_CLOSE,
			TYPE_CANCEL,
			TYPE_OK_CANCEL,
			TYPE_YES_NO,
			TYPE_YES_NO_CANCEL,
			TYPE_PROPERTIES,
			TYPE_WIZARD,
			TYPE_NO_YES
		};

		enum DialogImage
		{
			IMAGE_NONE,
			IMAGE_WARNING,
			IMAGE_INFO,
			IMAGE_QUESTION,
			IMAGE_ERROR
		};

		enum DialogAnimType
		{
			POSITIONAL_ANIMATION_STANDARD,
			POSITIONAL_ANIMATION_SHEET,
			POSITIONAL_ANIMATION_NONE
		};
		
		enum DialogResultCode
		{
			DIALOG_RESULT_OK  = 1,
			DIALOG_RESULT_YES = 1,
			DIALOG_RESULT_CANCEL = 2,
			DIALOG_RESULT_NO     = 2,
			DIALOG_YNC_RESULT_NO = 3 // 'NO' value used for Yes-No-Cancel dialog boxes (See SimpleDialog.cpp)
		};

		enum
		{
			PAGE_INITED		= 0x01,
			PAGE_CHANGED	= 0x02
		};

								Dialog();
		virtual					~Dialog();

		// Init is the starting point for a dialog.

		OP_STATUS				Init(DesktopWindow* parent_window, INT32 start_page = 0, BOOL modal = FALSE, OpBrowserView *parent_browser_view = NULL);

		// CloseDialog must be used to close a dialog. Close() is made private so that it is unavailable

		void					CloseDialog(BOOL call_cancel, BOOL immediatly = FALSE, BOOL user_initiated = FALSE);

		// Set a listener, notified when dialog closes.

		void					SetDialogListener(DialogListener* listener) {m_listener = listener;}

		// Get the parent desktop window if any

		DesktopWindow*			GetParentDesktopWindow() {return m_parent_desktop_window;}

		// Get the pages group root.. used by Application::WriteDialog

		OpGroup*				GetPagesGroup() {return m_pages_group;}

		/********* Now follows functions that subclasses overload at will to cause different behaviour ***********/

		// dialogs can override if something else than typical Ok, Cancel dialog to gain premade different behavior

		virtual DialogType		GetDialogType() {return TYPE_OK_CANCEL;}

		// OnInitVisibility is called once, right after dialog widgets have been created, but before dialog
		// is otherwise properly initialized. Here you can hide certain widgets that will never
		// be used in this dialog, since the dialog will then maybe crop the pages if the hidden widgets
		// caused the bounding rect of the pages to become smaller.

		virtual void			OnInitVisibility() {};

		// Before, OnInitVisibility was the only place to hide/show stuff and get the dialog to auto-crop
		// and was called only once. Now Dialog class supports to do this dynamicly with the ResetDialog()
		// call. You can show/hide widgets and then call ResetDialog() to make the dialog auto crop.
		// During ResetDialog call, Dialog will call this OnReset() hook so that a subclass may do
		// additional adjustment before the final size is settled on.

		virtual void			OnReset() {};

		// OnInit is called once, before dialog is shown. Init stuff here!

		virtual void			OnInit() {};

		// OnSetFocus is called once, right after dialog is shown and default focus has been set. Set focus differetly if desired

		virtual void			OnSetFocus() {};

		// OnInitPage is called everytime a page is to be shown.

		virtual void			OnInitPage(INT32 page_number, BOOL first_time) {};

		// OnButtonClicked is called when one of the bottom dialog buttons have been clicked
		// Usually only overloaded if you did not give them any action in GetButtonInfo

		virtual void			OnButtonClicked(INT32 index) {};

		// Either OnOk and OnCancel is called after dialog is closed. Closing a dialog window will cause a OnCancel.
		// In other words, for a standard dialog, you are guaranteed that one of them are called.

		virtual UINT32			OnOk() {return 0;};
		virtual void			OnCancel() {};
		virtual void			OnYNCCancel() {};

		// For a wizard type dialog, this hook will be called when clicking forward. Overload to cause dialog to jump to a specific page.

		virtual UINT32			OnForward(UINT32 page_number) {return page_number + 1;}

		// For a wizard type dialog, this hook will be called when clicking forward. Overload to validate controls on a specific page before going to the next page.
	
		virtual BOOL			OnValidatePage(INT32 page_number) { return TRUE; }

		// For a wizard type dialog, overload this to specify if a certain page is to be considered the last one in a process.

		virtual BOOL			IsLastPage(INT32 page_number) {return GetPageByNumber(page_number)->Suc() == NULL;}

		// For lowlevel dialog buttons override (those butons at the bottom), overload GetButtonCount and GetButtonInfo

		virtual INT32			GetButtonCount();
		virtual void			GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name);

		virtual Str::LocaleString GetOkTextID();
		virtual Str::LocaleString GetCancelTextID();
		virtual Str::LocaleString GetYNCCancelTextID();
		virtual Str::LocaleString GetHelpTextID();
		virtual Str::LocaleString GetDoNotShowAgainTextID();

		// Default implementation of GetButtonCount and GetButtonInfo will call these, depending on dialog type, and you may overload them

		virtual OpInputAction*	GetOkAction() {return OP_NEW(OpInputAction, (OpInputAction::ACTION_OK));}
		virtual	const uni_char*	GetOkText() {return m_ok_text.CStr();}
		virtual OpInputAction*	GetCancelAction() {return OP_NEW(OpInputAction, (OpInputAction::ACTION_CANCEL));}
		virtual	const uni_char*	GetCancelText() {return m_cancel_text.CStr();}
		virtual OpInputAction*	GetYNCCancelAction();
		virtual	const uni_char*	GetYNCCancelText() {return m_ync_cancel_text.CStr();}
		virtual OpInputAction*	GetHelpAction();
		virtual	const uni_char*	GetHelpText() {return m_help_text.CStr();}

		virtual const uni_char* GetDoNotShowAgainText() {return m_do_not_show_text.CStr();}

		// Default help button implementation will look up this anchor in help files. Overload to set help.

		virtual	const char*		GetHelpAnchor() {return "";}

		// Overload to specify that you want a Do not show this dialog again checkbox. Query later with IsDoNotShowDialogAgainChecked()

		virtual BOOL			GetDoNotShowAgain() {return FALSE;}
		BOOL					IsDoNotShowDialogAgainChecked();
		void					SetDoNotShowDialogAgain(BOOL checked);
		BOOL					IsPageChanged(INT32 index);
		BOOL					IsPageInited(INT32 index);

		void					SetPageChanged(INT32 index, BOOL changed);

		// Overload to specify that the dialog should have a progress indicator at the bottom.
		virtual BOOL			GetShowProgressIndicator() const { return FALSE; }
		void					UpdateProgress(UINT position, UINT total, const uni_char *label = 0, BOOL show_only_label = FALSE);

		virtual BOOL HasBeenShown() const { return m_has_been_shown; }

		// Overload to specify that pages should be shown as tabs. Default for TYPE_PROPERTIES

		virtual BOOL			GetShowPagesAsTabs() {return m_dialog_type == TYPE_PROPERTIES;}

		// Overload to specify that pages should be shown as a list on the left side.

		virtual BOOL			GetShowPagesAsList() {return FALSE;}

		// Overload to hide (return FALSE) certain pages.

		virtual BOOL			GetShowPage(INT32 page_number) {return TRUE;}

		// Overload to specify that dialog should disable parent. Defaults to TRUE if dialog has a parent.

		virtual BOOL			GetModality() {return m_parent_desktop_window != NULL;}

		// Overload to specify that the dialog should become overlayed as a child to the parent window.

		virtual BOOL			GetOverlayed() {return FALSE;}

		// Overload to specify what type of positional animation that the dialog should have when opened and closed
		// (only works for overlayed dialogs)

#ifdef _MACINTOSH_
		virtual DialogAnimType	GetPosAnimationType() {return POSITIONAL_ANIMATION_SHEET;}
#else
		virtual DialogAnimType	GetPosAnimationType() {return POSITIONAL_ANIMATION_STANDARD;}
#endif
	
		// Overload to specify that the dialog should have dropshadow.

		virtual BOOL			GetDropShadow() {return FALSE;}

		// Overload to specify that the dialog should be transparent

		virtual BOOL			GetTransparent() {return FALSE;}

		// Overload to specify that the dialog should be a popup window.

		virtual BOOL			GetIsPopup() {return FALSE;}

		// Overload to specify that the dialog should not center the pages.

		virtual BOOL			GetCenterPages() {return TRUE;}

		// Overload to specify that the parent windows OpBrowserView should be dimmed as long as the dialog is open.

		virtual BOOL			GetDimPage() {return FALSE;}

		// Overload to block message processing

		virtual BOOL			GetIsBlocking() {return FALSE;}

		// Overload to allow a dialog that can be minimzed. Works only for non-modal dialogs.

		virtual BOOL			GetIsConsole() {return FALSE;}

		// Overload to get a dialog that interacts as a tool, it stays on top of the parent windows, 
		// has no borders and supports alpha transparent skinning.

		virtual BOOL			GetIsWindowTool() {return FALSE;}

		// Overload to get centered dialog buttons

		virtual BOOL			HasCenteredButtons() {return FALSE;}

		// Overload to specify if widgets should be auto scaled or not

		virtual BOOL			IsScalable() {return TRUE;}

		// Overload to specify if dialog should not be movable.

#ifdef _MACINTOSH_
		virtual BOOL			IsMovable() {return FALSE;}
#else
		virtual BOOL			IsMovable() {return TRUE;}
#endif

		// Overload to specify that is has a custom default button so dialog should not set any of the dialog buttons as default

		virtual BOOL			HasDefaultButton() {return FALSE;}

		// Overload to specify if the dialog should be disabled for a few milliseconds when activated to protect from doubleclicking the page

		virtual BOOL			GetProtectAgainstDoubleClick();

		// Overload to specify if the dialog should be resize the buttons in the button panel if the text doesn't fit

		virtual BOOL			GetIsResizingButtons() {return TRUE;}

		// Overload to specify different skin for dialog elements

		virtual const char*		GetPagesSkin() {return m_pages_skin.CStr();}
		virtual const char*		GetHeaderSkin() {return m_header_skin.CStr();}
		// Extra skin for the control buttons, default skin adds a shadow to the buttons
		virtual const char*		GetButtonBorderSkin() {return m_button_border_skin.CStr();}
		
		virtual const char*		GetDialogImage();
		virtual DialogImage		GetDialogImageByEnum() {return IMAGE_NONE;}

		// @short Overload to change the initial position and size of the dialog
		// Will be called just before the dialog is shown.
		// @param initial_placement	Contains the planned position and size, the rect can be directly adjusted
		virtual void			GetPlacement(OpRect& initial_placement) {initial_placement = m_placement_rect;};
		// @short Overload to change the initial position and size of the dialog, and set the positioning properties on the overlay layout widget.
		// Will be called just before the dialog is shown.
		// The default center the dialog.
		// @param initial_placement	Contains the planned position and size, the rect can be directly adjusted
		// @param overlay_layout_widget The widget that will position the overlayed dialog.
		virtual void			GetOverlayPlacement(OpRect& initial_placement, OpWidget* overlay_layout_widget);

		// @short Overload to hide dialog when DesktopWindow gets inactivated
		virtual BOOL			HideWhenDesktopWindowInActive() { return FALSE; }

		/********* Now follows helper functions used by subclasses  ***********/

		void					SetWidgetText(const char* name, const uni_char* text);
		void					SetWidgetText(const char* name, const char* text8);
		void					SetWidgetText(const char* name, Str::LocaleString string_id);
		void					GetWidgetText(const char* name, OpString& text, BOOL empty_if_not_found = TRUE);
		void					GetWidgetText(const char* name, OpString8& text8, BOOL empty_if_not_found = TRUE);
		OP_STATUS				GetWidgetTextUTF8(const char* name, OpString8& text8, BOOL empty_if_not_found = TRUE);
		OP_STATUS				FormatWidgetText(const char* name, Str::LocaleString format_string_id, const OpStringC& value);
		void					SetWidgetValue(const char* name, INT32 value);
		INT32					GetWidgetValue(const char* name, INT32 default_value = 0);
		void					SetWidgetEnabled(const char* name, BOOL enabled);
		void					SetWidgetReadOnly(const char* name, BOOL read_only);
		void					SetWidgetFocus(const char* name, BOOL focus = TRUE);
		BOOL					IsWidgetEnabled(const char* name);
		void					ShowWidget(const char* name, BOOL show = TRUE);

		// Lowlevel stuff..

		void					ResetDialog();
		INT32					GetCurrentPage() {return m_current_page_number;}
		void					SetCurrentPage(INT32 page_number);
		void					DeletePage(OpWidget* page);

		void					SetAllowNext(INT32 page_number, BOOL allow_next) {};
		void					EnableBackButton(BOOL enable);
		void					EnableForwardButton(BOOL enable);
		void					EnableOkButton(BOOL enable);
		void					SetForwardButtonText(const OpStringC& text);
		void					SetFinishButtonText(const uni_char *text);

		void					FocusButton(INT32 button);
		void					EnableButton(INT32 button, BOOL enable);
		void					ShowButton(INT32 button, BOOL show);
		void					SetButtonText(INT32 button, const uni_char* text);
		void					SetButtonAction(INT32 button, OpInputAction* action);
		// This functionality is needed in X-windows
		void					SetResourceName( const char *name ) { GetOpWindow()->SetResourceName(name); }

		// If a dialog has expanding sections, this dialog will change all the rects of the groups so all visible groups 
		// are adjacent to each other
		void					CompressGroups();
	
		/********* Now follows various interface implementations  ***********/

		// Subclassing hooks of DesktopWindow

		virtual	const char*		GetFallbackSkin();
		virtual void			OnClose(BOOL user_initiated);
		virtual void			OnActivate(BOOL activate, BOOL first_time);
		virtual void			OnRelayout();
		virtual void			OnRelayout(OpWidget* widget);
		virtual void			OnShow(BOOL show);
		virtual BOOL			RespectsGlobalVisibilityChange(BOOL visible) { return m_has_been_shown; }
		virtual void			SetParentWorkspace(OpWorkspace* workspace, BOOL activate = TRUE) {}
		virtual void			UpdateLanguage();

		// Implementing the DesktopWindowListener interface

		virtual void			OnDesktopWindowActivated(DesktopWindow* desktop_window, BOOL active);
		virtual void			OnDesktopWindowChanged(DesktopWindow* desktop_window);

		// == OpWidgetListener ======================

		virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);
		virtual void			OnClick(OpWidget *widget, UINT32 id);
		virtual BOOL			OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked);

		// == OpInputContext ======================

		Type					GetType() {return DIALOG_TYPE;}
		virtual BOOL			OnInputAction(OpInputAction* action);
		virtual const char*		GetInputContextName() {return "Dialog";}

		// OpTimerListener

		void					OnTimeOut(OpTimer* timer);

		// DocumentWindowListener
		virtual void	OnDocumentChanging(DocumentDesktopWindow* document_window) { }
		virtual void	OnUrlChanged(DocumentDesktopWindow* document_window, const uni_char* url){ }
		virtual void	OnLoadingFinished(DocumentDesktopWindow* document_window, OpLoadingListener::LoadingFinishStatus, BOOL was_stopped_by_user){ }

	protected:

		/**
		* Sets a label or label-like widget in bold.  Works with OpLabel and
		* OpMultilineEdit only.
		*
		* @param label_name the widget name
		*/
		void					SetLabelInBold(const char* label_name);

		BOOL					IsProtectedAgainstDoubleClick() {return m_protected_against_double_click;}
		void					SetDefaultPushButton(INT32 index);
		void					InitPage(INT32 index);
		OpWidget*				GetPageByNumber(INT32 page_number) const;
		UINT32					GetPageCount() const;
		void					SetCallCancel(BOOL call_cancel) { m_call_cancel = call_cancel; } 
		void					OkCloseDialog(BOOL immediatly = FALSE, BOOL user_initiated = FALSE); // same as CloseDialog, only that it sends the OnOk() to listeners
		OpButtonStrip*			GetButtonStrip() { return m_button_strip; }

	private:
		class DialogCloser
		{
		public:
			DialogCloser(Dialog* dialog) : m_dialog(dialog) {}
			~DialogCloser() { if (m_dialog) m_dialog->Close(TRUE); }
			void Release() { m_dialog = 0; }

		private:
			Dialog* m_dialog;
		};

		void						ReadDialog();
		const PrefsEntry*			ReadDialogWidgets(const PrefsEntry *entry, OpWidget* parent);
		void						ResetPageWidgetsToOriginalSizes(OpWidget* parent);

		virtual void				SetFocus();
		void						ScaleWidget(OpWidget* widget);
		INT32						ScaleX(INT32 x) {return (INT32) (m_x_scale * (float) x);}
		INT32						ScaleY(INT32 y) {return (INT32) (m_y_scale * (float) y);}
		void						WidgetChanged(OpWidget* widget);
		void						ShowDialogInternal();

		// Function to split appart the "group_open:group_closed" action on ACTION_SWITCH_GROUPS
		void						GetGroupsToSwitch(const OpStringC &action_string, OpString8 &group_open, OpString8 &group_closed);

		OpCheckBox*					m_do_not_show_again;
		OpProgressBar*				m_progress_indicator;
		OpTabs*						m_tabs;
		OpWidget*					m_skin_widget;
		OpWidget*					m_header_widget;
		OpLabel*					m_title_widget;
		OpWidget*					m_dialog_mover_widget;
		OpWidget*					m_image_widget;
		OpGroup*					m_content_group;
		OpGroup*					m_pages_group;
		OpString					m_finish_button_text;
		OpWidget*					m_current_page;
		INT32						m_current_page_number;
		DialogListener*				m_listener;
		OpAutoVector<INT32>			m_back_history;
		DialogType					m_dialog_type;
		DesktopWindow*				m_parent_desktop_window;
		BOOL						m_is_ok;
		BOOL						m_is_ync_cancel;
		BOOL						m_call_cancel;
		OpButtonStrip*				m_button_strip;
		OpWidgetVector<OpWidget>*	m_pages;
		OpString8					m_pages_skin;
		OpString8					m_header_skin;
		OpString8					m_button_border_skin;

		OpString					m_ok_text;
		OpString					m_cancel_text;
		OpString					m_ync_cancel_text;
		OpString					m_help_text;
		OpString					m_do_not_show_text;

		SimpleTreeModel*			m_pages_model;
		OpTreeView*					m_pages_view;

		float						m_x_scale;
		float						m_y_scale;

		INT32                       m_full_width;
		INT32                       m_full_height;

		BOOL						m_back_enabled;
		BOOL						m_forward_enabled;
		BOOL						m_ok_enabled;
		OpRect						m_placement_rect;
		BOOL 						m_has_been_shown;
		BOOL  						m_content_valid;

		OpTimer*					m_timer;
		BOOL						m_protected_against_double_click;

		OpBrowserView*				m_parent_browser_view;
		OpOverlayLayoutWidget*		m_overlay_layout_widget;
};

#endif //DIALOG_H
