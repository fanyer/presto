/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * Chris Pine, Tim Johansson, Emil Segerås
 */

#include "core/pch.h"

#ifdef VEGA_OPPAINTER_SUPPORT
#include "adjunct/quick/managers/AnimationManager.h"
#include "adjunct/quick/widgets/OpPagebar.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpWindow.h"
#include "modules/libgogi/pi_impl/mde_opwindow.h"
#include "modules/libgogi/pi_impl/mde_opview.h"
#include "modules/libvega/src/oppainter/vegaoppainter.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/display/vis_dev.h"
#include "adjunct/desktop_util/prefs/PrefsCollectionDesktopUI.h"

double ValidateDuration(double duration)
{
	if (duration == -1)
		duration = DEFAULT_ANIMATION_DURATION * 1000;
#ifdef _DEBUG
	if (g_op_system_info->GetShiftKeyState() == (SHIFTKEY_SHIFT | SHIFTKEY_CTRL))
		duration *= 10;
#endif
	return duration;
}

QuickAnimationManager::QuickAnimationManager() : m_callback_set(false), m_posted_msg(false), m_disable_animations_counter(0), m_is_advancing_animations(false)
{}

OP_STATUS QuickAnimationManager::Init()
{
	// Prepare and build curve paths
	for(int i = 0; i < NUM_ANIMATION_CURVES; i++)
	{
		RETURN_IF_ERROR(animationCurvePath[i].prepare(0));
		RETURN_IF_ERROR(animationCurvePath[i].moveTo(0,0));
		switch (i)
		{
		case ANIM_CURVE_BEZIER:
			{
			VEGA_FIX f = VEGA_INTTOFIX(500); // The larger the value, the more linear is the result.
			RETURN_IF_ERROR(animationCurvePath[i].cubicBezierTo(
						VEGA_INTTOFIX(1000), VEGA_INTTOFIX(1000), 
						VEGA_INTTOFIX(1000) - f, 0, 
						f, VEGA_INTTOFIX(1000), 
						VEGA_INTTOFIX(1)/10));
			}
			break;
		case ANIM_CURVE_SPEED_UP:
			RETURN_IF_ERROR(animationCurvePath[i].cubicBezierTo(
						VEGA_INTTOFIX(1000), VEGA_INTTOFIX(1000), 
						VEGA_INTTOFIX(500), 0, 
						VEGA_INTTOFIX(1000), VEGA_INTTOFIX(500), 
						VEGA_INTTOFIX(1)/10));
			break;
		case ANIM_CURVE_SLOW_DOWN:
			RETURN_IF_ERROR(animationCurvePath[i].cubicBezierTo(
						VEGA_INTTOFIX(1000), VEGA_INTTOFIX(1000), 
						0, VEGA_INTTOFIX(500), 
						VEGA_INTTOFIX(500), VEGA_INTTOFIX(1000), 
						VEGA_INTTOFIX(1)/10));
			break;
		case ANIM_CURVE_LINEAR:
		default:
			RETURN_IF_ERROR(animationCurvePath[i].lineTo(VEGA_INTTOFIX(1000), VEGA_INTTOFIX(1000)));
			break;
		}
	}
	return OpStatus::OK;
}

QuickAnimationManager::~QuickAnimationManager()
{
	if (m_callback_set)
	{
		g_main_message_handler->UnsetCallBack(this, MSG_QUICK_ADVANCE_ANIMATION, (INTPTR) this);
		g_main_message_handler->RemoveDelayedMessage(MSG_QUICK_ADVANCE_ANIMATION, (MH_PARAM_1)this, 0);
	}
	m_active_animations.RemoveAll();
}

bool QuickAnimationManager::GetEnabled()
{
	if (m_disable_animations_counter > 0)
		return false;
	return !!g_pcui->GetIntegerPref(PrefsCollectionUI::EnableUIAnimations);
}

