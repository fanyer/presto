/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef SPELL_CHECK_CONTEXT_H
#define	SPELL_CHECK_CONTEXT_H

#include "modules/inputmanager/inputcontext.h"

/**
 * The Spell Check feature-specific UI context.
 *
 * Currently hooked up into Opera's input action system in a somewhat clumsy
 * and definitely not scalable way.  This should improve once DSK-271123 is
 * implemented.
 */
class SpellCheckContext : public OpInputContext
{
public:
	SpellCheckContext();
	virtual ~SpellCheckContext();

	virtual const char*	GetInputContextName()
		{ return "SpellCheck Application"; }

	virtual BOOL OnInputAction(OpInputAction* action);

#ifdef INTERNAL_SPELLCHECK_SUPPORT
public:
	int GetSpellSessionID() { return m_spell_session_id; }
	void SetSpellSessionID(int id);
private:
	int	m_spell_session_id;
#endif // INTERNAL_SPELLCHECK_SUPPORT

};

#endif // SPELL_CHECK_CONTEXT_H
