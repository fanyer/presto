/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef CSS_ANIMATIONS

#include "modules/style/css_animations.h"
#include "modules/style/src/css_animation.h"
#include "modules/style/css.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/style/css_property_list.h"
#include "modules/style/src/css_keyframes.h"
#include "modules/doc/frm_doc.h"
#include "modules/layout/cascade.h"
#include "modules/layout/layout_workplace.h"
#include "modules/style/css_media.h"

/** Simple wrapper to replace INT32Vector, with a OpGenericVector as a
	public base. The public base is necessary for the use of
	CopyAllToVector functionality in OpHashTable operating on a
	OpGenericVector reference. */

class PropertyVector : public OpGenericVector
{
public:
	CSSProperty Get(int i) const { return CSSProperty((INTPTR)OpGenericVector::Get(i)); }
	int GetCount() const { return (int)OpGenericVector::GetCount(); }
};

CSS_Animation::Keyframe::~Keyframe()
{
	CSS_decl::Unref(m_timing);
	CSS_decl::Unref(m_to);
}

void
CSS_Animation::Keyframe::Set(double at, CSS_decl* v, CSS_decl* t)
{
	CSS_decl::Ref(v);
	CSS_decl::Unref(m_to);
	m_to = v;

	m_at = at;

	CSS_decl::Ref(t);
	CSS_decl::Unref(m_timing);
	m_timing = t;
}

CSS_Animation::~CSS_Animation()
{
	OP_DELETEA(m_animated_properties);
	OP_DELETEA(m_keyframes);
}

/** The "simple" keyframes are used during complation of keyframes
	rules to actual animations. The word "simple" refers to the fact
	that they have only one keyframe selector (the normalized
	'position') in contrast to keyframe rule, which may have
	several. */

struct CSS_SimpleKeyframe
{
	double position;
	CSS_property_list* prop_list;
	int original_index;
};

int CompareKeyframes(const void* a, const void* b)
{
	const CSS_SimpleKeyframe* k1 = reinterpret_cast<const CSS_SimpleKeyframe*>(a);
	const CSS_SimpleKeyframe* k2 = reinterpret_cast<const CSS_SimpleKeyframe*>(b);

	double a_val = k1->position;
	double b_val = k2->position;

	if (a_val > b_val)
		return 1;
	else
		if (a_val < b_val)
			return -1;
		else
			return k1->original_index - k2->original_index;
}

