/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef NEARBY_ELEMENT_DETECTION

#include "modules/widgets/finger_touch/element_expander_impl.h"

#include "modules/display/vis_dev.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/docman.h"
#include "modules/dochand/win.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/widgets/finger_touch/element_expander_container.h"
#include "modules/widgets/finger_touch/element_of_interest.h"
#include "modules/widgets/finger_touch/fingertouch_config.h"
#include "modules/windowcommander/src/WindowCommander.h"

static inline void CheckLegalTransitions(ElementExpanderState s1, ElementExpanderState s2)
{
#ifdef _DEBUG
	char state_grid[EE_LAST_STATE_DO_NOT_USE][EE_LAST_STATE_DO_NOT_USE];
	op_memset(state_grid, FALSE, EE_LAST_STATE_DO_NOT_USE * EE_LAST_STATE_DO_NOT_USE);
	state_grid[EE_EXPANDING][EE_EXPANDED]                = TRUE;
	state_grid[EE_EXPANDING][EE_HIDING_SLASH_ACTIVATING] = TRUE;
	state_grid[EE_EXPANDED][EE_EXPANDING_TO_EDIT_MODE]   = TRUE;
	state_grid[EE_EXPANDED][EE_HIDING_SLASH_ACTIVATING]  = TRUE;
	state_grid[EE_HIDING_SLASH_ACTIVATING][EE_HIDDEN]    = TRUE;
	state_grid[EE_HIDDEN][EE_HIDING_SLASH_ACTIVATING]    = TRUE;
	state_grid[EE_HIDDEN][EE_EXPANDING]					 = TRUE;
	state_grid[EE_HIDDEN][EE_EXPANDING_TO_EDIT_MODE]	 = TRUE;
	state_grid[EE_EXPANDING_TO_EDIT_MODE][EE_EDITMODE]	 = TRUE;
	state_grid[EE_EXPANDING_TO_EDIT_MODE][EE_HIDING_SLASH_ACTIVATING] = TRUE;
	state_grid[EE_EDITMODE][EE_HIDING_SLASH_ACTIVATING]	 = TRUE;

	OP_ASSERT(state_grid[s1][s2] && "Bad state transition, fix your code!");
#endif
}

ElementExpanderImpl::ElementExpanderImpl(FramesDocument* document, OpPoint origin, unsigned int radius)
	: document(document),
	  overlay_window(0),
	  overlay_container(0),
	  origin(origin),
	  radius((unsigned int)((float)radius)),
	  scale_factor(1.4f),
	  running_animations(0),
	  state(EE_HIDDEN)
{
	element_minimium_size = g_pccore->GetIntegerPref(PrefsCollectionCore::ExpandedMinimumSize);
	timer.SetTimerListener(this);
}

ElementExpanderImpl::~ElementExpanderImpl()
{
	g_main_message_handler->UnsetCallBacks(this);

	Clear();

	// Elements of interest must cancel animations on end.
	OP_ASSERT(!running_animations && "There are pending animations, we are going to crash!");

	OP_DELETE(overlay_container);
	OP_DELETE(overlay_window);
}

