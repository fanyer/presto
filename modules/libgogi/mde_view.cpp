/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
*/
#include "core/pch.h"
#include "modules/libgogi/mde.h"

#ifdef MDE_SUPPORT_VIEWSYSTEM

#ifdef VEGA_OPPAINTER_SUPPORT
#include "modules/libvega/src/oppainter/vegaoppainter.h"
#endif // VEGA_OPPAINTER_SUPPORT

#ifdef MDE_DEBUG_INFO
// Helper class to debug if the view is being deleted when it's on the stack.
class MDE_StackDebug
{
public:
	MDE_View *m_view;
	MDE_StackDebug(MDE_View *view) : m_view(view)	{ m_view->m_on_stack++; }
	~MDE_StackDebug()								{ m_view->m_on_stack--; }
};
#define MDE_DEBUG_VIEW_ON_STACK(view) MDE_StackDebug tmpdebug(view)
#else
#define MDE_DEBUG_VIEW_ON_STACK(view) ((void)0)
#endif

void MoveChildren(MDE_View *parent, int dx, int dy)
{
	MDE_View *tmp = parent->m_first_child;
	while(tmp)
	{
		tmp->m_rect.x += dx;
		tmp->m_rect.y += dy;
		tmp = tmp->m_next;
	}
}

// == MDE_Screen ==========================================================

void MDE_Screen::Init(int width, int height)
{
#ifdef MDE_SUPPORT_MOUSE
	m_captured_input = NULL;
	m_captured_button = 0;
	m_mouse_x = 0;
	m_mouse_y = 0;
#endif
#ifdef MDE_SUPPORT_TOUCH
	op_memset(&m_touch, 0, sizeof(m_touch));
#endif // MDE_SUPPORT_TOUCH
#ifdef MDE_DEBUG_INFO
	m_debug_flags = MDE_DEBUG_DEFAULT;
#endif
#ifdef MDE_SUPPORT_SPRITES
	m_first_sprite = NULL;
	m_last_sprite = NULL;
#endif
#ifdef MDE_SUPPORT_DND
	m_drag_x = 0;
	m_drag_y = 0;
	m_last_dnd_view = NULL;
	m_dnd_capture_view = NULL;
#endif
	m_screen = this;
	m_rect.w = width;
	m_rect.h = height;
	Invalidate(m_rect);
	UpdateRegion();
}

class MDE_ScrollOperation : public ListElement<MDE_ScrollOperation>
{
public:
	MDE_ScrollOperation(MDE_View* view, const MDE_RECT& rect, int dx, int dy, bool move_children)
	: m_view(view), m_rect(rect), m_dx(dx), m_dy(dy), m_move_children(move_children)
	{}
	bool Add(MDE_View* view, const MDE_RECT& rect, int dx, int dy, bool move_children)
	{
		OP_ASSERT(m_view == view);
		if (!MDE_RectIsIdentical(m_rect, rect) || m_move_children != move_children)
			return false;
		m_dx += dx;
		m_dy += dy;
		return true;
	}
	void Scroll(MDE_BUFFER* screen_buf, MDE_Region& region)
	{
		m_view->ScrollRectInternal(m_rect, m_dx, m_dy, m_move_children, screen_buf, &region, true);
	}
	void Move()
	{
		if (m_view->IsVisible())
			m_view->Invalidate(m_rect, true);

		if (m_move_children && m_view->m_first_child)
		{
			MoveChildren(m_view, m_dx, m_dy);
			m_view->UpdateRegion();
		}

		if (m_view->IsVisible())
			m_view->Invalidate(m_rect, true);
	}
	MDE_View* const m_view;
	const MDE_RECT m_rect;
	int m_dx, m_dy;
	const bool m_move_children;
};

OP_STATUS MDE_Screen::AddDeferredScroll(MDE_View* view, const MDE_RECT& rect, int dx, int dy, bool move_children)
{
	MDE_ScrollOperation* o;

	// find previous matching entry and add to that if possible
	for (o = m_deferred_scroll_operations.Last(); o; o = o->Pred())
	{
		if (o->m_view == view)
		{
			if (o->Add(view, rect, dx, dy, move_children))
				return OpStatus::OK;
			break;
		}
	}

	// no matching entry found, create new one
	o = OP_NEW(MDE_ScrollOperation, (view, rect, dx, dy, move_children));
	if (!o)
	{
		// allocation of new entry failed, move all children
		for (MDE_ScrollOperation* o = m_deferred_scroll_operations.First(); o; o = o->Suc())
			o->Move();
		m_deferred_scroll_operations.Clear();
		return OpStatus::ERR_NO_MEMORY;
	}
	o->Into(&m_deferred_scroll_operations);

	// invalidation deferred, manually inform platform it should validate
	view->SetInvalidState();

	return OpStatus::OK;
}

void MDE_Screen::AdjustRectForDeferredScroll(MDE_View* view, MDE_RECT& rect)
{
	for (MDE_ScrollOperation* o = m_deferred_scroll_operations.First(); o; o = o->Suc())
	{
		if (o->m_view == view && MDE_RectContains(o->m_rect, rect))
		{
			rect.x -= o->m_dx;
			rect.y -= o->m_dy;
			return;
		}
		else if (o->m_view == view && MDE_RectIntersects(o->m_rect, rect))
		{
			// We intersect it partly, so should we adjust it or not?
			// The only sure thing is to make a union of both.
			MDE_RECT old_rect = rect;
			rect.x -= o->m_dx;
			rect.y -= o->m_dy;
			rect = MDE_RectUnion(old_rect, rect);
			return;
		}
	}
}

void MDE_Screen::ApplyDeferredScroll(MDE_BUFFER* screen_buf, MDE_Region& region)
{
	OP_ASSERT(screen_buf);

	// Take all deferred scrolls out from the list so the Invalidations we do from 
	// scrolling isn't adjusted by AdjustRectForDeferredScroll.
	// When there is several pending (non merged) scrolls on the same view,
	// the already made invalidations will be adjusted in ScrollRect and we don't
	// want to adjust them twice.
	List<MDE_ScrollOperation> all_operations;
	all_operations.Append(&m_deferred_scroll_operations);

	while (MDE_ScrollOperation *o = all_operations.First())
	{
		o->Out();
		o->Scroll(screen_buf, region);
		OP_DELETE(o);
	}
}

void MDE_Screen::OnChildRemoved(MDE_View* child)
{
	for (MDE_ScrollOperation* o = m_deferred_scroll_operations.First(); o; )
	{
		MDE_ScrollOperation* next = o->Suc();
		// search upwards for match
		MDE_View* v = o->m_view;
		while (v && v != child)
			v = v->m_parent;
		if (v)
		{
			o->Out();
			OP_DELETE(o);
		}
		o = next;
	}
}

#ifdef MDE_DEBUG_INFO

void MDE_Screen::SetDebugInfo(int flags)
{
	m_debug_flags = flags;
	Invalidate(MDE_MakeRect(0, 0, m_rect.w, m_rect.h), true);
}

void MDE_Screen::DrawDebugRect(const MDE_RECT &rect, unsigned int col, MDE_BUFFER *dstbuf)
{
	MDE_SetColor(col, dstbuf);
	MDE_DrawRect(rect, dstbuf);
}

#endif // MDE_DEBUG_INFO

#ifdef MDE_SUPPORT_SPRITES

void MDE_Screen::AddSprite(MDE_Sprite *spr)
{
	if (spr->m_screen)
		return;

	if (m_first_sprite)
		m_first_sprite->m_prev = spr;
	spr->m_next = m_first_sprite;
	m_first_sprite = spr;
	if (!m_last_sprite)
		m_last_sprite = spr;

	spr->m_displayed = false;
	spr->m_screen = this;
	spr->Invalidate();
}

void MDE_Screen::RemoveSprite(MDE_Sprite *spr)
{
	if (spr->m_next)
		spr->m_next->m_prev = spr->m_prev;
	if (spr->m_prev)
		spr->m_prev->m_next = spr->m_next;
	if (spr == m_first_sprite)
		m_first_sprite = spr->m_next;
	if (spr == m_last_sprite)
		m_last_sprite = spr->m_prev;
	if (spr->m_displayed)
		// Invalidate screen. Could be changed to backblit the background buffer immediately.
		Invalidate(spr->m_displayed_rect, true);
	spr->m_displayed = false;
	spr->m_screen = NULL;
	spr->m_prev = NULL;
	spr->m_next = NULL;
}

void MDE_Screen::PaintSpriteInViewInternal(MDE_BUFFER *screen, MDE_Sprite *spr, MDE_View *view, bool paint_background)
{
	if (!view->IsVisible())
		return;

	// Call OnPaint for each part of the visible region of view and its children.
	if (!view->ValidateRegion())
		m_screen->OutOfMemory();

	for(int i = 0; i < view->m_region.num_rects; i++)
	{
		MDE_BUFFER sub_buffer;
		MDE_MakeSubsetBuffer(spr->m_displayed_rect, &sub_buffer, screen);

		MDE_SetClipRect(MDE_MakeRect(view->m_region.rects[i].x - spr->m_displayed_rect.x,
									view->m_region.rects[i].y - spr->m_displayed_rect.y,
									view->m_region.rects[i].w,
									view->m_region.rects[i].h), &sub_buffer);
		sub_buffer.outer_clip = sub_buffer.clip;

		if (paint_background)
			MDE_DrawBuffer(spr->m_buf, MDE_MakeRect(0, 0, spr->m_buf->w, spr->m_buf->h), 0, 0, &sub_buffer);
		else
			spr->OnPaint(MDE_MakeRect(0, 0, spr->m_buf->w, spr->m_buf->h), &sub_buffer);
	}
	// Paint sprites in childviews regions
	MDE_View *tmp = view->m_first_child;
	while(tmp)
	{
		PaintSpriteInViewInternal(screen, spr, tmp, paint_background);
		tmp = tmp->m_next;
	}
}