OP_STATUS QuickAnimationManager::preparePageLoadAnimation(OpView *view, OpWindow *window, AnimationEvent evt)
{
#ifdef VEGA_OPPAINTER_ANIMATIONS
	if (GetEnabled())
	{
		MDE_View *mde_view = ((MDE_OpView*)view)->GetMDEWidget();
		if (!mde_view->m_region.num_rects)
			return OpStatus::OK;
		/*MDE_RECT mde_animArea = mde_view->m_region.rects[0];
		OpRect animArea = OpRect(mde_animArea.x, mde_animArea.y, mde_animArea.w, mde_animArea.h);
		VEGAOpPainter* p = (VEGAOpPainter*)view->GetPainter(animArea);
		if (p)
		{
			if (evt == EVT_LOAD_PAGE_BACK)
				p->prepareAnimation(animArea, VEGAOpPainter::ANIM_LOAD_PAGE_BACK);
			else if (evt == EVT_LOAD_PAGE_FWD)
				p->prepareAnimation(animArea, VEGAOpPainter::ANIM_LOAD_PAGE_FWD);
			else
				p->prepareAnimation(animArea, VEGAOpPainter::ANIM_LOAD_PAGE);
			view->ReleasePainter(OpRect());
		}*/
		OpBrowserView *pw = ((DesktopOpWindow*)window)->GetBrowserView();
		if (pw && (evt == EVT_LOAD_PAGE_BACK || evt == EVT_LOAD_PAGE_FWD || evt == EVT_LOAD_PAGE))
		{
			QuickAnimationBitmapWindow *anim_win = OP_NEW(QuickAnimationBitmapWindow, ());
			if (OpStatus::IsError(anim_win->Init(pw->GetParentDesktopWindow(), window, pw)))
				OP_DELETE(anim_win);
			else
			{
				anim_win->setCloseOnComplete(true);

				INT32 x, y;
				UINT32 w, h;
				anim_win->GetInnerPos(x, y);
				anim_win->GetInnerSize(w, h);
				anim_win->animateOpacity(1, 0);
				anim_win->animateIgnoreMouseInput(true, true);
				if (evt == EVT_LOAD_PAGE)
					anim_info.Set(anim_win, ANIM_CURVE_LINEAR, 300);
				else
				{
					anim_win->animateRect(OpRect(x, y, w, h), OpRect(x + INT32(evt == EVT_LOAD_PAGE_FWD ? -w : w)*1.1, y, w, h));
					anim_info.Set(anim_win, ANIM_CURVE_SLOW_DOWN, 600);
				}
			}
		}
	}
#endif // VEGA_OPPAINTER_ANIMATIONS
	return OpStatus::OK;
}

OP_STATUS QuickAnimationManager::startPageLoadAnimation(OpView *view, OpWindow *window)
{
#ifdef VEGA_OPPAINTER_ANIMATIONS
	if (GetEnabled())
	{
		MDE_View *mde_view = ((MDE_OpView*)view)->GetMDEWidget();
		if (!mde_view->m_region.num_rects)
			return OpStatus::OK;
		/*MDE_RECT mde_animArea = mde_view->m_region.rects[0];
		OpRect animArea = OpRect(mde_animArea.x, mde_animArea.y, mde_animArea.w, mde_animArea.h);
		VEGAOpPainter* p = (VEGAOpPainter*)view->GetPainter(animArea);
		if (p)
		{
			p->startAnimation(animArea);
			view->ReleasePainter(OpRect());
		}*/
		if (anim_info.anim_win)
		{
			anim_info.anim_win->setCachedWindow(window);
			g_animation_manager->startAnimation(anim_info.anim_win, anim_info.curve, anim_info.duration);
			anim_info.Reset();
		}
	}
#endif // VEGA_OPPAINTER_ANIMATIONS
	return OpStatus::OK;
}

void QuickAnimationManager::abortPageLoadAnimation()
{
#ifdef VEGA_OPPAINTER_ANIMATIONS
	if (anim_info.anim_win)
		anim_info.anim_win->Close(true);
#endif
}

void QuickAnimationManager::removeListenerFromAllAnimations(QuickAnimationListener* listener)
{
	QuickAnimation* nanim;
	for (QuickAnimation* anim = (QuickAnimation*)m_active_animations.First(); anim; anim = nanim)
	{
		nanim = (QuickAnimation*)anim->Suc();

		anim->RemoveIfListener(listener);
	}
}

