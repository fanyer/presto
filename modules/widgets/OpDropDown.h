/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_DROP_DOWN_H
#define OP_DROP_DOWN_H

#define DROPDOWN_MINIMUM_DRAG 4

#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpWidget.h"
#include "modules/widgets/OpListBox.h"
#include "modules/widgets/WidgetWindow.h"

#include "modules/pi/OpView.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/inputmanager/inputaction.h"

#ifdef PLATFORM_FONTSWITCHING
# include "modules/display/fontdb.h"
#endif // PLATFORM_FONTSWITCHING

class OpWindowListener;
class OpDropDownWindow;
class OpDropDownButtonAccessor;
class OpDropDown;

/***********************************************************************************
**
**	DropDownEdit - an OpEdit inside a dropdown
**
***********************************************************************************/

class DropDownEdit : public OpEdit
{
	public:
		DropDownEdit(OpDropDown* dropdown);

		virtual void OnFocus(BOOL focus,FOCUS_REASON reason);

#ifdef WIDGETS_IME_SUPPORT
		virtual void OnCandidateShow(BOOL visible);
		virtual void OnStopComposing(BOOL cancel);
#endif // WIDGETS_IME_SUPPORT

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		virtual OpAccessibleItem* GetAccessibleLabelForControl();
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

#ifdef WIDGETS_IME_ITEM_SEARCH
		virtual IM_WIDGETINFO OnCompose();
		BOOL OnInputAction(OpInputAction* action);
#endif // WIDGETS_IME_ITEM_SEARCH
			
	/**
	 sets the hint that is only visible when the edit field contains text.
	 @param hint_text the new ghost text
	 @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY om OOM
	 */
	OP_STATUS SetHintText(const uni_char* hint_text);

	void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);

	const uni_char* GetHintText() const { return m_hint_string.Get();}

	/**
	 causes the contents (icon if present, ghost string or text, and caret) of the edit field
	 to be painted onto the visual device
	 @param inner_rect the bounds within which to paint
	 @param fcol the text color
	 */
	void PaintHint(OpRect inner_rect, UINT32 hcol);

	void SetHintEnabled(bool enabled) {m_hint_enabled = enabled;}
private:
	OpDropDown*		m_dropdown;// pointer back to dropdown that owns me
	OpWidgetString	m_hint_string; // like a ghost text, but displayed only when there is some input
	bool			m_hint_enabled; // hint on/off switch
};



/** OpDropDown is showing a single OpStringItem and a "dropdown button". When the dropdown button is invoked, it will
	open a menu below the OpDropDown (or above if there is not enough room) which will show a OpListBox will all the items
	available in this OpDropDown. */

class OpDropDown
	: public OpWidget
#ifndef QUICK
	, public OpWidgetListener
