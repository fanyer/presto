/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/widgets/OpWidget.h"
#include "modules/doc/frm_doc.h"
#include "modules/display/fontcache.h"
#include "modules/display/vis_dev.h"
#include "modules/display/FontAtt.h"
#include "modules/display/fontdb.h"
#include "modules/display/styl_man.h"
#include "modules/layout/cssprops.h"
#include "modules/style/css.h"
#include "modules/forms/piforms.h"
#include "modules/widgets/optextfragment.h"
#include "modules/widgets/OpWidgetStringDrawer.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/unicode/unicode_stringiterator.h"
#include "modules/display/writingsystemheuristic.h"

#ifdef WIDGETS_ADDRESS_SERVER_HIGHLIGHTING
# include "modules/url/url_man.h"
# ifdef PUBLIC_DOMAIN_LIST
#  include "modules/security_manager/include/security_manager.h"
# endif // PUBLIC_DOMAIN_LIST
#endif // WIDGETS_ADDRESS_SERVER_HIGHLIGHTING

#ifdef GADGET_SUPPORT
#include "modules/dochand/win.h"
#endif // GADGET_SUPPORT

#ifdef SUPPORT_IME_PASSWORD_INPUT
# include "modules/prefs/prefsmanager/collections/pc_display.h"
#endif // SUPPORT_IME_PASSWORD_INPUT

int GetTabWidth(int xpos, VisualDevice* vis_dev);


#ifdef _GLYPHTESTING_SUPPORT_
/**
   GetTxtExtent does not font switch for these glyphs, so special care
   has to be taken when measuring. NoFontswitchNoncollapsingWhitespace
   will estimate width based on em box for glyphs not in font. if
   _GLYPHTESTING_SUPPORT_ is not available
   NoFontswitchNoncollapsingWhitespace will always estimate, which is
   probably not desirable in this context.
 */
# define NON_COLLAPSING_SPACE_GLYPH(uc) (styleManager->NoFontswitchNoncollapsingWhitespace(uc) || (uc) == 0x20)
#endif // _GLYPHTESTING_SUPPORT_

/**
	Workaround for windows returning strange widths on character 10 and 13 but drawing them with width 0.
*/

INT32 GetStringWidth(const uni_char* str, INT32 len, VisualDevice* vis_dev, int text_format)
{
	int i = 0;
	int width = 0;
	int start = 0; // Start of the text chunk to be measured.

	while (i < len)
	{
		for(; i < len; i++)
		{
			if (str[i] == 10 || str[i] == 13
	#ifdef _GLYPHTESTING_SUPPORT_
				|| NON_COLLAPSING_SPACE_GLYPH(str[i])
	#endif // _GLYPHTESTING_SUPPORT_
				)
			{
				width += vis_dev->MeasureNonCollapsingSpaceWord(str+i, 1, vis_dev->GetCharSpacingExtra());
				break;
			}
		}

		if (i - start > 0)
			width += vis_dev->GetTxtExtentEx(str+start, i-start, text_format);

		start = ++i;
	}
	return width;
}

/**
   ~same as GetTxtExtentToEx, but special care is taken when measuring
   fragments that end with space characters, since these won't trigger
   font switching even if they're not in font.

   @param frag the text fragment
   @param word the string to measure - relative to frag, not whole string
   @param from offset from start of fragment to start of text to
   measure - should not be bigger than to
   @param to offset from start of fragment to end of text to measure -
   should not be bigger than fragment length
   @param vis_dev the visual device used to measure text - correct
   font, size etc should be set before calling
   @param text_format see VisualDevice::GetTxtExtentEx()

   @return the extents of the measured string
 */
unsigned int FragmentExtentFromTo(OP_TEXT_FRAGMENT* frag,
                                  const uni_char* word,
                                  unsigned int from, unsigned int to,
                                  VisualDevice* vis_dev, int text_format)
{
	unsigned int length = frag->wi.GetLength();

	OP_ASSERT(from <= to);
	OP_ASSERT(to <= frag->wi.GetLength());

	if (from)
	{
		word += from;
		length -= from;
		to -= from;
	}

	unsigned int extent = 0;
#ifdef _GLYPHTESTING_SUPPORT_
	if (frag->wi.HasTrailingWhitespace())
	{
		unsigned int new_to = to;
		while (new_to && NON_COLLAPSING_SPACE_GLYPH(word[new_to-1]))
			-- new_to;
		extent = vis_dev->MeasureNonCollapsingSpaceWord(word+new_to, to-new_to, vis_dev->GetCharSpacingExtra());
		to = new_to;
	}
#endif // _GLYPHTESTING_SUPPORT_

	extent += vis_dev->GetTxtExtentToEx(word, length, to, text_format);
	return extent;
}

// == OpWidgetString ==========================================================

OpWidgetString::OpWidgetString()
	: widget(NULL)
	, str(g_widget_globals->dummy_str)
	, str_original(NULL)
#if defined(WIDGETS_IME_SUPPORT) && defined(SUPPORT_IME_PASSWORD_INPUT)
    , uncoveredChar(-1)
#endif
	, m_packed_init(0)
	, width(0)
	, height(0)
	, max_length(65530)
	, m_script(WritingSystem::Unknown)
#ifdef WIDGETS_ADDRESS_SERVER_HIGHLIGHTING
	, m_highlight_type(None)
#endif // WIDGETS_ADDRESS_SERVER_HIGHLIGHTING
#ifdef WIDGETS_HEURISTIC_LANG_DETECTION
	, m_heuristic_script(WritingSystem::Unknown)
#endif // WIDGETS_HEURISTIC_LANG_DETECTION
	, m_text_decoration(0)
#ifdef WIDGETS_IME_SUPPORT
	, m_ime_info(NULL)
#endif // WIDGETS_IME_SUPPORT
	, sel_start(-1)
	, sel_stop(-1)
#ifdef WIDGETS_ADDRESS_SERVER_HIGHLIGHTING
	, m_highlight_ofs(-1)
	, m_highlight_len(-1)
#endif // WIDGETS_ADDRESS_SERVER_HIGHLIGHTING
{
	m_packed.is_overstrike = FALSE;
	m_packed.password_mode = FALSE;
	m_packed.convert_ampersand = FALSE;
	m_packed.draw_ampersand_underline = FALSE;
	m_packed.need_update = FALSE;
}

OpWidgetString::~OpWidgetString()
{
	if (str != g_widget_globals->dummy_str && str != str_original)
	{
		uni_char *nonconst_str = (uni_char *)str;
		OP_DELETEA(nonconst_str);
	}
	op_free(str_original);
}

