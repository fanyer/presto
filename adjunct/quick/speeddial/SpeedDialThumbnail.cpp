/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/speeddial/SpeedDialThumbnail.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/controller/AutoWindowReloadController.h"
#include "adjunct/quick/managers/AnimationManager.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/speeddial/DesktopSpeedDial.h"
#include "adjunct/quick/speeddial/SpeedDialExtensionContent.h"
#include "adjunct/quick/speeddial/SpeedDialPageContent.h"
#include "adjunct/quick/speeddial/SpeedDialPlusContent.h"
#include "adjunct/quick/widgets/OpSpeedDialView.h"
#include "modules/widgets/OpButton.h"
#include "modules/dragdrop/dragdrop_manager.h"

namespace
{
	const double DISPLACEMENT_ANIMATION_DURATION = 400;
	const double DISPLACEMENT_FACTOR = 30;
}

enum
{
	ANIMATE_IN,
	ANIMATE_MOVE,
	ANIMATE_OUT_AND_USE_UNDO,
	ANIMATE_OUT_AND_DO_NOT_USE_UNDO
};

DEFINE_CONSTRUCT(SpeedDialThumbnail)

SpeedDialThumbnail::SpeedDialThumbnail()
	: m_entry(NULL)
	, m_drag_object(NULL)
	, m_mouse_down_active(false)
{
	Config config;
	config.m_title_border_image     = "Speed Dial Thumbnail Title Label Skin";
	config.m_close_border_image     = "Speed Dial Thumbnail Close Button Skin";
	config.m_close_foreground_image = "Speeddial Close";
	config.m_busy_border_image      = "Speed Dial Thumbnail Spinner Label Skin";
	config.m_busy_foreground_image  = "Thumbnail Reload Image";
	config.m_drag_type              = DRAG_TYPE_SPEEDDIAL;

	init_status = GenericThumbnail::Init(config);
}

void SpeedDialThumbnail::OnDeleted()
{
	if (m_entry != NULL)
		m_entry->RemoveListener(*this);

	OP_DELETE(m_drag_object);

	GenericThumbnail::OnDeleted();
}

OP_STATUS SpeedDialThumbnail::SetEntry(const DesktopSpeedDial* entry)
{
	if (m_entry != NULL)
		m_entry->RemoveListener(*this);

	m_entry = entry;
	if (m_entry == NULL)
		return SetContent(NULL);

	GetBorderSkin()->SetImage(m_entry->IsEmpty()?"Speed Dial Thumbnail Plus Widget Skin":"Speed Dial Thumbnail Widget Skin");

	m_entry->AddListener(*this);

	if (m_entry->IsEmpty())
	{
		OpAutoPtr<SpeedDialPlusContent> content(OP_NEW(SpeedDialPlusContent, (*m_entry)));
		RETURN_OOM_IF_NULL(content.get());
		RETURN_IF_ERROR(content->Init());
		RETURN_IF_ERROR(SetContent(content.release()));
	}
	else if (m_entry->GetExtensionWUID().IsEmpty())
	{
		OpAutoPtr<SpeedDialPageContent> content(OP_NEW(SpeedDialPageContent, (*m_entry)));
		RETURN_OOM_IF_NULL(content.get());
		RETURN_IF_ERROR(content->Init());
		RETURN_IF_ERROR(SetContent(content.release()));
	}
	else
	{
		OpAutoPtr<SpeedDialExtensionContent> content(OP_NEW(SpeedDialExtensionContent, (*m_entry)));
		RETURN_OOM_IF_NULL(content.get());
		RETURN_IF_ERROR(content->Init());
		RETURN_IF_ERROR(SetContent(content.release()));
	}

	GetContent()->GetButton()->SetTabStop(true);

	return OpStatus::OK;
}

void SpeedDialThumbnail::OnSpeedDialExpired()
{
	OpStatus::Ignore(GetContent()->Refresh());
}

void SpeedDialThumbnail::OnSpeedDialEntryScaleChanged()
{
	OpStatus::Ignore(GetContent()->Zoom());
}

void SpeedDialThumbnail::AnimateThumbnailIn()
{
	QuickAnimationParams params(this);

	params.duration = 0.2;
	params.curve    = ANIM_CURVE_SLOW_DOWN;

	params.move_type = ANIM_MOVE_RECT_TO_ORIGINAL;
	params.fade_type = ANIM_FADE_IN;

	params.listener       = this;
	params.callback_param = ANIMATE_IN;

	params.start_rect = rect.InsetBy(rect.width/4, rect.height/4);

	g_animation_manager->startAnimation(params);
}