OP_BOOLEAN ElementExpanderImpl::Show()
{
	if (Empty())
		return OpBoolean::IS_FALSE;

	OpWindow* parent_window = document->GetWindow()->GetWindowCommander()->GetOpWindow();
	UINT32 window_width, window_height;

	parent_window->GetInnerSize(&window_width, &window_height);

	overlay_rect.Empty();

	OP_ASSERT(!overlay_window);
	OP_ASSERT(!overlay_container);

	RETURN_IF_ERROR(OpWindow::Create(&overlay_window));
	RETURN_IF_ERROR(overlay_window->Init(OpWindow::STYLE_OVERLAY, OpTypedObject::WINDOW_TYPE_UNKNOWN, parent_window));

	overlay_container = OP_NEW(ElementExpanderContainer, (this));
	if (!overlay_container)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(overlay_container->Init(OpRect(0, 0, 0, 0), overlay_window));

#ifdef WIDGETS_F_T_GREY_BACKGROUND
	overlay_container->SetEraseBg(TRUE);
	overlay_container->GetRoot()->SetBackgroundColor(OP_RGBA(0, 0, 0, 128));
#else
	overlay_container->GetRoot()->SetBackgroundColor(OP_RGBA(0, 0, 0, 0));
	overlay_container->SetEraseBg(FALSE);
#endif // WIDGETS_F_T_GREY_BACKGROUND

	overlay_container->GetRoot()->SetListener(this);
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_FINGERTOUCH_ANIMATE, (MH_PARAM_1)this));

	OpRect visible_rect;
	GetAvailableRect(visible_rect);
	RETURN_IF_ERROR(LayoutElements(visible_rect, LAYER_SPACING, LAYER_PADDING, scale_factor));

	// No elements found suitable for expansion after laying out.
	if (Empty())
		return OpBoolean::IS_FALSE;

	//when using viewport, let's create overlay window as big as the whole
	//page so user can scroll it
	if (document)
	{
		//visual device can be NULL only for top-level document manager,
		//so it should be safe to assume it's not NULL in this case
		VisualDevice* vd = document->GetVisualDevice();
		OpRect rect = vd->GetRenderingViewport();
		window_width = rect.width;
		window_height = rect.height;
	}

	overlay_rect.Set(0, 0, window_width, window_height);
	overlay_container->SetPos(AffinePos(overlay_rect.x, overlay_rect.y));
	// FIXME, invalidates the whole gogi buffer
	overlay_container->SetSize(overlay_rect.width, overlay_rect.height);
	overlay_window->SetInnerPos(overlay_rect.x, overlay_rect.y);
	// FIXME, invalidates the whole gogi buffer
	overlay_window->SetInnerSize(overlay_rect.width, overlay_rect.height);
	overlay_window->Show(TRUE);

	// Start the link expansion timer
	ExtendTimeOut();

	for (ElementOfInterest* eoi = First(); eoi; eoi = eoi->Suc())
	{
		if (eoi->GetHtmlElement())
			eoi->GetHtmlElement()->SetReferenced(TRUE);
		RETURN_IF_ERROR(eoi->PrepareForDisplay());
	}

	ExpandElements();

	return OpBoolean::IS_TRUE;
}

void ElementExpanderImpl::Hide(BOOL animate, BOOL can_ignore/* = FALSE*/)
{
	if (!animate && state != EE_HIDDEN)
	{
		if (state != EE_HIDING_SLASH_ACTIVATING)
			ChangeState(EE_HIDING_SLASH_ACTIVATING);
		ChangeState(EE_HIDDEN);
	}
	else
	{
		// May be called from the outside, ignore if we are in the wrong state.
		if (state == EE_HIDING_SLASH_ACTIVATING || state == EE_HIDDEN)
			return;
		// We may get hide in the state when we scheduled animations and waiting
		// for MSG_FINGERTOUCH_ANIMATE to arrive
		// It can happen by double tapping while page loading or by outside call.
		if (state == EE_EXPANDING && !running_animations)
		{
			//If this was double tap then ignore the second tap, don't do any action
			if (can_ignore)
				return;
			g_main_message_handler->RemoveDelayedMessage(MSG_FINGERTOUCH_ANIMATE, reinterpret_cast<MH_PARAM_1>(this), 0);
			ChangeState(EE_HIDING_SLASH_ACTIVATING);
			ChangeState(EE_HIDDEN);
			return;
		}
		HideAllExceptOne(NULL);
	}
}

void ElementExpanderImpl::Scroll(int dx, int dy)
{
	if (state == EE_HIDDEN)
		return;

	for (ElementOfInterest* eoi = First(); eoi; eoi = eoi->Suc())
		eoi->Move(dx, dy);

	if (!AnyElementsVisible())
		Hide(FALSE);
}

void ElementExpanderImpl::OnElementRemoved(HTML_Element* html_element)
{
	for (ElementOfInterest* eoi = First(); eoi; eoi = eoi->Suc())
		if (eoi->GetHtmlElement() == html_element)
		{
			/* FIXME: May want to improve this by just removing the element,
			   instead of just ending the whole thing. */

			Hide(FALSE);
			return;
		}
}

ElementOfInterest* ElementExpanderImpl::GetActiveElement() const
{
	for (ElementOfInterest* eoi = First(); eoi; eoi = eoi->Suc())
		if (eoi->IsActive())
			return eoi;

	return 0;
}

