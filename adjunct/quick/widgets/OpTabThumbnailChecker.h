/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef OP_TAB_THUMBNAIL_CHECKER_H
#define OP_TAB_THUMBNAIL_CHECKER_H

#include "adjunct/quick_toolkit/widgets/OpBar.h"

class OpTabThumbnailChecker
{
public:

	OpTabThumbnailChecker(OpBar::Alignment alignment,
						  OpBar::Wrapping wrapping) :
		m_alignment(alignment),
		m_wrapping(wrapping) {}

	BOOL IsThumbnailAllowed()
	{
		return ThumbnailEnabledAlignment(m_alignment) &&
			ThumbnailEnabledWrapping(m_wrapping);
	}

protected:

	BOOL ThumbnailEnabledAlignment(OpBar::Alignment alignment)
	{
#ifdef TAB_THUMBNAILS_ON_SIDES
		return TRUE;
#else
		return (alignment == OpBar::ALIGNMENT_TOP || alignment == OpBar::ALIGNMENT_BOTTOM);
#endif // TAB_THUMBNAILS_ON_SIDES
	}

	BOOL ThumbnailEnabledWrapping(OpBar::Wrapping wrapping)
	{
		return (wrapping == OpBar::WRAPPING_OFF || wrapping == OpBar::WRAPPING_EXTENDER);
	}

private:

	const OpBar::Alignment m_alignment;
	const OpBar::Wrapping m_wrapping;
};

#endif // OP_TAB_THUMBNAIL_CHECKER_H
