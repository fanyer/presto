/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_MEDIA_RULE_H
#define CSS_MEDIA_RULE_H

#include "modules/style/css_media.h"
#include "modules/style/src/css_conditional_rule.h"

class CSS_MediaRule : public CSS_ConditionalRule
{
public:

	CSS_MediaRule(CSS_MediaObject* media) : m_media_object(media) {}

	virtual BOOL Evaluate(FramesDocument* doc, CSS_MediaType medium);

	virtual Type GetType() { return MEDIA; }

	virtual ~CSS_MediaRule() { OP_DELETE(m_media_object); }

	short GetMediaTypes() { return m_media_object ? m_media_object->GetMediaTypes() : short(CSS_MEDIA_TYPE_ALL); }

	CSS_MediaObject* GetMediaObject(BOOL create = FALSE);

	virtual OP_STATUS GetCssTextHeader(CSS* stylesheet, TempBuffer* buf);
	virtual CSS_PARSE_STATUS SetCssText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len);

private:

	/** A media object representation of the media query list for this rule. */
	CSS_MediaObject* m_media_object;
};

#endif // CSS_MEDIA_RULE_H