OP_STATUS ElementExpanderImpl::LayoutElements(const OpRect& dest, int spacing, int padding, float scale)
{
	unsigned int max_elm_size = radius * 2;

	// Step 0: Remove elements that are unsuitable for expansion (due to size or position).

	RemoveUnsuitableElements(dest, max_elm_size);

	/* Step 1: Lay out each element individually.
	   May cause overlap with other elements. */

	ElementOfInterest* eoi = First();
	while (eoi)
	{
		ElementOfInterest* suc_eoi = eoi->Suc();

		// Scale elements and center them over their original rectangles.

		eoi->GenerateInitialDestRect(scale, max_elm_size, OriginClickOnScreen());

		if (eoi->GetHtmlElement())
			eoi->GetHtmlElement()->SetReferenced(TRUE);

		OP_STATUS status = eoi->PrepareForLayout(overlay_window);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(eoi);
		}
		else
		{
			// Include spacing in padding while laying out.
			eoi->SetDestinationPadding(padding + spacing);
		}

		eoi = suc_eoi;
	}

	const int count = Cardinal();
	int i;

	if (!count)
		return OpStatus::OK;

	// Create a temporary array of pointers

	ElementOfInterest** tmp_elms = OP_NEWA(ElementOfInterest*, count);

	if (!tmp_elms)
		return OpStatus::ERR_NO_MEMORY;

	ElementOfInterest** pp1 = tmp_elms;

	*pp1 = First();
	while ((*pp1)->Suc())
	{
		*(pp1+1) = (*pp1)->Suc();
		++ pp1;
	}

	/* Step 2: Spread the elements out so they don't overlap, trying to move
	   them as little away from the original position as possible */

	OP_STATUS status = RemoveElementsOverlap(tmp_elms, count);

	if (OpStatus::IsError(status))
	{
		OP_DELETEA(tmp_elms);
		return status;
	}

	/* Step 3: Move the whole bunch of target rectangles so they better fit the
	   screen (if possible). Prioritize top and left */

	OpRect containing_rect;

	for (i = 0; i < count; i ++)
		containing_rect.UnionWith(tmp_elms[i]->GetDestination());

	int oldXOffset = containing_rect.x;
	int oldYOffset = containing_rect.y;

	// Translate expanded elements so the vertical edge of the containing rect
	// which is closest to the screen edge closest to the user click
	// are shown inside the screen:
	Edge nearest_vert_scr_edge = EDGE_WEST, nearest_horiz_scr_edge = EDGE_NORTH;
	GetNearestEdge(nearest_horiz_scr_edge, nearest_vert_scr_edge, OriginClickOnScreen(), dest);

	SmartPositionRectInRect(containing_rect, dest, nearest_horiz_scr_edge, nearest_vert_scr_edge);

	// Final step: Adjust final position and set padding.

	for (i = 0; i < count; i ++)
	{
		ElementOfInterest* eoi = tmp_elms[i];

		if (containing_rect.x != oldXOffset || containing_rect.y != oldYOffset)
			eoi->TranslateDestination(containing_rect.x - oldXOffset, containing_rect.y - oldYOffset);

		eoi->SetDestinationPadding(padding);
	}

	OP_DELETEA(tmp_elms);

	return OpStatus::OK;
}

void ElementExpanderImpl::GetNearestEdge(ElementExpanderImpl::Edge& horiz_edge, ElementExpanderImpl::Edge& vert_edge,
									 const OpPoint& p, const OpRect& r)
{
	vert_edge = (p.x - r.Left() < r.width / 2) ? EDGE_WEST : EDGE_EAST;
	horiz_edge = (p.y - r.Top() < r.height / 2) ? EDGE_NORTH : EDGE_SOUTH;
}

