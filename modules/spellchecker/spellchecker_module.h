/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SPELLCHECKER_MODULE_H
#define SPELLCHECKER_MODULE_H

#ifdef INTERNAL_SPELLCHECK_SUPPORT

#include "modules/hardcore/opera/module.h"

class OpSpellCheckerManager;
class SpellUIData;



#if defined(SPC_USE_HUNSPELL_ENGINE)
#define HUNSPELL_STATIC
#define hunspell_u_toupper(c) uni_toupper(c)
#define hunspell_u_tolower(c) uni_tolower(c)
#define hunspell_u_isalpha(c) uni_isalpha(c)
#ifndef HAS_COMPLEX_GLOBALS
#include "modules/spellchecker/hunspell_3p/src/hunspell/csutil.h"
#define HUNSPELL_GARRAY(name) (&(g_opera->spellchecker_module.m_##name[0]))
#define g_hunspell_iso1_tbl HUNSPELL_GARRAY(iso1_tbl)
#define g_hunspell_iso2_tbl HUNSPELL_GARRAY(iso2_tbl)
#define g_hunspell_iso3_tbl HUNSPELL_GARRAY(iso3_tbl)
#define g_hunspell_iso4_tbl HUNSPELL_GARRAY(iso4_tbl)
#define g_hunspell_iso5_tbl HUNSPELL_GARRAY(iso5_tbl)
#define g_hunspell_iso6_tbl HUNSPELL_GARRAY(iso6_tbl)
#define g_hunspell_iso7_tbl HUNSPELL_GARRAY(iso7_tbl)
#define g_hunspell_iso8_tbl HUNSPELL_GARRAY(iso8_tbl)
#define g_hunspell_iso9_tbl HUNSPELL_GARRAY(iso9_tbl)
#define g_hunspell_iso10_tbl HUNSPELL_GARRAY(iso10_tbl)
#define g_hunspell_koi8r_tbl HUNSPELL_GARRAY(koi8r_tbl)
#define g_hunspell_koi8u_tbl HUNSPELL_GARRAY(koi8u_tbl)
#define g_hunspell_cp1251_tbl HUNSPELL_GARRAY(cp1251_tbl)
#define g_hunspell_iso13_tbl HUNSPELL_GARRAY(iso13_tbl)
#define g_hunspell_iso14_tbl HUNSPELL_GARRAY(iso14_tbl)
#define g_hunspell_iso15_tbl HUNSPELL_GARRAY(iso15_tbl)
#define g_hunspell_iscii_devanagari_tbl HUNSPELL_GARRAY(iscii_devanagari_tbl)
#define g_hunspell_tis620_tbl HUNSPELL_GARRAY(tis620_tbl)
#define g_hunspell_encds HUNSPELL_GARRAY(encds)
#define g_hunspell_lang2enc HUNSPELL_GARRAY(lang2enc)

extern void hunspell_init_iso1_tbl();
extern void hunspell_init_iso2_tbl();
extern void hunspell_init_iso3_tbl();
extern void hunspell_init_iso4_tbl();
extern void hunspell_init_iso5_tbl();
extern void hunspell_init_iso6_tbl();
extern void hunspell_init_iso7_tbl();
extern void hunspell_init_iso8_tbl();
extern void hunspell_init_iso9_tbl();
extern void hunspell_init_iso10_tbl();
extern void hunspell_init_koi8r_tbl();
extern void hunspell_init_koi8u_tbl();
extern void hunspell_init_cp1251_tbl();
extern void hunspell_init_iso13_tbl();
extern void hunspell_init_iso14_tbl();
extern void hunspell_init_iso15_tbl();
extern void hunspell_init_iscii_devanagari_tbl();
extern void hunspell_init_tis620_tbl();
extern void hunspell_init_encds();
extern void hunspell_init_lang2enc();

#endif
#endif

class SpellcheckerModule : public OperaModule
{
public:
	SpellcheckerModule() : m_manager(NULL), m_ui_data(NULL) {
#if defined(SPC_USE_HUNSPELL_ENGINE) && !defined(HAS_COMPLEX_GLOBALS)
		hunspell_init_iso1_tbl();
		hunspell_init_iso2_tbl();
		hunspell_init_iso3_tbl();
		hunspell_init_iso4_tbl();
		hunspell_init_iso5_tbl();
		hunspell_init_iso6_tbl();
		hunspell_init_iso7_tbl();
		hunspell_init_iso8_tbl();
		hunspell_init_iso9_tbl();
		hunspell_init_iso10_tbl();
		hunspell_init_koi8r_tbl();
		hunspell_init_koi8u_tbl();
		hunspell_init_cp1251_tbl();
		hunspell_init_iso13_tbl();
		hunspell_init_iso14_tbl();
		hunspell_init_iso15_tbl();
		hunspell_init_iscii_devanagari_tbl();
		hunspell_init_tis620_tbl();
		hunspell_init_encds();
		hunspell_init_lang2enc();
#endif
	}
	~SpellcheckerModule();
	
	// From OperaModule
	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();

	BOOL FreeCachedData(BOOL toplevel_context);
	OpSpellCheckerManager *GetSpellCheckManager() { return m_manager; }
	SpellUIData *GetSpellCheckUIData() { return m_ui_data; }

#if defined(SPC_USE_HUNSPELL_ENGINE) && !defined(HAS_COMPLEX_GLOBALS)
	struct cs_info m_iso1_tbl[256];
	struct cs_info m_iso2_tbl[256];
	struct cs_info m_iso3_tbl[256];
	struct cs_info m_iso4_tbl[256];
	struct cs_info m_iso5_tbl[256];
	struct cs_info m_iso6_tbl[256];
	struct cs_info m_iso7_tbl[256];
	struct cs_info m_iso8_tbl[256];
	struct cs_info m_iso9_tbl[256];
	struct cs_info m_iso10_tbl[256];
	struct cs_info m_koi8r_tbl[256];
	struct cs_info m_koi8u_tbl[256];
	struct cs_info m_cp1251_tbl[256];
	struct cs_info m_iso13_tbl[256];
	struct cs_info m_iso14_tbl[256];
	struct cs_info m_iso15_tbl[256];
	struct cs_info m_iscii_devanagari_tbl[256];
	struct cs_info m_tis620_tbl[256];
	struct enc_entry m_encds[HUNSPELL_NUM_ENCDS];
	struct lang_map m_lang2enc[HUNSPELL_NUM_LANGS];
#endif
private:
	OpSpellCheckerManager *m_manager;
	SpellUIData *m_ui_data;
};

#define g_internal_spellcheck (g_opera->spellchecker_module.GetSpellCheckManager())
#define g_spell_ui_data (g_opera->spellchecker_module.GetSpellCheckUIData())

#define SPELLCHECKER_MODULE_REQUIRED

#endif // INTERNAL_SPELLCHECK_SUPPORT
#endif // !SPELLCHECKER_MODULE_H
