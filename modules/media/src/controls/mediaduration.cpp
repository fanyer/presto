/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2009-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef MEDIA_PLAYER_SUPPORT
#if !defined(MEDIA_SIMPLE_CONTROLS) || defined(JIL_CAMERA_SUPPORT)

#include "modules/media/src/controls/mediaduration.h"

#include "modules/skin/OpSkinManager.h"
#include "modules/display/vis_dev.h"

/* static */ void
MediaTimeHelper::DoubleToTime(double time, unsigned &hours, unsigned &minutes, unsigned &seconds)
{
	unsigned uint_time = static_cast<unsigned>(time);
	seconds = uint_time % 60;
	minutes = (uint_time % 3600) / 60;
	hours	= uint_time / 3600;
}

/* static */ OP_STATUS
MediaTimeHelper::TimeToString(double time, TempBuffer &str, BOOL for_measure)
{
	unsigned hours, minutes, seconds;
	DoubleToTime(time, hours, minutes, seconds);

	OP_STATUS status;
	if (for_measure)
	{
		/* Measure using all-zeros, based on the assumption that a '0'
		 * is likely to be the widest digit. The result of this
		 * measurement can thus be considered a pessimistic
		 * approximation. */
		const char* hms_template = "00:00:00";
		if (hours == 0)
		{
			hms_template += 3; /* Skip hour field. */

			if (minutes < 10)
				hms_template++; /* Only one digit minutes. */
		}
		else if (hours < 10)
			hms_template++; /* Only one digit hours. */

		status = str.Append(hms_template);
	}
	else
	{
		if (hours)
			status = str.AppendFormat(UNI_L("%i:%02i:%02i"), hours, minutes, seconds);
		else
			status = str.AppendFormat(UNI_L("%i:%02i"), minutes, seconds);
	}
	return status;
}

/* static */ OP_STATUS
MediaTimeHelper::GenerateTimeString(TempBuffer& res, double pos, double duration, BOOL for_measure)
{
	res.Clear();

	if (for_measure && op_isfinite(duration))
		pos = duration; /* Measure "<duration>/<duration>" to get the
						 * maximum number of digits. */

	if (op_isnan(pos))
		pos = 0;

	RETURN_IF_ERROR(TimeToString(pos, res, for_measure));

	if (!op_isfinite(duration))
		return OpStatus::OK;

	RETURN_IF_ERROR(res.Append('/'));
	return TimeToString(duration, res, for_measure);
}

/* static */ OP_STATUS
MediaDuration::Create(MediaDuration*& duration)
{
	if ((duration = OP_NEW(MediaDuration, ())))
	{
		duration->Init();
		return OpStatus::OK;
	}
	return OpStatus::ERR_NO_MEMORY;
}

void
MediaDuration::Init()
{
	GetBorderSkin()->SetImage("Media Duration");
	SetJustify(JUSTIFY_RIGHT, FALSE /* changed_by_user */);
}

/* virtual */ void
MediaDuration::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	// Background skin
	g_skin_manager->DrawElement(GetVisualDevice(), "Media Duration", paint_rect);

	TempBuffer time_str;
	RETURN_VOID_IF_ERROR(MediaTimeHelper::GenerateTimeString(time_str, m_position, m_duration));
	RETURN_VOID_IF_ERROR(string.Set(time_str.GetStorage(), this));

	// Adjust for padding
	OpRect rect(paint_rect);
	INT32 pad_left, pad_top, pad_right, pad_bottom;
	GetBorderSkin()->GetPadding(&pad_left, &pad_top, &pad_right, &pad_bottom);
	rect.x += pad_left;
	rect.width -= pad_left + pad_right;

	UINT32 color;
	g_skin_manager->GetTextColor("Media Duration", &color);
	string.Draw(rect, GetVisualDevice(), color);
}

/* virtual */ void
MediaDuration::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	// Get height from skin
	GetBorderSkin()->GetSize(w, h);

	// Get padding
	INT32 pad_left, pad_top, pad_right, pad_bottom;
	GetBorderSkin()->GetPadding(&pad_left, &pad_top, &pad_right, &pad_bottom);

	// Get width from measuring widest possible string
	TempBuffer time_str;
	RETURN_VOID_IF_ERROR (MediaTimeHelper::GenerateTimeString(time_str, m_position, m_duration, TRUE /* for_measure */));
	RETURN_VOID_IF_ERROR (string.Set(time_str.GetStorage(), this));
	*w = string.GetWidth() + pad_left + pad_right;

	string.Reset(this);
}

#endif // !defined(MEDIA_SIMPLE_CONTROLS) || defined(JIL_CAMERA_SUPPORT)
#endif // MEDIA_PLAYER_SUPPORT