void QuickAnimationManager::advanceAnimations()
{
	if (m_is_advancing_animations)
		return;

	double now = g_op_time_info->GetRuntimeMS();
	m_is_advancing_animations = true;

	bool end_all_animations = !GetEnabled();

	QuickAnimation* nanim;
	for (QuickAnimation* anim = (QuickAnimation*)m_active_animations.First(); anim; anim = nanim)
	{
		nanim = (QuickAnimation*)anim->Suc();

		if (anim->m_animation_start == 0)
			// start-time was delayed in startAnimation.
			anim->m_animation_start = now;

		double duration = now-anim->m_animation_start;
		if (duration >= anim->m_animation_duration || end_all_animations)
		{
			anim->OnAnimationUpdate(1.);
			anim->Out();
			anim->OnAnimationComplete();
		}
		else
		{
			double animpos = duration / anim->m_animation_duration;
			if (anim->m_animation_curve == ANIM_CURVE_BOUNCE)
			{
				double slow_down   = 1 - (1-animpos)*(1-animpos)*(1-animpos);
				double tiny_bounce = 0.06*(1-cos(2*M_PI*animpos));
				animpos = slow_down + tiny_bounce;
			}
			else for (unsigned segment = 0; segment < animationCurvePath[anim->m_animation_curve].getNumLines(); ++segment)
			{
				VEGA_FIX* segpos = animationCurvePath[anim->m_animation_curve].getLine(segment);
				if (segpos[VEGALINE_STARTX] <= animpos*1000. && segpos[VEGALINE_ENDX] >= animpos*1000.)
				{
					double len = segpos[VEGALINE_ENDX]-segpos[VEGALINE_STARTX];
					double pos = animpos*1000.-segpos[VEGALINE_STARTX];
					pos = pos/len;
					animpos = segpos[VEGALINE_STARTY]*(1-pos)+segpos[VEGALINE_ENDY]*pos;
					animpos/=1000.;
					break;
				}
			}
			anim->OnAnimationUpdate(animpos);
		}
	}
	m_is_advancing_animations = false;

	if (!m_active_animations.Empty() && !m_posted_msg)
	{
		g_main_message_handler->PostDelayedMessage(MSG_QUICK_ADVANCE_ANIMATION, (MH_PARAM_1)this, 0, 1);
		m_posted_msg = true;
	}
}

void QuickAnimationManager::startAnimation(QuickAnimationParams& params)
{
	QuickAnimationWidget* animation = OP_NEW(QuickAnimationWidget,
		(params.widget,
		 params.move_type,
		 true,
		 params.fade_type,
		 params.callback_param,
		 params.listener));

	if (animation)
	{
		if (params.start_rect.x      != -1 ||
		    params.start_rect.y      != -1 ||
		    params.start_rect.width  != -1 ||
		    params.start_rect.height != -1)
			params.widget->SetRect(params.start_rect);

		animation->SetEndRect(params.end_rect);

		g_animation_manager->startAnimation(
			animation,
			params.curve,
			params.duration * 1000.0,
			false);
	}
}

void QuickAnimationManager::startAnimation(QuickAnimation* anim, AnimationCurve curve, double duration, bool delay_start_time)
{
	duration = ValidateDuration(duration);

	if (m_active_animations.Empty())
	{
		// Start the animation loop
		if (!m_callback_set)
		{
			g_main_message_handler->SetCallBack(this, MSG_QUICK_ADVANCE_ANIMATION, (MH_PARAM_1)this);
			m_callback_set = true;
		}
		if (!m_posted_msg)
		{
			g_main_message_handler->PostDelayedMessage(MSG_QUICK_ADVANCE_ANIMATION, (MH_PARAM_1)this, 0, 0);
			m_posted_msg = true;
		}
	}

	if (!anim->InList())
		anim->Into(&m_active_animations);
	anim->m_animation_curve = curve;
	anim->m_animation_duration = duration;
	anim->m_animation_start = delay_start_time ? 0 : g_op_time_info->GetRuntimeMS();
	anim->OnAnimationStart();
}

