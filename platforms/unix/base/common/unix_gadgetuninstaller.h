/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef UNIXGADGETUNINSTALLER_H
#define UNIXGADGETUNINSTALLER_H

#ifdef WIDGET_RUNTIME_SUPPORT

struct GadgetInstallerContext;


class UnixGadgetUninstaller
{
	public:
		UnixGadgetUninstaller();

		OP_STATUS Init(const GadgetInstallerContext& context);

		/**
		 * Uninstalls gadget files.
		 *
		 * @return status
		 */
		OP_STATUS Uninstall();

	private:
		const GadgetInstallerContext* m_context;

		/**
		 * @return directory containing widget's config.xml
		 */
		const OpStringC& GetGadgetDir() const;

		/**
		 * @return name of installed gadget in form:
		 * 		opera-widget-<normalize name>-<no>
		 */
		const OpStringC& GetInstallationName() const;

		OP_STATUS RemoveShareDir();

		OP_STATUS RemoveWrapperScript();

		OP_STATUS RemoveDesktopEntries();

		OP_STATUS UnlinkWrapperScript();
};

#endif // WIDGET_RUNTIME_SUPPORT
#endif // UNIXGADGETUNINSTALLER_H
