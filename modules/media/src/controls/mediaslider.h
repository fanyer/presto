/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2009-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MEDIASLIDER_H
#define MEDIASLIDER_H

#ifdef MEDIA_PLAYER_SUPPORT
#ifndef MEDIA_SIMPLE_CONTROLS

#include "modules/widgets/OpWidget.h"
#include "modules/widgets/WidgetWindow.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/media/mediatimeranges.h"

class MediaElement;
class MediaControls;
class MediaControlsOwner;

/** Custom slider widget for media progress and volume sliders. */
class MediaSlider
	: public OpWidget
{
public:
	enum MediaSliderType { Progress, Volume };

	/** Create a new MediaSlider object. */
	static OP_STATUS Create(MediaSlider*& slider, MediaSliderType slider_type);

	void SetMinValue(double min) { m_min = min; }
	void SetMaxValue(double max) { m_max = max; }
	void SetValue(double val, BOOL invalidate = TRUE);

	/** Get the current slider value */
	double GetCurrentValue() const { return m_current; }

	/** Get the last fixed value.
	 *
	 * When dragging this would be the value before the dragging
	 * started.
	 */
	double GetLastFixedValue() const { return m_current; }

	/** Is the slider currently being dragged? */
	BOOL IsDragging() const { return m_is_dragging; }

	/** Set the buffered time ranges.
	 *
	 * The buffered time ranges may be used by Progress sliders to draw
	 * the buffered regions of the progress track in a different color.
	 *
	 * The function will copy the incoming data, so the lifetime of the
	 * OpMediaTimeRanges object is not important.
	 *
	 * @param ranges The OpMediaTimeRanges that contains the time ranges.
	 * @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS SetBufferedRanges(const OpMediaTimeRanges* ranges)
	{
		return m_buffered_ranges.SetRanges(ranges);
	}

	/// OpWidget implementation
	virtual void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
	virtual void OnMouseMove(const OpPoint &point);
	virtual void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	virtual void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
	virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);
	virtual void SetValue(INT32 value) {}

	// OpInputContext implementation
	virtual BOOL OnInputAction(OpInputAction* action);
	virtual const char*	GetInputContextName() { return "Progress slider"; }

private:
	void HandleOnChange(BOOL changed_by_mouse);

protected:
	MediaSlider(MediaSliderType slider_type);

	/** Get the track skin name. */
	const char* GetTrackSkin() const { return m_slider_type == Progress ? "Media Seek Track" : "Media Volume Track"; }

	/** Get the knob skin name. */
	const char* GetKnobSkin() const { return m_slider_type == Progress ? "Media Seek Knob" : "Media Volume Knob"; }

	/** Is the slider oriented horizontally? */
	bool IsHorizontal() const { return m_slider_type == Progress; }

	/** Check if a (mouse) point is in the knob area. */
	BOOL IsPointInKnob(const OpPoint &point) const;

	/** Calculate the rect of the knob relative to the widget.
	 *
	 * Maps the current slider value to the size of the slider,
	 * taking skin margins into account.
	 *
	 * @param res Rect of the knob
	 * @return true if a valid rect could be found, false otherwise.
	 */
	bool CalcKnobRect(OpRect& res) const;

	/** Map a (mouse) point to the slider scale. */
	double PointToOffset(const OpPoint& point);

	/** Calculate an OpRect for a certain time interval in the track.
	 *
	 * The @c start of the range will be rounded away from zero (ceil), and the
	 * @c end of the range will be rounded towards zero (floor). This is to
	 * "properly" visualize tiny gaps (< 1px) that might be present in the
	 * total coverage.
	 *
	 * As an example, let's say that we are loading a ten second video clip,
	 * and that the buffered ranges currently are [0-4.2] (illustrated with
	 * '#'), and [4.8-10] (illustrated with '*').
	 *
	 * 0-----1-----2-----3-----4-----5-----6-----7-----8-----9-----10
	 * |#####|#####|#####|#####|##  *|*****|*****|*****|*****|*****|
	 * +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
	 *
	 * We encounter a problem when we want draw these buffered ranges on the
	 * screen. Let's say that we want to use a ten pixel wide rectangle to
	 * represent the buffering progress.
	 *
	 * +--0--+--1--+--2--+--3--+--4--+--5--+--6--+--7--+--8--+--9--+
	 * |#####|#####|#####|#####|#???*|*****|*****|*****|*****|*****|
	 * +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
	 *
	 * What to do about pixel 4? In time-units, it is partially overlapped by
	 * both the first and the second range. We could color the pixel to
	 * simplify the situation, but that would trick the user into thinking
	 * that the range really is continuous. To avoid losing the gap in the
	 * visual representation this function rounds the end points towards the
	 * center of the range.
	 *
	 * +--0--+--1--+--2--+--3--+--4--+--5--+--6--+--7--+--8--+--9--+
	 * |#####|#####|#####|#####|     |*****|*****|*****|*****|*****|
	 * +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
	 *
	 * This prevents a situation where the user might think that the video
	 * is loaded and disconnects from the Internet, thinking that the video
	 * will play back without a connection.
	 *
	 * The same will of course apply to gaps at the beginning and end of the
	 * file, so if we have one (or more) time range(s) like this:
	 *
	 * 0-----1-----2-----3-----4-----5-----6-----7-----8-----9-----10
	 * | ####|#####|#####|#####|#####|#####|#####|#####|#####|#### |
	 * +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
	 *
	 * It would be rounded inwards to yield this result on screen:
	 *
	 * +--0--+--1--+--2--+--3--+--4--+--5--+--6--+--7--+--8--+--9--+
	 * |     |#####|#####|#####|#####|#####|#####|#####|#####|     |
	 * +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
	 *
	 * Mapping a time range to a rectangle just ten pixels wide is of course
	 * ridiculous, but the same problem may realistically arise if a long
	 * video (e.g. two hours) is mapped to, say, 400 pixels. In that case,
	 * every horizontal pixel represents 18 seconds, so even gaps as large as
	 * (18 - 1) * 2 = 34 seconds (roughly) could trigger the problem.
	 *
	 * Note that because ranges are rounded inwards, very small ranges will
	 * be "lost":
	 *
	 * 0-----1-----2-----3-----4-----5-----6-----7-----8-----9-----10
	 * |     |     |     | ### |     | ****|***  |     |     |     |
	 * +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
	 *
	 * The above ranges would yield nothing on screen:
	 *
	 * +--0--+--1--+--2--+--3--+--4--+--5--+--6--+--7--+--8--+--9--+
	 * |     |     |     |     |     |     |     |     |     |     |
	 * +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
	 *
	 * This is the correct and expected behavior. It is the responsibility of
	 * the caller to calculate and provide the largest possible range(s), and
	 * not call this function repeatedly on fragments of a continuous range.
	 *
	 * @param bounds A rectangle representing the entire track.
	 * @param start The start of the interval (in seconds).
	 * @param end The end of the interval (in seconds).
	 * @return An OpRect matching the specified time interval.
	 */
	OpRect CalcRangeRect(const OpRect& bounds, double start, double end) const;

	/** Paint the portion(s) of the track that have been buffered.
	 *
	 * @param paint_rect The OpRect for this widget (from OnPaint).
	 */
	void PaintBufferedRanges(const OpRect& paint_rect);

	MediaSliderType m_slider_type;

	double m_min;
	double m_max;
	double m_current;

	BOOL m_is_dragging;

	MediaTimeRanges m_buffered_ranges;

#ifdef SELFTEST
public:
	OpRect DebugCalcRangeRect(const OpRect& bounds, double start, double end) const
	{
		return CalcRangeRect(bounds, start, end);
	}
#endif // SELFTEST
};