OP_STATUS OpWidgetString::UpdateFragments()
{
///// BEGIN mstensho fix
// Remove this when things work properly. I got a crash here because I had defined an empty
// font string to be used for combo-boxes (button text). I don't think this test should be
// in this code; this code should be able to safely assume that everything is all right. (mstensho)
	if (!widget->font_info.font_info)
	{
		// Shouldn't happen?
		OP_ASSERT(widget->font_info.font_info);
		return OpStatus::ERR;
	}
///// END mstensho fix

	FramesDocument* frm_doc = NULL;
	int orgiginal_fontnr = widget->font_info.font_info->GetFontNumber();
	WritingSystem::Script script = m_script;
#ifdef FONTSWITCHING
	if (script == WritingSystem::Unknown)
		// script takes preference over document codepage
		if (widget->GetScript() != WritingSystem::Unknown)
			script = widget->GetScript();
	if (script == WritingSystem::Unknown)
		if (widget->vis_dev->GetDocumentManager())
		{
			frm_doc = widget->vis_dev->GetDocumentManager()->GetCurrentDoc();
			if (frm_doc && frm_doc->GetHLDocProfile())
				script = frm_doc->GetHLDocProfile()->GetPreferredScript();
		}
		else
		{
#ifdef WIDGETS_HEURISTIC_LANG_DETECTION
			if (widget->IsMultilingual())
			{
				if (m_packed.need_update_script)
				{
					m_packed.need_update_script = FALSE;
					WritingSystemHeuristic detector;
					detector.Analyze(Get());
					m_heuristic_script = detector.GuessScript();
				}
				script = m_heuristic_script;
			}
			else
#endif // WIDGETS_HEURISTIC_LANG_DETECTION
			{
				const OpStringC lngcode = g_languageManager->GetLanguage();
				script = WritingSystem::FromLanguageCode(lngcode.CStr());
			}
		}
#endif

#if !defined(WIDGETS_IME_SUPPORT) || !defined(SUPPORT_IME_PASSWORD_INPUT)
	if (m_packed.password_mode)
	{
		fragments.Clear();
		return OpStatus::OK;
	}
#endif

	return fragments.Update(str, uni_strlen(str), widget->GetRTL() && !GetForceLTR(), FALSE, TRUE, FALSE, frm_doc, orgiginal_fontnr, script, TRUE, NULL, GetConvertAmpersand());
}

void OpWidgetString::Reset(OpWidget* widget)
{
	this->widget = widget;
	NeedUpdate();
}

OP_STATUS OpWidgetString::Set(const uni_char* instr, OpWidget* widget)
{
	return Set(instr, instr ? uni_strlen(instr) : 0, widget);
}

OP_STATUS OpWidgetString::Set(const uni_char* instr, int len, OpWidget* widget)
{
	NeedUpdate();

#ifdef WIDGETS_HEURISTIC_LANG_DETECTION
	m_packed.need_update_script = TRUE;
#endif // WIDGETS_HEURISTIC_LANG_DETECTION

	if (instr == NULL)
		instr = UNI_L("");

	if (str != g_widget_globals->dummy_str && str != str_original)
	{
		uni_char *nonconst_str = (uni_char *)str;
		OP_DELETEA(nonconst_str);
	}

	op_free(str_original);
	str_original = NULL;

	width = 0;
	height = 0;

	this->widget = widget;

	// Create the new string

	//OP_ASSERT(len <= max_length); // More characters than max_len !!
	// The spec says that maxlen is the maxlength the *user* can type in. Some pages expect that scripts can set a longer string.
	//len = max_length < len ? max_length : len; // Truncate len to max_length

	uni_char *new_str = OP_NEWA(uni_char, len + 1);

	if (new_str == NULL)
	{
		str = g_widget_globals->dummy_str;
		return OpStatus::ERR_NO_MEMORY;
	}

	op_memcpy(new_str, instr, len * sizeof(uni_char));
	new_str[len] = '\0';

	if (widget)
	{
		short tt = widget->text_transform;
		if (tt == CSS_TEXT_TRANSFORM_uppercase || tt == CSS_TEXT_TRANSFORM_lowercase)
		{
			str_original = uni_strdup(new_str);
			if (str_original)
			{
				if (tt == CSS_TEXT_TRANSFORM_uppercase)
				{
					uni_strupr(new_str);
				}
				else
				{
					uni_strlwr(new_str);
				}
			}
		}
	}

	str = new_str;

#if defined(WIDGETS_IME_SUPPORT) && defined(SUPPORT_IME_PASSWORD_INPUT)
	if (m_packed.password_mode)
	{
		str_original = uni_strdup(str);
		if (str_original != 0)
			UpdatePasswordText();
	}
#endif


	return OpStatus::OK;
}

void OpWidgetString::UpdateVisualDevice(VisualDevice *vis_dev)
{
	if (!vis_dev)
		vis_dev = widget ? widget->vis_dev : NULL;

	if (!vis_dev)
		return;

	WIDGET_FONT_INFO* fi = &widget->font_info;

	// Set all the font attributes, since we may not be in a format or paint loop when this is called
	vis_dev->SetFontSize(fi->size);
	vis_dev->SetFontWeight(m_packed.force_bold ? 7 : fi->weight);
	vis_dev->SetFontStyle((fi->italic || m_packed.force_italic) ? FONT_STYLE_ITALIC : FONT_STYLE_NORMAL);
	vis_dev->SetCharSpacingExtra(fi->char_spacing_extra);
}

int OpWidgetString::GetFontNumber()
{
	OP_ASSERT(widget);
	if (!widget)
		return 0;
	int orgiginal_fontnr = widget->font_info.font_info->GetFontNumber();
	if (m_packed.password_mode)
	{
		WritingSystem::Script script = m_script;
#ifdef FONTSWITCHING
		if (script == WritingSystem::Unknown)
			// script takes preference over document codepage
			if (widget->GetScript() != WritingSystem::Unknown)
				script = widget->GetScript();
		if (script == WritingSystem::Unknown)
			if (widget->vis_dev->GetDocumentManager())
			{
				FramesDocument* frm_doc = widget->vis_dev->GetDocumentManager()->GetCurrentDoc();
				if (frm_doc && frm_doc->GetHLDocProfile())
					script = frm_doc->GetHLDocProfile()->GetPreferredScript();
			}
#endif
		return styleManager->GetBestFont(g_widget_globals->passwd_char, 1, orgiginal_fontnr, script);
	}
	return orgiginal_fontnr;
}

/** Determine if accurate font sizes (from current zoom level) should be used when layouting and
 *  rendering this widget.
 */
static BOOL UseAccurateFontSize(OpWidget* widget)
{
	// Currently we only do this for edit fields. This should perhaps be considered for multiline
	// edit boxes as well but that is not handled by OpWidgetString so adding it here would not
	// make any difference.
	switch (widget->GetType())
	{
	case OpTypedObject::WIDGET_TYPE_EDIT:
		return TRUE;
	}

	return FALSE;
}

BOOL OpWidgetString::UseAccurateFontSize()
{
	return ::UseAccurateFontSize(widget);
}