void SpeedDialThumbnail::AnimateThumbnailOut(bool use_undo)
{
	QuickAnimationParams params(this);

	params.duration = 0.2;
	params.curve    = ANIM_CURVE_SPEED_UP;

	params.move_type = ANIM_MOVE_RECT_TO_ORIGINAL;
	params.fade_type = ANIM_FADE_OUT;

	params.listener       = this;
	params.callback_param = use_undo ? ANIMATE_OUT_AND_USE_UNDO : ANIMATE_OUT_AND_DO_NOT_USE_UNDO;

	params.end_rect = rect.InsetBy(rect.width/4, rect.height/4);

	g_animation_manager->startAnimation(params);
}

BOOL SpeedDialThumbnail::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_SET_AUTOMATIC_RELOAD:
				{
					INTPTR timeout = child_action->GetActionData();
					SpeedDialData::ReloadPolicy policy = m_entry->GetReloadPolicy();

					if (timeout == 0) // Custom time
					{
						int reload_timeout = m_entry->GetReloadTimeout();
						child_action->SetSelected(policy == SpeedDialData::Reload_UserDefined && !IsStandardTimeoutMenuItem(reload_timeout));
					}
					else if (timeout == SPEED_DIAL_RELOAD_INTERVAL_DEFAULT) // Never
					{
						child_action->SetSelected(policy == SpeedDialData::Reload_NeverSoft || policy == SpeedDialData::Reload_NeverHard);
					}
					else // Fixed time
					{
						int reload_timeout = m_entry->GetReloadTimeout();
						child_action->SetSelected(policy == SpeedDialData::Reload_UserDefined && reload_timeout == timeout);
					}
					return TRUE;
				}

				// Kind of weird wording: This means "let the page decide itself when to reload"
				case OpInputAction::ACTION_DISABLE_AUTOMATIC_RELOAD:
				{
					int reload_timeout = m_entry->GetPreviewRefreshTimeout();
					SpeedDialData::ReloadPolicy policy = m_entry->GetReloadPolicy();
					child_action->SetSelected(policy == SpeedDialData::Reload_PageDefined);
					child_action->SetEnabled(reload_timeout > 0);
					return TRUE;
				}
				case OpInputAction::ACTION_MANAGE:
				{
					const uni_char *type = child_action->GetActionDataString();
					if(type && uni_stricmp(type, UNI_L("extensions")) == 0)
					{
						// action data is set to the id of the speed dial automatically, but we need to override it
						// to get the shortcuts (based on action data = 0) to show up. See DSK-337833.
						child_action->SetActionData(0);
						child_action->SetEnabled(TRUE);
					}
					else
						child_action->SetEnabled(FALSE);

					return TRUE;
				}
			}
			break;
		}

		case OpInputAction::ACTION_RELOAD_THUMBNAIL:
			GetContent()->Refresh();
			return TRUE;


		case OpInputAction::ACTION_SET_AUTOMATIC_RELOAD:
		{
			int timeout = action->GetActionData();

			if (timeout == 0)
			{
				AutoWindowReloadController* controller = OP_NEW(AutoWindowReloadController, (NULL,m_entry));

				RETURN_VALUE_IF_ERROR(ShowDialog(controller, g_global_ui_context, g_application->GetActiveDesktopWindow()),FALSE);	
			}
			else if (timeout == SPEED_DIAL_RELOAD_INTERVAL_DEFAULT) // This means "Never reload"
			{
				DesktopSpeedDial new_entry;
				new_entry.Set(*m_entry);
				new_entry.SetReload(SpeedDialData::Reload_NeverHard, 0, true);
				OpStatus::Ignore(g_speeddial_manager->ReplaceSpeedDial(m_entry, &new_entry));
			}
			else
			{
				DesktopSpeedDial new_entry;
				new_entry.Set(*m_entry);
				new_entry.SetReload(SpeedDialData::Reload_UserDefined, timeout, TRUE);
				OpStatus::Ignore(g_speeddial_manager->ReplaceSpeedDial(m_entry, &new_entry));
			}
			return TRUE;
		}

		// Kind of weird wording: This means "let the page decide itself when to reload"
		case OpInputAction::ACTION_DISABLE_AUTOMATIC_RELOAD:
		{
			int timeout = m_entry->GetPreviewRefreshTimeout();
			DesktopSpeedDial new_entry;
			new_entry.Set(*m_entry);
			new_entry.SetReload(SpeedDialData::Reload_PageDefined, timeout, TRUE);
			OpStatus::Ignore(g_speeddial_manager->ReplaceSpeedDial(m_entry, &new_entry));
			return TRUE;
		}

		case OpInputAction::ACTION_DELETE:
			g_input_manager->InvokeAction(
					OpInputAction::ACTION_THUMB_CLOSE_PAGE,
					g_speeddial_manager->GetSpeedDialActionData(m_entry));
			return TRUE;

		case OpInputAction::ACTION_EDIT_PROPERTIES:
			g_input_manager->InvokeAction(
					OpInputAction::ACTION_THUMB_CONFIGURE,
					g_speeddial_manager->GetSpeedDialActionData(m_entry));
			return TRUE;

		case OpInputAction::ACTION_RELOAD_EXTENSION:
			RETURN_VALUE_IF_ERROR(GetContent()->Refresh(), FALSE);
			return TRUE;
	}

	return GenericThumbnail::OnInputAction(action);
}

