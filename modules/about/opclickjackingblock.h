/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OPCLICKJACKINGBLOCK_H
#define OPCLICKJACKINGBLOCK_H

#include "modules/about/opgenerateddocument.h"

/**
 * Generate a click jack block page, with a link the user
 * can click and proceed to the link that was blocked.
 */
class OpClickJackingBlock : public OpGeneratedDocument
{
public:
	/**
	 * @param url          URL into which the new page will be generated.
	 * @param blocked_url  URL that was blocked, used just for display.
	 */
	OpClickJackingBlock(const URL& url, const URL& blocked_url);

	/**
	 * @return OK on success, or any error reported by URL or string code.
	 */
	virtual OP_STATUS GenerateData();

private:
	URL m_blocked_url;
};

#endif // OPCLICKJACKINGBLOCK_H