void QuickAnimationManager::rescheduleAnimationFlip(QuickAnimation* anim, double new_duration)
{
	new_duration = ValidateDuration(new_duration);
	OP_ASSERT(anim->IsAnimating());
	double now = g_op_time_info->GetRuntimeMS();
	double duration = now - anim->m_animation_start;
	double position = duration / anim->m_animation_duration;
	position = MIN(1, position);
	anim->m_animation_duration = new_duration;
	anim->m_animation_start = now - anim->m_animation_duration * (1 - position);
}

void QuickAnimationManager::abortAnimation(QuickAnimation* anim)
{
	anim->Out();
}

void QuickAnimationManager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(msg == MSG_QUICK_ADVANCE_ANIMATION);
	m_posted_msg = false;
	advanceAnimations();
}

// == QuickDisableAnimationsHere =======================================================================================

QuickDisableAnimationsHere::QuickDisableAnimationsHere()
{
	if (g_animation_manager)
		g_animation_manager->m_disable_animations_counter++;
}

QuickDisableAnimationsHere::~QuickDisableAnimationsHere()
{
	if (g_animation_manager)
		g_animation_manager->m_disable_animations_counter--;
}

// == QuickAnimationWidget =============================================================================================

QuickAnimationWidget::QuickAnimationWidget(OpWidget *widget, AnimationMoveType move_type, bool horizontal, AnimationFadeType fade_type, int callback_param, QuickAnimationListener* listener)
	: m_widget(widget)
	, m_move_type(move_type)
	, m_fade_type(fade_type)
	, m_horizontal(horizontal)
	, m_hide_on_completion(false)
	, m_end_rect(-1, -1, -1, -1)
	, m_animation_ran_to_completion(false)
	, m_callback_param(callback_param)
	, m_listener(listener)
{
	m_widget->SetAnimation(this);
}

QuickAnimationWidget::~QuickAnimationWidget()
{
	if (m_move_type == ANIM_MOVE_RECT_TO_ORIGINAL)
		m_widget->SetFloating(false);

	// Reset animation only in case this is the current QuickAnimationWidget for m_widget
	if (m_widget->GetAnimation() == this)
		m_widget->ResetAnimation();

	if (m_hide_on_completion && !m_animation_ran_to_completion)
		m_widget->SetHidden(true);

	if(m_listener)
	{
		if (m_animation_ran_to_completion)
			m_listener->OnAnimationComplete(m_widget, m_callback_param);
		else
			m_listener->OnAnimationDestroyed(m_widget, m_callback_param);
	}
}

void QuickAnimationWidget::OnAnimationStart()
{
	m_curr_position = 0;
	m_start_rect = m_widget->GetRect();
	if (m_move_type == ANIM_MOVE_RECT_TO_ORIGINAL)
	{
		m_widget->SetOriginalRect(m_start_rect);
		m_widget->SetFloating(true);
	}
	if (m_fade_type == ANIM_FADE_IN)
		m_widget->SetVisibility(true);
}