OP_STATUS
CSS_Animation::CompileAnimation(CSS_KeyframesRule* keyframes)
{
	OP_NEW_DBG("CSS_Animation::CompileAnimation", "css_animations");

	/* Compiling animations consists of the following steps:

	   1. Gather all properties mentioned in any keyframe and put
		  those in a list, excluding the properties that are
		  non-animatable.

	   2. For each keyframe rule, create one simple keyframe for each
		  keyframe selector.

	   3. Sort the simple keyframes according to the keyframe selector
		  value.

	   4a. If there are no simple keyframe with keyframe selector 0,
		   create an initial implicit property keyframe for each
		   property.

	   4b. For each simple keyframe, find all set properties in that
		   keyframe and property keyframe to the property value. Also,
		   fill simple keyframes 0 and 1 with implicit property
		   keyframe for all properties.

	   4c. If there are no simple keyframe with keyframe selector 1,
		   create an final implicit property keyframe for each
		   property. */

	int decl_count = 0;
	int simple_keyframe_count = 0;

	PropertyVector properties;
	OpINT32Set prop_set;

	CSS_KeyframeRule* keyframe = keyframes->First();
	while (keyframe)
	{
		CSS_property_list* prop_list = keyframe->GetPropertyList();

		if (prop_list)
			for (CSS_decl* iter = prop_list->GetFirstDecl(); iter; iter = iter->Suc())
				if (iter->GetProperty() != CSS_PROPERTY_animation_timing_function)
					if (TransitionManager::IsTransitionable(CSSProperty(iter->GetProperty())))
					{
						RETURN_IF_MEMORY_ERROR(prop_set.Add(iter->GetProperty()));
						decl_count++;
					}

		for (CSS_KeyframeSelector* selector = keyframe->GetFirstSelector();
			 selector;
			 selector = selector->Suc())

			simple_keyframe_count++;

		keyframe = keyframe->Suc();
	}
	RETURN_IF_ERROR(prop_set.CopyAllToVector(properties));

	m_animated_properties_count = properties.GetCount();

	OP_DELETEA(m_animated_properties);
	m_animated_properties = OP_NEWA(PropertyAnimation, m_animated_properties_count);

	/* There are at most one compiled keyframe for each keyframe rule
	   and property, plus two potential implicit keyframes at start
	   and end for each property */

	OP_DELETEA(m_keyframes);
	m_keyframes = OP_NEWA(Keyframe, (2 + simple_keyframe_count) * m_animated_properties_count);
	m_keyframe_count = 0;

	CSS_SimpleKeyframe* simple_keyframes = OP_NEWA(CSS_SimpleKeyframe, simple_keyframe_count);

	CSS_decl* keyframe_timing = NULL;
	CSS_decl** to_decls = OP_NEWA(CSS_decl*, m_animated_properties_count);

	if (!to_decls || !m_animated_properties || !m_keyframes || !simple_keyframes ||
		OpStatus::IsMemoryError(m_animation_name.Set(keyframes->GetName())))
	{
		OP_DELETEA(to_decls);
		OP_DELETEA(m_animated_properties);
		OP_DELETEA(m_keyframes);
		OP_DELETEA(simple_keyframes);
		return OpStatus::ERR_NO_MEMORY;
	}

	for (int i = 0; i < m_animated_properties_count; i++)
		m_animated_properties[i].SetProperty(properties.Get(i));


	int ki = 0;
	for (CSS_KeyframeRule* iter = keyframes->First();
		 iter; iter = iter->Suc())
	{
		for (CSS_KeyframeSelector* selector = iter->GetFirstSelector();
			 selector;
			 selector = selector->Suc())
		{
			simple_keyframes[ki].position = selector->GetPosition();
			simple_keyframes[ki].original_index = ki;
			simple_keyframes[ki++].prop_list = iter->GetPropertyList();
		}
	}

	OP_ASSERT(ki == simple_keyframe_count);

	op_qsort(simple_keyframes, simple_keyframe_count, sizeof(*simple_keyframes), CompareKeyframes);

	for (int i = 0; i < m_animated_properties_count; i++)
		to_decls[i] = NULL;

	if (!simple_keyframe_count || simple_keyframes[0].position > 0)
		for (int i = 0; i < m_animated_properties_count; i++)
		{
			Keyframe& s = m_keyframes[m_keyframe_count++];
			s.Set(0.0, NULL, keyframe_timing);
			m_animated_properties[i].Append(s);
		}

	int simple_keyframe_id = 0;
	while (simple_keyframe_id < simple_keyframe_count)
	{
		CSS_SimpleKeyframe* keyframe = &simple_keyframes[simple_keyframe_id++];

		double position = keyframe->position;

		while (simple_keyframe_id < simple_keyframe_count &&
			   simple_keyframes[simple_keyframe_id].position == position)
			keyframe = &simple_keyframes[simple_keyframe_id++];

		CSS_property_list* prop_list = keyframe->prop_list;

		for (int i = 0; i < m_animated_properties_count; i++)
			to_decls[i] = NULL;

		keyframe_timing = NULL;

		for (int i = 0; i < m_animated_properties_count; i++)
			for (CSS_decl* iter = prop_list->GetFirstDecl(); iter; iter = iter->Suc())
				if (iter->GetProperty() == CSS_PROPERTY_animation_timing_function)
					keyframe_timing = iter;
				else
					if (iter->GetProperty() == properties.Get(i))
						to_decls[i] = iter;

		for (int i = 0; i < m_animated_properties_count; i++)
		{
			if (to_decls[i])
			{
				Keyframe& s = m_keyframes[m_keyframe_count++];
				s.Set(keyframe->position, to_decls[i], keyframe_timing);
				m_animated_properties[i].Append(s);
			}
			else
			{
				if (keyframe->position == 0)
				{
					Keyframe& s = m_keyframes[m_keyframe_count++];
					s.Set(0.0, NULL, keyframe_timing);
					m_animated_properties[i].Append(s);
				}
				else
					if (keyframe->position == 1.0)
					{
						Keyframe& s = m_keyframes[m_keyframe_count++];
						s.Set(1.0, NULL, keyframe_timing);
						m_animated_properties[i].Append(s);
					}
			}
		}
	}

	if (!simple_keyframe_count || simple_keyframes[simple_keyframe_count-1].position < 1.0)
		for (int i = 0; i < m_animated_properties_count; i++)
		{
			Keyframe& s = m_keyframes[m_keyframe_count++];
			s.Set(1.0, NULL, keyframe_timing);
			m_animated_properties[i].Append(s);
		}

	for (int i = 0; i < m_animated_properties_count; i++)
		m_animated_properties[i].Reset();

	if (m_reverse_iteration)
		for (int i=0; i<m_animated_properties_count; i++)
			m_animated_properties[i].Invert();

	OP_ASSERT(m_keyframe_count <= (2 + simple_keyframe_count) * m_animated_properties_count);

	OP_DELETEA(to_decls);
	OP_DELETEA(simple_keyframes);

	OP_DBG(("") << *this);

	return OpStatus::OK;
}

