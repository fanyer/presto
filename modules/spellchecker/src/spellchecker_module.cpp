/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef INTERNAL_SPELLCHECK_SUPPORT

#include "modules/spellchecker/spellchecker_module.h"

#include "modules/spellchecker/opspellcheckerapi.h"

SpellcheckerModule::~SpellcheckerModule()
{
	OP_ASSERT(!m_manager); // or Destroy wasn't called
}
	
void SpellcheckerModule::InitL(const OperaInitInfo& info)
{
	m_manager = OP_NEW_L(OpSpellCheckerManager, ());
	LEAVE_IF_ERROR(m_manager->Init());
	m_ui_data = m_manager->GetSpellUIData();
}
void SpellcheckerModule::Destroy()
{
	OP_DELETE(m_manager);
	m_manager = NULL;
	m_ui_data = NULL;
}

BOOL SpellcheckerModule::FreeCachedData(BOOL toplevel_context)
{
	return m_manager && m_manager->FreeCachedData(toplevel_context);
}

#endif // INTERNAL_SPELLCHECK_SUPPORT

