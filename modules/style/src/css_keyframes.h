/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_KEYFRAMES_H
#define CSS_KEYFRAMES_H

#ifdef CSS_ANIMATIONS

#include "modules/util/simset.h"
#include "modules/style/src/css_rule.h"
#include "modules/style/src/css_ruleset.h"

class CSS_property_list;

/** Contains a keyframe selector value in the [0,1] range. */

class CSS_KeyframeSelector : public ListElement<CSS_KeyframeSelector>
{
public:
	CSS_KeyframeSelector(double position) : m_position(position) {}
	double GetPosition() const { return m_position; }

private:
	double m_position;
};

class CSS_KeyframesRule;

/** CSS_KeyframeRule holds one keyframe consisting of a list of
	keyframe selectors and a property list. Hence this is a collection
	of keyframes that happens to share the same property list. */

class CSS_KeyframeRule : public CSS_Rule
{
public:
	/** Construct a keyframe rule from a list of keyframe selectors
		and a property list.

		The property list be NULL when using a keyframe rule as a
		comparison value */

	CSS_KeyframeRule(CSS_KeyframesRule* keyframes, List<CSS_KeyframeSelector>& positions, CSS_property_list* prop_list);
	~CSS_KeyframeRule();

	/** Return rule type */

	virtual Type GetType() { return KEYFRAME; }

	/** Get rule as css text. */

	virtual OP_STATUS GetCssText(CSS* stylesheet, TempBuffer* buf, unsigned int indent_level = 0);

	/* Parse text as a CSS keyframe and replace the value of this
	   keyframe with the result. */

	virtual CSS_PARSE_STATUS SetCssText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len);

	/** Get keyframe selector text */

	OP_STATUS GetKeyText(TempBuffer* buf) const;

	/** Set keyframe selector text */

	OP_STATUS SetKeyText(const uni_char* key_text, unsigned key_text_length);

	/** Get the list of properties to apply at the respective keyframe
		selectors. */

	CSS_property_list* GetPropertyList() { return m_prop_list; }

	/** Get first keyframe selector. Usually used to start a traversal
		of the list of keyframe selectors. */

	CSS_KeyframeSelector* GetFirstSelector() const { return m_positions.First(); }

	/** Get parent keyframes rule */

	CSS_KeyframesRule* GetKeyframesRule() { return m_keyframes; }

	/** Get next keyframe in list of keyframes in this keyframes
		rule. */

	CSS_KeyframeRule* Suc() const { return static_cast<CSS_KeyframeRule*>(Link::Suc()); }

	/** Throw away current keyframe selectors and reference to
		property list and replace them with a new list and a new list
		of properties. */

	void Replace(List<CSS_KeyframeSelector>& positions, CSS_property_list* props);

	/** Compare keys */

	BOOL operator==(const CSS_KeyframeRule& other);

private:
	CSS_KeyframesRule* m_keyframes;

	List<CSS_KeyframeSelector> m_positions;
	CSS_property_list* m_prop_list;
};

/** CSS_KeyframeRule holds a list of keyframes. */

class CSS_KeyframesRule : public CSS_BlockRule
{
public:
	CSS_KeyframesRule(uni_char* name) : m_name(name) {}
	~CSS_KeyframesRule() { OP_DELETEA(m_name); ClearRules(); }

	virtual Type GetType() { return KEYFRAMES; }

	virtual OP_STATUS GetCssText(CSS* stylesheet, TempBuffer* buf, unsigned int indent_level = 0);
	virtual CSS_PARSE_STATUS SetCssText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len);

	/** Add rule */
	void Add(CSS_KeyframeRule* rule) { rule->Into(&m_rules); }

	/** Clear rules/keyframes */
	void ClearRules() { m_rules.Clear(); }

	/** Get the name of the keyframes declaration */
	const uni_char* GetName() const { return m_name; }

	/** Set name */
	OP_STATUS SetName(const uni_char* new_name) { return UniSetStr(m_name, new_name); }

	/** Get number of rules (in this case keyframes) */
	unsigned int GetRuleCount() { return m_rules.Cardinal(); }

	/** Get rule by index */
	CSS_Rule* GetRule(unsigned int idx) { CSS_Rule* l = m_rules.First(); while (l && idx-- > 0) l = l->Suc(); return l; }

	/* Get first keyframe rule */
	CSS_KeyframeRule* First() const { return static_cast<CSS_KeyframeRule*>(m_rules.First()); }

	/** Parse and append rule given that parsing succeeded, otherwise returns parsing error. */
	CSS_PARSE_STATUS AppendRule(HLDocProfile* hld_prof, CSS* sheet, const uni_char* rule, unsigned rule_length);

	/** Delete rule */
	OP_STATUS DeleteRule(const uni_char* key_text, unsigned key_text_length);

	/** Find rule */
	OP_STATUS FindRule(CSS_KeyframeRule*& rule, const uni_char* key_text, unsigned key_text_length);

private:
	List<CSS_Rule> m_rules;

	uni_char* m_name;
};

#endif // CSS_ANIMATIONS
#endif // !CSS_KEYFRAMES_H
