/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "modules/display/coreview/coreviewclipper.h"
#include "modules/pi/OpPluginWindow.h"
#include "modules/dochand/win.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/layout/box/box.h"
#include "modules/layout/content/scrollable.h"

#ifdef _PLUGIN_SUPPORT_

// OpView::SetSize() takes UINT32 arguments - so don't pass negative values! Use this macro.
#define PREVENT_NEGATIVE(x) ((x > 0) ? x : 0)

// == ClipView ===============================================

ClipView::ClipView() :
	m_window(NULL),
	m_updating(FALSE),
#ifndef MOUSELESS
	m_ignore_mouse(FALSE)
#endif // MOUSELESS
{
}

ClipView::~ClipView()
{
}

OP_STATUS ClipView::Construct(const OpRect& rect, CoreView* parent)
{
	RETURN_IF_ERROR(CoreViewContainer::Construct(NULL, NULL, parent));
	m_virtual_rect = rect;
	// The default state of a plugin is to be visible, so set the visibility region to visible.
	RETURN_IF_ERROR(m_visible_region.Set(OpRect(0, 0, rect.width, rect.height)));
	m_clip_rect.Set(0, 0, rect.width, rect.height);
	return OpStatus::OK;
}

void ClipView::Update()
{
	if (m_updating)
		return;

	m_updating = TRUE;

	if (m_parent->GetContainer() == m_parent)
	{
		// The parent is a container so we don't need to clip anything since the OS does it for us.
		AffinePos virtual_pos(m_virtual_rect.x, m_virtual_rect.y);
		CoreView::SetPos(virtual_pos, FALSE, FALSE);
		CoreView::SetSize(m_virtual_rect.width, m_virtual_rect.height);
		m_opview->SetSize(PREVENT_NEGATIVE(m_virtual_rect.width), PREVENT_NEGATIVE(m_virtual_rect.height));
		m_opview->SetPos(m_virtual_rect.x, m_virtual_rect.y);
		m_window->SetPos(0, 0);
		m_window->SetSize(m_virtual_rect.width, m_virtual_rect.height);
		m_clip_rect.Set(0, 0, m_virtual_rect.width, m_virtual_rect.height);
	}
	else
		Clip();

	m_updating = FALSE;
}

void ClipView::Clip()
{
	// Calculate the visible rect.

	OpRect visible_rect = m_virtual_rect;

	// The virtual rect is relative to the nearest non-layoutbox CoreView, so we have to go through them and translate.
	CoreView *tmp_view = m_parent;
	while (tmp_view && tmp_view->GetIsLayoutBox())
	{
		OpRect tmp_view_extents = tmp_view->GetExtents();
		visible_rect.x -= tmp_view_extents.x;
		visible_rect.y -= tmp_view_extents.y;
		tmp_view = tmp_view->GetParent();
	}
	OpRect ofs_visible_rect = visible_rect;

	// Do clipping for parents up to container
	CoreView* parentcontainer = m_parent->GetContainer();
	CoreView* parent = m_parent;
	while(parent && parent != parentcontainer)
	{
		OpRect parent_extents = parent->GetExtents();
		OpRect parent_rect(0, 0, parent_extents.width, parent_extents.height);
		visible_rect.IntersectWith(parent_rect);
		visible_rect.x += parent_extents.x;
		visible_rect.y += parent_extents.y;

		// iframes don't have scrollable container as parent, so
		// special treatment is needed
		HTML_Element* helm = parent->GetOwningHtmlElement();
		if (helm && helm->IsMatchingType(HE_IFRAME, NS_HTML) && helm->GetLayoutBox())
		{
			for (HTML_Element* ancestor = helm; ancestor; ancestor = ancestor->Parent())
			{
				if (ScrollableArea* scrollable = ancestor->GetLayoutBox()->GetScrollable())
				{
					// do not clip further if iframe is positioned and current parent has overflow set
					if (!ancestor->GetLayoutBox()->GetClipAffectsTarget(helm))
						break;

					// make visible_rect be relative to the correct core view
					short px = 0;
					long  py = 0;
					CoreView* v = scrollable->GetParent();
					while (v && v != parent->GetParent())
					{
						OpRect r = v->GetExtents();
						px -= r.x;
						py -= r.y;
						v = v->GetParent();
					}
					visible_rect.x += px;
					visible_rect.y += py;

					// apply clipping
					OpRect scroll_rect = scrollable->GetExtents();
					visible_rect.IntersectWith(scroll_rect);

					// restore offset of visible_rect
					visible_rect.x -= px;
					visible_rect.y -= py;
				}
			}
		}

		parent = parent->GetParent();
	}
	m_parent->ConvertFromContainer(visible_rect.x, visible_rect.y);

	// Update clip rectangle

	m_clip_rect.Set(visible_rect);
	m_clip_rect.x -= ofs_visible_rect.x;
	m_clip_rect.y -= ofs_visible_rect.y;

	AffinePos virtual_pos(ofs_visible_rect.x, ofs_visible_rect.y);
	CoreView::SetPos(virtual_pos, FALSE, FALSE);
	CoreView::SetSize(m_virtual_rect.width, m_virtual_rect.height);

	OpRect trect(0, 0, m_virtual_rect.width, m_virtual_rect.height);
	ConvertToParentContainer(trect.x, trect.y);

	// Hide the plugin window if it should currently not be visible at all.
	if (m_clip_rect.IsEmpty())
		m_opview->SetSize(0, 0);
	else
		m_opview->SetSize(PREVENT_NEGATIVE(trect.width), PREVENT_NEGATIVE(trect.height));
	m_opview->SetPos(trect.x, trect.y);

	m_window->SetPos(0, 0);
	m_window->SetSize(m_virtual_rect.width, m_virtual_rect.height);
}

