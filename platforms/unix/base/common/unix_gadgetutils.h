/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 * 
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Tomasz Kupczyk (tkupczyk@opera.com)
 */

#ifndef UNIXGADGETUTILS_H
#define UNIXGADGETUTILS_H

#ifdef WIDGET_RUNTIME_SUPPORT

namespace UnixGadgetUtils
{
	/**
	 * Creates UNIX package from wgt file. Note, that responsibility of 
	 * closing pipe lies on caller.
	 *
	 * @param[in] wgt_file_path Path to wgt file
	 * @param[in] package_type Type of package to create e.g. deb
	 * @param[in] pipe Pipe for communication with script
	 * @param[out] dst_dir_path Output directory for package
	 * @param[in] output_file_name Override package name to output_file_name
	 * @param[in] blocking_call Block on call iff set to TRUE
	 * @return Error status.
	 */
	OP_STATUS CreateGadgetPackage(const uni_char* wgt_file_path,
			const uni_char* package_type,
			FILE** pipe,
			const uni_char* dst_dir_path = NULL,
			const uni_char* output_file_name = NULL,
			BOOL blocking_call = FALSE);

	/**
	 * Normalize profile name. Convert all to ASCII and replace
	 * spaces with minus ('-').
	 *
	 * @param[in] src String to normalize
	 * @param[out] dest Receives normalized src string
	 * @return Error status
	 */
	OP_STATUS NormalizeName(const OpStringC& src, OpString& dest);


	/**
	 * Get the widget name from the specified file. This file should
	 * be the config.xml of the widget
	 *
	 * @param filename[in] config.xml of the widget
	 * @param name[out] The widget name
	 *
	 * @return OpStatus::OK on success (but note that widget name can be empty),
	 *         OpStatus::ERR on parse error or OpStatus::ERR_NO_MEMORY on
	 *         insufficient memory available
	 */
	OP_STATUS GetWidgetName(const OpString& filename, OpString& name);
};

#endif // WIDGET_RUNTIME_SUPPORT
#endif // UNIXGADGETUTILS_H
