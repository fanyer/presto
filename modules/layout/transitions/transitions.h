/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/** @file transitions.h
 *
 * Implementation of CSS Transitions between computed values.
 *
 */

#ifndef TRANSITIONS_H
#define TRANSITIONS_H

#ifdef CSS_TRANSITIONS

#include "modules/hardcore/timer/optimer.h"
#include "modules/layout/layout_coord.h"
#include "modules/layout/layoutprops.h"
#include "modules/logdoc/elementref.h"
#include "modules/style/css_decl.h"
#include "modules/util/OpHashTable.h"
#include "modules/util/simset.h"

/** Timing functions are either defined as a stepping function or a cubic
	bezier curve. The timing function takes as its input the current elapsed
	percentage of the transition duration and outputs a percentage that
	determines how close the transition is to its final state. */

class TransitionTimingFunction
{
public:
	/** Construct and return a new timing function object based on the
		passed timing_value parameter.

		@param timing_value The value created by the CSS parser. Used
							to decide what type of function to instantiate
							and with which coefficients. NULL means 'ease'. */
	static TransitionTimingFunction* Make(const CSS_generic_value* timing_value);

	virtual ~TransitionTimingFunction() {}

	/** Call the transition timing function to get a percentage that the old value
		is transitioned towards the new value at a given percentage elapsed of the
		transition duration.

		@param x The elapsed time of the transition duration in the range [0, 1]. */
	virtual double Call(double x) = 0;

	/** Compare this timing function to another represented as CSS_generic_values.

		@param timing_value The timing coefficients to use for comparison. May be
							NULL, in which case it means 'ease'.
		@return TRUE if the in parameter represent a timing function that is
				equal to this. */
	virtual BOOL IsSameTimingFunction(const CSS_generic_value* timing_value) const = 0;
};

/** A cubic bezier parametric curve x(t), y(t), where (x,y) is
	between (0,0) and (1,1). */

class CubicBezierTimingFunction : public TransitionTimingFunction
{
	friend TransitionTimingFunction* TransitionTimingFunction::Make(const CSS_generic_value*);

public:

	/** See TransitionTimingFunction::Call for documentation. */
	virtual double Call(double x);

	/** See TransitionTimingFunction::IsSameTimingFunction for documentation. */
	virtual BOOL IsSameTimingFunction(const CSS_generic_value* timing_value) const;

	/** Enumerated indexes for bezier coordinates into bezier coordinate array. */
	enum { X1, Y1, X2, Y2, BEZIER_ARRAY_SIZE };

protected:
	/** Constructor.

		@param bezier The coefficients for the cubic-bezier function. */
	CubicBezierTimingFunction(const double bezier[BEZIER_ARRAY_SIZE]);

private:

	/** Enumerated indexes for the cubic equation coefficients into the m_cubic_coeff array. */
	enum { AX, BX, CX, AY, BY, CY, ADX, BDX, COEFF_ARRAY_SIZE };

	/** Calculate x(t), where x(t) is the x component of B(t) for the cubic bezier ease function. */
	double Bezier_x(double t) const
	{
		return t*(t*(m_cubic_coeff[AX]*t + m_cubic_coeff[BX]) + m_cubic_coeff[CX]);
	}

	/** Calculate y(t), where y(t) is the y component of B(t) for the cubic bezier ease function. */
	double Bezier_y(double t) const
	{
		return t*(t*(m_cubic_coeff[AY]*t + m_cubic_coeff[BY]) + m_cubic_coeff[CY]);
	}

	/** Calculate x'(t), where x(t) is the x component of B(t) for the cubic bezier ease function. */
	double Bezier_x_der(double t) const
	{
		return t*(t*m_cubic_coeff[ADX] + m_cubic_coeff[BDX]) + m_cubic_coeff[CX];
	}

	/** Coefficients for the cubic equation. */
	double m_cubic_coeff[COEFF_ARRAY_SIZE];
};

/** Linear timing function between (0,0) and (1,1).
	Special case of a cubic bezier function. y = x for all x. */

class LinearTimingFunction : public TransitionTimingFunction
{
	friend TransitionTimingFunction* TransitionTimingFunction::Make(const CSS_generic_value*);

public:
	/** See TransitionTimingFunction::Call for documentation. */
	virtual double Call(double x) { OP_ASSERT(x >= 0 && x <= 1); return x; }

	/** See TransitionTimingFunction::IsSameTimingFunction for documentation. */
	virtual BOOL IsSameTimingFunction(const CSS_generic_value* timing_value) const
	{
		return timing_value
			&& timing_value[0].GetValueType() == CSS_NUMBER
			&& timing_value[0].GetReal() == timing_value[1].GetReal()
			&& timing_value[2].GetReal() == timing_value[3].GetReal();
	}

protected:
	/** Constructor. Protected to make sure it is constructed from
		TransitionTimingFunction::Make only. */
	LinearTimingFunction() {}
};

/** Class for representing:
	transition-timing-function: step-start, step-end, steps(n, start/end). */

class StepTimingFunction : public TransitionTimingFunction
{
	friend TransitionTimingFunction* TransitionTimingFunction::Make(const CSS_generic_value*);

public:
	/** See TransitionTimingFunction::Call for documentation. */
	virtual double Call(double x)
	{
		if (x == 1.0)
			return 1.0;
		else
		{
			double start = m_start ? 1.0 : 0;
			return (op_floor(x * m_steps) + start) / m_steps;
		}
	}

	/** See TransitionTimingFunction::IsSameTimingFunction for documentation. */
	virtual BOOL IsSameTimingFunction(const CSS_generic_value* timing_value) const
	{
		return timing_value
			&& timing_value->GetValueType() == CSS_INT_NUMBER
			&& timing_value[0].GetNumber() == m_steps
			&& (timing_value[1].GetType() == CSS_VALUE_start) == !!m_start;
	}

protected:
	/** Construct a step function. Protected to make sure it is constructed
		from TransitionTimingFunction::Make only.

		@param steps Number of steps. Must not be 0.
		@param start TRUE if the transition triggers at the start of each interval,
					 FALSE if at the end. */
	StepTimingFunction(int steps, BOOL start)
		: m_steps(steps), m_start(start) { OP_ASSERT(steps > 0); }

private:
	/** Number of steps in the transition. Positive integer. */
	int m_steps;

	/** TRUE if the transition triggers at the start of each interval,
		FALSE if at the end. */
	BOOL m_start;
};


/** Struct for grouping transition timing data. */

struct TransitionTimingData
{
	/** Initialize members with default data.

		See corresponding members for more information. */
	TransitionTimingData() : delay_ms(0), duration_ms(0), start(0.0), end(1.0), extend(FALSE), reverse(FALSE), timing_value(NULL) {}