void MDE_Screen::PaintSprite(MDE_BUFFER* screen, MDE_Sprite* spr)
{
	MDE_BUFFER dst = *screen;
	dst.clip = dst.outer_clip;
	dst.method = MDE_METHOD_COPY;

	// Copy background
	MDE_DrawBuffer(&dst, MDE_MakeRect(0, 0, spr->m_rect.w, spr->m_rect.h), spr->m_rect.x, spr->m_rect.y, spr->m_buf);

	// Draw sprite
	if (spr->m_view)
	{
		PaintSpriteInViewInternal(&dst, spr, spr->m_view, false);
	}
	else
	{
		MDE_BUFFER sub_buffer;
		MDE_MakeSubsetBuffer(spr->m_displayed_rect, &sub_buffer, screen);
		spr->OnPaint(MDE_MakeRect(0, 0, spr->m_buf->w, spr->m_buf->h), &sub_buffer);
	}
}

void MDE_Screen::UnpaintSprite(MDE_BUFFER* screen, MDE_Sprite* spr)
{
	// Restore background
	if (spr->m_view)
		PaintSpriteInViewInternal(screen, spr, spr->m_view, true);
	else
		MDE_DrawBuffer(spr->m_buf, spr->m_displayed_rect, 0, 0, screen);
}

void MDE_Screen::DisplaySpritesInternal(MDE_BUFFER *screen, MDE_Region *update_region)
{
	if (!m_first_sprite)
		return;

	// Undisplay sprites that are displayed and invalid

	MDE_Sprite *spr = m_first_sprite;
	while (spr)
	{
		if (spr->m_displayed && spr->m_invalid)
		{
			// We have to undisplay all sprites covering this sprites rect (In reversed order)
			UndisplaySpritesInternal(screen, update_region, spr->m_displayed_rect);
		}
		spr = spr->m_next;
	}

	// Display sprites that are not displayed

	spr = m_first_sprite;
	while (spr)
	{
		if (!spr->m_displayed)
		{
			spr->m_displayed = true;
			spr->m_invalid = false;
			spr->m_displayed_rect = spr->m_rect;

			PaintSprite(screen, spr);

			MDE_RECT update_rect = MDE_RectClip(spr->m_displayed_rect, m_screen->m_rect);
			if (!MDE_RectIsEmpty(update_rect))
				if (!update_region->IncludeRect(update_rect))
				{
					m_screen->OutOfMemory();
					return;
				}
		}

		spr = spr->m_next;
	}
}

void MDE_Screen::UndisplaySpritesInternal(MDE_BUFFER *screen, MDE_Region *update_region, const MDE_RECT &rect)
{
	// Remove clipping and set COPY method on the screen
	MDE_BUFFER screen_noclip = *screen;
	screen_noclip.clip = screen_noclip.outer_clip;
	screen_noclip.method = MDE_METHOD_COPY;

	// Must undisplay sprites in reversed order to get backgrounds correct for overlapping sprites.
	MDE_Sprite *spr = m_last_sprite;
	while (spr)
	{
		UndisplaySpritesRecursiveInternal(&screen_noclip, update_region, rect, spr);
		spr = spr->m_prev;
	}
}

void MDE_Screen::UndisplaySpritesRecursiveInternal(MDE_BUFFER *screen, MDE_Region *update_region, const MDE_RECT &rect, MDE_Sprite *spr)
{
	if (spr->m_displayed && MDE_RectIntersects(spr->m_displayed_rect, rect))
	{
		// Look for sprites in front of this one that intersects. This might result in long 'chains' of sprites
		// being undisplayed just because the first sprite needed to be undisplayed.
		MDE_Sprite *tmp_spr = spr->m_next;
		while (tmp_spr)
		{
			if (tmp_spr->m_displayed)
				UndisplaySpritesRecursiveInternal(screen, update_region, spr->m_displayed_rect, tmp_spr);
			tmp_spr = tmp_spr->m_next;
		}

		spr->m_displayed = false;

		UnpaintSprite(screen, spr);

		MDE_RECT update_rect = MDE_RectClip(spr->m_displayed_rect, m_screen->m_rect);
		if (!MDE_RectIsEmpty(update_rect))
			if (!update_region->IncludeRect(update_rect))
			{
				m_screen->OutOfMemory();
				return;
			}
	}
}

// == MDE_Sprite ==========================================================

MDE_Sprite::MDE_Sprite()
	: m_buf(NULL)
	, m_screen(NULL)
	, m_view(NULL)
	, m_displayed(false)
	, m_invalid(false)
	, m_prev(NULL)
	, m_next(NULL)
	, m_hx(0)
	, m_hy(0)
{
	MDE_RectReset(m_rect);
	MDE_RectReset(m_displayed_rect);
}

MDE_Sprite::~MDE_Sprite()
{
	MDE_ASSERT(!m_screen); // Still in screen. Remove it before deleting it!
	MDE_DeleteBuffer(m_buf);
}

bool MDE_Sprite::Init(int w, int h, MDE_Screen *screen)
{
	m_buf = MDE_CreateBuffer(w, h, screen->GetFormat(), 0);
	m_rect.w = w;
	m_rect.h = h;
	return m_buf ? true : false;
}

void MDE_Sprite::SetView(MDE_View *view)
{
	m_view = view;
	Invalidate();
}

void MDE_Sprite::SetPos(int x, int y)
{
	x -= m_hx;
	y -= m_hy;
	if (x == m_rect.x && y == m_rect.y)
		return;
	m_rect.x = x;
	m_rect.y = y;
	Invalidate();
}

void MDE_Sprite::SetHotspot(int x, int y)
{
	int px = m_rect.x + m_hx;
	int py = m_rect.y + m_hy;
	m_hx = x;
	m_hy = y;
	SetPos(px, py);
}

void MDE_Sprite::Invalidate()
{
	m_invalid = true;
	if (m_screen && !m_screen->GetInvalidFlag())
	{
		m_screen->SetInvalidFlag(true);
		m_screen->OnInvalid();
	}
	// Invalidate all sprites. They might overlap and must be removed and painted in a special order again.
	if (m_screen)
	{
		MDE_Sprite *spr = m_next;
		while (spr)
		{
			spr->m_invalid = true;
			spr = spr->m_next;
		}
	}
}

#endif // MDE_SUPPORT_SPRITES

// == MDE_View ==========================================================

MDE_View::MDE_View()
	: m_activity_invalidation(ACTIVITY_INVALIDATION)
{
	MDE_RectReset(m_rect);
	MDE_RectReset(m_invalid_rect);
	m_is_visible = true;
	m_is_invalid = false;
	m_is_region_invalid = true;
	m_region_invalid_first_check = true;
	m_is_validating = false;
	m_is_transparent = false;
	m_is_fully_transparent = false;
	m_is_scrolling_transparent = false;
	m_affect_lower_regions = true;
	m_bypass_lock = false;
	m_updatelock_counter = 0;
	m_num_overlapping_transparentviews = 0;
	m_num_overlapping_scrolling_transparentviews = 0;
	m_scroll_transp_invalidate_extra = 0;
	m_scroll_invalidate_extra = 0;
	m_exclude_invalidation = false;
	m_visibility_check_needed = false;
#ifdef MDE_DEBUG_INFO
	m_color_toggle_debug = 0;
	m_on_stack = 0;
#endif
	m_parent = NULL;
	m_next = NULL;
	m_first_child = NULL;
	m_screen = NULL;
	// Doesn't matter what we set m_region_or_child_visible to. Set m_visibility_status_first_check to true
	// so the first time we calculate the visible status, we will run OnVisibilityChanged no matter what.
	m_region_or_child_visible = true;
	m_visibility_status_first_check = true;
	// Shouldn't matter what we initialize to, but be extra safe and do it.
	m_onbeforepaint_return = true;
}

MDE_View::~MDE_View()
{
	MDE_ASSERT(!m_parent); // Still in a parent. Remove the view before deleting it!
#ifdef MDE_DEBUG_INFO
	// If this assert trigs, this view is currently on the stack and there's a big chance it will crash soon!
	MDE_ASSERT(m_on_stack == 0);
#endif

	MDE_View *tmp = m_first_child;
	while(tmp)
	{
		MDE_View *next = tmp->m_next;
		tmp->m_parent = NULL;
		OP_DELETE(tmp);
		tmp = next;
	}
}

void MDE_View::SetScreenRecursive(MDE_Screen* screen)
{
	if (screen == m_screen)
		return;
	if (!screen)
		m_screen->OnChildRemoved(this);
	m_screen = screen;
	for (MDE_View* child = m_first_child; child; child = child->m_next)
		child->SetScreenRecursive(screen);
}

void MDE_View::AddChild(MDE_View *child, MDE_View *after)
{
	if (child->m_parent)
		child->m_parent->RemoveChild(child);
	if (after)
	{
		child->m_parent = this;
		child->m_next = after->m_next;
		after->m_next = child;
	}
	else
	{
		child->m_parent = this;
		child->m_next = m_first_child;
		m_first_child = child;
	}
	child->SetScreenRecursive(m_screen);

	UpdateRegion();
	child->Invalidate(MDE_MakeRect(0, 0, child->m_rect.w, child->m_rect.h), true);
	child->OnAdded();
}