void
CSS_Animation::Normalize(double runtime_ms, double& position, int& iterations) const
{
	if (m_description.duration == 0)
	{
		position = 0;
		iterations = 0;
	}
	else
	{
		OP_ASSERT(runtime_ms >= m_iteration_start);

		position = (runtime_ms - m_iteration_start) / m_description.duration;

		iterations = int(position);
		position = op_fmod(position, 1.0);
	}
}

/* virtual */ TransitionCallback::TransitionState
CSS_Animation::PropertyAnimation::GetCurrentState(StateData& data)
{
	if (transition_state == PAUSED)
		data.paused_runtime_ms = paused_runtime_ms;
	else
		data.paused_runtime_ms = 0;

	return transition_state;
}

BOOL
CSS_Animation::PropertyAnimation::Apply(double position, double animation_end, BOOL reverse,
										CSS_Properties* cssprops,
										BOOL apply_partially,
										const CSS_AnimationDescription& animation_description,
										CSS_AnimationManager* animation_manager)
{
	OP_NEW_DBG("CSS_Animation::PropertyAnimation::Apply", "css_animations");

	OP_ASSERT(position >= 0.0 && position <= 1.0);

	Keyframe* from = NULL;

	if (!to)
	{
		/* Find first frame */

		to = keyframes.First();
		do
		{
			from = to;
			to = to->Suc();
		} while (to && to->At() <= position);
	}
	else
		while (to->At() < position)
		{
			from = to;
			to = to->Suc();
		}

	OP_ASSERT(to);
	transition_state = ACTIVE;

#ifdef _DEBUG
	if (from)
		OP_DBG(("At %d%%, selecting [%d%%-%d%%]", int(100*position), int(100*from->At()), int(100*to->At())));
	else
		OP_DBG(("At %d%%, applying [%d%%] keyframe", int(100*position), int(100*to->At())));
#endif // _DEBUG

	if (from)
	{
		ApplyKeyframe(from, cssprops, apply_partially);

		BOOL extend = animation_description.fill_forwards && animation_end < 1.0;

		double transition_ratio = to->At() - from->At();
		double transition_start = (position - from->At()) / transition_ratio;

		OP_ASSERT(transition_start <= 1.0);

		double transition_end = MIN(1.0, (animation_end - from->At()) / transition_ratio);

		OP_ASSERT(transition_end >= transition_start);

		double transition_duration_ms = transition_ratio * animation_description.duration;

		const CSS_generic_value* timing = reverse ? to->Timing() : from->Timing();
		if (!timing)
			timing = animation_description.timing;

		if (transition_duration_ms)
		{
			transition_description.property = property;
			transition_description.timing = TransitionTimingData(0,
																 transition_duration_ms,
																 transition_start,
																 transition_end,
																 extend, reverse, timing);
			transition_description.callback = this;

			OP_DBG(("") << transition_description);

			animation_manager->RegisterAnimatedProperty(&transition_description);
		}

		return TRUE;
	}
	else
	{
		ApplyKeyframe(to, cssprops, apply_partially);

		return FALSE;
	}
}

void
CSS_Animation::PropertyAnimation::Invert()
{
	Keyframe* old_first = keyframes.First();
	if (old_first)
		old_first->Invert();

	Keyframe* last = keyframes.Last();
	while (last != old_first)
	{
		last->Invert();

		Keyframe* move = last;
		last = last->Pred();

		move->Out();
		move->Precede(old_first);
	}
}

void CSS_Animation::PropertyAnimation::Stop()
{
	if (transition_state != FINISHED)
		transition_state = STOPPED;
}

void
CSS_Animation::PropertyAnimation::ApplyKeyframe(CSS_Animation::Keyframe* keyframe, CSS_Properties* cssprops, BOOL apply_partially)
{
	OP_NEW_DBG("CSS_Animation::PropertyAnimation::ApplyKeyframe", "css_animations");

	if (CSS_decl* decl = keyframe->Target())
	{
		OP_DBG(("\tSelecting '") << property << ": " << *decl << "'");

		OP_ASSERT(decl->GetCascadingPoint() == CSS_decl::POINT_ANIMATIONS);
		cssprops->SelectProperty(decl, 0, 0, apply_partially);
	}
}