OP_STATUS OpWidgetString::Update(VisualDevice* vis_dev/* = 0*/)
{
	if (!vis_dev && widget)
		vis_dev = widget->vis_dev;

	if (widget)
		widget->CheckScale();

	if (!m_packed.need_update || !widget || !vis_dev)
	{
		return OpStatus::OK;
	}

	width = 0;
	height = 0;

#if defined(WIDGETS_IME_SUPPORT) && defined(SUPPORT_IME_PASSWORD_INPUT)
	if (m_packed.password_mode)
		UpdatePasswordText();
#endif

	UpdateVisualDevice(vis_dev);

	if (UseAccurateFontSize())
		vis_dev->BeginAccurateFontSize();

	OP_STATUS status = UpdateFragments();
	if ( OpStatus::IsError(status) )
	{
		if (UseAccurateFontSize())
			vis_dev->EndAccurateFontSize();
		return status;
	}

	int orgiginal_fontnr = GetFontNumber();
	vis_dev->SetFont(orgiginal_fontnr);

	// Update width and height
#if !defined(WIDGETS_IME_SUPPORT) || !defined(SUPPORT_IME_PASSWORD_INPUT)
	if (m_packed.password_mode)
	{
		UINT32 len = uni_strlen(str);

		width = vis_dev->GetTxtExtent(g_widget_globals->passwd_char, 1) * len;
		height = MAX(vis_dev->GetFontHeight(), vis_dev->GetFontAscent() + vis_dev->GetFontDescent());
	}
	else
#endif
	{
		UINT32 i;
		INT32 base_ascent = vis_dev->GetFontAscent();
		for(i = 0; i < fragments.GetNumFragments(); i++)
		{
			OP_TEXT_FRAGMENT* frag = fragments.Get(i);
			if (!GetConvertAmpersand() || frag->wi.GetLength() != 1 || str[frag->start] != '&')
			{
				vis_dev->SetFont(frag->wi.GetFontNumber());

				int fontheight = base_ascent - vis_dev->GetFontAscent() + vis_dev->GetFontHeight();
				if (fontheight > height)
					height = fontheight;

				if (frag->wi.IsTabCharacter())
				{
					frag->wi.SetWidth(GetTabWidth(width, vis_dev));
					frag->wi.SetLength(1);
				}
				else if (str[frag->start] == 173) // Soft hyphen
					frag->wi.SetWidth(0);
				else
				{
					int text_format = 0;
					if (frag->packed.bidi & BIDI_FRAGMENT_MIRRORED)
						text_format |= TEXT_FORMAT_REVERSE_WORD;
					frag->wi.SetWidth(MIN(GetStringWidth(&str[frag->start], frag->wi.GetLength(), vis_dev, text_format), WORD_INFO_WIDTH_MAX));
				}

				width += frag->wi.GetWidth();
			}
		}
	}

	if (height == 0)
		height = MAX(vis_dev->GetFontHeight(), vis_dev->GetFontAscent() + vis_dev->GetFontDescent());

	m_packed.need_update = FALSE;
	vis_dev->SetFont(orgiginal_fontnr);

	if (UseAccurateFontSize())
		vis_dev->EndAccurateFontSize();

#ifdef WIDGETS_ADDRESS_SERVER_HIGHLIGHTING
	OpStatus::Ignore(UpdateHighlightOffset());
#endif // WIDGETS_ADDRESS_SERVER_HIGHLIGHTING

	return OpStatus::OK;
}

const uni_char* OpWidgetString::Get() const
{
	return str_original ? str_original : str;
}

void OpWidgetString::SelectNone()
{
	sel_start = -1;
	sel_stop = -1;
}

#ifdef WIDGETS_IME_SUPPORT
void OpWidgetString::SetIMNone()
{
	m_ime_info = NULL;

#if defined(SUPPORT_IME_PASSWORD_INPUT)
	if (m_packed.password_mode)
		NeedUpdate();
#endif
}

void OpWidgetString::SetIMPos(INT32 pos, INT32 length)
{
	m_ime_info = &g_widget_globals->ime_info;
	m_ime_info->start = pos;
	m_ime_info->stop = pos + length;

#if defined(SUPPORT_IME_PASSWORD_INPUT)
	if (m_packed.password_mode)
		NeedUpdate();
#endif
}

void OpWidgetString::SetIMCandidatePos(INT32 pos, INT32 length)
{
	m_ime_info = &g_widget_globals->ime_info;
	m_ime_info->can_start = pos;
	m_ime_info->can_stop = pos + length;

#if defined(SUPPORT_IME_PASSWORD_INPUT)
	if (m_packed.password_mode)
		NeedUpdate();
#endif
}

void OpWidgetString::SetIMString(OpInputMethodString* string)
{
	m_ime_info = &g_widget_globals->ime_info;
	m_ime_info->string = string;
}

#endif // WIDGETS_IME_SUPPORT

void OpWidgetString::SelectFromCaret(INT32 caret_pos, INT32 delta)
{
	OpStatus::Ignore(Update());//FIXME:OOM
	if (sel_start == sel_stop)
		SelectNone();
	if (sel_start == caret_pos)
		sel_start = caret_pos + delta;
	else if (sel_stop == caret_pos)
		sel_stop = caret_pos + delta;
	if (sel_start == -1)
	{
		if(delta < 0)
		{
			sel_start = caret_pos + delta;
			sel_stop = caret_pos;
		}
		else
		{
			sel_start = caret_pos;
			sel_stop = caret_pos + delta;
		}
	}
}

void OpWidgetString::Select(INT32 start, INT32 stop)
{
	INT32 str_len = uni_strlen(str);
	// Put selection start between 0 and str_len
	sel_start = MAX(start, 0);
	sel_start = MIN(sel_start, str_len);
	// Put selection end between selection start and str_len
	sel_stop = MIN(stop, str_len);
	sel_stop = MAX(sel_stop, sel_start);
	if (sel_start == sel_stop)
		SelectNone();
}

INT32 OpWidgetString::GetWidth()
{
	OpStatus::Ignore(Update());//FIXME:OOM
	if (widget)
		return MAX(0, width + widget->text_indent);
	return width;
}

INT32 OpWidgetString::GetHeight()
{
	OpStatus::Ignore(Update());//FIXME:OOM
	return height;
}

void GetStartStopSelection(int *sel_start, int *sel_stop, const WordInfo& wi)
{
	int wlen = wi.GetLength();
	if (wi.IsTabCharacter())
		wlen = 1;
	if (*sel_start < 0)
		*sel_start = 0;
	if (*sel_start > wlen)
		*sel_start = wlen;
	if (*sel_stop < 0)
		*sel_stop = 0;
	if (*sel_stop > wlen)
		*sel_stop = wlen;
}