void MDE_View::RemoveChildInternal(MDE_View *child, bool temporary)
{
	MDE_ASSERT(child->m_parent == this);
	MDE_ASSERT(m_first_child);

	if (!m_first_child)
		return;
	else if (m_first_child == child)
		m_first_child = child->m_next;
	else
	{
		MDE_View *tmp = m_first_child->m_next;
		MDE_View *prev = m_first_child;
		while(tmp)
		{
			if (tmp == child)
			{
				prev->m_next = child->m_next;
				break;
			}
			prev = tmp;
			tmp = tmp->m_next;
		}
	}

#ifdef MDE_SUPPORT_MOUSE
	if (!temporary && child->m_screen)
	{
		// If we are removing a mouse captured widget (or parent of it), unset the capture!
		MDE_View *tmp = child->m_screen->m_captured_input;
		while (tmp)
		{
			if (tmp == child)
			{
				child->m_screen->m_captured_input = NULL;
				break;
			}
			tmp = tmp->m_parent;
		}

#ifdef MDE_SUPPORT_TOUCH
		child->ReleaseFromTouchCapture();
#endif // MDE_SUPPORT_TOUCH
#ifdef MDE_SUPPORT_DND
		// If we are removing the last known dnd view (or parent of it), unset the view!
		tmp = child->m_screen->m_last_dnd_view;
		while (tmp)
		{
			if (tmp == child)
			{
				child->m_screen->m_last_dnd_view = NULL;
				break;
			}
			tmp = tmp->m_parent;
		}
		tmp = child->m_screen->m_dnd_capture_view;
		while (tmp)
		{
			if (tmp == child)
			{
				child->m_screen->m_dnd_capture_view = NULL;
				break;
			}
			tmp = tmp->m_parent;
		}
#endif // MDE_SUPPORT_DND
	}
#endif

	child->SetScreenRecursive(NULL);
	child->m_parent = NULL;
	child->m_next = NULL;
	child->OnRemoved();

	UpdateRegion();

	if (child->m_is_visible)
		Invalidate(child->m_rect, true);
}

void MDE_View::RemoveChild(MDE_View *child)
{
	RemoveChildInternal(child, false);
}

void MDE_View::SetRect(const MDE_RECT &rect, bool invalidate)
{
	if (rect.x != m_rect.x || rect.y != m_rect.y || rect.w != m_rect.w || rect.h != m_rect.h)
	{
		int dw = rect.w - m_rect.w;
		int dh = rect.h - m_rect.h;

		if (m_parent && m_is_visible && invalidate && !MDE_RectIsEmpty(m_rect))
			m_parent->Invalidate(m_rect, true);

		MDE_RECT old_rect = m_rect;
		m_rect = rect;

		if (IsVisible() && invalidate)
		{
			if (dw > 0)
				Invalidate(MDE_MakeRect(m_rect.w - dw, 0, dw, m_rect.h), true);
			if (dh > 0)
				Invalidate(MDE_MakeRect(0, m_rect.h - dh, m_rect.w, dh), true);
		}

		if (m_is_transparent)
			Invalidate(MDE_MakeRect(0, 0, m_rect.w, m_rect.h));

		if (m_parent && m_is_visible)
			m_parent->UpdateRegion();
		else
			UpdateRegion();

		if (old_rect.w != m_rect.w || old_rect.h != m_rect.h)
			OnResized(old_rect.w, old_rect.h);
		if (!MDE_RectIsIdentical(old_rect, m_rect))
			OnRectChanged(old_rect);
	}
}

void MDE_View::SetVisibility(bool visible)
{
	if (visible != m_is_visible)
	{
		m_is_visible = visible;
		if (!visible)
		{
			if (m_parent)
				m_parent->Invalidate(m_rect, true);
		}
		else
		{
			// If this view was invalid already, Invalidate would fail to propagate the invalid state
			// to its parents (in MDE_View::SetInvalidState). Clear it here so it will do the job.
			MDE_RectReset(m_invalid_rect);

			SetInvalidFlag(false);

			Invalidate(MDE_MakeRect(0, 0, m_rect.w, m_rect.h), true);
		}
		if (m_parent)
			m_parent->UpdateRegion();
	}
}

void MDE_View::SetZ(MDE_Z z)
{
	if (this == m_screen || !m_parent)
		return;
	MDE_View *parent = m_parent;
	MDE_View *tmp = NULL;
	switch(z)
	{
	case MDE_Z_LOWER:
		tmp = parent->m_first_child;
		if (tmp != this)
		{
			if (tmp->m_next == this)
				tmp = NULL;
			else
			{
				while (tmp->m_next->m_next != this)
					tmp = tmp->m_next;
			}
 			parent->RemoveChildInternal(this, true);
			parent->AddChild(this, tmp);
		}
		break;
	case MDE_Z_HIGHER:
		MDE_ASSERT(false); // FIX
		break;
	case MDE_Z_TOP:
		if (!m_next)
			break;
		tmp = m_next;
		while(tmp && tmp->m_next)
			tmp = tmp->m_next;
 		parent->RemoveChildInternal(this, true);
		parent->AddChild(this, tmp);
		break;
	case MDE_Z_BOTTOM:
		parent->RemoveChildInternal(this, true);
		parent->AddChild(this);
		break;
	}
}

void MDE_View::SetFullyTransparent(bool fully_transparent)
{
#ifdef MDE_SUPPORT_TRANSPARENT_VIEWS
	if (fully_transparent != m_is_fully_transparent)
	{
		m_is_fully_transparent = fully_transparent;
		if (m_screen)
			m_screen->UpdateRegion();
	}
#endif // MDE_SUPPORT_TRANSPARENT_VIEWS
}

void MDE_View::SetScrollingTransparent(bool scrolling_transparent)
{
#ifdef MDE_SUPPORT_TRANSPARENT_VIEWS
	if (scrolling_transparent != m_is_scrolling_transparent)
		m_is_scrolling_transparent = scrolling_transparent;
#endif // MDE_SUPPORT_TRANSPARENT_VIEWS
}

void MDE_View::SetTransparent(bool transparent)
{
#ifdef MDE_SUPPORT_TRANSPARENT_VIEWS
	if (transparent != m_is_transparent)
	{
		m_is_transparent = transparent;
		if (m_parent)
		{
			m_parent->Invalidate(m_rect, true);
			if (m_screen)
				m_screen->UpdateRegion();
		}
	}
#endif // MDE_SUPPORT_TRANSPARENT_VIEWS
}

void MDE_View::SetAffectLowerRegions(bool affect_lower_regions)
{
	if (affect_lower_regions != m_affect_lower_regions)
	{
		m_affect_lower_regions = affect_lower_regions;
		if (m_parent)
		{
			m_parent->Invalidate(m_rect, true);
			if (m_screen)
				m_screen->UpdateRegion();
		}
	}
}

MDE_View *MDE_View::GetTransparentParent()
{
#ifdef MDE_SUPPORT_TRANSPARENT_VIEWS
	MDE_View *tmp = m_parent;
	while(tmp)
	{
		if (tmp->m_is_transparent)
			return tmp;
		tmp = tmp->m_parent;
	}
#endif // MDE_SUPPORT_TRANSPARENT_VIEWS
	return NULL;
}

void MDE_View::Invalidate(const MDE_RECT &crect, bool include_children, bool ignore_if_hidden, bool recursing_internal, bool bypass_lock)
{
	if (m_is_transparent && m_is_fully_transparent && !m_is_scrolling_transparent)
		return;
#ifdef MDE_SUPPORT_TRANSPARENT_VIEWS
	if (m_exclude_invalidation)
		return;

	MDE_RECT rect = crect;
	if (m_screen)
		m_screen->AdjustRectForDeferredScroll(this, rect);

	// Fix: Precalculate value of has transparent parent instead of calling GetTransparentParent.
	// If this view is transparent and locked, don't invalidate the parent,
	// since this will cause this view to invalidate immediately, even though
	// it's locked. Instead invalidate the parent when the view is unlocked.
	if (!recursing_internal && m_is_visible && m_parent && ((m_is_transparent || GetTransparentParent()) && !IsLockedAndNoBypass()))
	{
		m_exclude_invalidation = true;

		MDE_RECT rect_in_parent = rect;
		rect_in_parent.x += m_rect.x;
		rect_in_parent.y += m_rect.y;
		m_parent->Invalidate(rect_in_parent, true, false, false, true);

		m_exclude_invalidation = false;
		return;
	}
#endif

	if (ignore_if_hidden && !m_is_region_invalid && m_region.num_rects == 0)
	{
		// The region is known and contains no visible parts, so we don't need to call InvalidateInternal.
	}
	else
		InvalidateInternal(rect);

	if (m_is_invalid && bypass_lock)
		m_bypass_lock = true;

	// FIX: This could be moved into SetInvalidState to optimize. We would need a m_is_invalid_self though!
	if (m_is_invalid && !recursing_internal)
	{
		OnInvalidSelf();
		MDE_View *tmp = m_parent;
		while(tmp)
		{
			tmp->OnInvalidSelf();
			tmp = tmp->m_parent;
		}
	}

	if (include_children)
	{
		MDE_View *tmp = m_first_child;
		while(tmp)
		{
			if (MDE_RectIntersects(rect, tmp->m_rect))
			{
				MDE_RECT ofsrect = rect;
				ofsrect.x -= tmp->m_rect.x;
				ofsrect.y -= tmp->m_rect.y;
				tmp->Invalidate(ofsrect, include_children, ignore_if_hidden, true, bypass_lock);
			}
			tmp = tmp->m_next;
		}
	}
}

