/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Øyvind Østlund
**
*/

#include "core/pch.h"

#include "DirectWrite.h"
#include "platforms/windows/utils/com_helpers.h"

//
// Helper functions to convert from DirectWrite font weights to Opera font weights, and back.
//

const DWRITE_FONT_WEIGHT ToDWriteWeight(UINT8 weight)
{
	switch (weight)
	{
		case 1: return DWRITE_FONT_WEIGHT_THIN;
		case 2: return DWRITE_FONT_WEIGHT_EXTRA_LIGHT;
		case 3: return DWRITE_FONT_WEIGHT_LIGHT;
		case 4: return DWRITE_FONT_WEIGHT_NORMAL;
		case 5: return DWRITE_FONT_WEIGHT_MEDIUM;
		case 6: return DWRITE_FONT_WEIGHT_SEMI_BOLD;
		case 7: return DWRITE_FONT_WEIGHT_BOLD;
		case 8: return DWRITE_FONT_WEIGHT_EXTRA_BOLD;
		case 9: return DWRITE_FONT_WEIGHT_BLACK;
		default:
		{
			// The calling code should be fixed.
			OP_ASSERT(weight >= 0 && weight <= 9);
			return DWRITE_FONT_WEIGHT_NORMAL;
		}
	}
}

const UINT8 ToFontWeight(DWRITE_FONT_WEIGHT dw_weight)
{
	switch (dw_weight)
	{
		case DWRITE_FONT_WEIGHT_THIN:			return 1; // 100
		case DWRITE_FONT_WEIGHT_EXTRA_LIGHT:	return 2; // 200
		case DWRITE_FONT_WEIGHT_LIGHT:			return 3; // 300
		case DWRITE_FONT_WEIGHT_NORMAL:			return 4; // 400
		case DWRITE_FONT_WEIGHT_MEDIUM:			return 5; // 500
		case DWRITE_FONT_WEIGHT_DEMI_BOLD:		return 6; // 600
		case DWRITE_FONT_WEIGHT_BOLD:			return 7; // 700
		case DWRITE_FONT_WEIGHT_EXTRA_BOLD:		return 8; // 800
		case DWRITE_FONT_WEIGHT_BLACK:			return 9; // 900
		case DWRITE_FONT_WEIGHT_EXTRA_BLACK:	return 9; // 950
		default:
		{
			OP_ASSERT(!"This font size must be new. Look it up in DWRITE_FONT_WEIGHT and fill it into the switch.");
			return 4; // 400
		}
	}
}


//
// DWriteGlyphRun
//

void DWriteGlyphRun::Delete()
{
	OP_DELETEA(advances);
	OP_DELETEA(indices);
	OP_DELETEA(offsets);
	advances = NULL;
	indices = NULL;
	offsets = NULL;
	size = 0;
	len = 0;
}

OP_STATUS DWriteGlyphRun::Append(DWriteGlyphRun* run)
{
	UINT new_size = len + run->len;

	if (new_size > size)
		RETURN_IF_ERROR(Resize(new_size * 2, !len ? false : true));

	op_memcpy(advances + len, run->advances, run->len * sizeof(FLOAT));
	op_memcpy(indices + len, run->indices, run->len * sizeof(UINT16));
	op_memcpy(offsets + len, run->offsets, run->len * sizeof(DWRITE_GLYPH_OFFSET));

	// FIXME: (Potential memory and speed optimization) Very few chars uses offsets, so this can be optimized away in most cases.
	//if (run->offsets)
	//{
	//	SetOffsets(run->offsets, len, run->len);
	//}

	len = new_size;
	
	return OpStatus::OK;
}

OP_STATUS DWriteGlyphRun::Resize(UINT new_size, bool preserve)
{
	if (size == new_size)
		return OpStatus::OK;

	OP_ASSERT(!((len > new_size) && preserve) && "If preserve is chosen, new_size can't be smaller than len");

	UINT16* new_indices = OP_NEWA(UINT16, new_size); // at least 'maxglyphs' long
	FLOAT* new_advance = OP_NEWA(FLOAT, new_size); // at least 'num glyphs' long
	DWRITE_GLYPH_OFFSET* new_offset = OP_NEWA(DWRITE_GLYPH_OFFSET, new_size); // at least 'num glyphs' long

	if (!new_indices || !new_advance || !new_offset)
	{
		OP_DELETEA(new_indices);
		OP_DELETEA(new_advance);
		OP_DELETEA(new_offset);
		return OpStatus::ERR_NO_MEMORY;
	}

	if (preserve)
	{
		op_memcpy(new_indices, indices, size * sizeof(UINT16));
		op_memcpy(new_advance, advances, size * sizeof(FLOAT));
		op_memcpy(new_offset, offsets, size * sizeof(DWRITE_GLYPH_OFFSET));
	}

	OP_DELETEA(indices);
	OP_DELETEA(advances);
	OP_DELETEA(offsets);

	indices = new_indices;
	advances = new_advance;
	offsets = new_offset;

	size = new_size;

	return OpStatus::OK;
}


