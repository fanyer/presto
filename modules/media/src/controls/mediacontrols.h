/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2009-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MEDIA_CONTROLS_H
#define MEDIA_CONTROLS_H

#ifdef MEDIA_PLAYER_SUPPORT

#include "modules/media/src/controls/mediafader.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpWidget.h"

class MediaButton;
class MediaControls;
class MediaDuration;
class MediaSlider;
class OpMediaTimeRanges;
class VolumeSlider;
class VolumeSliderContainer;

class MediaControlsOwner
{
public:
	virtual double GetMediaDuration() const = 0;
	virtual double GetMediaPosition() = 0;
	virtual OP_STATUS SetMediaPosition(const double position) = 0;
	virtual double GetMediaVolume() const = 0;
	virtual OP_STATUS SetMediaVolume(const double volume) = 0;
	virtual BOOL GetMediaMuted() const = 0;
	virtual OP_STATUS SetMediaMuted(const BOOL muted) = 0;
	virtual BOOL GetMediaPaused() const = 0;
	virtual OP_STATUS MediaPlay() = 0;
	virtual OP_STATUS MediaPause() = 0;

	/** Get buffered time ranges.
	 *
	 * @param[out] ranges Set to the time ranges in seconds which are currently
	 * buffered and could be played back without network access (unless seeking
	 * is required to reach those ranges, in which case it cannot be
	 * guaranteed).
	 *
	 * @return A MediaTimeRanges pointer which may be used to query buffered
	 *         time range information, or NULL if not supported. If we run
	 *         out of memory, NULL will be returned after notifying the memory
	 *         manager (i.e. the caller does not need to do this).
	 */
	virtual const OpMediaTimeRanges* GetMediaBufferedTimeRanges() = 0;

	/** Should the controls be painted with opacity?
	 *
	 * @return TRUE if controls may be painted with an opacity, or
	 *         FALSE if they must be either invisible or opaque.
	 */
	virtual BOOL UseControlsOpacity() const = 0;

	/** Should the controls be faded in and out?
	 *
	 * @return TRUE if controls visibility depends on mouse or
	 *         keyboard focus, or FALSE if they are always visible.
	 */
	virtual BOOL UseControlsFading() const = 0;

	virtual void SetControlsVisible(bool visible) = 0;
	virtual void HandleFocusGained() = 0;
};

/** Custom media controls button with skin and icon. */
class MediaButton
	: public OpButton
{
public:
	/** Create a new MediaButton object. */
	static OP_STATUS Create(MediaButton*& button);

	/** Set playing/paused. */
	void SetPaused(BOOL paused);

	/** Set volume. */
	void SetVolume(double volume);

	// OpWidget overrides
	virtual void OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);
	virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

private:
	MediaButton() : OpButton(TYPE_CUSTOM, STYLE_IMAGE) {}
};

/** Dummy wrapper to avoid ambiguity when OpWidget itself inherits from
 *  OpWidgetListener, as it does on Desktop (evenes). */
class MediaButtonListener : public OpWidgetListener
{
public:
	virtual ~MediaButtonListener() {}
};

/** Media controls container. */
class MediaControls : public OpWidget, public MediaButtonListener, public FaderListener
{
public:
	static OP_STATUS Create(MediaControls*& controls, MediaControlsOwner* owner);
	~MediaControls();

#ifndef MEDIA_SIMPLE_CONTROLS
	// OpInputContext overrides
	virtual const char*	GetInputContextName() { return "Media Controls"; }
	virtual BOOL OnInputAction(OpInputAction* action);
	virtual void OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);
	virtual void OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);
#endif // !MEDIA_SIMPLE_CONTROLS

	// OpWidget overrides
	virtual void OnResize(INT32* new_w, INT32* new_h);
	virtual void OnClick(OpWidget* widget, UINT32 id = 0);
	virtual void GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);
#ifndef MOUSELESS
	/* These seemingly meaningless overrides are implemented to avoid warnings
	   caused, by the overrides of same-named functions from OpWidgetListener
	   (via MediaButtonListener). */
	virtual void OnMouseMove(const OpPoint &point) { OpWidget::OnMouseMove(point); }
	virtual void OnMouseLeave() { OpWidget::OnMouseLeave(); }
#endif // MOUSELESS

#ifndef MEDIA_SIMPLE_CONTROLS
	virtual void OnMove();

	// MediaButtonListener (OpWidgetListener) implementation
	virtual void OnChange(OpWidget* widget, BOOL changed_by_mouse);
	virtual void OnMouseMove(OpWidget* widget, const OpPoint &point);
	virtual void OnMouseLeave(OpWidget* widget);

#endif // !MEDIA_SIMPLE_CONTROLS

	// FaderListener implementation
	void OnOpacityChange(MediaControlsFader* fader, UINT8 opacity);
	void OnVisibilityChange(MediaControlsFader* fader, BOOL visible);

	/** Paint the controls in the content area. */
	void Paint(VisualDevice* visdev, const OpRect& content);

	/** Called when the mouse enters the element area. */
	void OnElementMouseOver();

	/** Called when the mouse exits the element area. */
	void OnElementMouseOut();

	/** Play/pause state has changed. */
	void OnPlayPause();

	/** Duration has become known or changed. */
	void OnDurationChange();

	/** The current time has changed due to playing or seeking.
	 *
	 * @param invalidate TRUE if controls need to be invalidated,
	 * FALSE if they will be repainted anyway.
	 */
	void OnTimeUpdate(BOOL invalidate);

	/** The volume or muted state has changed. */
	void OnVolumeChange();

	/** Called when the set of buffered ranges has changed. */
	void OnBufferedRangesChange();

	/** Get the area occupied by the controls.
	 *
	 * @param content An OpRect within which to position the controls.
	 * @return An OpRect contained within content, in the same
	 *         coordinate system as content. (Not relative to it.)
	 */
	OpRect GetControlArea(const OpRect& content);

	/** Ensure the visibility of the controls.
	 *
	 * The controls are shown or hidden as necessary to reflect the
	 * current state.
	 */
	void EnsureVisibility();

	/** Force the the controls to show.
	 *
	 * If controls should be shown, show them immediately (without any
	 * fading et.c).
	 */
	void ForceVisibility();

#ifndef MEDIA_SIMPLE_CONTROLS
	void DetachVolumeSlider() { m_volume_slider_container = NULL; }
#endif // !MEDIA_SIMPLE_CONTROLS

private:
	MediaControls(MediaControlsOwner* owner);

	OP_STATUS Init();
	bool IsShowing();
	void Show(MediaControlsFader& fader);
	void Hide(MediaControlsFader& fader);

	void PaintInternal(VisualDevice* visdev, const OpRect& paint_rect);

	MediaControlsOwner* m_owner;

	MediaButton* m_play_pause;
	MediaControlsFader m_fader;

#ifndef MEDIA_SIMPLE_CONTROLS
	MediaButton* m_volume;
	MediaSlider* m_slider;
	MediaDuration* m_duration;
	VolumeSliderContainer* m_volume_slider_container;
	MediaControlsFader m_volume_fader;
	BOOL m_has_keyboard_focus;
#endif // !MEDIA_SIMPLE_CONTROLS

	UINT8 m_opacity;
	BOOL m_hover;
};

#endif // MEDIA_PLAYER_SUPPORT
#endif // MEDIA_CONTROLS_H
