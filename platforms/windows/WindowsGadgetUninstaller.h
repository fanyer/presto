/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef WINDOWS_GADGET_UNINSTALLER_H
#define WINDOWS_GADGET_UNINSTALLER_H

#ifdef WIDGET_RUNTIME_SUPPORT

struct GadgetInstallerContext;


/**
 * Abstracts the Windows gadget uninstaller script creation.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class WindowsGadgetUninstaller
{
public:
	/**
	 * Writes the uninstaller script for a gadget.
	 *
	 * @param script_path the desired path to the script
	 * @param context describes the gadget installation
	 * @return status
	 */
	static OP_STATUS WriteScript(const OpStringC& script_path,
			const GadgetInstallerContext& context);

private:
	/**
	 * Transforms the gadget name so that it's suitable to use it in Visual
	 * Basic script.
	 *
	 * @param str string to be updated with VBS escape code
	 * @return status
	 */
	static OP_STATUS EscapeCharForVBSStr(OpString& str);
};

#endif // WIDGET_RUNTIME_SUPPORT
#endif // WINDOWS_GADGET_UNINSTALLER_H