int OffsetToPosInFragment(int ofs, OP_TEXT_FRAGMENT* frag, const uni_char* word, VisualDevice* vis_dev, BOOL use_accurate_font_size)
{
	int x_pos;
	if (frag->wi.IsTabCharacter())
	{
		x_pos = (ofs == frag->start ? 0 : frag->wi.GetWidth());
	}
	else if (*word == 173 && frag->wi.GetWidth() == 0) // Nonvisible Soft hyphen
		x_pos = 0;
	else
	{
		if (use_accurate_font_size)
			vis_dev->BeginAccurateFontSize();

		vis_dev->SetFont(frag->wi.GetFontNumber());

		int text_format = 0;
		if (frag->packed.bidi & BIDI_FRAGMENT_MIRRORED)
			text_format |= TEXT_FORMAT_REVERSE_WORD;

		x_pos = FragmentExtentFromTo(frag, word, 0, ofs - frag->start, vis_dev, text_format);

		if (use_accurate_font_size)
			vis_dev->EndAccurateFontSize();
	}
#ifdef SUPPORT_TEXT_DIRECTION
	if (frag->packed.bidi & BIDI_FRAGMENT_MIRRORED)
		x_pos = frag->wi.GetWidth() - x_pos;
#endif // SUPPORT_TEXT_DIRECTION
	return x_pos;
}

int PosToOffsetInFragment(int x_pos, OP_TEXT_FRAGMENT* frag, const uni_char* word, VisualDevice* vis_dev, BOOL use_accurate_font_size)
{
#ifdef SUPPORT_TEXT_DIRECTION
	if (frag->packed.bidi & BIDI_FRAGMENT_MIRRORED)
		x_pos = frag->wi.GetWidth() - x_pos;
#endif // SUPPORT_TEXT_DIRECTION

	vis_dev->SetFont(frag->wi.GetFontNumber());

	if (frag->wi.IsTabCharacter())
	{
		return frag->start + (x_pos <= (int)frag->wi.GetWidth() / 2 ? 0 : 1);
	}
	else if (*word == 173 && frag->wi.GetWidth() == 0) // Nonvisible Soft hyphen
		return 0;
	else
	{
		if (use_accurate_font_size)
			vis_dev->BeginAccurateFontSize();

		int text_format = 0;
		if (frag->packed.bidi & BIDI_FRAGMENT_MIRRORED)
			text_format |= TEXT_FORMAT_REVERSE_WORD;

		UnicodeStringIterator iter(word, 0, frag->wi.GetLength());

		while(!iter.IsAtEnd())
		{
			unsigned int i = iter.Index();

#ifdef SUPPORT_UNICODE_NORMALIZE
			iter.NextBaseCharacter();
#else
			iter.Next();
#endif

			// How many UTF-16 code units is the character composed of?
			unsigned int len = iter.Index() - i;

			INT32 extent_before = FragmentExtentFromTo(frag, word, 0, i, vis_dev, text_format);
			INT32 char_width =
#ifdef _GLYPHTESTING_SUPPORT_
				NON_COLLAPSING_SPACE_GLYPH(word[i]) ?
				vis_dev->MeasureNonCollapsingSpaceWord(word+i, len, 0) :
#endif // _GLYPHTESTING_SUPPORT_
				vis_dev->GetTxtExtentEx(&word[i], len, text_format);

			if (x_pos < extent_before + char_width / 2)
			{
				if (use_accurate_font_size)
					vis_dev->EndAccurateFontSize();
				return frag->start + i;
			}
		}

		if (use_accurate_font_size)
			vis_dev->EndAccurateFontSize();
	}
	return frag->start + frag->wi.GetLength();
}

#ifdef INTERNAL_SPELLCHECK_SUPPORT
/** Draws the wavy red line that indicates an misspelling under a word */
void DrawMisspelling(VisualDevice* vis_dev, int x, int y, const uni_char* word, OP_TEXT_FRAGMENT* frag, int height, BOOL rtl)
{
	int line_len = frag->wi.GetWidth();
	const int last_char_idx = frag->wi.GetLength()-1;
	int i;
	for(i = last_char_idx; i && uni_isspace(word[i]); i--)
		;
	if(i < last_char_idx)
	{
		int space_len = FragmentExtentFromTo(frag, word, i+1, frag->wi.GetLength(), vis_dev, 0);
		line_len -= space_len;
		if(rtl)
			x += space_len;
	}

	int wave_height = height/9;
	int wave_width = wave_height;
	int line_base = y + height - wave_height - 1;
	vis_dev->DrawMisspelling(OpPoint(x,line_base), line_len, wave_height, OP_RGB(0xFF,0,0), wave_width);
}
#endif // INTERNAL_SPELLCHECK_SUPPORT

#ifdef SUPPORT_TEXT_DIRECTION
void DrawRTLString(VisualDevice* vis_dev, INT32 x, INT32 y, const uni_char* str, INT32 len,
				   INT32 sel_start, INT32 sel_stop, UINT32 color, UINT32 fcol_sel, UINT32 bcol_sel, INT32 height, OP_TEXT_FRAGMENT* frag, short word_width)
{
/*#ifdef _DEBUG
	color = OP_RGB(255, 0, 0);
#endif*/

	int text_format = TEXT_FORMAT_REVERSE_WORD;
	vis_dev->SetColor(color);
	vis_dev->TxtOutEx(x, y, str, len, text_format, word_width);

	if (sel_start != sel_stop) // We have a selection
	{
		// Draw the *full* string within a clipped rectangle which is set to the selected part of the string.
		// (To not break that characters glues into eachother).

		int startx = FragmentExtentFromTo(frag, str, sel_start, len, vis_dev, text_format);
		int stopx =  FragmentExtentFromTo(frag, str, sel_stop,  len, vis_dev, text_format);
		if (startx > stopx)
		{
			int tmp = startx;
			startx = stopx;
			stopx = tmp;
		}

		OpRect r(startx + x, y, stopx - startx, height);
		vis_dev->SetColor(bcol_sel);
		vis_dev->FillRect(r);
		vis_dev->SetColor(fcol_sel);
		vis_dev->BeginClipping(r);
		vis_dev->TxtOutEx(x, y, str, len, text_format, word_width);
		vis_dev->EndClipping();
	}
#ifdef INTERNAL_SPELLCHECK_SUPPORT
	if(frag->wi.IsMisspelling())
		DrawMisspelling(vis_dev, x, y, str, frag, height, TRUE);
#endif // INTERNAL_SPELLCHECK_SUPPORT
}
#endif // SUPPORT_TEXT_DIRECTION

