/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2003-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/unix/base/x11/x11_mdescreen.h"

#include "modules/libgogi/pi_impl/mde_opview.h"
#include "adjunct/quick/managers/AnimationManager.h"
#include "x11_opmessageloop.h"
#include "x11_windowwidget.h"
#include "x11_vegawindow.h"

X11MdeScreen::X11MdeScreen(int bufferWidth, int bufferHeight, int bufferStride, MDE_FORMAT bufferFormat, void* bufferData)
  : MDE_GenericScreen(bufferWidth, bufferHeight, bufferStride, bufferFormat, bufferData)
  , m_mde_validate_screen_posted(false)
  , m_in_validate(false)
  , m_oninvalid_while_in_validate(false)
  , m_last_validate_end(g_op_time_info->GetRuntimeMS())
  , m_vega_window(NULL)
  , m_mde_buffer(NULL)
{
}

X11MdeScreen::~X11MdeScreen()
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
		m_listeners.GetNext(it)->OnDeleted(this);

	if (m_mde_validate_screen_posted)
		X11OpMessageLoop::CancelLowLatencyCallbacks(this);
}

void X11MdeScreen::OnBeforeRectPaint(const MDE_RECT &rect) 
{
	if (m_mde_buffer)
		m_mde_buffer->Erase(rect);
}

void X11MdeScreen::OnValidate()
{
	// Advance animations here. We are going to paint anyway.
	if (g_animation_manager)
		g_animation_manager->advanceAnimations();
}

void X11MdeScreen::ValidateMDEScreen()
{
	OP_ASSERT(!m_in_validate);
	m_in_validate = true;
	m_oninvalid_while_in_validate = false;
	Validate(true);
	m_last_validate_end = g_op_time_info->GetRuntimeMS();
	if (m_in_validate && !m_vega_window->IsPresentInProgressFor(this))
		OnPresentComplete();
	/* m_in_validate is set to false once the back buffer
	 * (X11GlWindow) reports completion.
	 */
}

void X11MdeScreen::LowLatencyCallback(UINTPTR data)
{
	m_mde_validate_screen_posted = false;
	ValidateMDEScreen();
}

void X11MdeScreen::OnPresentComplete()
{
	m_in_validate = false;
	if (m_oninvalid_while_in_validate)
	{
		m_oninvalid_while_in_validate = false;
		PostValidateMDEScreen();
	}
}

OP_STATUS X11MdeScreen::PostValidateMDEScreen()
{
	if(m_mde_validate_screen_posted)
		return OpStatus::OK;

	if (m_in_validate)
	{
		m_oninvalid_while_in_validate = true;
		return OpStatus::OK;
	}

	m_mde_validate_screen_posted = true;

	// Make sure we don't spend all our time in Validate()
	/* I've chosen to give opera at least 5 ms to do actual work
	 * between screen updates.  There's no deep thinking behind the
	 * choice of exactly 5 ms, and no actual testing.  However, I feel
	 * this will strike a reasonable balance between giving opera
	 * processing time and delaying rendering of new frames.  Proper
	 * tuning of this value would probably be a good idea.  --eirik
	 */
	double earliest = m_last_validate_end + 5;

	m_mde_validate_screen_posted = true;
	/* If 'earliest' is in fact in the past, the callback will be
	 * triggered at the earliest possible opportunity.  Which is
	 * exactly what we want.
	 */
	return X11OpMessageLoop::PostLowLatencyCallbackAbsoluteTime(this, 0, earliest);
}

void X11MdeScreen::ScrollPixels(const MDE_RECT &rect, int dx, int dy)
{
	OP_ASSERT(ScrollPixelsSupported());
	if (!m_mde_buffer)
		return;

	m_mde_buffer->ScrollPixels(rect, dx, dy);
}

bool X11MdeScreen::ScrollPixelsSupported()
{
	if (!m_mde_buffer)
		return false;
	return m_mde_buffer->ScrollPixelsSupported();
}
