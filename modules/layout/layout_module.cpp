/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/layout/layout_module.h"
#include "modules/layout/content_generator.h"
#include "modules/layout/cssprops.h"
#include "modules/layout/box/box.h"
#include "modules/layout/content/content.h"
#include "modules/layout/layoutprops.h" // for ATTR_HEIGHT_SUPPORT, ATTR_HEIGHT_SUPPORT, ATTR_BACKGROUND_SUPPORT

#include "modules/display/styl_man.h"
#include "modules/pi/OpFont.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/util/handy.h"

#include "modules/layout/cssprops.h"

#define LAYOUT_PROPERTIES_POOL_SIZE 100		// Known to reach about 80 on usatoday.com and ebay.de
#define HTM_LAYOUT_POOL_SIZE  20
#define CONTAINER_REFLOW_STATE_POOL_SIZE 40	// More than big enough, probably
#define VERTICALBOX_REFLOW_STATE_POOL_SIZE 40
#define INLINEBOX_REFLOW_STATE_POOL_SIZE 40
#define ABSOLUTEBOX_REFLOW_STATE_POOL_SIZE 20
#define TABLECONTENT_REFLOW_STATE_POOL_SIZE 20
#define TABLEROWGROUP_REFLOW_STATE_POOL_SIZE 20
#define TABLEROW_REFLOW_STATE_POOL_SIZE 20
#define TABLECELL_REFLOW_STATE_POOL_SIZE 20
#define SPACEMANAGER_REFLOW_STATE_POOL_SIZE 40
#define FLOAT_REFLOW_CACHE_POOL_SIZE 200
#define REFLOWELEM_POOL_SIZE 200

#ifdef INTERNAL_SPELLCHECK_SUPPORT
# define MISSPELLING_PAINT_INFO_POOL_SIZE 100
#endif // INTERNAL_SPELLCHECK_SUPPORT

static void InitStylesL();

LayoutModule::LayoutModule() :
	content_generator(NULL),
	first_line_elm(NULL),
	m_img_alt_content(NULL),
	m_space_content(NULL),
	m_support_attr_array(NULL),
	m_blink_on(TRUE),
	layout_properties_pool(LAYOUT_PROPERTIES_POOL_SIZE),
	htm_layout_properties_pool(HTM_LAYOUT_POOL_SIZE),
	container_reflow_state_pool(CONTAINER_REFLOW_STATE_POOL_SIZE),
	verticalbox_reflow_state_pool(VERTICALBOX_REFLOW_STATE_POOL_SIZE),
	inlinebox_reflow_state_pool(INLINEBOX_REFLOW_STATE_POOL_SIZE),
	absolutebox_reflow_state_pool(ABSOLUTEBOX_REFLOW_STATE_POOL_SIZE),
	tablecontent_reflow_state_pool(TABLECONTENT_REFLOW_STATE_POOL_SIZE),
	tablerowgroup_reflow_state_pool(TABLEROWGROUP_REFLOW_STATE_POOL_SIZE),
	tablerow_reflow_state_pool(TABLEROW_REFLOW_STATE_POOL_SIZE),
	tablecell_reflow_state_pool(TABLECELL_REFLOW_STATE_POOL_SIZE),
	spacemanager_reflow_state_pool(SPACEMANAGER_REFLOW_STATE_POOL_SIZE),
	float_reflow_cache_pool(FLOAT_REFLOW_CACHE_POOL_SIZE),
	reflowelem_pool(REFLOWELEM_POOL_SIZE),
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	misspelling_paint_info_pool(MISSPELLING_PAINT_INFO_POOL_SIZE),
#endif // INTERNAL_SPELLCHECK_SUPPORT
	array_decl(0),
	m_shared_css_manager(NULL),
	props_array(NULL),
	tmp_word_info_array(NULL)
{
	quotes[0].value_type = 0;
	quotes[0].value.string = NULL;

#ifdef PLATFORM_FONTSWITCHING
	ellipsis_str[0] = 0;
	ellipsis_str[1] = 0;
#else
	ellipsis_str = NULL;
#endif
}