void QuickAnimationWidget::OnAnimationUpdate(double position)
{
	if (!m_widget->GetParent())
		// Widget has no parent so we're not intrested in animating it any more.
		// This can happen if it's removed before animation is completed since it may still live after it has been removed.
		return;

	if (m_fade_type != ANIM_FADE_NONE)
	{
		UINT8 old_opacity = m_curr_position * 255;
		UINT8 new_opacity = position * 255;
		if (new_opacity != old_opacity)
			m_widget->InvalidateAll();
	}
	if (m_move_type == ANIM_MOVE_RECT_TO_ORIGINAL)
	{
		bool has_explicit_end_rect = false;

		if (m_end_rect.x      != -1 ||
		    m_end_rect.y      != -1 ||
		    m_end_rect.width  != -1 ||
		    m_end_rect.height != -1)
			has_explicit_end_rect = true;

		// The widgets originalrect will be the destination rectangle as soon as relayout has
		// been called at least once since it was set to floating.
		if (!has_explicit_end_rect && m_start_rect.Equals(m_widget->GetOriginalRect()))
		{
			((OpWidget*)m_widget)->GetParent()->Relayout(true, false);
		}
		else
		{
			OpRect curr_rect = m_widget->GetRect();
			OpRect end_rect  = has_explicit_end_rect ? m_end_rect : m_widget->GetOriginalRect();

			curr_rect.x      = m_start_rect.x      + (end_rect.x      - m_start_rect.x     ) * position;
			curr_rect.y      = m_start_rect.y      + (end_rect.y      - m_start_rect.y     ) * position;
			curr_rect.width  = m_start_rect.width  + (end_rect.width  - m_start_rect.width ) * position;
			curr_rect.height = m_start_rect.height + (end_rect.height - m_start_rect.height) * position;

			if (!curr_rect.Equals(m_widget->GetRect()))
			{
				m_widget->SetRect(curr_rect);
				m_widget->GetParent()->Relayout(true, false);
			}
		}
	}
	else if (m_move_type != ANIM_MOVE_NONE)
	{
		INT32 old_rw, old_rh;
		INT32 new_rw, new_rh;

		m_widget->GetRequiredSize(old_rw, old_rh);
		m_curr_position = position;
		m_widget->GetRequiredSize(new_rw, new_rh);

		// Fix for when OpThumbnailPagebar is in vertical layout (it overrides toolbar-layout and ignores widgets required size) so we will not know
		// if this animation step need a relayout or not.
		bool force_relayout = m_widget->GetParent() && m_widget->GetParent()->GetType() == OpTypedObject::WIDGET_TYPE_PAGEBAR && !((OpPagebar*)m_widget->GetParent())->IsHorizontal();

		if (((OpWidget*)m_widget)->GetParent() &&
			(force_relayout || (old_rw != new_rw) || (old_rh != new_rh)))
			((OpWidget*)m_widget)->GetParent()->Relayout(true, false);
	}

	m_curr_position = position;
}

void QuickAnimationWidget::OnAnimationComplete()
{
	m_animation_ran_to_completion = true;

	if (m_fade_type == ANIM_FADE_OUT)
		m_widget->SetVisibility(false);

	if (m_hide_on_completion)
		m_widget->SetHidden(true);

	OP_DELETE(this);
}

void QuickAnimationWidget::GetCurrentValue(int &default_width, int &default_height)
{
	if (m_move_type == ANIM_MOVE_RECT_TO_ORIGINAL)
		return;

	int &value = m_horizontal ? default_width : default_height;
	if (m_move_type == ANIM_MOVE_GROW)
		value = value * m_curr_position;
	else if (m_move_type == ANIM_MOVE_SHRINK)
		value = value * (1 - m_curr_position);

	// Not make sense for ANIM_MOVE_RECT_TO_ORIGINAL
}

UINT8 QuickAnimationWidget::GetOpacity()
{
	if (m_fade_type == ANIM_FADE_IN)
		return 255 * m_curr_position;
	else if (m_fade_type == ANIM_FADE_OUT)
		return 255 * (1 - m_curr_position);
	return 255;
}

// == QuickAnimationWindowObject =============================================================================================

OP_STATUS QuickAnimationWindowObject::Init(DesktopWindow *animation_window, OpWidget *animation_widget, OpWindow *animation_op_window)
{
	m_animation_window = animation_window;
	m_animation_widget = animation_widget;
	m_animation_op_window = animation_op_window ? animation_op_window : m_animation_window->GetOpWindow();

	return m_animation_window ? m_animation_window->AddListener(this) : OpStatus::OK;
}

QuickAnimationWindowObject::~QuickAnimationWindowObject()
{
	if (m_animation_window)
		m_animation_window->RemoveListener(this);
	if (m_cached_window)
		((MDE_OpWindow*)m_cached_window)->SetCached(false);
}

