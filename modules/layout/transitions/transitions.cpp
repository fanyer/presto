/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/** @file transitions.cpp
 *
 * Implementation of CSS Transitions between computed values.
 *
 */

#include "core/pch.h"

#ifdef CSS_TRANSITIONS

#include "modules/doc/frm_doc.h"
#include "modules/layout/cascade.h"
#include "modules/layout/layout_workplace.h"
#include "modules/layout/transitions/transitions.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/style/src/css_aliases.h"

/* static */ TransitionTimingFunction*
TransitionTimingFunction::Make(const CSS_generic_value* timing_value)
{
	double bezier[CubicBezierTimingFunction::BEZIER_ARRAY_SIZE] = { 0.25, 0.1, 0.25, 1.0 };

	if (timing_value)
	{
		if (timing_value->GetValueType() == CSS_INT_NUMBER)
		{
			return OP_NEW(StepTimingFunction, (timing_value[0].GetNumber(), timing_value[1].GetType() == CSS_VALUE_start));
		}
		else
		{
			bezier[CubicBezierTimingFunction::X1] = timing_value[CubicBezierTimingFunction::X1].GetReal();
			bezier[CubicBezierTimingFunction::Y1] = timing_value[CubicBezierTimingFunction::Y1].GetReal();
			bezier[CubicBezierTimingFunction::X2] = timing_value[CubicBezierTimingFunction::X2].GetReal();
			bezier[CubicBezierTimingFunction::Y2] = timing_value[CubicBezierTimingFunction::Y2].GetReal();

			/* if the coefficients of the Bezier_x(t) and Bezier_y(t) are the same, the Bezier curve is linear.
			   Optimization to avoid any Newton-Raphson calculations in that case. */
			if (bezier[CubicBezierTimingFunction::X1] == bezier[CubicBezierTimingFunction::Y1] &&
				bezier[CubicBezierTimingFunction::X2] == bezier[CubicBezierTimingFunction::Y2])
				return OP_NEW(LinearTimingFunction, ());
		}
	}

	return OP_NEW(CubicBezierTimingFunction, (bezier));
}

CubicBezierTimingFunction::CubicBezierTimingFunction(const double bezier[BEZIER_ARRAY_SIZE])
{
	m_cubic_coeff[AX] = 3*(bezier[X1]-bezier[X2])+1;
	m_cubic_coeff[BX] = 3*(bezier[X2]-2*bezier[X1]);
	m_cubic_coeff[CX] = 3*bezier[X1];

	m_cubic_coeff[AY] = 3*(bezier[Y1]-bezier[Y2])+1;
	m_cubic_coeff[BY] = 3*(bezier[Y2]-2*bezier[Y1]);
	m_cubic_coeff[CY] = 3*bezier[Y1];

	m_cubic_coeff[ADX] = 3*m_cubic_coeff[AX];
	m_cubic_coeff[BDX] = 2*m_cubic_coeff[BX];
}

#define BEZIER_MAX_DEV 1e-4
#define BEZIER_MIN_SLOPE 1e-6

double
CubicBezierTimingFunction::Call(double x)
{
	OP_ASSERT(x >= 0 && x <= 1);

	/* Newton-Raphson used to find the next t with max deviation of
	   +-BEZIER_MAX_DEV. */

	double t = x;

	for (int i=0; i < 10; i++)
	{
		double dx = Bezier_x(t) - x;

		if (op_fabs(dx) < BEZIER_MAX_DEV)
		{
			/* We're looking for a solution for t in [0, 1]. Also, this should
			   make sure we end up with a return value for y in [0, 1]. */
			if (t < 0)
				t = 0;
			else if (t > 1)
				t = 1;

			return Bezier_y(t);
		}

		double dx_dt = Bezier_x_der(t);

		/* Avoid overflow */
		if (dx_dt < BEZIER_MIN_SLOPE)
			break;

		t = t - dx / dx_dt;
	}

	/* We end up here if the derivative slope is not steep enough or
	   Newton-Raphson doesn't converge. Try bisection instead. */

	double t_min = 0;
	double t_max = 1;
	t = x;

	while (t_min < t_max)
	{
		double dx = Bezier_x(t) - x;
		if (op_fabs(dx) < BEZIER_MAX_DEV)
			break;
		if (dx > 0)
			t_max = t;
		else
			t_min = t;
		t = t_min + 0.5*(t_max - t_min);
	}

	return Bezier_y(t);
}

/* virtual */ BOOL
CubicBezierTimingFunction::IsSameTimingFunction(const CSS_generic_value* timing_value) const
{
	double bezier[CubicBezierTimingFunction::BEZIER_ARRAY_SIZE] = { 0.25, 0.1, 0.25, 1.0 };

	if (timing_value)
	{
		if (timing_value[CubicBezierTimingFunction::X1].GetValueType() != CSS_NUMBER)
			return FALSE;

		bezier[CubicBezierTimingFunction::X1] = timing_value[CubicBezierTimingFunction::X1].GetReal();
		bezier[CubicBezierTimingFunction::Y1] = timing_value[CubicBezierTimingFunction::Y1].GetReal();
		bezier[CubicBezierTimingFunction::X2] = timing_value[CubicBezierTimingFunction::X2].GetReal();
		bezier[CubicBezierTimingFunction::Y2] = timing_value[CubicBezierTimingFunction::Y2].GetReal();
	}
	return	m_cubic_coeff[CX] == 3*bezier[CubicBezierTimingFunction::X1] &&
			m_cubic_coeff[CY] == 3*bezier[CubicBezierTimingFunction::Y1] &&
			m_cubic_coeff[AX]*2 + m_cubic_coeff[BX] == 2-3*bezier[CubicBezierTimingFunction::X2] &&
			m_cubic_coeff[AY]*2 + m_cubic_coeff[BY] == 2-3*bezier[CubicBezierTimingFunction::Y2];
}

void
PropertyTransition::Reverse(double runtime_ms)
{
	OP_NEW_DBG("PropertyTransition::Reverse", "transitions");

	/* The remaining time of transition will be how far into the
	   reverse transition we are. */
	double remaining_ms = m_duration_ms - (runtime_ms - GetStartMS());

	/* runtime_ms is past the time the transition should've finished,
	   but the the last time Animate was run, it hadn't. Pretend we're
	   exactly at the end, but haven't triggered the transitionend event.
	   We'll keep transitionend triggering within Animate for simplicity.
	   Since the time computed style changes is not defined, it should
	   not be a spec violation. */
	if (remaining_ms < 0)
		remaining_ms = 0;

	OP_DBG(("Before: remaining_ms=%lf, m_duration_ms=%lf, runtime_ms=%lf, m_trigger_ms=%lf", remaining_ms, m_duration_ms, runtime_ms, m_trigger_ms));

	/* Adjust the trigger time to be (delay + remaining time) before
	   the current elapsed time. */
	m_trigger_ms = runtime_ms - m_delay_ms - remaining_ms;

	OP_DBG(("After: m_trigger_ms=%lf", m_trigger_ms));

	m_reverse = !m_reverse;
}

OP_STATUS
PropertyTransition::TransitionEnd(FramesDocument *doc, HTML_Element* elm)
{
	if (m_callback)
		return m_callback->TransitionEnd(doc, elm);
	else
	{
		RETURN_IF_ERROR(doc->ConstructDOMEnvironment());

		DOM_Environment* dom_env = doc->GetDOMEnvironment();
		if (dom_env)
		{
			DOM_Environment::EventData data;
			data.type = TRANSITIONEND;
			data.target = elm;
			data.elapsed_time = GetDurationMS() / 1000.0;
			data.css_property = GetProperty();
			RETURN_IF_MEMORY_ERROR(dom_env->HandleEvent(data));
		}

		return OpStatus::OK;
	}
}

void
PropertyTransition::UpdateState(double runtime_ms)
{
	if (!m_callback)
		/* When we have no callback, the state is always ACTIVE */

		return;

	if (m_state == TransitionCallback::PAUSED)
	{
		m_trigger_ms += runtime_ms - m_paused_ms;
		m_paused_ms = runtime_ms;
	}

	TransitionCallback::StateData data;
	TransitionCallback::TransitionState new_state = m_callback->GetCurrentState(data);

	if (new_state != m_state)
	{
		OP_ASSERT(m_state != TransitionCallback::STOPPED);

		if (new_state == TransitionCallback::PAUSED)
			m_paused_ms = data.paused_runtime_ms;
		else
			if (new_state == TransitionCallback::STOPPED)
				m_end = m_start; // Force end

		m_state = new_state;
	}
}

/* virtual */ int
ColorTransition::Animate(double runtime_ms)
{
	COLORREF cur_col = InterpolatePremultipliedRGBA(m_from, m_to, CallTimingFunction(runtime_ms));
	if (cur_col != m_current)
	{
		m_current = cur_col;
		return PROPS_CHANGED_UPDATE;
	}
	else
		return PROPS_CHANGED_NONE;
}

/* virtual */ void
ColorTransition::GetTransitionValue(HTMLayoutProperties& props) const
{
	switch (GetProperty())
	{
	case CSS_PROPERTY_color:
		props.font_color = m_current;
		props.transition_packed.font_color = 1;
		break;
	case CSS_PROPERTY_background_color:
		props.bg_color = m_current;
		props.transition_packed.bg_color = 1;
		break;
	case CSS_PROPERTY_border_top_color:
		props.border.top.color = m_current;
		props.transition_packed.border_top_color = 1;
		break;
	case CSS_PROPERTY_border_left_color:
		props.border.left.color = m_current;
		props.transition_packed.border_left_color = 1;
		break;
	case CSS_PROPERTY_border_right_color:
		props.border.right.color = m_current;
		props.transition_packed.border_right_color = 1;
		break;
	case CSS_PROPERTY_border_bottom_color:
		props.border.bottom.color = m_current;
		props.transition_packed.border_bottom_color = 1;
		break;
	case CSS_PROPERTY_outline_color:
		props.outline.color = m_current;
		props.transition_packed2.outline_color = 1;
		break;
	case CSS_PROPERTY_column_rule_color:
		props.column_rule.color = m_current;
		props.transition_packed2.column_rule_color = 1;
		break;
	case CSS_PROPERTY_selection_color:
		props.selection_color = m_current;
		props.transition_packed.selection_color = 1;
		break;
	case CSS_PROPERTY_selection_background_color:
		props.selection_bgcolor = m_current;
		props.transition_packed.selection_bgcolor = 1;
		break;
	default:
		OP_ASSERT(!"Unhandled property in ColorTransition::GetTransitionValue");
		break;
	}
}

