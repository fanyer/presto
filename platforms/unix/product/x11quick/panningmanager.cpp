/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#include "core/pch.h"

#include "panningmanager.h"

#include "modules/pi/OpScreenInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_unix.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/windowcommander/OpViewportController.h"

#include "platforms/unix/base/x11/inpututils.h"
#include "platforms/unix/base/x11/x11_cursor.h"
#include "platforms/unix/base/x11/x11_opdragmanager.h" // GenerateMouseMove()
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_widgetlist.h"
#include "platforms/utilix/x11_all.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/WindowCommanderProxy.h"

#define TICKS_BETWEEN_EXTRA_STEPS 2


// To be used in VisDevListners
BOOL IsPanning()
{
	return g_panning_manager && g_panning_manager->IsPanning();
}



PanningManager* g_panning_manager = 0;


// static
OP_STATUS PanningManager::Create()
{
	if (!g_panning_manager )
	{
		g_panning_manager = OP_NEW(PanningManager,());
	}
	return g_panning_manager ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

// static
void PanningManager::Destroy()
{
	OP_DELETE(g_panning_manager);
	g_panning_manager = 0;
}

// static
OP_STATUS PanningManager::Start(OpWindowCommander* commander, OpPage* page)
{
	RETURN_IF_ERROR(PanningManager::Create());	
	return g_panning_manager->Setup(commander, page);
}

// static
BOOL PanningManager::Scroll(OpWindowCommander* commander, OpInputAction::Action action, int lines)
{
	if (lines < 1 )
		return FALSE;

	if (commander->GetDocumentFullscreenState() == OpWindowCommander::FULLSCREEN_PROJECTION)
		return FALSE;

	OpViewportController* controller = commander->GetViewportController();
	if (!controller)
		return FALSE;
	
	OpRect viewport = controller->GetVisualViewport();

	int x = viewport.x;
	int y = viewport.y;

	unsigned int w=0, h=0;
	controller->GetDocumentSize(&w, &h);
	
	OpRect limit(0, 0, w, h);

	switch (action)
	{
		case OpInputAction::ACTION_SCROLL_UP:
			y -= lines;
			y = MAX(y, limit.y);
			break;

		case OpInputAction::ACTION_SCROLL_DOWN:
			y += lines;
			if (y + viewport.height > limit.height )
			{ 
				y = limit.height - viewport.height;
				y = MAX(y, limit.y);
			}
			break;

		case OpInputAction::ACTION_SCROLL_LEFT:
			x -= lines;
			x = MAX(x, limit.x);
			break;

		case OpInputAction::ACTION_SCROLL_RIGHT:
			x += lines;
			if (x + viewport.width > limit.width)
			{
				x = limit.width - viewport.width;
				x = MAX(x, limit.x);
			}
			break;

		default:
			return FALSE;
	}

	if (x != viewport.x || y != viewport.y)
	{
		controller->SetBufferedMode(TRUE);
		controller->SetVisualViewportPos(OpPoint(x, y));
		controller->SetRenderingViewportPos(OpPoint(x, y));
		controller->SetBufferedMode(FALSE);
	}

	return TRUE;
}



PanningManager::PanningManager()
	:m_timer(0)
	,m_active(FALSE)
	,m_commander(0)
	,m_active_page(0)
	,m_captured_widget(0)
	,m_vertical_step(0)
	,m_horizontal_step(0)
	,m_num_on_edge(0)
	,m_num_extra_step(0)

{
}


PanningManager::~PanningManager()
{
	Stop();
	OP_DELETE(m_timer);
}


OP_STATUS PanningManager::Setup(OpWindowCommander* commander, OpPage* page)
{
	if (m_active)
	{
		return OpStatus::OK;
	}
	
	if (!commander)
	{
		return OpStatus::ERR;
	}

	m_captured_widget = g_x11->GetWidgetList()->GetCapturedWidget();
	if (!m_captured_widget)
	{
		return OpStatus::ERR;
	}

	if (!InputUtils::GetGlobalPointerPos(m_anchor))
	{
		return OpStatus::ERR;
	}

	m_commander = commander;
	m_active_page     = page;
	m_vertical_step   = 0;
	m_horizontal_step = 0;
	m_num_on_edge     = 0;
	m_num_extra_step  = 0;

	g_application->SetToolTipListener(NULL);
	m_captured_widget->SaveCursor();

	OP_DELETE(m_timer);
	m_timer = OP_NEW(OpTimer, ());
	if (!m_timer)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	RETURN_IF_ERROR(m_captured_widget->AddX11WidgetListener(this));
	if (m_active_page)
		RETURN_IF_ERROR(m_active_page->AddListener(this));

	m_timer->SetTimerListener(this);
	m_timer->Start(15);

	m_captured_widget->Grab();

	m_active = TRUE;
	
	return OpStatus::OK;
}


void PanningManager::OnTimeOut(OpTimer* timer)
{
	if (timer == m_timer)
	{
		Exec();
		m_timer->Start(15);
	}
}

void PanningManager::Exec()
{
	OpPoint p;
	if (InputUtils::GetGlobalPointerPos(p) )
	{
		PanningManager::ComputeStep(p);

		Direction v_direction = MOVE_NOWHERE;
		Direction h_direction = MOVE_NOWHERE;

		if (m_vertical_step < 0)
		{
			v_direction = MOVE_UP;
			Scroll(m_commander, OpInputAction::ACTION_SCROLL_UP, -m_vertical_step);
		}
		else if (m_vertical_step > 0)
		{
			v_direction = MOVE_DOWN;
			Scroll(m_commander, OpInputAction::ACTION_SCROLL_DOWN, m_vertical_step);
		}

		if (g_pcunix->GetIntegerPref(PrefsCollectionUnix::PanningMode) == 1)
		{
			if (m_horizontal_step < 0)
			{
				h_direction = MOVE_LEFT;
				Scroll(m_commander, OpInputAction::ACTION_SCROLL_LEFT, -m_horizontal_step);
			}
			else if (m_horizontal_step > 0)
			{
				h_direction = MOVE_RIGHT;
				Scroll(m_commander, OpInputAction::ACTION_SCROLL_RIGHT, m_horizontal_step);
			}
		}

		if (m_captured_widget)
		{
			if (v_direction == MOVE_NOWHERE && h_direction == MOVE_NOWHERE )
				m_captured_widget->SetCursor(X11Cursor::PANNING_IDLE);
			else if (v_direction == MOVE_UP)
			{
				if (h_direction == MOVE_RIGHT)
					m_captured_widget->SetCursor(X11Cursor::PANNING_UP_RIGHT);
				else if (h_direction == MOVE_LEFT)
					m_captured_widget->SetCursor(X11Cursor::PANNING_UP_LEFT);
				else
					m_captured_widget->SetCursor(X11Cursor::PANNING_UP);
			}
			else if (v_direction == MOVE_DOWN)
			{
				if (h_direction == MOVE_RIGHT)
					m_captured_widget->SetCursor(X11Cursor::PANNING_DOWN_RIGHT);
				else if (h_direction == MOVE_LEFT)
					m_captured_widget->SetCursor(X11Cursor::PANNING_DOWN_LEFT);
				else
					m_captured_widget->SetCursor(X11Cursor::PANNING_DOWN);
			}
			else
			{
				if (h_direction == MOVE_RIGHT)
					m_captured_widget->SetCursor(X11Cursor::PANNING_RIGHT);
				else if (h_direction == MOVE_LEFT)
					m_captured_widget->SetCursor(X11Cursor::PANNING_LEFT);
			}
		}
	}
}

void PanningManager::Stop()
{
	if (m_timer)
	{
		m_timer->Stop();
	}

	if (m_captured_widget)
	{
		m_captured_widget->RemoveX11WidgetListener(this);
		m_captured_widget->RestoreCursor();
		m_captured_widget->Ungrab();
		m_captured_widget = 0;
	}

	if (m_active_page)
	{
		m_active_page->RemoveListener(this);
		m_active_page = 0;
	}

	m_active = FALSE;

}


void PanningManager::OnDeleted(X11Widget* widget)
{
	if (m_captured_widget == widget)
	{
		m_captured_widget = 0;
		Stop();
	}
}


void PanningManager::OnPageClose(OpWindowCommander* commander)
{
	if (m_active && m_commander == commander)
	{
		m_active_page = 0;
		Stop();
	}
}


BOOL PanningManager::HandleEvent(XEvent* event)
{
	if (m_active)
	{
		switch (event->type)
		{
			case KeyPress:
			case ButtonPress:
				Stop();
				X11OpDragManager::GenerateMouseMove(); // Update mouse shape 
				return TRUE;

			case MotionNotify:
				return TRUE;

		}
	}

	return FALSE;
}



void PanningManager::ComputeStep(const OpPoint& p)
{
	if (p.y < m_anchor.y-20)
		m_vertical_step = -(m_anchor.y-p.y) / 20;
	else if (p.y > m_anchor.y+20)
		m_vertical_step = (p.y-m_anchor.y) / 20;
	else
		m_vertical_step = 0;

	if (p.x < m_anchor.x-20)
		m_horizontal_step = -(m_anchor.x-p.x) / 20;
	else if (p.x > m_anchor.x+20)
		m_horizontal_step = (p.x-m_anchor.x) / 20;
	else
		m_horizontal_step = 0;


	OpScreenProperties screen_properties;
	g_op_screen_info->GetProperties(&screen_properties, 0);

	if (p.y <= 0)
	{
		if (m_num_on_edge > 0)
		{
			m_num_on_edge = 0;
			m_num_extra_step = 0;
		}
		m_num_on_edge--;
		int val = -m_num_on_edge;

		if (val % TICKS_BETWEEN_EXTRA_STEPS == 0)
		{
			m_num_extra_step = val / TICKS_BETWEEN_EXTRA_STEPS;
		}

		m_vertical_step -= m_num_extra_step;
		int max_step = screen_properties.height / 2;
		if (m_vertical_step < -max_step )
			m_vertical_step = -max_step;
	}
	else if (p.y+1 >= (int)screen_properties.height)
	{
		if (m_num_on_edge < 0)
		{
			m_num_on_edge = 0;
		    m_num_extra_step = 0;
		}

		m_num_on_edge++;
		
		int val = m_num_on_edge;
		if (val % TICKS_BETWEEN_EXTRA_STEPS == 0)
		{
			m_num_extra_step = val / TICKS_BETWEEN_EXTRA_STEPS;
		}
		m_vertical_step += m_num_extra_step;
		int max_step = screen_properties.height / 2;
		if (m_vertical_step > max_step)
			m_vertical_step = max_step;
	}
}