	/** Initialize members.

		See corresponding members for more information. */
	TransitionTimingData(double del_ms, double dur_ms, double start, double end, BOOL ext, BOOL rev, const CSS_generic_value* timing_val) : delay_ms(del_ms), duration_ms(dur_ms), start(start), end(end), extend(ext), reverse(rev), timing_value(timing_val) {}

	/** Compare timing data with regards to transition properties

		@return TRUE if the timing data are equal as in represent the same
				delay, duration and timing-function. The coefficients for
				the timing function may not be the same if they still
				represent the same function. */
	BOOL IsSame(double del_ms, double dur_ms, const TransitionTimingFunction& func) const
	{
		return del_ms == delay_ms && duration_ms == dur_ms && func.IsSameTimingFunction(timing_value);
	}

	/** @return TRUE if the timing data is so that a transition will not be immediate. */
	BOOL DoStart() const { return duration_ms > 0 && delay_ms + duration_ms > 0 || delay_ms > 0; }

	/** transition-delay */
	double delay_ms;
	/** transition-duration */
	double duration_ms;
	/** The normalized value in the [0,1] range at which the
		transition should start. Default is 0 */
	double start;
	/** The normalized value in the [0,1] range at which the
		transition should end. Default is 1. */
	double end;
	/** TRUE if the transition should stay at end value even after it
		has ended. */
	BOOL extend;
	/** TRUE if the transition function should be reversed. */
	BOOL reverse;

	/** transition-timing-function */
	const CSS_generic_value* timing_value;
};

/** Provides detailed control over how one property transition
	transition behaves */
class TransitionCallback
{
public:

	/** Called when the transition has ended. */
	virtual OP_STATUS TransitionEnd(FramesDocument* doc, HTML_Element* elm) = 0;

	/** The different states a transition can be in */
	enum TransitionState
	{
		/** Default state; when the transition is transitioning */
		ACTIVE,

		/** Paused; stays at the current position until unpaused */
		PAUSED,

		/** Stopped; the transition will be removed and deleted */
		STOPPED,

		/** Finished; reserved. Not used by the transition code itself. */
		FINISHED
	};

	struct StateData {
		double paused_runtime_ms;
	};

	/** Get state of the transition. Allows the controlling part of
		the transition to pause and stop a transition during its
		execution. */
	virtual TransitionState GetCurrentState(StateData& data) = 0;
};

/** The transition description holds all data that describes a
	transition. Returned by the iterator to allow the caller to find
	out which properties to create transitions for and describe how
	they should behave. */

struct TransitionDescription : public ListElement<TransitionDescription>
{
	TransitionDescription() : property(CSSProperty(-1)), callback(NULL) {}

	TransitionDescription& operator=(const TransitionDescription& other);

	/** The property to setup a transition for. */
	CSSProperty property;

	/** The properties relating to timing. */
	TransitionTimingData timing;

	/** The callback that allows an initiator of an transition to
		control and get notifications about a transition. */
	TransitionCallback* callback;

#ifdef _DEBUG
	friend Debug& operator<<(Debug& d, const TransitionDescription& t);
#endif // _DEBUG
};

/** Base class for computed value transitions.

	One transition object is representing an ongoing transition for a given
	property on a given HTML element. */

class PropertyTransition : public ListElement<PropertyTransition>
{
public:

	enum StateChange
	{
		TRANSITION_KEEP,
		TRANSITION_REVERSE,
		TRANSITION_REPLACE
	};

	/** Construct the computed value transition object.

		@param description Describes the transition: duration, delay, timing, etc.
		@param runtime_ms The current time in ms
		@param timing_func The timing function to use */
	PropertyTransition(const TransitionDescription& description,
					   double runtime_ms,
					   TransitionTimingFunction* timing_func)
		: m_trigger_ms(runtime_ms),
		  m_delay_ms(description.timing.delay_ms),
		  m_duration_ms(description.timing.duration_ms),
		  m_start(description.timing.start),
		  m_end(description.timing.end),
		  m_timing_func(timing_func),
		  m_property(description.property),
		  m_reverse(FALSE),
		  m_double_reverse(description.timing.reverse),
		  m_extend(description.timing.extend),
		  m_state(TransitionCallback::ACTIVE),
		  m_callback(description.callback) { OP_ASSERT(m_timing_func); }

	virtual ~PropertyTransition() { OP_DELETE(m_timing_func); }

	/** @return the CSS property for this transition. */
	CSSProperty GetProperty() const { return m_property; }

	/** Update the current transition value based on the input time.
		This method is responsible for triggering reflow/updates that
		the change to the current transition value would require.

		@param runtime_ms The current elapsed time. Relative to the same
						  starting point as the runtime_ms passed when creating
						  the transition.
		@return A set of PropertyChange bits (cssprops.h) saying what changes
				need to be applied because the current value changed. If the
				transitioned value didn't change for the delta time the transition
				progressed, the method will return CSSPROPS_CHANGED_NONE. */
	virtual int Animate(double runtime_ms) = 0;

	/** Set the computed style for the transitioned property based on the
		current value of the on-going transition.

		@param props The computed style properties for the HTML_Element.
					 The value for the transitioned property will be changed
					 by this method. */
	virtual void GetTransitionValue(HTMLayoutProperties& props) const = 0;

	/** Handle the end of a transition

		@param doc The document in which this transition happens.
		@return OpStatus::ERR_NO_MEMORY on OOM. Otherwise OpStatus::OK. */
	OP_STATUS TransitionEnd(FramesDocument* doc, HTML_Element* elm);

	/** Check if the transition has started and not reached its end state.

		@param runtime_ms Current GetRuntimeMS from OpSystemInfo.
		@return TRUE if the transition has started at runtime_ms. */
	BOOL InProgress(double runtime_ms) const { return runtime_ms >= GetStartMS(); }

	/** Peek next state of the transition.

		The transition state is updated each time the transition
		timeout is triggered and the transitions update and sometime
		it is required to know the state before that has happened.

		@return The next state of the transition. */
	TransitionCallback::TransitionState PeekNextState() { TransitionCallback::StateData dummy; return m_callback ? m_callback->GetCurrentState(dummy) : TransitionCallback::ACTIVE; }

	/** Check if the transition is finished and should be removed from the list of transitions.

		@param runtime_ms Current GetRuntimeMS from OpSystemInfo.
		@return TRUE if the transition has reached its end state at runtime_ms. */
	BOOL IsFinished(double runtime_ms) { return runtime_ms >= GetEndMS() && !m_extend; }

	/** Returns TRUE when the transition is still within its active interval */
	BOOL HasPendingChange(double runtime_ms) const { return runtime_ms < GetEndMS(); }

	/** @return the transition-duration for this transition in ms. */
	double GetDurationMS() const { return m_duration_ms; }