/* static */ OP_STATUS
ColorTransition::UpdateTransition(const TransitionDescription& description,
								  PropertyTransition*& trans,
								  COLORREF from,
								  COLORREF to,
								  double runtime_ms)
{
	OP_NEW_DBG("ColorTransition::UpdateTransition", "transitions");

	from = GetRGBA(from);
	to = GetRGBA(to);

	if (from == USE_DEFAULT_COLOR || to == USE_DEFAULT_COLOR)
	{
		/* The to or from color value is not transitionable.
		   Set both to USE_DEFAULT_COLOR to skip starting a new transition. */
		from = USE_DEFAULT_COLOR;
		to = USE_DEFAULT_COLOR;
	}

	if (trans)
	{
		/* Transition in progress. */
		ColorTransition* color_trans = static_cast<ColorTransition*>(trans);
		PropertyTransition::StateChange change = color_trans->NewToValue(to, runtime_ms, description.timing);
		if (change == TRANSITION_REPLACE)
		{
			/* Delete transition in progress. Might be replaced by a new one
			   further down. */
			OP_DBG(("TRANSITION_REPLACE"));
			trans->Out();
			OP_DELETE(trans);
			trans = NULL;
		}
		else
		{
			/* NewToValue either found that the new end value matched the
			   current one, or it matched the start value of the current
			   transition and reversed the transition. */
			OP_ASSERT(change == TRANSITION_KEEP || change == TRANSITION_REVERSE);
			return OpStatus::OK;
		}
	}

	OP_DBG(("from=%ld, to=%ld"));

	if (description.timing.DoStart() && from != to || description.callback != NULL)
	{
		OP_DBG(("DoStart()"));
		TransitionTimingFunction* timing_func = TransitionTimingFunction::Make(description.timing.timing_value);
		if (!timing_func)
			return OpStatus::ERR_NO_MEMORY;

		ColorTransition* color_trans = OP_NEW(ColorTransition, (description, runtime_ms, timing_func, from, to));
		if (color_trans)
			trans = color_trans;
		else
		{
			OP_DELETE(timing_func);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

PropertyTransition::StateChange
ColorTransition::NewToValue(COLORREF to, double runtime_ms, const TransitionTimingData& timing)
{
	if (to == USE_DEFAULT_COLOR)
		return  TRANSITION_REPLACE;

	if (to == (m_reverse ? m_from : m_to))
		return TRANSITION_KEEP;
	else
		if (to == (m_reverse ? m_to : m_from) && SameTiming(timing))
		{
			Reverse(runtime_ms);
			return TRANSITION_REVERSE;
		}
		else
			return TRANSITION_REPLACE;
}

/* virtual */ int
LengthTransition::Animate(double runtime_ms)
{
	int changes = PROPS_CHANGED_NONE;
	double percentage = CallTimingFunction(runtime_ms);
	LayoutCoord cur = LayoutCoord(static_cast<int>(OpRound(m_from + (m_to - m_from)*percentage)));
	if (cur != m_current)
	{
		m_current = cur;
		switch (GetProperty())
		{
		case CSS_PROPERTY_column_rule_width:
			changes |= PROPS_CHANGED_UPDATE;
			break;
		case CSS_PROPERTY_outline_width:
		case CSS_PROPERTY_outline_offset:
			changes |= PROPS_CHANGED_BOUNDS;
			break;
		case CSS_PROPERTY_letter_spacing:
			changes |= PROPS_CHANGED_REMOVE_CACHED_TEXT;
			/* fall through */
		default:
			changes |= PROPS_CHANGED_SIZE;
			break;
		}
	}
	return changes;
}

/* virtual */ void
LengthTransition::GetTransitionValue(HTMLayoutProperties& props) const
{
	switch (GetProperty())
	{
	case CSS_PROPERTY_width:
		props.content_width = m_current;
		props.transition_packed2.width = 1;
		break;
	case CSS_PROPERTY_height:
		props.content_height = m_current;
		props.transition_packed2.height = 1;
		break;
	case CSS_PROPERTY_left:
		props.left = m_current;
		props.transition_packed2.left = 1;
		break;
	case CSS_PROPERTY_right:
		props.right = m_current;
		props.transition_packed2.right = 1;
		break;
	case CSS_PROPERTY_top:
		props.top = m_current;
		props.transition_packed2.top = 1;
		break;
	case CSS_PROPERTY_bottom:
		props.bottom = m_current;
		props.transition_packed.bottom = 1;
		break;
	case CSS_PROPERTY_min_width:
		props.min_width = m_current;
		props.transition_packed2.min_width = 1;
		break;
	case CSS_PROPERTY_min_height:
		props.min_height = m_current;
		props.transition_packed2.min_height = 1;
		break;
	case CSS_PROPERTY_max_width:
		props.max_width = m_current;
		props.transition_packed2.max_width = 1;
		break;
	case CSS_PROPERTY_max_height:
		props.max_height = m_current;
		props.transition_packed2.max_height = 1;
		break;
	case CSS_PROPERTY_border_top_width:
		props.border.top.width = static_cast<short>(m_current);
		props.transition_packed.border_top_width = 1;
		break;
	case CSS_PROPERTY_border_bottom_width:
		props.border.bottom.width = static_cast<short>(m_current);
		props.transition_packed.border_bottom_width = 1;
		break;
	case CSS_PROPERTY_border_left_width:
		props.border.left.width = static_cast<short>(m_current);
		props.transition_packed.border_left_width = 1;
		break;
	case CSS_PROPERTY_border_right_width:
		props.border.right.width = static_cast<short>(m_current);
		props.transition_packed.border_right_width = 1;
		break;
	case CSS_PROPERTY_margin_left:
		props.margin_left = m_current;
		props.transition_packed2.margin_left = 1;
		break;
	case CSS_PROPERTY_margin_right:
		props.margin_right = m_current;
		props.transition_packed2.margin_right = 1;
		break;
	case CSS_PROPERTY_margin_top:
		props.margin_top = m_current;
		props.transition_packed2.margin_top = 1;
		break;
	case CSS_PROPERTY_margin_bottom:
		props.margin_bottom = m_current;
		props.transition_packed2.margin_bottom = 1;
		break;
	case CSS_PROPERTY_padding_left:
		props.padding_left = m_current;
		props.transition_packed2.padding_left = 1;
		break;
	case CSS_PROPERTY_padding_right:
		props.padding_right = m_current;
		props.transition_packed2.padding_right = 1;
		break;
	case CSS_PROPERTY_padding_top:
		props.padding_top = m_current;
		props.transition_packed2.padding_top = 1;
		break;
	case CSS_PROPERTY_padding_bottom:
		props.padding_bottom = m_current;
		props.transition_packed2.padding_bottom = 1;
		break;
	case CSS_PROPERTY_outline_width:
		props.outline.width = static_cast<short>(m_current);
		props.transition_packed2.outline_width = 1;
		break;
	case CSS_PROPERTY_outline_offset:
		props.outline_offset = static_cast<short>(m_current);
		props.transition_packed2.outline_offset = 1;
		break;
	case CSS_PROPERTY_letter_spacing:
		props.letter_spacing = static_cast<short>(m_current);
		props.transition_packed.letter_spacing = 1;
		break;
	case CSS_PROPERTY_vertical_align:
		props.vertical_align_type = 0;
		props.vertical_align = static_cast<short>(m_current);
		props.transition_packed2.valign = 1;
		break;
	case CSS_PROPERTY_text_indent:
		props.text_indent = static_cast<short>(m_current);
		props.transition_packed.text_indent = 1;
		break;
	case CSS_PROPERTY_column_gap:
		props.column_gap = m_current;
		props.transition_packed.column_gap = 1;
		break;
	case CSS_PROPERTY_column_width:
		props.column_width = m_current;
		props.transition_packed2.column_width = 1;
		break;
	case CSS_PROPERTY_column_rule_width:
		props.column_rule.width = static_cast<short>(m_current);
		props.transition_packed2.column_rule_width = 1;
		break;
	case CSS_PROPERTY_flex_basis:
		props.flex_basis = m_current;
		props.transition_packed3.flex_basis = 1;
		break;
	default:
		OP_ASSERT(!"Unhandled property in LengthTransition::GetTransitionValue");
		break;
	}
}

/* static */ OP_STATUS
LengthTransition::UpdateTransition(const TransitionDescription& description,
								   PropertyTransition*& trans,
								   LayoutCoord from,
								   LayoutCoord to,
								   double runtime_ms,
								   BOOL percentage)
{
	if (trans)
	{
		/* Transition in progress. */
		LengthTransition* len_trans = static_cast<LengthTransition*>(trans);
		OP_ASSERT(len_trans->IsPercentage() == percentage);
		PropertyTransition::StateChange change = len_trans->NewToValue(to, runtime_ms, description.timing);
		if (change == TRANSITION_REPLACE)
		{
			/* Delete transition in progress. Might be replaced by a new one
			   further down. */
			trans->Out();
			OP_DELETE(trans);
			trans = NULL;
		}
		else
		{
			/* NewToValue either found that the new end value matched the
			   current one, or it matched the start value of the current
			   transition and reversed the transition. */
			OP_ASSERT(change == TRANSITION_KEEP || change == TRANSITION_REVERSE);
			return OpStatus::OK;
		}
	}

	if (description.timing.DoStart() && from != to || description.callback != NULL)
	{
		TransitionTimingFunction* timing_func = TransitionTimingFunction::Make(description.timing.timing_value);
		if (!timing_func)
			return OpStatus::ERR_NO_MEMORY;

		LengthTransition* len_trans;
		if (percentage)
			len_trans = OP_NEW(PercentageTransition, (description, runtime_ms, timing_func, from, to));
		else
			len_trans = OP_NEW(LengthTransition, (description, runtime_ms, timing_func, from, to));

		if (len_trans)
			trans = len_trans;
		else
		{
			OP_DELETE(timing_func);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

PropertyTransition::StateChange
LengthTransition::NewToValue(LayoutCoord to, double runtime_ms, const TransitionTimingData& timing)
{
	if (to == (m_reverse ? m_from : m_to))
		return TRANSITION_KEEP;
	else
		if (to == (m_reverse ? m_to : m_from) && SameTiming(timing))
		{
			Reverse(runtime_ms);
			return TRANSITION_REVERSE;
		}
		else
			return TRANSITION_REPLACE;
}

/* virtual */ void
PercentageTransition::GetTransitionValue(HTMLayoutProperties& props) const
{
	switch (GetProperty())
	{
	case CSS_PROPERTY_width:
		props.content_width = -m_current;
		props.transition_packed2.width = 1;
		break;
	case CSS_PROPERTY_height:
		props.content_height = -m_current;
		props.SetHeightIsPercent();
		props.transition_packed2.height = 1;
		break;
	case CSS_PROPERTY_flex_basis:
		props.flex_basis = -m_current;
		props.transition_packed3.flex_basis = 1;
		break;
	default:
		OP_ASSERT(!"Unhandled property in PercentageTransition::GetTransitionValue");
		break;
	}
}

/* virtual */ int
Length2Transition::Animate(double runtime_ms)
{
	double percentage = CallTimingFunction(runtime_ms);

	LayoutCoord cur[2] = {
		LayoutCoord(static_cast<int>(OpRound(m_from[0] + (m_to[0] - m_from[0])*percentage))),
		LayoutCoord(static_cast<int>(OpRound(m_from[1] + (m_to[1] - m_from[1])*percentage)))
	};

	if (cur[0] != m_current[0] ||
		cur[1] != m_current[1])
	{
		m_current[0] = cur[0];
		m_current[1] = cur[1];

		switch (GetProperty())
		{
		case CSS_PROPERTY_border_spacing:
			return PROPS_CHANGED_SIZE;

		case CSS_PROPERTY__o_object_position:
			return PROPS_CHANGED_SIZE | PROPS_CHANGED_UPDATE;

# ifdef CSS_TRANSFORMS
		case CSS_PROPERTY_transform_origin:
			return PROPS_CHANGED_BOUNDS;
# endif // CSS_TRANSFORMS

		case CSS_PROPERTY_border_bottom_left_radius:
		case CSS_PROPERTY_border_bottom_right_radius:
		case CSS_PROPERTY_border_top_left_radius:
		case CSS_PROPERTY_border_top_right_radius:
			return PROPS_CHANGED_UPDATE;
		}
	}

	return PROPS_CHANGED_NONE;
}

/* virtual */ void
Length2Transition::GetTransitionValue(HTMLayoutProperties& props) const
{
	switch (GetProperty())
	{
	case CSS_PROPERTY_border_spacing:
		props.border_spacing_horizontal = static_cast<short>(m_current[0]);
		props.border_spacing_horizontal = static_cast<short>(m_current[1]);
		props.transition_packed.border_spacing = 1;
		break;

	case CSS_PROPERTY__o_object_position:
		props.object_fit_position.x = m_current[0];
		props.object_fit_position.y = m_current[1];
		props.object_fit_position.x_percent = m_packed.is_percent1;
		props.object_fit_position.y_percent = m_packed.is_percent2;
		props.transition_packed2.object_position = 1;
		break;

# ifdef CSS_TRANSFORMS
	case CSS_PROPERTY_transform_origin:
		props.transform_origin_x = m_current[0];
		props.transform_origin_y = m_current[1];
		props.info2.transform_origin_x_is_percent = props.info2.transform_origin_x_is_decimal = !!m_packed.is_percent1;
		props.info2.transform_origin_y_is_percent = props.info2.transform_origin_y_is_decimal = !!m_packed.is_percent2;
		props.transition_packed2.transform_origin = 1;
		break;
# endif // CSS_TRANSFORMS

	case CSS_PROPERTY_border_bottom_left_radius:
		props.border.bottom.SetRadiusStart(static_cast<short>(m_current[0]), m_current[0] < 0);
		props.border.left.SetRadiusEnd(static_cast<short>(m_current[1]), m_current[1] < 0);
		props.transition_packed.border_bottom_left_radius = 1;
		break;

	case CSS_PROPERTY_border_bottom_right_radius:
		props.border.bottom.SetRadiusEnd(static_cast<short>(m_current[0]), m_current[0] < 0);
		props.border.right.SetRadiusEnd(static_cast<short>(m_current[1]), m_current[1] < 0);
		props.transition_packed.border_bottom_right_radius = 1;
		break;

	case CSS_PROPERTY_border_top_left_radius:
		props.border.top.SetRadiusStart(static_cast<short>(m_current[0]), m_current[0] < 0);
		props.border.left.SetRadiusStart(static_cast<short>(m_current[1]), m_current[1] < 0);
		props.transition_packed.border_top_left_radius = 1;
		break;

	case CSS_PROPERTY_border_top_right_radius:
		props.border.top.SetRadiusEnd(static_cast<short>(m_current[0]), m_current[0] < 0);
		props.border.right.SetRadiusStart(static_cast<short>(m_current[1]), m_current[1] < 0);
		props.transition_packed.border_top_right_radius = 1;
		break;
	}
}

/* static */ OP_STATUS
Length2Transition::UpdateTransition(const TransitionDescription& description,
									PropertyTransition*& trans,
									const LayoutCoord from[2],
									const LayoutCoord to[2],
									BOOL percent_from[2],
									BOOL percent_to[2],
									double runtime_ms)
{
	BOOL compatible = percent_from[0] == percent_to[0] && percent_from[1] == percent_to[1];

	if (trans)
	{
		/* Transition in progress. */

		PropertyTransition::StateChange change = TRANSITION_REPLACE;

		if (compatible)
			change = static_cast<Length2Transition*>(trans)->NewToValue(to, runtime_ms, description.timing);

		if (change == TRANSITION_REPLACE)
		{
			/* Delete transition in progress. Might be replaced by a new one
			   further down. */
			trans->Out();
			OP_DELETE(trans);
			trans = NULL;
		}
		else
		{
			/* NewToValue either found that the new end value matched the
			   current one, or it matched the start value of the current
			   transition and reversed the transition. */
			OP_ASSERT(change == TRANSITION_KEEP || change == TRANSITION_REVERSE);
			return OpStatus::OK;
		}
	}

	if (compatible && description.timing.DoStart() && (from[0] != to[0] || from[1] != to[1]) || description.callback != NULL)
	{
		TransitionTimingFunction* timing_func = TransitionTimingFunction::Make(description.timing.timing_value);
		if (!timing_func)
			return OpStatus::ERR_NO_MEMORY;

		Length2Transition* len_trans = OP_NEW(Length2Transition, (description, runtime_ms, timing_func, from, to, percent_from));
		if (len_trans)
			trans = len_trans;
		else
		{
			OP_DELETE(timing_func);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

PropertyTransition::StateChange
Length2Transition::NewToValue(const LayoutCoord to[2], double runtime_ms, const TransitionTimingData& timing)
{
	BOOL new_is_from = to[0] == m_from[0] && to[1] == m_from[1];
	BOOL new_is_to = to[0] == m_to[0] && to[1] == m_to[1];

	if (m_reverse)
	{
		BOOL tmp = new_is_to;
		new_is_to = new_is_from;
		new_is_from = tmp;
	}

	if (new_is_to)
		return TRANSITION_KEEP;
	else
		if (new_is_from && SameTiming(timing))
		{
			Reverse(runtime_ms);
			return TRANSITION_REVERSE;
		}
		else
			return TRANSITION_REPLACE;
}

/* virtual */ int
RectTransition::Animate(double runtime_ms)
{
	double percentage = CallTimingFunction(runtime_ms);

	BOOL changed = FALSE;

	for (int i=0; i<4; i++)
	{
		if (m_from[i] != CLIP_AUTO)
		{
			LayoutCoord cur = LayoutCoord(static_cast<int>(OpRound(m_from[i] + (m_to[i] - m_from[i])*percentage)));
			if (m_current[i] != cur)
			{
				m_current[i] = cur;
				changed = TRUE;
			}
		}
	}

	if (changed)
		return PROPS_CHANGED_SIZE;
	else
		return PROPS_CHANGED_NONE;
}

/* virtual */ void
RectTransition::GetTransitionValue(HTMLayoutProperties& props) const
{
	OP_ASSERT(GetProperty() == CSS_PROPERTY_clip);

	props.clip_top = m_current[TOP];
	props.clip_left = m_current[LEFT];
	props.clip_bottom = m_current[BOTTOM];
	props.clip_right = m_current[RIGHT];
	props.transition_packed.clip = 1;
}

/* static */ OP_STATUS
RectTransition::UpdateTransition(const TransitionDescription& description,
								 PropertyTransition*& trans,
								 const LayoutCoord from[4],
								 const LayoutCoord to[4],
								 double runtime_ms)
{
	if (trans)
	{
		/* Transition in progress. */

		PropertyTransition::StateChange change = static_cast<RectTransition*>(trans)->NewToValue(to, runtime_ms, description.timing);

		if (change == TRANSITION_REPLACE)
		{
			/* Delete transition in progress. Might be replaced by a new one
			   further down. */
			trans->Out();
			OP_DELETE(trans);
			trans = NULL;
		}
		else
		{
			/* NewToValue either found that the new end value matched the
			   current one, or it matched the start value of the current
			   transition and reversed the transition. */
			OP_ASSERT(change == TRANSITION_KEEP || change == TRANSITION_REVERSE);
			return OpStatus::OK;
		}
	}

	if (description.timing.DoStart() && (from[TOP] != to[TOP] || from[LEFT] != to[LEFT] ||
										from[BOTTOM] != to[BOTTOM] || from[RIGHT] != to[RIGHT]) || description.callback != NULL)
	{
		TransitionTimingFunction* timing_func = TransitionTimingFunction::Make(description.timing.timing_value);
		if (!timing_func)
			return OpStatus::ERR_NO_MEMORY;

		RectTransition* len_trans = OP_NEW(RectTransition, (description, runtime_ms, timing_func, from, to));
		if (len_trans)
			trans = len_trans;
		else
		{
			OP_DELETE(timing_func);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

PropertyTransition::StateChange
RectTransition::NewToValue(const LayoutCoord to[4], double runtime_ms, const TransitionTimingData& timing)
{
	BOOL new_is_from = TRUE;
	BOOL new_is_to = TRUE;

	for (int i=0; i<4; i++)
	{
		if (to[i] != m_from[i])
			new_is_from = FALSE;
		if (to[i] != m_to[i])
			new_is_to = FALSE;
	}

	if (m_reverse)
	{
		BOOL tmp = new_is_to;
		new_is_to = new_is_from;
		new_is_from = tmp;
	}

	if (new_is_to)
		return TRANSITION_KEEP;
	else
		if (new_is_from && SameTiming(timing))
		{
			Reverse(runtime_ms);
			return TRANSITION_REVERSE;
		}
		else
			return TRANSITION_REPLACE;
}

/* virtual */ int
IntegerTransition::Animate(double runtime_ms)
{
	int changes = PROPS_CHANGED_NONE;
	double percentage = CallTimingFunction(runtime_ms);
	int cur = static_cast<int>(op_floor(m_from + (m_to - m_from)*percentage));
	if (cur != m_current)
	{
		switch (GetProperty())
		{
		case CSS_PROPERTY_z_index:
			changes = PROPS_CHANGED_UPDATE_SIZE;
			break;
		case CSS_PROPERTY_font_weight:
			changes = PROPS_CHANGED_SIZE | PROPS_CHANGED_REMOVE_CACHED_TEXT;
			break;
		case CSS_PROPERTY_column_count:
			if ((cur <= 1) == (m_current <= 1))
				changes = PROPS_CHANGED_SIZE;
			else
				changes = PROPS_CHANGED_STRUCTURE;
			break;
		case CSS_PROPERTY_order:
			changes = PROPS_CHANGED_SIZE;
			break;
		default:
			OP_ASSERT(!"Unhandled property in IntegerTransition::Animate");
		}
		m_current = cur;
	}
	return changes;
}

/* virtual */ void
IntegerTransition::GetTransitionValue(HTMLayoutProperties& props) const
{
	switch (GetProperty())
	{
	case CSS_PROPERTY_z_index:
		props.z_index = m_current;
		props.transition_packed2.z_index = 1;
		break;
	case CSS_PROPERTY_font_weight:
		props.font_weight = m_current;
		props.transition_packed.font_weight = 1;
		break;
	case CSS_PROPERTY_column_count:
		props.column_count = static_cast<short>(m_current);
		props.transition_packed.column_count = 1;
		break;
	case CSS_PROPERTY_order:
		props.order = m_current;
		props.transition_packed3.order = 1;
		break;
	default:
		OP_ASSERT(!"Unhandled property in IntegerTransition::GetTransitionValue");
		break;
	}
}

/* static */ OP_STATUS
IntegerTransition::UpdateTransition(const TransitionDescription& description,
									PropertyTransition*& trans,
									int from,
									int to,
									double runtime_ms)
{
	if (trans)
	{
		/* Transition in progress. */
		IntegerTransition* int_trans = static_cast<IntegerTransition*>(trans);
		PropertyTransition::StateChange change = int_trans->NewToValue(to, runtime_ms, description.timing);
		if (change == TRANSITION_REPLACE)
		{
			/* Delete transition in progress. Might be replaced by a new one
			   further down. */
			trans->Out();
			OP_DELETE(trans);
			trans = NULL;
		}
		else
		{
			/* NewToValue either found that the new end value matched the
			   current one, or it matched the start value of the current
			   transition and reversed the transition. */
			OP_ASSERT(change == TRANSITION_KEEP || change == TRANSITION_REVERSE);
			return OpStatus::OK;
		}
	}

	if (description.timing.DoStart() && from != to || description.callback != NULL)
	{
		TransitionTimingFunction* timing_func = TransitionTimingFunction::Make(description.timing.timing_value);
		if (!timing_func)
			return OpStatus::ERR_NO_MEMORY;

		IntegerTransition* int_trans = OP_NEW(IntegerTransition, (description, runtime_ms, timing_func, from, to));
		if (int_trans)
			trans = int_trans;
		else
		{
			OP_DELETE(timing_func);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

PropertyTransition::StateChange
IntegerTransition::NewToValue(int to, double runtime_ms, const TransitionTimingData& timing)
{
	if (to == (m_reverse ? m_from : m_to))
		return TRANSITION_KEEP;
	else
		if (to == (m_reverse ? m_to : m_from) && SameTiming(timing))
		{
			Reverse(runtime_ms);
			return TRANSITION_REVERSE;
		}
		else
			return TRANSITION_REPLACE;
}

/* virtual */ int
LayoutFixedTransition::Animate(double runtime_ms)
{
	int changes = PROPS_CHANGED_NONE;
	double percentage = CallTimingFunction(runtime_ms);
	LayoutFixed cur = FloatToLayoutFixed(LayoutFixedToFloat(m_from) + LayoutFixedToFloat(m_to - m_from)*percentage);
	if (cur != m_current)
	{
		switch (GetProperty())
		{
		case CSS_PROPERTY_font_size:
		case CSS_PROPERTY_word_spacing:
			changes |= PROPS_CHANGED_REMOVE_CACHED_TEXT;
			/* fall through */
		case CSS_PROPERTY_line_height:
			changes |= PROPS_CHANGED_SIZE;
			break;
		default:
			OP_ASSERT(!"Unhandled property in LayoutFixedTransition::Animate");
			break;
		}
		m_current = cur;
	}
	return changes;
}

/* virtual */ void
LayoutFixedTransition::GetTransitionValue(HTMLayoutProperties& props) const
{
	switch (GetProperty())
	{
	case CSS_PROPERTY_font_size:
		props.decimal_font_size_constrained = props.decimal_font_size = m_current;
		props.font_size = LayoutFixedToInt(m_current);
		props.transition_packed.font_size = 1;
		break;
	case CSS_PROPERTY_line_height:
		props.line_height_i = m_current;
		props.transition_packed.line_height = 1;
		break;
	case CSS_PROPERTY_word_spacing:
		props.word_spacing_i = m_current;
		props.transition_packed.word_spacing = 1;
		break;
	default:
		OP_ASSERT(!"Unhandled property in LayoutFixedTransition::GetTransitionValue");
		break;
	}
}

/* static */ OP_STATUS
LayoutFixedTransition::UpdateTransition(const TransitionDescription& description,
										PropertyTransition*& trans,
										LayoutFixed from,
										LayoutFixed to,
										double runtime_ms)
{
	if (trans)
	{
		/* Transition in progress. */
		LayoutFixedTransition* fix_trans = static_cast<LayoutFixedTransition*>(trans);
		PropertyTransition::StateChange change = fix_trans->NewToValue(to, runtime_ms, description.timing);
		if (change == TRANSITION_REPLACE)
		{
			/* Delete transition in progress. Might be replaced by a new one
			   further down. */
			trans->Out();
			OP_DELETE(trans);
			trans = NULL;
		}
		else
		{
			/* NewToValue either found that the new end value matched the
			   current one, or it matched the start value of the current
			   transition and reversed the transition. */
			OP_ASSERT(change == TRANSITION_KEEP || change == TRANSITION_REVERSE);
			return OpStatus::OK;
		}
	}

	if (description.timing.DoStart() && from != to || description.callback != NULL)
	{
		TransitionTimingFunction* timing_func = TransitionTimingFunction::Make(description.timing.timing_value);
		if (!timing_func)
			return OpStatus::ERR_NO_MEMORY;

		LayoutFixedTransition* fix_trans = OP_NEW(LayoutFixedTransition, (description, runtime_ms, timing_func, from, to));
		if (fix_trans)
			trans = fix_trans;
		else
		{
			OP_DELETE(timing_func);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

PropertyTransition::StateChange
LayoutFixedTransition::NewToValue(LayoutFixed to, double runtime_ms, const TransitionTimingData& timing)
{
	if (to == (m_reverse ? m_from : m_to))
		return TRANSITION_KEEP;
	else
		if (to == (m_reverse ? m_to : m_from) && SameTiming(timing))
		{
			Reverse(runtime_ms);
			return TRANSITION_REVERSE;
		}
		else
			return TRANSITION_REPLACE;
}

/* virtual */ int
NumberTransition::Animate(double runtime_ms)
{
	int changes = PROPS_CHANGED_NONE;
	double percentage = CallTimingFunction(runtime_ms);
	double cur = m_from + (m_to - m_from)*percentage;
	if (cur != m_current)
	{
		switch (GetProperty())
		{
		case CSS_PROPERTY_opacity:
			changes |= PROPS_CHANGED_SVG_REPAINT;
			if (m_current == 1.0 || cur == 1.0)
				changes |= PROPS_CHANGED_STRUCTURE;
			else
				changes |= PROPS_CHANGED_UPDATE;
			break;
		case CSS_PROPERTY_flex_grow:
		case CSS_PROPERTY_flex_shrink:
			changes |= PROPS_CHANGED_SIZE;
			break;
		default:
			OP_ASSERT(!"Unhandled property in NumberTransition::Animate");
			break;
		}
		m_current = cur;
	}

	return changes;
}

/* virtual */ void
NumberTransition::GetTransitionValue(HTMLayoutProperties& props) const
{
	switch (GetProperty())
	{
	case CSS_PROPERTY_opacity:
		props.opacity = static_cast<UINT8>(OpRound(m_current * 255));
		props.transition_packed2.opacity = 1;
		break;
	case CSS_PROPERTY_flex_grow:
		props.flex_grow = float(m_current);
		props.transition_packed3.flex_grow = 1;
		break;
	case CSS_PROPERTY_flex_shrink:
		props.flex_shrink = float(m_current);
		props.transition_packed3.flex_shrink = 1;
		break;
	default:
		OP_ASSERT(!"Unhandled property in NumberTransition::GetTransitionValue");
	}
}

/* static */ OP_STATUS
NumberTransition::UpdateTransition(const TransitionDescription& description,
								   PropertyTransition*& trans,
								   double from,
								   double to,
								   double runtime_ms)
{
	if (trans)
	{
		/* Transition in progress. */
		NumberTransition* num_trans = static_cast<NumberTransition*>(trans);
		PropertyTransition::StateChange change = num_trans->NewToValue(to, runtime_ms, description.timing);
		if (change == TRANSITION_REPLACE)
		{
			/* Delete transition in progress. Might be replaced by a new one
			   further down. */
			trans->Out();
			OP_DELETE(trans);
			trans = NULL;
		}
		else
		{
			/* NewToValue either found that the new end value matched the
			   current one, or it matched the start value of the current
			   transition and reversed the transition. */
			OP_ASSERT(change == TRANSITION_KEEP || change == TRANSITION_REVERSE);
			return OpStatus::OK;
		}
	}

	if (description.timing.DoStart() && from != to || description.callback != NULL)
	{
		TransitionTimingFunction* timing_func = TransitionTimingFunction::Make(description.timing.timing_value);
		if (!timing_func)
			return OpStatus::ERR_NO_MEMORY;

		NumberTransition* num_trans = OP_NEW(NumberTransition, (description, runtime_ms, timing_func, from, to));
		if (num_trans)
			trans = num_trans;
		else
		{
			OP_DELETE(timing_func);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

PropertyTransition::StateChange
NumberTransition::NewToValue(double to, double runtime_ms, const TransitionTimingData& timing)
{
	if (to == (m_reverse ? m_from : m_to))
		return TRANSITION_KEEP;
	else
		if (to == (m_reverse ? m_to : m_from) && SameTiming(timing))
		{
			Reverse(runtime_ms);
			return TRANSITION_REVERSE;
		}
		else
			return TRANSITION_REPLACE;
}

/* virtual */ int
KeywordTransition::Animate(double runtime_ms)
{
	OP_ASSERT(GetProperty() == CSS_PROPERTY_visibility);

	CSSValue cur;
	double percentage = CallTimingFunction(runtime_ms);

	if (percentage == 0)
		cur = m_from;
	else if (percentage == 1)
		cur = m_to;
	else if (m_from == CSS_VALUE_visible || m_to == CSS_VALUE_visible)
		cur = CSS_VALUE_visible;
	else if (m_from == CSS_VALUE_hidden || m_to == CSS_VALUE_hidden)
		cur = CSS_VALUE_hidden;
	else
		cur = CSS_VALUE_collapse;

	if (cur != m_current)
	{
		m_current = cur;
		return PROPS_CHANGED_UPDATE | PROPS_CHANGED_SVG_RELAYOUT;
	}

	return PROPS_CHANGED_NONE;
}

/* virtual */ void
KeywordTransition::GetTransitionValue(HTMLayoutProperties& props) const
{
	OP_ASSERT(GetProperty() == CSS_PROPERTY_visibility);
	props.visibility = m_current;
	props.transition_packed.visibility = 1;
}

/* static */ OP_STATUS
KeywordTransition::UpdateTransition(const TransitionDescription& description,
									PropertyTransition*& trans,
									CSSValue from,
									CSSValue to,
									double runtime_ms)
{
	if (trans)
	{
		/* Transition in progress. */
		KeywordTransition* kw_trans = static_cast<KeywordTransition*>(trans);
		PropertyTransition::StateChange change = kw_trans->NewToValue(to, runtime_ms, description.timing);
		if (change == TRANSITION_REPLACE)
		{
			/* Delete transition in progress. Might be replaced by a new one
			   further down. */
			trans->Out();
			OP_DELETE(trans);
			trans = NULL;
		}
		else
		{
			/* NewToValue either found that the new end value matched the
			   current one, or it matched the start value of the current
			   transition and reversed the transition. */
			OP_ASSERT(change == TRANSITION_KEEP || change == TRANSITION_REVERSE);
			return OpStatus::OK;
		}
	}

	if (description.timing.DoStart() && from != to || description.callback != NULL)
	{
		TransitionTimingFunction* timing_func = TransitionTimingFunction::Make(description.timing.timing_value);
		if (!timing_func)
			return OpStatus::ERR_NO_MEMORY;

		KeywordTransition* kw_trans = OP_NEW(KeywordTransition, (description, runtime_ms, timing_func, from, to));
		if (kw_trans)
			trans = kw_trans;
		else
		{
			OP_DELETE(timing_func);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

PropertyTransition::StateChange
KeywordTransition::NewToValue(CSSValue to, double runtime_ms, const TransitionTimingData& timing)
{
	if (to == (m_reverse ? m_from : m_to))
		return TRANSITION_KEEP;
	else
		if (to == (m_reverse ? m_to : m_from) && SameTiming(timing))
		{
			Reverse(runtime_ms);
			return TRANSITION_REVERSE;
		}
		else
			return TRANSITION_REPLACE;
}

# ifdef CSS_TRANSFORMS

OP_STATUS
TransformTransition::Construct()
{
	OP_ASSERT(m_from && !m_current);
	m_current = static_cast<CSS_transform_list*>(m_from->CreateCopy());
	if (m_current)
	{
		m_current->Ref();
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

/* virtual */ int
TransformTransition::Animate(double runtime_ms)
{
	int changes = PROPS_CHANGED_NONE;

	double percentage = CallTimingFunction(runtime_ms);

	AffineTransform res;
	BOOL was_identity = m_current->GetAffineTransform(res) && res.IsIdentity();

	if (m_current->Transition(m_from, m_to, float(percentage)))
	{
		/* Identity matrix might mean that the start state was 'none'.
		   In that case we'll have to change layout box. */
		if (was_identity)
			changes = PROPS_CHANGED_STRUCTURE;
		else
			changes = PROPS_CHANGED_BOUNDS;
	}

	if (IsFinished(runtime_ms))
	{
		/* Identity matrix might mean that the end state is 'none'.
		   In that case we'll have to change layout box. */
		if (m_current->GetAffineTransform(res) && res.IsIdentity())
			changes = PROPS_CHANGED_STRUCTURE;
	}
	return changes;
}

/* virtual */ void
TransformTransition::GetTransitionValue(HTMLayoutProperties& props) const
{
	props.transforms_cp = m_current;
	props.transition_packed2.transform = 1;
}

/* static */ OP_STATUS
TransformTransition::UpdateTransition(const TransitionDescription& description,
									  VisualDevice* vis_dev,
									  PropertyTransition*& trans,
									  const HTMLayoutProperties* computed_old,
									  const HTMLayoutProperties* computed_new,
									  double runtime_ms)
{
	CSS_decl* from_decl = computed_old->transforms_cp;
	CSS_decl* to_decl = computed_new->transforms_cp;

	// Both 'none'.
	if (!from_decl && !to_decl)
		return OpStatus::OK;

	OP_ASSERT(!from_decl || from_decl->GetDeclType() == CSS_DECL_TRANSFORM_LIST);
	OP_ASSERT(!to_decl || to_decl->GetDeclType() == CSS_DECL_TRANSFORM_LIST);

	CSS_transform_list* from_list = static_cast<CSS_transform_list*>(from_decl);
	CSS_transform_list* to_list = static_cast<CSS_transform_list*>(to_decl);

	CSS_DeclAutoPtr from_ptr;
	CSS_DeclAutoPtr to_ptr;

	if (from_list)
	{
		from_list = static_cast<CSS_transform_list*>(from_list->CreateCopy());
		if (from_list)
		{
			from_list->Ref();
			from_ptr.reset(from_list);
			computed_old->SetDisplayProperties(vis_dev);
			CSSLengthResolver length_resolver(vis_dev);
			length_resolver.FillFromVisualDevice();
			from_list->ResolveRelativeLengths(length_resolver);
		}
		else
			return OpStatus::ERR_NO_MEMORY;
	}

	if (to_list)
	{
		to_list = static_cast<CSS_transform_list*>(to_list->CreateCopy());
		if (to_list)
		{
			to_list->Ref();
			to_ptr.reset(to_list);
			computed_old->SetDisplayProperties(vis_dev);
			CSSLengthResolver length_resolver(vis_dev);
			length_resolver.FillFromVisualDevice();
			to_list->ResolveRelativeLengths(length_resolver);
		}
		else
			return OpStatus::ERR_NO_MEMORY;
	}

	BOOL compatible = TRUE;

	if (to_list && from_list)
		compatible = CSS_transform_list::MakeCompatible(*from_list, *to_list);
	else
		if (from_list)
		{
			to_list = from_list->MakeCompatibleZeroList();
			if (to_list)
			{
				to_list->Ref();
				to_ptr.reset(to_list);
			}
			else
				return OpStatus::ERR_NO_MEMORY;
		}
		else
		{
			from_list = to_list->MakeCompatibleZeroList();
			if (from_list)
			{
				from_list->Ref();
				from_ptr.reset(from_list);
			}
			else
				return OpStatus::ERR_NO_MEMORY;
		}

	if (trans)
	{
		/* Transition in progress. */
		TransformTransition* decl_trans = static_cast<TransformTransition*>(trans);

		PropertyTransition::StateChange change = TRANSITION_REPLACE;
		if (compatible)
			change = decl_trans->NewToValue(to_ptr.get(), runtime_ms, description.timing);

		if (change == TRANSITION_REPLACE)
		{
			/* Delete transition in progress. Might be replaced by a new one
			   further down. */
			trans->Out();
			OP_DELETE(trans);
			trans = NULL;
		}
		else
		{
			/* NewToValue either found that the new end value matched the
			   current one, or it matched the start value of the current
			   transition and reversed the transition. */
			OP_ASSERT(change == TRANSITION_KEEP || change == TRANSITION_REVERSE);
			return OpStatus::OK;
		}
	}

	if (description.timing.DoStart() && compatible && (!from_list->IsEqual(to_list) || description.callback != NULL))
	{
		TransitionTimingFunction* timing_func = TransitionTimingFunction::Make(description.timing.timing_value);
		if (!timing_func)
			return OpStatus::ERR_NO_MEMORY;

		TransformTransition* new_trans = OP_NEW(TransformTransition, (description,
																	  runtime_ms,
																	  timing_func,
																	  from_list,
																	  to_list));
		if (new_trans)
		{
			if (OpStatus::IsSuccess(new_trans->Construct()))
				trans = new_trans;
			else
			{
				OP_DELETE(new_trans);
				return OpStatus::ERR_NO_MEMORY;
			}
		}
		else
		{
			OP_DELETE(timing_func);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

PropertyTransition::StateChange
TransformTransition::NewToValue(const CSS_decl* to, double runtime_ms, const TransitionTimingData& timing)
{
	if (to->IsEqual(m_reverse ? m_from : m_to))
		return TRANSITION_KEEP;
	else
		if (to->IsEqual(m_reverse ? m_to : m_from) && SameTiming(timing))
		{
			Reverse(runtime_ms);
			return TRANSITION_REVERSE;
		}
		else
			return TRANSITION_REPLACE;
}
# endif // CSS_TRANSFORMS

static inline void
SetGenericValuesFromShadow(CSS_generic_value*& values, const Shadow& shadow, BOOL add_comma)
{
	if (add_comma)
		values++->SetValueType(CSS_COMMA);

	values->SetReal(shadow.left);
	values++->SetValueType(CSS_PX);
	values->SetReal(shadow.top);
	values++->SetValueType(CSS_PX);
	values->SetReal(shadow.blur);
	values++->SetValueType(CSS_PX);
	values->SetReal(shadow.spread);
	values++->SetValueType(CSS_PX);
	values->value.color = GetRGBA(shadow.color);
	values++->SetValueType(CSS_DECL_COLOR);
	values->SetType(shadow.inset ? CSS_VALUE_inset : CSS_VALUE_outset);
	values++->SetValueType(CSS_IDENT);
}

/** Make to and from shadow lists which contains the extensive set of
	values for each shadow, resolving relative lengths, and replacing
	undefined shadow colors with the passed color property value. The
	shortest shadow list is padded with 0 offset shadows with transparent
	colors to make the lists have the same number of shadows.

	@param data @see GenArrayDeclTransition::Data
	@return IS_TRUE with two new declaration object returned in from_decl
			and to_decl which the caller is responsible for deleting.
			IS_FALSE with no new declaration objects if the shadow lists
			could not be transitioned because of inset/outset mismatch.
			ERR_NO_MEMORY, with no new objects, on OOM. */
static OP_BOOLEAN CreateExtendedShadowLists(GenArrayDeclTransition::Data& data)
{
	Shadows from_shadows;
	from_shadows.Set(data.from_decl);
	int from_count = from_shadows.GetCount();

	Shadows to_shadows;
	to_shadows.Set(data.to_decl);
	int to_count = to_shadows.GetCount();

	int extended_count = MAX(from_count, to_count);
	if (extended_count == 0)
		return OpBoolean::IS_FALSE;

	short prop = data.from_decl ? data.from_decl->GetProperty() : data.to_decl->GetProperty();

	CSS_generic_value* new_from_values = OP_NEWA(CSS_generic_value, extended_count*7-1);
	CSS_generic_value* new_to_values = OP_NEWA(CSS_generic_value, extended_count*7-1);
	CSS_gen_array_decl* new_from_decl = CSS_heap_gen_array_decl::Create(prop, new_from_values, extended_count*7-1);
	CSS_gen_array_decl* new_to_decl = CSS_heap_gen_array_decl::Create(prop, new_to_values, extended_count*7-1);

	if (!new_from_decl || !new_to_decl)
	{
		OP_DELETE(new_from_decl);
		OP_DELETE(new_to_decl);
		return OpStatus::ERR_NO_MEMORY;
	}

	Shadows::Iterator from_iter(from_shadows);
	Shadows::Iterator to_iter(to_shadows);

	for (int i=0; i<extended_count; i++)
	{
		Shadow from_shadow;
		if (from_iter.GetNext(data.length_resolver_old, from_shadow))
		{
			if (from_shadow.color == USE_DEFAULT_COLOR)
				from_shadow.color = data.computed_old->font_color;
		}
		else
		{
			from_shadow.Reset();
			from_shadow.color = OP_RGBA(0,0,0,0);
		}

		Shadow to_shadow;
		if (to_iter.GetNext(data.length_resolver_new, to_shadow))
		{
			if (to_shadow.color == USE_DEFAULT_COLOR)
				to_shadow.color = data.computed_new->font_color;
		}
		else
		{
			to_shadow.Reset();
			to_shadow.color = OP_RGBA(0,0,0,0);
		}

		if (i >= from_count)
			from_shadow.inset = to_shadow.inset;
		else
			if (i >= to_count)
				to_shadow.inset = from_shadow.inset;
			else
				if (from_shadow.inset != to_shadow.inset)
				{
					OP_DELETE(new_from_decl);
					OP_DELETE(new_to_decl);
					return OpBoolean::IS_FALSE;
				}

		SetGenericValuesFromShadow(new_from_values, from_shadow, i > 0);
		SetGenericValuesFromShadow(new_to_values, to_shadow, i > 0);
	}

	data.from_decl = new_from_decl;
	data.to_decl = new_to_decl;
	return OpBoolean::IS_TRUE;
}

/** Helper function for finding the greatest common divisor of two integers. */

static inline int gcd(int a, int b)
{
	while (b)
	{
		int r = a % b;
		a = b;
		b = r;
	}
	return a;
}

static inline void
SetPositionValuesFromBackground(CSS_generic_value*& values, const BackgroundProperties& bg_props, BOOL add_comma)
{
	if (add_comma)
		values++->SetValueType(CSS_COMMA);

	values->SetValueType(CSS_IDENT);
	values++->SetType(bg_props.bg_pos_xref);

	short xunit = bg_props.packed.bg_xpos_per ? CSS_PERCENTAGE : CSS_PX;
	float xpos = static_cast<float>(bg_props.bg_xpos);
	if (xunit == CSS_PERCENTAGE)
		xpos /= 100.0f;

	values->SetValueType(xunit);
	values++->SetReal(xpos);

	values->SetValueType(CSS_IDENT);
	values++->SetType(bg_props.bg_pos_yref);

	short yunit = bg_props.packed.bg_ypos_per ? CSS_PERCENTAGE : CSS_PX;
	float ypos = static_cast<float>(bg_props.bg_ypos);
	if (yunit == CSS_PERCENTAGE)
		ypos /= 100.0f;

	values->SetValueType(yunit);
	values++->SetReal(ypos);
}

/** Make to and from position lists that contain both position components
	for each position, resolving relative lengths.

	@param data @see GenArrayDeclTransition::Data
	@return IS_TRUE with two new declaration object returned in from_decl
			and to_decl which the caller is responsible for deleting.
			IS_FALSE with no new declaration objects if the background
			positions could not be transitioned. ERR_NO_MEMORY, with no new
			objects, on OOM. */
static OP_BOOLEAN CreateBackgroundPositionLists(GenArrayDeclTransition::Data& data)
{
	CSS_gen_array_decl* pos_old = data.computed_old->bg_images.GetPositions();
	CSS_gen_array_decl* pos_new = data.computed_new->bg_images.GetPositions();

	if (!pos_old || !pos_new || pos_old->GetLayerCount() != pos_new->GetLayerCount())
		return OpBoolean::IS_FALSE;

	int layers;
	int old_layers = pos_old->GetLayerCount();
	int new_layers = pos_new->GetLayerCount();

	if (old_layers == new_layers)
		layers = old_layers;
	else
		layers = old_layers * new_layers / gcd(old_layers, new_layers);

	int value_count = layers*5-1;

	CSS_generic_value* new_from_values = OP_NEWA(CSS_generic_value, value_count);
	CSS_generic_value* new_to_values = OP_NEWA(CSS_generic_value, value_count);
	CSS_gen_array_decl* new_from_decl = CSS_heap_gen_array_decl::Create(CSS_PROPERTY_background_position, new_from_values, value_count);
	CSS_gen_array_decl* new_to_decl = CSS_heap_gen_array_decl::Create(CSS_PROPERTY_background_position, new_to_values, value_count);

	if (!new_from_decl || !new_to_decl)
	{
		OP_DELETE(new_from_decl);
		OP_DELETE(new_to_decl);
		return OpStatus::ERR_NO_MEMORY;
	}

	BackgroundProperties background_from, background_to;

	for (int i=0; i<layers; i++)
	{
		background_from.Reset();
		background_to.Reset();

		data.computed_old->bg_images.SetBackgroundPosition(data.length_resolver_old, i, background_from);
		data.computed_new->bg_images.SetBackgroundPosition(data.length_resolver_new, i, background_to);

		BOOL transitionable = background_from.bg_pos_xref == background_to.bg_pos_xref &&
							  background_from.bg_pos_yref == background_to.bg_pos_yref;

		if (background_from.packed.bg_xpos_per != background_to.packed.bg_xpos_per)
		{
			if (background_from.bg_xpos == 0)
				background_from.packed.bg_xpos_per = background_to.packed.bg_xpos_per;
			else if (background_to.bg_xpos == 0)
				background_to.packed.bg_xpos_per = background_from.packed.bg_xpos_per;
			else
				transitionable = FALSE;
		}

		if (background_from.packed.bg_ypos_per != background_to.packed.bg_ypos_per)
		{
			if (background_from.bg_ypos == 0)
				background_from.packed.bg_ypos_per = background_to.packed.bg_ypos_per;
			else if (background_to.bg_ypos == 0)
				background_to.packed.bg_ypos_per = background_from.packed.bg_ypos_per;
			else
				transitionable = FALSE;
		}

		if (!transitionable)
		{
			OP_DELETE(new_from_decl);
			OP_DELETE(new_to_decl);
			return OpBoolean::IS_FALSE;
		}

		SetPositionValuesFromBackground(new_from_values, background_from, i > 0);
		SetPositionValuesFromBackground(new_to_values, background_to, i > 0);
	}

	new_from_decl->SetLayerCount(layers);
	new_to_decl->SetLayerCount(layers);

	data.from_decl = new_from_decl;
	data.to_decl = new_to_decl;
	return OpBoolean::IS_TRUE;
}

static inline void
SetSizeValuesFromBackground(CSS_generic_value*& values, const BackgroundProperties& bg_props, BOOL add_comma)
{
	if (add_comma)
		values++->SetValueType(CSS_COMMA);

	short xunit = bg_props.packed.bg_xsize_per ? CSS_PERCENTAGE : CSS_PX;
	float xsize = static_cast<float>(bg_props.bg_xsize);
	if (xunit == CSS_PERCENTAGE)
		xsize /= 100.0f;

	values->SetValueType(xunit);
	values++->SetReal(xsize);

	short yunit = bg_props.packed.bg_ysize_per ? CSS_PERCENTAGE : CSS_PX;
	float ysize = static_cast<float>(bg_props.bg_ysize);
	if (yunit == CSS_PERCENTAGE)
		ysize /= 100.0f;

	values->SetValueType(yunit);
	values++->SetReal(ysize);
}

/** Make to and from size lists that contain both size components
	for each size, resolving relative lengths.

	@param data @see GenArrayDeclTransition::Data
	@return IS_TRUE with two new declaration object returned in from_decl
			and to_decl which the caller is responsible for deleting.
			IS_FALSE with no new declaration objects if the background
			sizes could not be transitioned. ERR_NO_MEMORY, with no new
			objects, on OOM. */
static OP_BOOLEAN CreateBackgroundSizeLists(GenArrayDeclTransition::Data& data)
{
	CSS_gen_array_decl* size_old = data.computed_old->bg_images.GetSizes();
	CSS_gen_array_decl* size_new = data.computed_new->bg_images.GetSizes();

	if (!size_old || !size_new || size_old->GetLayerCount() != size_new->GetLayerCount())
		return OpBoolean::IS_FALSE;

	int layers;
	int old_layers = size_old->GetLayerCount();
	int new_layers = size_new->GetLayerCount();

	if (old_layers == new_layers)
		layers = old_layers;
	else
		layers = old_layers * new_layers / gcd(old_layers, new_layers);

	int value_count = layers*3-1;

	CSS_generic_value* new_from_values = OP_NEWA(CSS_generic_value, value_count);
	CSS_generic_value* new_to_values = OP_NEWA(CSS_generic_value, value_count);
	CSS_gen_array_decl* new_from_decl = CSS_heap_gen_array_decl::Create(CSS_PROPERTY_background_size, new_from_values, value_count);
	CSS_gen_array_decl* new_to_decl = CSS_heap_gen_array_decl::Create(CSS_PROPERTY_background_size, new_to_values, value_count);

	if (!new_from_decl || !new_to_decl)
	{
		OP_DELETE(new_from_decl);
		OP_DELETE(new_to_decl);
		return OpStatus::ERR_NO_MEMORY;
	}

	BackgroundProperties background_from, background_to;

	for (int i=0; i<layers; i++)
	{
		background_from.Reset();
		background_to.Reset();

		data.computed_old->bg_images.SetBackgroundSize(data.length_resolver_old, i, background_from);
		data.computed_new->bg_images.SetBackgroundSize(data.length_resolver_new, i, background_to);

		BOOL transitionable = !background_from.packed.bg_size_contain &&
							  !background_from.packed.bg_size_cover &&
							  background_from.bg_xsize != BG_SIZE_X_AUTO &&
							  background_from.bg_ysize != BG_SIZE_Y_AUTO &&
							  !background_to.packed.bg_size_contain &&
							  !background_to.packed.bg_size_cover &&
							  background_to.bg_xsize != BG_SIZE_X_AUTO &&
							  background_to.bg_ysize != BG_SIZE_Y_AUTO;

		if (background_from.packed.bg_xsize_per != background_to.packed.bg_xsize_per)
		{
			if (background_from.bg_xsize == 0)
				background_from.packed.bg_xsize_per = background_to.packed.bg_xsize_per;
			else if (background_to.bg_xsize == 0)
				background_to.packed.bg_xsize_per = background_from.packed.bg_xsize_per;
			else
				transitionable = FALSE;
		}

		if (background_from.packed.bg_ysize_per != background_to.packed.bg_ysize_per)
		{
			if (background_from.bg_ysize == 0)
				background_from.packed.bg_ysize_per = background_to.packed.bg_ysize_per;
			else if (background_to.bg_ysize == 0)
				background_to.packed.bg_ysize_per = background_from.packed.bg_ysize_per;
			else
				transitionable = FALSE;
		}

		if (!transitionable)
		{
			OP_DELETE(new_from_decl);
			OP_DELETE(new_to_decl);
			return OpBoolean::IS_FALSE;
		}

		SetSizeValuesFromBackground(new_from_values, background_from, i > 0);
		SetSizeValuesFromBackground(new_to_values, background_to, i > 0);
	}

	new_from_decl->SetLayerCount(layers);
	new_to_decl->SetLayerCount(layers);

	data.from_decl = new_from_decl;
	data.to_decl = new_to_decl;
	return OpBoolean::IS_TRUE;
}

OP_STATUS
GenArrayDeclTransition::Construct()
{
	OP_ASSERT(!m_current);

	m_current = CSS_copy_gen_array_decl::Copy(m_from->GetProperty(), m_from);
	if (m_current)
	{
		m_current->Ref();
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

/* virtual */ int
GenArrayDeclTransition::Animate(double runtime_ms)
{
	double percentage = CallTimingFunction(runtime_ms);

	if (m_current->Transition(m_from, m_to, float(percentage)))
	{
		CSSProperty prop = GetProperty();
		if (prop == CSS_PROPERTY_box_shadow || prop == CSS_PROPERTY_text_shadow)
			return PROPS_CHANGED_BOUNDS;
		else
			return PROPS_CHANGED_UPDATE;
	}
	else
		return PROPS_CHANGED_NONE;
}

/* virtual */ void
GenArrayDeclTransition::GetTransitionValue(HTMLayoutProperties& props) const
{
	switch (GetProperty())
	{
	case CSS_PROPERTY_box_shadow:
		props.box_shadows.Set(m_current);
		props.transition_packed.box_shadow = 1;
		break;
	case CSS_PROPERTY_text_shadow:
		props.text_shadows.Set(m_current);
		props.transition_packed.text_shadow = 1;
		break;
	case CSS_PROPERTY_background_position:
		props.bg_images.SetPositions(m_current);
		props.transition_packed.bg_pos = 1;
		break;
	case CSS_PROPERTY_background_size:
		props.bg_images.SetSizes(m_current);
		props.transition_packed.bg_size = 1;
		break;
	default:
		OP_ASSERT(!"Unhandled property in GenArrayDeclTransition::GetTransitionValue");
		break;
	}
}

/* static */ OP_STATUS
GenArrayDeclTransition::UpdateTransition(const TransitionDescription& description,
										 VisualDevice* vis_dev,
										 PropertyTransition*& trans,
										 const HTMLayoutProperties* computed_old,
										 const HTMLayoutProperties* computed_new,
										 double runtime_ms,
										 CreateArrayDeclFunction create_func)
{
	CSS_gen_array_decl *from_arr = NULL, *to_arr = NULL;
	switch (description.property)
	{
	case CSS_PROPERTY_box_shadow:
		from_arr = static_cast<CSS_gen_array_decl*>(computed_old->box_shadows.Get());
		to_arr = static_cast<CSS_gen_array_decl*>(computed_new->box_shadows.Get());
		break;
	case CSS_PROPERTY_text_shadow:
		from_arr = static_cast<CSS_gen_array_decl*>(computed_old->text_shadows.Get());
		to_arr = static_cast<CSS_gen_array_decl*>(computed_new->text_shadows.Get());
		break;
	case CSS_PROPERTY_background_size:
		from_arr = computed_old->bg_images.GetSizes();
		to_arr = computed_new->bg_images.GetSizes();
		break;
	case CSS_PROPERTY_background_position:
		from_arr = computed_old->bg_images.GetPositions();
		to_arr = computed_new->bg_images.GetPositions();
		break;
	default:
		break;
	}

	// Both 'none'.
	if (!from_arr && !to_arr)
		return OpStatus::OK;

	OP_ASSERT(!from_arr || from_arr->GetDeclType() == CSS_DECL_GEN_ARRAY);
	OP_ASSERT(!to_arr || to_arr->GetDeclType() == CSS_DECL_GEN_ARRAY);

	CSS_DeclAutoPtr from_ptr;
	CSS_DeclAutoPtr to_ptr;

	Data data(vis_dev, computed_old, computed_new, from_arr, to_arr);
	OP_BOOLEAN stat = create_func(data);
	if (stat == OpBoolean::IS_TRUE)
	{
		from_arr = data.from_decl;
		to_arr = data.to_decl;
		OP_ASSERT(from_arr && to_arr);
		from_arr->Ref();
		from_ptr.reset(from_arr);
		to_arr->Ref();
		to_ptr.reset(to_arr);
	}
	else
		if (OpBoolean::IsMemoryError(stat))
			return stat;

	OP_ASSERT(stat == OpBoolean::IS_TRUE ||
			  stat == OpBoolean::IS_FALSE);

	if (trans)
	{
		/* Transition in progress. */
		GenArrayDeclTransition* arr_trans = static_cast<GenArrayDeclTransition*>(trans);

		PropertyTransition::StateChange change = TRANSITION_REPLACE;
		if (stat == OpBoolean::IS_TRUE)
			change = arr_trans->NewToValue(to_arr, runtime_ms, description.timing);

		if (change == TRANSITION_REPLACE)
		{
			/* Delete transition in progress. Might be replaced by a new one
			   further down. */
			trans->Out();
			OP_DELETE(trans);
			trans = NULL;
		}
		else
		{
			/* NewToValue either found that the new end value matched the
			   current one, or it matched the start value of the current
			   transition and reversed the transition. */
			OP_ASSERT(change == TRANSITION_KEEP || change == TRANSITION_REVERSE);
			return OpStatus::OK;
		}
	}

	if (stat == OpBoolean::IS_TRUE && description.timing.DoStart() && (!from_arr->IsEqual(to_arr) || description.callback != NULL))
	{
		TransitionTimingFunction* timing_func = TransitionTimingFunction::Make(description.timing.timing_value);
		if (!timing_func)
			return OpStatus::ERR_NO_MEMORY;

		GenArrayDeclTransition* new_trans = OP_NEW(GenArrayDeclTransition, (description,
																			runtime_ms,
																			timing_func,
																			from_arr,
																			to_arr));
		if (new_trans)
		{
			if (OpStatus::IsSuccess(new_trans->Construct()))
				trans = new_trans;
			else
			{
				OP_DELETE(new_trans);
				return OpStatus::ERR_NO_MEMORY;
			}
		}
		else
		{
			OP_DELETE(timing_func);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

PropertyTransition::StateChange
GenArrayDeclTransition::NewToValue(const CSS_decl* to, double runtime_ms, const TransitionTimingData& timing)
{
	if (to->IsEqual(m_reverse ? m_from : m_to))
		return TRANSITION_KEEP;
	else
		if (to->IsEqual(m_reverse ? m_to : m_from) && SameTiming(timing))
		{
			Reverse(runtime_ms);
			return TRANSITION_REVERSE;
		}
		else
			return TRANSITION_REPLACE;
}

BOOL
ElementTransitions::HasPendingChange(double runtime_ms) const
{
        PropertyTransition* trans = m_transitions.First();
        while (trans)
        {
                if (trans->HasPendingChange(runtime_ms))
                        return TRUE;
                trans = trans->Suc();
        }
        return FALSE;
}

void
ElementTransitions::GetTransitionValues(HTMLayoutProperties& props) const
{
	PropertyTransition* trans = m_transitions.First();
	while (trans)
	{
		if (trans->PeekNextState() != TransitionCallback::STOPPED)
			trans->GetTransitionValue(props);
		trans = trans->Suc();
	}
}

OP_STATUS
ElementTransitions::Animate(FramesDocument* doc, double runtime_ms, HTML_Element*& docprops_elm)
{
	int change_bits = PROPS_CHANGED_NONE;

	PropertyTransition* trans = m_transitions.First();

	while (trans)
	{
		trans->UpdateState(runtime_ms);

		change_bits |= trans->Animate(runtime_ms);

		if (!trans->HasPendingChange(runtime_ms))
			RETURN_IF_ERROR(trans->TransitionEnd(doc, GetElm()));

		if (trans->IsFinished(runtime_ms))
		{
			PropertyTransition* del = trans;

			change_bits |= trans->Animate(trans->GetEndMS());

			trans = trans->Suc();
			del->Out();
			OP_DELETE(del);
		}
		else
			trans = trans->Suc();
	}

	if (change_bits != PROPS_CHANGED_NONE)
	{
		LayoutWorkplace* wp = doc->GetLogicalDocument()->GetLayoutWorkplace();
		wp->HandleChangeBits(GetElm(), change_bits);
		if (wp->HasDocRootProps(GetElm()))
			docprops_elm = GetElm();
	}

	return OpStatus::OK;
}

int
ElementTransitions::DeleteTransitions(const short* props, unsigned int count)
{
	PropertyTransition* trans = m_transitions.First();
	int changes = PROPS_CHANGED_NONE;

	while (trans)
	{
		CSSProperty prop = trans->GetProperty();
		unsigned int i=0;
		for (; i<count; i++)
			if (props[i] == prop)
				break;

		if (i == count)
		{
			PropertyTransition* del = trans;
			trans = trans->Suc();

			if (!del->HasCallback())
			{
				// Transitions with callbacks are stopped through the
				// callback. Normal transitions we can just delete.

				// Animate to the end state to get the correct changes.
				changes |= del->Animate(del->GetEndMS());

				del->Out();
				OP_DELETE(del);
			}
		}
		else
			trans = trans->Suc();
	}
	return changes;
}

PropertyTransition*
ElementTransitions::GetPropertyTransition(CSSProperty property) const
{
	PropertyTransition* trans = m_transitions.First();
	while (trans && trans->GetProperty() != property)
		trans = trans->Suc();
	return trans;
}

OP_STATUS
ElementTransitions::TransitionEnd(FramesDocument* doc, PropertyTransition* transition)
{
	RETURN_IF_ERROR(doc->ConstructDOMEnvironment());
	DOM_Environment* dom_env = doc->GetDOMEnvironment();
	if (dom_env)
	{
		DOM_Environment::EventData data;
		data.type = TRANSITIONEND;
		data.target = GetElm();
		data.elapsed_time = transition->GetDurationMS() / 1000.0;
		data.css_property = transition->GetProperty();
		RETURN_IF_MEMORY_ERROR(dom_env->HandleEvent(data));
	}
	return OpStatus::OK;
}

/* virtual */ void
ElementTransitions::OnRemove(FramesDocument* document)
{
	OP_ASSERT(GetElm());

	if (document)
		if (LogicalDocument *logdoc = document->GetLogicalDocument())
			logdoc->GetLayoutWorkplace()->RemoveTransitionsFor(GetElm());
}

/* virtual */ void
ElementTransitions::OnDelete(FramesDocument* document)
{
	OnRemove(document);
}

ElementTransitions*
TransitionManager::GetTransitionsFor(HTML_Element* element) const
{
	ElementTransitions* ret = NULL;
	if (OpStatus::IsError(m_elm_transitions.GetData(element, &ret)))
		return NULL;
	return ret;
}

ElementTransitions*
TransitionManager::GetOrCreateTransitionsFor(HTML_Element* element)
{
	ElementTransitions* ret = GetTransitionsFor(element);
	if (!ret)
	{
		ret = OP_NEW(ElementTransitions, (element));
		if (ret && OpStatus::IsError(m_elm_transitions.Add(element, ret)))
		{
			OP_DELETE(ret);
			ret = NULL;
		}
	}
	return ret;
}

void
TransitionManager::ElementRemoved(HTML_Element* element)
{
	ElementTransitions* transitions;
	if (OpStatus::IsSuccess(m_elm_transitions.Remove(element, &transitions)))
	{
		OP_DELETE(transitions);
	}
}

void
TransitionManager::AbortTransitions(HTML_Element* element)
{
	if (ElementTransitions* transitions = GetTransitionsFor(element))
		transitions->AbortAll();
}

void
TransitionManager::Animate()
{
	BOOL active_transitions = FALSE;
	double runtime_ms = g_op_time_info->GetRuntimeMS();

	OpHashIterator* iterator = m_elm_transitions.GetIterator();
	OP_STATUS stat = iterator ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;

	if (iterator)
	{
		OpVector<ElementTransitions> delete_transitions;
		HTML_Element* docprops_elm = NULL;
		stat = iterator->First();
		while (stat == OpStatus::OK)
		{
			ElementTransitions* transitions = static_cast<ElementTransitions*>(iterator->GetData());
			if (transitions)
			{
				stat = transitions->Animate(m_doc, runtime_ms, docprops_elm);
				if (OpStatus::IsError(stat))
					break;

				if (transitions->IsEmpty())
				{
					stat = delete_transitions.Add(transitions);
					if (OpStatus::IsError(stat))
						break;
				}
				else
					if (transitions->HasPendingChange(runtime_ms))
						active_transitions = TRUE;
			}
			stat = iterator->Next();
		}
		OP_DELETE(iterator);

		for (unsigned int i = 0; i < delete_transitions.GetCount(); i++)
		{
			ElementTransitions* elm_trans = delete_transitions.Get(i);
			ElementTransitions* dummy;
			OpStatus::Ignore(m_elm_transitions.Remove(elm_trans->GetElement(), &dummy));
			OP_DELETE(elm_trans);
		}

		if (docprops_elm && !OpStatus::IsMemoryError(stat))
			stat = m_doc->GetLogicalDocument()->GetLayoutWorkplace()->RecalculateDocRootProps(docprops_elm);
	}

	if (OpStatus::IsMemoryError(stat))
		m_doc->GetHLDocProfile()->SetIsOutOfMemory(TRUE);

	m_active_transitions = active_transitions;

	if (m_active_transitions)
		m_timer.Start(STYLE_CSS_TRANSITION_INTERVAL);
}


/** Check if to and from values are component-wise both in percent or pixels
	and modify the input values accordingly.

	@param from The start border radii of the transition.
	@param to The end border radii of the transition.
	@param percent_from Are the from radii percent or pixels?
	@param percent_to Are the to radii percent or pixels?
	@return TRUE if the from and to values became compatible, otherwise FALSE. */

static inline BOOL
MakeCompatibleBorderRadii(LayoutCoord from[2], LayoutCoord to[2], BOOL percent_from[2], BOOL percent_to[2])
{
	if ((from[0] != to[0] || from[1] != to[1]) &&
		(from[0] == 0 || to[0] == 0 || (from[0] > 0) == (to[0] > 0)) &&
		(from[1] == 0 || to[1] == 0 || (from[1] > 0) == (to[1] > 0)))
	{
		for (int i=0; i<2; i++)
		{
			if (from[i] < 0)
				percent_from[i] = TRUE;
			if (to[i] < 0)
				percent_to[i] = TRUE;
			if (percent_from[i] != percent_to[i])
			{
				if (from[i] == 0)
					percent_from[i] = TRUE;
				else
				{
					OP_ASSERT(to[i] == 0);
					percent_to[i] = TRUE;
				}
			}
		}
		return TRUE;
	}
	else
		return FALSE;
}

/* Array of all animatable properties. */

static const short all_props[] = {
	CSS_PROPERTY_color,
	CSS_PROPERTY_background_color,
	CSS_PROPERTY_border_top_color,
	CSS_PROPERTY_border_left_color,
	CSS_PROPERTY_border_right_color,
	CSS_PROPERTY_border_bottom_color,
	CSS_PROPERTY_outline_color,
	CSS_PROPERTY_column_rule_color,
	CSS_PROPERTY_selection_color,
	CSS_PROPERTY_selection_background_color,
	CSS_PROPERTY_z_index,
	CSS_PROPERTY_line_height,
	CSS_PROPERTY_font_weight,
	CSS_PROPERTY_column_count,
	CSS_PROPERTY_opacity,
	CSS_PROPERTY_order,
	CSS_PROPERTY_flex_grow,
	CSS_PROPERTY_flex_shrink,
	CSS_PROPERTY_width,
	CSS_PROPERTY_height,
	CSS_PROPERTY_left,
	CSS_PROPERTY_right,
	CSS_PROPERTY_top,
	CSS_PROPERTY_bottom,
	CSS_PROPERTY_min_width,
	CSS_PROPERTY_min_height,
	CSS_PROPERTY_max_width,
	CSS_PROPERTY_max_height,
	CSS_PROPERTY_border_top_width,
	CSS_PROPERTY_border_bottom_width,
	CSS_PROPERTY_border_left_width,
	CSS_PROPERTY_border_right_width,
	CSS_PROPERTY_margin_left,
	CSS_PROPERTY_margin_right,
	CSS_PROPERTY_margin_top,
	CSS_PROPERTY_margin_bottom,
	CSS_PROPERTY_padding_left,
	CSS_PROPERTY_padding_right,
	CSS_PROPERTY_padding_top,
	CSS_PROPERTY_padding_bottom,
	CSS_PROPERTY_outline_width,
	CSS_PROPERTY_outline_offset,
	CSS_PROPERTY_word_spacing,
	CSS_PROPERTY_letter_spacing,
	CSS_PROPERTY_vertical_align,
	CSS_PROPERTY_font_size,
	CSS_PROPERTY_text_indent,
	CSS_PROPERTY_column_gap,
	CSS_PROPERTY_column_width,
	CSS_PROPERTY_column_rule_width,
	CSS_PROPERTY_flex_basis,
	CSS_PROPERTY_border_spacing,
	CSS_PROPERTY__o_object_position,
# ifdef CSS_TRANSFORMS
	CSS_PROPERTY_transform_origin,
	CSS_PROPERTY_transform,
# endif
	CSS_PROPERTY_visibility,
	CSS_PROPERTY_box_shadow,
	CSS_PROPERTY_text_shadow,
	CSS_PROPERTY_clip,
	CSS_PROPERTY_background_position,
	CSS_PROPERTY_background_size,
	CSS_PROPERTY_border_bottom_left_radius,
	CSS_PROPERTY_border_bottom_right_radius,
	CSS_PROPERTY_border_top_left_radius,
	CSS_PROPERTY_border_top_right_radius
};


/* Property arrays for shorthands. */

static const short background_props[] = {
	CSS_PROPERTY_background_color,
	CSS_PROPERTY_background_position,
	CSS_PROPERTY_background_size
};

static const short border_props[] = {
	CSS_PROPERTY_border_top_color,
	CSS_PROPERTY_border_left_color,
	CSS_PROPERTY_border_right_color,
	CSS_PROPERTY_border_bottom_color,
	CSS_PROPERTY_border_top_width,
	CSS_PROPERTY_border_left_width,
	CSS_PROPERTY_border_right_width,
	CSS_PROPERTY_border_bottom_width
};

static const short border_top_props[] = {
	CSS_PROPERTY_border_top_color,
	CSS_PROPERTY_border_top_width
};

static const short border_left_props[] = {
	CSS_PROPERTY_border_left_color,
	CSS_PROPERTY_border_left_width
};

static const short border_right_props[] = {
	CSS_PROPERTY_border_right_color,
	CSS_PROPERTY_border_right_width
};

static const short border_bottom_props[] = {
	CSS_PROPERTY_border_bottom_color,
	CSS_PROPERTY_border_bottom_width
};

static const short border_color_props[] = {
	CSS_PROPERTY_border_top_color,
	CSS_PROPERTY_border_left_color,
	CSS_PROPERTY_border_right_color,
	CSS_PROPERTY_border_bottom_color
};

static const short border_radius_props[] = {
	CSS_PROPERTY_border_bottom_left_radius,
	CSS_PROPERTY_border_bottom_right_radius,
	CSS_PROPERTY_border_top_left_radius,
	CSS_PROPERTY_border_top_right_radius
};

static const short border_width_props[] = {
	CSS_PROPERTY_border_top_width,
	CSS_PROPERTY_border_left_width,
	CSS_PROPERTY_border_right_width,
	CSS_PROPERTY_border_bottom_width
};

static const short columns_props[] = {
	CSS_PROPERTY_column_count,
	CSS_PROPERTY_column_width
};

static const short column_rule_props[] = {
	CSS_PROPERTY_column_rule_color,
	CSS_PROPERTY_column_rule_width
};

static const short font_props[] = {
	CSS_PROPERTY_font_size,
	CSS_PROPERTY_font_weight,
	CSS_PROPERTY_line_height
};

static const short margin_props[] = {
	CSS_PROPERTY_margin_left,
	CSS_PROPERTY_margin_right,
	CSS_PROPERTY_margin_top,
	CSS_PROPERTY_margin_bottom
};

static const short outline_props[] = {
	CSS_PROPERTY_outline_color,
	CSS_PROPERTY_outline_width,
	CSS_PROPERTY_outline_offset
};

static const short padding_props[] = {
	CSS_PROPERTY_padding_left,
	CSS_PROPERTY_padding_right,
	CSS_PROPERTY_padding_top,
	CSS_PROPERTY_padding_bottom
};


OP_STATUS
TransitionManager::StartTransitions(LayoutProperties* old_props, LayoutProperties* new_props, Head& extra_transitions, double runtime_ms, int& changes)
{
	OP_ASSERT(old_props->html_element && old_props->html_element == new_props->html_element);

	const HTMLayoutProperties* computed_new = new_props->GetProps();

	HTML_Element* element = old_props->html_element;
	HTMLayoutProperties* computed_old = old_props->GetProps();

	TransitionsIterator transitions(*computed_new, extra_transitions);

	ElementTransitions* elm_trans = GetTransitionsFor(element);

	if (elm_trans && !transitions.IsAll() && computed_new->transition_property != computed_old->transition_property)
	{
		int count;
		const short* props;
		transitions.GetProperties(props, count);
		changes |= elm_trans->DeleteTransitions(props, count);
	}

	TransitionDescription description;

	while (transitions.Next(description))
		RETURN_IF_ERROR(UpdateTransition(element, elm_trans, description, computed_old, computed_new, runtime_ms, changes));

	if (elm_trans && !elm_trans->IsEmpty() && !m_active_transitions)
	{
		m_active_transitions = TRUE;
		m_timer.Start(STYLE_CSS_TRANSITION_INTERVAL);
	}

	return OpStatus::OK;
}

OP_STATUS
TransitionManager::UpdateTransition(HTML_Element* element,
									ElementTransitions*& elm_trans,
									const TransitionDescription& description,
									const HTMLayoutProperties* computed_old,
									const HTMLayoutProperties* computed_new,
									double runtime_ms,
									int& changes)
{
	if (computed_new->IsTransitioning(description.property))
		/* Computed style from transitioning ascendant should
		   not start a transition. */
		return OpStatus::OK;

	PropertyTransition* prop_trans = elm_trans ? elm_trans->GetPropertyTransition(description.property) : NULL;

	BOOL delete_transition = FALSE;

	switch (description.property)
	{
		/* Color transitions below. */

	case CSS_PROPERTY_color:
		if (computed_old->font_color != computed_new->font_color || description.callback != NULL)
			RETURN_IF_ERROR(ColorTransition::UpdateTransition(description, prop_trans, computed_old->font_color, computed_new->font_color, runtime_ms));
		break;

	case CSS_PROPERTY_background_color:
		if (computed_old->bg_color != computed_new->bg_color || description.callback != NULL)
		{
			COLORREF old_col = computed_old->bg_color;
			if (old_col == USE_DEFAULT_COLOR)
				old_col = static_cast<COLORREF>(CSS_COLOR_transparent);
			COLORREF new_col = computed_new->bg_color;
			if (new_col == USE_DEFAULT_COLOR)
				new_col = static_cast<COLORREF>(CSS_COLOR_transparent);
			RETURN_IF_ERROR(ColorTransition::UpdateTransition(description, prop_trans, old_col, new_col, runtime_ms));
		}
		break;

	case CSS_PROPERTY_border_top_color:
		if (computed_old->border.top.color != computed_new->border.top.color || description.callback != NULL)
			RETURN_IF_ERROR(ColorTransition::UpdateTransition(description, prop_trans, computed_old->border.top.color, computed_new->border.top.color, runtime_ms));
		break;

	case CSS_PROPERTY_border_left_color:
		if (computed_old->border.left.color != computed_new->border.left.color || description.callback != NULL)
			RETURN_IF_ERROR(ColorTransition::UpdateTransition(description, prop_trans, computed_old->border.left.color, computed_new->border.left.color, runtime_ms));
		break;

	case CSS_PROPERTY_border_right_color:
		if (computed_old->border.right.color != computed_new->border.right.color || description.callback != NULL)
			RETURN_IF_ERROR(ColorTransition::UpdateTransition(description, prop_trans, computed_old->border.right.color, computed_new->border.right.color, runtime_ms));
		break;

	case CSS_PROPERTY_border_bottom_color:
		if (computed_old->border.bottom.color != computed_new->border.bottom.color || description.callback != NULL)
			RETURN_IF_ERROR(ColorTransition::UpdateTransition(description, prop_trans, computed_old->border.bottom.color, computed_new->border.bottom.color, runtime_ms));
		break;

	case CSS_PROPERTY_outline_color:
		if (computed_old->outline.color != computed_new->outline.color || description.callback != NULL)
			RETURN_IF_ERROR(ColorTransition::UpdateTransition(description, prop_trans, computed_old->outline.color, computed_new->outline.color, runtime_ms));
		break;

	case CSS_PROPERTY_column_rule_color:
		if (computed_old->column_rule.color != computed_new->column_rule.color || description.callback != NULL)
			RETURN_IF_ERROR(ColorTransition::UpdateTransition(description, prop_trans, computed_old->column_rule.color, computed_new->column_rule.color, runtime_ms));
		break;

	case CSS_PROPERTY_selection_color:
		if (computed_old->selection_color != computed_new->selection_color || description.callback != NULL)
		{
			COLORREF old_col = computed_old->selection_color;
			COLORREF new_col = computed_new->selection_color;
			if (old_col == USE_DEFAULT_COLOR || new_col == USE_DEFAULT_COLOR)
			{
				COLORREF def_col = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_TEXT_SELECTED);
				if (old_col == USE_DEFAULT_COLOR)
					old_col = def_col;
				if (new_col == USE_DEFAULT_COLOR)
					new_col = def_col;
			}
			RETURN_IF_ERROR(ColorTransition::UpdateTransition(description, prop_trans, old_col, new_col, runtime_ms));
		}
		break;

	case CSS_PROPERTY_selection_background_color:
		if (computed_old->selection_bgcolor != computed_new->selection_bgcolor || description.callback != NULL)
		{
			COLORREF old_col = computed_old->selection_bgcolor;
			COLORREF new_col = computed_new->selection_bgcolor;
			if (old_col == USE_DEFAULT_COLOR || new_col == USE_DEFAULT_COLOR)
			{
				COLORREF def_col = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED);
				if (old_col == USE_DEFAULT_COLOR)
					old_col = def_col;
				if (new_col == USE_DEFAULT_COLOR)
					new_col = def_col;
			}
			RETURN_IF_ERROR(ColorTransition::UpdateTransition(description, prop_trans, old_col, new_col, runtime_ms));
		}
		break;

		/* Integer transitions below. */

	case CSS_PROPERTY_z_index:
		if (computed_old->z_index >= CSS_ZINDEX_MIN_VAL &&
			computed_new->z_index >= CSS_ZINDEX_MIN_VAL)
		{
			if (computed_old->z_index != computed_new->z_index || description.callback != NULL)
				RETURN_IF_ERROR(IntegerTransition::UpdateTransition(description, prop_trans, computed_old->z_index, computed_new->z_index, runtime_ms));
		}
		else
			delete_transition = TRUE;
		break;

	case CSS_PROPERTY_line_height:
		if (computed_old->line_height_i != LINE_HEIGHT_SQUEEZE_I &&
			computed_new->line_height_i != LINE_HEIGHT_SQUEEZE_I &&
			(computed_old->line_height_i <= LayoutFixed(0) && computed_new->line_height_i <= LayoutFixed(0) ||
			 computed_old->line_height_i >= LayoutFixed(0) && computed_new->line_height_i >= LayoutFixed(0)))
		{
			if (computed_old->line_height_i != computed_new->line_height_i || description.callback != NULL)
				RETURN_IF_ERROR(LayoutFixedTransition::UpdateTransition(description, prop_trans, computed_old->line_height_i, computed_new->line_height_i, runtime_ms));
		}
		else
			delete_transition = TRUE;
		break;

	case CSS_PROPERTY_font_weight:
		if (computed_old->font_weight != computed_new->font_weight || description.callback != NULL)
			RETURN_IF_ERROR(IntegerTransition::UpdateTransition(description, prop_trans, computed_old->font_weight, computed_new->font_weight, runtime_ms));
		break;

	case CSS_PROPERTY_column_count:
		if (computed_old->column_count != SHRT_MIN && computed_new->column_count != SHRT_MIN)
		{
			if (computed_old->column_count != computed_new->column_count || description.callback != NULL)
				RETURN_IF_ERROR(IntegerTransition::UpdateTransition(description, prop_trans, computed_old->column_count, computed_new->column_count, runtime_ms));
		}
		else
			delete_transition = TRUE;
		break;

		/* Number transitions below. */

	case CSS_PROPERTY_opacity:
		if (computed_old->opacity != computed_new->opacity || description.callback != NULL)
			RETURN_IF_ERROR(NumberTransition::UpdateTransition(description, prop_trans, computed_old->opacity / 255.0, computed_new->opacity / 255.0, runtime_ms));
		break;

	case CSS_PROPERTY_order:
		if (computed_old->order != computed_new->order || description.callback != NULL)
			RETURN_IF_ERROR(IntegerTransition::UpdateTransition(description, prop_trans, computed_old->order, computed_new->order, runtime_ms));
		break;

	case CSS_PROPERTY_flex_grow:
		if (computed_old->flex_grow > 0.0 && computed_new->flex_grow > 0.0)
		{
			if (computed_old->flex_grow != computed_new->flex_grow || description.callback != NULL)
				RETURN_IF_ERROR(NumberTransition::UpdateTransition(description, prop_trans, computed_old->flex_grow, computed_new->flex_grow, runtime_ms));
		}
		else
			delete_transition = TRUE;
		break;

	case CSS_PROPERTY_flex_shrink:
		if (computed_old->flex_shrink > 0.0 && computed_new->flex_shrink > 0.0)
		{
			if (computed_old->flex_shrink != computed_new->flex_shrink || description.callback != NULL)
				RETURN_IF_ERROR(NumberTransition::UpdateTransition(description, prop_trans, computed_old->flex_shrink, computed_new->flex_shrink, runtime_ms));
		}
		else
			delete_transition = TRUE;
		break;

		/* Length transitions below. */

	case CSS_PROPERTY_width:
		if (computed_old->content_width >= CONTENT_WIDTH_MIN &&
			computed_new->content_width >= CONTENT_WIDTH_MIN &&
			(computed_old->WidthIsPercent() == computed_new->WidthIsPercent() ||
			 computed_old->content_width == 0 || computed_new->content_width == 0))
		{
			if (computed_old->content_width != computed_new->content_width || description.callback != NULL)
			{
				if (computed_new->WidthIsPercent() || computed_old->WidthIsPercent())
					RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, -computed_old->content_width, -computed_new->content_width, runtime_ms, TRUE));
				else
					RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->content_width, computed_new->content_width, runtime_ms, FALSE));
			}
		}
		else
			delete_transition = TRUE;
		break;

	case CSS_PROPERTY_height:
		if (computed_old->content_height >= CONTENT_HEIGHT_MIN &&
			computed_new->content_height >= CONTENT_HEIGHT_MIN &&
			(computed_old->GetHeightIsPercent() == computed_new->GetHeightIsPercent() ||
			 computed_old->content_height == 0 || computed_new->content_height == 0))
		{
			if (computed_new->GetHeightIsPercent() || computed_old->GetHeightIsPercent())
				RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, -computed_old->content_height, -computed_new->content_height, runtime_ms, TRUE));
			else
				RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->content_height, computed_new->content_height, runtime_ms, FALSE));
		}
		else
			if (prop_trans)
			{
				if (computed_old->content_height != computed_new->content_height || description.callback != NULL)
				{
					if (computed_new->GetHeightIsPercent() || computed_old->GetHeightIsPercent())
						RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, -computed_old->content_height, -computed_new->content_height, runtime_ms, TRUE));
					else
						RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->content_height, computed_new->content_height, runtime_ms, FALSE));
				}
			}
			else
				delete_transition = TRUE;
		break;

	case CSS_PROPERTY_left:
		if (computed_old->left != HPOSITION_VALUE_AUTO &&
			computed_new->left != HPOSITION_VALUE_AUTO)
		{
			if (computed_old->left != computed_new->left || description.callback != NULL)
				RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->left, computed_new->left, runtime_ms, FALSE));
		}
		else
			delete_transition = TRUE;
		break;

	case CSS_PROPERTY_right:
		if (computed_old->right != HPOSITION_VALUE_AUTO &&
			computed_new->right != HPOSITION_VALUE_AUTO)
		{
			if (computed_old->right != computed_new->right || description.callback != NULL)
				RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->right, computed_new->right, runtime_ms, FALSE));
		}
		else
			delete_transition = TRUE;
		break;

	case CSS_PROPERTY_top:
		if (computed_old->top != VPOSITION_VALUE_AUTO &&
			computed_new->top != VPOSITION_VALUE_AUTO)
		{
			if (computed_old->top != computed_new->top || description.callback != NULL)
				RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->top, computed_new->top, runtime_ms, FALSE));
		}
		else
			delete_transition = TRUE;
		break;

	case CSS_PROPERTY_bottom:
		if (computed_old->bottom != VPOSITION_VALUE_AUTO &&
			computed_new->bottom != VPOSITION_VALUE_AUTO)
		{
			if (computed_old->bottom != computed_new->bottom || description.callback != NULL)
				RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->bottom, computed_new->bottom, runtime_ms, FALSE));
		}
		else
			delete_transition = TRUE;
		break;

	case CSS_PROPERTY_min_width:
		if (computed_old->min_width >= 0 && computed_new->min_width >= 0)
		{
			if (computed_old->min_width != computed_new->min_width || description.callback != NULL)
				RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->min_width, computed_new->min_width, runtime_ms, FALSE));
		}
		else
			delete_transition = TRUE;
		break;

	case CSS_PROPERTY_min_height:
		if (computed_old->min_height >= 0 && computed_new->min_height >= 0)
		{
			if (computed_old->min_height != computed_new->min_height || description.callback != NULL)
				RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->min_height, computed_new->min_height, runtime_ms, FALSE));
		}
		else
			delete_transition = TRUE;
		break;

	case CSS_PROPERTY_max_width:
		if (computed_old->max_width >= 0 && computed_new->max_width >= 0)
		{
			if (computed_old->max_width != computed_new->max_width || description.callback != NULL)
				RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->max_width, computed_new->max_width, runtime_ms, FALSE));
		}
		else
			delete_transition = TRUE;
		break;

	case CSS_PROPERTY_max_height:
		if (computed_old->max_height >= 0 && computed_new->max_height >= 0)
		{
			if (computed_old->max_height != computed_new->max_height || description.callback != NULL)
				RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->max_height, computed_new->max_height, runtime_ms, FALSE));
		}
		else
			delete_transition = TRUE;
		break;

	case CSS_PROPERTY_border_top_width:
		if (computed_old->border.top.width != computed_new->border.top.width || description.callback != NULL)
			RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, LayoutCoord(computed_old->border.top.width), LayoutCoord(computed_new->border.top.width), runtime_ms, FALSE));
		break;

	case CSS_PROPERTY_border_bottom_width:
		if (computed_old->border.bottom.width != computed_new->border.bottom.width || description.callback != NULL)
			RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, LayoutCoord(computed_old->border.bottom.width), LayoutCoord(computed_new->border.bottom.width), runtime_ms, FALSE));
		break;

	case CSS_PROPERTY_border_left_width:
		if (computed_old->border.left.width != computed_new->border.left.width || description.callback != NULL)
			RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, LayoutCoord(computed_old->border.left.width), LayoutCoord(computed_new->border.left.width), runtime_ms, FALSE));
		break;

	case CSS_PROPERTY_border_right_width:
		if (computed_old->border.right.width != computed_new->border.right.width || description.callback != NULL)
			RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, LayoutCoord(computed_old->border.right.width), LayoutCoord(computed_new->border.right.width), runtime_ms, FALSE));
		break;

	case CSS_PROPERTY_margin_left:
		if (!computed_old->GetMarginLeftAuto() && !computed_new->GetMarginLeftAuto())
		{
			if (computed_old->margin_left != computed_new->margin_left || description.callback != NULL)
				RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->margin_left, computed_new->margin_left, runtime_ms, FALSE));
		}
		else
			delete_transition = TRUE;
		break;

	case CSS_PROPERTY_margin_right:
		if (!computed_old->GetMarginRightAuto() && !computed_new->GetMarginRightAuto())
		{
			if (computed_old->margin_right != computed_new->margin_right || description.callback != NULL)
				RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->margin_right, computed_new->margin_right, runtime_ms, FALSE));
		}
		else
			delete_transition = TRUE;
		break;

	case CSS_PROPERTY_margin_top:
		if (!computed_old->GetMarginTopAuto() && !computed_new->GetMarginTopAuto())
		{
			if (computed_old->margin_top != computed_new->margin_top || description.callback != NULL)
				RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->margin_top, computed_new->margin_top, runtime_ms, FALSE));
		}
		else
			delete_transition = TRUE;
		break;

	case CSS_PROPERTY_margin_bottom:
		if (!computed_old->GetMarginBottomAuto() && !computed_new->GetMarginBottomAuto())
		{
			if (computed_old->margin_bottom != computed_new->margin_bottom || description.callback != NULL)
				RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->margin_bottom, computed_new->margin_bottom, runtime_ms, FALSE));
		}
		else
			delete_transition = TRUE;
		break;

	case CSS_PROPERTY_padding_left:
		if (computed_old->padding_left != computed_new->padding_left || description.callback != NULL)
			RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->padding_left, computed_new->padding_left, runtime_ms, FALSE));
		break;

	case CSS_PROPERTY_padding_right:
		if (computed_old->padding_right != computed_new->padding_right || description.callback != NULL)
			RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->padding_right, computed_new->padding_right, runtime_ms, FALSE));
		break;

	case CSS_PROPERTY_padding_top:
		if (computed_old->padding_top != computed_new->padding_top || description.callback != NULL)
			RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->padding_top, computed_new->padding_top, runtime_ms, FALSE));
		break;

	case CSS_PROPERTY_padding_bottom:
		if (computed_old->padding_bottom != computed_new->padding_bottom || description.callback != NULL)
			RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->padding_bottom, computed_new->padding_bottom, runtime_ms, FALSE));
		break;

	case CSS_PROPERTY_outline_width:
		if (computed_old->outline.width != computed_new->outline.width || description.callback != NULL)
			RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, LayoutCoord(computed_old->outline.width), LayoutCoord(computed_new->outline.width), runtime_ms, FALSE));
		break;

	case CSS_PROPERTY_outline_offset:
		if (computed_old->outline_offset != computed_new->outline_offset || description.callback != NULL)
			RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, LayoutCoord(computed_old->outline_offset), LayoutCoord(computed_new->outline_offset), runtime_ms, FALSE));
		break;

	case CSS_PROPERTY_word_spacing:
		if (computed_old->word_spacing_i != computed_new->word_spacing_i || description.callback != NULL)
			RETURN_IF_ERROR(LayoutFixedTransition::UpdateTransition(description, prop_trans, computed_old->word_spacing_i, computed_new->word_spacing_i, runtime_ms));
		break;

	case CSS_PROPERTY_letter_spacing:
		if (computed_old->letter_spacing != computed_new->letter_spacing || description.callback != NULL)
			RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, LayoutCoord(computed_old->letter_spacing), LayoutCoord(computed_new->letter_spacing), runtime_ms, FALSE));
		break;

	case CSS_PROPERTY_vertical_align:
		if (computed_old->vertical_align_type == CSS_VALUE_UNSPECIFIED &&
			computed_new->vertical_align_type == CSS_VALUE_UNSPECIFIED)
		{
			if (computed_old->vertical_align != computed_new->vertical_align || description.callback != NULL)
				RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, LayoutCoord(computed_old->vertical_align), LayoutCoord(computed_new->vertical_align), runtime_ms, FALSE));
		}
		else
			delete_transition = TRUE;
		break;

	case CSS_PROPERTY_font_size:
		if (computed_old->decimal_font_size != computed_new->decimal_font_size || description.callback != NULL)
			RETURN_IF_ERROR(LayoutFixedTransition::UpdateTransition(description, prop_trans, computed_old->decimal_font_size, computed_new->decimal_font_size, runtime_ms));
		break;

	case CSS_PROPERTY_text_indent:
		if (computed_old->text_indent != computed_new->text_indent || description.callback != NULL)
			RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, LayoutCoord(computed_old->text_indent), LayoutCoord(computed_new->text_indent), runtime_ms, FALSE));
		break;

	case CSS_PROPERTY_column_gap:
		if (computed_old->column_gap != CONTENT_WIDTH_AUTO &&
			computed_new->column_gap != CONTENT_WIDTH_AUTO)
		{
			if (computed_old->column_gap != computed_new->column_gap || description.callback != NULL)
				RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->column_gap, computed_new->column_gap, runtime_ms, FALSE));
		}
		else
			delete_transition = TRUE;
		break;

	case CSS_PROPERTY_column_width:
		if (computed_old->column_width != CONTENT_WIDTH_AUTO &&
			computed_new->column_width != CONTENT_WIDTH_AUTO)
		{
			if (computed_old->column_width != computed_new->column_width || description.callback != NULL)
				RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->column_width, computed_new->column_width, runtime_ms, FALSE));
		}
		else
			delete_transition = TRUE;
		break;

	case CSS_PROPERTY_column_rule_width:
		if (computed_old->column_rule.width != computed_new->column_rule.width || description.callback != NULL)
			RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, LayoutCoord(computed_old->column_rule.width), LayoutCoord(computed_new->column_rule.width), runtime_ms, FALSE));
		break;

	case CSS_PROPERTY_flex_basis:
		if (computed_old->flex_basis >= CONTENT_SIZE_MIN && computed_new->flex_basis >= CONTENT_SIZE_MIN &&
			((computed_old->flex_basis < 0) == (computed_new->flex_basis < 0) ||
			 computed_old->flex_basis == 0 || computed_new->flex_basis == 0))
		{
			if (computed_old->flex_basis != computed_new->flex_basis || description.callback != NULL)
			{
				if (computed_new->flex_basis < 0 || computed_old->flex_basis < 0)
					RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, -computed_old->flex_basis, -computed_new->flex_basis, runtime_ms, TRUE));
				else
					RETURN_IF_ERROR(LengthTransition::UpdateTransition(description, prop_trans, computed_old->flex_basis, computed_new->flex_basis, runtime_ms, FALSE));
			}
		}
		else
			delete_transition = TRUE;
		break;

		/* Length2 transition below. */

	case CSS_PROPERTY_border_spacing:
		if (computed_old->border_spacing_horizontal != computed_new->border_spacing_horizontal ||
			computed_old->border_spacing_vertical != computed_new->border_spacing_vertical || description.callback != NULL)
		{
			LayoutCoord old_spacing[2] = { LayoutCoord(computed_old->border_spacing_horizontal), LayoutCoord(computed_old->border_spacing_vertical) };
			LayoutCoord new_spacing[2] = { LayoutCoord(computed_new->border_spacing_horizontal), LayoutCoord(computed_new->border_spacing_vertical) };
			BOOL percent[2] = { FALSE, FALSE };
			RETURN_IF_ERROR(Length2Transition::UpdateTransition(description, prop_trans, old_spacing, new_spacing, percent, percent, runtime_ms));
		}
		break;

	case CSS_PROPERTY__o_object_position:
		{
			LayoutCoord from[2];
			LayoutCoord to[2];
			BOOL percent_from[2];
			BOOL percent_to[2];

			from[0] = LayoutCoord(computed_old->object_fit_position.x);
			from[1] = LayoutCoord(computed_old->object_fit_position.y);

			percent_from[0] = !!computed_old->object_fit_position.x_percent;
			percent_from[1] = !!computed_old->object_fit_position.y_percent;

			to[0] = LayoutCoord(computed_new->object_fit_position.x);
			to[1] = LayoutCoord(computed_new->object_fit_position.y);

			percent_to[0] = !!computed_new->object_fit_position.x_percent;
			percent_to[1] = !!computed_new->object_fit_position.y_percent;

			if (percent_from[0] != percent_to[0])
			{
				if (from[0] == LayoutCoord(0))
					percent_from[0] = percent_to[0];
				else if (to[0] == LayoutCoord(0))
					percent_to[0] = percent_from[0];
			}
			if (percent_from[1] != percent_to[1])
			{
				if (from[1] == LayoutCoord(0))
					percent_from[1] = percent_to[1];
				else if (to[1] == LayoutCoord(0))
					percent_to[1] = percent_from[1];
			}

			if (percent_from[0] == percent_to[0] && percent_from[1] == percent_to[1])
				RETURN_IF_ERROR(Length2Transition::UpdateTransition(description, prop_trans, from, to, percent_from, percent_to, runtime_ms));
			else
				delete_transition = TRUE;
		}
		break;

