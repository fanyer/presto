/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_RULESET_H
#define CSS_RULESET_H

#include "modules/style/css_property_list.h"
#include "modules/style/css_media.h"
#include "modules/style/src/css_rule.h"
#include "modules/style/src/css_selector.h"

class CSS_ConditionalRule;

class CSS_BlockRule : public CSS_Rule
{
public:
	/** Constructor. */

	CSS_BlockRule() : m_conditional(NULL), m_rule_number(0) {}

	/** If this style rule is inside an conditional rule, the CSS_ConditionalRule object for
		that rule is returned. Otherwise NULL. */
	CSS_ConditionalRule* GetConditionalRule() { return m_conditional; }

	/** Set the CSS_ConditionalRule object of the @media rule in which this style rule
		reside. */
	void SetConditionalRule(CSS_ConditionalRule* rule) { m_conditional = rule; }

	unsigned int GetRuleNumber() { return m_rule_number; }
	void SetRuleNumber(unsigned int num) { m_rule_number = num; }

protected:

	/** A pointer to the @media in which this rule resides. */
	CSS_ConditionalRule* m_conditional;

	/** Rule number for style rules. */
	unsigned int m_rule_number;
};

class CSS_DeclarationBlockRule : public CSS_BlockRule
{
public:
	/** Constructor.

		@param pl The object will store this pointer and increase
				  its reference count. */
	CSS_DeclarationBlockRule(CSS_property_list* pl = NULL);

	/** Destructor. Unrefs the property list and deletes the selectors. */
	virtual ~CSS_DeclarationBlockRule();

	/** Set the property list containing all the declarations of the rule. */
	virtual void SetPropertyList(HLDocProfile* hld_profile, CSS_property_list* pl, BOOL delete_duplicates);

	/** Return a property list containing all the declarations of the rule. */
	CSS_property_list* GetPropertyList() { return m_prop_list; }

	/** Add the textual representation of this rule to the TempBuffer. */
	virtual OP_STATUS GetCssText(CSS* stylesheet, TempBuffer* buf, unsigned int indent_level = 0);

protected:

	/** Dereference current property list, set the new one and reference it. */
	void SetPropertyListInternal(CSS_property_list* pl)
	{
		if (pl)
			pl->Ref();

		if (m_prop_list)
			m_prop_list->Unref();

		m_prop_list = pl;
	}

	/** The declarations. */
	CSS_property_list* m_prop_list;
};

class CSS_StyleRule : public CSS_DeclarationBlockRule
{
public:

	/** Constructor. Sets members to initial values. */
	CSS_StyleRule();

	/** Destructor. Unrefs the property list and deletes the selectors. */
	virtual ~CSS_StyleRule();

	/** Set the property list containing all the declarations of the rule. */
	virtual void SetPropertyList(HLDocProfile* hld_profile, CSS_property_list* pl, BOOL delete_duplicates);

	/** Add a selector to the style rule. */
	void AddSelector(CSS_Selector* sel);

	/** Try to match the selectors of this rule. Returns TRUE if at least one
		of them matches. */
	inline BOOL Match(CSS_MatchContext* params,
					  CSS_MatchContextElm* context_elm,
					  unsigned short& specificity,
					  unsigned short& selection_specificity) const;

	/** Rule type. */
	virtual Type GetType() { return STYLE; }

	/** Return the single selector for this page rule which contains pseudo page selector attributes. */
	CSS_Selector* FirstSelector() { return static_cast<CSS_Selector*>(m_selectors.First()); }

	/** Return TRUE if any of the selectors has a given target element type. */
	BOOL HasTargetElmType(Markup::Type type);

	/** Delete all selector objects. Done by the parser before adding new selectors when setting selectorText. */
	void DeleteSelectors() { m_selectors.Clear(); }

	/** Add the textual representation of this rule to the TempBuffer. */
	virtual OP_STATUS GetCssText(CSS* stylesheet, TempBuffer* buf, unsigned int indent_level = 0);

