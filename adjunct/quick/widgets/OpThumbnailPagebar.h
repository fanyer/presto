/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_THUMBNAIL_PAGEBAR_H
#define OP_THUMBNAIL_PAGEBAR_H

#include "adjunct/quick/widgets/OpPagebar.h"
#include "adjunct/quick/widgets/PagebarButton.h"
#include "adjunct/quick/widgets/OpDragScrollbar.h"
#include "modules/util/adt/oplisteners.h"

class OpIndicatorButton;

/**
* @brief Listener to get notification when the pagebar switches to/from thumbnails
* @author Petter Nilsen
*
*
* Declaration: 
* Allocation:  
* Deletion:    
*
*/
class PagebarThumbnailStatusListener
{
public:
	virtual ~PagebarThumbnailStatusListener() {}

	/**
	*
	*/
	virtual void OnShowThumbnails(BOOL force_update = FALSE) = 0;

	/**
	*
	*/
	virtual void OnHideThumbnails(BOOL force_update = FALSE) = 0;
};

/**
 * @brief Head/Tail toolbar subclass used to override toolbar layout
 * @author Petter Nilsen
 *
 *
 * Declaration: 
 * Allocation:  
 * Deletion:    
 *
 */
class PagebarHeadTail :
	public PagebarThumbnailStatusListener,
	public OpToolbar
{
public:
	enum HeadTailType
	{
		PAGEBAR_UNKNOWN = 0,
		PAGEBAR_HEAD,
		PAGEBAR_TAIL,
		PAGEBAR_FLOATING
	};

	/**
	 * @param obj
	 * @param prefs_setting
	 * @param autoalignment_prefs_setting
	 */
	static OP_STATUS Construct(PagebarHeadTail** obj,
							   PrefsCollectionUI::integerpref prefs_setting = PrefsCollectionUI::DummyLastIntegerPref,
							   PrefsCollectionUI::integerpref autoalignment_prefs_setting = PrefsCollectionUI::DummyLastIntegerPref);

	/**
	 *
	 * @param button
	 */
	OP_STATUS CreateButton(OpButton **button);

	void OnLayout(BOOL compute_size_only, INT32 available_width, INT32 available_height, INT32& used_width, INT32& used_height);

	// used to know what skin we will be using
	void SetHeadTailType(HeadTailType type) { m_type = type; }

	// Implementing the PagebarThumbnailStatusListener interface
	virtual void OnShowThumbnails(BOOL force_update = FALSE);
	virtual void OnHideThumbnails(BOOL force_update = FALSE);

	// OpWidgetListener override to check if we allow the drop
	void OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);

protected:

	PagebarHeadTail(PrefsCollectionUI::integerpref prefs_setting = PrefsCollectionUI::DummyLastIntegerPref,
					PrefsCollectionUI::integerpref autoalignment_prefs_setting = PrefsCollectionUI::DummyLastIntegerPref);

	void UpdateButtonSkins(const char *name, const char *fallback_name);

private:
	HeadTailType	m_type;
	BOOL			m_use_thumbnails;
};

/**
 * @brief Thumbnail/image control inside a PagebarButton, painting is handled differently
 *        than a normal button so it needs custom code
 * @author Petter Nilsen
 *
 *
 * Declaration: 
 * Allocation:  
 * Deletion:    
 *
 */
class PagebarImage : public OpWidget
{
public:

	PagebarImage(PagebarButton* parent);

	static OP_STATUS Construct(PagebarImage** obj, PagebarButton* parent);

	void ShowThumbnailImage(Image& image, const OpPoint &logoStart, BOOL fixed_image);

	OP_STATUS SetTitle(const uni_char* title);

	OP_STATUS GetTitle(OpString& title)
		{ return m_title_button->GetText(title); }

	OpButton* GetIconButton() { return m_icon_button; }
	
	// Implementing OpToolTipListener

	// no tooltip data to be found here but our parent might have it
	virtual BOOL HasToolTipText(OpToolTip* tooltip)
		{ return GetParent()->HasToolTipText(tooltip); }

	virtual void GetToolTipText(OpToolTip* tooltip, OpInfoText& text)
		{ GetParent()->GetToolTipText(tooltip, text); }

