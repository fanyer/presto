/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGFontTraversalObject.h"
#include "modules/svg/src/SVGFontImpl.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGTraverse.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGFontSize.h"
#include "modules/svg/src/parser/svgnumberparser.h"

#include "modules/layout/cssprops.h"

/**
 * This traverser is meant to build an OpFontInfo structure from a Markup::SVGE_FONT element tree.
 * Nevermind that OpFontInfo contains less information than we can extract here, we only
 * want the OpFontInfo for the fontswitching to work. Later, when needed, we can create the
 * svgfont given this OpFontInfo.
 */

BOOL SVGFontTraversalObject::AllowChildTraverse(HTML_Element* element)
{
	Markup::Type type = element->Type();

	/**
	 * "SVGB and SVGT support a subset of SVG fonts where only the 'd' attribute on the
	 *  'glyph' and 'missing-glyph' elements is available. Arbitrary SVG within a 'glyph'
	 *  is not supported."
	 */
	return (type == Markup::SVGE_FONT || type == Markup::SVGE_FONT_FACE  || type == Markup::SVGE_HKERN ||
			type == Markup::SVGE_VKERN || type == Markup::SVGE_FONT_FACE_SRC);
}

OP_STATUS SVGFontTraversalObject::CSSAndEnterCheck(HTML_Element* element, BOOL& traversal_needs_css)
{
	// FIXME: Reject textnodes by returning SKIP_ELEMENT
	traversal_needs_css = FALSE;
	return OpStatus::OK;
}

#ifdef FONTSWITCHING
static BOOL UCRangeToBlockRange(UnicodePoint uc_start, UnicodePoint uc_end, int& start_block, int& end_block)
{
	UnicodePoint block_lowest, block_highest;
	styleManager->GetUnicodeBlockInfo(uc_start, start_block, block_lowest, block_highest);
	if (start_block == UNKNOWN_BLOCK_NUMBER)
		return FALSE;

	if (uc_end > (unsigned)block_highest)
	{
		// Range spans several blocks
		styleManager->GetUnicodeBlockInfo(uc_end, end_block, block_lowest, block_highest);
	}
	else
	{
		end_block = start_block;
	}

	if (end_block == UNKNOWN_BLOCK_NUMBER)
		return FALSE;

	// FIXME: It would be nice it we could resolve to
	// _something_ if the block lookups fail
	return TRUE;
}
#endif // FONTSWITCHING