#ifdef WIDGETS_ELLIPSIS_SUPPORT
BOOL OpWidgetString::DrawFragmentSpecial(VisualDevice* vis_dev, int &x_pos, int y, OP_TEXT_FRAGMENT* frag, ELLIPSIS_POSITION ellipsis_position, int &used_space, const OpRect& rect)
{
	const uni_char* frag_str = str + frag->start;

	int text_format = 0;
	if (frag->packed.bidi & BIDI_FRAGMENT_MIRRORED)
		text_format |= TEXT_FORMAT_REVERSE_WORD;

	if (GetConvertAmpersand() && frag->wi.GetLength() == 1 && *frag_str == '&')
	{
		if (GetDrawAmpersandUnderline())
		{
			int underline_char_x = x_pos;
			int underline_char_w = vis_dev->GetTxtExtentEx(frag_str + 1, 1, text_format);
			if (text_format & TEXT_FORMAT_REVERSE_WORD)
				underline_char_x -= underline_char_w;
			vis_dev->DrawLine(OpPoint(underline_char_x, y + height - 1), underline_char_w, TRUE, 1);
		}
		return TRUE;
	}
	else if (width > rect.width)
	{
		// Entire string is wider than the rect and needs ellipsis.

		const BOOL reverse_fragments = ShouldDrawFragmentsReversed(rect, ellipsis_position);

		int e_pos, ellipsis_width = vis_dev->GetTxtExtent(UNI_L("..."), 3);

		if (ellipsis_position == ELLIPSIS_END)
			e_pos = rect.x + rect.width - ellipsis_width;
		else
			e_pos = rect.x + ((rect.width - ellipsis_width) >> 1);

		if (x_pos > e_pos)
		{
			// We are drawing a fragment after the ellipsis.
			if (ellipsis_position == ELLIPSIS_END)
				return TRUE; // We are done!

			if (x_pos > e_pos + ellipsis_width)
				return FALSE;

			// Part after ellipsis.
			int available_space = rect.x + rect.width - x_pos;
			int space_left_of_string = this->width - used_space;
			if (space_left_of_string - (int)frag->wi.GetWidth() < available_space)
			{
				int new_width = frag->wi.GetWidth();

				unsigned int i;
				// We have reached the part after the center that we can continue with.
				for (i = 0; i < frag->wi.GetLength() + 1; i++)
				{
					new_width = FragmentExtentFromTo(frag, frag_str, i, frag->wi.GetLength(), vis_dev, text_format);
					if (space_left_of_string - (int)frag->wi.GetWidth() + new_width < available_space)
						break;
				}
				int x = reverse_fragments ? rect.Right() - (x_pos - rect.x) - new_width : x_pos;
				// FIXME: possibly breaks truezoom - segment is split, so word_width cannot be passed
				vis_dev->TxtOutEx(x, y, frag_str + i, frag->wi.GetLength() - i, text_format);
				x_pos += new_width;
			}
			used_space += frag->wi.GetWidth();
			return TRUE;
		}

		if (x_pos + (int)frag->wi.GetWidth() > e_pos)
		{
			// We are drawing the last fragment before the ellipsis.
			const bool cut_from_start = reverse_fragments && !(text_format & TEXT_FORMAT_REVERSE_WORD);
			if (cut_from_start)
				frag_str += frag->wi.GetLength() - 1;

			unsigned int len, new_width = 0;
			for (len = 0; len < frag->wi.GetLength(); len++)
			{
				// Check how many letters that fit
				int next_width = FragmentExtentFromTo(frag, frag_str, 0, len + 1, vis_dev, text_format);
				if (x_pos + next_width > e_pos)
					break;
				new_width = next_width;
				if (cut_from_start)
					--frag_str;
			}
			if (cut_from_start)
				++frag_str;

			int x = reverse_fragments ? rect.Right() - (x_pos - rect.x) - new_width : x_pos;
			// FIXME: possibly breaks truezoom - segment is split, so word_width cannot be passed
			vis_dev->TxtOutEx(x, y, frag_str, len, text_format);
			x_pos += new_width;

			x = reverse_fragments ? rect.Right() - (x_pos - rect.x) - ellipsis_width : x_pos;
			// FIXME: possibly breaks truezoom - segment is split, so word_width cannot be passed
			vis_dev->TxtOut(x, y, UNI_L("..."), 3);
			x_pos += ellipsis_width;

			if (fragments.GetNumFragments() == 1)
			{
				// This is the only element. Draw the ending part.
				int tmp_used_space = 0;
				if (new_width || ellipsis_width) ///< Safeguard for bad font, causing endless recursion
					DrawFragmentSpecial(vis_dev, x_pos, y, frag, ellipsis_position, tmp_used_space, rect);
			}
			used_space += frag->wi.GetWidth();
			return TRUE;
		}
	}
	return FALSE;
}
#endif // WIDGETS_ELLIPSIS_SUPPORT

