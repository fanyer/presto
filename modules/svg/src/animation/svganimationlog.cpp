/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/animation/svganimationlog.h"
#include "modules/svg/src/animation/svgtimingarguments.h"
#include "modules/svg/src/SVGTimedElementContext.h"
#include "modules/svg/src/SVGDocumentContext.h"

#ifdef SVG_ANIMATION_LOG

#define MAX_ELEMENT_NAME 100

/* virtual */
SVGAnimationLogListener::~SVGAnimationLogListener()
{
    outfile.Close();
}

/* virtual */ void
SVGAnimationLogListener::Notification(const NotificationData &notification_data)
{
    if (notification_data.notification_type == NotificationData::INTERVAL_CREATED)
    {
		const SVGAnimationInterval *interval = notification_data.extra.interval_created.animation_interval;
		const uni_char *element_name = notification_data.timing_arguments->timed_element_context->GetElement()->GetId();

		char buf[MAX_ELEMENT_NAME + 1]; /* ARRAY OK 2009-04-23 ed */

		if (element_name != NULL)
		{
			unsigned element_name_length = uni_strlen(element_name);
			if (uni_cstrlcpy(buf, element_name, element_name_length) >= MAX_ELEMENT_NAME)
				buf[MAX_ELEMENT_NAME] = '\0';
		}
		else
		{
			int res = op_snprintf(buf, MAX_ELEMENT_NAME, "%p", notification_data.timing_arguments->timed_element_context->GetElement());
			if(res > MAX_ELEMENT_NAME)
				res = MAX_ELEMENT_NAME;
			buf[res > 0 ? res : 0] = '\0';
		}

		outfile.Print("INTERVAL %d %d %d \"%s\"\n",
					  (int)interval->Begin(),
					  (int)interval->SimpleDuration(),
					  (int)interval->End(),
					  buf);
    }
}

/* virtual */ BOOL
SVGAnimationLogListener::Accept(SVGAnimationWorkplace *potential_animation_workplace)
{
    OP_NEW_DBG("SVGAnimationLogListener", "svg_animation_log");
    if (animation_workplace == NULL)
    {
	animation_workplace = potential_animation_workplace;

	URL url = animation_workplace->GetSVGDocumentContext()->GetURL();

	OpString filename;
	filename.AppendFormat(UNI_L("animation-%.0f.log"),
			      g_op_time_info->GetRuntimeMS());

	OP_DBG((UNI_L("Animation log started. Output goes to '%s'\n"), filename.CStr()));

	if (OpStatus::IsError(outfile.Construct(filename, OPFILE_HOME_FOLDER)))
	    return FALSE;

	if (OpStatus::IsError(outfile.Open(OPFILE_WRITE)))
	    return FALSE;

	outfile.Print("Animation log v1.0\n");
	outfile.Print("URL \"%s\"\n", url.GetName(FALSE, PASSWORD_HIDDEN));

	return TRUE;
    }
    else
    {
	return FALSE;
    }
}

#endif // SVG_ANIMATION_LOG

#endif // SVG_SUPPORT
