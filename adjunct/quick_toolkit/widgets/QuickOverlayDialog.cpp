/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickOverlayDialog.h"

#include "adjunct/quick/managers/AnimationManager.h"
#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
#include "adjunct/quick_toolkit/widgets/OpOverlay.h"
#include "adjunct/quick_toolkit/widgets/OpWindowMover.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"

namespace
{
	const int DIALOG_ANIMATION_CALLOUT_OFFSET = 15;
}

QuickOverlayDialog::QuickOverlayDialog()
	: m_bounding_widget(NULL)
	, m_placer_widget(NULL)
	, m_placer(NULL)
	, m_mover_widget(NULL)
#ifdef QUICK_TOOLKIT_OVERLAY_IS_SHEET_BY_DEFAULT
	, m_animation_type(DIALOG_ANIMATION_SHEET)
#else
	, m_animation_type(DIALOG_ANIMATION_FADE_AND_SLIDE)
#endif
	, m_dims_page(false)
{
	SetStyle(OpWindow::STYLE_TRANSPARENT);
}

QuickOverlayDialog::~QuickOverlayDialog()
{
	OP_DELETE(m_placer);

	if (m_placer_widget && !m_placer_widget->IsDeleted())
		m_placer_widget->Delete();
}

OP_STATUS QuickOverlayDialog::SetTitle(const OpStringC& title)
{
	RETURN_IF_ERROR(QuickDialog::SetTitle(title));

	// Sheets don't display the title.
	if (m_animation_type == DIALOG_ANIMATION_SHEET)
		return OpStatus::OK;

	if (!m_mover_widget)
	{
		OpAutoPtr<QuickWindowMover> mover_widget(OP_NEW(QuickWindowMover, ()));
		RETURN_OOM_IF_NULL(mover_widget.get());
		RETURN_IF_ERROR(mover_widget->Init());

		mover_widget->SetSkin("Label Edit Skin");
		mover_widget->SetPreferredWidth(WidgetSizes::Fill);

		m_mover_widget = mover_widget.release();
		SetDialogHeader(m_mover_widget);
	}

	return m_mover_widget->GetOpWidget()->SetText(title);
}

OP_STATUS QuickOverlayDialog::OnShowing()
{
	OP_NEW_DBG("QuickOverlayDialog::OnShowing", "quicktoolkit");

	if (m_placer_widget)
		return OpStatus::OK;

	DesktopWindow* parent_window =
			GetDesktopWindow() ? GetDesktopWindow()->GetParentDesktopWindow() : NULL;

	if (parent_window)
	{
		if (!m_bounding_widget)
			m_bounding_widget = parent_window->GetBrowserView();
		if (!m_bounding_widget)
			m_bounding_widget = parent_window->GetWidgetContainer()->GetRoot();
	}
	if (!m_bounding_widget)
	{
		OP_ASSERT(!"The dialog must either have a bounding widget set or have a parent window");
		return OpStatus::ERR_NULL_POINTER;
	}

	OpAutoPtr<OpOverlayLayoutWidget> placer_widget(OP_NEW(OpOverlayLayoutWidget, ()));
	RETURN_OOM_IF_NULL(placer_widget.get());
	RETURN_IF_ERROR(placer_widget->Init(GetDesktopWindow(), m_bounding_widget));
	m_placer_widget = placer_widget.release();

	m_placer_widget->SetXResizeEffect(RESIZE_MOVE);
	m_placer_widget->SetYResizeEffect(RESIZE_MOVE);

	m_placer_widget->SetListener(this);

	if (m_dims_page && parent_window && parent_window->GetBrowserView())
		parent_window->GetBrowserView()->RequestDimmed(true);

	if (m_placer)
	{
		UpdatePlacement();
	}
	else
	{
#ifdef QUICK_TOOLKIT_OVERLAY_IS_SHEET_BY_DEFAULT
		SetDialogPlacer(OP_NEW(SheetPlacer, ()));
#else
		SetDialogPlacer(OP_NEW(DefaultPlacer, ()));
#endif
		RETURN_OOM_IF_NULL(m_placer);
	}

	if (m_mover_widget)
		m_mover_widget->GetOpWidget()->SetTargetWindow(*GetDesktopWindow());

	RETURN_IF_ERROR(AnimateIn());

	return OpStatus::OK;
}