#endif
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	, public AccessibleListListener
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
{
protected:
	OpDropDown(BOOL editable_text = FALSE);

public:
	static OP_STATUS Construct(OpDropDown** obj, BOOL editable_text = FALSE);

#ifdef WIDGETS_CLONE_SUPPORT
	virtual OP_STATUS CreateClone(OpWidget **obj, OpWidget *parent, INT32 font_size, BOOL expanded);
	virtual BOOL IsCloneable() { return TRUE; }
#endif

	/** Makes the text showing the current selected item editable. Editing the text won't affect the OpString item,
		only the text value of this widget. if auto_update is TRUE, the text in the textfield will automatically be updated
		when items are selected in the dropdown list. */
	OP_STATUS SetEditableText(BOOL auto_update = FALSE);

	/** Only for editable_text dropdowns. */
	void SetAction(OpInputAction* action);

	/** Only for editable_text dropdowns. */
	OP_STATUS SetGhostText(const uni_char* ghost_text);

	/** Makes it always call OnChange if and item is pressed. Even if the item already was selected. */
	void SetAlwaysInvoke(BOOL always_invoke);

	/** Shortcut to ItemHandler::AddItem, and updates the menu if visible. */
	virtual OP_STATUS AddItem(const uni_char* txt, INT32 index = -1, INT32 *got_index = NULL, INTPTR user_data = 0);

	/** Shortcut to ItemHandler::RemoveItem, and updates the menu if visible. */
	void RemoveItem(INT32 index);

	/** Shortcut to ItemHandler::RemoveAll, and updates the menu if visible. */
	void RemoveAll();

	/** Shortcut to ItemHandler::ChangeItem. */
	OP_STATUS ChangeItem(const uni_char* txt, INT32 index);

	/** Removes all the items */
	void Clear();

	/** Shortcut for ItemHandler::BeginGroup */
	OP_STATUS BeginGroup(const uni_char* txt);
	/** Shortcut for ItemHandler::EndGroup */
	void EndGroup();
	/** Shortcut for ItemHandler::RemoveGroup */
	void RemoveGroup(int group_nr);
	/** Shortcut for ItemHandler::RemoveAllGroups */
	void RemoveAllGroups();

	virtual void SetListener(OpWidgetListener* listener, BOOL force = TRUE);

	/** Shortcut for ItemHandler::GetItemUserData */
	INTPTR GetItemUserData(INT32 index) { return ih.GetItemUserData(index); }

	/** Get the text of the item at index (excluding group-items), or NULL
		if there is no item with that index. */
	const uni_char* GetItemText(INT32 index);

	/**
	 * Set one or more text decoration properties on the string at the
	 * given index. Use 0 to clear the property.
	 *
	 * @param index Item index
	 * @param text_decoration One or more of WIDGET_TEXT_DECORATION types
	 * @param replace If TRUE, the current value is fully replaced, if FALSE
	 *        the new value is added to the current value
	 */
	void SetTextDecoration(INT32 index, INT32 text_decoration, BOOL replace);

	/** Select item and invoke the listeners about the change. */
	void SelectItemAndInvoke(INT32 nr, BOOL selected, BOOL emulate_click = FALSE);

	/** Select or unselect item at nr (excluding group items). If nr is -1, no item
		will be selected and the dropdown will be blank. */
	void SelectItem(INT32 nr, BOOL selected);
	/** Shortcut for ItemHandler::EnableItem */
	void EnableItem(INT32 nr, BOOL enabled);

	/**
	 * This returns the number of items excluding any "begin group" and "end group"
	 * items that aren't visible in most of the API anyway.
	 */
	INT32 CountItems() { return ih.CountItems(); }
	/**
	 * This returns the number of items including any "begin group" and "end group"
	 * items.
	 */
	UINT32 CountItemsIncludingGroups() { return ih.CountItemsAndGroups(); }

	/**
	 * Returns TRUE if the element at the index
	 * (counting all elements, including starts
	 * and stops) is a group stop.
	 */
	BOOL IsElementAtRealIndexGroupStop(UINT32 idx) { return ih.IsElementAtRealIndexGroupStop(idx); }
	/**
	 * Returns TRUE if the element at the index
	 * (counting all elements, including starts
	 * and stops) is a group start.
	 */
	BOOL IsElementAtRealIndexGroupStart(UINT32 idx) { return ih.IsElementAtRealIndexGroupStart(idx); }

	/**
	 * Returns TRUE if the menu is currently dropped down
	 */
	BOOL IsDroppedDown() { return m_dropdown_window != NULL; }
#ifdef QUICK
	virtual void InvokeAction(INT32 index);
	void SetEditOnChangeDelay(INT32 onchange_delay);
	INT32 GetEditOnChangeDelay();
#endif

	/** Enable/disable delayed OnChange. If delayed onchange is enabled and a item is selected,
		the OnChange listener will be invoked shortly later from a posted message instead
		of immediately. */
	void SetDelayOnChange(BOOL d) { m_dropdown_packed.delay_onchange = !!d; }

	BOOL IsSelected(INT32 nr) { return ih.IsSelected(nr); }
	BOOL IsEnabled(INT32 nr) { return ih.IsEnabled(nr); }

	/** Get the nr of the focused item (excluding group items) or -1 if no item is selected. */
	INT32 GetSelectedItem();

	/** Get number of visible items (always 1) */
	INT32 GetVisibleItems() { return 1; }

	/** Set the ScrollbarColors on the scrollbar in this listbox. */
	void SetScrollbarColors(const ScrollbarColors &colors) {}

	/** Shortcut for ItemHandler::RecalculateWidestItem */
	void RecalculateWidestItem() { ih.RecalculateWidestItem(this); }

	void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

#ifdef SKIN_SUPPORT
	virtual void SetMiniSize(BOOL is_mini);
#endif

	/** Show the menu and return TRUE if successfull. If it is already open, it will be closed and return FALSE. */
	BOOL ShowMenu();

	/** Show or hide the dropdown button. It's shown by default. */
	void ShowButton(BOOL show);

	virtual OP_STATUS SetText(const uni_char* text) { return SetText(text, FALSE); }
	OP_STATUS SetText(const uni_char* text, BOOL force_no_onchange, BOOL with_undo = FALSE);

	OP_STATUS		GetText(OpString& text);
	void			SetValue(INT32 value) { SelectItemAndInvoke(value, TRUE, FALSE); }
	INT32			GetValue() { return GetSelectedItem(); }

	virtual BOOL IsScrollable(BOOL vertical);

	// == Hooks ======================
	void OnMove();
	void OnDeleted();
	void OnAdded()	{RecalculateWidestItem();}
	void OnLayout();
	void OnRelayout() {}
	void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
	void OnFocus(BOOL focus,FOCUS_REASON reason);
#ifndef MOUSELESS
	void OnMouseMove(const OpPoint &point);
	void OnMouseLeave();
	void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
	BOOL OnMouseWheel(INT32 delta, BOOL vertical);
#endif // !MOUSELESS

#ifdef QUICK
	virtual void OnDragStart(const OpPoint& point);
	virtual void OnFontChanged() {ghost_string.NeedUpdate();}
	virtual void OnDirectionChanged() { OnFontChanged(); }
	virtual void OnUpdateActionState();
#endif

#ifdef WIDGETS_IME_SUPPORT
	virtual void OnCandidateShow(BOOL visible);
#endif

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	OpStringItem* GetItemToPaint();

	// Hooking up OpWidgetListener

#ifndef MOUSELESS
	void OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
#endif

#ifndef QUICK
	/* These seemingly meaningless overrides are implemented to avoid warnings
	   caused, by the overrides of same-named functions from OpWidget. */
	void OnLayout(OpWidget *widget) {}
	void OnRelayout(OpWidget* widget) {}
	void OnPaint(OpWidget *widget, const OpRect &paint_rect) {}
	void OnMouseMove(OpWidget *widget, const OpPoint &point) {}
	void OnMouseLeave(OpWidget *widget) {}
#endif // QUICK

	// Implementing the OpTreeModelItem interface

	virtual Type		GetType() {return WIDGET_TYPE_DROPDOWN;}
	virtual BOOL		IsOfType(Type type) { return type == WIDGET_TYPE_DROPDOWN || OpWidget::IsOfType(type); }

	// == OpInputContext ======================

	virtual BOOL		OnInputAction(OpInputAction* action);
	virtual const char*	GetInputContextName() {return "Dropdown Widget";}

#ifdef SKIN_SUPPORT
	// Implementing OpWidgetImageListener interface

	virtual void			OnImageChanged(OpWidgetImage* widget_image);
#endif

	void BlockPopupByMouse();

#ifdef PLATFORM_FONTSWITCHING
	void SetPreferredCodePage(OpFontInfo::CodePage cp) { preferred_codepage = cp; }
#endif
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

	void CreateButtonAccessorIfNeeded();

	//AccessibleListListener implementation
	virtual BOOL IsListFocused() const;
	virtual BOOL IsListVisible() const;
	virtual OP_STATUS GetAbsolutePositionOfList(OpRect &rect);
	virtual OpAccessibleItem* GetAccessibleParentOfList() {return this;}
	virtual OP_STATUS ScrollItemTo(INT32 item, Accessibility::ScrollTo scroll_to) {return OpStatus::ERR;}

	virtual OP_STATUS		AccessibilityGetText(OpString& str);
	virtual Accessibility::ElementKind AccessibilityGetRole() const {return Accessibility::kElementKindDropdown;}
	virtual int GetAccessibleChildrenCount();
	virtual OpAccessibleItem* GetAccessibleChild(int);
	virtual int GetAccessibleChildIndex(OpAccessibleItem* child);
	virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
	virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();
	virtual OP_STATUS AccessibilityClicked();
#endif
	/**
	   the number of options ([start, stop[) that need be updated when
	   SelectContent::Paint is called.  this is so that styles are
	   applied to all relevant option elements in a select.  for a
	   list box, only the options overlapped by area need be updated,
	   since drawing always comes from layout. for a closed dropdown,
	   only the selected option need be updated. for an open dropdwon,
	   all options need be updated, since drawing typically doesn't
	   come from layout.
	 */
	void GetRelevantOptionRange(const OpRect& area, UINT32& start, UINT32& stop);

	virtual void OnScaleChanged()
	{
		ghost_string.NeedUpdate();
		if (m_dropdown_window)
			ClosePopup();
	}

	// the element is relaid out e.g. due to style changes.
	virtual void SetLocked(BOOL locked);

	/**
	 * Locks/unlocks updates of the widget. Use this with caution.
	 * Note also that the widget is not updated automatically when
	 * unlocking. Update() must be explicitly called to do so.
	 */
	void LockUpdate(BOOL val)
	{
		if (IsLocked() == val)
			return; // Nothing to do. Already in a proper state.

		SetLocked(val);
		if (val)
		{
			m_locked_dropdown_window = m_dropdown_window;
			m_dropdown_window = NULL;
		}
		else
		{
			m_dropdown_window = m_locked_dropdown_window;
			m_locked_dropdown_window = NULL;
		}
	}

	/** Updates the widget */
	void Update();

private:
	enum { CLOSE_POPUP_AFTER_RELAYOUT_TIMEOUT = 1000 };
	BOOL m_posted_close_msg;
	void Changed(BOOL emulate_click);
#ifndef MOUSELESS
	OpPoint MousePosInListbox();
	bool IsInsideButton(const OpPoint& point);
#endif
	/**
	 * Called when there was some change in the dropdown or that can
	 * influence the the dropdown window if opened. (e.g. position, items,
	 * viewport, scale).
	 * @param items_changed TRUE when there was some addition or removal of
	 *		  this dropdown's items/options, FALSE in other cases.
	 */
	void UpdateMenu(BOOL items_changed);
#ifdef WIDGETS_IME_ITEM_SEARCH
	BOOL SelectCurrentEntryAndDismiss(OpInputAction* action);
#endif // WIDGETS_IME_ITEM_SEARCH

public:
	OpWidgetString ghost_string;
	ItemHandler ih;

	union
	{
		struct
		{
			unsigned int always_invoke:1;
			unsigned int is_hovering_button:1;
			unsigned int auto_update:1;
			unsigned int delay_onchange:1;
			unsigned int need_update:1;
			unsigned int enable_menu:1;
			unsigned int show_button:1;
		} m_dropdown_packed;
		UINT32 m_packed_init;
	};

	DropDownEdit* edit;
	OpDropDownWindow* m_dropdown_window;
	OpDropDownWindow* m_locked_dropdown_window;

	void AddMargin(OpRect *rect);
	/** Returns FALSE if there was no menu open, otherwise TRUE. */
	BOOL ClosePopup(BOOL immediately = FALSE);
	void SetBackOldItem(); // Internal. To do some magic to share items in dropdown and the listbox that comes up in dropdownmenu.
	INT32 old_item_nr;
#ifdef PLATFORM_FONTSWITCHING
	OpFontInfo::CodePage preferred_codepage;
#endif
private:
#ifdef DROPDOWN_MINIMUM_DRAG
	OpPoint m_drag_start;
#endif
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	OpDropDownButtonAccessor* m_button_accessor;
#endif

#ifdef WIDGETS_IMS_SUPPORT
public:
	virtual void DoCommitIMS(OpIMSUpdateList *updates);
	virtual void DoCancelIMS();

protected:
	virtual OP_STATUS SetIMSInfo(OpIMSObject& ims_object);
#endif // WIDGETS_IMS_SUPPORT
};

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
class OpDropDownButtonAccessor : public OpAccessibleItem
{
public:
	OpDropDownButtonAccessor(OpDropDown* parent) : m_parent(parent) {}
	~OpDropDownButtonAccessor() {}