void ClipView::OnMoved()
{
	CoreView::OnMoved();
	Update();
}

void ClipView::OnResized()
{
	CoreView::OnResized();
	Update();
}

void ClipView::SetVirtualRect(const OpRect& rect)
{
	m_virtual_rect = rect;
}

#ifndef MOUSELESS

BOOL ClipView::OnMouseWheel(INT32 delta,BOOL vertical, ShiftKeyState keystate)
{
	return m_parent->MouseWheel(delta, vertical, keystate);
}

BOOL ClipView::GetHitStatus(INT32 x, INT32 y)
{
#ifndef MOUSELESS
	if (m_ignore_mouse)
		return FALSE;
#endif // MOUSELESS

	if (CoreView::GetHitStatus(x, y))
	{
		for (int i = 0; i < m_visible_region.num_rects; i++)
			if (m_visible_region.rects[i].Contains(OpPoint(x, y)))
				return TRUE;
	}
	return FALSE;
}

bool ClipView::GetHitStatus(int x, int y, OpView *view)
{
	return !!GetHitStatus(x, y);
}

#endif // !MOUSELESS

// == ClipViewEntry ===============================================

ClipViewEntry::ClipViewEntry() : m_view(NULL)
{
}

ClipViewEntry::~ClipViewEntry()
{
	OP_DELETE(m_view);
}

OP_STATUS ClipViewEntry::Construct(const OpRect& rect, CoreView* parent)
{
	m_view = OP_NEW(ClipView, ());
	if (!m_view)
		return OpStatus::ERR_NO_MEMORY;
	return m_view->Construct(rect, parent);
}

// == CoreViewClipper ===============================================

CoreViewClipper::~CoreViewClipper()
{
	OP_ASSERT(!clipviews.GetCount());
}

void CoreViewClipper::Add(ClipViewEntry* clipview)
{
	clipviews.Add(clipview);
}

void CoreViewClipper::Remove(ClipViewEntry* clipview)
{
	clipviews.RemoveByItem(clipview);
}

ClipViewEntry* CoreViewClipper::Get(OpPluginWindow* window)
{
	for (UINT32 i = 0; i < clipviews.GetCount(); ++i)
	{
		ClipViewEntry* entry = clipviews.Get(i);
		OP_ASSERT(entry);
		if (entry->GetPluginWindow() == window)
			return entry;
	}
	return NULL;
}

void CoreViewClipper::Scroll(int dx, int dy, CoreView *parent)
{
	for (UINT32 i = 0; i < clipviews.GetCount(); ++i)
	{
		ClipViewEntry* entry = clipviews.Get(i);
		OP_ASSERT(entry);
		if (!entry->GetClipView()->GetFixedPosition() && (!parent || parent == entry->GetClipView()->GetParent()))
		{
			OpRect virtual_rect = entry->GetVirtualRect();
			virtual_rect.x += dx;
			virtual_rect.y += dy;
			entry->SetVirtualRect(virtual_rect);
		}
	}
}

void CoreViewClipper::Update(CoreView *parent/* = 0*/)
{
	for (UINT32 i = 0; i < clipviews.GetCount(); ++i)
	{
		ClipViewEntry* entry = clipviews.Get(i);
		OP_ASSERT(entry);
		if (!parent || parent == entry->GetClipView()->GetParent())
			entry->Update();
	}
}

void CoreViewClipper::Hide()
{
	for (UINT32 i = 0; i < clipviews.GetCount(); ++i)
		clipviews.Get(i)->GetClipView()->SetVisibility(FALSE);
}

#endif // _PLUGIN_SUPPORT_