# ifdef CSS_TRANSFORMS
	case CSS_PROPERTY_transform_origin:
		{
			LayoutCoord from[2];
			LayoutCoord to[2];
			BOOL percent_from[2];
			BOOL percent_to[2];

			from[0] = computed_old->transform_origin_x;
			from[1] = computed_old->transform_origin_y;

			percent_from[0] = !!computed_old->info2.transform_origin_x_is_percent;
			percent_from[1] = !!computed_old->info2.transform_origin_y_is_percent;

			to[0] = computed_new->transform_origin_x;
			to[1] = computed_new->transform_origin_y;

			percent_to[0] = !!computed_new->info2.transform_origin_x_is_percent;
			percent_to[1] = !!computed_new->info2.transform_origin_y_is_percent;

			if (percent_from[0] != percent_to[0])
			{
				if (from[0] == LayoutCoord(0))
					percent_from[0] = percent_to[0];
				else if (to[0] == LayoutCoord(0))
					percent_to[0] = percent_from[0];
			}
			if (percent_from[1] != percent_to[1])
			{
				if (from[1] == LayoutCoord(0))
					percent_from[1] = percent_to[1];
				else if (to[1] == LayoutCoord(0))
					percent_to[1] = percent_from[1];
			}

			if (percent_from[0] == percent_to[0] && percent_from[1] == percent_to[1])
			{
				if (percent_from[0] && !computed_old->info2.transform_origin_x_is_decimal)
					from[0] *= 100;
				if (percent_from[1] && !computed_old->info2.transform_origin_y_is_decimal)
					from[1] *= 100;
				if (percent_to[0] && !computed_new->info2.transform_origin_x_is_decimal)
					to[0] *= 100;
				if (percent_to[1] && !computed_new->info2.transform_origin_y_is_decimal)
					to[1] *= 100;

				RETURN_IF_ERROR(Length2Transition::UpdateTransition(description, prop_trans, from, to, percent_from, percent_to, runtime_ms));
			}
			else
				delete_transition = TRUE;
		}
		break;
