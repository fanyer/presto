// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Arjan van Leeuwen (arjanl)

#ifndef OP_HOVER_BUTTON_H
#define OP_HOVER_BUTTON_H

#include "modules/widgets/OpButton.h"

/** @brief Button that can change text properties on hover
  * @author Arjan van Leeuwen
  *
  * Use this button to create a 'link-style' button that changes on hover
  */
class OpHoverButton : public OpButton
{
protected:
	/** Protected constructor
	  */
	OpHoverButton(UINT32 hover_color, ButtonType button_type = TYPE_CUSTOM, ButtonStyle button_style = STYLE_TEXT)
		: OpButton(button_type, button_style), m_hover_color(hover_color) {}

public:
	/** Construction function
	  * @param hover_color Color of text when hovered
	  */
	static OP_STATUS Construct(OpHoverButton** obj, UINT32 hover_color, ButtonType button_type = TYPE_CUSTOM, ButtonStyle button_style = STYLE_TEXT);

	/** Called when mouse is moved on this widget
	  */
	virtual void OnMouseMove(const OpPoint &point);

	/** Called when mouse leaves this widget
	  */
	virtual void OnMouseLeave();

protected:
	UINT32			m_hover_color; 	///< Color used when hovering over button
	OpWidgetString	m_right_string; ///< Text that will be displayed right of string
};

#endif // OP_HOVER_BUTTON_H