void
CSS_Animation::PropertyAnimation::ApplyFirst(CSS_Properties* cssprops, BOOL apply_partially)
{
	Keyframe* first = keyframes.First();

	if (first && first->At() == 0.0)
		ApplyKeyframe(first, cssprops, apply_partially);
}

void
CSS_Animation::PropertyAnimation::ApplyLast(CSS_Properties* cssprops, BOOL apply_partially)
{
	Keyframe* final = to;
	if (!final && keyframes.Last() && keyframes.Last()->At() == 1.0)
		final = keyframes.Last();

	if (final)
		ApplyKeyframe(final, cssprops, apply_partially);
}

void
CSS_Animation::PropertyAnimation::ApplyPaused(CSS_Properties* cssprops, BOOL apply_partially, double runtime_ms)
{
	if (transition_state != PAUSED)
	{
		transition_state = PAUSED;
		paused_runtime_ms = runtime_ms;
	}

	if (Keyframe* paused_frame = to)
		ApplyKeyframe(paused_frame, cssprops, apply_partially);
}

/* virtual */ OP_STATUS
CSS_Animation::PropertyAnimation::TransitionEnd(FramesDocument* doc, HTML_Element* element)
{
	OP_ASSERT(element);

	if (transition_state != FINISHED)
	{
		transition_state = FINISHED;
		element->MarkPropsDirty(doc);
	}

	return OpStatus::OK;
}

void
CSS_Animation::UpdatePausedState(FramesDocument* doc, HTML_Element* element, double runtime_ms, BOOL is_paused)
{
	OP_ASSERT(element != NULL);

	if (m_state == STATE_PAUSED && !is_paused)
	{
		m_iteration_start += runtime_ms - m_iteration_paused;
		MoveTo(doc, element, STATE_ACTIVE);

		element->SetPropsDirty();
	}
	else if (m_state == STATE_ACTIVE && is_paused)
	{
		m_iteration_paused = runtime_ms;
		MoveTo(doc, element, STATE_PAUSED);

		element->SetPropsDirty();
	}
}

