/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#ifdef SVG_SUPPORT
#ifdef SELFTEST

#include "modules/svg/src/SVGTimedElementContext.h"
#include "modules/svg/src/animation/svganimationworkplace.h"
#include "modules/svg/src/animation/svganimationworkplace.h"

#ifdef SVG_SUPPORT_ANIMATION_LISTENER

#include "modules/svg/src/animation/svganimationselftest.h"
#include "modules/svg/src/animation/svgtimingarguments.h"

#define SVG_SELFTEST_NUMBER_EPSILON 0.001
#define SVG_SELFTEST_INTERVAL_EPSILON 0.00001

SVGAnimationSelftest::SVGAnimationSelftest() :
    op_status(OpStatus::OK)
{
}

SVGAnimationSelftest::~SVGAnimationSelftest()
{
    ClearRules();
}

void
SVGAnimationSelftest::ClearRules()
{
    RuleItem *rule_item = static_cast<RuleItem *>(rules.First());
    while(rule_item != NULL)
    {
		RuleItem *delete_item = rule_item;
		rule_item = static_cast<RuleItem *>(rule_item->Suc());
		delete_item->Out();
		OP_DELETE(delete_item);
    }
}

void
SVGAnimationSelftest::InsertRule(const Rule &rule)
{
    RuleItem *rule_item = OP_NEW(RuleItem, ());
    if (rule_item)
    {
		rule_item->success_count = 0;
		rule_item->failure_count = 0;
		rule_item->rule = rule;
		rule_item->Into(&rules);
    }
    else
    {
		op_status = OpStatus::ERR_NO_MEMORY;
    }
}

void
SVGAnimationSelftest::Notification(const SVGAnimationWorkplace::AnimationListener::NotificationData &data)
{
    RuleItem *rule_item = static_cast<RuleItem *>(rules.First());
    while(rule_item != NULL)
    {
		if (MatchRuleItem(*rule_item, data))
			return;

		rule_item = static_cast<RuleItem *>(rule_item->Suc());
    }
}

BOOL
SVGAnimationSelftest::MatchRuleItem(RuleItem &rule_item,
									const SVGAnimationWorkplace::AnimationListener::NotificationData &data)
{
    const Rule &rule = rule_item.rule;
    BOOL apply_rule = RuleItemApplies(rule_item, data);

    if (apply_rule)
    {
		BOOL test_success = TRUE;

		if (rule.rule_type == Rule::RULE_INTERVAL &&
			data.notification_type == SVGAnimationWorkplace::AnimationListener::NotificationData::INTERVAL_CREATED)
		{
			const SVGAnimationInterval *created_interval = data.extra.interval_created.animation_interval;

			if (created_interval->Begin() != rule.extra.interval.begin)
				test_success = FALSE;
			else if (created_interval->End() != rule.extra.interval.end)
				test_success = FALSE;
			else if (created_interval->SimpleDuration() != rule.extra.interval.simple_duration)
				test_success = FALSE;
		}
		else if (rule.rule_type == Rule::RULE_VALUE &&
				 data.notification_type == SVGAnimationWorkplace::AnimationListener::NotificationData::ANIMATION_VALUE)
		{
			const SVGAnimationValue &supplied_value = rule.extra.value.animation_value;
			const SVGAnimationValue &computed_value = *data.extra.animation_value.animation_value;

			if (computed_value.value_type != supplied_value.value_type)
				test_success = FALSE;
			else if (computed_value.value_type == SVGAnimationValue::VALUE_NUMBER)
			{
				if (op_fabs(computed_value.value.number - supplied_value.value.number) > SVG_SELFTEST_NUMBER_EPSILON)
					test_success = FALSE;
			}
		}

		if (test_success)
		{
			rule_item.success_count++;
		}
		else
		{
			rule_item.failure_count++;
		}
		return TRUE;
    }
    else
    {
		return FALSE;
    }
}

/* static */ void
SVGAnimationSelftest::SetupIntervalRule(Rule &rule,
										const char *animation_element_id,
										SVG_ANIMATION_TIME begin,
										SVG_ANIMATION_TIME end,
										SVG_ANIMATION_TIME simple_duration)
{
    rule.rule_type = Rule::RULE_INTERVAL;
    rule.rule_condition = Rule::PASS_ON_ONE;
    rule.animation_element_id = animation_element_id;
    rule.extra.interval.begin = begin;
    rule.extra.interval.end = end;
    rule.extra.interval.simple_duration = simple_duration;
}