void DrawFragment(VisualDevice* vis_dev, int x, int y,
	int sel_start, int sel_stop, int ime_start, int ime_stop,
	const uni_char* str, OP_TEXT_FRAGMENT* frag,
	UINT32 color, UINT32 fcol_sel, UINT32 bcol_sel, VD_TEXT_HIGHLIGHT_TYPE selection_highlight_type,
	int height, BOOL use_accurate_font_size)
{
#if 0
	if (frag->packed.bidi & BIDI_FRAGMENT_NORMAL)
		vis_dev->SetColor(190, 190, 190);
	else if (frag->packed.bidi & BIDI_FRAGMENT_NEUTRAL_NORMAL)
		vis_dev->SetColor(230, 230, 230);
	else if (frag->packed.bidi & BIDI_FRAGMENT_MIRRORED)
		vis_dev->SetColor(255, 150, 150);
	else if (frag->packed.bidi & BIDI_FRAGMENT_NEUTRAL_MIRRORED)
		vis_dev->SetColor(255, 190, 190);
	vis_dev->RectangleOut(x, y, frag->wi.GetWidth(), height);
#endif

	const uni_char* word = str + frag->start;

	if (*word == 173 && frag->wi.GetWidth() == 0) // Nonvisible Soft hyphen
		return;
	if (frag->wi.GetLength() == 1)
	{
		// Don't paint control codes or format codes
		switch(Unicode::GetCharacterClass(str[frag->start]))
		{
		case CC_Cf:
		case CC_Cc:
			return;
		default:
			break;
		}
	}

#if defined(_MACINTOSH_)
	BOOL has_ime_highlight = FALSE;
	int local_sel_stop = 0;
#endif
	if (sel_start != sel_stop)
	{
		sel_start -= frag->start;
		sel_stop -= frag->start;
#if defined(_MACINTOSH_)
		// IME highlight might not apply to this section, but other sections still depend on it,
		// for drawing implementation.
		// Note that after the call to GetStartStopSelection, sel_start and sel_stop may not be different anymore, but
		// I am relying on the implementation detail, that this function is called with a non-empty selection if
		// ANY fragment has an active ime highlight. If that ever changes, this will stop working.
		has_ime_highlight = (ime_start != ime_stop);
		local_sel_stop = sel_stop;
#endif
		GetStartStopSelection(&sel_start, &sel_stop, frag->wi);
	}
	if (ime_start != ime_stop)
	{
		ime_start -= frag->start;
		ime_stop -= frag->start;
		GetStartStopSelection(&ime_start, &ime_stop, frag->wi);
	}

	const short word_width = use_accurate_font_size ? -1 : frag->wi.GetWidth();

#ifdef SUPPORT_TEXT_DIRECTION
	BOOL is_numbers = FALSE;
	if (word[0] >= '0' && word[0] <= '9')
		is_numbers = TRUE;

	if (frag->packed.bidi & BIDI_FRAGMENT_ANY_MIRRORED && !is_numbers && !frag->wi.IsTabCharacter())
		DrawRTLString(vis_dev, x, y, word, frag->wi.GetLength(), sel_start, sel_stop, color, fcol_sel, bcol_sel, height, frag, word_width);
	else
#endif // SUPPORT_TEXT_DIRECTION
	{
		int text_format = 0;
		if (frag->packed.bidi & BIDI_FRAGMENT_MIRRORED)
			text_format |= TEXT_FORMAT_REVERSE_WORD;

		vis_dev->SetColor(color);
		if (!frag->wi.IsTabCharacter())
			vis_dev->TxtOutEx(x, y, word, frag->wi.GetLength(), text_format, word_width);

#if defined(_MACINTOSH_)
		// Don't draw "selected" background if it is an IME highlight, rahter, change the colour of the underlining.
		if (ime_start == ime_stop)
#endif
		if (sel_start != sel_stop)
		{
			int start_x, stop_x;
			if (sel_stop - sel_start == (int) frag->wi.GetLength() || frag->wi.IsTabCharacter())
			{
				start_x = 0;
				stop_x = frag->wi.GetWidth();
			}
			else
			{
				if (use_accurate_font_size)
					vis_dev->BeginAccurateFontSize();
				start_x = FragmentExtentFromTo(frag, word, 0, sel_start, vis_dev, text_format);
				stop_x  = FragmentExtentFromTo(frag, word, 0, sel_stop,  vis_dev, text_format);
				if (use_accurate_font_size)
					vis_dev->EndAccurateFontSize();
			}

			OpRect selrect(x + start_x, y, stop_x - start_x, height);

			if (bcol_sel != CSS_COLOR_transparent)
				vis_dev->DrawTextBgHighlight(selrect, bcol_sel, selection_highlight_type);
			if (!frag->wi.IsTabCharacter())
			{
				vis_dev->BeginClipping(selrect);
				vis_dev->SetColor(fcol_sel);
				vis_dev->TxtOutEx(x, y, word, frag->wi.GetLength(), text_format, word_width);
				vis_dev->EndClipping();
			}
		}
#ifdef WIDGETS_IME_SUPPORT
		if (ime_start != ime_stop)
		{
			if (use_accurate_font_size)
				vis_dev->BeginAccurateFontSize();

#if defined(_MACINTOSH_)
			// What this code attempts to do is this:
			// if you are just typing, you should get a 2px black underline.
			// if you are adjusting the input, the edit highlight should be a 2px black
			// underline, while the rest of the ime input has a 2px gray underline.
			INT32 x_ime_start;
			INT32 x_ime_stop;
			OpPoint from(x, y + height - 2);
			if (has_ime_highlight)
			{
				x_ime_start = FragmentExtentFromTo(frag, word, 0, ime_start, vis_dev, text_format);
				if (ime_start < sel_start)
				{
					vis_dev->SetColor(147, 147, 147);
					x_ime_stop = FragmentExtentFromTo(frag, word, 0, sel_start, vis_dev, text_format);
					from.x = x + x_ime_start;
					vis_dev->DrawLine(from, x_ime_stop - x_ime_start - 1, TRUE, 2);
					x_ime_start = x_ime_stop;
				}
				if (sel_start < sel_stop)
				{
					vis_dev->SetColor(0, 0, 0);
					x_ime_stop = FragmentExtentFromTo(frag, word, 0, sel_stop, vis_dev, text_format);
					from.x = x + x_ime_start;
					int len = x_ime_stop - x_ime_start;
					if (local_sel_stop == sel_stop)
						len--;
					vis_dev->DrawLine(from, len, TRUE, 2);
					x_ime_start = x_ime_stop;
				}
				if (sel_stop < ime_stop)
				{
					vis_dev->SetColor(147, 147, 147);
					x_ime_stop = FragmentExtentFromTo(frag, word, 0, ime_stop, vis_dev, text_format);
					from.x = x + x_ime_start;
					vis_dev->DrawLine(from, x_ime_stop - x_ime_start - 1, TRUE, 2);
					x_ime_start = x_ime_stop;
					vis_dev->SetColor(0, 0, 0);
				}
			}
			else
			{
				x_ime_start = FragmentExtentFromTo(frag, word, 0, ime_start, vis_dev, text_format);
				x_ime_stop =  FragmentExtentFromTo(frag, word, 0, ime_stop,  vis_dev, text_format);
				vis_dev->SetColor(0, 0, 0);
				from.x = x + x_ime_start;
				vis_dev->DrawLine(from, x_ime_stop - x_ime_start, TRUE, 2);
			}
#else
			int start_x = FragmentExtentFromTo(frag, word, 0, ime_start, vis_dev, text_format);
			int stop_x =  FragmentExtentFromTo(frag, word, 0, ime_stop,  vis_dev, text_format);
			vis_dev->SetColor(WIDGETS_IME_CANDIDATE_HIGHLIGHT_COLOR);
			vis_dev->DrawLine(OpPoint(x + start_x, y + height - 2), stop_x - start_x, TRUE, 1);
#endif
			if (use_accurate_font_size)
				vis_dev->EndAccurateFontSize();
		}
#endif // WIDGETS_IME_SUPPORT
#ifdef INTERNAL_SPELLCHECK_SUPPORT
		if(frag->wi.IsMisspelling())
			DrawMisspelling(vis_dev, x, y, word, frag, height, FALSE);
#endif // INTERNAL_SPELLCHECK_SUPPORT
	}
}

OP_SYSTEM_COLOR OpWidgetString::GetSelectionTextColor(BOOL is_search_hit, BOOL is_focused)
{
	const OP_SYSTEM_COLOR fg_colors[2][2] = {
		// normal selection
		{ OP_SYSTEM_COLOR_TEXT_SELECTED_NOFOCUS, OP_SYSTEM_COLOR_TEXT_SELECTED },
		// search selection
		{ OP_SYSTEM_COLOR_TEXT_HIGHLIGHTED_NOFOCUS, OP_SYSTEM_COLOR_TEXT_HIGHLIGHTED },
	};
	return fg_colors[is_search_hit ? 1 : 0][is_focused ? 1 : 0];
}

OP_SYSTEM_COLOR OpWidgetString::GetSelectionBackgroundColor(BOOL is_search_hit, BOOL is_focused)
{
	const OP_SYSTEM_COLOR bg_colors[2][2] = {
		// normal selection
		{ OP_SYSTEM_COLOR_BACKGROUND_SELECTED_NOFOCUS, OP_SYSTEM_COLOR_BACKGROUND_SELECTED },
		// search selection
		{ OP_SYSTEM_COLOR_BACKGROUND_HIGHLIGHTED_NOFOCUS, OP_SYSTEM_COLOR_BACKGROUND_HIGHLIGHTED },
	};
	return bg_colors[is_search_hit ? 1 : 0][is_focused ? 1 : 0];
}

void OpWidgetString::Draw(const OpRect& rect, VisualDevice* vis_dev, UINT32 color)
{
	OpWidgetStringDrawer drawer;
	drawer.Draw(this, rect, vis_dev, color);
}