void MDE_View::InvalidateInternal(const MDE_RECT &rect)
{
	MDE_ASSERT(!MDE_RectIsInsideOut(rect));

	if (MDE_RectIsEmpty(rect) || !m_is_visible)
		return;
	if (!MDE_RectIntersects(rect, MDE_MakeRect(0, 0, m_rect.w, m_rect.h)))
		return;

#ifndef MDE_SMALLER_AREA_MORE_ONPAINT

	// == m_invalid_rect only ======================================

	if (MDE_RectIsEmpty(m_invalid_rect))
//	if (!m_is_invalid)
		m_invalid_rect = rect;
	else
		m_invalid_rect = MDE_RectUnion(m_invalid_rect, rect);

#else // MDE_SMALLER_AREA_MORE_ONPAINT

	// == m_invalid_rect and m_invalid_region ======================

	// FIX: Check if slack is much enough?
	// FIX: if max number of rects in region is hit, we could divide it into smallest unions instead of using no region at all.

	if (MDE_RectIsEmpty(m_invalid_rect))
//	if (!m_is_invalid)
	{
		m_invalid_rect = rect;
		m_invalid_region.Set(rect);
	}
	else
	{
		m_invalid_rect = MDE_RectUnion(m_invalid_rect, rect);

		if (MDE_RectIntersects(rect, MDE_MakeRect(0, 0, m_rect.w, m_rect.h)))
		{
			MDE_RECT new_rect = rect;
			int num_rects = m_invalid_region.num_rects;
			bool need_overlap_check = false;
			for (int i = 0; i < num_rects; i++)
			{
				const int fluff = 20;
				MDE_RECT new_rect_fluff = new_rect;
				new_rect_fluff.x -= ((new_rect_fluff.w>>2) + fluff);
				new_rect_fluff.y -= ((new_rect_fluff.h>>2) + fluff);
				new_rect_fluff.w += ((new_rect_fluff.w>>2) + fluff);
				new_rect_fluff.h += ((new_rect_fluff.h>>2) + fluff);
				MDE_RECT big_rect = m_invalid_region.rects[i];
				big_rect.x -= ((big_rect.w>>2) + fluff);
				big_rect.y -= ((big_rect.h>>2) + fluff);
				big_rect.w += ((big_rect.w>>2) + fluff);
				big_rect.h += ((big_rect.h>>2) + fluff);

				if (MDE_RectIntersects(big_rect, new_rect_fluff))
				{
					MDE_RECT new_rect_union = MDE_RectUnion(m_invalid_region.rects[i], new_rect);
					int new_rect_union_area = MDE_RectUnitArea(new_rect_union);

					// If the new union area is considerably larger (adding more than 200*200px) than the 2 rectangles alone,
					// just add the new_rect to the region after removing the overlap (if possible).
					if (new_rect_union_area > 1024 &&
						new_rect_union_area - (MDE_RectUnitArea(m_invalid_region.rects[i]) + MDE_RectUnitArea(new_rect)) > 40000 &&
						MDE_RectRemoveOverlap(new_rect, m_invalid_region.rects[i]))
					{
						// MDE_RectRemoveOverlap has changed new_rect to not overlap.
					}
					else
					{
						// To prevent overlapping, we must change new_rect for the next loop and set need_overlap_check.
						new_rect = new_rect_union;
						// We know the AddRect will succeed because we remove one first.
						m_invalid_region.RemoveRect(i);
						m_invalid_region.AddRect(new_rect);
						// Make sure we don't test against this new union again
						num_rects--;
						i = -1; // Will increase to 0 by forloop.
						need_overlap_check = true;
						if (num_rects <= 0)
							break;
					}
				}
			}
			if (need_overlap_check) // a larger rect is now included in the region
			{
				// Check region for overlapping rects.
				int i, j;
				for (i = 0; i < m_invalid_region.num_rects; i++)
					for (j = 0; j < m_invalid_region.num_rects; j++)
						if (i != j && MDE_RectIntersects(m_invalid_region.rects[i], m_invalid_region.rects[j]))
						{
							m_invalid_region.rects[i] = MDE_RectUnion(m_invalid_region.rects[i], m_invalid_region.rects[j]);
							m_invalid_region.RemoveRect(j);
							i = -1; // Will increase to 0 by forloop.
							break;
						}

#ifdef _DEBUG
				for (i = 0; i < m_invalid_region.num_rects; i++)
					for (j = 0; j < m_invalid_region.num_rects; j++)
						if (i != j && MDE_RectIntersects(m_invalid_region.rects[i], m_invalid_region.rects[j]))
						{
							MDE_ASSERT(false);
						}
#endif
			}
			else // new_rect wasn't near any other rect in the region, or it was decided to be added to region anyway. Add it.
			{
				if (!m_invalid_region.num_rects)
					m_invalid_region.Set(m_invalid_rect);
				else if (!m_invalid_region.AddRect(new_rect))
					m_invalid_region.Reset(); // if we fail, m_invalid_rect will be used instead.

				// FIX: Implement smarter routine. Check area! (w>>5)*(h>>5)
				if (m_invalid_region.num_rects > 5)
					m_invalid_region.Reset(); // Assume many rects in region (many paints) is actually slower than painting full area once.
			}
		}
	}
#endif // MDE_SMALLER_AREA_MORE_ONPAINT

	SetInvalidState();
}

void MDE_View::SetInvalidState()
{
	// Update invalid flag in the view hierarchy up to root and call OnInvalid the first time.
	if (m_is_visible)
	{
		if (!m_is_invalid)
		{
			SetInvalidFlag(true);
			OnInvalid();
		}

		// Update the flag on parents.
		MDE_View *tmp = m_parent;
		while(tmp && !tmp->m_is_invalid && tmp->m_is_visible)
		{
			tmp->OnInvalid();
			tmp->SetInvalidFlag(true);
			tmp = tmp->m_parent;
		}
	}
}

bool MDE_View::ShouldNotBePainted()
{
	return !m_is_invalid || !IsVisible() || IsLockedAndNoBypass() || m_is_validating;
}

void MDE_View::Validate(bool include_children)
{
	MDE_DEBUG_VIEW_ON_STACK(this);

	// Way 1: rectangulation. several OnPaint (one for each rect in the region) to not paint over children
	// Way 2: one paint and then repaint all children that is covered by the invalid rect. Will need doublebuffer.
	// Way 3: rectangulation. Store everything in a region which is iterated in every drawprimitive.
	// Current is 1

	if (ShouldNotBePainted())
		return;

	if (m_parent && GetTransparentParent())
	{
		// Transparent views can't be Validated themself. We have to Validate the parent of the transparent view.
		// That will also validate this view.
		MDE_View *transp_parent = GetTransparentParent();
		while (transp_parent->GetTransparentParent())
			transp_parent = transp_parent->GetTransparentParent();
		if (transp_parent->m_parent)
			transp_parent->m_parent->Validate(include_children);
		return;
	}

	OnValidate();

	// Run the OnVisibilityChanged callbacks on views that wants it.
	CheckVisibilityChange(false);

	// This will call OnBeforePaint which may lead to new invalidated areas, so it is important that it
	// is done before we use m_invalid_region and m_invalid_rect in ValidateInternal.
	// It may also even cause a change to the size of the screen which replace the entire backbuffer,
	// so we must also do it before we call LockBuffer.
	BeforePaintInternal(include_children);

	MDE_Region update_region;
	MDE_BUFFER *screen = m_screen->LockBuffer();

	m_screen->ApplyDeferredScroll(screen, update_region);

	if (m_parent)
	{
		if (!GetTransparentParent())
		{
			MDE_BUFFER buf;
			m_parent->MakeSubsetOfScreen(&buf, screen);
			ValidateInternal(&buf, screen, &update_region, include_children);
		}
	}
	else
	{
		if (m_screen->UseTransparentBackground())
		{
			VEGAOpPainter* painter = m_screen->GetVegaPainter();
			painter->SetVegaTranslation(0, 0);
			painter->SetColor(0, 0, 0, 0);
			for (int i=0; i<m_invalid_region.num_rects; i++)
			{
				painter->ClearRect(OpRect(m_invalid_region.rects[i].x, m_invalid_region.rects[i].y, m_invalid_region.rects[i].w, m_invalid_region.rects[i].h));
			}
		}
		ValidateInternal(screen, screen, &update_region, include_children);
	}

#ifdef MDE_SUPPORT_SPRITES
	m_screen->DisplaySpritesInternal(screen, &update_region);
#endif // MDE_SUPPORT_SPRITES

	m_screen->UnlockBuffer(&update_region);
}

void MDE_View::BeforePaintInternal(bool include_children)
{
	MDE_DEBUG_VIEW_ON_STACK(this);

	m_onbeforepaint_return = false;

	// views which have a transparent parent doesn't seem to be
	// invalidated, always call OnBeforePaintEx for them
	BOOL bypass_invalid_check = (m_is_transparent || GetTransparentParent())
					&& !IsLockedAndNoBypass()
					&& !m_is_validating
					&& m_is_visible;

	if (!bypass_invalid_check && ShouldNotBePainted())
		return;

	if (!MDE_RectIsEmpty(m_invalid_rect) || bypass_invalid_check)
	{
		// Make sure this views region is updated before we use it
		if (!ValidateRegion())
		{
			m_screen->OutOfMemory();
			return;
		}

		if (m_region.num_rects)
		{
			m_is_validating = true;
			m_onbeforepaint_return = OnBeforePaintEx();
			m_is_validating = false;

			if (!m_onbeforepaint_return)
			{
				SetInvalidState();
				return;
			}
		}
	}

	// Recursively call BeforePaintInternal on children
	if (include_children)
	{
		m_onbeforepaint_return = true;

		m_is_validating = true;
		MDE_View *tmp = m_first_child;
		while(tmp)
		{
			tmp->BeforePaintInternal(include_children);
			tmp = tmp->m_next;
		}
		m_is_validating = false;
	}
}