/* static */ void
SVGAnimationSelftest::SetupValueRule(Rule &rule,
									 const char *animation_element_id,
									 SVG_ANIMATION_INTERVAL_POSITION interval_position,
									 const SVGAnimationValue &animation_value)
{
	rule.rule_type = Rule::RULE_VALUE;
	rule.rule_condition = Rule::PASS_ON_ZERO_OR_MORE;
	rule.animation_element_id = animation_element_id;
	rule.extra.value.animation_value = animation_value;
	rule.extra.value.interval_position = interval_position;
}

SVGAnimationSelftest::TestStatus
SVGAnimationSelftest::GetTestStatus()
{
    if (OpStatus::IsMemoryError(op_status))
    {
		return TEST_STATUS_NO_MEMORY;
    }

    RuleItem *rule_item = static_cast<RuleItem *>(rules.First());
    while(rule_item != NULL)
    {
		TestStatus rule_status = CheckRuleItem(*rule_item);
		if (rule_status == TEST_STATUS_NOT_DONE || rule_status == TEST_STATUS_FAILED)
		{
			return rule_status;
		}
		else
		{
			rule_item = static_cast<RuleItem *>(rule_item->Suc());
		}
    }

    return TEST_STATUS_SUCCESS;
}

SVGAnimationSelftest::TestStatus
SVGAnimationSelftest::CheckRuleItem(RuleItem &rule_item)
{
    const Rule &rule = rule_item.rule;

    if (rule.rule_condition == Rule::PASS_ON_ONE &&
		rule_item.success_count == 1 && rule_item.failure_count == 0)
    {
		return TEST_STATUS_SUCCESS;
    }
    else if (rule.rule_condition == Rule::PASS_ON_ONE_OR_MORE &&
			 rule_item.success_count >= 1 && rule_item.failure_count == 0)
    {
		return TEST_STATUS_SUCCESS;
    }
    else if (rule.rule_condition == Rule::PASS_ON_ZERO_OR_MORE &&
			 rule_item.success_count >= 0 && rule_item.failure_count == 0)
    {
		return TEST_STATUS_SUCCESS;
    }
    else if (rule.rule_condition == Rule::PASS_ON_ONE &&
			 rule_item.success_count == 0 && rule_item.failure_count == 0)
    {
		return TEST_STATUS_NOT_DONE;
    }
    else if (rule.rule_condition == Rule::PASS_ON_ONE_OR_MORE &&
			 rule_item.success_count == 0 && rule_item.failure_count == 0)
    {
		return TEST_STATUS_NOT_DONE;
    }
    else
		return TEST_STATUS_FAILED;
}

BOOL
SVGAnimationSelftest::RuleItemApplies(RuleItem &rule_item,
									  const SVGAnimationWorkplace::AnimationListener::NotificationData &data)
{
    const Rule &rule = rule_item.rule;

    if (rule_item.failure_count > 0)
		return FALSE;

    if (rule.rule_condition == Rule::PASS_ON_ONE && rule_item.success_count == 1)
		return FALSE;

    if (rule.animation_element_id != NULL)
    {
		const uni_char *timed_element_id = data.timing_if->GetElement()->GetId();
		if (timed_element_id != NULL)
		{
			if (!uni_str_eq(rule.animation_element_id, timed_element_id))
				return FALSE;
		}
		else
		{
			return FALSE;
		}
    }


	if (rule.rule_type == Rule::RULE_VALUE &&
		data.notification_type == SVGAnimationWorkplace::AnimationListener::NotificationData::ANIMATION_VALUE)
	{
		if (op_fabs(rule.extra.value.interval_position -
					data.extra.animation_value.interval_position) > SVG_SELFTEST_INTERVAL_EPSILON)
			return FALSE;
	}

	return TRUE;
}

#endif // SVG_SUPPORT_ANIMATION_LISTENER
#endif // SELFTEST
#endif // SVG_SUPPORT