void
CSS_Animation::Apply(CSS_AnimationManager* animation_manager,
					 HTML_Element* element,
					 double runtime_ms,
					 CSS_Properties* cssprops,
					 BOOL apply_partially)
{
	OP_NEW_DBG("CSS_Animation::Apply", "css_animations");

	BOOL again;
	FramesDocument* doc = animation_manager->GetFramesDocument();

	/* First handle state transitions, there can be multiple state
	   transitions within one Apply. */

	do
	{
		again = FALSE;

		switch (m_state)
		{
		case STATE_PRESTART:

			if (m_animated_properties_count == 0)
				MoveTo(doc, element, STATE_FINISHED);
			else
			{
				again = TRUE;

				if (m_description.delay <= 0)
				{
					m_iteration_start = runtime_ms + m_description.delay;
					MoveTo(doc, element, STATE_ACTIVE);
				}
				else
					MoveTo(doc, element, STATE_DELAY);
			}

			break;

		case STATE_DELAY:
			if (m_delay_timer.IsStarted() && m_delay_timer.TimeSinceFiring() >= 0)
			{
				m_iteration_start = runtime_ms;
				again = TRUE;
				MoveTo(doc, element, STATE_ACTIVE);
			}
			break;

		case STATE_ACTIVE:
			{
				double position = 0.0; /* Normalized position in the animation iteration */
				int iterations;
				Normalize(runtime_ms, position, iterations);

				if ((position >= m_iterations_left || iterations > 0 && iterations >= m_iterations_left) && m_iterations_left >= 0)
				{
					m_elapsed_time += m_iterations_left * m_description.duration;

					MoveTo(doc, element, STATE_FINISHED);
				}
				else
					if (iterations > 0 && (iterations < m_iterations_left || m_iterations_left < 0))
					{
						m_elapsed_time += iterations * m_description.duration;
						m_iteration_count += iterations;

						if (m_iterations_left >= 0)
							m_iterations_left -= iterations;

						BOOL old_reverse_iteration = m_reverse_iteration;
						BOOL odd_iteration = (m_iteration_count % 2) != 0;

						m_reverse_iteration = (m_description.reverse_on_odd && odd_iteration ||
											   m_description.reverse_on_even && !odd_iteration);

						if (old_reverse_iteration != m_reverse_iteration)
							for (int i=0; i<m_animated_properties_count; i++)
								m_animated_properties[i].Invert();

						for (int i=0; i<m_animated_properties_count; i++)
							m_animated_properties[i].Reset();

#ifdef _DEBUG
						if (old_reverse_iteration != m_reverse_iteration)
							OP_DBG(("") << *this);
#endif // _DEBUG

						m_iteration_start = runtime_ms;
						position = 0.0;

						MoveTo(doc, element, STATE_ITERATION_DONE);
						MoveTo(doc, element, STATE_ACTIVE);
					}
			}
		}
	} while (again);

	/* Now act upon the current state, no more state transitions from
	   this point forward. */

	switch (m_state)
	{
	case STATE_ACTIVE:
		{
			double position = 0.0; /* Normalized position in the animation iteration */
			int dummy;
			Normalize(runtime_ms, position, dummy);

			/* Apply and set up transitions for each animated property. */

			/* The animation may end before reaching normalized
			   position 1.0, because of a non-integer
			   interation-count. 'animation_end' is the normalized
			   position where the animation ends, sent along to the
			   transition to possibly stop early. */

			double animation_end = 1.0;;
			if (m_iterations_left >= 0)
				animation_end = MIN(m_iterations_left, 1.0);

			for (int i=0; i<m_animated_properties_count; i++)
			{
				PropertyAnimation& anim = m_animated_properties[i];

				if (anim.Apply(position, animation_end, m_reverse_iteration, cssprops, apply_partially, m_description, animation_manager))
					element->SetPropsDirty();
			}
		}
		break;

	case STATE_DELAY:
		{
			if (m_description.fill_backwards)
				/* animation-fill-mode: both|backwards applies the
				   from/0% keyframe (if any) while waiting for the
				   animation to start. */

				for (int i=0; i<m_animated_properties_count; i++)
					m_animated_properties[i].ApplyFirst(cssprops, apply_partially);

			if (!m_delay_timer.IsStarted())
			{
				OP_ASSERT(m_description.delay > 0);

				m_delay_timer.Start(unsigned(m_description.delay));
				m_description.delay = 0.0;
			}
		}

		break;

	case STATE_FINISHED:
		if (m_description.fill_forwards)
			for (int i=0; i<m_animated_properties_count; i++)
				m_animated_properties[i].ApplyLast(cssprops, apply_partially);

		break;

	case STATE_PAUSED:
		for (int i=0; i<m_animated_properties_count; i++)
			m_animated_properties[i].ApplyPaused(cssprops, apply_partially, runtime_ms);

		break;
	}
}

#ifdef _DEBUG

Debug&
operator<<(Debug& d, const CSS_Animation::AnimationState& s)
{
	switch (s)
	{
	case CSS_Animation::STATE_PRESTART:
		d << "PRESTART";
		break;
	case CSS_Animation::STATE_DELAY:
		d << "DELAY";
		break;
	case CSS_Animation::STATE_ACTIVE:
		d << "ACTIVE";
		break;
	case CSS_Animation::STATE_ITERATION_DONE:
		d << "ITERATION_DONE";
		break;
	case CSS_Animation::STATE_FINISHED:
		d << "FINISHED";
		break;
	case CSS_Animation::STATE_PAUSED:
		d << "PAUSED";
		break;
	case CSS_Animation::STATE_STOPPED:
		d << "STOPPED";
		break;
	default:
		OP_ASSERT(!"Not reached");
		break;
	}

	return d;
}

#endif // _DEBUG

void
CSS_Animation::MoveTo(FramesDocument* doc, HTML_Element* element, AnimationState state)
{
	OP_NEW_DBG("CSS_Animation::MoveTo", "css_animations");

	OP_DBG(("") << m_animation_name.CStr() << ": " << m_state << " -> " << state);

	OP_STATUS status = OpStatus::OK;

	if (m_state == STATE_PRESTART && state == STATE_ACTIVE)
		status = SendAnimationEvent(doc, element, ANIMATIONSTART);
	else
		if (m_state == STATE_DELAY && state == STATE_ACTIVE)
			status = SendAnimationEvent(doc, element, ANIMATIONSTART);
		else
			if (m_state == STATE_ITERATION_DONE && state == STATE_ACTIVE)
				status = SendAnimationEvent(doc, element, ANIMATIONITERATION);
			else
				if (m_state == STATE_ACTIVE && state == STATE_FINISHED)
					status = SendAnimationEvent(doc, element, ANIMATIONEND);

	if (OpStatus::IsMemoryError(status))
		g_memory_manager->RaiseCondition(status);

	m_state = state;
}

