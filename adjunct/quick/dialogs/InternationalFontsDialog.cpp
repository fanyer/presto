/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "InternationalFontsDialog.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/display/styl_man.h"
#include "modules/pi/OpLocale.h"
#include "modules/widgets/OpWidgetFactory.h"
#include "modules/style/css.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"

struct InternationalFontInfo
{
	Str::StringList1		block_name_string;
	UINT8					block_nr;
	const uni_char*			example_string;
	WritingSystem::Script	script;
};

InternationalFontInfo blocks[num_blocks]  = {
	{ Str::SI_IDPREFS_BLOCK_ARABIC,			13,	UNI_L("\x639\x631\x628\x64a\x629"),				WritingSystem::Arabic }, // Spells "Arabic" in Arabic
	{ Str::SI_IDPREFS_BLOCK_ARMENIAN,		10,	UNI_L("\x531\x532\x533\x561\x562\x563"),		WritingSystem::Unknown },
	{ Str::SI_IDPREFS_BLOCK_BASICLATIN,		0,	UNI_L("Opera ")/* UNI_L(VER_NUM_STR)*/,			WritingSystem::LatinWestern },
	{ Str::SI_IDPREFS_BLOCK_BENGALI,		16,	UNI_L("\x985\x986\x987\x988\x995\x996\x997"),	WritingSystem::IndicBengali },
	{ Str::SI_IDPREFS_BLOCK_CHEROKEE,		76,	UNI_L("\x13A0\x13A1\x13A2\x13A3\x13A4\x13A5"),	WritingSystem::Unknown },
	{ Str::SI_IDPREFS_BLOCK_SIMPLIFIED,		59,	UNI_L("\x7B80\x4F53\x4E2D\x6587"),				WritingSystem::ChineseSimplified }, // Spells "Simplified Chinese" in Chinese
	{ Str::SI_IDPREFS_BLOCK_TRADITIONAL,	59,	UNI_L("\x7E41\x9AD4\x4E2D\x6587"),				WritingSystem::ChineseTraditional },// Spells "Traditional Chinese" in Chinese
	{ Str::SI_IDPREFS_BLOCK_CURRENCYSYMBOLS,33,	UNI_L("\x20A0\x20A1\x20A2\x20A3\x20A4"),		WritingSystem::LatinWestern },
	{ Str::SI_IDPREFS_BLOCK_CYRILLIC,		9,	UNI_L("\x420\x443\x441\x441\x43A\x438\x439"),	WritingSystem::Cyrillic},
	{ Str::SI_IDPREFS_BLOCK_DEVANGARI,		15,	UNI_L("\x905\x906\x907\x915\x916\x917"),		WritingSystem::IndicDevanagari },
	{ Str::SI_IDPREFS_BLOCK_ETHIOPIC,		75,	UNI_L("\x1200\x1201\x1202\x1203\x1204\x1205"),	WritingSystem::Unknown },
	{ Str::SI_IDPREFS_BLOCK_GEN_PUNCTUATION,31,	UNI_L("\x2030\x2031\x2032\x2033\x2034\x2035"),	WritingSystem::Unknown },
	{ Str::SI_IDPREFS_BLOCK_GEORGIAN,		26,	UNI_L("\x10D0\x10D1\x10D2\x10A0\x10A1\x10A2"),	WritingSystem::Unknown },
	{ Str::SI_IDPREFS_BLOCK_GREEK,			7,	UNI_L("\x395\x3BB\x3BB\x3B7\x3BD\x3B9\x3BA\x3AC"), WritingSystem::Greek }, // Spells "Greek" in Greek
	{ Str::SI_IDPREFS_BLOCK_GREEK_EXT,		30,	UNI_L("\x1F00\x1F08\x1F10\x1F18\x1FE5\x1FEC"),	WritingSystem::Greek },
	{ Str::SI_IDPREFS_BLOCK_GUJARATI,		18,	UNI_L("\xA85\xA86\xA87\xA88\xA95\xA96\xA97"),	WritingSystem::IndicGujarati },
	{ Str::SI_IDPREFS_BLOCK_GURMUKHI,		17, UNI_L("\xA05\xA06\xA07\xA08\xA15\xA16\xA17"),	WritingSystem::IndicGurmukhi },
	{ Str::SI_IDPREFS_BLOCK_WIDTHFORMS,		68,	UNI_L("\xFF1F\xFF40\xFF1A\xFF1B\xFF01"),		WritingSystem::Unknown },
	{ Str::SI_IDPREFS_BLOCK_HANGUL,			56,	UNI_L("\xD55C\xAD6D\xC5B4"),					WritingSystem::Korean }, // Spells "Korean" in Korean (Hangul)
	{ Str::SI_IDPREFS_BLOCK_HEBREW,			11,	UNI_L("\x5D0\x5D1\x5D2\x5D3\x5D4\x5D5\x5D6"),	WritingSystem::Hebrew },
	{ Str::SI_IDPREFS_BLOCK_HIRAGANA,		49,	UNI_L("\x3072\x3089\x304C\x306A"),				WritingSystem::Japanese }, // Spells "Hiragana" in Japanese
	{ Str::SI_IDPREFS_BLOCK_CJK_SYMBOLS,	48,	UNI_L("\x3004\x300A\x300B\x3012\x3021\x3022\x3023"), WritingSystem::Unknown },
	{ Str::SI_IDPREFS_BLOCK_KANJI,			61,	UNI_L("\x6F22\x5B57"),							WritingSystem::Japanese }, // Spells "Kanji" in Japanese
	{ Str::SI_IDPREFS_BLOCK_KANNADA,		22, UNI_L("\xC85\xC86\xC87\xC88\xC95\xC96\xC97"),	WritingSystem::IndicKannada },
	{ Str::SI_IDPREFS_BLOCK_KATAKANA,		50,	UNI_L("\x30AB\x30BF\x30AB\x30CA"),				WritingSystem::Japanese } , // Spells "Katakana" in Japanese
	{ Str::SI_IDPREFS_BLOCK_KHMER,			80,	UNI_L("\x1780\x1781\x1782\x17A3\x17A4\x17A5"),	WritingSystem::Unknown },
	{ Str::SI_IDPREFS_BLOCK_LAO,			25,	UNI_L("\xE81\xE82\xE84\xE87\xEB0\xEC0\xEC6"),	WritingSystem::IndicLao },
	{ Str::SI_IDPREFS_BLOCK_LATIN1,			1,	UNI_L("\xA9\xC6\xD8\xC5\xC4\xD6\xE6\xF8\xE5\xE4\xF6"), WritingSystem::LatinWestern },
	{ Str::SI_IDPREFS_BLOCK_LATIN_EXT_A,	2,	UNI_L("\x100\x101\x126\x127\x132\x133\x13F\x140"), WritingSystem::LatinUnknown },
	{ Str::SI_IDPREFS_BLOCK_LATIN_EXT_B,	3,  UNI_L("\x180\x181\x1A2\x1A3\x1B5\x1B6\x1F1\x1F3"), WritingSystem::LatinUnknown},
	{ Str::SI_IDPREFS_BLOCK_LATIN_EXT,		29,	UNI_L("\x1E00\x1E01\x1E02\x1E03\x1EAA\x1EAB"),	WritingSystem::LatinUnknown},
	{ Str::SI_IDPREFS_BLOCK_MALAYALAM,		23, UNI_L("\xD05\xD06\xD07\xD08\xD15\xD16\xD17"),	WritingSystem::IndicMalayalam },
	{ Str::SI_IDPREFS_BLOCK_MONGOLIAN,		81,	UNI_L("\x1820\x1821\x1822\x1811\x1812\x1813"),	WritingSystem::Unknown },
	{ Str::SI_IDPREFS_BLOCK_MYANMAR,		74,	UNI_L("\x1000\x1001\x1002\x1041\x1042\x1043"),	WritingSystem::IndicMyanmar },
	{ Str::SI_IDPREFS_BLOCK_NUMBERFORMS,	36,	UNI_L("\x2160\x2161\x2162\x2163\x2164"),		WritingSystem::Unknown },
	{ Str::SI_IDPREFS_BLOCK_OGHAM,			78,	UNI_L("\x169B\x1681\x1682\x1683\x1684\x1685\x1685\x169C"), WritingSystem::Unknown },
	{ Str::SI_IDPREFS_BLOCK_ORIYA,			19,	UNI_L("\xB05\xB06\xB07\xB08\xB15\xB16\xB17"),	WritingSystem::IndicOriya },
	{ Str::SI_IDPREFS_BLOCK_RUNIC,			79,	UNI_L("\x16AE\x16D4\x16C2\x16B1\x16C6\x16EC\x16DF\x16C8\x16D6\x16B1\x16AB"), WritingSystem::Unknown }, // Spells "Opera" in Runic (twice, using different alphabets) :-)
	{ Str::SI_IDPREFS_BLOCK_SINHALA,		73, UNI_L("\xD85\xD86\xD87\xD9A\xD9B\xD9C"),		WritingSystem::IndicSinhala },
	{ Str::SI_IDPREFS_BLOCK_SYRIAC,			71,	UNI_L("\x710\x712\x713\x715\x717\x700"),		WritingSystem::Unknown },
	{ Str::SI_IDPREFS_BLOCK_TAMIL,			20, UNI_L("\xB85\xB86\xB87\xB88\xB95\xB99\xB9A"),	WritingSystem::IndicTamil },
	{ Str::SI_IDPREFS_BLOCK_TELUGU,			21,	UNI_L("\xC05\xC06\xC07\xC07\xC15\xC16\xC17"),	WritingSystem::IndicTelugu },
	{ Str::SI_IDPREFS_BLOCK_THAANA,			72,	UNI_L("\x780\x781\x782\x798\x799\x79A"),		WritingSystem::Unknown },
	{ Str::SI_IDPREFS_BLOCK_THAI,			24,	UNI_L("\xE01\xE02\xE03\xE04\xE30\xE3F"),		WritingSystem::Thai },
	{ Str::SI_IDPREFS_BLOCK_TIBETAN,		70, UNI_L("\xF00\xF01\xF02\xF03\xF20\xF21\xF22"),	WritingSystem::Unknown },
	{ Str::SI_IDPREFS_BLOCK_CANADIAN,		77,	UNI_L("\x1401\x1402\x1403\x1404\x1405\x1406"),	WritingSystem::Unknown }
};