# endif // CSS_TRANSFORMS

	case CSS_PROPERTY_border_bottom_left_radius:
		{
			LayoutCoord from[2];
			LayoutCoord to[2];
			BOOL percent_from[2] = { FALSE, FALSE };
			BOOL percent_to[2] = { FALSE, FALSE };

			from[0] = LayoutCoord(computed_old->border.bottom.radius_start);
			from[1] = LayoutCoord(computed_old->border.left.radius_end);
			to[0] = LayoutCoord(computed_new->border.bottom.radius_start);
			to[1] = LayoutCoord(computed_new->border.left.radius_end);

			if (from[0] < 0 && !computed_old->border.bottom.packed.radius_start_is_decimal)
				from[0] *= 100;
			if (from[1] < 0 && !computed_old->border.left.packed.radius_end_is_decimal)
				from[1] *= 100;
			if (to[0] < 0 && !computed_new->border.bottom.packed.radius_start_is_decimal)
				to[0] *= 100;
			if (to[1] < 0 && !computed_new->border.left.packed.radius_end_is_decimal)
				to[1] *= 100;

			if (MakeCompatibleBorderRadii(from, to, percent_from, percent_to))
				RETURN_IF_ERROR(Length2Transition::UpdateTransition(description, prop_trans, from, to, percent_from, percent_to, runtime_ms));
			else
				delete_transition = TRUE;
		}
		break;

	case CSS_PROPERTY_border_bottom_right_radius:
		{
			LayoutCoord from[2];
			LayoutCoord to[2];
			BOOL percent_from[2] = { FALSE, FALSE };
			BOOL percent_to[2] = { FALSE, FALSE };

			from[0] = LayoutCoord(computed_old->border.bottom.radius_end);
			from[1] = LayoutCoord(computed_old->border.right.radius_end);
			to[0] = LayoutCoord(computed_new->border.bottom.radius_end);
			to[1] = LayoutCoord(computed_new->border.right.radius_end);

			if (from[0] < 0 && !computed_old->border.bottom.packed.radius_end_is_decimal)
				from[0] *= 100;
			if (from[1] < 0 && !computed_old->border.right.packed.radius_end_is_decimal)
				from[1] *= 100;
			if (to[0] < 0 && !computed_new->border.bottom.packed.radius_end_is_decimal)
				to[0] *= 100;
			if (to[1] < 0 && !computed_new->border.right.packed.radius_end_is_decimal)
				to[1] *= 100;

			if (MakeCompatibleBorderRadii(from, to, percent_from, percent_to))
				RETURN_IF_ERROR(Length2Transition::UpdateTransition(description, prop_trans, from, to, percent_from, percent_to, runtime_ms));
			else
				delete_transition = TRUE;
		}
		break;

	case CSS_PROPERTY_border_top_left_radius:
		{
			LayoutCoord from[2];
			LayoutCoord to[2];
			BOOL percent_from[2] = { FALSE, FALSE };
			BOOL percent_to[2] = { FALSE, FALSE };

			from[0] = LayoutCoord(computed_old->border.top.radius_start);
			from[1] = LayoutCoord(computed_old->border.left.radius_start);
			to[0] = LayoutCoord(computed_new->border.top.radius_start);
			to[1] = LayoutCoord(computed_new->border.left.radius_start);

			if (from[0] < 0 && !computed_old->border.top.packed.radius_start_is_decimal)
				from[0] *= 100;
			if (from[1] < 0 && !computed_old->border.left.packed.radius_start_is_decimal)
				from[1] *= 100;
			if (to[0] < 0 && !computed_new->border.top.packed.radius_start_is_decimal)
				to[0] *= 100;
			if (to[1] < 0 && !computed_new->border.left.packed.radius_start_is_decimal)
				to[1] *= 100;

			if (MakeCompatibleBorderRadii(from, to, percent_from, percent_to))
				RETURN_IF_ERROR(Length2Transition::UpdateTransition(description, prop_trans, from, to, percent_from, percent_to, runtime_ms));
			else
				delete_transition = TRUE;
		}
		break;

	case CSS_PROPERTY_border_top_right_radius:
		{
			LayoutCoord from[2];
			LayoutCoord to[2];
			BOOL percent_from[2] = { FALSE, FALSE };
			BOOL percent_to[2] = { FALSE, FALSE };

			from[0] = LayoutCoord(computed_old->border.top.radius_end);
			from[1] = LayoutCoord(computed_old->border.right.radius_start);
			to[0] = LayoutCoord(computed_new->border.top.radius_end);
			to[1] = LayoutCoord(computed_new->border.right.radius_start);

			if (from[0] < 0 && !computed_old->border.top.packed.radius_end_is_decimal)
				from[0] *= 100;
			if (from[1] < 0 && !computed_old->border.right.packed.radius_start_is_decimal)
				from[1] *= 100;
			if (to[0] < 0 && !computed_new->border.top.packed.radius_end_is_decimal)
				to[0] *= 100;
			if (to[1] < 0 && !computed_new->border.right.packed.radius_start_is_decimal)
				to[1] *= 100;

			if (MakeCompatibleBorderRadii(from, to, percent_from, percent_to))
				RETURN_IF_ERROR(Length2Transition::UpdateTransition(description, prop_trans, from, to, percent_from, percent_to, runtime_ms));
			else
				delete_transition = TRUE;
		}
		break;

		/* Keyword transitions below. */

	case CSS_PROPERTY_visibility:
		if (computed_old->visibility != computed_new->visibility || description.callback != NULL)
			RETURN_IF_ERROR(KeywordTransition::UpdateTransition(description, prop_trans, CSSValue(computed_old->visibility), CSSValue(computed_new->visibility), runtime_ms));
		break;

		/* Declaration transitions below. */

