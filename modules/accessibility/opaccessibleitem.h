/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OPACCESSIBLEITEM_H
#define OPACCESSIBLEITEM_H

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

class OpWindow;
#include "modules/accessibility/AccessibilityManager.h"
#include "modules/accessibility/accessibilityenums.h"
#include "modules/hardcore/keys/opkeys.h"

/** The listener. Has the responsibility to provide the OpAccessibilityExtension with information it needs, and
  * to be able to get/create to other acessible controls in the hierarchy.
  */

class OpAccessibleItem
{
public:

	virtual ~OpAccessibleItem() { AccessibilityManager::GetInstance()->AccessibleItemRemoved(this); }

	/** Get the accessible item that is the parent of this accessible item. This can be null only for top level items.
	  */
	virtual OpAccessibleItem* GetAccessibleParent() = 0;

	/** Reports wether the accessible item is ready to be queried for any other accessibility method.
	  * If this returns false, other methods may not be called, except GetAccessibleParent.
	  */
	virtual BOOL AccessibilityIsReady() const = 0;

	/** Sends an event through the AccessibilityManager. Allows for shorter syntax and
	  * allows subclasses to intercept events sent by the class they inherit from.
	  */
	virtual void AccessibilitySendEvent(Accessibility::Event evt) { OpStatus::Ignore(AccessibilityManager::GetInstance()->SendEvent(this, evt)); }

	/** Returns the OpWindow related to this accessible item. Only required for kElementKindWindow
	  */
	virtual OpWindow* AccessibilityGetWindow() const {return NULL;}

	/** The assistive app has requested that the item should be clicked on. Do whatever would have happened, had it been a mouse clicking on it.
	  */
	virtual OP_STATUS AccessibilityClicked() { return OpStatus::OK; }

	/** The assistive app has requested the item to be scrolled into view
	  */
	virtual OP_STATUS AccessibilityScrollTo(Accessibility::ScrollTo scroll_to) { return OpStatus::ERR; }

	/** The assistive app has requested that the item should get this value. Used for buttons, checkboxes, radio buttons, sliders, and scrollbars
	  */
	virtual OP_STATUS AccessibilitySetValue(int value) { return OpStatus::OK; }

	/** The assistive app has requested that the item should get this text. Only used for editable text-fields.
	  */
	virtual OP_STATUS AccessibilitySetText(const uni_char* text) { return OpStatus::OK; }

	/** Changes the selection in a list, returns FALSE if the selection was not modified
	  */
	virtual BOOL AccessibilityChangeSelection(Accessibility::SelectionSet flags, OpAccessibleItem* child) { return FALSE; }

	/** Move keyboard focus here. Should be handled by all objects that can normally get keyboard focus.
	  * The user has 2 ways to input text in a program: Either by telling his screenreader, somehow, to move the keyboard focus
	  * to a given object, and use the program's normal input functionalities, OR to prepare the new value for editable controls
	  * in some other fashion and call SetText, SetValue or Clicked. All editable controls should support both methods if they make sense.
	  */
	virtual BOOL AccessibilitySetFocus() { return FALSE; }

	/** Expand the item. Should only be implemented by table/list items that has state kAccessibilityStateExpandable.
	  */
	virtual BOOL AccessibilitySetExpanded(BOOL expanded) { return FALSE; }

	/* *** Basic getters for the different properties *** */

	/** Gets the absolute (screen) coordinates of the control. Needed for a lot of reasons:
	  * 1: So the screenreader can set up it's own spatial navigation tables.
	  * 2: So the screenreader can highlight and magnify the focused area.
	  * 3: To help the screenreader detemine some of the visual clues to related objects, if such information is not explicitly available.
	  */
	virtual OP_STATUS AccessibilityGetAbsolutePosition(OpRect &rect) = 0;

	/** Get the integer value of a control, if it has one.
	  * Checkboxes and radio buttons MUST return 0 if unchecked, and 1 if checked/selected.
	  * Scrollbars return their current scroll position (in pixels)
	  * Sliders should return whatever value they have.
	  * Buttons that have toggle state return their state, other bottons can ignore this fuction.
	  */
	virtual OP_STATUS AccessibilityGetValue(int &value) { return OpStatus::ERR; }

	/** Get the minimum value of a control, if it has one.
	  * Scrollbars and sliders MUST implement this, other can ignore it.
	  */
	virtual OP_STATUS AccessibilityGetMinValue(int &value) { return OpStatus::ERR; }

	/** Get the minimum value of a control, if it has one.
	  * Scrollbars and sliders MUST implement this, other can ignore it.
	  */
	virtual OP_STATUS AccessibilityGetMaxValue(int &value) { return OpStatus::ERR; }

	/** Get text associated with an accessible item, such as title of a button or window, or contants of a text area.
	  */
	virtual OP_STATUS AccessibilityGetText(OpString& str) = 0;

	/** The assistive app wants to change text selection range.
	  */
	virtual OP_STATUS AccessibilitySetSelectedTextRange(int start, int end) { return OpStatus::OK; }

	/** The assistive app wants to get text selection range.
	  */
	virtual OP_STATUS AccessibilityGetSelectedTextRange(int &start, int &end) { return OpStatus::ERR; }

