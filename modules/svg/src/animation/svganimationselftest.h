/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_ANIMATION_SELFTEST_H
#define SVG_ANIMATION_SELFTEST_H

#ifdef SVG_SUPPORT
#ifdef SELFTEST
#ifdef SVG_SUPPORT_ANIMATION_LISTENER

#include "modules/svg/src/animation/svganimationworkplace.h"
#include "modules/svg/src/animation/svganimationvalue.h"
#include "modules/hardcore/timer/optimer.h"

class SVGAnimationSelftest
{
public:
    SVGAnimationSelftest();
    ~SVGAnimationSelftest();

    struct Rule
    {
		enum RuleType {
			RULE_INTERVAL,
			RULE_VALUE
		};

		RuleType rule_type;

		enum RuleCondition {
			PASS_ON_ZERO_OR_MORE,
			/**< The test applies for zero or more successes and zero
			 * failures. It passes for zero of more successes and zero
			 * failures */

			PASS_ON_ONE_OR_MORE,
			/**< The test applies for zero or more successes and zero
			 * failures. It passes for one of more successes and zero
			 * failures */

			PASS_ON_ONE
			/**< The test applies for zero success and zero
			 * failures. It passes for one success and zero
			 * failures */
		};

		RuleCondition rule_condition;

		const char *animation_element_id;

		struct IntervalData {
			SVG_ANIMATION_TIME begin;
			SVG_ANIMATION_TIME end;
			SVG_ANIMATION_TIME simple_duration;
		};

		struct ValueData {
			SVG_ANIMATION_INTERVAL_POSITION interval_position;
			SVGAnimationValue animation_value;
		};

		union {
			IntervalData interval;
			ValueData value;
		} extra;
    };

    void InsertRule(const Rule &rule);
    void ClearRules();

    enum TestStatus {
		TEST_STATUS_NO_MEMORY,
		TEST_STATUS_NOT_DONE,
		TEST_STATUS_SUCCESS,
		TEST_STATUS_FAILED
    };

    TestStatus GetTestStatus();

    void Notification(const SVGAnimationWorkplace::AnimationListener::NotificationData &data);

    static void SetupIntervalRule(Rule &rule,
								  const char *animation_element_id,
								  SVG_ANIMATION_TIME begin,
								  SVG_ANIMATION_TIME end,
								  SVG_ANIMATION_TIME simple_duration);

    static void SetupValueRule(Rule &rule,
							   const char *animation_element_id,
							   SVG_ANIMATION_INTERVAL_POSITION interval_position,
							   const SVGAnimationValue &animation_value);


private:
    struct RuleItem : public Link
    {
		Rule rule;

		int success_count;
		int failure_count;
    };

    BOOL MatchRuleItem(RuleItem &rule_item,
					   const SVGAnimationWorkplace::AnimationListener::NotificationData &data);

    BOOL RuleItemApplies(RuleItem &rule,
						 const SVGAnimationWorkplace::AnimationListener::NotificationData &data);
    SVGAnimationSelftest::TestStatus CheckRuleItem(RuleItem &rule_item);

    Head rules;

    OP_STATUS op_status;
};

/**
 * This class can as a helper for listening to animations. See
 * svg.animation.basic.ot for an example of its use. Note that it will
 * only accept the first loaded document, and ignore later ones.
 */
class SVGSelftestAnimationListener :
	public SVGAnimationWorkplace::AnimationListener,
	public OpTimerListener
{
public:
	SVGSelftestAnimationListener() : duration(200),
									 total_time(0),
									 time_out_time(10000),
									 animation_selftest(NULL),
									 animation_workplace(NULL) {}
	virtual ~SVGSelftestAnimationListener() {
		Out();
	}

	virtual void Notification(const NotificationData &notification_data) {
		OP_ASSERT(animation_selftest);
		animation_selftest->Notification(notification_data);
	}

	virtual void Passed() = 0;
	virtual void Failed(const char *message) = 0;

	void OnTimeOut(OpTimer *timer) {
		OP_ASSERT(animation_selftest);
		SVGAnimationSelftest::TestStatus test_status = animation_selftest->GetTestStatus();
		if (test_status == SVGAnimationSelftest::TEST_STATUS_SUCCESS)
			Passed();
		else if (test_status == SVGAnimationSelftest::TEST_STATUS_FAILED)
			Failed("Failed: Test failed\n");
		else if (total_time < time_out_time)
		{
			total_time += duration;
			timer->Start(duration);
		}
		else
		{
			Failed("Failed: Timed out.\n");
		}
	}

	virtual BOOL Accept(SVGAnimationWorkplace *potential_animation_workplace) {
		if (animation_workplace == NULL)
		{
			animation_workplace = potential_animation_workplace;
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}

	OpTimer *StartListening() {
		OpTimer *timer = OP_NEW(OpTimer, ());
		if (timer == NULL)
		{
			Failed("Failed: No memory\n");
		}
		else
		{
			timer->SetTimerListener(this);
			total_time += duration;
			timer->Start(duration);
		}
		return timer;
	}

	unsigned duration;
	unsigned total_time;
	unsigned time_out_time;

	SVGAnimationSelftest *animation_selftest;
	SVGAnimationWorkplace *animation_workplace;
};

#endif // SELFTEST
#endif // SVG_SUPPORT_ANIMATION_LISTENER
#endif // SVG_SUPPORT
#endif // SVG_ANIMATION_SELFTEST_H