static int strcompare(const void *arg1, const void *arg2)
{
	int ans;
	TRAPD(err, ans = g_oplocale->CompareStringsL(*((uni_char**)arg1), *((uni_char**)arg2)));
	OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: handle error
	return ans;
}

static int compareblocknames(const void *arg1, const void *arg2)
{
# define TEMPBUFFERSIZE 64

	OpString title1;
	OpString title2;

	g_languageManager->GetString(((InternationalFontInfo*) arg1)->block_name_string, title1);
	g_languageManager->GetString(((InternationalFontInfo*) arg2)->block_name_string, title2);
	/* Make sure we sort in locale-specific order, because if we didn't Swedish
	 * text would be sorted ???instead of ??? which is the correct way to do
	 * it. */
	int ans;
	TRAPD(err, ans = g_oplocale->CompareStringsL(title1.CStr(), title2.CStr()));
	OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: handle error
	return ans;
}


InternationalFontsDialog::InternationalFontsDialog()
{

}

InternationalFontsDialog::~InternationalFontsDialog()
{
	OP_DELETEA(m_normal_list);
	OP_DELETEA(m_mono_list);
}


void InternationalFontsDialog::Init(DesktopWindow* parent_window)
{
	Dialog::Init(parent_window);
}


void InternationalFontsDialog::OnInit()
{
#ifdef LANGUAGE_FILE_SUPPORT
	// Sort the block names alphabetically if we may have
	// translated them
	static BOOL s_block_dialog_sorted = FALSE;

	if (!s_block_dialog_sorted)
	{
		qsort((void *) blocks, num_blocks, sizeof(InternationalFontInfo), compareblocknames);
		s_block_dialog_sorted = TRUE;
	}
#endif
	OpString encodingstring;

	m_normal_list = OP_NEWA(uni_char*, styleManager->GetFontManager()->CountFonts());
	m_mono_list = OP_NEWA(uni_char*, styleManager->GetFontManager()->CountFonts());
	if (!m_normal_list || !m_mono_list)
	{
		Close(TRUE);
		return;
	}

	// Add unicode block names to combo box and fill in cached settings

	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Encoding_dropdown");

	if (dropdown)
	{
		for(int i=0; i < num_blocks; i++)
		{
			g_languageManager->GetString(blocks[i].block_name_string, encodingstring);

			INT32 got_index;
			dropdown->AddItem(encodingstring.CStr(), -1, &got_index);

			const OpFontInfo* font;
			font = styleManager->GetPreferredFont(blocks[i].block_nr, FALSE, blocks[i].script);
			m_cached_normal[i] = (font ? font->GetFace() : NULL);
			font = styleManager->GetPreferredFont(blocks[i].block_nr, TRUE, blocks[i].script);
			m_cached_mono[i] = (font ? font->GetFace() : NULL);
		}
	}


	OpTreeView* normal_treeview = (OpTreeView*) GetWidgetByName("Normal_font_treeview");
	OpTreeView* monotype_treeview = (OpTreeView*) GetWidgetByName("Monotype_font_treeview");

	normal_treeview->SetDeselectable(FALSE);
	monotype_treeview->SetDeselectable(FALSE);

	normal_treeview->SetShowColumnHeaders(FALSE);
	monotype_treeview->SetShowColumnHeaders(FALSE);

	normal_treeview->SetTreeModel(&m_normal_model);
	monotype_treeview->SetTreeModel(&m_monotype_model);

	g_languageManager->GetString(Str::SI_IDPREFS_BLOCK_DEFAULT_FONT, m_default_choice_str);
	g_languageManager->GetString(Str::SI_IDPREFS_BLOCK_NO_FONTS, m_no_fonts_str);

	OnEncodingChange();
}