void QuickAnimationWindowObject::OnAnimationStart()
{
	OnAnimationUpdate(0);
	if (m_ignore_mouse_input_on)
		((MDE_OpWindow*)m_animation_op_window)->SetIgnoreMouseInput(m_iminp_s);
}

void QuickAnimationWindowObject::OnAnimationUpdate(double position)
{
	if (m_opacity_on)
		m_animation_op_window->SetTransparency((m_opacity_s + (m_opacity_t - m_opacity_s) * position) * 255.0);
	if (m_rect_on)
	{
		OpRect r = m_rect_s;
		r.x += (m_rect_t.x - m_rect_s.x) * position;
		r.y += (m_rect_t.y - m_rect_s.y) * position;
		r.width += (m_rect_t.width - m_rect_s.width) * position;
		r.height += (m_rect_t.height - m_rect_s.height) * position;
		if (m_animation_widget)
		{
			m_animation_widget->SetRect(r);
		}
		else
		{
			((DesktopOpWindow*)m_animation_op_window)->SetInnerPosAndSize(r.x, r.y, r.width, r.height);
		}
	}
}

void QuickAnimationWindowObject::OnAnimationComplete()
{
	if (m_ignore_mouse_input_on)
		((MDE_OpWindow*)m_animation_op_window)->SetIgnoreMouseInput(m_iminp_t);
	if (m_hide_when_completed)
		m_animation_op_window->Show(false);
}

void QuickAnimationWindowObject::reset()
{
	if (m_opacity_on)
		m_animation_op_window->SetTransparency(255);
	m_hide_when_completed = false;
	m_opacity_on = false;
	m_rect_on = false;
	m_ignore_mouse_input_on = false;
}

void QuickAnimationWindowObject::animateOpacity(double source, double target)
{
	m_opacity_on = true;
	m_opacity_s = source;
	m_opacity_t = target;
}

void QuickAnimationWindowObject::animateRect(const OpRect &source, const OpRect &target)
{
	m_rect_on = true;
	m_rect_s = source;
	m_rect_t = target;
}

void QuickAnimationWindowObject::animateIgnoreMouseInput(bool source, bool target)
{
	m_ignore_mouse_input_on = true;
	m_iminp_s = source;
	m_iminp_t = target;
}

void QuickAnimationWindowObject::setCachedWindow(OpWindow *cached_window)
{
	OP_ASSERT(!m_cached_window);
	m_cached_window = cached_window;
	((MDE_OpWindow*)m_cached_window)->SetCached(true);
}

void QuickAnimationWindowObject::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	OP_DELETE(this);
}

void QuickAnimationWindowObject::OnDesktopWindowAnimationRectChanged(DesktopWindow* desktop_window, const OpRect &source, const OpRect &target)
{
	if (m_rect_on)
		animateRect(source, target);
}


// == QuickAnimationWindow =============================================================================================

OP_STATUS QuickAnimationWindow::Init(DesktopWindow* parent_window, OpWindow* parent_op_window, OpWindow::Style style, UINT32 effects)
{
	RETURN_IF_ERROR(DesktopWindow::Init(style, parent_window, parent_op_window, effects));

	GetWidgetContainer()->GetRoot()->SetListener(this);

	RETURN_IF_ERROR(QuickAnimationWindowObject::Init(this, NULL));

	return OpStatus::OK;
}

void QuickAnimationWindow::OnAnimationComplete()
{
	QuickAnimationWindowObject::OnAnimationComplete();
	if (m_close_on_complete)
	{
		Show(false);
		Close(false);
	}
}

void QuickAnimationWindow::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	// Leave empty to override QuickAnimationWindowObject, which closes the
	// window.  QuickAnimationWindow is a DesktopWindow and is therefore closed
	// before the parent DesktopWindow automatically.
}

// == QuickAnimationBitmapWindow =======================================================================================

QuickAnimationBitmapWindow::~QuickAnimationBitmapWindow()
{
	OP_DELETE(bitmap);
	if (this == g_animation_manager->anim_info.anim_win)
		g_animation_manager->anim_info.Reset();
}

