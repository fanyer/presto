/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#ifndef __COLOR_SELECTOR_DIALOG_H__
#define __COLOR_SELECTOR_DIALOG_H__

#include "adjunct/quick_toolkit/widgets/Dialog.h"

class ColorSelectorDialog : public Dialog
{
public:
	struct ColorSelectorDialogListener
	{
		virtual void OnDialogClose(bool accepted, UINT32 color) = 0;
	};

public:
	ColorSelectorDialog(UINT32 color);
	~ColorSelectorDialog();

	virtual void OnInit();

	BOOL OnInputAction(OpInputAction* action);

	virtual BOOL GetModality() {return TRUE;}
	virtual BOOL GetIsBlocking() {return TRUE;}
	virtual Type GetType() {return DIALOG_TYPE_PRINT;}
	virtual const char*	GetWindowName()	{return "Color Selector Dialog";}

	INT32 GetButtonCount() { return 2; };
	void GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name);
	void OnChange(OpWidget *widget, BOOL changed_by_mouse);

	void SetColorSelectorDialogListener(ColorSelectorDialogListener* listener) {m_color_selector_dialog_listener = listener;}
	void SetColor(UINT32 color);

private:
	UINT32 GetColorValue(const char* name, BOOL& valid);

	/**
	 * Switch between ARGB and ABGR color format
	 */
	INT32 SwitchRGB(UINT32 color);

	/**
	 * Remove '&' entries from label strings
	 *
	 * @param name Resource name
	 */
	void RemoveAmpersand(const char* name);

private:
	UINT32 m_color;
	ColorSelectorDialogListener* m_color_selector_dialog_listener;
	BOOL m_validate;
};


#endif