UINT32 InternationalFontsDialog::OnOk()
{
	// Update settings in style manager and write to prefs file
	for(int i=0; i < num_blocks; ++i)
	{
		if (blocks[i].script == WritingSystem::ChineseSimplified ||
			blocks[i].script == WritingSystem::ChineseTraditional)
		{
			styleManager->SetPreferredFont(55, FALSE, m_cached_normal[i], blocks[i].script);
			styleManager->SetPreferredFont(55, TRUE, m_cached_mono[i], blocks[i].script);

			styleManager->SetPreferredFont(59, FALSE, m_cached_normal[i], blocks[i].script);
			styleManager->SetPreferredFont(59, TRUE, m_cached_mono[i], blocks[i].script);
		}
		else if (blocks[i].block_nr == 61) // Kanji
		{
			styleManager->SetPreferredFont(54, FALSE, m_cached_normal[i], blocks[i].script);
			styleManager->SetPreferredFont(54, TRUE, m_cached_mono[i], blocks[i].script);

			styleManager->SetPreferredFont(55, FALSE, m_cached_normal[i], blocks[i].script);
			styleManager->SetPreferredFont(55, TRUE, m_cached_mono[i], blocks[i].script);

			styleManager->SetPreferredFont(59, FALSE, m_cached_normal[i], blocks[i].script);
			styleManager->SetPreferredFont(59, TRUE, m_cached_mono[i], blocks[i].script);

			styleManager->SetPreferredFont(61, FALSE, m_cached_normal[i], blocks[i].script);
			styleManager->SetPreferredFont(61, TRUE, m_cached_mono[i], blocks[i].script);
         }
		else
		{
			styleManager->SetPreferredFont(blocks[i].block_nr, FALSE, m_cached_normal[i], blocks[i].script);
			styleManager->SetPreferredFont(blocks[i].block_nr, TRUE, m_cached_mono[i], blocks[i].script);
		}

		g_pcfontscolors->WritePreferredFontL(blocks[i].block_nr, FALSE, m_cached_normal[i], OpFontInfo::CodePageFromScript(blocks[i].script));
		g_pcfontscolors->WritePreferredFontL(blocks[i].block_nr, TRUE, m_cached_mono[i], OpFontInfo::CodePageFromScript(blocks[i].script));
	}

	return 0;
}


