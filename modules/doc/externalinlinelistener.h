/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef EXTERNAL_INLINE_LISTENER_H
#define EXTERNAL_INLINE_LISTENER_H

#include "modules/util/simset.h"

class FramesDocument;

/**
 * Interface used to monitor inline loads.
 */
class ExternalInlineListener
	: public ListElement<ExternalInlineListener>
{
private:
	friend class FramesDocument;
	BOOL has_inc_wait_for_paint_blockers;
protected:
	ExternalInlineListener() : has_inc_wait_for_paint_blockers(FALSE) {}
public:
	virtual void LoadingProgress(FramesDocument *document, const URL &url) { LoadingProgress(url); }
	virtual void LoadingStopped(FramesDocument *document, const URL &url) { LoadingStopped(url); }
	virtual void LoadingRedirected(FramesDocument *document, const URL &from, const URL &to) { LoadingRedirected(from, to); }

	virtual void LoadingProgress(const URL &url) {}
	virtual void LoadingStopped(const URL &url) {}
	virtual void LoadingRedirected(const URL &from, const URL &to) {}
};

#endif // EXTERNAL_INLINE_LISTENER_H
