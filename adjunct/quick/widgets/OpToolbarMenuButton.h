/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Petter Nilsen
 */

#ifndef OP_TOOLBAR_MENU_BUTTON_H
#define OP_TOOLBAR_MENU_BUTTON_H

#include "modules/widgets/OpButton.h"

/***********************************************************************************
**  @class	OpToolbarMenuButton
**	@brief	Custom menu that will contain the main menu, but on a toolbar
**
************************************************************************************/
class OpToolbarMenuButton : public OpButton
{
public:
	static OP_STATUS Construct(OpToolbarMenuButton** obj, OpButton::ButtonStyle button_style = OpButton::STYLE_IMAGE);

	void UpdateButton(OpButton::ButtonStyle button_style);

	// == OpWidget ================================
	virtual void	GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

protected:
	OpToolbarMenuButton(OpButton::ButtonStyle button_style = OpButton::STYLE_IMAGE);
	OpToolbarMenuButton(const OpToolbarMenuButton & menu_button);
	virtual ~OpToolbarMenuButton() {}

	void Init();

	void SetSkin();

	// == OpTypedObject ===========================
	virtual	Type	GetType() {return WIDGET_TYPE_TOOLBAR_MENU_BUTTON;}

	// == OpInputContext ======================
	virtual	BOOL OnInputAction(OpInputAction* action);
	virtual	BOOL IsInputContextAvailable(FOCUS_REASON reason);

	// == OpWidget ================================
	virtual BOOL	OnContextMenu(const OpPoint &point, BOOL keyboard_invoked);
};

#endif // OP_TOOLBAR_MENU_BUTTON_H
