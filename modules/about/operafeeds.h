/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Arne Martin Guettler
*/

#if defined WEBFEEDS_DISPLAY_SUPPORT && defined _LOCALHOST_SUPPORT_ && !defined OPERAFEEDS_H
#define OPERAFEEDS_H

#include "modules/about/opgenerateddocument.h"

class OpFeed;

/**
 * Generator for the opera:feeds document.
 */
class OperaFeeds : public OpGeneratedDocument
{
public:
	OperaFeeds(URL &url, OpFeed *preview_feed = NULL)
		: OpGeneratedDocument(url, OpGeneratedDocument::XHTML5), m_preview_feed(preview_feed)
	{}

	/**
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();

private:
	OpFeed *m_preview_feed;
};

#endif