void SpeedDialThumbnail::SetFocus(FOCUS_REASON reason)
{
	GetContent()->GetButton()->SetFocus(reason);
}

void SpeedDialThumbnail::OnMouseMove(const OpPoint& point)
{
	if (g_widget_globals->captured_widget != this
			|| !m_mouse_down_active
			|| m_entry->IsEmpty()
			|| GetLocked())
		return GenericThumbnail::OnMouseMove(point);

	if (!IsDragging())
	{
		const int drag_treshold = 5;
		const bool treshold_reached = ((labs(point.x - m_mousedown_point.x) > drag_treshold) ||
		                               (labs(point.y - m_mousedown_point.y) > drag_treshold));
		if (treshold_reached)
		{
			if (StartDragging(point.x, point.y))
			{
				SetZ(Z_TOP);
			}
		}
	}

	if (IsDragging())
	{
		OpPoint parent_point = point;
		parent_point.x += rect.x;
		parent_point.y += rect.y;
		if (!GetParent()->GetBounds().Contains(parent_point) && !g_drag_manager->IsDragging())
		{
			if(g_input_manager->GetTouchInputState() & TOUCH_INPUT_ACTIVE)
			{
				// if this is caused by inertia, we don't want it flying off the viewport, nor
				// do we want to start an drag and drop outside Opera with touch
				StopDragging();
				return;
			}
			// Stop dragging and start drag'n'drop when we move it out from the viewport.
			StopDragging();
			if (StartDragging(parent_point.x, parent_point.y) && m_drag_object)
			{
				// Yes, that's on purpose: reset member first, then call StartDrag().
				DesktopDragObject* object = m_drag_object;
				m_drag_object = NULL;
				// This transfers drag object ownership to OpDragManager:
				g_drag_manager->StartDrag(object, NULL, FALSE);
			}
			return;
		}

		// Drag the floating thumbnail widget.
		int x = rect.x + point.x - m_mousedown_point.x;
		int y = rect.y + point.y - m_mousedown_point.y;
		SetRect(OpRect(x, y, rect.width, rect.height));

		// See if we are hovering over another thumbnail, and if so,
		// move the thumbnails to the new positions.
		SpeedDialThumbnail *other_sdt = GetDropThumbnail();
		if (other_sdt && other_sdt->GetContent()->AcceptsDragDrop(*m_drag_object))
		{
			other_sdt->GetContent()->HandleDragDrop(*m_drag_object);

			int target_pos = g_speeddial_manager->FindSpeedDial(m_entry);
			m_drag_object->SetID(target_pos);

			GetParent()->Relayout(true, false);
		}
	}
}

void SpeedDialThumbnail::OnMouseLeave()
{
	if (IsDragging())
	{
		StopDragging();
		return;
	}

	GenericThumbnail::OnMouseLeave();
}

void SpeedDialThumbnail::OnMouseDown(const OpPoint& point, MouseButton button, UINT8 nclicks)
{
	if (button == MOUSE_BUTTON_1)
	{
		m_mouse_down_active = true;

		m_mousedown_point = point;
		// Start floating/dragging the currently clicked thumbnail widget
		SetFloating(true);
	}

	GenericThumbnail::OnMouseDown(point, button, nclicks);

	GetParentOpSpeedDial()->OnMouseDownOnThumbnail(button, nclicks);
}