OP_STATUS SVGFontTraversalObject::SetupFontInfoForFontFace(HTML_Element* element)
{
	OpFontInfo* fontinfo = m_fontinfo;

	// only set the font-family if not set already (covers the case where font-face-uri is used)
	if (fontinfo->GetFace() == NULL)
	{
		// uni_char*
		SVGString* family = NULL;
		RETURN_IF_ERROR(AttrValueStore::GetString(element, Markup::SVGA_FONT_FAMILY, &family));
		if (family)
		{
			OpString16 family_str;
			RETURN_IF_ERROR(family_str.Set(family->GetString(), family->GetLength()));
			RETURN_IF_ERROR(fontinfo->SetFace(family_str));
		}
	}

	// Vector of values
	// normal | italic | oblique
	SVGVector* fs = NULL;
	AttrValueStore::GetVector(element, Markup::SVGA_FONT_STYLE, fs);
	if (fs)
	{
		for (UINT32 i = 0; i < fs->GetCount(); i++)
		{
			SVGString* eval = static_cast<SVGString*>(fs->Get(i));
			const uni_char* str = eval->GetString();
			unsigned strlen = eval->GetLength();

			int enum_val = SVGEnumUtils::GetEnumValue(SVGENUM_FONT_STYLE, str, strlen);
			if (enum_val != -1)
			{
				switch (enum_val)
				{
				case SVGFONTSTYLE_ITALIC:
					fontinfo->SetItalic(TRUE);
					break;
				case SVGFONTSTYLE_NORMAL:
					fontinfo->SetNormal(TRUE);
					break;
				case SVGFONTSTYLE_OBLIQUE:
					fontinfo->SetOblique(TRUE);
					break;
				}
			}
		}
	}

	// Vector of values
	// normal | smallcaps
	SVGVector* fv = NULL;
	AttrValueStore::GetVector(element, Markup::SVGA_FONT_VARIANT, fv);
	if (fv)
	{
		for (UINT32 i = 0; i < fv->GetCount(); i++)
		{
			SVGString* eval = static_cast<SVGString*>(fv->Get(i));
			const uni_char* str = eval->GetString();
			unsigned strlen = eval->GetLength();

			int enum_val = SVGEnumUtils::GetEnumValue(SVGENUM_FONT_VARIANT, str, strlen);
			if (enum_val == SVGFONTVARIANT_SMALLCAPS)
				fontinfo->SetSmallcaps(TRUE);
		}
	}

	{
		SVGFontSize font_size;
		RETURN_IF_ERROR(AttrValueStore::GetFontSize(element, font_size));
		if (font_size.Mode() == SVGFontSize::MODE_RESOLVED)
		{
			SVGNumber len = font_size.ResolvedLength();
			fontinfo->SetParsedFontSize(len.GetRoundedIntegerValue());
		}
		else if (font_size.Mode() == SVGFontSize::MODE_LENGTH)
		{
			const SVGLength& len = font_size.Length();
			int unit = len.GetUnit();

			if (unit != CSS_LENGTH_em && unit != CSS_LENGTH_ex)
			{
				fontinfo->SetParsedFontSize(len.GetScalar().GetRoundedIntegerValue());
			}
		}
	}

	SVGVector* fw = NULL;
	AttrValueStore::GetVector(element, Markup::SVGA_FONT_WEIGHT, fw);
	if (fw)
	{
		for (UINT32 i = 0; i < fw->GetCount(); i++)
		{
			SVGString* eval = static_cast<SVGString*>(fw->Get(i));
			const uni_char* str = eval->GetString();
			unsigned strlen = eval->GetLength();
			int enum_val = SVGEnumUtils::GetEnumValue(SVGENUM_FONT_WEIGHT, str, strlen);
			if (enum_val != -1)
			{
				if (enum_val == SVGFONTWEIGHT_ALL)
				{
					for (int j = 0; j <= 9; j++)
						fontinfo->SetWeight(j, TRUE);

					break;
				}

				switch (enum_val)
				{
				case SVGFONTWEIGHT_100:
					fontinfo->SetWeight(1, TRUE);
					break;
				case SVGFONTWEIGHT_200:
					fontinfo->SetWeight(2, TRUE);
					break;
				case SVGFONTWEIGHT_300:
					fontinfo->SetWeight(3, TRUE);
					break;
				case SVGFONTWEIGHT_NORMAL:
				case SVGFONTWEIGHT_400:
					fontinfo->SetWeight(4, TRUE);
					break;
				case SVGFONTWEIGHT_500:
					fontinfo->SetWeight(5, TRUE);
					break;
				case SVGFONTWEIGHT_600:
					fontinfo->SetWeight(6, TRUE);
					break;
				case SVGFONTWEIGHT_BOLD:
				case SVGFONTWEIGHT_700:
					fontinfo->SetWeight(7, TRUE);
					break;
				case SVGFONTWEIGHT_800:
					fontinfo->SetWeight(8, TRUE);
					break;
				case SVGFONTWEIGHT_900:
					fontinfo->SetWeight(9, TRUE);
					break;
				}
			}
		}
	}

#ifdef FONTSWITCHING
	SVGVector* vector = NULL;
	AttrValueStore::GetVector(element, Markup::SVGA_UNICODE_RANGE, vector);
	if (vector && vector->GetCount() > 0)
	{
		SVGNumberParser parser;
		for (unsigned int i = 0; i < vector->GetCount(); i++)
		{
			SVGString* s = static_cast<SVGString*>(vector->Get(i));
			OP_ASSERT(s);

			UnicodePoint uc_start, uc_end;
			if (OpStatus::IsError(parser.ParseUnicodeRange(s->GetString(), s->GetLength(),
														   uc_start, uc_end)))
				continue;

			if (uc_start > uc_end) // ???
				continue;

			int start_block_no, end_block_no;
			if (UCRangeToBlockRange(uc_start, uc_end, start_block_no, end_block_no))
				for (int i = start_block_no; i <= end_block_no; ++i)
					fontinfo->SetBlock(i, TRUE);
		}
	}
	else
#endif // FONTSWITCHING
	{
#ifndef FONTSWITCHING
		for (int i = 0; i < 128; i++)
			fontinfo->SetBlock(i, TRUE);
#endif // !FONTSWITCHING

		// "If the attribute is not specified, the effect is as if a value of "U+0-10FFFF"
		//  were specified."

		// That makes it hard for us to font switch. While we could do:
		// for(int i = 0; i < 128; i++) fontinfo->SetBlock(i, TRUE);
		// it wouldn't be good. Instead we enable blocks when we see glyphs for them.
		for (int i = 0; i < (int)WritingSystem::NumScripts; ++i)
			fontinfo->SetScript((WritingSystem::Script)i);
	}
	return OpStatus::OK;
}

