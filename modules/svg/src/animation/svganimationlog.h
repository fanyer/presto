/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_ANIMATION_LOG_H
#define SVG_ANIMATION_LOG_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/animation/svganimationworkplace.h"

#ifdef SVG_ANIMATION_LOG

class SVGAnimationLogListener :
	public SVGAnimationWorkplace::AnimationListener
{
public:
	SVGAnimationLogListener() :
		animation_workplace(NULL) {}

	virtual ~SVGAnimationLogListener();

	virtual void Notification(const NotificationData &notification_data);

	virtual BOOL Accept(SVGAnimationWorkplace *potential_animation_workplace);

	SVGAnimationWorkplace *animation_workplace;
	OpFile outfile;
};

#endif // SVG_ANIMATION_LOG
#endif // SVG_SUPPORT
#endif // !SVG_ANIMATION_LOG_H
