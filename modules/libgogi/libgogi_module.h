/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef LIBGOGI_MODULE_H
#define LIBGOGI_MODULE_H

#include "modules/hardcore/opera/module.h"
#include "modules/util/simset.h"

class LibgogiModule : public OperaModule
{
public:
	LibgogiModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();

	class MDE_FontEngine* mde_font_engine;
#ifdef MDE_USE_ALPHA_LOOKUPTABLE
	unsigned char *lutbl_alpha;
#endif


	/**
	 * MDE_GenericScreen's constructor will add itself to this list
	 * and MDE_GenericScreen's destructor will remove itself from this
	 * list. If the deleted MDE_GenericScreen is equal to the
	 * attribute screen, then that attribute is replaced by the next
	 * screen on this list.
	 */
	Head m_list_of_screens;

	class MDE_IMEManager* ime_manager;

#ifdef MDE_MMAP_MANAGER
	class MDE_MMapManager* m_mmap_man;
#endif // MDE_MMAP_MANAGER

#ifdef USE_PREMULTIPLIED_ALPHA
	UINT32 m_generic_color_lookup[257];
	UINT32 m_black_lookup[256];
	UINT32* m_color_lookup;
#endif // USE_PREMULTIPLIED_ALPHA

#ifdef MDE_NATIVE_WINDOW_SUPPORT
	Head nativeWindows;
#endif // MDE_NATIVE_WINDOW_SUPPORT

	/**
	   pointer to last clicked view.

	   NOTE: this pointer should never be accessed! it is compared to
	   newly captured input only, to suppress triggering double click
	   when the clicks fall in different views.
	 */
	class MDE_View* m_last_captured_input;
};

#define g_mde_ime_manager g_opera->libgogi_module.ime_manager
#define g_mde_font_engine g_opera->libgogi_module.mde_font_engine

#ifdef MDE_MMAP_MANAGER
#define g_mde_mmap_manager g_opera->libgogi_module.m_mmap_man
#endif // MDE_MMAP_MANAGER


#ifdef MDE_NATIVE_WINDOW_SUPPORT
#define g_mdeNativeWindows g_opera->libgogi_module.nativeWindows
#endif // MDE_NATIVE_WINDOW_SUPPORT

#define g_mde_last_captured_input g_opera->libgogi_module.m_last_captured_input

#define LIBGOGI_MODULE_REQUIRED

#endif // !LIBGOGI_MODULE_H
