/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef GADGET_SUPPORT

#include "modules/gadgets/gadgets_module.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/gadgets/OpGadgetParsers.h"

GadgetsModule::GadgetsModule()
	: m_gadget_manager(NULL)
	, m_gadget_parsers(NULL)
{
}

void
GadgetsModule::InitL(const OperaInitInfo& info)
{
	LEAVE_IF_ERROR(OpGadgetParsers::Make(m_gadget_parsers));
	LEAVE_IF_ERROR(OpGadgetManager::Make(m_gadget_manager));
#ifndef HAS_COMPLEX_GLOBALS
	extern void init_g_gadget_start_file();
	CONST_ARRAY_INIT(g_gadget_start_file);
	extern void init_g_gadget_start_file_type();
	CONST_ARRAY_INIT(g_gadget_start_file_type);
	extern void init_g_gadget_default_icon();
	CONST_ARRAY_INIT(g_gadget_default_icon);
	/*extern void init_g_gadget_default_icon_type();*/
	/*CONST_ARRAY_INIT(g_gadget_default_icon_type);*/
#endif
}

void
GadgetsModule::Destroy()
{
	OP_DELETE(m_gadget_manager);
	m_gadget_manager = NULL;
	OP_DELETE(m_gadget_parsers);
	m_gadget_parsers = NULL;
}

#endif // GADGET_SUPPORT
