/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/style/css_viewport.h"

#ifdef CSS_VIEWPORT_SUPPORT

# include "modules/display/FontAtt.h"
# include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"

const CSS_ViewportLength
CSS_ViewportLength::ToPixels(const CSS_DeviceProperties& device_props, BOOL horizontal) const
{
	if (m_type == AUTO || m_type == PX)
		return *this;

	double length(0);

	switch (m_type)
	{
	case DEVICE_WIDTH:
		length = double(device_props.device_width);
		break;
	case DEVICE_HEIGHT:
		length = double(device_props.device_height);
		break;
	case DESKTOP_WIDTH:
		if (device_props.desktop_width == 0)
			return CSS_ViewportLength(AUTO);
		else
			length = double(device_props.desktop_width);
		break;
	case PERCENT:
		if (horizontal)
			length = (device_props.viewport_width * m_length) / 100;
		else
			length = (device_props.viewport_height * m_length) / 100;
		break;
	case EM:
	case REM:
		length = m_length * device_props.font_size;
		break;
	case EX:
		{
			FontAtt log_font;
			g_pcfontscolors->GetFont(OP_SYSTEM_FONT_DOCUMENT_NORMAL, log_font);

			length = m_length * log_font.GetExHeight();
		}
		break;
	case CM:
		length = (m_length * 96) / 2.54;
		break;
	case MM:
		length = (m_length * 96) / 25.4;
		break;
	case INCH:
		length = m_length * 96;
		break;
	case PT:
		length = (m_length * 96) / 72;
		break;
	case PC:
		length = (m_length * 96) / 6;
		break;
	default:
		OP_ASSERT(!"Some unit is not handled.");
		break;
	}

	return CSS_ViewportLength(length, PX);
}

void
CSS_Viewport::ResetComputedValues()
{
	CSS_ViewportLength auto_length;
	m_min_width = auto_length;
	m_max_width = auto_length;
	m_min_height = auto_length;
	m_max_height = auto_length;
	m_zoom = CSS_VIEWPORT_ZOOM_AUTO;
	m_min_zoom = CSS_VIEWPORT_ZOOM_AUTO;
	m_max_zoom = CSS_VIEWPORT_ZOOM_AUTO;
	m_zoomable = TRUE;
	m_orientation = Auto;
	m_packed_init = 0;
}

void
CSS_Viewport::Constrain(const CSS_DeviceProperties& device_props)
{
	/* Sanity check for input values. */

	OP_ASSERT(device_props.device_width > 0 &&
			  device_props.device_height > 0 &&
			  device_props.viewport_width > 0 &&
			  device_props.viewport_height > 0 &&
			  device_props.font_size > 0);

	m_actual_zoom = m_zoom;
	m_actual_min_zoom = m_min_zoom;
	m_actual_max_zoom = m_max_zoom;
	m_actual_zoomable = m_zoomable;
	m_actual_orientation = m_orientation;

	/* 1. Resolve relative and absolute lengths, percentages, and keywords ('device-width',
		  'device-height') to pixel values for the 'min-width', 'max-width',
		  'min-height' and 'max-height' properties. */

	CSS_ViewportLength min_width = m_min_width.ToPixels(device_props, TRUE);
	CSS_ViewportLength max_width = m_max_width.ToPixels(device_props, TRUE);
	CSS_ViewportLength min_height = m_min_height.ToPixels(device_props, FALSE);
	CSS_ViewportLength max_height = m_max_height.ToPixels(device_props, FALSE);

	/* 2. If min-width or max-width is not 'auto', set width = MAX(min-width, MIN(max-width, initial-width)) */

	CSS_ViewportLength width;
	if (!min_width.IsAuto() || !max_width.IsAuto())
	{
		CSS_ViewportLength initial_width(device_props.viewport_width, CSS_ViewportLength::PX);
		width = Max(min_width, Min(max_width, initial_width));
	}

	/* 3. If min-height or max-height is not 'auto', set height = MAX(min-height, MIN(max-height, initial-height)) */

	CSS_ViewportLength height;
	if (!min_height.IsAuto() || !max_height.IsAuto())
	{
		CSS_ViewportLength initial_height(device_props.viewport_height, CSS_ViewportLength::PX);
		height = Max(min_height, Min(max_height, initial_height));
	}

	/* 4. If min-zoom is not 'auto' and max-zoom is not 'auto', set max-zoom = MAX(min-zoom, max-zoom) */

	if (m_actual_max_zoom != CSS_VIEWPORT_ZOOM_AUTO &&
		m_actual_min_zoom != CSS_VIEWPORT_ZOOM_AUTO &&
		m_actual_max_zoom < m_actual_min_zoom)
		m_actual_max_zoom = m_actual_min_zoom;

	/* 5. If zoom is not 'auto', set zoom = MIN(max-zoom, MAX(min-zoom, zoom)) */

	if (m_actual_zoom != CSS_VIEWPORT_ZOOM_AUTO)
	{
		if (m_actual_min_zoom != CSS_VIEWPORT_ZOOM_AUTO && m_actual_zoom < m_actual_min_zoom)
			m_actual_zoom = m_actual_min_zoom;
		else if (m_actual_max_zoom != CSS_VIEWPORT_ZOOM_AUTO && m_actual_zoom > m_actual_max_zoom)
			m_actual_zoom = m_actual_max_zoom;
	}

	if (width.IsAuto())
	{
		if (m_actual_zoom == CSS_VIEWPORT_ZOOM_AUTO)
			/* 6. If width and zoom are both 'auto', set width = initial-width */
			width = device_props.viewport_width;
		else if (height.IsAuto())
			/* 7. If width is 'auto', and height is 'auto', set width = (initial-width / zoom) */
			width = device_props.viewport_width / m_actual_zoom;
		else
			/* 8. If width is 'auto', set width = height * (initial-width / initial-height) */
			width = double(height) * device_props.viewport_width / double(device_props.viewport_height);
	}

	/* Check that width has been resolved. */
	OP_ASSERT(!width.IsAuto());

	/* 9. If height is 'auto', set height = width * (initial-height / initial-width) */

	if (height.IsAuto())
		height = double(width) * device_props.viewport_height / double(device_props.viewport_width);

	/* Check that height has been resolved. */
	OP_ASSERT(!height.IsAuto());

	if (m_actual_zoom != CSS_VIEWPORT_ZOOM_AUTO || m_actual_max_zoom != CSS_VIEWPORT_ZOOM_AUTO)
	{
		double max_initial_zoom = m_actual_zoom == CSS_VIEWPORT_ZOOM_AUTO ? m_actual_max_zoom : m_actual_zoom;

		/* 10. If zoom or max-zoom is not 'auto', set width = MAX(width, (initial-width / MIN(zoom, max-zoom))) */

		double extended_width(device_props.viewport_width / max_initial_zoom);
		if (double(width) < extended_width)
			width = extended_width;

		/* 11. If zoom or max-zoom is not 'auto', set height = MAX(height, (initial-height / MIN(zoom, max-zoom))) */

		double extended_height(device_props.viewport_height / max_initial_zoom);
		if (double(height) < extended_height)
			height = extended_height;
	}

	m_actual_width = width;
	m_actual_height = height;
}

#endif // CSS_VIEWPORT_SUPPORT