void
LayoutModule::InitL(const OperaInitInfo& info)
{
	content_generator = OP_NEW_L(ContentGenerator, ());

	LEAVE_IF_NULL(first_line_elm = NEW_HTML_Element());
    LEAVE_IF_ERROR(first_line_elm->Construct((HLDocProfile*)NULL, NS_IDX_DEFAULT, Markup::HTE_P, (HtmlAttrEntry*) NULL, HE_INSERTED_BY_LAYOUT));
	first_line_elm->SetIsFirstLinePseudoElement();

	CSS_generic_value gen_val;
	gen_val.value_type = CSS_FUNCTION_ATTR;
	gen_val.value.string = (uni_char *)UNI_L("alt");
	LEAVE_IF_NULL(m_img_alt_content = CSS_copy_gen_array_decl::Create(0, &gen_val, 1));

	gen_val.value_type = CSS_STRING_LITERAL;
	gen_val.value.string = (uni_char*)UNI_L(" ");
	LEAVE_IF_NULL(m_space_content = CSS_copy_gen_array_decl::Create(0, &gen_val, 1));

	m_support_attr_array = OP_NEWA_L(long, Markup::HTE_LAST - Markup::HTE_FIRST + 1);
	op_memset(m_support_attr_array, 0, sizeof(long)*(Markup::HTE_LAST - Markup::HTE_FIRST + 1));
	m_support_attr_array[Markup::HTE_IMG - Markup::HTE_FIRST] = ATTR_WIDTH_SUPPORT | ATTR_HEIGHT_SUPPORT;
#ifdef CANVAS_SUPPORT
	m_support_attr_array[Markup::HTE_CANVAS - Markup::HTE_FIRST] = ATTR_WIDTH_SUPPORT | ATTR_HEIGHT_SUPPORT;
#endif // CANVAS_SUPPORT
#ifdef MEDIA_HTML_SUPPORT
	m_support_attr_array[Markup::HTE_VIDEO - Markup::HTE_FIRST] = ATTR_WIDTH_SUPPORT | ATTR_HEIGHT_SUPPORT;
#endif // MEDIA_HTML_SUPPORT
	m_support_attr_array[Markup::HTE_EMBED - Markup::HTE_FIRST] = ATTR_WIDTH_SUPPORT | ATTR_HEIGHT_SUPPORT;
	m_support_attr_array[Markup::HTE_APPLET - Markup::HTE_FIRST] = ATTR_WIDTH_SUPPORT | ATTR_HEIGHT_SUPPORT;
	m_support_attr_array[Markup::HTE_OBJECT - Markup::HTE_FIRST] = ATTR_WIDTH_SUPPORT | ATTR_HEIGHT_SUPPORT;
	m_support_attr_array[Markup::HTE_IFRAME - Markup::HTE_FIRST] = ATTR_WIDTH_SUPPORT | ATTR_HEIGHT_SUPPORT;
	m_support_attr_array[Markup::HTE_MARQUEE - Markup::HTE_FIRST] = ATTR_WIDTH_SUPPORT | ATTR_HEIGHT_SUPPORT;
	m_support_attr_array[Markup::HTE_TABLE - Markup::HTE_FIRST] = ATTR_WIDTH_SUPPORT | ATTR_HEIGHT_SUPPORT | ATTR_BACKGROUND_SUPPORT;
	m_support_attr_array[Markup::HTE_TD - Markup::HTE_FIRST] = ATTR_WIDTH_SUPPORT | ATTR_HEIGHT_SUPPORT | ATTR_BACKGROUND_SUPPORT;
	m_support_attr_array[Markup::HTE_TH - Markup::HTE_FIRST] = ATTR_WIDTH_SUPPORT | ATTR_HEIGHT_SUPPORT | ATTR_BACKGROUND_SUPPORT;
	m_support_attr_array[Markup::HTE_TR - Markup::HTE_FIRST] = ATTR_BACKGROUND_SUPPORT | ATTR_HEIGHT_SUPPORT;
	m_support_attr_array[Markup::HTE_BODY - Markup::HTE_FIRST] = ATTR_BACKGROUND_SUPPORT;
	m_support_attr_array[Markup::HTE_HR - Markup::HTE_FIRST] = ATTR_WIDTH_SUPPORT;
	m_support_attr_array[Markup::HTE_PRE - Markup::HTE_FIRST] = ATTR_WIDTH_SUPPORT;
	m_support_attr_array[Markup::HTE_COL - Markup::HTE_FIRST] = ATTR_WIDTH_SUPPORT;
	m_support_attr_array[Markup::HTE_COLGROUP - Markup::HTE_FIRST] = ATTR_WIDTH_SUPPORT;

	InitMemoryPoolsL();

	InitStylesL();
#ifndef HAS_COMPLEX_GLOBALS
	extern void init_RomanStr();
	init_RomanStr();
	extern void init_DecimalLeadingZeroStr();
	init_DecimalLeadingZeroStr();
#endif // HAS_COMPLEX_GLOBALS
	m_shared_css_manager = SharedCssManager::CreateL();

	props_array = OP_NEWA_L(CssPropertyItem, CSSPROPS_ITEM_COUNT);
	tmp_word_info_array = OP_NEWA_L(WordInfo, WORD_INFO_MAX_SIZE);

#ifdef PLATFORM_FONTSWITCHING
	ellipsis_str[0] = 0x2026;
	ellipsis_str[1] = 0;
#else
	ellipsis_str = UNI_L("...");
#endif

	ellipsis_str_len = uni_strlen(ellipsis_str);
}