void QuickOverlayDialog::SetDialogPlacer(QuickOverlayDialogPlacer* placer)
{
	OP_DELETE(m_placer);
	m_placer = placer;
	
	if (!m_placer)
	{
		m_placer = OP_NEW(DummyPlacer, ());
	}
	else if (m_placer_widget)
	{
		UpdatePlacement();
	}
}

OpRect QuickOverlayDialog::CalculatePlacement()
{
	OP_NEW_DBG("QuickOverlayDialog::CalculatePlacement", "quicktoolkit");

	OP_ASSERT(m_placer_widget);
	OP_ASSERT(m_placer);

	const unsigned nominal_width = GetNominalWidth();
	const OpRect size(DEFAULT_SIZEPOS, DEFAULT_SIZEPOS, nominal_width, GetNominalHeight(nominal_width));
	const OpRect bounds = m_bounding_widget->GetRect();

	OpRect placement = m_placer->CalculatePlacement(bounds, size);

	// Force within bounds.
	placement.width = MIN(placement.width, bounds.width);
	placement.height = MIN(placement.height, bounds.height);
	placement.x = MIN(bounds.width - placement.width, MAX(0, placement.x));
	placement.y = MIN(bounds.height - placement.height, MAX(0, placement.y));

	if (!m_placer->UsesRootCoordinates())
		// Transform to parent window coordinates.
		placement.OffsetBy(bounds.x, bounds.y);

	OP_DBG(("placement = ") << placement);
	
	return placement;
}

void QuickOverlayDialog::UpdatePlacement()
{
	// Request to update placement might come when we're already closing,
	// during the time when MSG_CLOSE_AUTOCOMPL_POPUP has not been handled yet.
	if (GetDesktopWindow()->IsClosing())
		return;

	const OpRect& new_placement = CalculatePlacement();
	m_placer_widget->SetRect(new_placement);
	m_placer->PointArrow(*GetDesktopWindow()->GetSkinImage(), new_placement);
}

void QuickOverlayDialog::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	OP_ASSERT(desktop_window == GetDesktopWindow());

	if (m_dims_page)
	{
		DesktopWindow* parent_window = desktop_window->GetParentDesktopWindow();
		OpBrowserView* browser_view = parent_window ? parent_window->GetBrowserView() : NULL;
		if (browser_view)
			browser_view->RequestDimmed(false);
	}

	OpStatus::Ignore(AnimateOut());

	if (m_placer_widget && !m_placer_widget->IsDeleted())
		m_placer_widget->Delete();
	m_placer_widget = NULL;

	QuickDialog::OnDesktopWindowClosing(desktop_window, user_initiated);
}

void QuickOverlayDialog::PrepareInAnimation(QuickAnimationWindowObject& animation)
{
	switch (m_animation_type)
	{
		case DIALOG_ANIMATION_NONE:
			break;

		case DIALOG_ANIMATION_FADE_AND_SLIDE:
		{
			const OpRect target_rect = m_placer_widget->GetRect();
			OpRect source_rect = target_rect;
			source_rect.y -= source_rect.height / 4;
			animation.animateRect(source_rect, target_rect);
		}
		// fall through
		case DIALOG_ANIMATION_FADE:
			animation.animateOpacity(0, 1);
			animation.m_animation_curve = ANIM_CURVE_SLOW_DOWN;
			break;

		case DIALOG_ANIMATION_CALLOUT:
		{
			const OpRect target_rect = m_placer_widget->GetRect();
			OpRect source_rect = target_rect;
			int offset = DIALOG_ANIMATION_CALLOUT_OFFSET;
			SkinArrow* arrow = GetDesktopWindow()->GetSkinImage()->GetArrow();
			if (arrow != NULL && arrow->part == SKINPART_TILE_TOP)
				offset = -offset;
			source_rect.y += offset;
			animation.animateRect(source_rect, target_rect);

			animation.animateOpacity(0, 1);
			animation.m_animation_curve = ANIM_CURVE_SLOW_DOWN;
			break;
		}

		case DIALOG_ANIMATION_SHEET:
		{
			const OpRect target_rect = m_placer_widget->GetRect();
			OpRect source_rect = target_rect;
			source_rect.y -= source_rect.height * 2;
			animation.animateRect(source_rect, target_rect);

			animation.m_animation_curve = ANIM_CURVE_LINEAR;
			break;
		}

		default:
			OP_ASSERT(!"Unknown animation type");
	}
}

