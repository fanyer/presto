/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef QUICK_DIALOG_H
#define QUICK_DIALOG_H

#include "adjunct/quick_toolkit/widgets/QuickWindow.h"

class QuickButtonStrip;
class QuickSkinElement;
class QuickWidget;


/** @brief Represents a UI dialog
  */
class QuickDialog : public QuickWindow
{
public:
	QuickDialog();

	void SetParentWindow(DesktopWindow* parent) { QuickWindow::SetParentWindow(parent); }
	void SetStyle(OpWindow::Style style) { QuickWindow::SetStyle(style); }

	/** Set standard dialog header
	  * @param header header content
	  */
	void SetDialogHeader(QuickWidget* header);

	/** Set standard dialog contents
	  * @param button_strip Button strip to use at bottom of dialog
	  * @param main_content Content of dialog
	  * @param alert_image If non-NULL, skin name of an image to use next to the dialog content
	  */
	OP_STATUS SetDialogContent(QuickButtonStrip* button_strip, QuickWidget* main_content, const OpStringC8& alert_image = NULL);

	/** @return the main content widget
	  */
	QuickWidget* GetDialogContent() const;

	/** This method has the same functionality as standard Init() but allows to determine if
	 * dialog's content can be scrolled.
	 *  @param content_can_be_scrolled determines if dialog's content can be scrolled
	 */
	OP_STATUS Init(BOOL content_can_be_scrolled);

	/** Overridable functions for getting skin details for dialog elements
	 * @return Name of skin element to use by default
	 */
	virtual const char* GetDefaultHeaderSkin() { return "Dialog Header Skin"; }
	virtual const char* GetDefaultContentSkin() { return "Dialog Page Skin"; }
	virtual const char* GetDefaultButtonSkin() { return "Dialog Button Border Skin"; }

	// Overriding QuickWindow
	virtual OP_STATUS Init();
	virtual OP_STATUS SetName(const OpStringC8 & window_name);
	virtual const char* GetDefaultSkin() const { return "Dialog Skin"; }
	virtual BOOL SkinIsBackground() const { return FALSE; }

private:
	OP_STATUS ConstructAlertContent(QuickWidget* content, const OpStringC8& alert_image);
	void UpdateSkin();

	BOOL               m_content_can_be_scrolled;
	QuickSkinElement*  m_header;
	QuickSkinElement*  m_content;
	QuickSkinElement*  m_buttonstrip;
};

#endif // QUICK_DIALOG_H