OP_STATUS
CSS_Animation::SendAnimationEvent(FramesDocument* doc, HTML_Element* element, DOM_EventType event_type)
{
	if (!element)
		return OpStatus::OK; // Skip sending event from animations belonging to removed elements.

	RETURN_IF_ERROR(doc->ConstructDOMEnvironment());

	if (DOM_Environment* dom_env = doc->GetDOMEnvironment())
	{
		DOM_Environment::EventData data;
		data.type = event_type;
		data.target = element;

		if (event_type == ANIMATIONSTART && m_description.delay < 0)
			data.elapsed_time = - m_description.delay / 1000.0;
		else
			data.elapsed_time = m_elapsed_time / 1000.0;

		data.animation_name = m_animation_name.CStr();

		RETURN_IF_MEMORY_ERROR(dom_env->HandleEvent(data));
	}

	return OpStatus::OK;
}

/* virtual */ void
CSS_Animation::OnTimeOut(OpTimer* timer)
{
	m_animations->OnAnimationDelayTimeout();
}

BOOL
CSS_Animation::Trace(const uni_char* animation_name)
{
	if (m_traced || !uni_str_eq(animation_name, m_animation_name.CStr()))
		return FALSE;

	return m_traced = TRUE;
}

BOOL
CSS_Animation::IsRemovable() const
{
	if (m_state == STATE_STOPPED)
	{
		BOOL is_removable = TRUE;

		for (int i=0; i<m_animated_properties_count; i++)
			if (!m_animated_properties[i].IsRemovable())
				is_removable = FALSE;

		return is_removable;
	}
	else
		return FALSE;
}

void
CSS_Animation::Restart(FramesDocument* doc, HTML_Element* element)
{
	OP_ASSERT(m_state == STATE_STOPPED);

	for (int i=0; i<m_animated_properties_count; i++)
		m_animated_properties[i].Reset();

	MoveTo(doc, element, STATE_PRESTART);
}

BOOL
CSS_Animation::Stop(FramesDocument* doc, HTML_Element* element)
{
	if (m_state != STATE_STOPPED)
	{
		for (int i=0; i<m_animated_properties_count; i++)
			m_animated_properties[i].Stop();

		MoveTo(doc, element, STATE_STOPPED);

		return TRUE;
	}
	else
		return FALSE;
}

#ifdef _DEBUG

Debug&
operator<<(Debug& d, const CSS_Animation::PropertyAnimation& p)
{
	d << "\tPropertyAnimation: " << p.property;

	CSS_Animation::Keyframe* keyframe = p.keyframes.First();
	while (keyframe)
	{
		d << "\n\t " << int(keyframe->At() * 100) << "% :\t";

		if (keyframe->Target())
			d << *keyframe->Target();
		else
			d << "(implicit)";

		if (const CSS_generic_value* timing = keyframe->Timing())
		{
			if (timing->GetValueType() == CSS_INT_NUMBER)
			{
				d << " steps("  << timing[0].GetNumber() << ", ";

				if (timing[1].GetType() == CSS_VALUE_step_start)
					d << "start)";
				else
					d << "end)";
			}
			else
			{
				d << " cubic-bezier("
				  << timing[0].GetReal() << ", "
				  << timing[1].GetReal() << ", "
				  << timing[2].GetReal() << ", "
				  << timing[3].GetReal() << ")";
			}
		}

		keyframe = keyframe->Suc();
	}

	return d;
}

Debug&
operator<<(Debug& d, const CSS_Animation& a)
{
	d << "Animation: " << a.m_animation_name.CStr() << "\n";

	for (int i=0; i<a.m_animated_properties_count; i++)
		d << a.m_animated_properties[i];

	return d;
}
#endif // _DEBUG

void
CSS_ElementAnimations::Append(CSS_Animation* animation)
{
	animation->Into(&m_animations);
}

void
CSS_ElementAnimations::Apply(CSS_AnimationManager* animation_manager, double runtime_ms, CSS_Properties* cssprops, BOOL apply_partially)
{
	for (CSS_Animation* iter = m_animations.Last(); iter; iter = iter->Pred())
		iter->Apply(animation_manager, GetElm(), runtime_ms, cssprops, apply_partially);
}