void MDE_View::ValidateInternal(MDE_BUFFER *buf, MDE_BUFFER *screen, MDE_Region *update_region, bool include_children)
{
	MDE_DEBUG_VIEW_ON_STACK(this);

	if (ShouldNotBePainted())
		return;
	if (!m_onbeforepaint_return)
		return;

#ifdef MDE_SUPPORT_TRANSPARENT_VIEWS
	if (m_is_transparent)
	{
		SetInvalidFlag(false);
		return;
	}
#endif
	m_bypass_lock = false;

	if (include_children)
	{
		SetInvalidFlag(false);
	}

	if (!MDE_RectIntersects(buf->outer_clip, m_rect))
	{
		m_invalid_region.Reset();
		MDE_RectReset(m_invalid_rect);
		return;
	}

	m_is_validating = true;

#ifdef MDE_DEBUG_INFO
	MDE_RECT debug_invalid_rect = m_invalid_rect;
	MDE_Region debug_invalid_region;
#endif

	MDE_BUFFER sub_buffer;
	MDE_MakeSubsetBuffer(m_rect, &sub_buffer, buf);

	if (!MDE_RectIsEmpty(m_invalid_rect))
	{
		// Make sure this views region is updated before we use it.
		if (!ValidateRegion())
		{
			m_screen->OutOfMemory();
			m_is_validating = false;
			return;
		}

#ifdef MDE_DEBUG_INFO
		debug_invalid_rect = m_invalid_rect;
		if (m_color_toggle_debug)
			m_color_toggle_debug = 0;
		else
			m_color_toggle_debug = 192;
		if ((m_screen->m_debug_flags & MDE_DEBUG_UPDATE_REGION) ||
			(m_screen->m_debug_flags & MDE_DEBUG_INVALIDATE_RECT))
		{
			m_invalid_rect = MDE_MakeRect(0, 0, m_rect.w, m_rect.h);
			debug_invalid_region.Swap(&m_invalid_region);
		}
#endif

		// Will reset m_invalid_region but keep the contents in region.
		MDE_Region region;
		region.Swap(&m_invalid_region);
		// Reset m_invalid_rect and keep the contents in invalid.
		MDE_RECT invalid = m_invalid_rect;
		MDE_RectReset(m_invalid_rect);

		// Get this views position on the screen here, so we only have to do it once.
		int pos_on_screen_x = 0, pos_on_screen_y = 0;
		ConvertToScreen(pos_on_screen_x, pos_on_screen_y);

		MDE_RECT *rects = region.rects;
		int n, num_rects = region.num_rects;
		if (!num_rects)
		{
			num_rects = 1;
			rects = &invalid;
		}

		// For all invalid region rects, we will check and clip to the views region if and what we need to paint.
		for (n = 0; n < num_rects; n++)
		{
			invalid = MDE_RectClip(rects[n], MDE_MakeRect(0, 0, m_rect.w, m_rect.h));

			// Include the damaged area in the update_region.
			MDE_RECT damaged_rect = invalid;
			damaged_rect.x += pos_on_screen_x;
			damaged_rect.y += pos_on_screen_y;

			damaged_rect = MDE_RectClip(damaged_rect, m_screen->m_rect);

			if (!MDE_RectIsEmpty(damaged_rect))
			{
#ifdef MDE_SUPPORT_SPRITES
				if (m_region.num_rects)
					m_screen->UndisplaySpritesInternal(screen, update_region, damaged_rect);
#endif

				for(int i = 0; i < m_region.num_rects; i++)
				{
					MDE_RECT r = m_region.rects[i];
					r.x -= pos_on_screen_x;
					r.y -= pos_on_screen_y;
					if (!MDE_RectIsEmpty(m_region.rects[i]) && MDE_RectIntersects(r, invalid))
					{
						r = MDE_RectClip(invalid, r);

						MDE_SetClipRect(r, &sub_buffer);
						if (!MDE_RectIsEmpty(sub_buffer.clip))
						{
							MDE_RECT painted_rect = sub_buffer.clip;

							m_screen->OnBeforeRectPaint(MDE_MakeRect(painted_rect.x + pos_on_screen_x,
																	 painted_rect.y + pos_on_screen_y,
																	 painted_rect.w, painted_rect.h));

							bool painted = PaintInternal(sub_buffer, pos_on_screen_x, pos_on_screen_y);

#ifdef MDE_DEBUG_INFO
							if (m_screen->m_debug_flags & MDE_DEBUG_UPDATE_REGION)
							{
								m_screen->DrawDebugRect(MDE_MakeRect(m_region.rects[i].x - pos_on_screen_x,
																	m_region.rects[i].y  - pos_on_screen_y,
																	m_region.rects[i].w, m_region.rects[i].h), MDE_RGB(0,200,0), &sub_buffer);
							}
							if (m_screen->m_debug_flags & MDE_DEBUG_INVALIDATE_RECT)
							{
								unsigned int col = MDE_RGB(255,m_color_toggle_debug,64);
								m_screen->DrawDebugRect(debug_invalid_rect, col, &sub_buffer);
								if (debug_invalid_region.num_rects <= 1)
									m_screen->DrawDebugRect(MDE_MakeRect(	debug_invalid_rect.x + 1, debug_invalid_rect.y + 1, debug_invalid_rect.w - 2, debug_invalid_rect.h - 2), col, &sub_buffer);
								col = MDE_RGB(128,m_color_toggle_debug,255);
								if (debug_invalid_region.num_rects > 1)
									for(int d = 0; d < debug_invalid_region.num_rects; d++)
									{
										m_screen->DrawDebugRect(debug_invalid_region.rects[d], col, &sub_buffer);
										m_screen->DrawDebugRect(MDE_MakeRect(debug_invalid_region.rects[d].x + 1, debug_invalid_region.rects[d].y + 1, debug_invalid_region.rects[d].w - 2,debug_invalid_region.rects[d].h - 2), col, &sub_buffer);
									}
							}
#endif // MDE_DEBUG_INFO

#ifdef MDE_SUPPORT_TRANSPARENT_VIEWS
#ifdef _DEBUG
							// Check views with overlapping transparentviews
						/*						if (m_num_overlapping_transparentviews)
							{
								MDE_SetColor(MDE_RGB(100,255,0), &sub_buffer);
								MDE_DrawRectFill(MDE_MakeRect(2, 2, 5, 5), &sub_buffer);
							}*/
#endif
#endif
							m_screen->OnRectPainted(MDE_MakeRect(painted_rect.x + pos_on_screen_x,
																	painted_rect.y + pos_on_screen_y,
																	painted_rect.w, painted_rect.h));

							if (painted)
							{
								// Include the painted rect in the update_region that should be flushed to screen.
								if (!update_region->IncludeRect(MDE_MakeRect(painted_rect.x + pos_on_screen_x,
																			painted_rect.y + pos_on_screen_y,
																			painted_rect.w, painted_rect.h)))
								{
									m_screen->OutOfMemory();
									m_is_validating = false;
									return;
								}
							}
						}
					}
				}
			}
		}
	}

	// Validate children
	if (include_children)
	{
		MDE_View *tmp = m_first_child;
		while(tmp)
		{
			tmp->ValidateInternal(&sub_buffer, screen, update_region, include_children);
			tmp = tmp->m_next;
		}
	}

	m_is_validating = false;
}

bool MDE_View::PaintInternal(MDE_BUFFER sub_buffer, int pos_on_screen_x, int pos_on_screen_y)
{
	MDE_DEBUG_VIEW_ON_STACK(this);

	sub_buffer.outer_clip = MDE_RectClip(sub_buffer.outer_clip, sub_buffer.clip);

	MDE_RECT stackrect = sub_buffer.clip;
	if (!OnPaintEx(stackrect, &sub_buffer))
		return false;

#ifdef MDE_SUPPORT_TRANSPARENT_VIEWS
	// FIX: m_num_overlapping_transparentviews should not count views over a nonvisible but overlapping part.
	if (m_num_overlapping_transparentviews > 0 || m_num_overlapping_scrolling_transparentviews > 0)
	{
		// Paint all transparent views that is child of this
		MDE_View *tmp = m_first_child;
		while(tmp)
		{
			if (tmp != this && tmp->m_is_transparent && tmp->IsVisible())
			{
				MDE_RECT child_rect = tmp->m_rect;
				MDE_BUFFER child_buffer;
				MDE_MakeSubsetBuffer(child_rect, &child_buffer, &sub_buffer);

#ifdef DEBUG_GFX
				if (MDE_RectIsEmpty(sub_buffer.clip))
					int stop = 0;
#endif
				tmp->PaintAllChildrenInternal(child_buffer, pos_on_screen_x + tmp->m_rect.x, pos_on_screen_y + tmp->m_rect.y);
			}
			tmp = tmp->m_next;
		}
		// Paint all other transparent views that cover this
		MDE_View *tmpparent = m_parent, *tmpstart = this;
		while(tmpparent)
		{
			int tmp_x = 0, tmp_y = 0;
			tmpparent->ConvertToScreen(tmp_x, tmp_y);

			tmp = tmpstart->m_next;

			while(tmp)
			{
				if (tmp->m_is_visible && !MDE_RectIsEmpty(tmp->m_rect))
				{
					if (tmp->m_is_transparent)
					{
						MDE_RECT child_rect = tmp->m_rect;
						child_rect.x = child_rect.x + tmp_x - pos_on_screen_x;
						child_rect.y = child_rect.y + tmp_y - pos_on_screen_y;
						MDE_BUFFER child_buffer;
						MDE_MakeSubsetBuffer(child_rect, &child_buffer, &sub_buffer);

#ifdef DEBUG_GFX
						if (MDE_RectIsEmpty(sub_buffer.clip))
							int stop = 0;
#endif
						tmp->PaintAllChildrenInternal(child_buffer, pos_on_screen_x + tmp->m_rect.x, pos_on_screen_y + tmp->m_rect.y);
					}
				}
				tmp = tmp->m_next;
			}
			tmpstart = tmpparent;
			tmpparent = tmpparent->m_parent;
		}

	}
#endif // MDE_SUPPORT_TRANSPARENT_VIEWS
	return true;
}