void
LayoutModule::Destroy()
{
	OP_DELETE(content_generator);
	content_generator = NULL;

	OP_DELETE(first_line_elm);
	first_line_elm = NULL;

	OP_DELETE(m_img_alt_content);
	m_img_alt_content = NULL;

	OP_DELETE(m_space_content);
	m_space_content = NULL;

	OP_DELETEA(m_support_attr_array);
	m_support_attr_array = NULL;

	DestroyMemoryPools();

	OP_DELETE(m_shared_css_manager);
	m_shared_css_manager = NULL;

	/* Reset the decl_is_referenced bit of all our CssPropertyItems.
	   The reference has been transfered to the props_array (which is
	   long gone). */

	if (props_array)
	{
		op_memset(props_array, 0, sizeof(CssPropertyItem) * CSSPROPS_ITEM_COUNT);
		OP_DELETEA(props_array);
		props_array = NULL;
	}

	OP_DELETEA(tmp_word_info_array);
	tmp_word_info_array = NULL;
}

void LayoutModule::InitMemoryPoolsL()
{
	layout_properties_pool.ConstructL();
	htm_layout_properties_pool.ConstructL();
	container_reflow_state_pool.ConstructL();
	verticalbox_reflow_state_pool.ConstructL();
	inlinebox_reflow_state_pool.ConstructL();
	absolutebox_reflow_state_pool.ConstructL();
	tablecontent_reflow_state_pool.ConstructL();
	tablerowgroup_reflow_state_pool.ConstructL();
	tablerow_reflow_state_pool.ConstructL();
	tablecell_reflow_state_pool.ConstructL();
	spacemanager_reflow_state_pool.ConstructL();
	float_reflow_cache_pool.ConstructL();
	reflowelem_pool.ConstructL();
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	misspelling_paint_info_pool.ConstructL();
#endif // INTERNAL_SPELLCHECK_SUPPORT
}

void LayoutModule::DestroyMemoryPools()
{
	layout_properties_pool.Clean();
	layout_properties_pool.Destroy();

	htm_layout_properties_pool.Clean();
	htm_layout_properties_pool.Destroy();

	container_reflow_state_pool.Clean();
	container_reflow_state_pool.Destroy();

	verticalbox_reflow_state_pool.Clean();
	verticalbox_reflow_state_pool.Destroy();

	inlinebox_reflow_state_pool.Clean();
	inlinebox_reflow_state_pool.Destroy();

	absolutebox_reflow_state_pool.Clean();
	absolutebox_reflow_state_pool.Destroy();

	tablecontent_reflow_state_pool.Clean();
	tablecontent_reflow_state_pool.Destroy();

	tablerowgroup_reflow_state_pool.Clean();
	tablerowgroup_reflow_state_pool.Destroy();

	tablerow_reflow_state_pool.Clean();
	tablerow_reflow_state_pool.Destroy();

	tablecell_reflow_state_pool.Clean();
	tablecell_reflow_state_pool.Destroy();

	spacemanager_reflow_state_pool.Clean();
	spacemanager_reflow_state_pool.Destroy();

	float_reflow_cache_pool.Clean();
	float_reflow_cache_pool.Destroy();

	reflowelem_pool.Clean();
	reflowelem_pool.Destroy();

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	misspelling_paint_info_pool.Clean();
	misspelling_paint_info_pool.Destroy();
#endif // INTERNAL_SPELLCHECK_SUPPORT
}

