/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_PAGEBAR_H
#define OP_PAGEBAR_H

#include "modules/widgets/OpWidget.h"
#include "modules/widgets/OpNamedWidgetsCollection.h"
#include "adjunct/quick/models/DesktopGroupModelItem.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick/windows/DocumentWindowListener.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"

class OpTabGroupButton;

#define TAB_DELAYED_ACTIVATION_TIME 400 // Delay (in milliseconds) before activating a hovered tab while doing drag and drop

/***********************************************************************************
**
**	OpPagebar
**
**  OpPagebar is a toolbar with a number of children: the tab buttons (which are added
**  as widgets to be auto-layouted by OpToolbar), head- and tail-toolbars (added as
**  children, not as widgets) and layouted within OnLayout()), and a "new tab" button
**  for new skins. If wrapping is off, this button is auto-aligned, otherwise it is
**  layouted next to the tail toolbar.
**
***********************************************************************************/

class OpPagebar : 
	public OpToolbar,
	public OpWorkspace::WorkspaceListener,
	public DesktopGroupListener,
	public OpToolbar::ContentListener
{
	protected:
		OpPagebar(BOOL privacy_mode);

	public:

		static OP_STATUS	Construct(OpPagebar** obj, OpWorkspace* workspace);

		/**
		 ** Returns the space that is used by the auto-layouted child widgets. It returns the available
		 ** space minus the space on the left and right (or top and bottom for vertial pagebars) used for
		 ** the fixed-width children (head and tail toolbar, and sometimes the "new tab" button).
		 ** Possible negative margins of the floating toolbar are taken into account.
		 **/
		virtual void		GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom);
		virtual BOOL		SetSelected(INT32 index, BOOL invoke_listeners);
		virtual INT32		GetDragSourcePos(DesktopDragObject* drag_object);
		virtual BOOL		IsCloseButtonVisible(DesktopWindow* desktop_window, BOOL check_for_space_only);
		virtual INT32		GetRowHeight();
		virtual INT32		GetRowWidth();

		INT32				GetPagebarButtonMin();
		INT32				GetPagebarButtonMax();

		virtual bool 		IsDropAvailable(DesktopDragObject* drag_object, int x, int y);
		virtual void  		UpdateDropAction(DesktopDragObject* drag_object, bool accepted);

		BOOL				IsInitialized() const { return m_initialized; }

		OpWorkspace*		GetWorkspace() const { return m_workspace; }

		// ensure the menu button is available
		OP_STATUS			EnsureMenuButton(BOOL show);
		void				ShowMenuButtonMenu();
		// Set flag to allow or reject showing the menu button. Call @ref EnsureMenuButton to update visibility
		void 				SetAllowMenuButton(BOOL allow_menu_button) {m_allow_menu_button = allow_menu_button; }

		/** Activate the next tab (compared to the currently active tab) on the pagebar */
		void				ActivateNext(BOOL forwards);

		// Subclassing OpBar
		virtual BOOL		SetAlignment(Alignment alignment, BOOL write_to_prefs = FALSE);

		// Subclassing OpToolbar
		// Hooks
		virtual void		OnRelayout();
		virtual void		OnLayout();
		virtual void		OnDeleted();
		virtual void		OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
		virtual void		OnSettingsChanged(DesktopSettings* settings);
		virtual void		OnReadContent(PrefsSection *section) {}
		virtual void		OnWriteContent(PrefsFile* prefs_file, const char* name) {}
		virtual void		OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
		virtual void		OnAlignmentChanged();

		// Implementing the OpToolbar::ContentListener interface

		virtual BOOL 		OnReadWidgetType(const OpString8& type);

		// Implementing the OpWidgetListener interface

		void				StartDrag(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
		virtual void		OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y) {} // Done by manually calling StartDrag (if we are not just rearranging tabs)
		virtual void		OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		virtual	void		OnDragLeave(OpWidget* widget, OpDragObject* drag_object);
		virtual	void 		OnDragCancel(OpWidget* widget, OpDragObject* drag_object);
		virtual void		OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);

		virtual BOOL		OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked);
		virtual void		OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);

		// == OpInputContext ======================

		virtual BOOL		OnInputAction(OpInputAction* action);
		virtual const char*	GetInputContextName() {return "Pagebar";}

		// Implementing the OpTreeModelItem interface

		virtual Type		GetType() {return WIDGET_TYPE_PAGEBAR;}
		virtual BOOL		IsOfType(Type type) { return type == WIDGET_TYPE_PAGEBAR || OpToolbar::IsOfType(type); }

		// Implementing the DesktopGroupListener interface
		virtual void		OnCollapsedChanged(DesktopGroupModelItem* group, bool collapsed) { SetGroupCollapsed(group->GetID(), collapsed); }
		virtual void		OnActiveDesktopWindowChanged(DesktopGroupModelItem* group, DesktopWindow* active) { SetGroupCollapsed(group->GetID(), group->IsCollapsed()); }
		virtual void		OnGroupDestroyed(DesktopGroupModelItem* group) {}

		// Implementing the OpWorkspace::Listener interface

		virtual void		OnDesktopWindowAdded(OpWorkspace* workspace, DesktopWindow* desktop_window);
		virtual void		OnDesktopWindowRemoved(OpWorkspace* workspace, DesktopWindow* desktop_window);
		virtual void		OnDesktopWindowOrderChanged(OpWorkspace* workspace);
		virtual void		OnDesktopWindowActivated(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL activate);
		virtual void		OnDesktopWindowAttention(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL attention) {}
		virtual void		OnDesktopWindowChanged(OpWorkspace* workspace, DesktopWindow* desktop_window);
		virtual void		OnDesktopWindowClosing(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL user_initiated) {}
		virtual void		OnDesktopGroupAdded(OpWorkspace* workspace, DesktopGroupModelItem& group);
		virtual void		OnDesktopGroupCreated(OpWorkspace* workspace, DesktopGroupModelItem& group);
		virtual void		OnWorkspaceEmpty(OpWorkspace* workspace,BOOL has_minimized_window) {}
		virtual void		OnWorkspaceAdded(OpWorkspace* workspace) {}
		virtual void		OnWorkspaceDeleted(OpWorkspace* workspace);
		virtual void		OnDesktopGroupRemoved(OpWorkspace* workspace, DesktopGroupModelItem& group);
		virtual void		OnDesktopGroupDestroyed(OpWorkspace* workspace, DesktopGroupModelItem& group) {}
		virtual void		OnDesktopGroupChanged(OpWorkspace* workspace, DesktopGroupModelItem& group) {}

		// Implementing the DocumentDesktopWindow::DesktopWindowListener interface

		virtual void		OnReadStyle(PrefsSection *section);
		virtual void		OnWriteStyle(PrefsFile* prefs_file, const char* name);

		virtual BOOL		OnBeforeExtenderLayout(OpRect& extender_rect);
		virtual INT32		OnBeforeAvailableWidth(INT32 available_width);
		virtual BOOL		OnBeforeWidgetLayout(OpWidget *widget, OpRect& layout_rect);

		virtual void		EnableTransparentSkin(BOOL enable);

		void				UpdateIsTopmost(BOOL top);

		virtual void		SetExtraTopPaddings(unsigned int head_top_padding, unsigned int normal_top_padding, unsigned int tail_top_padding, unsigned int tail_top_padding_width);

		virtual void		OnLockedByUser(PagebarButton *button, BOOL locked);

		void				ClearAllHoverStackOverlays();
		PagebarButton*		FindDropButton();

	    // Retrieving Widget position of widgets inside the pagebar 
	    // FIXME: There might already be some better (faster) way to do this
	    INT32                GetWidgetPos(OpWidget* widget);

		PagebarButton*		GetPagebarButton(const OpPoint& point);

		// Retrieving PagebarButtons and positions of PagebarButtons based on the PagebarButton index (i.e skipping other widget types)
		INT32				GetPosition(PagebarButton* button);
	
		OpSkinElement*		GetGroupBackgroundSkin(BOOL use_thumbnails);
		INT32				GetGroupSpacing(BOOL use_thumbnails);
		int 				FindActiveTab(int position, UINT32 group_number);
		int 				FindFirstTab(int position, UINT32 group_number);
		virtual int  		GetActiveTabInGroup(int position);
		virtual void  		IncDropPosition(int& position, bool step_out_of_group);
		bool 				IsInGroup(INT32 position);
		bool 				IsInExpandedGroup(INT32 position);
		int 				CalculateDropPosition(int x, int y, bool& collapsed_group);

		// Group Functions
	
		/** Return TRUE if the group is currently being collapsed (still expanded but animating to the collapsed state) */
	    BOOL	IsGroupCollapsing(UINT32 group_number);

		// Used to hide the OpTabGroupButton before a collapsed group starts to be dragged
		void	HideGroupOpTabGroupButton(PagebarButton* button, BOOL hide);

	    BOOL	IsGroupExpanded(UINT32 group_number, PagebarButton* exclude = NULL);
		void	SetGroupCollapsed(UINT32 group_number, BOOL collapsed);

		// Original group numbers should only be used when dragging a collapsed group 
		// though an expanded group. It allows the group to still be identified.
		void	SetOriginalGroupNumber(UINT32 group_number);
		void	ResetOriginalGroupNumber();

		// Animate all widgets in the toolbar to the new rects they get next layout
		void AnimateAllWidgetsToNewRect(INT32 ignore_pos = -1);

		OpTabGroupButton* FindTabGroupButtonByGroupNumber(INT32 group_no);

	protected:

		/** Count the visible buttons' width; returns -1 if button was not layouted yet. */
		INT32	GetVisibleButtonWidth(DesktopWindow *window);

		virtual OP_STATUS CreatePagebarButton(PagebarButton*& button, DesktopWindow* desktop_window);
		virtual void SetupButton(PagebarButton* button);
		virtual PagebarButton* GetFocusedButton();
		virtual OP_STATUS CreateToolbar(OpToolbar** toolbar);
		virtual OP_STATUS InitPagebar();
		virtual OP_STATUS SetWorkspace(OpWorkspace* workspace);
		void	SetHeadAlignmentFromPagebar(Alignment pagebar_alignment, BOOL force_on = FALSE);
		void	SetTailAlignmentFromPagebar(Alignment pagebar_alignment);
		void	PaintGroupRect(OpRect &overlay_rect, BOOL use_thumbnails);

	protected:
		OpToolbar*			m_head_bar;
		OpToolbar*			m_tail_bar;
		OpWorkspace*		m_workspace;
		OpToolbar*			m_floating_bar;

	private:
		enum PagebarDropLocation
		{
			PREVIOUS = 0,
			STACK = 1,
			NEXT = 2
		};

		INT32 GetWindowPos(DesktopWindow* desktop_window);
		void DumpPagebar();
		void AddTabGroupButton(INT32 pos, DesktopGroupModelItem& group);

		/** If the order of the windows has changed or if groups or items have been removed, this function
		  * makes sure that the pagebar order is the same as the order in DesktopWindowCollection
		  * @param item Check the children of this item
		  * @param group_id Group that should be assigned to the buttons that represent children of item
		  * @param widget_pos Which widget to start checking at (widget at this position should represent first child of 'item')
		  * @return A new widget position. All widgets before this position have been reordered to represent the children of 'item'
		  */
		INT32 RestoreOrder(DesktopWindowCollectionItem* item, UINT32 group_id, INT32 widget_pos);

		INT32 FindPagebarButton(DesktopWindow* window, INT32 start_pos);
		INT32 FindTabGroupButton(INT32 group_no, INT32 start_pos);

		void OnDrag(OpWidget* widget, DesktopDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		void OnDrop(OpWidget* widget, DesktopDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		BOOL OnDragOnButton(PagebarButton* button, DesktopDragObject* drag_object, INT32 x, INT32 y);
		void OnDragWindow(OpWidget* widget, DesktopDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		void OnDropWindow(OpWidget* widget, DesktopDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		void OnDropBookmark(OpWidget* widget, DesktopDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		void OnDropContact(OpWidget* widget, DesktopDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		void OnDropHistory(OpWidget* widget, DesktopDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		void OnDropURL(OpWidget* widget, DesktopDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		void OpenURL(const OpStringC& url, DesktopWindow* target_window, DesktopWindowCollectionItem* parent, INT32 pos);
		DesktopWindowCollectionItem* GetDropWindowPosition(INT32 pos, INT32& window_pos, BOOL ignore_groups = FALSE);

		/**
		 * Return the desktop window that is associated with the page bar button at the given
		 * position.
		 *
		 * @param pos The postion of the button in the list. 0 starts at the very left/top
		 * @param x X coordinate of position in toolbar (not button)
		 * @param y X coordinate of position in toolbar (not button)
		 *
		 * @return The desktop window if the position is valid and the cooridinates specify
		 *         the center area of the button, otherwise NULL.
		 */
		DesktopWindow* GetTargetWindow(INT32 pos, INT32 x, INT32 y);

		int FindFirstGroupTab(INT32 position);
		int FindLastGroupTab(INT32 position);

		void OnNewDesktopGroup(OpWorkspace* workspace, DesktopGroupModelItem& group);
		void OnDragDropOfPreviouslyDraggedPagebarButton(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		void OnDragMoveOfPreviouslyDraggedPagebarButton(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		PagebarDropLocation CalculatePagebarDropLocation(OpWidget* widget, INT32 x, INT32 y);
		void RemoveAnyHighlightOnTargetButton();

		BOOL				m_initialized;

		BOOL				m_is_handling_action;
		OpButton::ButtonStyle	m_default_button_style;

		BOOL				m_reported_on_top;
		int					m_reported_height;
		BOOL 				m_allow_menu_button;

		INT32				m_dragged_pagebar_button;

		DesktopSettings*	m_settings;

	protected:
		unsigned int		m_extra_top_padding_head;
		unsigned int		m_extra_top_padding_normal;
		unsigned int		m_extra_top_padding_tail;
		unsigned int		m_extra_top_padding_tail_width;
};

#endif // OP_PAGEBAR_H