	/** Parse a text buffer to replace the contents of this StyleRule. If the text buffer does not
		parse as a StyleRule, an error will be returned, and this rule will remain untouched. */
	virtual CSS_PARSE_STATUS SetCssText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len);

	/** Add the textual representation of this rule's selector to buf. */
	virtual OP_STATUS GetSelectorText(CSS* stylesheet, TempBuffer* buf);

	/** Replace the selector of this StyleRule with the selector in "text". If "text" is not a parsable
		selector, the current selector will remain untouched, and a parse error will be returned. */
	virtual CSS_PARSE_STATUS SetSelectorText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len);

#ifdef SCOPE_CSS_RULE_ORIGIN
	/** Set the original line number for this rule. */
	void SetLineNumber(unsigned line_number){ m_line_number = line_number; }

	/** Get the original line number for this rule. */
	unsigned GetLineNumber() const { return m_line_number; }
#endif // SCOPE_CSS_RULE_ORIGIN

protected:
	/** List of CSS_Selector objects. */
	Head m_selectors;

private:
#ifdef SCOPE_CSS_RULE_ORIGIN
	/** The original line number for this style rule. The line number
	    points to the start of the declaration block. */
	unsigned m_line_number;
#endif // SCOPE_CSS_RULE_ORIGIN
};

class CSS_PageRule : public CSS_StyleRule
{
public:
	/** Constructor. */
	CSS_PageRule() { m_name = NULL; }

	/** Destructor. */
	virtual ~CSS_PageRule() { OP_DELETEA(m_name); }

	/** Return page name if specified, otherwise NULL. */
	uni_char* GetName() { return m_name; }

	/** Set the page name.
		@param val The page name. Must be allocated on heap using new.
				   CSS_PageRule is responsible for deleting it. */
	void SetName(uni_char* val) { OP_DELETEA(m_name); m_name = val; }

	/** Rule type. */
	virtual Type GetType() { return PAGE; }

	/** Return specificity. */
	unsigned short GetSpecificity() { return PageSelector() ? PageSelector()->GetSpecificity() : 0; }

	/** Return a bitset of pseudo pages for this rule. */
	unsigned short GetPagePseudo() { return PageSelector() ? PageSelector()->GetPseudoPage() : 0; }

	virtual CSS_PARSE_STATUS SetCssText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len);
	virtual OP_STATUS GetSelectorText(CSS* stylesheet, TempBuffer* buf);
	virtual CSS_PARSE_STATUS SetSelectorText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len);

private:
	/** Return the single selector for this page rule which contains pseudo page selector attributes. */
	CSS_Selector* PageSelector() { return FirstSelector(); }

	/** The name part of the @page rule. NULL if no name is specified.
		The pseudo pages are stored in the selector. */
	uni_char* m_name;
};


inline BOOL CSS_StyleRule::Match(CSS_MatchContext* context, CSS_MatchContextElm* context_elm,
								 unsigned short& specificity, unsigned short& selection_specificity) const
{
	BOOL match = FALSE;
	specificity = 0;
	selection_specificity = 0;

	CSS_Selector* css_sel = static_cast<CSS_Selector*>(m_selectors.First());
	while (css_sel)
	{
		if ((!css_sel->HasClassInTarget() || context_elm->GetClassAttribute()) &&
			(!css_sel->HasIdInTarget() || context_elm->GetIdAttribute()))
		{
			CSS_Selector::MatchResult sel_match = css_sel->Match(context, context_elm);
			if (sel_match == CSS_Selector::MATCH)
				match = TRUE;
			if (sel_match != CSS_Selector::NO_MATCH)
			{
				short new_specificity = css_sel->GetSpecificity();
				if (sel_match == CSS_Selector::MATCH)
				{
					if (new_specificity > specificity)
						specificity = new_specificity;
				}
				else if (new_specificity > selection_specificity)
					selection_specificity = new_specificity;
			}
		}

		if (match && !context->MatchAll())
			break;

		css_sel = static_cast<CSS_Selector*>(css_sel->Suc());
	}
	return match;
}

#endif // CSS_RULESET_H