extern const float tsep_scale = (float) 1.0;
extern const float lsep_scale = (float) 0.5;

struct StyleCreateInfo
{
	unsigned short type;
	signed char lho, rho, ls, ts;
	bool i, b;
	char font;
	char col;
	bool default_col, scale, inherit_fs, uline, strike;
};

// Negative margin values x mean: (-x) em / 24
#define SEP_NORMAL -24
#define SEP_H1 -16
#define SEP_H2 -20
#define SEP_H3_L -24
#define SEP_H3_T -24
#define SEP_H4_L -24
#define SEP_H4_T -24
#define SEP_H5_L -40
#define SEP_H5_T -40
#define SEP_H6_L -56
#define SEP_H6_T -56
#define SEP_FORM_L 0
#define SEP_FORM_T -24
#define SEP_FS_L -16
#define SEP_FS_R -16
#define SEP_FS_T -8
#define SEP_FS_B -18



const StyleCreateInfo style_create[] =
{ // type lho rho ls ts i b font col def_col scal inh_fs ulin strike
	{ Markup::HTE_DOC_ROOT, 0, 0, 0, 0, false, false, OP_SYSTEM_FONT_DOCUMENT_NORMAL, OP_SYSTEM_COLOR_DOCUMENT_NORMAL, false, false, false, false, false },
	{ Markup::HTE_TABLE, 0, 0, 0, 0, false, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_TH, 0, 0, 0, 0, false, true, -1, -1, false, false, false, false, false },
	{ Markup::HTE_P, 0, 0, SEP_NORMAL, SEP_NORMAL, false, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_DD, DEFAULT_INDENT_PIXELS, 0, 0, 0, false, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_OL, DEFAULT_INDENT_PIXELS, 0, SEP_NORMAL, SEP_NORMAL, false, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_UL, DEFAULT_INDENT_PIXELS, 0, SEP_NORMAL, SEP_NORMAL, false, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_DL, 0, 0, SEP_NORMAL, SEP_NORMAL, false, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_MENU, DEFAULT_INDENT_PIXELS, 0, SEP_NORMAL, SEP_NORMAL, false, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_DIR, DEFAULT_INDENT_PIXELS, 0, SEP_NORMAL, SEP_NORMAL, false, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_BLOCKQUOTE, DEFAULT_INDENT_PIXELS, DEFAULT_INDENT_PIXELS, SEP_NORMAL, SEP_NORMAL, false, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_FIGURE, DEFAULT_INDENT_PIXELS, DEFAULT_INDENT_PIXELS, SEP_NORMAL, SEP_NORMAL, false, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_HR, 0, 0, -12, -12, false, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_ISINDEX, 0, 0, 10, 10, false, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_LI, 0, 0, 0, 0, false, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_H1, 0, 0, SEP_H1, SEP_H1, false, false, OP_SYSTEM_FONT_DOCUMENT_HEADER1, OP_SYSTEM_COLOR_DOCUMENT_HEADER1, true, false, false, false, false },
	{ Markup::HTE_H2, 0, 0, SEP_H2, SEP_H2, false, false, OP_SYSTEM_FONT_DOCUMENT_HEADER2, OP_SYSTEM_COLOR_DOCUMENT_HEADER2, true, false, false, false, false },
	{ Markup::HTE_H3, 0, 0, SEP_H3_L, SEP_H3_T, false, false, OP_SYSTEM_FONT_DOCUMENT_HEADER3, OP_SYSTEM_COLOR_DOCUMENT_HEADER3, false, false, false, false, false },
	{ Markup::HTE_H4, 0, 0, SEP_H4_L, SEP_H4_T, false, false, OP_SYSTEM_FONT_DOCUMENT_HEADER4, OP_SYSTEM_COLOR_DOCUMENT_HEADER4, false, false, false, false, false },
	{ Markup::HTE_H5, 0, 0, SEP_H5_L, SEP_H5_T, false, false, OP_SYSTEM_FONT_DOCUMENT_HEADER5, OP_SYSTEM_COLOR_DOCUMENT_HEADER5, false, false, false, false, false },
	{ Markup::HTE_H6, 0, 0, SEP_H6_L, SEP_H6_T, false, false, OP_SYSTEM_FONT_DOCUMENT_HEADER6, OP_SYSTEM_COLOR_DOCUMENT_HEADER6, false, false, false, false, false },
	{ Markup::HTE_DFN, 0, 0, 0, 0, true, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_U, 0, 0, 0, 0, false, false, -1, -1, false, false, true, false, false },
	{ Markup::HTE_STRIKE, 0, 0, 0, 0, false, false, -1, -1, false, false, false, true, false },
	{ Markup::HTE_S, 0, 0, 0, 0, false, false, -1, -1, false, false, false, true, false },
	{ Markup::HTE_I, 0, 0, 0, 0, true, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_ADDRESS, 0, 0, 0, 0, true, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_B, 0, 0, 0, 0, false, true, -1, -1, false, false, false, false, false },
	{ Markup::HTE_EM, 0, 0, 0, 0, true, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_VAR, 0, 0, 0, 0, true, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_CITE, 0, 0, 0, 0, true, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_A, 0, 0, 0, 0, false, false, -1, OP_SYSTEM_COLOR_LINK, false, false, false, false, false },
	{ Markup::HTE_STRONG, 0, 0, 0, 0, false, true, -1, -1, false, false, false, false, false },
	{ Markup::HTE_INPUT, 0, 0, 0, 0, false, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_SELECT, 0, 0, 0, 0, false, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_TEXTAREA, 0, 0, 0, 0, false, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_FORM, 0, 0, SEP_FORM_L, SEP_FORM_T, false, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_FIELDSET, SEP_FS_L, SEP_FS_R, SEP_FS_T, SEP_FS_B, false, false, -1, -1, false, false, false, false, false },
	{ Markup::HTE_PRE, 0, 0, SEP_NORMAL, SEP_NORMAL, false, false, OP_SYSTEM_FONT_DOCUMENT_PRE, OP_SYSTEM_COLOR_DOCUMENT_PRE, false, false, false, false, false },
	{ Markup::HTE_LISTING, 0, 0, 0, 0, false, false, OP_SYSTEM_FONT_DOCUMENT_PRE, OP_SYSTEM_COLOR_DOCUMENT_PRE, false, false, false, false, false },
	{ Markup::HTE_PLAINTEXT, 0, 0, 0, 0, false, false, OP_SYSTEM_FONT_DOCUMENT_PRE, OP_SYSTEM_COLOR_DOCUMENT_PRE, false, false, false, false, false },
	{ Markup::HTE_XMP, 0, 0, 19, 19, false, false, OP_SYSTEM_FONT_DOCUMENT_PRE, OP_SYSTEM_COLOR_DOCUMENT_PRE, false, false, false, false, false },
	{ Markup::HTE_CODE, 0, 0, 0, 0, false, false, OP_SYSTEM_FONT_DOCUMENT_PRE, OP_SYSTEM_COLOR_DOCUMENT_PRE, false, false, false, false, false },
	{ Markup::HTE_SAMP, 0, 0, 0, 0, false, false, OP_SYSTEM_FONT_DOCUMENT_PRE, OP_SYSTEM_COLOR_DOCUMENT_PRE, false, false, false, false, false },
	{ Markup::HTE_TT, 0, 0, 0, 0, false, false, OP_SYSTEM_FONT_DOCUMENT_PRE, OP_SYSTEM_COLOR_DOCUMENT_PRE, false, false, false, false, false },
	{ Markup::HTE_KBD, 0, 0, 0, 0, false, false, OP_SYSTEM_FONT_DOCUMENT_PRE, OP_SYSTEM_COLOR_DOCUMENT_PRE, false, false, false, false, false },
};