	/** @return at which time for GetRuntimeMS the transition should start
		(after transition-delay). */
	double GetStartMS() const { return m_trigger_ms + m_delay_ms; }

	/** @return at which time for GetRuntimeMS the transition should finish. */
	double GetEndMS() const { return m_trigger_ms + m_delay_ms + ((m_end - m_start) * m_duration_ms); }

	/** Update the state of the transition at current point in
		time.

		For a transition with a callback, the callback is asked for
		the new state, if any. */
	void UpdateState(double runtime_ms);

	BOOL HasCallback() { return m_callback != NULL; }

protected:

	/** @return y(t) for a given x(t). Returns 0 if x(t) < 0, and 1 if x(t) > 1. */
	double CallTimingFunction(double runtime_ms) const
	{
		double in = GetTimingInput(runtime_ms);
		if (in < 0.0)
			return 0.0;
		else if (in > 1.0)
			return 1.0;
		else
		{
			double out = m_timing_func->Call(in);
			if (m_double_reverse)
				out = 1 - out;
			return out;
		}
	}

	/** @return the input value to the timing function for a given elapsed time. */
	double GetTimingInput(double runtime_ms) const
	{
		double in = ((runtime_ms - GetStartMS()) / m_duration_ms) + m_start;
		if (m_reverse || m_double_reverse)
			in = 1 - in;
		return in;
	}

	/** Reverse the current transition by updating its timing members.

		@param runtime_ms The current elapsed time. Used to find the new
						  internal trigger and duration values. */
	void Reverse(double runtime_ms);

	/** Check if timing data corresponds to this transition's timing data. */
	BOOL SameTiming(const TransitionTimingData& timing) const { return timing.IsSame(m_delay_ms, m_duration_ms, *m_timing_func); }

	/** the time this transition was triggered, which is when the
		transition-delay starts. */
	double m_trigger_ms;

	/** when we are paused, this holds the last seen runtime_ms. */
	double m_paused_ms;

	/** transition-delay value. */
	double m_delay_ms;

	/** transition-duration value. */
	double m_duration_ms;

	/** the normalized value where the transition should start */
	double m_start;

	/** the normalized value where the transition should end */
	double m_end;

	/** timing function. */
	TransitionTimingFunction* m_timing_func;

	/** The CSS property whose computed value is transitioned. */
	CSSProperty m_property;

	/** The transition is reversing as described in the spec. */
	BOOL m_reverse;

	/** The transition timing function is reversed. Used to implement animation-direction:alternate */
	BOOL m_double_reverse;

	/** When TRUE, don't delete this transition when finished. Instead
		hold the last value indefinitely. The end event is still sent
		though. */
	BOOL m_extend;

	/** The current state of the transition. For a non-callback
		transition, this is always ACTIVE but callback transition can
		also be PAUSED and STOPPED. */
	TransitionCallback::TransitionState m_state;

	/** Transition callback that allows extra control and information
		about a property transition. May be NULL. */
	TransitionCallback* m_callback;
};

/** Transition class for color values. */

class ColorTransition : public PropertyTransition
{
public:
	/** Construct a color value transition with from, to, and initial transition value.

		@see PropertyTransition::PropertyTransition. */
	ColorTransition(const TransitionDescription& description,
					double runtime_ms,
					TransitionTimingFunction* timing_func,
					COLORREF from,
					COLORREF to)
					: PropertyTransition(description, runtime_ms, timing_func),
					  m_from(from),
					  m_to(to),
					  m_current(from) {}

	/** @see PropertyTransition::Animate. */
	virtual int Animate(double runtime_ms);

	/** @see PropertyTransition::GetTransitionValue. */
	virtual void GetTransitionValue(HTMLayoutProperties& props) const;

	/** Start, replace or reverse a transition of a color value based
		on the old and new computed style values and the already running
		transition if present.

		@param prop The CSS property
		@param trans The transition currently running as input. If the
					 transition is deleted, it will be NULL upon return.
					 If the transition is replaced, the new transition
					 object will be returned through this value. If the
					 transition is reversed or not changed, the object
					 will be the same as the input on return.
		@param from The new from color value.
		@param to The new to color value.
		@param runtime_ms The current elapsed time. */
	static OP_STATUS UpdateTransition(const TransitionDescription& description,
									  PropertyTransition*& trans,
									  COLORREF from,
									  COLORREF to,
									  double runtime_ms);

private:

	/** Pass the new computed style value to see if it changes the current
		to-value, if it reverses the transition, or if it's a new value that
		should replace this transition with a new one.

		@param to The new computed color.
		@param runtime_ms The current elapsed time. Relative to the same
						  starting point as the runtime_ms passed when creating
						  the transition.
		@param timing delay, duration and timing-function data for the new to-value.
		@return TRANSITION_KEEP if the computed style didn't change,
				TRANSITION_REVERSE if the transition is reversed,
				and TRANSITION_REPLACE if it should be replaced by
				a new transition. */
	StateChange NewToValue(COLORREF to, double runtime_ms, const TransitionTimingData& timing);

	/** The color for the transition start point. */
	COLORREF m_from;
	/** The color for the transition end point. */
	COLORREF m_to;
	/** The current color for the transition. */
	COLORREF m_current;
};

/** Transition class for length values (pixels and percentages). */

class LengthTransition : public PropertyTransition
{
public:
	/** Construct a length value transition with from, to, and initial transition value.

		@see PropertyTransition::PropertyTransition. */
	LengthTransition(const TransitionDescription& description,
					 double runtime_ms,
					 TransitionTimingFunction* timing_func,
					 LayoutCoord from,
					 LayoutCoord to)
					: PropertyTransition(description, runtime_ms, timing_func),
					   m_from(from),
					   m_to(to),
					   m_current(from) {}

	/* @return TRUE if the transition is a percentage transition. */
	virtual BOOL IsPercentage() const { return FALSE; }

	/** @see PropertyTransition::Animate. */
	virtual int Animate(double runtime_ms);

	/** @see PropertyTransition::GetTransitionValue. */
	virtual void GetTransitionValue(HTMLayoutProperties& props) const;

	/** Start, replace or reverse a transition of a length value based
		on the old and new computed style values and the already running
		transition if present.

		@param description Properties defining the updated transition
		@param trans The transition currently running as input. If the
					 transition is deleted, it will be NULL upon return.
					 If the transition is replaced, the new transition
					 object will be returned through this value. If the
					 transition is reversed or not changed, the object
					 will be the same as the input on return.
		@param from The new from length value.
		@param to The new to length value.
		@param runtime_ms The current elapsed time.
		@param percentage TRUE if the from and to values are percentages.
						  If trans is not NULL, it has to be a
						  PercentageTransition if this parameter is TRUE
						  and vice versa. */
	static OP_STATUS UpdateTransition(const TransitionDescription& description,
									  PropertyTransition*& trans,
									  LayoutCoord from,
									  LayoutCoord to,
									  double runtime_ms,
									  BOOL percentage);

protected:

	/** Pass the new computed style value to see if it changes the current
		to-value, if it reverses the transition, or if it's a new value that
		should replace this transition with a new one.

		@param to The new computed length.
		@param runtime_ms The current elapsed time. Relative to the same
						  starting point as the runtime_ms passed when creating
						  the transition.
		@param timing delay, duration and timing-function data for the new to-value.
		@return TRANSITION_KEEP if the computed style didn't change,
				TRANSITION_REVERSE if the transition is reversed,
				and TRANSITION_REPLACE if it should be replaced by
				a new transition. */
	StateChange NewToValue(LayoutCoord to, double runtime_ms, const TransitionTimingData& timing);

	/** The length for the transition start point. */
	LayoutCoord m_from;
	/** The length for the transition end point. */
	LayoutCoord m_to;
	/** The current length for the transition. */
	LayoutCoord m_current;
};

/** Transition class for percentages. */

class PercentageTransition : public LengthTransition
{
public:
	/** Construct a percentage value transition with from, to, and initial transition value.

		@see PropertyTransition::PropertyTransition. */
	PercentageTransition(const TransitionDescription& description,
						 double runtime_ms,
						 TransitionTimingFunction* timing_func,
						 LayoutCoord from,
						 LayoutCoord to)
						 : LengthTransition(description, runtime_ms, timing_func, from, to) {}

	/* @return TRUE if the transition is a percentage transition. */
	virtual BOOL IsPercentage() const { return TRUE; }

	/** @see PropertyTransition::GetTransitionValue. */
	virtual void GetTransitionValue(HTMLayoutProperties& props) const;
};

/** Transition class for two length values (pixels/percentages). */

class Length2Transition : public PropertyTransition
{
public:
	/** Construct a "<length> <length>" value transition with from, to, and
		initial transition value.

		@see PropertyTransition::PropertyTransition. */
	Length2Transition(const TransitionDescription& description,
					  double runtime_ms,
					  TransitionTimingFunction* timing_function,
					  const LayoutCoord from[2],
					  const LayoutCoord to[2],
					  BOOL percent[2])
					  : PropertyTransition(description, runtime_ms, timing_function)
	{
		m_from[0] = m_current[0] = from[0];
		m_from[1] = m_current[1] = from[1];
		m_to[0] = to[0];
		m_to[1] = to[1];
		m_packed_init = 0;
		m_packed.is_percent1 = !!percent[0];
		m_packed.is_percent2 = !!percent[1];
	}

	/** @see PropertyTransition::Animate. */
	virtual int Animate(double runtime_ms);

	/** @see PropertyTransition::GetTransitionValue. */
	virtual void GetTransitionValue(HTMLayoutProperties& props) const;

	/** Start, replace or reverse a transition of two length values based
		on the old and new computed style values and the already running
		transition if present.

		@param description Properties defining the updated transition
		@param trans The transition currently running as input. If the
					 transition is deleted, it will be NULL upon return.
					 If the transition is replaced, the new transition
					 object will be returned through this value. If the
					 transition is reversed or not changed, the object
					 will be the same as the input on return.
		@param from The new from length values.
		@param to The new to length values.
		@param percent_from Are the from values percentage values?
		@param percent_to Are the to values percentage values?
		@param runtime_ms The current elapsed time.
		@return ERR_NO_MEMORY on OOM. */
	static OP_STATUS UpdateTransition(const TransitionDescription& description,
									  PropertyTransition*& trans,
									  const LayoutCoord from[2],
									  const LayoutCoord to[2],
									  BOOL percent_from[2],
									  BOOL percent_to[2],
									  double runtime_ms);

protected:

	/** Pass the new computed style value to see if it changes the current
		to-value, if it reverses the transition, or if it's a new value that
		should replace this transition with a new one.

		@param to The new computed lengths.
		@param runtime_ms The current elapsed time. Relative to the same
						  starting point as the runtime_ms passed when creating
						  the transition.
		@param timing delay, duration and timing-function data for the new to-value.
		@return TRANSITION_KEEP if the computed style didn't change,
				TRANSITION_REVERSE if the transition is reversed,
				and TRANSITION_REPLACE if it should be replaced by
				a new transition. */
	StateChange NewToValue(const LayoutCoord to[2], double runtime_ms, const TransitionTimingData& timing);

	/** The lengths for the transition start point. */
	LayoutCoord m_from[2];
	/** The lengths for the transition end point. */
	LayoutCoord m_to[2];
	/** The current lengths for the transition. */
	LayoutCoord m_current[2];
	/** Packed structure. */
	union
	{
		struct
		{
			/** Is the first length a percentage value. */
			unsigned int is_percent1:1;
			/** Is the second length a percentage value. */
			unsigned int is_percent2:1;
		} m_packed;
		UINT32 m_packed_init;
	};
};

/** Transition class for a rectangle (pixels lengths). */

class RectTransition : public PropertyTransition
{
public:
	/** Construct a rectangle transition with from, to, and
		initial transition value.

		@see PropertyTransition::PropertyTransition. */

	enum { TOP, LEFT, BOTTOM, RIGHT };

	RectTransition(const TransitionDescription& description,
				   double runtime_ms,
				   TransitionTimingFunction* timing_function,
				   const LayoutCoord from[4],
				   const LayoutCoord to[4])
				   : PropertyTransition(description, runtime_ms, timing_function)
	{
		m_from[TOP] = m_current[TOP] = from[TOP];
		m_from[LEFT] = m_current[LEFT] = from[LEFT];
		m_from[BOTTOM] = m_current[BOTTOM] = from[BOTTOM];
		m_from[RIGHT] = m_current[RIGHT] = from[RIGHT];
		m_to[TOP] = to[TOP];
		m_to[LEFT] = to[LEFT];
		m_to[BOTTOM] = to[BOTTOM];
		m_to[RIGHT] = to[RIGHT];
	}

	/** @see PropertyTransition::Animate. */
	virtual int Animate(double runtime_ms);

	/** @see PropertyTransition::GetTransitionValue. */
	virtual void GetTransitionValue(HTMLayoutProperties& props) const;

	/** Start, replace or reverse a rectangle transition based on the old and
		new computed style values and the already running transition if present.

		@param description Properties defining the updated transition
		@param trans The transition currently running as input. If the
					 transition is deleted, it will be NULL upon return.
					 If the transition is replaced, the new transition
					 object will be returned through this value. If the
					 transition is reversed or not changed, the object
					 will be the same as the input on return.
		@param from The new from rectangle.
		@param to The new to rectangle.
		@param runtime_ms The current elapsed time.
		@return ERR_NO_MEMORY on OOM. */
	static OP_STATUS UpdateTransition(const TransitionDescription& description,
									  PropertyTransition*& trans,
									  const LayoutCoord from[4],
									  const LayoutCoord to[4],
									  double runtime_ms);

protected:

	/** Pass the new computed style value to see if it changes the current
		to-value, if it reverses the transition, or if it's a new value that
		should replace this transition with a new one.

		@param to The new computed lengths.
		@param runtime_ms The current elapsed time. Relative to the same
						  starting point as the runtime_ms passed when creating
						  the transition.
		@param timing delay, duration and timing-function data for the new to-value.
		@return TRANSITION_KEEP if the computed style didn't change,
				TRANSITION_REVERSE if the transition is reversed,
				and TRANSITION_REPLACE if it should be replaced by
				a new transition. */
	StateChange NewToValue(const LayoutCoord to[4], double runtime_ms, const TransitionTimingData& timing);

	/** The rectangle for the transition start point. */
	LayoutCoord m_from[4];
	/** The rectangle for the transition end point. */
	LayoutCoord m_to[4];
	/** The current rectangle for the transition. */
	LayoutCoord m_current[4];
};

/** Transition class for integer values. */

class IntegerTransition : public PropertyTransition
{
public:
	/** Construct an integer value transition with from, to, and initial transition value.

		@see PropertyTransition::PropertyTransition. */
	IntegerTransition(const TransitionDescription& description,
					  double runtime_ms,
					  TransitionTimingFunction* timing_func,
					  int from,
					  int to)
					  : PropertyTransition(description, runtime_ms, timing_func),
						m_from(from),
						m_to(to),
						m_current(from) {}

	/** @see PropertyTransition::Animate. */
	virtual int Animate(double runtime_ms);

	/** @see PropertyTransition::GetTransitionValue. */
	virtual void GetTransitionValue(HTMLayoutProperties& props) const;

	/** Start, replace or reverse a transition of an integer value based
		on the old and new computed style values and the already running
		transition if present.

		@param description Properties defining the updated transition
		@param trans The transition currently running as input. If the
					 transition is deleted, it will be NULL upon return.
					 If the transition is replaced, the new transition
					 object will be returned through this value. If the
					 transition is reversed or not changed, the object
					 will be the same as the input on return.
		@param from The new from integer value.
		@param to The new to integer value.
		@param runtime_ms The current elapsed time. */
	static OP_STATUS UpdateTransition(const TransitionDescription& description,
									  PropertyTransition*& trans,
									  int from,
									  int to,
									  double runtime_ms);

private:

	/** Pass the new computed style value to see if it changes the current
		to-value, if it reverses the transition, or if it's a new value that
		should replace this transition with a new one.

		@param to The new computed integer.
		@param runtime_ms The current elapsed time. Relative to the same
						  starting point as the runtime_ms passed when creating
						  the transition.
		@param timing delay, duration and timing-function data for the new to-value.
		@return TRANSITION_KEEP if the computed style didn't change,
				TRANSITION_REVERSE if the transition is reversed,
				and TRANSITION_REPLACE if it should be replaced by
				a new transition. */
	StateChange NewToValue(int to, double runtime_ms, const TransitionTimingData& timing);

	/** The integer for the transition start point. */
	int m_from;
	/** The integer for the transition end point. */
	int m_to;
	/** The current integer for the transition. */
	int m_current;
};

/** Transition class for LayoutFixed values. */

class LayoutFixedTransition : public PropertyTransition
{
public:
	/** Construct a LayoutFixed value transition with from, to, and initial transition value.

		@see PropertyTransition::PropertyTransition. */
	LayoutFixedTransition(const TransitionDescription& description,
						  double runtime_ms,
						  TransitionTimingFunction* timing_func,
						  LayoutFixed from,
						  LayoutFixed to)
						  : PropertyTransition(description, runtime_ms, timing_func),
						    m_from(from),
							m_to(to),
							m_current(from) {}

	/** @see PropertyTransition::Animate. */
	virtual int Animate(double runtime_ms);

	/** @see PropertyTransition::GetTransitionValue. */
	virtual void GetTransitionValue(HTMLayoutProperties& props) const;

	/** Start, replace or reverse a transition of a fixed point value based
		on the old and new computed style values and the already running
		transition if present.

		@param description Properties defining the updated transition
		@param trans The transition currently running as input. If the
					 transition is deleted, it will be NULL upon return.
					 If the transition is replaced, the new transition
					 object will be returned through this value. If the
					 transition is reversed or not changed, the object
					 will be the same as the input on return.
		@param from The new from fixed point value.
		@param to The new to fixed point value.
		@param runtime_ms The current elapsed time. */
	static OP_STATUS UpdateTransition(const TransitionDescription& description,
									  PropertyTransition*& trans,
									  LayoutFixed from,
									  LayoutFixed to,
									  double runtime_ms);

protected:

	/** Pass the new computed style value to see if it changes the current
		to-value, if it reverses the transition, or if it's a new value that
		should replace this transition with a new one.

		@param to The new computed fixed point value.
		@param runtime_ms The current elapsed time. Relative to the same
						  starting point as the runtime_ms passed when creating
						  the transition.
		@param timing delay, duration and timing-function data for the new to-value.
		@return TRANSITION_KEEP if the computed style didn't change,
				TRANSITION_REVERSE if the transition is reversed,
				and TRANSITION_REPLACE if it should be replaced by
				a new transition. */
	StateChange NewToValue(LayoutFixed to, double runtime_ms, const TransitionTimingData& timing);

	/** The fixed point value at the transition start point. */
	LayoutFixed m_from;
	/** The fixed point value at the transition end point. */
	LayoutFixed m_to;
	/** The current fixed point value for the transition. */
	LayoutFixed m_current;
};

/** Transition class for number values. */

class NumberTransition : public PropertyTransition
{
public:
	/** Construct a number value transition with from, to, and initial transition value.

		@see PropertyTransition::PropertyTransition. */
	NumberTransition(const TransitionDescription& description,
					 double runtime_ms,
					 TransitionTimingFunction* timing_func,
					 double from,
					 double to)
					 : PropertyTransition(description, runtime_ms, timing_func),
					   m_from(from),
					   m_to(to),
					   m_current(from) {}

	/** @see PropertyTransition::Animate. */
	virtual int Animate(double runtime_ms);

	/** @see PropertyTransition::GetTransitionValue. */
	virtual void GetTransitionValue(HTMLayoutProperties& props) const;

	/** Start, replace or reverse a transition of a number value based
		on the old and new computed style values and the already running
		transition if present.

		@param description Properties defining the updated transition
		@param trans The transition currently running as input. If the
					 transition is deleted, it will be NULL upon return.
					 If the transition is replaced, the new transition
					 object will be returned through this value. If the
					 transition is reversed or not changed, the object
					 will be the same as the input on return.
		@param from The new from number value.
		@param to The new to number value.
		@param runtime_ms The current elapsed time. */
	static OP_STATUS UpdateTransition(const TransitionDescription& description,
									  PropertyTransition*& trans,
									  double from,
									  double to,
									  double runtime_ms);

private:

