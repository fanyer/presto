/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 */

#ifndef OP_EXTENSION_SET_H
#define OP_EXTENSION_SET_H

#include "adjunct/quick_toolkit/widgets/OpToolbar.h"

/**
 * Toolbar, where extension buttons initially appear.
 *
 * It can be embedded inside other toolbars.
 */
class OpExtensionSet : public OpToolbar
{
public:

	OpExtensionSet();

	virtual ~OpExtensionSet() {}

	static OP_STATUS Construct(OpExtensionSet** extension_set);

	//
	// OpToolbar
	//

	virtual Type GetType() { return WIDGET_TYPE_EXTENSION_SET; }

	virtual void OnReadContent(PrefsSection *section);

	virtual void OnWriteContent(PrefsFile* prefs_file, const char* name);

	//
	// OpInputContext
	//

	virtual BOOL OnInputAction(OpInputAction *action);

};

#endif // OP_EXTENSIONS_SET_H
