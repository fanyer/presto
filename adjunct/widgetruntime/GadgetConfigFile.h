/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef GADGET_CONFIG_FILE_H
#define GADGET_CONFIG_FILE_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "modules/util/opfile/opfile.h"

struct GadgetInstallerContext;


/**
 * Represents a gadget installation configuration stored in a file.
 *
 * In the filesystem, this currently corresponds to an "install.conf" file
 * stored in the gadget installation directory.
 *
 * Note that this is something completely different from
 * GADGET_CONFIGURATION_FILE.  GADGET_CONFIGURATION_FILE is a crucial part of
 * the gadget.  Our "install.conf" is only created and used by our installation
 * mechanism.
 *
 * FIXME: The whole class and some GadgetStartup and related code should be
 * refactored so that a GadgetInstallInfo class replaces and augments
 * GadgetConfigFile.  A GadgetInstallInfo object would represent all the
 * information pertinent to a single widget installation: the widget profile
 * name, the location of GADGET_CONFIGURATION_FILE, etc.  Currently, code
 * dealing with these issues is scattered among several GadgetStartup methods,
 * GadgetConfigFile, and platform code.  And GadgetConigFile methods Write()
 * and Read() are too messy in their implementation.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class GadgetConfigFile
{
public:
	/**
	 * Constructs a GadgetConfigFile given a gadget installation context.
	 *
	 * @param installer_context the gadget installation context
	 */
	explicit GadgetConfigFile(const GadgetInstallerContext& installer_context);

	~GadgetConfigFile();

	/**
	 * Returns the gadget profile name used by the gadget installation
	 * corresponding to this configuration file.
	 *
	 * @return the gadget profile name
	 */
	const OpStringC& GetProfileName() const;

	/**
	 * Writes the gadget installation configuration to a physical file.
	 *
	 * @return status
	 */
	OP_STATUS Write() const;

	/**
	 * Returns the physical file containing the gadget installation
	 * configuration.
	 *
	 * @return the file with the gadget installation configuration
	 */
	const OpFile& GetFile() const;

	/**
	 * Constructs a GadgetConfigFile from a file stored in a gadget installation
	 * directory.
	 *
	 * @param gadget_path the path to the GADGET_CONFIGURATION_FILE file of the
	 * 		installed gadget
	 * @return @c NULL on failure (including "file not found", etc.)
	 */
	static GadgetConfigFile* Read(const OpStringC& gadget_path);


private:
	GadgetConfigFile();

	// Non-copyable.
	GadgetConfigFile(const GadgetConfigFile&);
	GadgetConfigFile& operator=(const GadgetConfigFile&);

	static OP_STATUS InitFile(const GadgetInstallerContext& context,
			OpFile& file);
	static OP_STATUS InitFile(const OpStringC& gadget_path, OpFile& file);

	GadgetInstallerContext* m_installer_context;
	OpFile m_file;
};


#endif // WIDGET_RUNTIME_SUPPORT
#endif // GADGET_CONFIG_FILE_H