	/** Optional extra description of an item. Usually not needed, since the title usually suffices to tell what it does.
	  * Is used on search fields, to tell what is being searched on, since neither the role or contents contains this information.
	  * VoiceOver on Mac, when an item gets focus reads TEXT, DESCRIPTION and ROLE. So if you have a button with title "Reload",
	  * users will hear "Reload button", wich is all a sighted user gets as well. Adding more info will just cause annoyance.
	  * ONLY add information here if there are important things available to a sighted user that is not covered by text and role.
	  */
	virtual OP_STATUS AccessibilityGetDescription(OpString& str) { str.Empty(); return OpStatus::OK; }

	/** The url associated with a link.
	  */
	virtual OP_STATUS AccessibilityGetURL(OpString& str) { str.Empty(); return OpStatus::OK; }

	/** The keyboard shortcut associated with a control, if any.
	  */
	virtual OP_STATUS AccessibilityGetKeyboardShortcut(ShiftKeyState* shifts, uni_char* kbdShortcut) { return OpStatus::ERR; }

	/** Returns a string containing all attributes related to the objects.
	  */
	virtual OP_STATUS AccessibilityGetAttributes(OpString& attrs) { attrs.Empty(); return OpStatus::OK; }

	/** Current state an item is in. kAccessibilityStateFocused and kAccessibilityStateInvisible are
	  * critical to get right.
	  */
	virtual Accessibility::State AccessibilityGetState() = 0;

	/** The role (or archetype) of an object. If none fit and you think it's important, talk to anders or julien.picalausa about getting it added.
	  *
	  * I will not cover the roles that are self-explanatory, so assuming that it isn't follow this simple guide:
	  * Can you interact with it?
	  *  - yes
	  *		Can you type text into it?
	  *			- yes ->			kElementKindSinglelineTextedit or kElementKindMultilineTextedit
	  * 							(better to use the wrong one than something else, as these may trigger special typing/selection help)
	  *			- no
	  *				Does it look like a button?
	  *					- yes/sort of ->	kElementKindButton
	  *					- no
	  *						Does it go to a url?
	  *							- yes ->	kElementKindLink
	  *							- no ->		kElementKindButton
	  *  - no
	  *		What does it display?
	  *			- text ->					kElementKindStaticText
	  *			- other media ->			kElementKindImage
	  *			- can't tell (plugin) ->	kElementKindImage
	  *			- two or more of above ->	Split object up in smaller pieces.
	  * 
	  */
	virtual Accessibility::ElementKind AccessibilityGetRole() const = 0;

	/** If the object associated with this listener is functioning as label for another extension, return that other extension.
	  * For instance if the text "Select your country" precedes a drop-down wit a list of countries,
	  * the text object "Select your country" should return that it is a label of that drop-down.
	  * This is needed because all that otherwise links these two objects are visual clues. These clues need to be formalised.
	  **/
	virtual OpAccessibleItem* GetAccessibleControlForLabel() { return NULL; }

	/** Likewise, in the preceding exaple the dropdown would have to report the label containing the descriptive text.
	  * @GetAccessibleControlForLabel and @GetAccessibleLabelForControl must be balanced. I.e. if A returns B in one,
	  * then B must return A in the other.
	  */
	virtual OpAccessibleItem* GetAccessibleLabelForControl() { return NULL; }

	/* *** Navigating the accessible hierarchy *** */

	/** Get number of accessible children, even if these do not have an accessible object associated with them yet.
	  */
	virtual int GetAccessibleChildrenCount() = 0;

	/******************************************************************************
	*  All the following can return NULL if they do not find an appropriate child *
	******************************************************************************/
	
	/** Get the accessible object associated with a child, creating it if it doesn't exist yet.
	  *
	  */
	virtual OpAccessibleItem* GetAccessibleChild(int n) = 0;

	/** Get the accessible object associated with a child, creating it if it doesn't exist yet.
	  * Will only return immediate children of the object or the object itself.
	  * @param x global x coordinate
	  * @param y global y coordinate
	  */
	virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y) = 0;

	/** Get the accessible object associated with the next/previous sibling,
	  * creating it if it doesn't exist yet.
	  */
	virtual OpAccessibleItem* GetNextAccessibleSibling() = 0;
	virtual OpAccessibleItem* GetPreviousAccessibleSibling() = 0;

	/** Get the accessible object in the specified direction
	  */
	virtual OpAccessibleItem* GetLeftAccessibleObject() = 0;
	virtual OpAccessibleItem* GetRightAccessibleObject() = 0;
	virtual OpAccessibleItem* GetDownAccessibleObject() = 0;
	virtual OpAccessibleItem* GetUpAccessibleObject() = 0;

	/** Get the accessible object associated with a child, creating it if it doesn't exist yet.
	  * Will only return immediate children of the object or the object itself.
	  */
	virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf() = 0;

	/** Given an accessible child item, returns which index it can be retrieved by,
	  * or Accessibility::NoSuchChild if no such child can be found.
	  */
	virtual int GetAccessibleChildIndex(OpAccessibleItem* child) = 0;

	virtual OP_STATUS GetAccessibleHeaders(int level, OpVector<OpAccessibleItem> &headers) { return OpStatus::ERR; }

	virtual OP_STATUS GetAccessibleLinks(OpVector<OpAccessibleItem> &links) { return OpStatus::ERR; }
	/******************************************************************************/
	
};

#endif // ACCESSIBILITY_EXTENSION_SUPPORT

#endif // OPACCESSIBILITYEXTLISTENER_H