	virtual void GetToolTipThumbnailText(OpToolTip* tooltip, OpString& title, OpString& url_string, URL& url)
		{ GetParent()->GetToolTipThumbnailText(tooltip, title, url_string, url); }

	virtual BOOL GetPreferredPlacement(OpToolTip* tooltip, OpRect &ref_rect, PREFERRED_PLACEMENT &placement)
		{ return GetParent()->GetPreferredPlacement(tooltip, ref_rect, placement); }

	// == OpWidget Hooks ======================

	void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
	void OnLayout();

	BOOL GetTextRect(OpRect &text_rect);
	BOOL GetIconRect(OpRect &icon_rect);

	void SetCloseButtonRect(OpRect& rect) { m_close_button_rect = rect; }

	void SetIndicatorButton(OpIndicatorButton* button) { m_indicator_button = button; }

protected:

	OpRect GetContentRect();
	INT32 GetTitleHeight();
	OpRect GetThumbnailRect(const OpRect& content_rect);
	void SetSkinState(OpWidget* parent);
	void AddBorder(OpRect& content_rect, int border);
	void RemoveTextHeight(OpRect& content_rect, const OpRect& text_rect);
	virtual void DrawSkin(VisualDevice* visual_device, OpRect& content_rect);
	virtual void DrawThumbnail(VisualDevice* vd, OpRect& content_rect);

private:
	PagebarButton* m_parent;
	Image          m_thumbnail;
	OpPoint        m_logoStart;         ///< position of logo in thumbnail
	OpButton*      m_icon_button;
	OpButton*      m_title_button;
	OpIndicatorButton* m_indicator_button;
	OpRect         m_close_button_rect; // Position of the close button
	BOOL           m_fixed_image;       // TRUE if this is a fixed skin image
};

/**
 * @brief 
 * @author Petter Nilsen
 *
 *
 * Declaration: 
 * Allocation:  
 * Deletion:    
 *
 */
class OpThumbnailPagebarButton : public PagebarButton, public PagebarThumbnailStatusListener
{
public:
	OpThumbnailPagebarButton(class OpThumbnailPagebar* pagebar, DesktopWindow* desktop_window);
	virtual ~OpThumbnailPagebarButton();

	static OP_STATUS Construct(OpThumbnailPagebarButton** obj, OpThumbnailPagebar* pagebar, DesktopWindow* desktop_window);

	virtual void OnDeleted();

	virtual BOOL CanUseThumbnails()
		{ return m_use_thumbnail && !IsCompactButton(); }

	virtual void RequestUpdateThumbnail();

	// Implementing the MessageObject interface
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	// Implementing the DesktopWindowListener interface
	// page content or privacy mode change
	virtual void	OnDesktopWindowContentChanged(DesktopWindow* desktop_window) { SetSkin(); RequestUpdateThumbnail(); }

	virtual void OnLayout();

	// Implementing the DocumentDesktopWindow::DocumentWindowListener interface
	virtual void	OnStartLoading(DocumentDesktopWindow* document_window);
	virtual void	OnLoadingFinished(DocumentDesktopWindow* document_window, OpLoadingListener::LoadingFinishStatus status, BOOL was_stopped_by_user = FALSE);

	// Implementing the OpWidget interface
	virtual OP_STATUS GetText(OpString& text) { return GetTitle(text); }
	virtual void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
	virtual void GetRequiredSize(INT32& width, INT32& height);
	virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

	// Implementing the PagebarThumbnailStatusListener interface
	virtual void OnShowThumbnails(BOOL force_update = FALSE) { SetUseThumbnail(!IsCompactButton()); }
	virtual void OnHideThumbnails(BOOL force_update = FALSE) { SetUseThumbnail(FALSE); }

	virtual void OnCompactChanged();

	virtual void UpdateTextAndIcon(bool update_icon = true, bool update_text = true, bool delay_update = false);

	virtual OpWidgetImage* GetIconSkin();

	virtual BOOL ShouldShowFavicon();

protected:

	void ClearRegularFields();
	virtual void SetUseThumbnail(BOOL use_thumbnail);
	virtual void UpdateThumbnail();
	virtual OP_STATUS InitPagebarButton();
	virtual OP_STATUS GetTitle(OpString& title);
	virtual OP_STATUS SetTitle(const OpStringC& title);

	void SetSkin();

	OpThumbnailPagebar* m_pagebar;          ///<
	PagebarImage*		m_thumbnail_button; ///<
	BOOL				m_use_thumbnail;    ///<
};

/**
 * @brief 
 * @author Petter Nilsen
 *
 *
 * Declaration: 
 * Allocation:  
 * Deletion:    
 *
 */
class OpThumbnailPagebar
	: public OpPagebar,
	  public OpDragScrollbarTarget
{
	friend class OpPagebar;

public:

	virtual void OnRelayout();
	virtual void OnLayout(BOOL compute_size_only,
						  INT32 available_width,
						  INT32 available_height,
						  INT32& used_width,
						  INT32& used_height);

	// Subclassing OpToolbar

	virtual void OnSettingsChanged(DesktopSettings* settings);
	virtual BOOL SupportsThumbnails() { return TRUE; }
	virtual BOOL SetAlignment(Alignment alignment, BOOL write_to_prefs = FALSE);

	virtual void			OnDeleted();

	// == OpInputContext ======================

	virtual BOOL OnInputAction(OpInputAction* action);

	// Implementing the OpDragScrollbarTarget interface
	
	virtual OpRect GetWidgetSize() { return GetRect(); }
	virtual void SizeChanged(const OpRect& rect);
	virtual BOOL IsDragScrollbarEnabled();
	virtual INT32 GetMinHeightSnap();
	virtual void GetMinMaxHeight(INT32& min_height, INT32& max_height);
	virtual void GetMinMaxWidth(INT32& min_height, INT32& max_height);
	virtual void EndDragging();

	virtual BOOL	    CanUseThumbnails();

	OP_STATUS AddThumbnailStatusListener(PagebarThumbnailStatusListener* listener) { return m_thumbnail_status_listeners.Add(listener); }
	OP_STATUS RemoveThumbnailStatusListener(PagebarThumbnailStatusListener* listener) { return m_thumbnail_status_listeners.Remove(listener); }

	virtual void		SetExtraTopPaddings(unsigned int head_top_padding, unsigned int normal_top_padding, unsigned int tail_top_padding, unsigned int tail_top_padding_width);
protected:

	OpThumbnailPagebar(BOOL privacy);
	virtual OP_STATUS CreateToolbar(OpToolbar** toolbar);
	virtual OP_STATUS CreatePagebarButton(PagebarButton*& button, DesktopWindow* desktop_window);
	virtual INT32 GetRowHeight();
	virtual INT32 GetRowWidth();
	virtual OP_STATUS InitPagebar();
	void NotifyButtons(BOOL force_update = FALSE);
	void ToggleThumbnailsEnabled();
	void SetupDraggedHeight(INT32 used_height);
	void SetupDraggedWidth(INT32 used_width);
	BOOL HeightAllowsThumbnails();
	void CommitThumbnailsEnabled();
	void RestoreThumbnailsEnabled();
	void SetMinimalHeight(INT32	minimal_height) { m_minimal_height = minimal_height;}
	void SetDraggedHeight(INT32 dragged_height) { m_dragged_height = dragged_height;}
	BOOL IsThumbnailsEnabled() const { return m_thumbnails_enabled; }
	void SetupButton(PagebarButton* button);
	void ResetLayoutCache();

private:

	INT32 	m_dragged_height;     ///< height as set by dragging
	INT32 	m_dragged_width;      ///< width as set by dragging
	BOOL	m_thumbnails_enabled; ///<
	BOOL	m_first_layout;	      ///< TRUE on first layout, otherwise FALSE
	INT32	m_minimal_height;     ///< minimal height, any scaling can not go below this height
	INT32	m_maximum_width;
	INT32	m_minimum_width;
	BOOL	m_hide_tab_thumbnails;	/// if we're on the left or right and there's not enough space to show tab thumbnails
	OpListeners<PagebarThumbnailStatusListener> m_thumbnail_status_listeners; ///<
};

#endif // OP_THUMBNAIL_PAGEBAR_H