void MDE_View::PaintAllChildrenInternal(MDE_BUFFER sub_buffer, int pos_on_screen_x, int pos_on_screen_y)
{
#ifdef MDE_SUPPORT_TRANSPARENT_VIEWS
	if (MDE_RectIsEmpty(sub_buffer.clip))
		return;

	sub_buffer.outer_clip = MDE_RectClip(sub_buffer.outer_clip, sub_buffer.clip);

	MDE_RECT stackrect = sub_buffer.clip;
	OnPaintEx(stackrect, &sub_buffer);

	MDE_View *tmp = m_first_child;
	while(tmp)
	{
		if (tmp != this && tmp->IsVisible())
		{
			MDE_RECT child_rect = tmp->m_rect;
			MDE_BUFFER child_buffer;
			MDE_MakeSubsetBuffer(child_rect, &child_buffer, &sub_buffer);

#ifdef DEBUG_GFX
			if (MDE_RectIsEmpty(sub_buffer.clip))
				int stop = 0;
#endif
			tmp->PaintAllChildrenInternal(child_buffer, pos_on_screen_x + tmp->m_rect.x, pos_on_screen_y + tmp->m_rect.y);
		}
		tmp = tmp->m_next;
	}
#else

MDE_ASSERT(0); // We assume nothing is calling PaintAllChildrenInternal except transparency
               // support to save footprint. If called, enable the code above.

#endif // MDE_SUPPORT_TRANSPARENT_VIEWS
}

#ifdef MDE_NO_MEM_PAINT

void MDE_View::ValidateOOM(bool include_children)
{
	// This is way 2 described in Validate
	if (ShouldNotBePainted())
		return;

	OnValidate();

	MDE_RECT invalid;
	MDE_BUFFER *screen = m_screen->LockBuffer();

	MDE_RectReset(invalid);
	MaxInvalidRect(invalid);

#ifdef MDE_SUPPORT_SPRITES
	MDE_Region update_region_sprites;
	m_screen->UndisplaySpritesInternal(screen, &update_region_sprites, invalid);
#endif

	m_is_validating = true;

	if (m_parent)
	{
		MDE_BUFFER buf;
		m_parent->MakeSubsetOfScreen(&buf, screen);
		m_parent->ConvertFromScreen(invalid.x, invalid.y);
		ValidateOOMInternal(&buf, invalid, include_children);
	}
	else
	{
		ValidateOOMInternal(screen, invalid, include_children);
	}

	m_is_validating = false;

#ifdef MDE_SUPPORT_SPRITES
	m_screen->DisplaySpritesInternal(screen, &update_region_sprites);
#endif

	MDE_Region update_region;
	update_region.rects = &invalid;
	update_region.num_rects = 1;
	m_screen->UnlockBuffer(&update_region);
	update_region.rects = NULL;
	update_region.num_rects = 0;
}

void MDE_View::MaxInvalidRect(MDE_RECT& irect)
{
	if (!m_is_invalid || !IsVisible())
		return;
	MDE_RECT inv = m_invalid_rect;
	inv = MDE_RectClip(inv, MDE_MakeRect(0, 0, m_rect.w, m_rect.h));
	if (m_parent)
		inv = MDE_RectClip(inv, MDE_MakeRect(-m_rect.x, -m_rect.y, m_parent->m_rect.w, m_parent->m_rect.h));
	ConvertToScreen(inv.x, inv.y);
	irect = MDE_RectUnion(inv, irect);
	MDE_View *tmp = m_first_child;
	while(tmp)
	{
		tmp->MaxInvalidRect(irect);
		tmp = tmp->m_next;
	}
}

void MDE_View::ValidateOOMInternal(MDE_BUFFER *buf, const MDE_RECT& invalid, bool include_children)
{
	if (!IsVisible() || IsLockedAndNoBypass())
		return;

	if (include_children)
	{
		SetInvalidFlag(false);
	}

	m_bypass_lock = false;

	if (!MDE_RectIntersects(buf->outer_clip, m_rect))
	{
		MDE_RectReset(m_invalid_rect);
		return;
	}

	MDE_BUFFER sub_buffer;
	MDE_MakeSubsetBuffer(m_rect, &sub_buffer, buf);

	MDE_RECT inv = invalid;
	inv.x -= m_rect.x;
	inv.y -= m_rect.y;
	inv = MDE_RectClip(inv, MDE_MakeRect(0, 0, m_rect.w, m_rect.h));

	MDE_RectReset(m_invalid_rect);
	m_invalid_region.Reset();

	if (!MDE_RectIsEmpty(inv))
	{
		MDE_SetClipRect(inv, &sub_buffer);

		if (!MDE_RectIsEmpty(sub_buffer.clip))
		{
			// A little hacky.. Will change if regionclipping is moved into drawprimitives.
			MDE_RECT old = sub_buffer.outer_clip;
			sub_buffer.outer_clip = MDE_RectClip(sub_buffer.outer_clip, sub_buffer.clip);

			MDE_RECT stackrect = sub_buffer.clip;
			OnPaintEx(stackrect, &sub_buffer);

			// FIX: Somehow call OnRectPainted after all views has been painted.
			//m_screen->OnRectPainted(MDE_MakeRect(stackrect.x + pos_on_screen_x, stackrect.y + pos_on_screen_y, stackrect.w, stackrect.h));
		}
	}

	// Validate children
	if (include_children)
	{
		MDE_View *tmp = m_first_child;
		while(tmp)
		{
			tmp->ValidateOOMInternal(&sub_buffer, inv, include_children);
			tmp = tmp->m_next;
		}
	}
}
#endif // MDE_NO_MEM_PAINT

void MDE_View::UpdateRegion(bool include_children)
{
	// Call the OnRegionInvalid callback if it's the first time since
	// it was valid, or first time ever.
	if (!m_is_region_invalid || m_region_invalid_first_check)
	{
		m_is_region_invalid = true;
		m_region_invalid_first_check = false;

		OnRegionInvalid();
	}

	// Make sure we re-check visibility later
	if (!m_visibility_check_needed && ThisOrParentWantsVisibilityChange())
		m_visibility_check_needed = true;
	// Update the flag on parents too if needed. This must be done separetely because we might
	// get here even if we don't set it above (If the view was just added).
	if (m_visibility_check_needed)
	{
		MDE_View *tmp = m_parent;
		while (tmp && !tmp->m_visibility_check_needed)
		{
			tmp->m_visibility_check_needed = true;
			tmp = tmp->m_parent;
		}
	}

	if (include_children)
	{
		// Update children regions
		MDE_View *tmp = m_first_child;
		while(tmp)
		{
			tmp->UpdateRegion();
			tmp = tmp->m_next;
		}
	}
}

void MDE_View::SetBusy()
{
	m_activity_invalidation.Begin();
}

void MDE_View::SetIdle()
{
	m_activity_invalidation.End();
}

bool MDE_View::ThisOrParentWantsVisibilityChange()
{
	MDE_View *tmp = this;
	while (tmp)
	{
		if (tmp->GetOnVisibilityChangeWanted())
			return true;
		tmp = tmp->m_parent;
	}
	return false;
}

void MDE_View::CheckVisibilityChange(bool force_check)
{
	MDE_DEBUG_VIEW_ON_STACK(this);

	if (!m_visibility_check_needed && !force_check)
		return;

	m_visibility_check_needed = false;

	// First iterate through children so we can be sure those regions are up to date.
	// Force further iteration for all children of views that wants the callback.
	MDE_View *tmp = m_first_child;
	while(tmp)
	{
		tmp->CheckVisibilityChange(GetOnVisibilityChangeWanted() || force_check);
		tmp = tmp->m_next;
	}

	if (ThisOrParentWantsVisibilityChange())
	{
		// Now check if this view or any child view has visible parts
		// All child views should be updated at this point.

		ValidateRegion();

		bool vis = m_region.num_rects ? true : false;
		for (MDE_View* v = m_first_child; v && !vis; v = v->m_next)
			vis = v->m_region_or_child_visible;

		// Do the call if it changes or if it's the first time
		if (vis != m_region_or_child_visible || m_visibility_status_first_check)
		{
			m_visibility_status_first_check = false;
			m_region_or_child_visible = vis;
			OnVisibilityChanged(vis);
		}
	}
}

void MDE_View::ValidateRegionDone()
{
	m_is_region_invalid = false;
}

bool MDE_View::AddToOverlapRegion(MDE_Region &overlapping_region, int parent_screen_x, int parent_screen_y)
{
	MDE_ASSERT(!m_is_transparent);
	MDE_RECT r = m_rect;
	r.x += parent_screen_x;
	r.y += parent_screen_y;
	if (m_custom_overlap_region.num_rects)
	{
		// The view has a custom clipping region so add just the parts actually visible.
		int i;
		MDE_Region rgn;
		if (!rgn.Set(r))
			return false;
		for(i = 0; i < m_custom_overlap_region.num_rects; i++)
		{
			MDE_RECT r = m_custom_overlap_region.rects[i];
			r.x += m_rect.x + parent_screen_x;
			r.y += m_rect.y + parent_screen_y;
			if (!rgn.ExcludeRect(r))
				return false;
		}
		for(i = 0; i < rgn.num_rects; i++)
		{
			bool ret = overlapping_region.AddRect(rgn.rects[i]);
			if (!ret)
				return false;
		}
	}
	else
	{
		// The view doesn't have a custom clipping region so just add the entire view.
		bool ret = overlapping_region.AddRect(r);
		if (!ret)
			return false;
	}
	return true;
}