void QuickOverlayDialog::PrepareOutAnimation(QuickAnimationWindowObject& animation)
{
	switch (m_animation_type)
	{
		case DIALOG_ANIMATION_NONE:
			break;

		case DIALOG_ANIMATION_FADE_AND_SLIDE:
		{
			OpRect source_rect;
			GetDesktopWindow()->GetRect(source_rect);
			OpRect target_rect = source_rect;
			target_rect.y += target_rect.height / 4;
			animation.animateRect(source_rect, target_rect);
		}
		// fall through
		case DIALOG_ANIMATION_FADE:
			animation.animateOpacity(1, 0);
			animation.m_animation_curve = ANIM_CURVE_SPEED_UP;
			break;

		case DIALOG_ANIMATION_CALLOUT:
		{
			OpRect source_rect;
			GetDesktopWindow()->GetRect(source_rect);
			OpRect target_rect = source_rect;
			int offset = DIALOG_ANIMATION_CALLOUT_OFFSET;
			SkinArrow* arrow = GetDesktopWindow()->GetSkinImage()->GetArrow();
			if (arrow != NULL && arrow->part == SKINPART_TILE_TOP)
				offset = -offset;
			target_rect.y += offset;
			animation.animateRect(source_rect, target_rect);

			animation.animateOpacity(1, 0);
			animation.m_animation_curve = ANIM_CURVE_SPEED_UP;
			break;
		}

		case DIALOG_ANIMATION_SHEET:
		{
			OpRect source_rect;
			GetDesktopWindow()->GetRect(source_rect);
			OpRect target_rect = source_rect;
			target_rect.y -= target_rect.height * 2;
			animation.animateRect(source_rect, target_rect);

			animation.m_animation_curve = ANIM_CURVE_LINEAR;
			break;
		}

		default:
			OP_ASSERT(!"Unknown animation type");
	}
}

OP_STATUS QuickOverlayDialog::AnimateIn()
{
	if (m_animation_type == DIALOG_ANIMATION_NONE)
		return OpStatus::OK;

	OpAutoPtr<QuickAnimationWindowObject> animation(OP_NEW(QuickAnimationWindowObject, ()));
	RETURN_OOM_IF_NULL(animation.get());
	RETURN_IF_ERROR(animation->Init(GetDesktopWindow(), NULL));

	animation->animateIgnoreMouseInput(true, false);
	PrepareInAnimation(*animation);

	const AnimationCurve curve = animation->m_animation_curve;
	g_animation_manager->startAnimation(animation.release(), curve);

	return OpStatus::OK;
}

OP_STATUS QuickOverlayDialog::AnimateOut()
{
	if (m_animation_type == DIALOG_ANIMATION_NONE)
		return OpStatus::OK;

	if (GetDesktopWindow()->GetParentDesktopWindow() == NULL)
		return OpStatus::ERR;

	if (GetDesktopWindow()->GetParentDesktopWindow()->IsClosing())
		return OpStatus::OK;

	OpAutoPtr<QuickAnimationBitmapWindow> animation(OP_NEW(QuickAnimationBitmapWindow, ()));
	RETURN_OOM_IF_NULL(animation.get());
	RETURN_IF_ERROR(animation->Init(
				GetDesktopWindow()->GetParentDesktopWindow(),
				GetDesktopWindow()->GetOpWindow(), NULL));

	animation->animateIgnoreMouseInput(true, true);
	PrepareOutAnimation(*animation);

	const AnimationCurve curve = animation->m_animation_curve;
	g_animation_manager->startAnimation(animation.release(), curve);

	return OpStatus::OK;
}