	/** Pass the new computed style value to see if it changes the current
		to-value, if it reverses the transition, or if it's a new value that
		should replace this transition with a new one.

		@param to The new computed number.
		@param runtime_ms The current elapsed time. Relative to the same
						  starting point as the runtime_ms passed when creating
						  the transition.
		@param timing delay, duration and timing-function data for the new to-value.
		@return TRANSITION_KEEP if the computed style didn't change,
				TRANSITION_REVERSE if the transition is reversed,
				and TRANSITION_REPLACE if it should be replaced by
				a new transition. */
	StateChange NewToValue(double to, double runtime_ms, const TransitionTimingData& timing);

	/** The number for the transition start point. */
	double m_from;
	/** The number for the transition end point. */
	double m_to;
	/** The current number for the transition. */
	double m_current;
};

/** Transition class for keywords. */

class KeywordTransition : public PropertyTransition
{
public:
	/** Construct a keyword transition with from, to, and initial transition value.

		@see PropertyTransition::PropertyTransition. */
	KeywordTransition(const TransitionDescription& description,
					  double runtime_ms,
					  TransitionTimingFunction* timing_func,
					  CSSValue from,
					  CSSValue to)
					  : PropertyTransition(description, runtime_ms, timing_func),
						m_from(from),
						m_to(to),
						m_current(from) {}

	/** @see PropertyTransition::Animate. */
	virtual int Animate(double runtime_ms);

	/** @see PropertyTransition::GetTransitionValue. */
	virtual void GetTransitionValue(HTMLayoutProperties& props) const;

	/** Start, replace or reverse a transition of a keyword value based
		on the old and new computed style values and the already running
		transition if present.

		@param description Properties defining the updated transition
		@param trans The transition currently running as input. If the
					 transition is deleted, it will be NULL upon return.
					 If the transition is replaced, the new transition
					 object will be returned through this value. If the
					 transition is reversed or not changed, the object
					 will be the same as the input on return.
		@param from The new from keyword.
		@param to The new to keyword.
		@param runtime_ms The current elapsed time. */
	static OP_STATUS UpdateTransition(const TransitionDescription& description,
									  PropertyTransition*& trans,
									  CSSValue from,
									  CSSValue to,
									  double runtime_ms);

private:

	/** Pass the new computed style value to see if it changes the current
		to-value, if it reverses the transition, or if it's a new value that
		should replace this transition with a new one.

		@param to The new computed keyword.
		@param runtime_ms The current elapsed time. Relative to the same
						  starting point as the runtime_ms passed when creating
						  the transition.
		@param timing delay, duration and timing-function data for the new to-value.
		@return TRANSITION_KEEP if the computed style didn't change,
				TRANSITION_REVERSE if the transition is reversed,
				and TRANSITION_REPLACE if it should be replaced by
				a new transition. */
	StateChange NewToValue(CSSValue to, double runtime_ms, const TransitionTimingData& timing);

	/** The keyword for the transition start point. */
	CSSValue m_from;
	/** The keyword for the transition end point. */
	CSSValue m_to;
	/** The current keyword for the transition. */
	CSSValue m_current;
};

# ifdef CSS_TRANSFORMS

/** Transition class for CSS_transform_list values. */

class TransformTransition : public PropertyTransition
{
public:
	/** Construct a transition between two CSS_transform_list objects with
		from and to values.

		@see PropertyTransition::PropertyTransition. */
	TransformTransition(const TransitionDescription& description,
						double runtime_ms,
						TransitionTimingFunction* timing_function,
						CSS_transform_list* from,
						CSS_transform_list* to)
						: PropertyTransition(description, runtime_ms, timing_function),
						  m_from(from),
						  m_to(to),
						  m_current(NULL)
	{
		OP_ASSERT(m_from && m_to);
		m_from->Ref();
		m_to->Ref();
	}

	/* Destructor. Dereference declarations. */
	virtual ~TransformTransition()
	{
		CSS_decl::Unref(m_from);
		CSS_decl::Unref(m_to);
		CSS_decl::Unref(m_current);
	}

	/** Second phase constructor. Allocates current state.

		@return ERR_NO_MEMORY on OOM, otherwise OK. */
	OP_STATUS Construct();

	/** @see PropertyTransition::Animate. */
	virtual int Animate(double runtime_ms);

	/** @see PropertyTransition::GetTransitionValue. */
	virtual void GetTransitionValue(HTMLayoutProperties& props) const;

	/** Start, replace or reverse a transition of a CSS transform based
		on the old and new computed style values and the already running
		transition if present.

		@param vis_dev The VisualDevice for the document. Used for setting
					   the correct values for resolving relative lengths within
					   the transforms.
		@param trans The transition currently running as input. If the
					 transition is deleted, it will be NULL upon return.
					 If the transition is replaced, the new transition
					 object will be returned through this value. If the
					 transition is reversed or not changed, the object
					 will be the same as the input on return.
		@param computed_old The old computed styles.
		@param computed_new The new computed styles.
		@param runtime_ms The current elapsed time. */
	static OP_STATUS UpdateTransition(const TransitionDescription& description,
									  VisualDevice* vis_dev,
									  PropertyTransition*& trans,
									  const HTMLayoutProperties* computed_old,
									  const HTMLayoutProperties* computed_new,
									  double runtime_ms);

private:

	/** Pass the new computed style value to see if it changes the current
		to-value, if it reverses the transition, or if it's a new value that
		should replace this transition with a new one.

		@param to The new computed transform.
		@param runtime_ms The current elapsed time. Relative to the same
						  starting point as the runtime_ms passed when creating
						  the transition.
		@param timing delay, duration and timing-function data for the new to-value.
		@return TRANSITION_KEEP if the computed style didn't change,
				TRANSITION_REVERSE if the transition is reversed,
				and TRANSITION_REPLACE if it should be replaced by
				a new transition. */
	StateChange NewToValue(const CSS_decl* to, double runtime_ms, const TransitionTimingData& timing);

	/** The declaration representation for the transition start point. */
	CSS_transform_list* m_from;
	/** The declaration representation for the transition end point. */
	CSS_transform_list* m_to;
	/** The current declaration representation for the transition. */
	CSS_transform_list* m_current;
};

# endif // CSS_TRANSFORMS

/** Transition class for properties that use CSS_gen_array_decl. */

class GenArrayDeclTransition : public PropertyTransition
{
public:

