// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
/** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Owner:    Alexander Remen (alexr)
 */

#ifndef OPACCORDION_H
#define OPACCORDION_H

#include "adjunct/quick/animation/QuickAnimation.h"

#include "modules/widgets/OpButton.h"

class OpScrollbar;
class OpUnreadBadge;

class OpAccordion : public OpWidget
				  , private QuickAnimation
{
public:

	class OpAccordionButton: public OpButton
	{
	public:
						OpAccordionButton(OpUnreadBadge* label, OpAccordion* accordion);
		void			SetNotifcationNumber(UINT32 value);
		void			ShowLabel(BOOL show_label);
		OpUnreadBadge*	GetNotificationLabel() const { return m_label; }
						~OpAccordionButton() {}
		virtual void	OnLayout();

		virtual void	SetFocus(FOCUS_REASON reason);

	private:
		BOOL			m_show_label; // whether it should show the label if there's anything to show
		OpUnreadBadge*	m_label;
		INT32			m_unread_badge_width;
		OpAccordion*    m_accordion;
	};

	// == Internal OpAccordionItem class ======================	
	class OpAccordionItem: public OpWidget
						 , private QuickAnimation
	{
	public :
		
		// == Public OpAccordionItem functions ======================
		static OP_STATUS	Construct(OpAccordionItem** obj, UINT32 id, OpString& name, const char* image_name, OpWidget* contained_widget, OpAccordion* parent);
							
		OpAccordionItem(UINT32 id, OpAccordionButton* button, OpWidget* contained_widget, OpAccordion* parent);
		
		OpButton*			GetButton()					{ return m_button; }
		OpWidget*			GetWidget()					{ return m_widget; }
		UINT32				GetItemID()					{ return m_id; }
		BOOL				IsExpanded()				{ return m_expanded; }
		BOOL				IsReallyVisible()			{ return m_is_really_visible; }
		void				SetIsReallyVisible(BOOL visible)	{ if (visible == m_is_really_visible) return; m_is_really_visible = visible; m_button->ShowLabel(!IsExpanded() || !visible);}
		void				SetAttention(BOOL attention, UINT32 number_of_notifications = 0);
		OP_STATUS			SetButtonText(OpString button_text);

		BOOL				ShowContextMenu(const OpPoint &point, BOOL center, BOOL use_keyboard_context);

		// == OpWidgetListener ======================
		virtual void		OnLayout();
		virtual BOOL		OnInputAction(OpInputAction* action);
		virtual void		OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
		virtual void		OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		virtual void		OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		virtual void		OnDragLeave(OpWidget* widget, OpDragObject* drag_object) { StopTimer(); m_accordion->OnDragLeave(widget, drag_object); }
		virtual void		OnDragCancel(OpWidget* widget, OpDragObject* drag_object) { OnDragLeave(widget, drag_object); }
		virtual void		OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
		virtual void		OnItemEdited(OpWidget *widget, INT32 pos, INT32 column, OpString& text) { widget->OnItemEdited(widget, pos, column, text); }
		virtual void		OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason) { if (new_input_context == m_widget && reason == FOCUS_REASON_KEYBOARD) m_accordion->ExpandItem(this, TRUE); }
		virtual BOOL		OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked);

		// == QuickAnimation ======================
		virtual void OnAnimationStart() {}
		virtual void OnAnimationUpdate(double position);
		virtual void OnAnimationComplete();
		
		// == OpWidget ======================
		virtual Type		GetType() {return WIDGET_TYPE_ACCORDION_ITEM; }
		virtual void		OnTimer();

		virtual void		GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);
		virtual void		GetMinimumSize(INT32* minw, INT32* minh);
	protected:
		friend class OpAccordion;
		void				SetExpanded(BOOL expanded, BOOL animate);
		double				GetAnimationProgress() { return m_animation_progress; }

	private:
		UINT32				m_id;					// items can be rearranged, so we need an id to identify them
		BOOL				m_expanded;				// whether the item is open or not
		OpAccordionButton*	m_button;				// the button used to expand/collapse and drag and drop
		OpWidget*			m_widget;				// the treeview or widget inside the accordion item
		OpAccordion*		m_accordion;			// pointer to its parent
		BOOL				m_attention;
		double				m_animation_progress;	// used for calculations while animations
		BOOL				m_expand_animation;		// whether we are animating opening or closing an item
		BOOL				m_is_really_visible;	// whether the item is visible and not scrolled off screen
	};

	class OpAccordionListener
	{
	public:
		virtual void OnItemExpanded(UINT32 id, BOOL expanded) = 0;
		virtual void OnItemMoved(UINT32 id, UINT32 new_position) = 0;
		~OpAccordionListener() {}
	};

	// == OpWidget/OpWidgetListener ======================
	virtual void		OnLayout();
	virtual void		OnPaintAfterChildren(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
	virtual void		OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
	virtual void		OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
	virtual void		OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
	virtual void		OnDragLeave(OpWidget* widget, OpDragObject* drag_object);
	virtual void		OnDragCancel(OpWidget* widget, OpDragObject* drag_object) { OnDragLeave(widget, drag_object); }
	virtual void		OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
	virtual void		OnScroll(OpWidget *widget, INT32 old_val, INT32 new_val, BOOL caused_by_input);
	virtual BOOL		OnMouseWheel(INT32 delta,BOOL vertical);
	virtual void		OnTimer();
	virtual BOOL		IsScrollable(BOOL vertical) { return TRUE; }
	virtual BOOL		OnScrollAction(INT32 delta, BOOL vertical, BOOL smooth);

	// == Public OpAccordion functions ======================

	static OP_STATUS	Construct(OpAccordion** obj);
	
	OpAccordion();
	
	/**
	 * Adds a new item to the accordion
	 * @param id - the internal id
	 * @param name - the title on the button
	 * @param image_name - the image on the expand/collapse button
	 * @param contained_widget - the widget inside the collapsable area
	 * @param position - where to insert the item
	 */
	OP_STATUS			AddAccordionItem(UINT32 id, OpString& name, const char* image_name, OpWidget* contained_widget, UINT32 insert_position = UINT_MAX);
	
	/**
	 * Remove an OpAccordionItem based on its id
	 * @param id of the item wanted
	 */
	void				RemoveAccordionItem(UINT32 id);

	/**
	 * Remove and delete all OpAccordionItem in the OpAccordion
	 */
	void				DeleteAll() { m_accordion_item_vector.DeleteAll(); }
	
	/**
	 * GetCount
	 */
	UINT32				GetCount() { return m_accordion_item_vector.GetCount(); }
	
	/**
	 * Get an OpAccordionItem based on its index in the vector
	 * @param index of the item wanted
	 * @return OpAccordionItem* or NULL
	 */
	OpAccordionItem*	GetItemByIndex(UINT32 index) { return m_accordion_item_vector.Get(index); }
	/**
	 * Get an OpAccordionItem based on its id
	 * @param id of the item wanted
	 * @return OpAccordionItem* or NULL
	 */
	OpAccordionItem*	GetItemById(UINT32 id);

	/**
	 * Expand an item 
	 * @param item to expand
	 * @param expand whether to expand (TRUE) or collapse (FALSE)
	 * @param animate - whether to animate when expanding/collapsing the item
	 */
	void				ExpandItem(OpAccordionItem* item, BOOL expand, BOOL animate = TRUE);

	/**
	 * Set a listener that the OpAccordion should notify about changes
	 * @param listener to set
	 */
	void				SetOpAccordionListener(OpAccordionListener* listener) { m_listener = listener; }

	/**
	 * Set which popup menu to display when right clicking below the last item when it's closed
	 */
	OP_STATUS			SetFallbackPopupMenu(const char* menu) { return m_fallback_popup_menu.Set(menu); }

	// == QuickAnimation ======================
	virtual void		OnAnimationStart() {}
	virtual void		OnAnimationUpdate(double position);
	virtual void		OnAnimationComplete();

	/** Calculates if a certain rect in one of the child widgets is visible and scrolls down to it if it's not visible
	 * @param accordion_item - one of the accordion items that should be visible
	 * @param y - y pos of the rect to keep visible relative to the accordion_item
	 * @param height - how much we should try to keep visible in the accordion
	 */
	void				EnsureVisibility(OpAccordionItem* accordion_item, INT32 y, INT32 height);

	/** Returns the position of an item 
	 * @param accordion_item - the item you are searching for
	 * @return the position
	 */
	INT32				GetAccordionItemPos(OpAccordionItem* accordion_item) { return m_accordion_item_vector.Find(accordion_item); }

protected:
	friend class OpAccordion::OpAccordionItem;
	
	/** Starts a timer that will trigger scrolling if the dragging is on the top or the bottom of the accordion.
	 * @param y - vertical position of the dragging mouse pointer relative to the accordion origin
	 */
	void				StartDragTimer(INT32 y);

private:
	void				ScrollToItem(OpAccordionItem* item);

	INT32				GetTotalHeight(UINT32 number_of_items_to_count = 0);

	OpWidgetVector <OpAccordionItem>	m_accordion_item_vector;	// holds all the items in a vector
	INT32								m_drag_drop_pos;			// used amongst other to draw the drag and drop line
	OpAccordionListener*				m_listener;					// the OpAccordionListener to notify about changes
	OpString8							m_fallback_popup_menu;		// Popup menu to display when right click outside accordion items
	OpScrollbar*						m_scrollbar;
	OpWidget*							m_animated_widget;			// pointer to scrollbar when toggled visibilty or accordion item when automatically scrolling to one or NULL
	double								m_animation_progress;		// used for calculations while animations
	INT32								m_total_height;
	INT32								m_animation_start;			// where we start the scrolling animation from
	enum ScrollDirection {
	  DOWN,
	  UP
	};
	ScrollDirection						m_scroll_direction;			// Scroll direction when dragging
	INT32								m_button_height;			// Height of a button
};

#endif // OPACCORDION_H
