/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/display/color.h" // USE_DEFAULT_COLOR
#include "modules/display/styl_man.h"


LayoutAttr::LayoutAttr()
{
    LeftHandOffset = 0;
    RightHandOffset = 0;
    LeadingOffset = 0;
    TrailingOffset = 0;
    LeadingSeparation = 0;
    TrailingSeparation = 0;
}

LayoutAttr::LayoutAttr(int lho, int rho, int lo, int to, int ls, int ts)
{
    LeftHandOffset = lho;
    RightHandOffset = rho;
    LeadingOffset = lo;
    TrailingOffset = to;
    LeadingSeparation = ls;
    TrailingSeparation = ts;
}

OP_STATUS PresentationAttr::PresentationFont::Set(const FontAtt* fa)
{
	FontAtt* old = Font;
	if (Font && !fa)
	{
		OP_DELETE(Font);
		Font = old = 0;
	}
	else if (!Font && fa)
	{
		old = OP_NEW(FontAtt, ());
		if (!old)
			return OpStatus::ERR_NO_MEMORY;
	}
	if (old)
	{
		Font = old;
		*Font = *fa;
		FontNumber = styleManager->GetFontNumber(Font->GetFaceName());
	}
	return OpStatus::OK;
}

PresentationAttr::PresentationAttr()
{
    Italic = FALSE;
    Bold = FALSE;
	InheritFontSize = FALSE;
   	Color = USE_DEFAULT_COLOR;
   	Underline = FALSE;
   	StrikeOut = FALSE;
}

PresentationAttr::PresentationAttr(BOOL i, BOOL b, long col, BOOL inherit_fs, BOOL uline, BOOL strike)
{
    Italic = i;
    Bold = b;
	InheritFontSize = inherit_fs;
    Color = col;
    Underline = uline;
    StrikeOut = strike;
}

OP_STATUS PresentationAttr::Construct(const FontAtt* lf)
{
	if (lf)
	{
		RETURN_IF_ERROR(font_attr[WritingSystem::Unknown].Set(lf));
		RETURN_IF_ERROR(SetFontsFromDefaultScript());
	}
	return OpStatus::OK;
}

OP_STATUS PresentationAttr::SetFontsFromDefaultScript()
{
	PresentationFont& def_attr = font_attr[WritingSystem::Unknown];
	OpFontInfo* default_font = styleManager->GetFontInfo(def_attr.FontNumber);

	if (!def_attr.Font)
		return OpStatus::OK;

	FontAtt att;
	att = *def_attr.Font;

#ifdef PERSCRIPT_GENERIC_FONT
	StyleManager::GenericFont generic;
	OpFontInfo::FontSerifs serifs = default_font->Monospace() ? OpFontInfo::UnknownSerifs : default_font->GetSerifs();
	switch (serifs)
	{
	case OpFontInfo::WithSerifs:
		generic = StyleManager::SERIF;
		break;
	case OpFontInfo::WithoutSerifs:
		generic = StyleManager::SANS_SERIF;
		break;
	default:
		generic = default_font->Monospace() ? StyleManager::MONOSPACE : StyleManager::SERIF;
	}
#endif // PERSCRIPT_GENERIC_FONT

	for (int i = 0; i < (int)WritingSystem::Unknown; ++i)
	{
		const WritingSystem::Script s = (WritingSystem::Script)i;
		OpFontInfo* new_font = default_font;

#ifdef PERSCRIPT_GENERIC_FONT
		if (!default_font->HasScript(s))
		{
			StyleManager::GenericFont this_generic = generic;
			if (generic == StyleManager::SERIF &&
				(s == WritingSystem::ChineseSimplified
				 || s == WritingSystem::ChineseTraditional
				 || s == WritingSystem::ChineseUnknown
				 || s == WritingSystem::Japanese
				 || s == WritingSystem::Korean))
			{
				// Don't use serif for cjk (maybe Arabic as well)
				this_generic = StyleManager::SANS_SERIF;
			}
			OP_ASSERT(styleManager);
			new_font = styleManager->GetFontInfo(styleManager->GetGenericFontNumber(this_generic, s));
		}
		OP_ASSERT(new_font);
#else // PERSCRIPT_GENERIC_FONT
# ifdef FONTSWITCHING
		OP_ASSERT(styleManager);
		new_font = styleManager->GetFontForScript(default_font, s);
# endif // FONTSWITCHING
#endif // PERSCRIPT_GENERIC_FONT

		if (new_font)
		{
			int fn = new_font->GetFontNumber();
			att.SetFontNumber(fn);
		}
		else
		{
			att.SetFontNumber(def_attr.FontNumber);
		}

		RETURN_IF_ERROR(font_attr[s].Set(&att));
	}

	return OpStatus::OK;
}

Style::Style(const LayoutAttr& lattr, const PresentationAttr& pattr)
  : layout_attr(lattr), pres_attr(pattr)
{
}

Style::Style(int lho, int rho, int lo, int to, int ls, int ts, BOOL i, BOOL b, long col, BOOL inherit_fs, BOOL uline, BOOL strike)
  : layout_attr(lho, rho, lo, to, ls, ts),
	pres_attr(i, b, col, inherit_fs, uline, strike)
{
}

OP_STATUS Style::Construct(const FontAtt* lf)
{
	return pres_attr.Construct(lf);
}

Style* Style::Create(int lho, int rho, int lo, int to, int ls, int ts, BOOL i, BOOL b, const FontAtt* lf, long col, BOOL inherit_fs, BOOL uline, BOOL strike)
{
	Style* style = OP_NEW(Style, (lho, rho, lo, to, ls, ts, i, b, col, inherit_fs, uline, strike));
	if (!style)
		return NULL;
	if (OpStatus::IsError(style->Construct(lf)))
	{
		OP_DELETE(style);
		return NULL;
	}
	return style;
}


void Style::SetLayoutAttr(const LayoutAttr& lattr)
{
    layout_attr = lattr;
}

OP_STATUS Style::SetPresentationAttr(const PresentationAttr& pattr)
{
	for (int i = 0; i < WritingSystem::NumScripts; ++i)
	{
		const WritingSystem::Script s = (WritingSystem::Script)i;
		RETURN_IF_ERROR(pres_attr.font_attr[s].Set(pattr.font_attr[s].Font));
	}

	pres_attr.InheritFontSize = pattr.InheritFontSize;
	pres_attr.Color = pattr.Color;
	pres_attr.Underline = pattr.Underline;
	pres_attr.StrikeOut = pattr.StrikeOut;
	return OpStatus::OK;
}

OP_STATUS Style::SetFont(const FontAtt* att, const WritingSystem::Script script)
{
	return pres_attr.font_attr[(int)script].Set(att);
}

Style::~Style()
{
	for (int i = 0; i < (int)WritingSystem::NumScripts; ++i)
		OP_DELETE(pres_attr.font_attr[i].Font);
}