OP_STATUS SVGFontTraversalObject::DoContent(HTML_Element* element)
{
	OP_STATUS result = OpStatus::OK;
	Markup::Type type = element->Type();
	OpFontInfo* fontinfo = m_fontinfo;
	SVGXMLFontData* fontdata = m_fontdata;

	OP_ASSERT((fontinfo && !fontdata) || (!fontinfo && fontdata)); // Mutually exclusive

	/**
	 * "Each 'font' element must have a 'font-face' child element which describes various
	 *  characteristics of the font."
	 */

	if (type == Markup::SVGE_FONT_FACE)
	{
		if (fontinfo)
		{
			RETURN_IF_ERROR(SetupFontInfoForFontFace(element));
		}

		if (fontdata)
		{
			SVGNumber val;

			// Set this first since so many values depend on it
			if (AttrValueStore::HasObject(element, Markup::SVGA_UNITS_PER_EM, NS_IDX_SVG))
			{
				result = AttrValueStore::GetNumber(element, Markup::SVGA_UNITS_PER_EM, val);
				if (OpStatus::IsSuccess(result))
					fontdata->SetUnitsPerEm(val);
			}
#if 0 // Since this code doesn't do anything
			if(AttrValueStore::HasObject(element, Markup::SVGA_FONT_STRETCH, NS_IDX_SVG))
			{
				// Not used
			}
			if(AttrValueStore::HasObject(element, Markup::SVGA_FONT_SIZE, NS_IDX_SVG))
			{
				// Not used
			}
			if(AttrValueStore::HasObject(element, Markup::SVGA_PANOSE_1, NS_IDX_SVG))
			{
				// Not used
			}
			if(AttrValueStore::HasObject(element, Markup::SVGA_STEMV, NS_IDX_SVG))
			{
				// Not used
			}
			if(AttrValueStore::HasObject(element, Markup::SVGA_STEMH, NS_IDX_SVG))
			{
				// Not used
			}
			if(AttrValueStore::HasObject(element, Markup::SVGA_SLOPE, NS_IDX_SVG))
			{
				// Not used
			}
			if(AttrValueStore::HasObject(element, Markup::SVGA_CAP_HEIGHT, NS_IDX_SVG))
			{
				// Not used
			}
			if(AttrValueStore::HasObject(element, Markup::SVGA_X_HEIGHT, NS_IDX_SVG))
			{
				// Not used
			}
			if(AttrValueStore::HasObject(element, Markup::SVGA_ACCENT_HEIGHT, NS_IDX_SVG))
			{
				// Not used
			}
			if(AttrValueStore::HasObject(element, Markup::SVGA_WIDTHS, NS_IDX_SVG))
			{
				// Not used
			}
			if(AttrValueStore::HasObject(element, Markup::SVGA_BBOX, NS_IDX_SVG))
			{
				// Not used
			}
#endif // 0

			// Now we can set this since we have seen units_per_em (if any)
			fontdata->SetSVGFontAttribute(SVGFontData::ADVANCE_X, m_last_seen_font_advance_x);
			fontdata->SetSVGFontAttribute(SVGFontData::ADVANCE_Y, m_last_seen_font_advance_y);

			const int attrs[] = {
				Markup::SVGA_ASCENT, SVGFontData::ASCENT,
				Markup::SVGA_DESCENT, SVGFontData::DESCENT,
				Markup::SVGA_UNDERLINE_POSITION, SVGFontData::UNDERLINE_POSITION,
				Markup::SVGA_OVERLINE_POSITION, SVGFontData::OVERLINE_POSITION,
				Markup::SVGA_STRIKETHROUGH_POSITION, SVGFontData::STRIKETHROUGH_POSITION,
				Markup::SVGA_UNDERLINE_THICKNESS, SVGFontData::UNDERLINE_THICKNESS,
				Markup::SVGA_OVERLINE_THICKNESS, SVGFontData::OVERLINE_THICKNESS,
				Markup::SVGA_STRIKETHROUGH_THICKNESS, SVGFontData::STRIKETHROUGH_THICKNESS,
				Markup::SVGA_ALPHABETIC, SVGFontData::ALPHABETIC_OFFSET,
				Markup::SVGA_HANGING, SVGFontData::HANGING_OFFSET,
				Markup::SVGA_IDEOGRAPHIC, SVGFontData::IDEOGRAPHIC_OFFSET,
				Markup::SVGA_MATHEMATICAL, SVGFontData::MATHEMATICAL_OFFSET,
				Markup::SVGA_V_ALPHABETIC, SVGFontData::V_ALPHABETIC_OFFSET,
				Markup::SVGA_V_HANGING, SVGFontData::V_HANGING_OFFSET,
				Markup::SVGA_V_IDEOGRAPHIC, SVGFontData::V_IDEOGRAPHIC_OFFSET,
				Markup::SVGA_V_MATHEMATICAL, SVGFontData::V_MATHEMATICAL_OFFSET,
			};

			const unsigned attr_count = ARRAY_SIZE(attrs) / 2;

			for (unsigned i = 0; i < attr_count; i++)
			{
				Markup::AttrType svg_attr = static_cast<Markup::AttrType>(attrs[2*i]);
				if (AttrValueStore::HasObject(element, svg_attr, NS_IDX_SVG))
				{
					SVGNumber val;
					result = AttrValueStore::GetNumber(element, svg_attr, val);
					if (OpStatus::IsSuccess(result))
					{
						SVGFontData::ATTRIBUTE font_attr = static_cast<SVGFontData::ATTRIBUTE>(attrs[2*i+1]);
						fontdata->SetSVGFontAttribute(font_attr, val);
					}
				}
			}
		}
	}
	else if (type == Markup::SVGE_GLYPH || type == Markup::SVGE_MISSING_GLYPH)
	{
		const uni_char* unicode = NULL;
		const uni_char* glyphname = NULL;
		SVGArabicForm arabic_form = SVGARABICFORM_ISOLATED;
		SVGGlyphData::GlyphDirectionMask gdir_mask = SVGGlyphData::BOTH;
		SVGVector* lang_vector = NULL;

		if (type == Markup::SVGE_GLYPH)
		{
			SVGString* sval = NULL;

			AttrValueStore::GetString(element, Markup::SVGA_GLYPH_NAME, &sval);
			if (sval)
				glyphname = sval->GetString();

			SVGEnum* arabic_form_obj = NULL;
			AttrValueStore::GetEnumObject(element, Markup::SVGA_ARABIC_FORM,
										  SVGENUM_ARABIC_FORM, &arabic_form_obj);
			if (arabic_form_obj)
				arabic_form = (SVGArabicForm)arabic_form_obj->EnumValue();

			AttrValueStore::GetString(element, Markup::SVGA_ORIENTATION, &sval);
			if (sval)
			{
				if (sval->GetLength() == 1)
				{
					uni_char uc = sval->GetString()[0];
					if (uc == 'h')
						gdir_mask = SVGGlyphData::HORIZONTAL;
					else if (uc == 'v')
						gdir_mask = SVGGlyphData::VERTICAL;
				}
			}

			AttrValueStore::GetVector(element, Markup::SVGA_LANG, lang_vector);

			AttrValueStore::GetString(element, Markup::SVGA_UNICODE, &sval);
			if (sval)
			{
				unicode = sval->GetString();

				if (unicode && sval->GetLength() > 0)
				{
					m_has_glyphs = TRUE;

#ifdef FONTSWITCHING
					if (fontinfo)
					{
						int consumed = 0;
						UnicodePoint up = Unicode::GetUnicodePoint(unicode, sval->GetLength(), consumed);
						if (consumed == (int)sval->GetLength())
						{
#ifdef _GLYPHTESTING_SUPPORT_
							fontinfo->SetGlyph(up);
#endif // _GLYPHTESTING_SUPPORT_

							// Find block for unicode char and enable that block
							int	block;
							UnicodePoint block_lowest, block_highest;
							styleManager->GetUnicodeBlockInfo(up, block, block_lowest, block_highest);
							if (block != UNKNOWN_BLOCK_NUMBER)
							{
								fontinfo->SetBlock(block, TRUE);
							}

#ifdef SVG_STORE_SHAPED_GLYPH
							// Shaped glyph
							if (unicode && sval->GetLength() == 1)
							{
								uni_char shaped_glyph = GetShapedGlyph(unicode[0], arabic_form);
								if (shaped_glyph)
								{
#ifdef _GLYPHTESTING_SUPPORT_
									fontinfo->SetGlyph(shaped_glyph);
#endif // _GLYPHTESTING_SUPPORT_

									styleManager->GetUnicodeBlockInfo(shaped_glyph, block,
																	  block_lowest, block_highest);
									if (block != UNKNOWN_BLOCK_NUMBER)
									{
										fontinfo->SetBlock(block, TRUE);
									}
								}
							}
#endif // SVG_STORE_SHAPED_GLYPH
						}
						else if (!fontinfo->HasLigatures() && consumed && (int)sval->GetLength() > consumed)
						{
							fontinfo->SetHasLigatures(TRUE);
						}
					}
#endif // FONTSWITCHING
				}
			}
		}
		else // type == Markup::SVGE_MISSING_GLYPH
		{
			// Count a missing-glyph element as a glyph too
			m_has_glyphs = TRUE;
		}

		if (fontdata)
		{
			OpBpath* path = NULL;
			SVGNumberObject* advX = NULL;
			SVGNumberObject* advY = NULL;

			AttrValueStore::GetPath(element, Markup::SVGA_D, &path);
			AttrValueStore::GetNumberObject(element, Markup::SVGA_HORIZ_ADV_X, &advX);
			AttrValueStore::GetNumberObject(element, Markup::SVGA_VERT_ADV_Y, &advY);

			if ((unicode || glyphname) && type == Markup::SVGE_GLYPH)
				RETURN_IF_MEMORY_ERROR(fontdata->AddGlyph(unicode, glyphname,
														  path, advX, advY,
														  arabic_form, gdir_mask,
														  lang_vector));
			else if (type == Markup::SVGE_MISSING_GLYPH)
				RETURN_IF_ERROR(fontdata->SetMissingGlyph(path, advX, advY));
		}
	}
	else if (fontdata && type == Markup::SVGE_FONT)
	{
		SVGNumberObject* val = NULL;

		AttrValueStore::GetNumberObject(element, Markup::SVGA_HORIZ_ADV_X, &val);
		if (val)
			m_last_seen_font_advance_x = val->value;

		AttrValueStore::GetNumberObject(element, Markup::SVGA_VERT_ADV_Y, &val);
		if (val)
			m_last_seen_font_advance_y = val->value;
#if 0
		if(AttrValueStore::HasObject(element, Markup::SVGA_HORIZ_ORIGIN_X, NS_IDX_SVG))
		{
		}
		if(AttrValueStore::HasObject(element, Markup::SVGA_HORIZ_ORIGIN_Y, NS_IDX_SVG))
		{
		}
		if(AttrValueStore::HasObject(element, Markup::SVGA_VERT_ORIGIN_X, NS_IDX_SVG))
		{
		}
		if(AttrValueStore::HasObject(element, Markup::SVGA_VERT_ORIGIN_Y, NS_IDX_SVG))
		{
		}
#endif
	}
	else if (fontdata && (type == Markup::SVGE_HKERN || type == Markup::SVGE_VKERN))
	{
		SVGNumberObject* k_val = NULL;
		SVGVector *u1 = NULL;
		SVGVector *g1 = NULL;
		SVGVector *u2 = NULL;
		SVGVector *g2 = NULL;

		AttrValueStore::GetNumberObject(element, Markup::SVGA_K, &k_val);
		if (!k_val)
			// This is required, but we treat it as a soft error =>
			// skip this kern element
			return OpStatus::OK;

		AttrValueStore::GetVector(element, Markup::SVGA_U1, u1);
		AttrValueStore::GetVector(element, Markup::SVGA_U2, u2);
		AttrValueStore::GetVector(element, Markup::SVGA_G1, g1);
		AttrValueStore::GetVector(element, Markup::SVGA_G2, g2);

		// "At least one each of u1 or g1 and at least one of u2 or g2 must be provided."
		if ((!g1 && !u1) || (!g2 && !u2))
			return OpStatus::OK; // Soft error, just skip it

		result = fontdata->AddKern(type == Markup::SVGE_HKERN, g1, g2, u1, u2, k_val->value);
	}
	else if (type == Markup::SVGE_FONT_FACE_URI)
	{
		if (element->Parent() &&
			element->Parent()->IsMatchingType(Markup::SVGE_FONT_FACE_SRC, NS_SVG))
		{
			HTML_Element* target = SVGUtils::FindHrefReferredNode(m_resolver, m_doc_ctx, element);
			if (target)
			{
				SVGElementResolverStack resolver(m_resolver);
				RETURN_IF_ERROR(resolver.Push(target));

				RETURN_IF_ERROR(SVGSimpleTraverser::TraverseElement(this, target));
			}
		}
	}

	return result;
}

#endif // SVG_SUPPORT
