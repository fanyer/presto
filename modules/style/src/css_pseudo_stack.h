/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_PSEUDO_STACK_H
#define CSS_PSEUDO_STACK_H

class HTML_Element;

#define CSS_PSEUDO_STACK_SIZE 50

/** Stack for storing uncommitted pseudo bits during selector matching. */
class CSS_PseudoStack
{
public:

	class StackElm
	{
	public:
		HTML_Element* m_html_elm;
		int m_pseudo;
	};

	CSS_PseudoStack() : m_stack(0), m_next_elm(0), m_has_pseudo(0) {}
	~CSS_PseudoStack() { OP_DELETEA(m_stack); }
	void ConstructL() { m_stack = OP_NEWA_L(StackElm, CSS_PSEUDO_STACK_SIZE); }

	BOOL Push(HTML_Element* elm, int pseudo)
	{
		if (m_next_elm < CSS_PSEUDO_STACK_SIZE)
		{
			m_stack[m_next_elm].m_html_elm = elm;
			m_stack[m_next_elm++].m_pseudo = pseudo;
			return TRUE;
		}
		else
			return FALSE;
	}

	int GetPos() { return m_next_elm; }
	void Backtrack(int pos) { m_next_elm = pos; }
	void Commit();
	int HasPseudo() { return m_has_pseudo; }
	void AddHasPseudo(int has_pseudo) { m_has_pseudo |= has_pseudo; }
	void ClearHasPseudo() { m_has_pseudo = 0; }

private:
	StackElm* m_stack;
	int m_next_elm;
	int m_has_pseudo;
};

#endif // CSS_PSEUDO_STACK_H
