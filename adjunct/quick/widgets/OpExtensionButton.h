/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 */

#ifndef OP_EXTENSION_BUTTON_H
#define OP_EXTENSION_BUTTON_H

#include "adjunct/quick/extensions/ExtensionButtonComponent.h"
#include "adjunct/quick/windows/OpRichMenuWindow.h"

class OpButton;

/** @brief Button representation of extension UIItem.
 *
 * This is widget that is showed inside single toolbar. Button text, title,
 * badge, popup url, colours etc are changed through ExtensionButtonComponent
 * interface.
 *
 * @see ExtensionButtonComposite
 */
class OpExtensionButton :
	public ExtensionButtonComponent,
	public OpButton
{
public:

	static OP_STATUS Construct(OpExtensionButton** obj, INT32 extension_id);

	OpExtensionButton(INT32 extension_id);

	virtual ~OpExtensionButton();

	//
	// OpExtensionButton
	//

	void ShowExtensionPopup();

	//
	// ExtensionButtonComponent
	//
	
	virtual INT32 GetId() { return m_extension_id; }
	virtual const OpStringC GetGadgetId() const { return m_gadget_id; }

	virtual OP_STATUS UpdateIcon(Image &icon);

	/** Change badge background color.
	 *
	 * @param color New color (alpha channel supported).
	 */
	virtual void SetBadgeBackgroundColor(COLORREF color);

	/** Change badge text color.
	 *
	 * @param color New color (no support for alpha channel).
	 */
	virtual void SetBadgeTextColor(COLORREF color);

	virtual void SetBadgeText(const OpStringC& text);

	virtual void SetDisabled(BOOL disabled);

	virtual void SetTitle(const OpStringC& title);

	//
	// OpButton
	//
	
	virtual void Click(BOOL plus_action /* = FALSE */);

	void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);

	virtual Type GetType() { return WIDGET_TYPE_EXTENSION_BUTTON; }

	virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

	virtual BOOL IsOfType(Type type)
	{ return type == WIDGET_TYPE_EXTENSION_BUTTON || OpButton::IsOfType(type); }

	virtual void OnAdded();

	virtual void OnDeleted();

	//
	// OpInputContext
	//

	virtual BOOL OnInputAction(OpInputAction *action); 

	//
	// OpWidget
	//

	virtual void OnShow(BOOL show);
	
	//
	// Together with OnMouseDown this function controls m_popup_switch value
	// to decide if popup should be opened or not. 
	// It's to get open&close effect, when user clicks button at least twice.
	//
	void MenuClosed();

	//
	// QuickOpWidgetBase
	//
	virtual void OnLayout();


private:

	virtual BOOL OnContextMenu(const OpPoint& point, const OpRect* avoid_rect, BOOL keyboard_invoked);

	INT32 m_extension_id;

	OpButton *m_badge;

	BOOL m_has_badge_text;

	BOOL m_enabled;
	
	OpString m_gadget_id;

	BOOL m_popup_switch; // see MenuClosed()
};

#endif // OP_EXTENSION_BUTTON_H
