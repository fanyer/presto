/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2009-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MEDIA_FADER_H
#define MEDIA_FADER_H

#ifdef MEDIA_PLAYER_SUPPORT

#include "modules/hardcore/timer/optimer.h"

class MediaControlsFader;

/** Callback listener for the MediaControlsFader. */
class FaderListener
{
public:
	/** Called when the opacity of the fader has changed. */
	virtual void OnOpacityChange(MediaControlsFader* fader, UINT8 opacity) = 0;

	/** Called when the visibility of the fader has changed. */
	virtual void OnVisibilityChange(MediaControlsFader* fader, BOOL visible) = 0;
};


/** Helper class for handling fading and visibility of media controls. */
class MediaControlsFader
	: private OpTimerListener
{
public:
	/** Constructor
	 *
	 * @param listener Fader listener
	 * @param min_opacity Minimum opacity
	 * @param max_opacity Maximum opacity
	 */
	MediaControlsFader(FaderListener* listener, UINT8 min_opacity = 0, UINT8 max_opacity = 255);

	/** Start fading in (increase opacity). */
	void FadeIn();

	/** Start fading out (decrease opacity). */
	void FadeOut();

	/** Instantly set to max opacity. */
	void Show();

	/** Hide after a delay. */
	void DelayedHide();

	// From OpTimerListener
	virtual void OnTimeOut(OpTimer* timer);

private:
	enum State { None, FadingIn, FadingOut, Hiding };

	void SetNewOpacity(int w_opacity);

	void StartTimer(unsigned timeout);
	void StopTimer();

	BOOL Done();

	OpTimer m_timer;
	FaderListener* m_listener;

	int m_min_opacity, m_max_opacity;
	int m_opacity;
	double m_lastms;

	State m_state;
	BOOL m_timer_running;
};

#endif // MEDIA_PLAYER_SUPPORT
#endif // MEDIA_FADER_H
