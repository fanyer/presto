/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifdef MDEFONT_MODULE

#ifndef MDEFONT_MODULE_H
#define MDEFONT_MODULE_H

class MDF_FontEngine;

class MdefontModule : public OperaModule
{
public:
	MdefontModule();

    void InitL(const OperaInitInfo& info);
    void Destroy();

	MDF_FontEngine* GetFontEngine();
	MDF_FontEngine* mdf_engine;

#ifdef USE_ONLY_LIMITED_FONT_SIZE_SET
	int* FixedSizeTable;
	int NumFixedSizes;
#endif // USE_ONLY_LIMITED_FONT_SIZE_SET

#ifdef MDF_OPENTYPE_SUPPORT
	struct IndicGlyphClass* m_indic_scripts;
	BOOL m_ot_buflock;
	class TempBuffer* m_ot_ubuf;
	class TempBuffer* m_ot_ibuf;
#endif // MDF_OPENTYPE_SUPPORT
};

inline
MDF_FontEngine* MdefontModule::GetFontEngine()
{
	OP_ASSERT(mdf_engine);
	return mdf_engine;
}

#define g_mdf_engine g_opera->mdefont_module.GetFontEngine()

#ifdef MDF_OPENTYPE_SUPPORT
#define g_indic_scripts g_opera->mdefont_module.m_indic_scripts
#define g_opentype_ubuf g_opera->mdefont_module.m_ot_ubuf
#define g_opentype_ibuf g_opera->mdefont_module.m_ot_ibuf
#define g_opentype_buflock g_opera->mdefont_module.m_ot_buflock
#endif // MDF_OPENTYPE_SUPPORT

#define MDEFONT_MODULE_REQUIRED

#endif // MDEFONT_MODULE_H

#endif // MDEFONT_MODULE