BOOL OpWidgetString::ShouldDrawFragmentsReversed(const OpRect& rect, ELLIPSIS_POSITION ellipsis_position) const
{
#ifdef WIDGETS_ELLIPSIS_SUPPORT
	return ellipsis_position == ELLIPSIS_END && width > rect.width && !GetForceLTR() && fragments.IsMostlyMirrored();
#else
	return FALSE;
#endif // WIDGETS_ELLIPSIS_SUPPORT
}

void OpWidgetString::SetScript(WritingSystem::Script script)
{
	m_script = script;
	NeedUpdate();
}

int OpWidgetString::GetJustifyAndIndent(const OpRect &rect)
{
	int x = rect.x;

	// Justify
	int width_and_indent = width + widget->text_indent;
	if(width_and_indent < rect.width)
	{
		if (widget->font_info.justify == JUSTIFY_CENTER)
			x += rect.width / 2 - width_and_indent / 2;
		else if (widget->font_info.justify == JUSTIFY_RIGHT)
			x += rect.width - width_and_indent;
	}

	// Add text-indent for LRT widgets. It is already added correctly for RTL because of its width (it is at the end, visually)
	if (!widget->GetRTL() || GetForceLTR())
		x += widget->text_indent;

	return x;
}

INT32 OpWidgetString::GetCaretPos(OpRect rect, OpPoint point, BOOL* snap_forward)
{
	OpStatus::Ignore(Update());//FIXME:OOM

	UpdateVisualDevice();

	point.x -= GetJustifyAndIndent(rect);

	VisualDevice* vis_dev = widget->vis_dev;
	int len = uni_strlen(str);
	if(len == 0)
		return 0;

	// Limit inside fragments so we know we get a hit in visual order.
	point.x = MAX(0, point.x);
	point.x = MIN(point.x, width - 1);

	// if both WIDGETS_IME_SUPPORT and SUPPORT_IME_PASSWORD_INPUT are
	// defined, str will contain nothing but password chars (except
	// str[uncoveredChar], if uncoveredChar >= 0), so regular approach
	// can be used.
	// if at least one of those is undefined measurment should be done
	// on a single password char and multiplied len times. this is to
	// avoid kerning issues - string is drawn one password char at the
	// time.
#if !defined(WIDGETS_IME_SUPPORT) || !defined(SUPPORT_IME_PASSWORD_INPUT)
	if (m_packed.password_mode)
	{
		// get width of password char
		const BOOL useAccurateFontSize = UseAccurateFontSize();
		if (useAccurateFontSize)
			vis_dev->BeginAccurateFontSize();
		vis_dev->SetFont(GetFontNumber());
		const INT32 passW = vis_dev->GetTxtExtent(g_widget_globals->passwd_char, 1);
		if (useAccurateFontSize)
			vis_dev->EndAccurateFontSize();
		OP_ASSERT(passW > 0);

		const INT32 nPassChars = (point.x + (passW >> 1)) / passW;
		OP_ASSERT(nPassChars >= 0);
		return nPassChars >= len ? len : nPassChars;
	}
	else
#endif // !WIDGETS_IME_SUPPORT || !SUPPORT_IME_PASSWORD_INPUT
	{
		INT32 x = 0;
		for(UINT32 i = 0; i < fragments.GetNumFragments(); i++)
		{
			OP_TEXT_FRAGMENT* frag = fragments.GetByVisualOrder(i);

			if (point.x >= x && point.x < x + (int)frag->wi.GetWidth())
			{
				int ofs = PosToOffsetInFragment(point.x - x, frag, &str[frag->start], vis_dev, UseAccurateFontSize());
				if (snap_forward)
					*snap_forward = (ofs == frag->start);
				return ofs;
			}

			x += frag->wi.GetWidth();
		}
	}

	return len;
}

INT32 OpWidgetString::GetCaretX(OpRect rect, int caret_pos, BOOL snap_forward)
{
	if (!widget || !widget->vis_dev)
	{
		return 0;
	}

	OpStatus::Ignore(Update());//FIXME:OOM

	UpdateVisualDevice();

	int x = GetJustifyAndIndent(rect);

#if !defined(WIDGETS_IME_SUPPORT) || !defined(SUPPORT_IME_PASSWORD_INPUT)
	if (m_packed.password_mode)
	{
		int orgiginal_fontnr = GetFontNumber();
		widget->vis_dev->SetFont(orgiginal_fontnr);
		int len = caret_pos;
#ifdef SUPPORT_IME_PASSWORD_INPUT
		if (fragment[0].len <= caret_pos)
		{
			x += widget->vis_dev->GetTxtExtent(&str[caret_pos - 1], 1);
			len--;
		}
#endif
		if (len)
		{
			x += widget->vis_dev->GetTxtExtent(g_widget_globals->passwd_char, 1) * len;
		}
		return x;
	}
	else
#endif
	{
		int candidate_x = x;
		for(UINT32 i = 0; i < fragments.GetNumFragments(); i++)
		{
			OP_TEXT_FRAGMENT* frag = fragments.GetByVisualOrder(i);
			int frag_start = frag->start;
			int frag_end = frag->start + frag->wi.GetLength();

			if (!snap_forward || i == fragments.GetNumFragments() - 1)
				frag_end++;

			if (snap_forward)
				frag_start--;

			if (frag->wi.GetLength() && caret_pos >= frag->start && caret_pos < frag_end)
			{
				// We have a candidate. Update candidate_x
				short old_font_num = widget->vis_dev->GetCurrentFontNumber();

				candidate_x = OffsetToPosInFragment(caret_pos, frag, &str[frag->start], widget->vis_dev, UseAccurateFontSize()) + x;

				widget->vis_dev->SetFont(old_font_num);

				// Only if we know we found the right fragment (depending on snap_forward), we will stop.
				if (caret_pos > frag_start)
					return candidate_x;
			}

			x += frag->wi.GetWidth();
		}
		return candidate_x;
	}
}

#ifdef WIDGETS_ADDRESS_SERVER_HIGHLIGHTING
void OpWidgetString::SetHighlightType(DomainHighlight type)
{
	if (m_highlight_type != type)
	{
		m_highlight_type = type;
		UpdateHighlightOffset();
	}
}

