/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2009-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MEDIADURATION_H
#define MEDIADURATION_H

#ifdef MEDIA_PLAYER_SUPPORT
#if !defined(MEDIA_SIMPLE_CONTROLS) || defined(JIL_CAMERA_SUPPORT)

#include "modules/widgets/OpWidget.h"

class MediaElement;

/** Helper functions for generating position/duration strings */
class MediaTimeHelper
{
public:
	/** Generate a time string from a media position and duration.
	 *
	 * @param[out] res Resulting string.
	 * @param pos Current position. NaN if unknown.
	 * @param duration Media duration. NaN if unknown.
	 * @param for_measure Generate a string to be used for measuring.
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	static OP_STATUS GenerateTimeString(TempBuffer& res, double pos, double duration, BOOL for_measure = FALSE);

private:
	static OP_STATUS TimeToString(double time, TempBuffer &str, BOOL for_measure);
	static void DoubleToTime(double time, unsigned &hours, unsigned &minutes, unsigned &seconds);
};

/** Display the position and duration in media controls. */
class MediaDuration : public OpWidget
{
public:
	/** Create a new MediaDuration object. */
	static OP_STATUS Create(MediaDuration*& duration);

	// OpWidget overrides
	virtual void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
	virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

	void SetPosition(double position, BOOL invalidate = TRUE) { m_position = position; if (invalidate) InvalidateAll(); }
	void SetDuration(double duration) { m_duration = duration; InvalidateAll(); }

private:
	MediaDuration()
		: m_position(0.0),
		  m_duration(0.0)
	{}

	void Init();

	OpWidgetString string;

	double m_position;
	double m_duration;
};

#endif //!defined(MEDIA_SIMPLE_CONTROLS) || defined(JIL_CAMERA_SUPPORT)
#endif // MEDIA_PLAYER_SUPPORT
#endif // MEDIADURATION_H