void InternationalFontsDialog::OnNormalFontChange()
{
	OpTreeView* treeview = (OpTreeView*) GetWidgetByName("Normal_font_treeview");
	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Encoding_dropdown");
	OpLabel* label = (OpLabel*)GetWidgetByName("Normal_font_preview");

	int selected = treeview->GetSelectedItemModelPos() - 1;
	int block = dropdown->GetSelectedItem();
	OpFontInfo* fontinfo = NULL;

	FontAtt font;
	g_pcfontscolors->GetFont(OP_SYSTEM_FONT_DOCUMENT_NORMAL, font);

	if (selected == -1) // Default choice
	{
		fontinfo = (OpFontInfo*)GetAutomaticFont(blocks[block].block_nr, blocks[block].script, FALSE);
	}
	else
	{
		fontinfo = (OpFontInfo*)styleManager->GetFontInfo(styleManager->GetFontNumber(m_normal_list[selected]));
	}

	label->SetText(blocks[block].example_string);
	label->SetFontInfo(fontinfo, 30, font.GetItalic(), font.GetWeight(), JUSTIFY_CENTER);

	m_cached_normal[block] = (selected == -1 ? NULL : m_normal_list[selected]);
}


void InternationalFontsDialog::OnMonoTypeFontChange()
{
	OpTreeView* treeview = (OpTreeView*) GetWidgetByName("Monotype_font_treeview");
	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Encoding_dropdown");
	OpLabel* label = (OpLabel*)GetWidgetByName("Monotype_font_preview");

	int selected = treeview->GetSelectedItemModelPos() - 1;
	int block = dropdown->GetSelectedItem();
	OpFontInfo* fontinfo = NULL;

	FontAtt font;
	g_pcfontscolors->GetFont(OP_SYSTEM_FONT_DOCUMENT_PRE, font);

	if (selected == -1) // Default choice
	{
		fontinfo = (OpFontInfo*)GetAutomaticFont(blocks[block].block_nr, blocks[block].script, TRUE);
	}
	else
	{
		fontinfo = (OpFontInfo*)styleManager->GetFontInfo(styleManager->GetFontNumber(m_mono_list[selected]));
	}

	label->SetText(blocks[block].example_string);
	label->SetFontInfo(fontinfo, 30, font.GetItalic(), font.GetWeight(), JUSTIFY_CENTER);

	m_cached_mono[block] = (selected == -1 ? NULL : m_mono_list[selected]);
}