/* static */
void ElementExpanderImpl::SmartPositionRectInRect(OpRect& a, const OpRect& b,
												  ElementExpanderImpl::Edge pri_horiz_edge,
												  ElementExpanderImpl::Edge pri_vert_edge)
{
	if (pri_vert_edge == EDGE_EAST || a.width <= b.width)
	{
		if (a.Right() > b.Right())
			a.x -= MIN(MAX(0, a.x - b.x), a.Right() - b.Right());
	}
	if (pri_vert_edge == EDGE_WEST || a.width <= b.width)
	{
		if (a.x < b.x)
			a.x += MIN(MAX(0, b.Right() - a.Right()), b.x - a.x);
	}

	if (pri_horiz_edge == EDGE_NORTH || a.height <= b.height)
	{
		if (a.y < b.y)
			a.y += MIN(MAX(0, b.Bottom() - a.Bottom()), b.y - a.y);
	}
	if (pri_horiz_edge == EDGE_SOUTH || a.height <= b.height)
	{
		if (a.Bottom() > b.Bottom())
			a.y -= MIN(MAX(0, a.y - b.y), a.Bottom() - b.Bottom());
	}
}

void ElementExpanderImpl::RemoveUnsuitableElements(const OpRect& visible_screen_rect, unsigned int max_elm_size)
{
	ElementOfInterest* elm = First();

	while (elm)
	{
		ElementOfInterest* next_elm = elm->Suc();

		if (!elm->GetOriginalRect().Intersecting(visible_screen_rect) ||
			!elm->IsSuitable(max_elm_size))
		{
			elm->Out();
			OP_DELETE(elm);
		}

		elm = next_elm;
	}
}

OpPoint ElementExpanderImpl::OriginClickOnScreen() const
{
	short root_x = 0;
	long root_y = 0;
	for (FramesDocument* doc = document; doc; doc = doc->GetDocManager()->GetParentDoc())
	{
		VisualDevice* vd = doc->GetVisualDevice();

		AffinePos vd_pos;
		vd->GetPosition(vd_pos);

		OpPoint vd_pos_pt = vd_pos.GetTranslation();

		root_x += vd_pos_pt.x - vd->ScaleToScreen(vd->GetRenderingViewX());
		root_y += vd_pos_pt.y - vd->ScaleToScreen(vd->GetRenderingViewY());
	}

	VisualDevice* vis_dev = document->GetVisualDevice();
	OpPoint document_origin(vis_dev->ScaleToScreen(origin.x, origin.y));
	document_origin.x += root_x;
	document_origin.y += root_y;

	return document_origin;
}

BOOL ElementExpanderImpl::HasAmbiguity() const
{
	BOOL ambiguity_detected = TRUE;

	if (Cardinal() == 1)
	{
		ElementOfInterest* e = First();
		if (e->HasEditMode())
		{
			// Spec: Complex form control (edit fields, drop downs, select lists) should go directly into active expanded (eg. editing state).
			// if it is the only one to be expanded.
			ambiguity_detected = FALSE;
		}
		else
		{
			if (!g_pccore->GetIntegerPref(PrefsCollectionCore::AlwaysExpandLinks))
			{
				// Other elements are activated only if the click is inside their rectangle.
				// when AlwaysExpandLinks is off
				OpPoint click_on_screen = OriginClickOnScreen();
				if (First()->GetOriginalRect().Contains(click_on_screen))
				{
					ambiguity_detected = FALSE;
				}
			}
		}
	}

	return ambiguity_detected;
}

void ElementExpanderImpl::TranslateElementDestinations(int offset_x, int offset_y)
{
	for (ElementOfInterest* e = First(); e; e = e->Suc())
		e->TranslateDestination(offset_x, offset_y);
}

