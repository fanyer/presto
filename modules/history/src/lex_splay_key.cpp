/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 * psmaas - Patricia Aas
 */

#include "core/pch.h"

#ifdef HISTORY_SUPPORT

#include "modules/history/src/lex_splay_key.h"

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void LexSplayKey::Decrement()
{ 
    OP_ASSERT(m_ref_counter); 
    m_ref_counter--; 
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
void LexSplayKey::Increment() 
{ 
    m_ref_counter++; 
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
BOOL LexSplayKey::InUse() 
{ 
    return m_ref_counter > 0; 
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
LexSplayKey::LexSplayKey() 
{
    m_ref_counter = 0;
}

#endif // HISTORY_SUPPORT
