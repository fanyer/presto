/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_CONDITION_H
#define CSS_CONDITION_H

/** Conditions as they are used in @supports.

	This code assumes that we "support a declaration" or "implement a given value of a given property"
	iff we successfully parse it.

	@see CSS_SupportsRule to which this class belongs. */
class CSS_Condition
{
public:
	CSS_Condition() : next(NULL), negated(FALSE) {}
	virtual ~CSS_Condition();
	void Negate();
	/** Evaluates this condition its component conditions. This is relatively cheap since
		the heavy lifting to determine support for each property is done during parsing.
		All we need to do now is check the \c supported and \c negated fields together with the
		logic operators (although the latter is admittedly O(n)). */
	virtual BOOL Evaluate() = 0;
	virtual OP_STATUS GetCssText(TempBuffer* buf) = 0;
	CSS_Condition *next;
protected:
	OP_STATUS GetCssTextNegatedStart(TempBuffer* buf);
	OP_STATUS GetCssTextNegatedEnd(TempBuffer* buf);
	BOOL negated;
};

class CSS_SimpleCondition : public CSS_Condition
{
public:
	CSS_SimpleCondition() : m_decl(NULL), supported(FALSE) {}

	/** Parses (using CSS_Parser) a string containing one declaration, setting \c supported if
		parsing was successful. Takes ownership of \c decl (for later serialization). */
	OP_STATUS SetDecl(uni_char *decl);
	OP_STATUS SetDecl(const uni_char *property, unsigned property_length, const uni_char *value, unsigned value_length);
	virtual ~CSS_SimpleCondition();
	virtual BOOL Evaluate();
	virtual OP_STATUS GetCssText(TempBuffer* buf);
private:
	/// The string that was parsed. Used for serialization.
	uni_char *m_decl;
	BOOL supported;
};

class CSS_CombinedCondition : public CSS_Condition
{
public:
	CSS_CombinedCondition() : CSS_Condition(), members(NULL) {}
	virtual ~CSS_CombinedCondition();
	/// Add a condition to the beginning of the list
	void AddFirst(CSS_Condition *c);
	/// Add a condition to the end of the list
	void AddLast(CSS_Condition *c);
	virtual OP_STATUS AppendCombiningWord(TempBuffer* buf) = 0;
	virtual OP_STATUS GetCssText(TempBuffer* buf);
protected:
	CSS_Condition *members;
};

class CSS_ConditionConjunction : public CSS_CombinedCondition
{
public:
	virtual BOOL Evaluate();
	virtual OP_STATUS AppendCombiningWord(TempBuffer* buf);
};
class CSS_ConditionDisjunction : public CSS_CombinedCondition
{
public:
	virtual BOOL Evaluate();
	virtual OP_STATUS AppendCombiningWord(TempBuffer* buf);
};

#endif // CSS_CONDITION_H