BOOL
CSS_ElementAnimations::Sweep()
{
	BOOL animation_changed = FALSE;

	CSS_Animation* iter = m_animations.First();
	while (iter)
	{
		CSS_Animation* animation = iter;
		iter = iter->Suc();

		if (!animation->Traced())
		{
			if (animation->Stop(m_doc, GetElm()))
				animation_changed = TRUE;

			if (animation->IsRemovable())
			{
				animation->Out();
				OP_DELETE(animation);

				animation_changed = TRUE;
			}
		}
		else
		{
			if (animation->IsRemovable())
			{
				animation_changed = TRUE;
				animation->Restart(m_doc, GetElm());
			}

			animation->Untrace();
		}
	}

	return animation_changed;
}

void CSS_ElementAnimations::OnAnimationDelayTimeout()
{
	if (GetElm())
		GetElm()->MarkPropsDirty(m_doc);
}

CSS_Animation*
CSS_ElementAnimations::TraceAnimation(const uni_char* name)
{
	for (CSS_Animation* animation = m_animations.First();
		 animation;
		 animation = animation->Suc())

		if (animation->Trace(name))
			return animation;

	return NULL;
}

/* virtual */
void CSS_ElementAnimations::OnRemove(FramesDocument* doc)
{
	if (doc)
		if (HLDocProfile* hld_profile = doc->GetHLDocProfile())
			if (CSS_AnimationManager* animation_manager = hld_profile->GetCSSCollection()->GetAnimationManager())
				animation_manager->RemoveAnimations(GetElm());
}

/* virtual */
void CSS_ElementAnimations::OnDelete(FramesDocument* doc)
{
	OnRemove(doc);
}

FramesDocument*
CSS_AnimationManager::GetFramesDocument() const
{
	return m_css_collection->GetFramesDocument();
}

CSS_ElementAnimations*
CSS_AnimationManager::GetAnimations(HTML_Element* element)
{
	CSS_ElementAnimations* element_animations = NULL;
	m_animations.GetData(element, &element_animations);
	return element_animations;
}

OP_STATUS
CSS_AnimationManager::SetupAnimation(HTML_Element* element, CSS_ElementAnimations*& animations)
{
	animations = OP_NEW(CSS_ElementAnimations, (GetFramesDocument(), element));

	if (!animations)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status;
	if (OpStatus::IsError(status = m_animations.Add(element, animations)))
	{
		OP_DELETE(animations);
		return status;
	}

	return OpStatus::OK;
}

void
CSS_AnimationManager::ApplyAnimations(HTML_Element* element, double runtime_ms, CSS_Properties* cssprops, BOOL apply_partially)
{
	if (CSS_ElementAnimations* current_animations = GetAnimations(element))
		current_animations->Apply(this, runtime_ms, cssprops, apply_partially);
}

void
CSS_AnimationManager::RegisterAnimatedProperty(TransitionDescription* description)
{
	description->Out();
	description->Into(&m_current_transitions);
}

void
CSS_AnimationManager::RemoveAnimations(HTML_Element* element)
{
	CSS_ElementAnimations* animations;
	if (OpStatus::IsSuccess(m_animations.Remove(element, &animations)))
		OP_DELETE(animations);
}

