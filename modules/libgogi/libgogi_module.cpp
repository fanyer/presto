/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef LIBGOGI_MODULE_REQUIRED

#ifdef WIDGETS_IME_SUPPORT
#include "modules/libgogi/pi_impl/mde_ime_manager.h"
#endif // WIDGETS_IME_SUPPORT
#ifdef MDE_MMAP_MANAGER
#include "modules/libgogi/mde_mmap.h"
#endif // MDE_MMAP_MANAGER

#ifdef USE_PREMULTIPLIED_ALPHA
# include "mde_config.h"
void SetColorMap(unsigned int* col_map, unsigned int color);
#endif // USE_PREMULTIPLIED_ALPHA

LibgogiModule::LibgogiModule()
	: mde_font_engine(NULL)
#ifdef MDE_USE_ALPHA_LOOKUPTABLE
	, lutbl_alpha(0)
#endif
	,ime_manager(NULL)
#ifdef MDE_MMAP_MANAGER
	, m_mmap_man(NULL)
#endif // MDE_MMAP_MANAGER
	, m_last_captured_input(0)
{
#ifdef USE_PREMULTIPLIED_ALPHA
	// should trigger regeneration first time, since alpha is 0
	m_generic_color_lookup[256] = 0;
	m_color_lookup = 0;
#endif // USE_PREMULTIPLIED_ALPHA
}

void
LibgogiModule::InitL(const OperaInitInfo& info)
{
#ifdef MDE_USE_ALPHA_LOOKUPTABLE
	lutbl_alpha = OP_NEWA_L(unsigned char, 256 * 256);
	for(int j = 0; j < 256; j++)
		for(int i = 0; i < 256; i++)
		{
# ifdef USE_PREMULTIPLIED_ALPHA
			/**
			   i should be seen as alpha and j color in this loop. ie
			   the alpha blended color value for a color c and an
			   alpha value a is lutbl_alpha[(a << 8) + c]
			 */
			lutbl_alpha[(i << 8) + j] = (i * (j+1)) >> 8;
# else // USE_PREMULTIPLIED_ALPHA
			int total_alpha = MIN(i + j, 255);
			int a = (i << 8) / (total_alpha + 1);
			lutbl_alpha[i + (j << 8)] = a;
# endif // USE_PREMULTIPLIED_ALPHA
		}
#endif

#ifdef USE_PREMULTIPLIED_ALPHA
	SetColorMap(m_black_lookup, static_cast<unsigned>(MDE_RGBA(0,0,0,0xff)));
#endif // USE_PREMULTIPLIED_ALPHA

#ifdef MDE_MMAP_MANAGER
	if (!m_mmap_man)
	{
		// Can sometimes be created by the font engine
		m_mmap_man = OP_NEW_L(MDE_MMapManager, ());
	}
#endif // MDE_MMAP_MANAGER


#ifdef WIDGETS_IME_SUPPORT
	LEAVE_IF_ERROR(MDE_IMEManager::Create(&ime_manager));
#endif // WIDGETS_IME_SUPPORT
}

void
LibgogiModule::Destroy()
{
#ifdef WIDGETS_IME_SUPPORT
	OP_DELETE(ime_manager);
#endif // WIDGETS_IME_SUPPORT}
#ifdef MDE_USE_ALPHA_LOOKUPTABLE
	OP_DELETEA(lutbl_alpha);
#endif
#ifdef MDE_MMAP_MANAGER
	OP_DELETE(m_mmap_man);
	m_mmap_man = NULL;
#endif // MDE_MMAP_MANAGER
}

#endif // LIBGOGI_MODULE_REQUIRED
