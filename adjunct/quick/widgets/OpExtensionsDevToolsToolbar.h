/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_EXTENSIONSDEVTOOLSTOOLBAR_H
#define OP_EXTENSIONSDEVTOOLSTOOLBAR_H

#include "adjunct/quick_toolkit/widgets/OpToolbar.h"

/***********************************************************************************
 **
 **	 OpExtensionsDevToolsToolbar
 **
 **  Toolbar with developer tools in extensions panel
 **
 **
 ***********************************************************************************/

class OpExtensionsDevToolsToolbar : public OpToolbar
{
public:

	OpExtensionsDevToolsToolbar();
	~OpExtensionsDevToolsToolbar();

	void ToggleVisibility();
	void Show();
	void Hide(BOOL focus_page = TRUE);

	// == Hooks ======================

	virtual void		OnAlignmentChanged();
	virtual void		OnReadContent(PrefsSection *section);

	// == OpInputContext ======================

	virtual BOOL			OnInputAction(OpInputAction* action);
	virtual const char*		GetInputContextName() {return "Extensions Developer Tools Toolbar";}

};

#endif // OP_EXTENSIONSDEVTOOLSTOOLBAR_H