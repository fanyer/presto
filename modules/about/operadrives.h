/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#if !defined MODULES_ABOUT_OPERADRIVES_H && defined SYS_CAP_FILESYSTEM_HAS_MULTIPLE_DRIVES && defined _LOCALHOST_SUPPORT_
#define MODULES_ABOUT_OPERADRIVES_H

#include "modules/about/opgenerateddocument.h"

/**
 * Generator for the opera:drives (drive letter listing) document.
 */
class OperaDrives : public OpGeneratedDocument
{
public:
	/**
	 * Constructor for the drive letter listing page generator.
	 *
	 * @param url URL to write to.
	 */
	OperaDrives(URL &url)
		: OpGeneratedDocument(url, OpGeneratedDocument::HTML5)
		{}

	/**
	 * Generate the opera:drives document to the specified internal URL.
	 *
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();
};

#endif