bool MDE_View::ValidateRegion(bool include_children)
{
	int i, j;

	if (!m_is_region_invalid)
		return true;

	m_region.Reset(false);
	if (MDE_RectIsEmpty(m_rect) || !IsVisible())
	{
		ValidateRegionDone();
		return true;
	}

	MDE_RECT r = m_rect;
	if (m_parent)
	{
		// Clip inside parents and convert to screen
		MDE_View* tmpp = m_parent;
		while (tmpp)
		{
			r = MDE_RectClip(r, MDE_MakeRect(0, 0, tmpp->m_rect.w, tmpp->m_rect.h));
			r.x += tmpp->m_rect.x;
			r.y += tmpp->m_rect.y;
			tmpp = tmpp->m_parent;
		}
		if (MDE_RectIsEmpty(r))
		{
#ifdef MDE_SUPPORT_TRANSPARENT_VIEWS
			m_num_overlapping_transparentviews = 0;
			m_num_overlapping_scrolling_transparentviews = 0;
			m_scroll_invalidate_extra = 0;
#endif
			ValidateRegionDone();
			return true;
		}
	}

	bool ret = m_region.Set(r);
	if (!ret)
		return false;

	MDE_Region overlapping_region;

#ifdef MDE_SUPPORT_TRANSPARENT_VIEWS
	m_num_overlapping_transparentviews = 0;
	m_num_overlapping_scrolling_transparentviews = 0;
	m_scroll_invalidate_extra = 0;
#endif

	// Iterate through all children in all parents of this. Add each views rect to overlapping_region.
	MDE_View *tmp, *tmpparent = m_parent, *tmpstart = this;
	while(tmpparent)
	{
		int tmp_x = 0, tmp_y = 0;
		tmpparent->ConvertToScreen(tmp_x, tmp_y);

		tmp = tmpstart->m_next;

		while(tmp)
		{
			if (tmp->m_is_visible && !MDE_RectIsEmpty(tmp->m_rect) && tmp->m_affect_lower_regions)
			{
				if (!tmp->m_is_transparent)
				{
					// We have a solid view that is overlapping. Add it to the overlapping_region.
					if (!tmp->AddToOverlapRegion(overlapping_region, tmp_x, tmp_y))
						return false;
				}
#ifdef MDE_SUPPORT_TRANSPARENT_VIEWS
				else if (MDE_RectIntersects(r, MDE_MakeRect(tmp_x + tmp->m_rect.x, tmp_y + tmp->m_rect.y, tmp->m_rect.w, tmp->m_rect.h)))
				{
					if (!tmp->m_is_fully_transparent)
						m_num_overlapping_transparentviews++;
					if (tmp->m_is_scrolling_transparent)
					{
						m_num_overlapping_scrolling_transparentviews++;
						m_scroll_invalidate_extra = MAX(m_scroll_invalidate_extra, tmp->m_scroll_transp_invalidate_extra);
					}
				}
#endif
			}
			tmp = tmp->m_next;
		}
		tmpstart = tmpparent;
		tmpparent = tmpparent->m_parent;
	}

	int tmp_x = 0, tmp_y = 0;
	ConvertToScreen(tmp_x, tmp_y);

	// Iterate through all children of this element.
	if (include_children)
	{
		tmp = m_first_child;
		while(tmp)
		{
			if (tmp->m_is_visible && !MDE_RectIsEmpty(tmp->m_rect) && tmp->m_affect_lower_regions)
			{
				if (!tmp->m_is_transparent)
				{
					if (!tmp->AddToOverlapRegion(overlapping_region, tmp_x, tmp_y))
						return false;
				}
#ifdef MDE_SUPPORT_TRANSPARENT_VIEWS
				else if (MDE_RectIntersects(r, MDE_MakeRect(tmp_x + tmp->m_rect.x, tmp_y + tmp->m_rect.y, tmp->m_rect.w, tmp->m_rect.h)))
				{
					if (!tmp->m_is_fully_transparent)
						m_num_overlapping_transparentviews++;
					if (tmp->m_is_scrolling_transparent)
					{
						m_num_overlapping_scrolling_transparentviews++;
						m_scroll_invalidate_extra = MAX(m_scroll_invalidate_extra, tmp->m_scroll_transp_invalidate_extra);
					}
				}
#endif
			}
			tmp = tmp->m_next;
		}
	}

	// If we have a custom clip region, we should add the nonvisible parts to
	// the overlap region so we don't paint them. We must do this for all parents with
	// custom clip regions.
	tmp = this;
	while(tmp)
	{
		if (tmp->m_custom_overlap_region.num_rects)
		{
			int tmp_x = 0, tmp_y = 0;
			tmp->ConvertToScreen(tmp_x, tmp_y);

			for(i = 0; i < tmp->m_custom_overlap_region.num_rects; i++)
			{
				MDE_RECT r = tmp->m_custom_overlap_region.rects[i];
				r.x += tmp_x;
				r.y += tmp_y;
				bool ret = overlapping_region.AddRect(r);
				if (!ret)
					return false;
			}
		}
		tmp = tmp->m_parent;
	}

	// Split all intersecting rectangles
	for(j = 0; j < overlapping_region.num_rects; j++)
	{
		int num_to_split = m_region.num_rects;
		for(i = 0; i < num_to_split; i++)
		{
			MDE_ASSERT(!MDE_RectIsEmpty(m_region.rects[i]));
			MDE_ASSERT(!MDE_RectIsEmpty(overlapping_region.rects[j]));
			if (!MDE_RectIsEmpty(m_region.rects[i]) && MDE_RectIntersects(m_region.rects[i], overlapping_region.rects[j]))
			{
				bool ret = m_region.ExcludeRect(m_region.rects[i], overlapping_region.rects[j]);
				if (!ret)
					return false;
				m_region.RemoveRect(i);
				num_to_split--;
				i--;
			}
		}
	}

	// Melt together rectangles
	m_region.CoalesceRects();

	ValidateRegionDone();
	return true;
}

void MDE_View::SetCustomOverlapRegion(MDE_Region *rgn)
{
	if (rgn->num_rects == m_custom_overlap_region.num_rects)
	{
		// If regions are equal, just return.
		int i;
		for (i = 0; i < rgn->num_rects; i++)
			if (!MDE_RectIsIdentical(m_custom_overlap_region.rects[i], rgn->rects[i]))
				break;
		if (i == rgn->num_rects)
			return;
	}

	m_custom_overlap_region.Swap(rgn);
	UpdateRegion();
}

void MDE_View::ConvertToScreen(int &x, int &y)
{
	MDE_View *tmp = this;
	while(tmp->m_parent)
	{
		x += tmp->m_rect.x;
		y += tmp->m_rect.y;
		tmp = tmp->m_parent;
	}
}

void MDE_View::ConvertFromScreen(int &x, int &y)
{
	MDE_View *tmp = this;
	while(tmp->m_parent)
	{
		x -= tmp->m_rect.x;
		y -= tmp->m_rect.y;
		tmp = tmp->m_parent;
	}
}

void MDE_View::LockUpdate(bool lock)
{
	if (lock)
		m_updatelock_counter++;
	else
	{
		MDE_ASSERT(m_updatelock_counter > 0);
		m_updatelock_counter--;

		// This view have been locked and might need painting but the parents might have been painted
		// and ignored this view (while locked). We must update the invalid state of the parents.
		if (m_updatelock_counter == 0 && m_is_invalid && m_parent)
		{
			// If we are transparent and got any invalidates while we were
			// locked, invalidating those parts on the parent have been
			// postponed. Make sure that they are done now.
			if (m_is_transparent || GetTransparentParent())
			{
				MDE_RECT rect_in_parent = m_invalid_rect;
				rect_in_parent.x += m_rect.x;
				rect_in_parent.y += m_rect.y;
				m_parent->Invalidate(rect_in_parent, true);
			}
			m_parent->SetInvalidState();
		}
	}
}

bool MDE_View::IsLockedAndNoBypass()
{
	if (m_updatelock_counter <= 0 || m_bypass_lock)
		return false;
	return true;
}

bool MDE_View::IsVisible()
{
	MDE_View *tmp = this;
	while(tmp)
	{
		if (!tmp->m_is_visible)
			return false;
		tmp = tmp->m_parent;
	}
	return true;
}

void MDE_View::ScrollRect(const MDE_RECT &rect, int dx, int dy, bool move_children)
{
	if (!m_screen || MDE_RectIsEmpty(rect))
		return;

#ifdef MDE_FULLUPDATE_SCROLL
	bool full_update = true;
#else
	bool full_update = !GetScrollMoveAllowed();
#endif

	// In some cases we should just invalidate and move children instead of scrolling.
	if (!full_update)
		full_update =	m_updatelock_counter > 0 ||
						MDE_ABS(dx) >= rect.w ||
						MDE_ABS(dy) >= rect.h ||
						!IsVisible() || !MDE_UseMoveRect() ||
						m_is_transparent ||
						GetTransparentParent();

	if (!full_update)
	{
		if (OpStatus::IsSuccess(m_screen->AddDeferredScroll(this, rect, dx, dy, move_children)))
		{
			// Move children immediately since core might be confused if they aren't moved.
			// When applying the deferred scroll, we'll not move them again but still have to know
			// about if they moved (So the deferred scroll should still get true for move_children).
			if (move_children && m_first_child)
			{
				MoveChildren(this, dx, dy);
				UpdateRegion();
			}
			return;
		}
		// OOM when appending this operation, invalidate everything
		full_update = true;
	}

	if (full_update)
	{
		if (IsVisible())
			Invalidate(rect, true);

		if (move_children && m_first_child)
		{
			MoveChildren(this, dx, dy);
			UpdateRegion();
		}

		if (IsVisible())
			Invalidate(rect, true);
		return;
	}

	ScrollRectInternal(rect, dx, dy, move_children);
}

