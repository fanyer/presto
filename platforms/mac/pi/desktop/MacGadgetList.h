/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MACGADGETLIST_H
#define MACGADGETLIST_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/widgetruntime/pi/PlatformGadgetList.h"


/**
 * @author
 */
class MacGadgetList : public PlatformGadgetList
{
public:
	virtual BOOL HasChangedSince(time_t last_update_time) const;

	virtual OP_STATUS GetAll(
			OpAutoVector<GadgetInstallerContext>& gadget_contexts,
			BOOL list_globally_installed_gadgets = FALSE) const;
	
private:	
	OP_STATUS GetWidgetListFilePath(OpString& widgetlist_file_path) const;
	OP_STATUS CreateGadgetContext(void* name,void *normalizedName, void *path, void *wgtPath, OpAutoPtr<GadgetInstallerContext>& context) const;
	OP_STATUS CreateGadgetContextFromBundle(void *aBundle, void *normalizedName,
								  OpAutoPtr<GadgetInstallerContext>& context) const;
};

#endif // WIDGET_RUNTIME_SUPPORT
#endif // MACGADGETLIST_H