	virtual BOOL AccessibilityIsReady() const {return TRUE;}

	virtual int GetAccessibleChildrenCount() {return 0;}
	virtual OpAccessibleItem* GetAccessibleParent() {return m_parent;}
	virtual OpAccessibleItem* GetAccessibleChild(int) {return NULL;}
	virtual int GetAccessibleChildIndex(OpAccessibleItem* child) {return Accessibility::NoSuchChild; }
	virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y) {return this;}
	virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf() {return this;}
	virtual Accessibility::ElementKind AccessibilityGetRole() const {return Accessibility::kElementKindDropdownButtonPart;}
	virtual Accessibility::State AccessibilityGetState();
	virtual OP_STATUS AccessibilityGetAbsolutePosition(OpRect &rect);
	virtual OP_STATUS AccessibilityGetText(OpString& str) { str.Empty(); return OpStatus::OK; }
	virtual OP_STATUS AccessibilityClicked();
	virtual OpAccessibleItem* GetAccessibleLabelForControl() { return m_parent->GetAccessibleLabelForControl(); }

	virtual OpAccessibleItem* GetNextAccessibleSibling() { return NULL; }
	virtual OpAccessibleItem* GetPreviousAccessibleSibling() { return NULL; }
	virtual OpAccessibleItem* GetLeftAccessibleObject() { return NULL; }
	virtual OpAccessibleItem* GetRightAccessibleObject() { return NULL; }
	virtual OpAccessibleItem* GetDownAccessibleObject() { return NULL; }
	virtual OpAccessibleItem* GetUpAccessibleObject() { return NULL; }

private:
	OpDropDown* m_parent;
};
#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#endif // OP_DROP_DOWN_H