OP_STATUS
CSS_AnimationManager::StartAnimations(LayoutProperties* cascade, double runtime_ms)
{
	HTML_Element* element = cascade->html_element;
	const HTMLayoutProperties& props = *cascade->GetProps();

	CSS_ElementAnimations* current_animations = GetAnimations(element);

	if (CSS_decl* cp = props.animation_name)
	{
		if (!current_animations)
			RETURN_IF_ERROR(SetupAnimation(element, current_animations));

		const CSS_generic_value* name_arr = cp->GenArrayValue();
		short name_arr_len = cp->ArrayValueLen();

		const CSS_generic_value* duration_arr = NULL;
		short duration_arr_len = 0;

		const CSS_generic_value* delay_arr = NULL;
		short delay_arr_len = 0;

		const CSS_generic_value* iteration_count_arr = NULL;
		short iteration_count_arr_len = 0;

		const CSS_generic_value* fill_mode_arr = NULL;
		short fill_mode_arr_len = 0;

		const CSS_generic_value* timing_arr = NULL;
		short timing_arr_len = 0;

		const CSS_generic_value* playstate_arr = NULL;
		short playstate_arr_len = 0;

		const CSS_generic_value* direction_arr = NULL;
		short direction_arr_len = 0;

		if ((cp = props.animation_duration) != NULL)
		{
			duration_arr = cp->GenArrayValue();
			duration_arr_len = cp->ArrayValueLen();
		}

		if ((cp = props.animation_delay) != NULL)
		{
			delay_arr = cp->GenArrayValue();
			delay_arr_len = cp->ArrayValueLen();
		}

		if ((cp = props.animation_iteration_count) != NULL)
		{
			iteration_count_arr = cp->GenArrayValue();
			iteration_count_arr_len = cp->ArrayValueLen();
		}

		if ((cp = props.animation_fill_mode) != NULL)
		{
			fill_mode_arr = cp->GenArrayValue();
			fill_mode_arr_len = cp->ArrayValueLen();
		}

		if ((cp = props.animation_timing_function) != NULL)
		{
			timing_arr = cp->GenArrayValue();
			timing_arr_len = cp->ArrayValueLen();
		}

		if ((cp = props.animation_play_state) != NULL)
		{
			playstate_arr = cp->GenArrayValue();
			playstate_arr_len = cp->ArrayValueLen();
		}

		if ((cp = props.animation_direction) != NULL)
		{
			direction_arr = cp->GenArrayValue();
			direction_arr_len = cp->ArrayValueLen();
		}

		for (int i=0; i<name_arr_len; i++)
		{
			const uni_char* animation_name = name_arr[i].GetString();

			if (uni_str_eq(animation_name, UNI_L("none")))
				continue;

			BOOL is_paused = playstate_arr_len && playstate_arr[i % playstate_arr_len].GetType() == CSS_VALUE_paused;

			CSS_Animation* current_animation = current_animations->TraceAnimation(animation_name);
			if (!current_animation)
			{
				CSS_AnimationDescription animation_description;

				animation_description.duration =
					duration_arr_len ? duration_arr[i % duration_arr_len].GetReal() * 1000.0 : 0.0;
				animation_description.delay =
					delay_arr_len ? delay_arr[i % delay_arr_len].GetReal() * 1000.0 : 0.0;
				animation_description.iteration_count = iteration_count_arr_len ?
					iteration_count_arr[i % iteration_count_arr_len].GetReal() : 1;
				animation_description.fill_forwards = fill_mode_arr_len &&
					(fill_mode_arr[i % fill_mode_arr_len].GetType() == CSS_VALUE_forwards ||
					 fill_mode_arr[i % fill_mode_arr_len].GetType() == CSS_VALUE_both);
				animation_description.fill_backwards = fill_mode_arr_len &&
					(fill_mode_arr[i % fill_mode_arr_len].GetType() == CSS_VALUE_backwards ||
					 fill_mode_arr[i % fill_mode_arr_len].GetType() == CSS_VALUE_both);
				animation_description.timing =
					timing_arr_len ? &timing_arr[(i*4) % timing_arr_len] : NULL;
				animation_description.is_paused = is_paused;
				animation_description.reverse_on_even =
					direction_arr_len && (direction_arr[i % direction_arr_len].GetType() == CSS_VALUE_reverse || direction_arr[i % direction_arr_len].GetType() == CSS_VALUE_alternate_reverse);
				animation_description.reverse_on_odd =
					direction_arr_len && (direction_arr[i % direction_arr_len].GetType() == CSS_VALUE_reverse || direction_arr[i % direction_arr_len].GetType() == CSS_VALUE_alternate);

				CSS_KeyframesRule* candidate_keyframes_rule = NULL;

				CSS_MediaType media_type = m_css_collection->GetFramesDocument()->GetMediaType();

				CSSCollection::Iterator iter(m_css_collection, CSSCollection::Iterator::STYLESHEETS);
				while (CSSCollectionElement* collection = iter.Next())
				{
					OP_ASSERT(collection->IsStyleSheet());
					CSS* css = static_cast<CSS*>(collection);

					if (CSS_KeyframesRule* keyframes_rule = css->GetKeyframesRule(m_css_collection->GetFramesDocument(),
																				  animation_name, media_type))
						candidate_keyframes_rule = keyframes_rule;
				}

				if (candidate_keyframes_rule)
				{
					current_animation = OP_NEW(CSS_Animation, (GetFramesDocument(), current_animations, animation_description));

					if (!current_animation ||
						OpStatus::IsMemoryError(current_animation->CompileAnimation(candidate_keyframes_rule)))

						return OpStatus::ERR_NO_MEMORY;

					current_animations->Append(current_animation);
					element->SetPropsDirty();
				}
			}
			else
			{
				/* The only dynamic change we allow to a running
				   animation is changing its play-state. */

				current_animation->UpdatePausedState(GetFramesDocument(), element, runtime_ms, is_paused);
			}
		}
	}

	if (current_animations && current_animations->Sweep())
		element->SetPropsDirty();

	return OpStatus::OK;
}

#endif // CSS_ANIMATIONS
