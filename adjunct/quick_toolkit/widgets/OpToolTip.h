/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPTOOLTIP_H
#define OPTOOLTIP_H

#include "modules/hardcore/timer/optimer.h"

#include "modules/widgets/WidgetWindow.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/url/url2.h"
#include "adjunct/quick/managers/AnimationManager.h"
#include "adjunct/quick_toolkit/widgets/DesktopWidgetWindow.h"

class GenericThumbnail;
class OpMultilineEdit;
class OpToolTip;
class OpButton;
class OpToolTipListener;
class QuickAnimationWindowObject;
class QuickFlowLayout;
class QuickScrollContainer;
class QuickWidget;

struct TooltipThumbnailEntry
{
	TooltipThumbnailEntry(GenericThumbnail* _thumbnail, BOOL fixed, BOOL close_button, INT32 _group_window_id, BOOL _active) : 
			thumbnail(_thumbnail), quickwidget_thumbnail(NULL), is_fixed_image(fixed), show_close_button(close_button), group_window_id(_group_window_id), active(_active) {}
	~TooltipThumbnailEntry();

	GenericThumbnail*	thumbnail;
	QuickWidget*		quickwidget_thumbnail;	// thumbnail's quickwidget
	BOOL				is_fixed_image;
	BOOL				show_close_button;
	INT32				group_window_id;
	BOOL				active;					// is this window the active one
};

class OpToolTipWindow : public DesktopWidgetWindow, public QuickAnimationWindowObject, public OpWidgetListener
{
public:
	OpToolTipWindow(OpToolTip *tooltip);
	virtual ~OpToolTipWindow();

	OP_STATUS Init(BOOL thumbnail_mode);
	void Update();
	void GetRequiredSize(UINT32& width, UINT32& height);
	void SetShowThumbnail(BOOL enable);

	// == DesktopWindowListener ====================
	virtual void OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);

	// == QuickAnimationWindowObject =================
	virtual void OnAnimationComplete();

	// OpTypedObject
	virtual Type GetType() { return WINDOW_TYPE_TOOLTIP; }
	
	// == WidgetWindow ===============================
	virtual void			OnResize(UINT32 width, UINT32 height);

	// OpWidgetListener
	virtual void OnMouseMove(const OpPoint &point);
	virtual void OnMouseLeave();

	// OpInputContext
	BOOL OnInputAction(OpInputAction* action);

	OP_STATUS	SetThumbnailEntry(UINT32 offset, OpToolTipThumbnailPair* entry);
	void		ClearThumbnailEntries(UINT32 max_entries);

protected:
	OP_STATUS	CreateThumbnailEntry(TooltipThumbnailEntry **entry, OpToolTipThumbnailPair* entry_pair);
	OP_STATUS	RecreateLayout();
	BOOL		DeleteWidgetFromLayout(QuickWidget *delete_widget);
	void		DoLayout();

private:
	friend class OpToolTip;
	OpAutoVector<TooltipThumbnailEntry> m_thumbnails;
	BOOL				m_thumbnail_mode;
	BOOL				m_close_on_animation_completion;
	DesktopWindow		*m_parent_window;
	OpToolTip			*m_tooltip;
	OpToolTipListener::PREFERRED_PLACEMENT m_placement;			// the preferred placement if m_has_preferred_placement is TRUE
	BOOL				m_has_preferred_placement;				// do we have a preferred placement (top, button, left or right) already?
	OpRect				m_ref_screen_rect;
	QuickFlowLayout		*m_content;
	OpMultilineEdit*	m_edit;
	QuickScrollContainer *m_scroll_container;
};

class OpToolTip : public OpTimerListener
{
public:
	OpToolTip();
	virtual ~OpToolTip();

	OP_STATUS Init();

	/**
	 * Sets the listener that will provide content for the tooltip. Setting the 
	 * listener to 0 will hide any visible tooltip
	 *
	 * @param listener The listener to use
	 * @param key The OP_KEY_NN value that was used to trigger the function. Use 0
	 *        if this did not happened as a direct result of a keyboard event
	 */
	void SetListener(OpToolTipListener* listener, uni_char key = 0);
	OpToolTipListener* GetListener() { return m_listener; }

	void				OnTooltipWindowClosing(OpToolTipWindow *window);

	INT32				GetItem() {return m_item;}
	void				SetImage(Image image, BOOL fixed_image);
	BOOL				HasImage() { return !m_image.IsEmpty(); }
	void				UrlChanged();		// called by the document window to update url in tooltip
	void				SetAdditionalText(const uni_char *extra_text) { OpStatus::Ignore(m_additional_text.Set(extra_text)); }
	void				SetSticky(BOOL sticky) { m_is_sticky = sticky; }
	BOOL				IsSticky() { return m_is_sticky; }

private:
	virtual				void OnTimeOut(OpTimer* timer);

	void Show();
	void Hide(BOOL hide_text_tooltip = TRUE, BOOL hide_thumb_tooltip = TRUE);

	OpToolTipWindow*	m_thumb_window;
	OpToolTipWindow*	m_text_window;
	OpToolTipListener*	m_listener;
	INT32				m_item;
	BOOL				m_thumb_visible;
	BOOL				m_text_visible;
	OpTimer*			m_timer;
	OpInfoText			m_text;
	Image				m_image;
	OpString			m_title;
	OpString			m_url_string;
	URL					m_url;
	OpString			m_additional_text;
	BOOL				m_is_fixed_image;
	OpVector<OpToolTipThumbnailPair> m_thumbnail_collection;
	BOOL				m_is_sticky;		// When TRUE, the tooltip will only close when the mouse cursor leaves the area of the tooltip
	BOOL				m_invalidate_thumbnails;				// Set to TRUE when we need to get new thumbnails
	BOOL				m_show_on_hover;
};

OpRect GetPlacement(int width, int height, OpWindow *window, WidgetContainer* widget_container,
					BOOL has_preferred_placement, const OpRect &ref_screen_rect, 
					OpToolTipListener::PREFERRED_PLACEMENT placement);

#endif // OPTOOLTIP_H