//
// DWriteString
//

OP_STATUS DWriteString::SetString(const uni_char* str, UINT len)
{
	// If there is not enough space for the string. Increase the size.
	if (len > size)
	{
		OP_DELETEA(this->str);
		this->str = OP_NEWA(uni_char, len);

		if (!str)
		{
			size = 0;
			len = 0;
			str = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}

		size = len;
	}

	if (len > 0)
		uni_strncpy(this->str, str, len);

	this->len = len;

	// If the new length is longer than the size of the glyph run, then resize it rather now than later.
	if (len > run.size)
		RETURN_IF_ERROR(run.Resize(len, false));

	return OpStatus::OK;
}


//
// DWriteOpFontInfo
//

OP_STATUS DWriteOpFontInfo::InitFontFamilyInfo(IDWriteFontFamily* dw_font_family, IDWriteFont* dw_font)
{
	// Font family name
	RETURN_IF_ERROR(InitFontFamilyName(dw_font_family));

	// Font style info
	InitFontStyleInfo(dw_font);

	return OpStatus::OK;
}

OP_STATUS DWriteOpFontInfo::InitFontFaceInfo(IDWriteFontFace* dw_font_face)
{
	// OS/2 font table
	const UINT8* os2_tab = NULL;
	UINT32 os2_tab_size = 0;
	void* os2_ctx = NULL;
	BOOL has_OS2 = FALSE;
	if (FAILED(dw_font_face->TryGetFontTable(DWRITE_MAKE_OPENTYPE_TAG('O', 'S', '/', '2'), (const void**)&os2_tab, &os2_tab_size, &os2_ctx, &has_OS2)) || !os2_tab || !has_OS2)
	{
		os2_tab = NULL;
		os2_tab_size = 0;
	}

	// Font classification
	InitFontClassification(os2_tab, os2_tab_size);

	// Font script and codepage
	InitScriptAndCodepage(os2_tab, os2_tab_size);

	dw_font_face->ReleaseFontTable(os2_ctx);

	return OpStatus::OK;
}

OP_STATUS DWriteOpFontInfo::CreateCopy(OpFontInfo* font_info)
{
	RETURN_IF_ERROR(font_info->SetFace(m_font_info.GetFace()));

	font_info->SetNormal(m_font_info.HasNormal());
	font_info->SetItalic(m_font_info.HasItalic());
	font_info->SetOblique(m_font_info.HasOblique());
	font_info->SetWeight(m_weight, TRUE);

	font_info->SetSerifs(m_font_info.GetSerifs());
	font_info->SetVertical(m_font_info.Vertical());
	font_info->SetMonospace(m_font_info.Monospace());
	font_info->SetSmallcaps(m_font_info.Smallcaps());

	for (UINT script = 0; script < WritingSystem::NumScripts; ++script)
	{
		WritingSystem::Script writing_script = static_cast<WritingSystem::Script>(script);

		if (font_info->HasScript(writing_script))
			font_info->SetScript(writing_script, TRUE);
	}

	font_info->MergeFontInfo(m_font_info);

	return OpStatus::OK;
}

const uni_char* DWriteOpFontInfo::GetFontName() const
{
	return m_font_info.GetFace();
}

OP_STATUS DWriteOpFontInfo::InitFontFamilyName(IDWriteFontFamily* dw_font_family)
{
	IDWriteLocalizedStrings* dw_localized_names = NULL;
	if (FAILED(dw_font_family->GetFamilyNames(&dw_localized_names)))
		return OpStatus::ERR;

	OpComPtr<IDWriteLocalizedStrings> localized_names_ptr(dw_localized_names);

	if (!dw_localized_names->GetCount())
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	UINT32 index = 0;
	BOOL exists = FALSE;
	HRESULT hr = E_FAIL;
	uni_char locale_name[LOCALE_NAME_MAX_LENGTH] = {0};

	// Try to find the localized font name index that matches the users locale setting.
	if(OPGetSystemDefaultLocaleName(locale_name, LOCALE_NAME_MAX_LENGTH))
		hr = localized_names_ptr->FindLocaleName(locale_name, &index, &exists);

	// First fallback will use the en-us localized font name.
	if (FAILED(hr) || !exists)
		hr = localized_names_ptr->FindLocaleName(UNI_L("en-us"), &index, &exists);

	// Last fallback is to just use the first font name in the font.
	if (FAILED(hr) || !exists)
		index = 0;

	UINT32 length;
	if (FAILED(localized_names_ptr->GetStringLength(index, &length)))
		return OpStatus::ERR;

	OpString font_name;
	RETURN_OOM_IF_NULL(font_name.Reserve(length + 1));
	if (FAILED(localized_names_ptr->GetString(index, font_name.CStr(), font_name.Size())) || font_name.IsEmpty())
		return OpStatus::ERR;

	return m_font_info.SetFace(font_name.CStr());
}