OP_STATUS OpWidgetString::UpdateHighlightOffset()
{
	m_highlight_ofs = -1;
	m_highlight_len = -1;

	if (m_highlight_type != None)
	{
		BOOL has_protocol = FALSE;

		OpString url;
		RETURN_IF_ERROR(url.Set(Get()));
		OpString protocol_string;

		RETURN_IF_ERROR(protocol_string.Set(Get()));
		int colon_index = protocol_string.FindFirstOf(':');

		if (colon_index != KNotFound)
		{
			protocol_string[colon_index] = '\0';
			URLType protocol = urlManager->MapUrlType(protocol_string.CStr());

			if (protocol != URL_UNKNOWN && protocol != URL_NULL_TYPE)
				has_protocol = TRUE;
		}

		if (!has_protocol)
			RETURN_IF_ERROR(url.Insert(0, "http://")); // Assume http

		URL tmp = urlManager->GetURL(url);

		INTPTR dummy;
		ServerName* server = (ServerName*)tmp.GetAttribute(URL::KServerName, &dummy);
		if (server)
		{
			BOOL ansi = url.FindI(server->GetName().CStr()) != KNotFound;
			int domain_length = ansi ? server->GetName().Length() : server->GetUniName().Length();
			int domain_offset = ansi ? url.FindI(server->GetName().CStr()) : url.FindI(server->GetUniName().CStr());

			m_highlight_ofs = 0;
			m_highlight_len = domain_length;

#ifdef PUBLIC_DOMAIN_LIST
			if (m_highlight_type == RootDomain)
			{
				while (server && server->GetUniName().Length())
				{
					BOOL is_public_domain;
					RETURN_IF_ERROR(g_secman_instance->IsPublicDomain(server->GetUniName(), is_public_domain));

					if (is_public_domain)
						break;

					m_highlight_len = ansi ? server->GetName().Length() : server->GetUniName().Length();
					m_highlight_ofs = domain_length - m_highlight_len;
					server = server->GetParentDomain();
				}
			}
# endif // PUBLIC_DOMAIN_LIST
			if (has_protocol)
				m_highlight_ofs += domain_offset;
		}
	}
	return OpStatus::OK;
}
#endif // WIDGETS_ADDRESS_SERVER_HIGHLIGHTING

#if defined(WIDGETS_IME_SUPPORT) && defined(SUPPORT_IME_PASSWORD_INPUT)

OP_STATUS OpWidgetString::UpdatePasswordText()
{
    int len = uni_strlen(str);
	uni_char *nonconst_str = (uni_char *)str;

	for (int index = 0; index < len; ++index)
		nonconst_str[index] = g_widget_globals->passwd_char[0];

    if (uncoveredChar>=0 && uncoveredChar<len)
	    nonconst_str[uncoveredChar] = str_original[uncoveredChar];

	return OpStatus::OK;
}

void OpWidgetString::ShowPasswordChar(int caret_pos)
{
	if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::ShowIMEPassword))
	{
		uncoveredChar = caret_pos-1;
		if (str_original)
			UpdatePasswordText();
		NeedUpdate();
	}
}
void OpWidgetString::HidePasswordChar()
{
	if (uncoveredChar >= 0)
	{
		uncoveredChar = -1;
		if (str_original)
			UpdatePasswordText();
		NeedUpdate();
	}
}

#endif

BOOL OpWidgetString::IsWithinSelection(VisualDevice* vis_dev, const OpPoint& point, const int scroll, const OpRect& text_rect)
{
	if (!text_rect.Contains(point))
		return FALSE;

	INT32 sel_start = this->sel_start;
	INT32 sel_stop = this->sel_stop;
	if (sel_start == sel_stop)
	{
		sel_start = -1;
		sel_stop = -1;
	}
	else if (sel_start > sel_stop)
	{
		INT32 tmp = sel_start;
		sel_start = sel_stop;
		sel_stop = tmp;
	}

	UpdateVisualDevice(vis_dev);
	OpPoint p(GetJustifyAndIndent(text_rect), text_rect.y + text_rect.height/2 - height/2);
	int x_pos = p.x;

	for(UINT32 i = 0; i < fragments.GetNumFragments(); i++)
	{
		OP_TEXT_FRAGMENT* frag = fragments.GetByVisualOrder(i);

		INT32 frag_sel_start = sel_start;
		INT32 frag_sel_stop = sel_stop;
		if (frag_sel_start != frag_sel_stop)
		{
			frag_sel_start -= frag->start;
			frag_sel_stop -= frag->start;
			GetStartStopSelection(&frag_sel_start, &frag_sel_stop, frag->wi);
		}

		if (frag_sel_start != frag_sel_stop)
		{
			const uni_char* word = str + frag->start;
			int start_x = OffsetToPosInFragment(frag_sel_start + frag->start, frag, word, vis_dev, UseAccurateFontSize());
			int stop_x = OffsetToPosInFragment(frag_sel_stop + frag->start, frag, word, vis_dev, UseAccurateFontSize());
			if (start_x > stop_x)
			{
				int tmp = start_x;
				start_x = stop_x;
				stop_x = tmp;
			}
			OpRect selrect(x_pos + start_x - scroll, p.y, stop_x - start_x, height);

			selrect.IntersectWith(text_rect);
			if (selrect.IsEmpty())
			{
				x_pos += frag->wi.GetWidth();
				continue;
			}
			if (selrect.Contains(point))
				return TRUE;
		}

		x_pos += frag->wi.GetWidth();
	}

	return FALSE;
}

OP_STATUS OpWidgetString::GetSelectionRects(VisualDevice* vis_dev, OpVector<OpRect>* list, OpRect& union_rect, int scroll, const OpRect& text_rect, BOOL visible_only)
{
	INT32 sel_start = this->sel_start;
	INT32 sel_stop = this->sel_stop;
	if (sel_start == sel_stop)
	{
		sel_start = -1;
		sel_stop = -1;
	}
	else if (sel_start > sel_stop)
	{
		INT32 tmp = sel_start;
		sel_start = sel_stop;
		sel_stop = tmp;
	}

	UpdateVisualDevice(vis_dev);
	OpPoint p(GetJustifyAndIndent(text_rect), text_rect.y + text_rect.height/2 - height/2);
	int x_pos = p.x;

	for(UINT32 i = 0; i < fragments.GetNumFragments(); i++)
	{
		OP_TEXT_FRAGMENT* frag = fragments.GetByVisualOrder(i);

		INT32 frag_sel_start = sel_start;
		INT32 frag_sel_stop = sel_stop;
		if (frag_sel_start != frag_sel_stop)
		{
			frag_sel_start -= frag->start;
			frag_sel_stop -= frag->start;
			GetStartStopSelection(&frag_sel_start, &frag_sel_stop, frag->wi);
		}

		OpRect* selrect = NULL;
		if (frag_sel_start != frag_sel_stop)
		{
			const uni_char* word = str + frag->start;
			int start_x = OffsetToPosInFragment(frag_sel_start + frag->start, frag, word, vis_dev, UseAccurateFontSize());
			int stop_x = OffsetToPosInFragment(frag_sel_stop + frag->start, frag, word, vis_dev, UseAccurateFontSize());
			if (start_x > stop_x)
			{
				int tmp = start_x;
				start_x = stop_x;
				stop_x = tmp;
			}
			selrect = OP_NEW(OpRect, (x_pos + start_x - scroll, p.y, stop_x - start_x, height));

			if (!selrect)
				return OpStatus::ERR_NO_MEMORY;

			if (visible_only)
			{
				selrect->IntersectWith(text_rect);
				if (selrect->IsEmpty())
				{
					OP_DELETE(selrect);
					x_pos += frag->wi.GetWidth();
					continue;
				}
			}
			OpAutoPtr<OpRect> auto_rect(selrect);
			RETURN_IF_ERROR(list->Add(selrect));
			auto_rect.release();
			union_rect.UnionWith(*selrect);
		}

		x_pos += frag->wi.GetWidth();
	}

	return OpStatus::OK;
}
