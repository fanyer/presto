/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/style/src/css_pseudo_stack.h"
#include "modules/logdoc/htm_elm.h"

void CSS_PseudoStack::Commit()
{
	while (m_next_elm > 0)
	{
		m_next_elm--;
		m_stack[m_next_elm].m_html_elm->SetCheckForPseudo(m_stack[m_next_elm].m_pseudo);
		m_has_pseudo |= m_stack[m_next_elm].m_pseudo;
	}
}