void DWriteOpFontInfo::InitFontClassification(const UINT8* os2_tab, const UINT32 os2_tab_size)
{
	// Use IBM font class id to determine serifs.
	switch (os2_tab_size >= 32 ? os2_tab[30] : 0)
	{
		case  0: // no clasification
		case  6: // reserved
		case  9: // ornamentals
		case 11: // reserved
		case 13: // reserved
		case 14: // reserved
		case 12: // symbolic
			break;

		case  1: // oldstyle serifs
		case  2: // transitional serifs
		case  3: // modern serifs
		case  4: // clarendon serifs
		case  5: // slab serifs
		case  7: // freeform serifs
			m_font_info.SetSerifs(OpFontInfo::WithSerifs);
			break;

		case  8: // sans serif
		case 10: // scripts
			m_font_info.SetSerifs(OpFontInfo::WithoutSerifs);
			break;

		default:
			OP_ASSERT(!"IBM font class out of bounds");
			break;
	}

	// Classification
	m_font_info.SetVertical(m_font_info.GetFace() && m_font_info.GetFace()[0] == '@');
	m_font_info.SetMonospace(FALSE);
	m_font_info.SetSmallcaps(FALSE);

	if (os2_tab_size >= 36)
	{
		UINT8 bFamily = os2_tab[32];

		if (bFamily == 2)
		{
			m_font_info.SetMonospace(os2_tab[35] == 9);
		}
	}
}

void DWriteOpFontInfo::InitFontStyleInfo(IDWriteFont* dw_font)
{
	// Style
	if (dw_font->GetStyle() == DWRITE_FONT_STYLE_NORMAL)
		m_font_info.SetNormal(TRUE);
	else if (dw_font->GetStyle() == DWRITE_FONT_STYLE_ITALIC)
		m_font_info.SetItalic(TRUE);
	else if (dw_font->GetStyle() == DWRITE_FONT_STYLE_OBLIQUE)
		m_font_info.SetOblique(TRUE);

	// Weight
	m_weight = ToFontWeight(dw_font->GetWeight());
}

void DWriteOpFontInfo::InitScriptAndCodepage(const UINT8* os2_tab, const UINT32 os2_tab_size)
{
	UINT32 ranges[4] = {0,0,0,0};
	UINT32 codepages[2] = {0,0};

	m_font_info.SetScript(WritingSystem::Unknown);

	if (os2_tab_size >= 58)
	{
		ranges[0] = (os2_tab[42]<<24) | (os2_tab[43]<<16) | (os2_tab[44]<<8) | (os2_tab[45]);
		ranges[1] = (os2_tab[46]<<24) | (os2_tab[47]<<16) | (os2_tab[48]<<8) | (os2_tab[49]);
		ranges[2] = (os2_tab[50]<<24) | (os2_tab[51]<<16) | (os2_tab[52]<<8) | (os2_tab[53]);
		ranges[3] = (os2_tab[54]<<24) | (os2_tab[55]<<16) | (os2_tab[56]<<8) | (os2_tab[57]);
	}

	if (os2_tab_size >= 86)
	{
		codepages[0] = (os2_tab[78]<<24) | (os2_tab[79]<<16) | (os2_tab[80]<<8) | (os2_tab[81]);
		codepages[1] = (os2_tab[82]<<24) | (os2_tab[83]<<16) | (os2_tab[84]<<8) | (os2_tab[85]);
	}

	// Fallback to iso-latin-1 if no unicode ranges are set.
	// This is true for the Ahem font.
	if (!os2_tab || (ranges[0] == 0 && ranges[1] == 0 && ranges[2] == 0 && ranges[3] == 0))
	{
		ranges[0] = 3; // set to iso-latin-1-ish
	}

	for (int r = 0; r < 4; ++r)
		for (int b = 0 ; b < 32 ; ++b)
			m_font_info.SetBlock(r * 32 + b, (ranges[r] & (1<<b)) == 0 ? FALSE : TRUE);

	m_font_info.SetScriptsFromOS2CodePageRanges(codepages, 1);
}