OP_STATUS ElementExpanderImpl::RemoveElementsOverlap(ElementOfInterest** tmp_elms, int count)
{
	// TODO: use a vector maybe?
	int *row_counts = OP_NEWA(int, count);

	if(!row_counts)
		return OpStatus::ERR_NO_MEMORY;

	int i, j;

	for (i=0; i < count; i++)
		row_counts[i] = 0;

	// max bottom and right positions before and after moving the elements
	int max_bottom = -1, max_bottom_new = -1;
	int max_right = -1, max_right_new = -1;

	op_qsort(tmp_elms, count, sizeof(ElementOfInterest**), &CmpYDir);

	max_bottom = tmp_elms[count-1]->GetDestination().Bottom();

	OpRect elm_rect;

	// separate elements into rows
	int row = -1;
	int boundary = INT_MIN;
	for (i=0; i < count; i++)
	{
		elm_rect = tmp_elms[i]->GetDestination();

		if (elm_rect.Top() >= boundary)
		{
			row++;
			// completely arbitrary
			boundary = elm_rect.Top() + 5;
		}
		row_counts[row]++;
	}

	// if the largest bottom y position from the previous row is larger than
	// the top y position of the current row, move all the elements of the
	// row down by the difference between those y positions

	int row_index = 0;
	for (i=1; i < count && row_counts[i]; i++)
	{
		int bottom_y = 0;

		for (j=0; j < row_counts[i-1]; j++)
		{
			elm_rect = tmp_elms[row_index + j]->GetDestination();
			if (elm_rect.Bottom() > bottom_y)
				bottom_y = elm_rect.Bottom();
		}

		row_index += row_counts[i-1];

		elm_rect = tmp_elms[row_index]->GetDestination();
		if (elm_rect.Top() < bottom_y)
		{
			int y_translation = bottom_y - elm_rect.Top();

			for (j=0; j < row_counts[i]; j++)
				tmp_elms[row_index + j]->TranslateDestination(0, y_translation);
		}
	}

	// spread each row's elements in the x direction
	for (i=0, row_index=0; i < count && row_counts[i]; row_index += row_counts[i++])
	{
		op_qsort(tmp_elms+row_index, row_counts[i], sizeof(ElementOfInterest*), &CmpXDir);

		// FIXME: sorted by x (left) doesn't mean its right is the 'rightest'
		if (tmp_elms[row_index+row_counts[i]-1]->GetDestination().Right() > max_right)
			max_right = tmp_elms[row_index+row_counts[i]-1]->GetDestination().Right();

		for (j=0; j < row_counts[i]; j++)
		{
			OpRect elm_rect1 = tmp_elms[row_index+j]->GetDestination();

			for (int k=j+1; k < row_counts[i]; k++)
			{
				OpRect elm_rect2 = tmp_elms[row_index+k]->GetDestination();

				if (!elm_rect1.Intersecting(elm_rect2))
					continue;

				int x_overlap = elm_rect1.Right() - elm_rect2.Left();

				tmp_elms[row_index+k]->TranslateDestination(x_overlap, 0);
			}
		}
	}

	for (i=0, row_index=0; i < count && row_counts[i]; row_index += row_counts[i++])
	{
		for (j=0; j < row_counts[i]; j++)
		{
			elm_rect = tmp_elms[row_index+j]->GetDestination();

			if (elm_rect.Bottom() > max_bottom_new)
				max_bottom_new = elm_rect.Bottom();

			if (elm_rect.Right() > max_right_new)
				max_right_new = elm_rect.Right();
		}
	}

	// Compensate for the growth of the element area by offsetting all the
	// elements in the opposite direction so they become more centered compared
	// to the old positions.

	int y_growth = max_bottom_new - max_bottom;
	int x_growth = max_right_new - max_right;

	if (y_growth < 0) y_growth = 0;
	if (x_growth < 0) x_growth = 0;

	// do something with OriginClickOnScreen() instead of this?
	TranslateElementDestinations(-x_growth / 2, -y_growth / 2);

	OP_DELETEA(row_counts);

	return OpStatus::OK;
}

/* static */ int ElementExpanderImpl::CmpXDir(const void* p1, const void* p2)
{
	const ElementOfInterest* const * e1 = reinterpret_cast<const ElementOfInterest* const *>(p1);
	const ElementOfInterest* const * e2 = reinterpret_cast<const ElementOfInterest* const *>(p2);

	return (*e1)->GetOriginalRect().x - (*e2)->GetOriginalRect().x;
}

/* static */ int ElementExpanderImpl::CmpYDir(const void* p1, const void* p2)
{
	const ElementOfInterest* const * e1 = reinterpret_cast<const ElementOfInterest* const *>(p1);
	const ElementOfInterest* const * e2 = reinterpret_cast<const ElementOfInterest* const *>(p2);

	return (*e1)->GetOriginalRect().y - (*e2)->GetOriginalRect().y;
}