const StyleCreateInfo form_style_create[] =
{      // type                  lho rho ls ts    i      b               font                         col                             def_col scal  inh_fs  ulin  strike
	{ STYLE_EX_FORM_SELECT, 	 0,  0,  0,  0, false, false, OP_SYSTEM_FONT_FORM_BUTTON, 		OP_SYSTEM_COLOR_TEXT, 				false, false, false, false, false },
	{ STYLE_EX_FORM_BUTTON, 	 0,  0,  0,  0, false, false, OP_SYSTEM_FONT_FORM_BUTTON, 		OP_SYSTEM_COLOR_TEXT,				false, false, false, false, false },
	{ STYLE_EX_FORM_TEXTINPUT, 	 0,  0,  0,  0, false, false, OP_SYSTEM_FONT_FORM_TEXT_INPUT, 	OP_SYSTEM_COLOR_TEXT_INPUT, 		false, false, false, false, false },
	{ STYLE_EX_FORM_TEXTAREA, 	 0,  0,  0,  0, false, false, OP_SYSTEM_FONT_FORM_TEXT, 		OP_SYSTEM_COLOR_TEXT, 				false, false, false, false, false },
	{ STYLE_MAIL_BODY,           0,  0,  0,  0, false, false, OP_SYSTEM_FONT_FORM_TEXT_INPUT,   OP_SYSTEM_COLOR_TEXT_INPUT,         false, false, false, false, false }
};

