/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef PAGEBAR_BUTTON_H
#define PAGEBAR_BUTTON_H

#include "modules/widgets/OpWidget.h"
#include "modules/hardcore/timer/optimer.h"
#include "modules/widgets/OpNamedWidgetsCollection.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick/windows/DocumentWindowListener.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick/widgets/OpPagebarItem.h"
#include "adjunct/quick/widgets/OpIndicatorButton.h"

class OpPagebar;
class DesktopGroupModelItem;
class OpTabGroupButton;

// Duration of animations related to tab groups (expanding/collapsing)
#define TAB_GROUP_ANIMATION_DURATION		400

/***********************************************************************************
**
**	PagebarButton - regular tab in the Pagebar
**
***********************************************************************************/

class PagebarButton : public OpPagebarItem,
						public DocumentWindowListener,
						public DesktopWindowListener
{
public:
	static OP_STATUS		Construct(PagebarButton** obj, DesktopWindow* desktop_window);

	PagebarButton(DesktopWindow* desktop_window);
	virtual ~PagebarButton();

	void					SetCloseButtonTooltip();

	/** Set if this button should act as a window mover instead of a regular tab */
	void					SetIsMoveWindowButton();

	virtual BOOL HasToolTipText(OpToolTip* tooltip)
	{
		if (IsFloating() || m_is_move_window_button)
			return FALSE;
		return GetDesktopWindow() ? GetDesktopWindow()->HasToolTipText(tooltip) : FALSE;
	}
	virtual BOOL			GetHideThumbnail(OpToolTip* tooltip) {return GetValue();}
	virtual TOOLTIP_TYPE	GetToolTipType(OpToolTip* tooltip);
	virtual void			GetToolTipText(OpToolTip* tooltip, OpInfoText& text);
	virtual void			GetToolTipThumbnailText(OpToolTip* tooltip, OpString& title, OpString& url_string, URL& url) { m_desktop_window->GetToolTipThumbnailText(tooltip, title, url_string, url); }
	virtual BOOL			GetPreferredPlacement(OpToolTip* tooltip, OpRect &ref_rect, PREFERRED_PLACEMENT &placement);
	virtual void			GetToolTipThumbnailCollection(OpToolTip* tooltip, OpVector<OpToolTipThumbnailPair>& thumbnail_collection);
	virtual INT32			GetToolTipWindowID() {return GetDesktopWindow() ? GetDesktopWindow()->GetID() : 0;}

	virtual void			OnMouseMove(const OpPoint &point);
	virtual void			OnMouseLeave();
	virtual void			OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	virtual void			OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
	virtual void			OnShow(BOOL show);

	virtual void			OnRemoving();
	virtual void			OnDeleted();
	virtual void			OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated) {m_desktop_window = NULL;}

	virtual void			OnLayout();
	virtual void			OnPaintAfterChildren(OpWidgetPainter* widget_painter, const OpRect &paint_rect);

	virtual void			GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom);
	virtual void			GetSkinMargins(INT32 *left, INT32 *top, INT32 *right, INT32 *bottom);
	virtual void			OnResize(INT32* new_w, INT32* new_h);
	virtual void			OnMove();
	virtual void			SetValue(INT32 value);

	void					InvalidateGroupRect();
	void					SetMouseMoveDragging(BOOL is_dragging);

	// Implementing the MessageObject interface
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	// Implementing the DocumentDesktopWindow::DocumentWindowListener interface
	virtual void OnStartLoading(DocumentDesktopWindow* document_window);
	virtual void OnLoadingFinished(DocumentDesktopWindow* document_window, OpLoadingListener::LoadingFinishStatus status, BOOL was_stopped_by_user);
	virtual void OnAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback);

	const OpWidgetImage *GetWidgetImage()
	{
		return m_desktop_window ? m_desktop_window->GetWidgetImage() : NULL;
	}
	INT32 GetDropArea( INT32 x, INT32 y );

	DesktopWindow		*GetDesktopWindow() { return m_desktop_window; }
	OpPagebar*			GetPagebar();

	void				UpdateTextAndIcon(bool update_icon, bool update_text, bool delay_update = false);

	virtual void		OnImageChanged(OpWidgetImage* widget_image);

	virtual void 		SetGroupNumber(UINT32 group_number, BOOL update_active_and_hidden = TRUE);
	virtual UINT32		GetGroupNumber(BOOL prefer_original_number = FALSE);

	void				SetOriginalGroupNumber(INT32 original_group_number) { m_original_group_number = original_group_number; }
	
	BOOL				IsActiveTabForGroup();
	
	BOOL                IsGrouped() { return GetGroupNumber() > 0; }
	DesktopGroupModelItem* GetGroup();

	/** Make sure the compact status reflects the desktopwindows locked status */
	void UpdateCompactStatus();

	virtual void UpdateLockedTab();

	// Is this button supposed to be rendered with a smaller size
	BOOL IsCompactButton() { return m_is_compact_button; }
	void SetIsCompactButton(BOOL compact);

	// Called when SetIsCompactButton is used to change to or from compact mode.
	virtual void OnCompactChanged() {}
	
	// virtual functions for overriding in selftests

	virtual BOOL IsLockedByUser() { return m_desktop_window && m_desktop_window->IsLockedByUser(); }
	virtual BOOL IsClosableByUser() { return m_desktop_window && m_desktop_window->IsClosableByUser(); }
	virtual BOOL WindowOnInputAction(OpInputAction* action) { return m_desktop_window->OnInputAction(action); }
	virtual BOOL IsDocumentLoading();

	virtual OpWidgetImage* GetIconSkin() { return GetForegroundSkin(); }

	virtual BOOL ShouldShowFavicon() { return TRUE; }
	virtual BOOL IsOfType(Type type) { return type == WIDGET_TYPE_PAGEBAR_BUTTON || OpPagebarItem::IsOfType(type); }
	virtual BOOL CanUseThumbnails() { return FALSE; }

	// Method to notify the button that is should render a temporary hover overlay to signify that a stack can be created.
	virtual void SetUseHoverStackOverlay(BOOL use_hover_stack_overlay);
	virtual BOOL GetUseHoverStackOverlay() { return m_use_hover_stack_overlay; }

	/** Set the button to hidden. NOTE: It may animate to hidden state so use IsHiddenOrHiding() if you also need to know if it's going to be hidden "soon" */
	virtual void SetHidden(BOOL is_hidden);
	/** Return TRUE if it's hidden currently, of on the way to become hidden by animation */
	BOOL IsHiddenOrHiding();

	/** @return an item that corresponds to this button. Can be a window or a group, depending on whether this button is a collapsed group */
	DesktopWindowCollectionItem* GetModelItem();

	DesktopDragObject* GetDragObject(OpTypedObject::Type type, INT32 x, INT32 y);

	BOOL		HasSpeedDialActive();		// TRUE if Speed Dial is active on this button, otherwise FALSE

	/** @return The location and size (rect) of this button, not including margins
	  */
	OpRect GetRectWithoutMargins();

	bool   ShowsPageLoading() { return m_show_page_loading; }

	void AddIndicator(OpIndicatorButton::IndicatorType type);

	void RemoveIndicator(OpIndicatorButton::IndicatorType type);

	OpIndicatorButton* GetIndicatorButton() { return m_indicator_button; }

	/**
	 * Schedules an activation of this button after the given amount of time.
	 * Subsequent calls will overwrite previously scheduled activations.
	 */
	void DelayedActivation(unsigned time);

	/** Cancels the scheduled activation */
	void CancelDelayedActivation();

	/** @return true if any delayed activation is scheduled currently */
	bool HasScheduledDelayedActivation();

	virtual void OnTimeOut(OpTimer*);

	virtual BOOL CanHoverToolTip() const { return TRUE; }