void InternationalFontsDialog::OnEncodingChange()
{
	OpTreeView* normal_treeview = (OpTreeView*) GetWidgetByName("Normal_font_treeview");
	OpTreeView* monotype_treeview = (OpTreeView*) GetWidgetByName("Monotype_font_treeview");
	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Encoding_dropdown");

  	int selected = dropdown->GetSelectedItem();

	if (selected >= 0 && selected < num_blocks)
	{
		// Clear everything both listboxes
		m_normal_count = m_normal_model.GetItemCount();
		if (m_normal_count > 0)
		{
			while (m_normal_count)
			{
				m_normal_model.Delete(0);
				--m_normal_count;
			}
		}

		m_mono_count = m_monotype_model.GetItemCount();
		if (m_mono_count > 0)
		{
			while (m_mono_count)
			{
				m_monotype_model.Delete(0);
				--m_mono_count;
			}
		}

		m_normal_count = m_mono_count = 0;

		// Get all fonts supporting the given block
		UINT32 i;
		UINT32 num_fonts;
		if (styleManager && styleManager->GetFontManager())
			num_fonts = styleManager->GetFontManager()->CountFonts();
		else
			num_fonts = 0;

		for (i=0; i < num_fonts; ++i)
		{
			OpFontInfo* fontinfo = styleManager->GetFontInfo(i);
			if (fontinfo && IsValidFont(*fontinfo, blocks[selected].block_nr, blocks[selected].script))
			{
				BOOL inlist = FALSE;
				if (fontinfo->Monospace())
				{
					for(UINT32 j=0; j < m_mono_count; j++)
					{
						if (fontinfo->GetFace() && m_mono_list[j] && uni_strcmp(fontinfo->GetFace(), m_mono_list[j]) == 0)
						{
							inlist = TRUE;
							break;
						}
					}

					if (!inlist)
					{
						m_mono_list[m_mono_count] = (uni_char*)fontinfo->GetFace();
						++m_mono_count;
					}
				}

				// Add all fonts to normal font list, even monospace fonts.
				inlist = FALSE;
				for (UINT32 j=0; j < m_normal_count; ++j)
				{
					if (fontinfo->GetFace() && m_normal_list[j] && uni_strcmp(fontinfo->GetFace(), m_normal_list[j]) == 0)
					{
						inlist = TRUE;
						break;
					}
				}

				if (!inlist)
				{
					m_normal_list[m_normal_count] = (uni_char*)fontinfo->GetFace();
					++m_normal_count;
				}
			}
		}

		if (m_normal_count > 0)
		{
			// Enable list
			normal_treeview->SetEnabled(TRUE);

			// Add default choice
#ifndef PLATFORM_FONTSWITCHING
			const OpFontInfo* font = GetAutomaticFont(blocks[selected].block_nr, blocks[selected].script, FALSE);
			if (font)
				uni_sprintf(m_automatic_normal_str, UNI_L("%s (%s)"), m_default_choice_str.CStr(), font->GetFace() ? font->GetFace() : UNI_L(""));
			else
#endif // PLATFORM_FONTSWITCHING
			{
				uni_sprintf(m_automatic_normal_str, UNI_L("%s"), m_default_choice_str.CStr());
			}

			m_normal_model.AddItem(m_automatic_normal_str);

			// Sort normal fonts
			qsort(m_normal_list, m_normal_count, sizeof(uni_char*), strcompare);
			// Add normal fonts to list and select the preferred font
			BOOL normal_selected = FALSE;
			for(i=0; i < m_normal_count; ++i)
			{
				m_normal_model.AddItem(m_normal_list[i]);
				if (!normal_selected && m_cached_normal[selected] && m_normal_list[i] && uni_strcmp(m_cached_normal[selected], m_normal_list[i]) == 0)
				{
					normal_treeview->SetSelectedItem(i+1);
					normal_selected = TRUE;
				}
			}

			// Select default font if no font is preferred
			if (!normal_selected)
				normal_treeview->SetSelectedItem(0);
		}
		else
		{
			// No fonts available

			normal_treeview->SetEnabled(FALSE);
			m_normal_model.AddItem(m_no_fonts_str.CStr());
			normal_treeview->SetSelectedItem(0);
		}

		if (m_mono_count > 0)
		{
			// Enable list
			monotype_treeview->SetEnabled(TRUE);

			// Add default choice
#ifndef PLATFORM_FONTSWITCHING
			const OpFontInfo* font = GetAutomaticFont(blocks[selected].block_nr, blocks[selected].script, TRUE);
			if (font)
				uni_sprintf(m_automatic_mono_str, UNI_L("%s (%s)"), m_default_choice_str.CStr(), font->GetFace() ? font->GetFace() : UNI_L(""));
			else
#endif // PLATFORM_FONTSWITCHING
			{
				uni_sprintf(m_automatic_mono_str, UNI_L("%s"), m_default_choice_str.CStr());
			}

			m_monotype_model.AddItem(m_automatic_mono_str);

			// Sort monospace fonts
			qsort(m_mono_list, m_mono_count, sizeof(uni_char*), strcompare);
			// Add monospace fonts to list and select the preferred font
			BOOL mono_selected = FALSE;
			for(i=0; i < m_mono_count; ++i)
			{
				m_monotype_model.AddItem(m_mono_list[i]);
				if (!mono_selected && m_cached_mono[selected] && m_mono_list[i] && uni_strcmp(m_cached_mono[selected], m_mono_list[i]) == 0)
				{
					monotype_treeview->SetSelectedItem(i+1);
					mono_selected = TRUE;
				}
			}

			// Select default font if no font is preferred
			if (!mono_selected)
				monotype_treeview->SetSelectedItem(0);
		}
		else
		{
			// No fonts available

			monotype_treeview->SetEnabled(FALSE);
			m_monotype_model.AddItem(m_no_fonts_str.CStr());
			monotype_treeview->SetSelectedItem(0);
		}

		OnNormalFontChange();
		OnMonoTypeFontChange();
	}
}