void MDE_View::ScrollRectInternal(const MDE_RECT& rect, int dx, int dy, bool move_children,
                                  MDE_BUFFER* const passed_screen_buf/* = 0*/, MDE_Region* passed_update_region/* = 0*/,
								  bool is_deferred/* = false*/)
{
	OP_ASSERT(!!passed_screen_buf == !!passed_update_region);

#ifndef MDE_FULLUPDATE_SCROLL
	bool ret = ValidateRegion();
	if (!ret)
	{
		Invalidate(rect);
		m_screen->OutOfMemory();
		return;
	}

#ifdef MDE_SUPPORT_TRANSPARENT_VIEWS
	if (m_num_overlapping_transparentviews > 0)
	{
		Invalidate(rect, true);
		if (move_children && m_first_child)
		{
			MoveChildren(this, dx, dy);
			UpdateRegion();
		}
		Invalidate(rect, true);
		return;
	}
#endif

	// Handle invalidated areas. Since the paint is pending,
	// we need to offset it so it's painted at the new position.
	if (!MDE_RectIsEmpty(m_invalid_rect))
	{
		// Use the clipped invalid rect in the check so we optimize away the case when invalidation rect is partly outside
		// the viewport. That case still allows us to just move the invalid area since the outside part doesn't matter.
		MDE_RECT clipped_invalid_rect = MDE_RectClip(m_invalid_rect, MDE_MakeRect(0, 0, m_rect.w, m_rect.h));
		if (MDE_RectContains(rect, clipped_invalid_rect))
		{
			// m_invalid_rect is inside the scrolled area. Just move it.
			m_invalid_rect.x += dx;
			m_invalid_rect.y += dy;
			m_invalid_region.Offset(dx, dy);
		}
		else if (!MDE_RectIntersects(rect, m_invalid_rect))
		{
			// The scrollrect and invalid rect doesn't intersect, so we should do nothing since they wouldn't be in the same virtual viewport.
		}
		else
		{
			// m_invalid_rect is outside the scrolled area. We can't just move it.
			// Set the invalidated area to the union of old and scrolled m_invalid_rect.
			MDE_RECT moved_rect = m_invalid_rect;
			moved_rect.x += dx;
			moved_rect.y += dy;
			m_invalid_rect = MDE_RectUnion(m_invalid_rect, moved_rect);
			// Don't bother with invalid_region (for now).
			m_invalid_region.Reset();
		}
	}

	int pos_on_screen_x = 0, pos_on_screen_y = 0;
	ConvertToScreen(pos_on_screen_x, pos_on_screen_y);

	// Call MDE_MoveRect on every rect of m_region.
	MDE_RECT destrect = rect;
	// First clip destrect to our own rect so we don't fetch pixels from outside outself if core
	// calls ScrollRect with a rect larger than the view.
	destrect = MDE_RectClip(destrect, MDE_MakeRect(0, 0, m_rect.w, m_rect.h));
	destrect.x += pos_on_screen_x;
	destrect.y += pos_on_screen_y;
	MDE_RECT cliprect = m_rect;
	if (m_parent)
	{
		cliprect = MDE_RectClip(cliprect, MDE_MakeRect(0, 0, m_parent->m_rect.w, m_parent->m_rect.h));
		m_parent->ConvertToScreen(cliprect.x, cliprect.y);
	}

	if (move_children && m_first_child)
	{
		// Exclude the children from our visibility region during
		// the scroll since they should scroll with the content.
		UpdateRegion(false);
		bool ret = ValidateRegion(false);
		if (!ret)
		{
			Invalidate(rect);
			m_screen->OutOfMemory();
			return;
		}

		// If this scroll is deferred, we have already moved the children.
		if (!is_deferred)
			MoveChildren(this, dx, dy);
	}

	// Check if the scroll area is inside the pending invalidate area.
	// If it is, we're just wasting performance scrolling it since it will all be repainted anyway.
	// This may often happen due to multiple scroll operations (without Validate between them) causing
	// invalidation handling above to expand the area when we already scroll to slow.

	MDE_RECT dst_rect_in_view = MDE_RectClip(rect, MDE_MakeRect(0, 0, m_rect.w, m_rect.h));
	if (MDE_RectContains(m_invalid_rect, dst_rect_in_view))
	{
		// Restore region
		if (move_children && m_first_child)
			UpdateRegion();

		return;
	}

	// Prepare the screen for scrolling
	MDE_Region local_update_region;
	MDE_Region* update_region = passed_screen_buf ? passed_update_region : &local_update_region;

	MDE_BUFFER *screen_buf = NULL;
	if (m_screen->ScrollPixelsSupported())
	{
#ifdef MDE_SUPPORT_SPRITES
		// Sprites are NOT SUPPORTED when implementing ScrollPixels!
		// Sprites rely on valid backbuffer so we can't just scroll the frontbuffer.
		MDE_ASSERT(!m_screen->m_first_sprite);
#endif
	}
	else
	{
		if (passed_screen_buf)
			screen_buf = passed_screen_buf;
		else
			screen_buf = m_screen->LockBuffer();
#ifdef MDE_SUPPORT_SPRITES
		m_screen->UndisplaySpritesInternal(screen_buf, update_region, MDE_MakeRect(rect.x + pos_on_screen_x,
																				rect.y + pos_on_screen_y,
																				rect.w, rect.h));
#endif
	}

	MDE_RECT region_bound = {0,0,0,0};
	int i=0;
	for(; i < m_region.num_rects; i++)
	{
		region_bound = MDE_RectUnion(region_bound, m_region.rects[i]);
	}
	destrect = MDE_RectClip(destrect, region_bound);

	// Scroll the parts that are in the visible region
	for(i = 0; i < m_region.num_rects; i++)
	{
		if (MDE_RectIntersects(destrect, m_region.rects[i]))
		{
			MDE_RECT r = MDE_RectClip(destrect, m_region.rects[i]);
			r = MDE_RectClip(r, cliprect);
			r = MDE_RectClip(r, m_screen->m_rect);
			if (!MDE_RectIsEmpty(r))
			{
				MDE_RECT move_rect = r;
				move_rect.x -= dx;
				move_rect.y -= dy;
				move_rect = MDE_RectClip(move_rect, destrect);
				move_rect.x += dx;
				move_rect.y += dy;
				if (!MDE_RectIsEmpty(move_rect))
				{
					if (screen_buf)
					{
						MDE_MoveRect(move_rect, dx, dy, screen_buf);
						m_screen->ScrollBackground(move_rect, dx, dy);

						bool ret = update_region->IncludeRect(move_rect);
						if (!ret)
						{
							if (!passed_screen_buf)
								m_screen->UnlockBuffer(update_region);
							Invalidate(rect);
							m_screen->OutOfMemory();
							return;
						}
					}
					else
					{
						m_screen->ScrollPixels(move_rect, dx, dy);
					}
				}

				if (dx < 0)
					Invalidate(MDE_MakeRect(r.x + r.w + dx - m_scroll_invalidate_extra - pos_on_screen_x, r.y - pos_on_screen_y, -dx + m_scroll_invalidate_extra, r.h), move_children);
				if (dy < 0)
					Invalidate(MDE_MakeRect(r.x - pos_on_screen_x, r.y + r.h + dy - m_scroll_invalidate_extra - pos_on_screen_y, r.w, -dy + m_scroll_invalidate_extra), move_children);
				if (dx > 0)
					Invalidate(MDE_MakeRect(r.x - pos_on_screen_x, r.y - pos_on_screen_y, dx + m_scroll_invalidate_extra, r.h), move_children);
				if (dy > 0)
					Invalidate(MDE_MakeRect(r.x - pos_on_screen_x, r.y - pos_on_screen_y, r.w, dy + m_scroll_invalidate_extra), move_children);
			}
		}
	}

	// Restore region
	if (move_children && m_first_child)
		UpdateRegion();

	// Unlock
	if (screen_buf)
	{
#ifdef MDE_SUPPORT_SPRITES
		if (!m_is_validating)
			m_screen->DisplaySpritesInternal(screen_buf, update_region);
#endif
		if (!passed_screen_buf)
			m_screen->UnlockBuffer(update_region);
	}
#endif // !MDE_FULLUPDATE_SCROLL
}

void MDE_View::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
	if (!m_is_transparent)
	{
#ifdef VEGA_OPPAINTER_SUPPORT
		VEGAOpPainter *painter = m_screen->GetVegaPainter();
		if (painter)
		{
			int x = 0;
			int y = 0;
			ConvertToScreen(x, y);
			painter->SetVegaTranslation(x, y);
			painter->SetColor(0, 0, 0, 0);
			painter->ClearRect(OpRect(rect.x, rect.y, rect.w, rect.h));
		}
#else
		MDE_SetColor(MDE_RGBA(0, 0, 0, 0), screen);
		MDE_DrawRectFill(rect, screen, false);
#endif // VEGA_OPPAINTER_SUPPORT
	}
}

void MDE_View::MakeSubsetOfScreen(MDE_BUFFER *buf, MDE_BUFFER *screen)
{
	MDE_BUFFER tmpbuf;
	MDE_BUFFER *parentbuf = screen;

	if (m_parent)
	{
		m_parent->MakeSubsetOfScreen(buf, screen);
		parentbuf = buf;
	}

	MDE_MakeSubsetBuffer(m_rect, &tmpbuf, parentbuf);
	*buf = tmpbuf;
}

#endif // MDE_SUPPORT_VIEWSYSTEM