protected:
	virtual OP_STATUS InitPagebarButton();
	void    SetShowPageLoading(bool show_page_loading);

	virtual void SetSkin() { }

	virtual OP_STATUS GetTitle(OpString& title) { return OpPagebarItem::GetText(title); }
	// Set title and widget script
	virtual OP_STATUS SetTitle(const OpStringC& title);
	virtual BOOL ShouldUpdateIcon() { return TRUE; }

protected:
	bool IsCloseButtonVisible();

	OpButton*			m_maximize_button;
	OpButton*			m_minimize_button;
	OpButton*			m_close_button;
	OpIndicatorButton*	m_indicator_button;
	INT32				m_old_min_width;					// used when restoring min width for previously pinned tabs
	INT32				m_old_max_width;					// used when restoring max width for previously pinned tabs

private:
	// These are internal functions just for OnMouseMove updating
	void OnMouseMoveWindow(const OpPoint& point);
	void OnMouseMoveOnPagebar(const OpPoint& point);
	void StartDrag(const OpPoint& point);
	void OnMouseMoveDragging(const OpPoint& point, int delta);
	void OnDragOver(PagebarButton* target, const OpPoint& delta_coords, BOOL target_after);
	void StopMoving();
	OpTabGroupButton* GetTabGroupButton();
	void UpdateMinAndMaxWidth();

	OpRect				m_old_group_skin_rect;
	DesktopWindow*		m_desktop_window;
	BOOL				m_delayed_title_update_in_progress;		// TRUE if a title update is in progress (message has been sent)
	OpPoint				m_mousedown_point;
	BOOL				m_selected_before_click;
	BOOL				m_is_move_window_button;
	BOOL				m_is_moving_window;
	BOOL				m_use_hover_stack_overlay;				// TRUE if the tab is to be rendered with a "possible to create tab stack/group" overlay
	BOOL				m_is_compact_button;					// Should button be rendered with a compact (pinned, ..) look
	INT32				m_original_group_number;				// Original group number before a tab moves (-1 if not set) Should ONLY ever be set for collapsed tabs that are being dragged
	BOOL				m_between_groups;						// Set when dragging between adjcent groups
	BOOL				m_mouse_move_dragging;
	BOOL                m_can_start_drag;                       // For general drag and drop
	bool                m_show_page_loading;                    // If the page is to be shown as loading to the users. Some cases (auto-reload) will not be shown as loading.
	OpTimer				m_activation_timer;
	bool				m_is_delayed_activation_scheduled;
};

#endif // PAGEBAR_BUTTON_H