void ElementExpanderImpl::ChangeState(ElementExpanderState s)
{
	WindowCommander* wic = document->GetWindow()->GetWindowCommander();
	OpFingertouchListener* listener = wic->GetFingertouchListener();

	CheckLegalTransitions(state, s);
	OP_ASSERT(s != state); // No NOT call this twice with the same state.
	state = s;

	timer.Stop();

	switch(state)
	{
	case EE_EXPANDING:
		listener->OnElementsExpanding(wic);
		break;
	case EE_EXPANDED:
		ExtendTimeOut();
		listener->OnElementsExpanded(wic);
		break;
	case EE_EXPANDING_TO_EDIT_MODE:
		break;
	case EE_EDITMODE:
		// Focus the clone widget, if activated.
		if (ElementOfInterest* activated_eoi = GetActiveElement())
			activated_eoi->GetActivatedWidget()->SetFocus(FOCUS_REASON_MOUSE);
		break;
	case EE_HIDING_SLASH_ACTIVATING:
		listener->OnElementsHiding(wic);
		document->GetVisualDevice()->SetFocus(FOCUS_REASON_MOUSE);
		break;
	case EE_HIDDEN:
		// There is no way back from this state.
		listener->OnElementsHidden(wic);
		overlay_window->Show(FALSE);
		break;
	default:
		OP_ASSERT(!"Unknown state");
	}
}

void ElementExpanderImpl::AdvanceState()
{
	switch(state)
	{
	case EE_HIDING_SLASH_ACTIVATING:
		ChangeState(EE_HIDDEN);
		break;
	case EE_EXPANDING:
		ChangeState(EE_EXPANDED);
		break;
	case EE_EXPANDING_TO_EDIT_MODE:
		ChangeState(EE_EDITMODE);
		break;
	case EE_HIDDEN:
		// Only OK if we are cancelling.
		break;
	default:
		OP_ASSERT(!"Invalid state after animation end!");
	}
}

void ElementExpanderImpl::OnAnimationEnded(ElementOfInterest* element)
{
	running_animations--;
	OP_ASSERT(running_animations >= 0);
	if (running_animations)
		return;

	AdvanceState();
}

void ElementExpanderImpl::OnAllAnimationsEnded()
{
	/* Remove all the elements except the active when going into edit mode.
	 * This should probably be in ChangeState, but it's here because StartAnimation loops across all
	 * the elements calling eoi->StartAnimation on each, which (down the line) eventually advances
	 * state, but before finishing the StartAnimation loop, so the elements can't be removed before
	 * the loop ends.
	 * FIXME: hackish, maybe use a callback?
	 */

	if (state != EE_EDITMODE)
		return;

	ElementOfInterest* activated_eoi = GetActiveElement();
	if (activated_eoi)
	{
		ElementOfInterest *element = First(), *suc;
		while (element)
		{
			suc = element->Suc();
			if (element != activated_eoi)
			{
				element->Out();
				OP_DELETE(element);
			}
			element = suc;
		}
	}
}

void ElementExpanderImpl::ScheduleAnimation(ElementOfInterest* eoi)
{
	TriggerCallback(MSG_FINGERTOUCH_ANIMATE);
}

void ElementExpanderImpl::ExpandElements()
{
	if (HasAmbiguity())
	{
		ChangeState(EE_EXPANDING);
		for (ElementOfInterest* eoi = First(); eoi; eoi = eoi->Suc())
			eoi->Expand();
	}
	else
	{
		Activate(First());
	}
}

void ElementExpanderImpl::Activate(ElementOfInterest* element)
{
	if (state != EE_EXPANDED && state != EE_HIDDEN)
		return;

	HTML_Element* html_element = element->GetHtmlElement();

	element->Activate();
	HideAllExceptOne(element);

	// FIXME: Should probably use better coordinate values here.

	int offset_x = 0;
	int offset_y = 0;
	int document_x = 0;
	int document_y = 0;

	// Store window used to report error later on.
	Window* window = document->GetWindow();

	// Warning; this call may destroy the ElementExpander. Do not use any class
	// member after this line.
	OP_STATUS status = document->HandleMouseEvent(ONCLICK, NULL, html_element, NULL, offset_x, offset_y, document_x, document_y, SHIFTKEY_NONE, MAKE_SEQUENCE_COUNT_AND_BUTTON(1, MOUSE_BUTTON_1), NULL);

	if (OpStatus::IsMemoryError(status))
		window->RaiseCondition(status);
}

void ElementExpanderImpl::HideAllExceptOne(ElementOfInterest* activated_element)
{
	ChangeState((activated_element && activated_element->HasEditMode())
				? EE_EXPANDING_TO_EDIT_MODE
				: EE_HIDING_SLASH_ACTIVATING);

	for (ElementOfInterest *element = First(); element; element = element->Suc())
		if (element != activated_element)
			element->Hide();
}

