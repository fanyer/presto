/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2002-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Owner: julienp
 */

#include "core/pch.h"

#ifdef FEATURE_SCROLL_MARKER

#include "modules/display/vis_dev.h"
#include "modules/pi/OpPainter.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/skin/OpSkinManager.h"
#include "OpScrollMarker.h"

OP_STATUS OpScrollMarker::Create(OpScrollMarker** scroll_marker, VisualDevice* vis_dev)
{

	//Just making sure that everything is set up correctly
	if (!scroll_marker)
		return OpStatus::ERR_NULL_POINTER;

	*scroll_marker= NULL;
	*scroll_marker = OP_NEW(OpScrollMarker, (vis_dev));

	if (!(*scroll_marker))
		return OpStatus::ERR_NO_MEMORY;

	OpTimer* timer = OP_NEW(OpTimer, ());
	if (!timer)
	{
		OP_DELETE(*scroll_marker);
		*scroll_marker= NULL;
		return OpStatus::ERR_NO_MEMORY;
	}
	timer->SetTimerListener(*scroll_marker);
	(*scroll_marker)->m_timer = timer;

	return OpStatus::OK;
}

OpScrollMarker::OpScrollMarker(VisualDevice* vis_dev)
	: m_vis_dev(vis_dev)
	, m_scroll_in_progress(FALSE)
{
	g_skin_manager->SetAnimationListener("Scroll Marker Up", this);
	g_skin_manager->SetAnimationListener("Scroll Marker Down", this);
}

OpScrollMarker::~OpScrollMarker()
{
	OP_DELETE(m_timer);
	if (m_visible)
	{
		if (m_direction > 0)
			g_skin_manager->StartAnimation(FALSE, "Scroll Marker Up", m_vis_dev);
		else
			g_skin_manager->StartAnimation(FALSE, "Scroll Marker Down", m_vis_dev);

	}
	g_skin_manager->RemoveAnimationListener("Scroll Marker Up", this);
	g_skin_manager->RemoveAnimationListener("Scroll Marker Down", this);
}

void OpScrollMarker::OnDocumentScroll(VisualDevice* vis_dev, INT32 dx, INT32 dy, SCROLL_REASON reason)
{
	//We don't want to show the scroll marker if the scrolling as done by a script
	if (reason != SCROLL_REASON_USER_INPUT || vis_dev!=m_vis_dev || (dy == 0 && !m_visible) || g_pcui->GetIntegerPref(PrefsCollectionUI::EnableScrollMarker) == 0)
		return;

#ifdef VIEWPORTS_SUPPORT
	/* FIXME: should actually use the visual viewport here, but since OnDocumentScroll()
	   is associated with the rendering viewport, that would cause problems. */

	OpRect viewport(vis_dev->GetRenderingViewport());
#else
	CoreView* view = m_vis_dev->GetView();
	if (!view)
		return;

	UINT32 doc_width;
	UINT32 doc_height;
	view->GetSize(&doc_width, &doc_height);

	UINT32 pos_x;
	UINT32 pos_y;
	m_vis_dev->GetDocumentPos(&pos_x, &pos_y);
	OpRect viewport(pos_x, pos_y, doc_width, doc_height);
#endif // VIEWPORTS_SUPPORT

	BOOL visible = m_visible;

	if (dy == 0)
	{
		m_rect.x = viewport.x;
		if (visible)
			Invalidate();

		return;
	}

	//immediately reset scroll marker when scrolling in different direction
	if (m_direction < 0 && dy > 0 || m_direction > 0 && dy < 0)
		m_scroll_in_progress = FALSE;

	if (!m_scroll_in_progress)
	{

		m_scroll_in_progress = TRUE;
		visible = FALSE;

		m_direction = 0;
		m_rect.x = viewport.x;
		m_rect.y = viewport.y+dy;
		m_rect.width = viewport.width;
		m_rect.height = viewport.height;
	}
	m_direction += dy;


	int scroll_marker_mode = g_pcui->GetIntegerPref(PrefsCollectionUI::EnableScrollMarker);

	
	if (!visible && (m_direction>0?m_direction*8:-m_direction*8) > viewport.height && (m_direction>0?m_direction*8:-m_direction*8) < (viewport.height*7))
	{
		if (scroll_marker_mode == 2 || (scroll_marker_mode == 1 && ((LONG)(viewport.y+viewport.height) == m_vis_dev->GetDocumentHeight() || viewport.y == 0)))
		{
			visible = TRUE;
		}
	}

	if (visible && (m_direction>0?m_direction*8:-m_direction*8) > (viewport.height*7))
		visible = FALSE;

	Show(visible);
	
	if (visible)
	{
		m_timer->Stop();
		m_timer->Start(OP_SCROLL_MARKER_SCROLL_TIME);
	}
	else
	{
		m_timer->Stop();
		m_timer->Start(OP_SCROLL_MARKER_INVISIBLE_SCROLL_TIME);
	}
}

void OpScrollMarker::OnTimeOut(OpTimer* timer)
{
	if (m_scroll_in_progress)
	{
		m_scroll_in_progress = FALSE;
		if (m_visible)
		{
			m_timer->Stop();
			m_timer->Start(OP_SCROLL_MARKER_PERSIST_TIME);
		}
		return;
	}

	if (!m_visible)
		return;

	Show(FALSE);
}

void OpScrollMarker::OnAnimatedImageChanged()
{
	Invalidate();
}

void OpScrollMarker::OnPaint(OpPainter* painter, OpRect rect)
{
	if (!m_visible)
		return;

	if (m_direction > 0)
		g_skin_manager->DrawElement(m_vis_dev, "Scroll Marker Up", m_rect);
	else
		g_skin_manager->DrawElement(m_vis_dev, "Scroll Marker Down", m_rect);

}

void OpScrollMarker::Show(BOOL visible)
{
	BOOL changed = (m_visible != visible);
	m_visible = visible;
	if (m_direction > 0)
		g_skin_manager->StartAnimation(visible, "Scroll Marker Up", m_vis_dev);
	else
		g_skin_manager->StartAnimation(visible, "Scroll Marker Down", m_vis_dev);

	if (changed)
		Invalidate();
}

void OpScrollMarker::Invalidate()
{
#ifdef VIEWPORTS_SUPPORT
	OpRect invalidate_rect(m_rect);

	invalidate_rect.IntersectWith(m_vis_dev->GetRenderingViewport());

	if (!invalidate_rect.IsEmpty())
		m_vis_dev->Update(invalidate_rect.x, invalidate_rect.y, invalidate_rect.width, invalidate_rect.height);
#else
	UINT32 doc_width;
	UINT32 doc_height;

	CoreView* view = m_vis_dev->GetView();
	if (!view)
		return;

	view->GetSize(&doc_width, &doc_height);

	OpRect screen_rect(0,0, doc_width, doc_height);

	UINT32 pos_x;
	UINT32 pos_y;
	m_vis_dev->GetDocumentPos(&pos_x, &pos_y);

	OpRect target_rect(m_rect.x-pos_x, m_rect.y-pos_y, m_rect.width, m_rect.height);

	if (screen_rect.Intersecting(target_rect))
	{
		screen_rect.IntersectWith(target_rect);
		view->Invalidate(screen_rect);
	}
#endif // VIEWPORTS_SUPPORT
}


#endif // FEATURE_SCROLL_MARKER

