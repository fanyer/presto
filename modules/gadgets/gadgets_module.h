/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/**
 * @file gadgets_module.h
 * Global declarations for the gadgets module.
 *
 * @author Lasse Magnussen <lasse@opera.com>
 */

#ifndef GADGETS_MODULE_H
#define GADGETS_MODULE_H

#ifdef GADGET_SUPPORT

#include "modules/hardcore/opera/module.h"

#define NUMBER_OF_DEFAULT_START_FILES	5
#define NUMBER_OF_DEFAULT_ICONS			5

class OpGadgetManager;
class OpGadgetParsers;
struct widget_namespace;

class GadgetsModule : public OperaModule
{
public:
	GadgetsModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();

	OpGadgetManager* m_gadget_manager;
	OpGadgetParsers* m_gadget_parsers;
#ifndef HAS_COMPLEX_GLOBALS
	const char *m_gadget_start_file[NUMBER_OF_DEFAULT_START_FILES];
	const char *m_gadget_start_file_type[NUMBER_OF_DEFAULT_START_FILES];
	const char *m_gadget_default_icon[NUMBER_OF_DEFAULT_ICONS];
	/*static const char *m_gadget_default_icon_type[NUMBER_OF_DEFAULT_ICONS];*/
#endif
};

/**  Singleton global instance of OpGadgetParsers */
#define g_gadget_parsers (g_opera->gadgets_module.m_gadget_parsers)

#ifndef HAS_COMPLEX_GLOBALS
/* Internal arrays */
# define g_gadget_start_file (g_opera->gadgets_module.m_gadget_start_file)
# define g_gadget_start_file_type (g_opera->gadgets_module.m_gadget_start_file_type)
# define g_gadget_default_icon (g_opera->gadgets_module.m_gadget_default_icon)
# define g_gadget_default_icon_type (g_opera->gadgets_module.m_gadget_default_icon_type)
#endif

#define GADGETS_MODULE_REQUIRED

#endif // GADGET_SUPPORT
#endif // !GADGETS_MODULE_H