	/** Function parameters to CreateArrayDeclFunction. */
	struct Data
	{
		/** Constructor. Initialize members. */
		Data(VisualDevice* vis_dev,
			 const HTMLayoutProperties* old_props,
			 const HTMLayoutProperties* new_props,
			 CSS_gen_array_decl* old_decl,
			 CSS_gen_array_decl* new_decl)
			 : from_decl(old_decl), to_decl(new_decl),
			   computed_old(old_props), computed_new(new_props),
			   length_resolver_old(vis_dev), length_resolver_new(vis_dev)
		{
			old_props->SetDisplayProperties(vis_dev);
			length_resolver_old.FillFromVisualDevice();
			new_props->SetDisplayProperties(vis_dev);
			length_resolver_new.FillFromVisualDevice();
		}

		/** The from declaration object to create an extended copy from. May
			be NULL, which means 'none'. */
		CSS_gen_array_decl* from_decl;
		/** The to declaration object to create an extended copy from. May be
			NULL, which means 'none'. */
		CSS_gen_array_decl* to_decl;
		/** The computed styles for the from values. */
		const HTMLayoutProperties* computed_old;
		/** The computed styles for the to values. */
		const HTMLayoutProperties* computed_new;
		/** Used for resolving relative lengths in the from_decl. */
		CSSLengthResolver length_resolver_old;
		/** Used for resolving relative lengths in the to_decl. */
		CSSLengthResolver length_resolver_new;
	};

	/** Function declaration to create two transitionable declarations to
		transition between based on the computed styles in 'data'.

		@param data Computed style parameters.
		@return IS_TRUE - data contains two new decl objects in from_decl
				and to_decl that the caller is responsible to keep alive
				by referencing/dereferencing them.
				IS_FALSE - the declarations were incompatible and couldn't
				be transitioned between. ERR_NO_MEMORY on OOM. */
	typedef OP_BOOLEAN (*CreateArrayDeclFunction)(Data& data);

	/** Construct a transition between two CSS_gen_array_decl objects with
		from and to values.

		@see PropertyTransition::PropertyTransition. */
	GenArrayDeclTransition(const TransitionDescription& description,
						   double runtime_ms,
						   TransitionTimingFunction* timing_function,
						   CSS_gen_array_decl* from,
						   CSS_gen_array_decl* to)
						   : PropertyTransition(description, runtime_ms, timing_function),
						     m_from(from),
							 m_to(to),
							 m_current(NULL)
	{
		OP_ASSERT(m_from && m_to);
		CSS_decl::Ref(m_from);
		CSS_decl::Ref(m_to);
	}

	/* Destructor. Dereference declarations. */
	virtual ~GenArrayDeclTransition()
	{
		CSS_decl::Unref(m_from);
		CSS_decl::Unref(m_to);
		CSS_decl::Unref(m_current);
	}

	/** Second phase constructor. Allocates current state.

		@return ERR_NO_MEMORY on OOM, otherwise OK. */
	OP_STATUS Construct();

	/** @see PropertyTransition::Animate. */
	virtual int Animate(double runtime_ms);

	/** @see PropertyTransition::GetTransitionValue. */
	virtual void GetTransitionValue(HTMLayoutProperties& props) const;

	/** Start, replace or reverse a GenArrayDeclTransition.

		@param description Properties defining the updated transition
		@param vis_dev The VisualDevice for the document. Used for setting
					   the correct values for resolving relative lengths within
					   the declaration.
		@param trans The transition currently running as input. If the
					 transition is deleted, it will be NULL upon return.
					 If the transition is replaced, the new transition
					 object will be returned through this value. If the
					 transition is reversed or not changed, the object
					 will be the same as the input on return.
		@param computed_old The old computed styles.
		@param computed_new The new computed styles.
		@param runtime_ms The current elapsed time. */
	static OP_STATUS UpdateTransition(const TransitionDescription& description,
									  VisualDevice* vis_dev,
									  PropertyTransition*& trans,
									  const HTMLayoutProperties* computed_old,
									  const HTMLayoutProperties* computed_new,
									  double runtime_ms,
									  CreateArrayDeclFunction create_func);

private:
	/** Pass the new computed style value to see if it changes the current
		to-value, if it reverses the transition, or if it's a new value that
		should replace this transition with a new one.

		@param to The new computed value.
		@param runtime_ms The current elapsed time. Relative to the same
						  starting point as the runtime_ms passed when creating
						  the transition.
		@param timing delay, duration and timing-function data for the new to-value.
		@return TRANSITION_KEEP if the computed style didn't change,
				TRANSITION_REVERSE if the transition is reversed,
				and TRANSITION_REPLACE if it should be replaced by
				a new transition. */
	StateChange NewToValue(const CSS_decl* to, double runtime_ms, const TransitionTimingData& timing);

	/** The declaration representation for the transition start point. */
	CSS_gen_array_decl* m_from;
	/** The declaration representation for the transition end point. */
	CSS_gen_array_decl* m_to;
	/** The current declaration representation for the transition. */
	CSS_heap_gen_array_decl* m_current;
};

/** The currently active property transitions for a given HTML_Element. */
class ElementTransitions : public ElementRef
{
public:

	/** Constructor. */
	ElementTransitions(HTML_Element* element) : ElementRef(element) {}

	/** Destructor. Delete active property transitions for this element. */
	~ElementTransitions() { m_transitions.Clear(); }

	/** Abort all transitions for this element. */
	void AbortAll() { m_transitions.Clear(); }

	/** Update the current state of the transitions for this element by calling
		Animate on all PropertyTransition objects. Finished transitions will
		be deleted.

		@param doc The document that this element belongs to.
		@param runtime_ms The current GetRuntimeMS().
		@param docprops_elm If these transitions are for an element type which
							sets document root properties
							(LayoutWorkplace::HasDocRootProps()), this parameter
							is set to GetElm() upon return.
		@return ERR_NO_MEMORY on OOM, otherwise OK. */
	OP_STATUS Animate(FramesDocument* doc, double runtime_ms, HTML_Element*& docprops_elm);

	/** Add a property transition for this element.

		@param prop_trans The transition to add. */
	void AddTransition(PropertyTransition* prop_trans) { prop_trans->Into(&m_transitions); }

	/** Delete transitions in progress for properties which are not in the
		provided list. For transition-property: all, this method should not
		be called since it should have no effect.

		@param transitions The list of properties currently included in
					 transition-property plus animated properties.
		@param count The array length of the props parameter. For
					 transition-property: none, this parameter is 0.
		@return A set of PropertyChange bits (cssprops.h) saying what changes
				need to be applied because the current value changed. If the
				transitioned value didn't change for the delta time the transition
				progressed, the method will return CSSPROPS_CHANGED_NONE. */
	int DeleteTransitions(const short* props, unsigned int count);