void InternationalFontsDialog::OnChange(OpWidget* widget, BOOL changed_by_mouse)
{
	if (widget->IsNamed("Encoding_dropdown"))
	{
		OnEncodingChange();
    }
	else if (widget->IsNamed("Normal_font_treeview"))
	{
		OnNormalFontChange();
    }
	else if (widget->IsNamed("Monotype_font_treeview"))
	{
		OnMonoTypeFontChange();
    }
	else
	{
		Dialog::OnChange(widget, changed_by_mouse);
	}
}

BOOL InternationalFontsDialog::IsValidFont(OpFontInfo &fontinfo, UINT8 block_nr, WritingSystem::Script script)
{
	if (fontinfo.Vertical())
		return FALSE;

	return script != WritingSystem::Unknown ? fontinfo.HasScript(script) || fontinfo.HasBlock(block_nr) : fontinfo.HasBlock(block_nr);
}

const OpFontInfo* InternationalFontsDialog::GetAutomaticFont(UINT8 block_nr, WritingSystem::Script script, BOOL monospace)
{
    OpFontInfo specified_font;

	Style* style = styleManager->GetStyle(monospace ? HE_PRE : HE_DOC_ROOT);
	int font_nr = style->GetPresentationAttr().GetPresentationFont(script).FontNumber;
	styleManager->GetFontManager()->GetFontInfo(font_nr, &specified_font);

#ifndef PLATFORM_FONTSWITCHING
	if (!IsValidFont(specified_font, block_nr, script))
    {
        const OpFontInfo* font = styleManager->GetRecommendedAlternativeFont(&specified_font, block_nr, script, 0, monospace);
        return font;
    }
    else
#endif // PLATFORM_FONTSWITCHING
    {
        return styleManager->GetFontInfo(specified_font.GetFontNumber());
    }
}