static void InitStylesL()
{
	FontAtt log_font;
	COLORREF col = USE_DEFAULT_COLOR;
	char last_font = -1;
	char last_color = -1;
	FontAtt* font_att = NULL; // Will be reused between iterations in the loop

	for (unsigned j = 0; j < 2; j++)
	{
		BOOL is_form_styles = j == 1;
		const StyleCreateInfo* current_table;
		unsigned char table_size;
		if (is_form_styles)
		{
			current_table = form_style_create;
			table_size = ARRAY_SIZE(form_style_create);
		}
		else
		{
			current_table = style_create;
			table_size = ARRAY_SIZE(style_create);
		}

		for (unsigned char i = 0; i < table_size; ++i)
		{
			const StyleCreateInfo* style_c_info = &current_table[i];

			if (style_c_info->font == -1)
				font_att = NULL;
			else if (style_c_info->font != last_font)
			{
				g_pcfontscolors->GetFont((OP_SYSTEM_FONT)style_c_info->font, log_font);
				font_att = &log_font;
				last_font = style_c_info->font;
			}

#ifdef HAS_FONT_FOUNDRY
			if (style_c_info->type == Markup::HTE_DOC_ROOT && !is_form_styles)
			{
				OpFontManager *fontman = styleManager->GetFontManager();
				if (log_font.GetFaceName())
				{
					OpString foundry;
					LEAVE_IF_ERROR(fontman->GetFoundry(log_font.GetFaceName(), foundry));
					LEAVE_IF_ERROR(fontman->SetPreferredFoundry(foundry.CStr()));
				}
			}
#endif // HAS_FONT_FOUNDRY

			if (style_c_info->col == -1)
				col = USE_DEFAULT_COLOR;
			else if (style_c_info->col != last_color)
				col = g_pcfontscolors->GetColor((OP_SYSTEM_COLOR)style_c_info->col);

			short lsep = style_c_info->ls;
			short tsep = style_c_info->ts;
			if (style_c_info->scale)
			{
				lsep = (short) (op_abs(int(log_font.GetHeight() * (float)0.5)));
				tsep = (short) (op_abs(int(log_font.GetHeight() * (float)1.0)));
			}

			BOOL uline = style_c_info->uline;
			BOOL strike  = style_c_info->strike;

			if (style_c_info->type == Markup::HTE_A && !is_form_styles)
			{
				if (!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LinkHasColor))
					col = USE_DEFAULT_COLOR;
				uline = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LinkHasUnderline);
				strike = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LinkHasStrikeThrough);
			}

			Style* style = Style::Create(style_c_info->lho, style_c_info->rho, 0, 0, lsep, tsep, style_c_info->i, style_c_info->b, font_att, col, style_c_info->inherit_fs, uline, strike);
			LEAVE_IF_NULL(style);

			if (is_form_styles)
			{
				styleManager->SetStyleEx(style_c_info->type, style);
			}
			else
			{
				LEAVE_IF_ERROR(styleManager->SetStyle((Markup::Type)style_c_info->type, style));
			}
		}
	}
}