	/** Set the computed styles for the transitioned properties based
		on the current values of the on-going PropertyTransitions for
		this element.

		@param props The computed style properties for the HTML_Element.
					 The values for transitioned properties will be changed
					 by this method. */
	void GetTransitionValues(HTMLayoutProperties& props) const;

	/** Check if the element has any active transitions.
		@return TRUE if it contains any PropertyTransitions, otherwise FALSE. */
	BOOL IsEmpty() const { return m_transitions.Empty(); }

	/** Check if the element has any transitions with pending changes,
		i.e. is still within its active interval.

		The value may differ from IsEmpty() because of extended
		transitions that keeps its end value indefinitely. */
	BOOL HasPendingChange(double runtime_ms) const;

	/** Return the HTML element. */
	HTML_Element* GetElement() const { return GetElm(); }

	/** If there is an ongoing transition for a given property,
		return the PropertyTransition object for it.

		@param property The property to return the transition object for.
		@return The transition object, or NULL if no transition is in
				progress. */
	PropertyTransition* GetPropertyTransition(CSSProperty property) const;

	/** Stop transitions for an element that is removed (ElementRef method). */
	virtual	void OnRemove(FramesDocument* document);

	/** Stop transitions for an element that is deleted (ElementRef method). */
	virtual void OnDelete(FramesDocument* document);

private:

	/** Dispatch the transitionend event.

		@param doc The document in which this transition happens.
		@param transition The transition that has reached its end state.
		@return OpStatus::ERR_NO_MEMORY on OOM. Otherwise OpStatus::OK. */
	OP_STATUS TransitionEnd(FramesDocument* doc, PropertyTransition* transition);

	/** List of PropertyTransition objects for the ElementRef element. */
	List<PropertyTransition> m_transitions;
};


/** Manage all transitions in progress.
	There is one TransitionManager per document. */

class TransitionManager : public OpTimerListener
{
public:

	/** Constructor. Set document and set up as timer listener. */
	TransitionManager(FramesDocument* doc) : m_doc(doc), m_active_transitions(FALSE) { m_timer.SetTimerListener(this); }

	/** Destructor. Delete all hash entries. */
	~TransitionManager() { m_elm_transitions.DeleteAll(); }

	/** OpTimerListener implementation. */
	virtual void OnTimeOut(OpTimer* timer) { Animate(); }

	/** Called from logdoc when an element is cleaned to remove any references.

		@param element The HTML_Element that is removed. */
	void ElementRemoved(HTML_Element* element);

	/** Abort transitions for the specified element.

		When an element loses its layout box, for instance, transitions must
		stop, so that we don't generate a "transitionend" event long after the
		box is gone.

		@param element The HTML_Element that is removed. */
	void AbortTransitions(HTML_Element* element);

	/** Create or update transitions. This method is called when reloading CSS
		properties.

		@param old_props The computed styles before the property reload.
		@param new_props The computed styles after the reload.
		@param extra_transitions Set of extra transitions to start, in addition to what the transition properties says
		@param runtime_ms The current GetRuntimeMS().
		@param changes A set of PropertyChange bits (cssprops.h) saying what
					   changes need to be applied because a transition is
					   removed or added. This method will add changes to
					   this parameter.
		@return ERR_NO_MEMORY on OOM, otherwise OK. */
	OP_STATUS StartTransitions(LayoutProperties* old_props, LayoutProperties* new_props, Head& extra_transitions, double runtime_ms, int& changes);

	/** Add computed style values from ongoing transitions to the computed styles
		for the given element.

		@param element The element we are getting computed styles for.
		@param props The computed styles for the element which should already
					 contain the computed styles as if there were no
					 transitions. */
	void GetTransitionValues(HTML_Element* element, HTMLayoutProperties& props) const
	{
		ElementTransitions* transitions = GetTransitionsFor(element);
		if (transitions)
			transitions->GetTransitionValues(props);
	}

	/** Check if a property is transitionable or not. */
	static BOOL IsTransitionable(CSSProperty property);

private:

	class TransitionsIterator
	{
	public:
		TransitionsIterator(const HTMLayoutProperties& props, Head& extra_transitions);

		BOOL Next(TransitionDescription& description);

		void GetProperties(const short*& props, int& count) const { props = m_prop_arr; count = m_count; }
		BOOL IsAll() { return m_all; }

	private:

		/** If a property is currently animated (part of the
			extra_transitions list), transitions for that property are ignored.

			This function scans the property list for the next
			property that isn't a member of 'extra_transitions' */

		void SkipTransitionsInExtra();

		unsigned int m_i;
		BOOL m_all;

		const short* m_prop_arr;
		const short* m_shorthand_arr;
		unsigned int m_count;
		unsigned int m_shorthand_count;

		const CSS_generic_value* m_delay_arr;
		const CSS_generic_value* m_duration_arr;
		const CSS_generic_value* m_timing_arr;

		unsigned int m_delay_len;
		unsigned int m_duration_len;
		unsigned int m_timing_len;

		Head& m_extra_transitions;
	};

	/** Update (or create if necessary) a transition for an	HTML_Element.

		@param element The element to update a transition for
		@param elm_trans The current transitions of the element
		@param description The description of the updated transition
		@param computed_old The set of computed style to transition from
		@param computed_new The set of computed style to transition to
		@return OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS UpdateTransition(HTML_Element* element,
							   ElementTransitions*& elm_trans,
							   const TransitionDescription& description,
							   const HTMLayoutProperties* computed_old,
							   const HTMLayoutProperties* computed_new,
							   double runtime_ms,
							   int& changes);

	/** Return the ElementTransitions object for an HTML_Element.

		@param element The HTML_Element to get the transitions for.
		@return An ElementTransitions object for the given HTML_Element. If
				there are no ongoing transitions, the method might return NULL. */
	ElementTransitions* GetTransitionsFor(HTML_Element* element) const;

	/** Return the ElementTransitions object for an HTML_Element. Create if it
		does not already exist.

		@param element The HTML_Element to get the transitions for.
		@return An ElementTransitions object for the given HTML_Element,
				NULL on OOM. */
	ElementTransitions* GetOrCreateTransitionsFor(HTML_Element* element);

	/** Animate all ElementTransitions. */
	void Animate();

	/** The document that object this is a transition manager for. */
	FramesDocument* m_doc;

	/** Hashtable for all ElementTransitions objects for a document. */
	OpPointerHashTable<const HTML_Element, ElementTransitions> m_elm_transitions;

	/** Timer used for animating transitions. */
	OpTimer m_timer;

	/** TRUE if there are transitions in progress for this transition manager. */
	BOOL m_active_transitions;
};

#ifdef _DEBUG
Debug& operator<<(Debug& d, const TransitionDescription& t);
#endif // _DEBUG

#endif // CSS_TRANSITIONS

#endif // TRANSITIONS_H