OP_STATUS QuickAnimationBitmapWindow::Init(DesktopWindow* parent_window, OpWindow* source_window, OpWidget* source_widget)
{
	INT32 pos_x = 0, pos_y = 0;
	UINT32 effects = 0;

	if (source_window)
	{
		bitmap = ((MDE_OpWindow*)source_window)->CreateBitmapFromWindow();
		if (!bitmap)
			return OpStatus::ERR;

		// Assume OpWindow::EFFECT_TRANSPARENT for now, most windows are transparent and it's
		// not yet possible to get if a native Window is transparent or not. GetEffects and 
		// GetTransparency should probably be added to OpWindow.
		effects = ((MDE_OpWindow*)source_window)->GetEffects() | OpWindow::EFFECT_TRANSPARENT;
		source_window->GetInnerPos(&pos_x, &pos_y);

		/*MDE_View *mde_view = ((MDE_OpWindow*)source_window)->GetMDEWidget();
		MDE_RECT mde_animArea = MDE_MakeRect(0, 0, mde_view->m_rect.w, mde_view->m_rect.h);
		mde_view->ConvertToScreen(mde_animArea.x, mde_animArea.y);

		OpRect animArea = OpRect(mde_animArea.x, mde_animArea.y, mde_animArea.w, mde_animArea.h);
		VEGAOpPainter* p = (VEGAOpPainter*)(mde_view->m_screen)->GetVegaPainter();
		p->SetVegaTranslation(0, 0);
		if (p)
		{
			bitmap = p->CreateBitmapFromBackground(OpRect(mde_animArea.x, mde_animArea.y, mde_animArea.w, mde_animArea.h));
		}
		if (!bitmap)
			return OpStatus::ERR;

		source_window->GetInnerPos(&pos_x, &pos_y);*/
	}
	else
	{
		VisualDevice *vis_dev = source_widget->GetVisualDevice();
		if (!vis_dev)
			return OpStatus::ERR;

		RETURN_IF_ERROR(OpBitmap::Create(&bitmap, source_widget->GetRect().width, source_widget->GetRect().height, false, true, 0, 0, true));

		OpPainter *painter = bitmap->GetPainter();
		if (!painter)
			return OpStatus::ERR;

		OpRect rect = source_widget->GetRect(true);
		OpRect paint_rect(0, 0, rect.width, rect.height);
		vis_dev->BeginPaint(painter, rect, paint_rect);
			vis_dev->Translate(rect.x, rect.y);
				source_widget->GenerateOnPaint(rect);
			vis_dev->Translate(-rect.x, -rect.y);
		vis_dev->EndPaint();

		bitmap->ReleasePainter();

		if (!source_widget->GetParent() && source_widget->GetParentOpWindow())
		{
			source_window = source_widget->GetParentOpWindow();
			effects = ((MDE_OpWindow*)source_window)->GetEffects();
			source_window->GetInnerPos(&pos_x, &pos_y);
		}
	}

	RETURN_IF_ERROR(QuickAnimationWindow::Init(parent_window, parent_window->GetOpWindow(), OpWindow::STYLE_TRANSPARENT, effects));
	// Animation windows shouldn't have focus
	((MDE_OpWindow*)GetOpWindow())->SetAllowAsActive(false);

	OpRect rect(pos_x, pos_y, bitmap->Width(), bitmap->Height());
	Show(true, &rect);
	return OpStatus::OK;
}

void QuickAnimationBitmapWindow::OnAnimationComplete()
{
	m_close_on_complete = true;
	QuickAnimationWindow::OnAnimationComplete();
}

void QuickAnimationBitmapWindow::OnPaint(OpWidget *widget, const OpRect &paint_rect)
{
	widget->GetVisualDevice()->BlitImage(bitmap, OpRect(0, 0, bitmap->Width(), bitmap->Height()), OpRect(0, 0, bitmap->Width(), bitmap->Height()));
}

// == QuickAnimationListener =======================================================================================

QuickAnimationListener::~QuickAnimationListener()
{
	if (g_animation_manager)
		g_animation_manager->removeListenerFromAllAnimations(this);
}

#endif // VEGA_OPPAINTER_SUPPORT