void SpeedDialThumbnail::OnMouseUp(const OpPoint& point, MouseButton button, UINT8 nclicks)
{
	m_mouse_down_active = false;

	if (IsDragging())
	{
		StopDragging();
		return;
	}

	if (!GetAnimation() || !GetAnimation()->IsAnimating())
		SetFloating(false);

	GenericThumbnail::OnMouseUp(point, button, nclicks);
}

void SpeedDialThumbnail::OnCaptureLost()
{
	StopDragging();

	GenericThumbnail::OnCaptureLost();
}

void SpeedDialThumbnail::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	// We drag the thumbnail "in place" now.
	// Starting dragging is done with StartDragging() only if dragging out of view.
}

void SpeedDialThumbnail::OnFocusChanged(OpWidget *widget, FOCUS_REASON reason)
{
	SetSelected(widget->IsFocused());
}

void SpeedDialThumbnail::OnAnimationComplete(OpWidget *anim_target, int callback_param)
{
	switch(callback_param)
	{
		case ANIMATE_OUT_AND_USE_UNDO:
			OpStatus::Ignore(g_speeddial_manager->RemoveSpeedDial(m_entry, true));
			break;

		case ANIMATE_OUT_AND_DO_NOT_USE_UNDO:
			OpStatus::Ignore(g_speeddial_manager->RemoveSpeedDial(m_entry, false));
			break;

		case ANIMATE_IN:   // fall through
		case ANIMATE_MOVE:
			break;

		default:
			OP_ASSERT(!"What kind of animation was this??");
	}
}

OpSpeedDialView* SpeedDialThumbnail::GetParentOpSpeedDial() const
{
	for (OpWidget* widget = GetParent(); widget != NULL; widget = widget->GetParent())
		if (widget->GetType() == WIDGET_TYPE_SPEEDDIAL)
			return static_cast<OpSpeedDialView*>(widget);

	return NULL;
}

#define OVERLAP_THRESHOLD (0.5)

SpeedDialThumbnail* SpeedDialThumbnail::GetDropThumbnail()
{
	// Find the SpeedDialThumbnail closest to where this one is hovering.
	OpSpeedDialView* parent = GetParentOpSpeedDial();
	double best_overlap = OVERLAP_THRESHOLD;
	SpeedDialThumbnail* best = NULL;

	if (rect.IsEmpty() || !parent)
	{
		OP_ASSERT(false); // how did this happen??
		return best;
	}

	for (int pos = 0; /* break when !sdt */; pos++)
	{
		SpeedDialThumbnail* sdt = parent->GetSpeedDialThumbnail(pos);
		if (!sdt)
			break;

		if (sdt == this)
			continue;

		// find the extent of horizontal and vertical overlap
		OpRect overlap_rect;
		if (sdt->IsFloating())
			overlap_rect = sdt->GetOriginalRect();
		else
			overlap_rect = sdt->GetRect();

		OP_ASSERT(overlap_rect.width == rect.width);

		overlap_rect.IntersectWith(rect);

		double overlap_area = overlap_rect.width * overlap_rect.height;
		double overlap = overlap_area / (rect.width * rect.height);

		if (overlap >= best_overlap)
		{
			best_overlap = overlap;
			best         = sdt;
		}
	}

	return best;
}

bool SpeedDialThumbnail::StartDragging(INT32 x, INT32 y)
{
	if (m_drag_object != NULL)
		return true;

	OpAutoPtr<DesktopDragObject> drag_object(GetContent()->GetButton()->GetDragObject(DRAG_TYPE_SPEEDDIAL, x, y));
	if (drag_object.get() != NULL && GetContent()->HandleDragStart(*drag_object))
	{
		m_drag_object = drag_object.release();
		return true;
	}
	return false;
}

void SpeedDialThumbnail::StopDragging()
{
	if (!IsDragging())
		return;

	// Animate it back to its original position.
	QuickAnimationParams params(this);
	params.duration       = 0.2;
	params.curve          = ANIM_CURVE_SLOW_DOWN;
	params.move_type      = ANIM_MOVE_RECT_TO_ORIGINAL;
	params.listener       = this;
	params.callback_param = ANIMATE_MOVE;
	g_animation_manager->startAnimation(params);

	OP_DELETE(m_drag_object);
	m_drag_object = NULL;
}

void SpeedDialThumbnail::GetRequiredThumbnailSize(INT32& width, INT32& height)
{
	width  = g_speeddial_manager->GetThumbnailWidth();
	height = g_speeddial_manager->GetThumbnailHeight();
}