/** Container for volume slider. */
class VolumeSliderContainer : public WidgetWindow
{
private:
	VolumeSliderContainer(MediaControls* controls) :
		m_controls(controls)
	{}

	OP_STATUS Init();

public:

	/** Create a volume slider container.
	 *
	 * @param[out] container Pointer to the created container
	 * @param controls The main controls class
	 * @return Potential memory error
	 */
	static OP_STATUS Create(VolumeSliderContainer*& container, MediaControls* controls);

	virtual ~VolumeSliderContainer();

	/** Calculate preferred position of the volume slider. */
	OpRect CalculateRect();

	/** Update position of the container.
	 *
	 * This is typically called when the controls are moved.
	 */
	void UpdatePosition();

	/** Check if the widget window contains a certain input context. */
	BOOL Contains(const OpInputContext* context) const { return context == m_slider; }

	/** Called when the widget window is resized. */
	virtual void OnResize(UINT32 width, UINT32 height);

	/** Get current slider value. */
	double GetCurrentValue() const { return m_slider->GetCurrentValue(); }

	/** Is the slider currently being dragged. */
	BOOL IsDragging() const { return m_slider->IsDragging(); }

	/** Set volume slider opacity. */
	void SetOpacity(UINT8 opacity) { GetWindow()->SetTransparency(opacity); }

	void SetVolume(double volume) { m_slider->SetValue(volume); }

	/** Invalidate widget. */
	void Invalidate() { m_slider->InvalidateAll(); }

private:
	MediaSlider* m_slider;
	MediaControls* m_controls;
};

#endif // !MEDIA_SIMPLE_CONTROLS
#endif // MEDIA_PLAYER_SUPPORT
#endif // MEDIASLIDER_H