BOOL ElementExpanderImpl::AnyElementsVisible() const
{
	OpRect visible_rect;

	GetAvailableRect(visible_rect);

	for (ElementOfInterest* eoi = First(); eoi; eoi = eoi->Suc())
		if (visible_rect.Intersecting(eoi->GetDestination()))
			return TRUE;

	return FALSE;
}

void ElementExpanderImpl::GetAvailableRect(OpRect& available) const
{
	//if we cant access the op window, we cant get the right dpi,
	//but there is a 'main screen' fallback, so we need to check
	OpScreenProperties sp;
	if(document
		&& document->GetDocManager()
		&& document->GetDocManager()->GetWindow())
	{
		g_op_screen_info->GetProperties(&sp, document->GetDocManager()->GetWindow()->GetOpWindow());
	} else {
		g_op_screen_info->GetProperties(&sp, NULL);
	}

	available.x = sp.workspace_rect.x;
	available.y = sp.workspace_rect.y;
	available.width = sp.workspace_rect.width;
	available.height = sp.workspace_rect.height;
}

unsigned int ElementExpanderImpl::GetScheduledAnimationsCount()
{
	unsigned int count = 0;

	for (ElementOfInterest* eoi = First(); eoi; eoi = eoi->Suc())
		if (eoi->HasScheduledAnimation())
			count ++;

	return count;
}

void ElementExpanderImpl::StartAnimation()
{
	if (running_animations)
		AdvanceState();

	running_animations = GetScheduledAnimationsCount();

	for (ElementOfInterest* eoi = First(); eoi; eoi = eoi->Suc())
		eoi->StartAnimation();

	OnAllAnimationsEnded();
}

void ElementExpanderImpl::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(timer == &this->timer);
	OP_ASSERT(GetState() == EE_EXPANDED);

	if (running_animations > 0)
		ExtendTimeOut();
	else
		Hide(TRUE);
}

void ElementExpanderImpl::ExtendTimeOut()
{
	timer.Start(OVERLAY_HIDE_DELAY);
}

void ElementExpanderImpl::OnClick(OpWidget *widget, UINT32 id)
{
	if (GetState() != EE_EXPANDED && GetState() != EE_EDITMODE)
		// Ignore clicks unless we are expanded
		return;

	// Check if we click inside an element of interest
	ElementOfInterest* eoi_clicked=GetEoiAtPosition(widget->GetMousePos());

	if (eoi_clicked)
	{
		eoi_clicked->OnClick(widget,id);
	}
	else
	{
		// Clicked on the overlay window. Hide everything.
		Hide(TRUE);
	}
}

void ElementExpanderImpl::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (GetState() != EE_EXPANDED && GetState() != EE_EDITMODE)
		// Ignore events unless we are expanded
		return;

	// Check if the mouse event is inside an element of interest
	ElementOfInterest* eoi_mouse_event = GetEoiAtPosition(OpPoint(x,y));
	if (eoi_mouse_event)
	{
		eoi_mouse_event->OnMouseEvent(widget,pos, x, y, button, down, nclicks);
	}
}

void ElementExpanderImpl::TriggerCallback(OpMessage msg)
{
	OP_ASSERT(g_main_message_handler->HasCallBack(this, msg, reinterpret_cast<MH_PARAM_1>(this)));
	g_main_message_handler->RemoveDelayedMessage(msg, reinterpret_cast<MH_PARAM_1>(this), 0);
	g_main_message_handler->PostDelayedMessage(msg, reinterpret_cast<MH_PARAM_1>(this), 0, 0);
}

void ElementExpanderImpl::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch(msg)
	{
	case MSG_FINGERTOUCH_ANIMATE:
		StartAnimation();
		break;
	}
}

ElementOfInterest* ElementExpanderImpl::GetEoiAtPosition(OpPoint pos)
{
	for (ElementOfInterest* eoi = First(); eoi; eoi = eoi->Suc())
	{
		if( eoi->GetDestination().Contains(pos) )
		{
			return eoi;
		}
	}

	return NULL;
}
#endif // NEARBY_ELEMENT_DETECTION