# ifdef CSS_TRANSFORMS
	case CSS_PROPERTY_transform:
		if (computed_old->transforms_cp != computed_new->transforms_cp || description.callback != NULL)
			RETURN_IF_ERROR(TransformTransition::UpdateTransition(description, m_doc->GetVisualDevice(), prop_trans, computed_old, computed_new, runtime_ms));
		break;
# endif // CSS_TRANSFORMS

		/* Shadow transitions below. */

	case CSS_PROPERTY_box_shadow:
		if (computed_old->box_shadows.Get() != computed_new->box_shadows.Get() || description.callback != NULL)
			RETURN_IF_ERROR(GenArrayDeclTransition::UpdateTransition(description, m_doc->GetVisualDevice(), prop_trans, computed_old, computed_new, runtime_ms, CreateExtendedShadowLists));
		break;

	case CSS_PROPERTY_text_shadow:
		if (computed_old->text_shadows.Get() != computed_new->text_shadows.Get() || description.callback != NULL)
			RETURN_IF_ERROR(GenArrayDeclTransition::UpdateTransition(description, m_doc->GetVisualDevice(), prop_trans, computed_old, computed_new, runtime_ms, CreateExtendedShadowLists));
		break;

		/* Clip rectangle. */

	case CSS_PROPERTY_clip:
		if (computed_old->clip_left == CLIP_NOT_SET ||
			computed_new->clip_left == CLIP_NOT_SET)
			delete_transition = TRUE;
		else
		{
			LayoutCoord from[4];
			LayoutCoord to[4];

			from[RectTransition::TOP] = computed_old->clip_top;
			from[RectTransition::LEFT] = computed_old->clip_left;
			from[RectTransition::BOTTOM] = computed_old->clip_bottom;
			from[RectTransition::RIGHT] = computed_old->clip_right;

			to[RectTransition::TOP] = computed_new->clip_top;
			to[RectTransition::LEFT] = computed_new->clip_left;
			to[RectTransition::BOTTOM] = computed_new->clip_bottom;
			to[RectTransition::RIGHT] = computed_new->clip_right;

			for (int i=0; i<4; i++)
				if ((from[i] == CLIP_AUTO) != (to[i] == CLIP_AUTO))
				{
					delete_transition = TRUE;
					break;
				}

			if (!delete_transition)
				RETURN_IF_ERROR(RectTransition::UpdateTransition(description, prop_trans, from, to, runtime_ms));
		}
		break;

		/* Background position. */

	case CSS_PROPERTY_background_position:
		if (computed_old->bg_images.GetPositions() != computed_new->bg_images.GetPositions() || description.callback != NULL)
			RETURN_IF_ERROR(GenArrayDeclTransition::UpdateTransition(description, m_doc->GetVisualDevice(), prop_trans, computed_old, computed_new, runtime_ms, CreateBackgroundPositionLists));
		break;

		/* Background size list. */

	case CSS_PROPERTY_background_size:
		if (computed_old->bg_images.GetSizes() != computed_new->bg_images.GetSizes() || description.callback != NULL)
			RETURN_IF_ERROR(GenArrayDeclTransition::UpdateTransition(description, m_doc->GetVisualDevice(), prop_trans, computed_old, computed_new, runtime_ms, CreateBackgroundSizeLists));
		break;

	default:
		break;
	}

	if (prop_trans && delete_transition)
	{
		prop_trans->Out();
		OP_DELETE(prop_trans);
		prop_trans = NULL;
	}

	/* If the transition is not in a list, it means the old one got replaced.
	   Add the new one to the ElementTransitions. */
	if (prop_trans && !prop_trans->InList())
	{
		if (!elm_trans)
			elm_trans = GetOrCreateTransitionsFor(element);

		if (elm_trans)
			elm_trans->AddTransition(prop_trans);
		else
		{
			OP_DELETE(prop_trans);
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	if (prop_trans)
		changes |= prop_trans->Animate(runtime_ms);

	return OpStatus::OK;
}

/* static */ BOOL
TransitionManager::IsTransitionable(CSSProperty property)
{
	const int all_props_count = ARRAY_SIZE(all_props);

	for (int i=0; i < all_props_count; i++)
		if (all_props[i] == property)
			return TRUE;

	return FALSE;
}

TransitionManager::TransitionsIterator::TransitionsIterator(const HTMLayoutProperties& props, Head& extra_transitions) :
	m_i(0),
	m_shorthand_arr(NULL),
	m_shorthand_count(0),
	m_delay_arr(NULL),
	m_duration_arr(NULL),
	m_timing_arr(NULL),
	m_delay_len(0),
	m_duration_len(0),
	m_timing_len(0),
	m_extra_transitions(extra_transitions)
{
	if (props.MayStartTransition())
	{
		m_all = !props.transition_property || props.transition_property->TypeValue() == CSS_VALUE_all;

		if (m_all)
		{
			m_prop_arr = all_props;
			m_count = ARRAY_SIZE(all_props);
		}
		else if (props.transition_property->GetDeclType() == CSS_DECL_ARRAY)
		{
			m_prop_arr = props.transition_property->ArrayValue();
			m_count = static_cast<unsigned int>(props.transition_property->ArrayValueLen());
		}
		else
		{
			OP_ASSERT(props.transition_property->GetDeclType() == CSS_DECL_TYPE &&
					  props.transition_property->TypeValue() == CSS_VALUE_none);
			m_prop_arr = NULL;
			m_count = 0;
		}
	}
	else
	{
		m_all = FALSE;
		m_prop_arr = NULL;
		m_count = 0;
	}

	if (props.transition_delay)
	{
		m_delay_arr = props.transition_delay->GenArrayValue();
		m_delay_len = static_cast<unsigned int>(props.transition_delay->ArrayValueLen());
	}

	if (props.transition_duration)
	{
		m_duration_arr = props.transition_duration->GenArrayValue();
		m_duration_len = static_cast<unsigned int>(props.transition_duration->ArrayValueLen());
	}

	if (props.transition_timing)
	{
		m_timing_arr = props.transition_timing->GenArrayValue();
		m_timing_len = static_cast<unsigned int>(props.transition_timing->ArrayValueLen()) / 4;
	}
}

void
TransitionManager::TransitionsIterator::SkipTransitionsInExtra()
{
	while (m_shorthand_count > 0)
	{
		TransitionDescription* iter = static_cast<TransitionDescription*>(m_extra_transitions.First());
		while (iter)
		{
			if (CSSProperty(m_shorthand_arr[m_shorthand_count - 1]) == iter->property)
				break;

			iter = static_cast<TransitionDescription*>(iter->Suc());
		}

		if (iter)
		{
			m_shorthand_count--;

			if (m_shorthand_count == 0)
				m_i++;
		}
		else
			return;
	}

	while (m_i < m_count)
	{
		TransitionDescription* iter = static_cast<TransitionDescription*>(m_extra_transitions.First());
		while (iter)
		{
			if (CSSProperty(m_prop_arr[m_i]) == iter->property)
				break;

			iter = static_cast<TransitionDescription*>(iter->Suc());
		}

		if (iter)
			m_i++;
		else
			return;
	}
}

BOOL
TransitionManager::TransitionsIterator::Next(TransitionDescription& description)
{
	if (!m_extra_transitions.Empty() && m_i < m_count)
		SkipTransitionsInExtra();

	if (m_i >= m_count)
	{
		/* We're finished with the transitions declared as CSS Transitions,
		   now continue with transitions from CSS Animations. */
		if (TransitionDescription* first_description = static_cast<TransitionDescription*>(m_extra_transitions.First()))
		{
			description = *first_description;
			first_description->Out();
			return TRUE;
		}
		else
			return FALSE;
	}
	else if (m_shorthand_count > 0)
	{
		/* We are in progress of starting transitions for a shorthand property.
		   The description was initialized with timing data when the shorthand
		   was handled below, but we now modify the property as we go through
		   all the properties that the shorthand consists of. */
		description.property = CSSProperty(m_shorthand_arr[--m_shorthand_count]);
		OP_ASSERT(description.property == CSSProperty(GetAliasedProperty(description.property)));
		if (m_shorthand_count == 0)
			m_i++;
		return TRUE;
	}
	else
	{
		description.property = CSSProperty(GetAliasedProperty(m_prop_arr[m_i]));
		unsigned int timing_idx = m_all ? 0 : m_i;
		double duration = m_duration_arr ? m_duration_arr[timing_idx % m_duration_len].GetReal() * 1000.0 : 0;

		description.timing =
			TransitionTimingData(m_delay_arr ? m_delay_arr[timing_idx % m_delay_len].GetReal() * 1000.0 : 0,
								 duration,
								 0.0,   // start
								 1.0,   // end
								 FALSE, // extend
								 FALSE, // reverse
								 m_timing_len ? &m_timing_arr[(timing_idx % m_timing_len) * 4] : NULL);

		switch (description.property)
		{
		case CSS_PROPERTY_background:
			m_shorthand_arr = background_props;
			m_shorthand_count = ARRAY_SIZE(background_props);
			break;
		case CSS_PROPERTY_border:
			m_shorthand_arr = border_props;
			m_shorthand_count = ARRAY_SIZE(border_props);
			break;
		case CSS_PROPERTY_border_top:
			m_shorthand_arr = border_top_props;
			m_shorthand_count = ARRAY_SIZE(border_top_props);
			break;
		case CSS_PROPERTY_border_left:
			m_shorthand_arr = border_left_props;
			m_shorthand_count = ARRAY_SIZE(border_left_props);
			break;
		case CSS_PROPERTY_border_right:
			m_shorthand_arr = border_right_props;
			m_shorthand_count = ARRAY_SIZE(border_right_props);
			break;
		case CSS_PROPERTY_border_bottom:
			m_shorthand_arr = border_bottom_props;
			m_shorthand_count = ARRAY_SIZE(border_bottom_props);
			break;
		case CSS_PROPERTY_border_color:
			m_shorthand_arr = border_color_props;
			m_shorthand_count = ARRAY_SIZE(border_color_props);
			break;
		case CSS_PROPERTY_border_radius:
			m_shorthand_arr = border_radius_props;
			m_shorthand_count = ARRAY_SIZE(border_radius_props);
			break;
		case CSS_PROPERTY_border_width:
			m_shorthand_arr = border_width_props;
			m_shorthand_count = ARRAY_SIZE(border_width_props);
			break;
		case CSS_PROPERTY_columns:
			m_shorthand_arr = columns_props;
			m_shorthand_count = ARRAY_SIZE(columns_props);
			break;
		case CSS_PROPERTY_column_rule:
			m_shorthand_arr = column_rule_props;
			m_shorthand_count = ARRAY_SIZE(column_rule_props);
			break;
		case CSS_PROPERTY_font:
			m_shorthand_arr = font_props;
			m_shorthand_count = ARRAY_SIZE(font_props);
			break;
		case CSS_PROPERTY_margin:
			m_shorthand_arr = margin_props;
			m_shorthand_count = ARRAY_SIZE(margin_props);
			break;
		case CSS_PROPERTY_outline:
			m_shorthand_arr = outline_props;
			m_shorthand_count = ARRAY_SIZE(outline_props);
			break;
		case CSS_PROPERTY_padding:
			m_shorthand_arr = padding_props;
			m_shorthand_count = ARRAY_SIZE(padding_props);
			break;
		default:
			m_i++;
			return TRUE;
		}
		OP_ASSERT(m_shorthand_count > 0);
		return Next(description);
	}
}

TransitionDescription& TransitionDescription::operator=(const TransitionDescription& other)
{
	property = other.property;
	timing = other.timing;
	callback = other.callback;
	return *this;
}

#ifdef _DEBUG

Debug&
operator<<(Debug& d, const TransitionDescription& t)
{
	d << "TransitionDescription():"
	  << "\n\tProperty: " << t.property
	  << "\n\tDuration: " << t.timing.duration_ms
	  << "\n\tStart: " << t.timing.start
	  << "\n\tEnd: " << t.timing.end
	  << "\n\tExtend " << (t.timing.extend ? "YES" : "NO")
	  << "\n\tReverse: " << (t.timing.reverse ? "YES" : "NO");

	if (t.timing.timing_value)
	{
		d << "\n\tTiming: ";

		if (t.timing.timing_value->GetValueType() == CSS_INT_NUMBER)
		{
			d << " steps("  << t.timing.timing_value[0].GetNumber() << ", ";

			if (t.timing.timing_value[0].GetType() == CSS_VALUE_step_start)
				d << "start)";
			else
				d << "end)";
		}
		else
		{
				d << "cubic-bezier("
				  << t.timing.timing_value[0].GetReal() << ", "
				  << t.timing.timing_value[1].GetReal() << ", "
				  << t.timing.timing_value[2].GetReal() << ", "
				  << t.timing.timing_value[3].GetReal() << ")";
		}
	}

	return d;
}

#endif // _DEBUG

#endif // CSS_TRANSITIONS
