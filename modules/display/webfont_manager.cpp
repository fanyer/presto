/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/display/webfont_manager.h"
#include "modules/display/woff.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/doc/frm_doc.h"
#include "modules/svg/SVGManager.h"
#include "modules/style/css_webfont.h"
#include "modules/display/check_sfnt.h"


WebFontManager::WebFontManager()
{
}

WebFontManager::~WebFontManager()
{
}

OpFont* WebFontManager::CreateFont(const uni_char* face, UINT32 size, UINT8 weight,
								   BOOL italic, BOOL must_have_getoutline,
								   INT32 blur_radius, FramesDocument* frm_doc)
{
	OP_NEW_DBG("CreateFont", "webfonts");
	FontValue* fv = NULL;
	BOOL found = FALSE;

	OP_ASSERT(face);
	if(!face)
	{
		return NULL;
	}

	int font_count = m_font_list.GetCount();
	for (int i = 0; i < font_count; i++)
	{
		fv = m_font_list.Get(i);
		if(face && fv->family_name && uni_strcmp(fv->family_name, face) == 0 && fv->AllowedInDocument(frm_doc))
		{
			found = TRUE;
			break;
		}
	}

	if(found)
	{
		BOOL smallcaps = FALSE;
		FontValue::FontVariant* variant = fv->GetFontVariant(size, weight, smallcaps, italic, frm_doc);
		if(variant)
		{
			OP_DBG((UNI_L("%dpx %s%s%s frm_doc: %p elm: %p webfontref: %p fontnumber: %d"),
				size, face, weight>4?UNI_L(" bold"):UNI_L(""), italic?UNI_L(" italic"):UNI_L(""), frm_doc,
				variant->element, variant->webfontref, fv->font_number));

			if(variant->element)
			{
#ifdef SVG_SUPPORT
				HTML_Element* element = variant->element;
				OpFont* font = NULL;
				OP_STATUS err = g_svg_manager->GetOpFont(element, size, &font);
				if(OpStatus::IsSuccess(err))
				{
					if(font && font->Type() != OpFontInfo::SVG_WEBFONT)
					{
						OP_ASSERT(!"SVG webfont instance returned invalid type.");
						OP_DELETE(font);
						font = NULL;
					}
					return font;
				}
#endif // SVG_SUPPORT
			}
			else if(variant->webfontref)
			{
				OpFontManager::OpWebFontRef ref = variant->webfontref;

				OpFont* font;
				// synthetization needed, but this has to be
				// communicated to font engine. let it create a new
				// ref if it wants to.
				// if a syntesized ref is created it will live as long
				// as the ref it was based on. the syntesized ref will
				// not be passed to OpFontManager::RemoveWebFont, it
				// should be destroyed when the orignal ref is.
				if (!variant->HasWeight(weight) || variant->is_italic != italic)
					font = styleManager->GetFontManager()->CreateFont(ref, weight, italic, size, blur_radius);
				else
					font = styleManager->GetFontManager()->CreateFont(ref, size, blur_radius);
				OP_DBG(("OpFont: %p WebFontRef: %p",
						font, ref
						));

				if(font && font->Type() != OpFontInfo::PLATFORM_WEBFONT && font->Type() != OpFontInfo::PLATFORM_LOCAL_WEBFONT)
				{
					OP_ASSERT(!"Platform webfont instance returned invalid type.");
					OP_DELETE(font);
					font = NULL;
				}

				return font;
			}
		}
	}

	OpFont* font =
		styleManager->GetFontManager()->CreateFont(face, size, weight, italic, must_have_getoutline, blur_radius);

	if(font && font->Type() != OpFontInfo::PLATFORM)
	{
		OP_ASSERT(!"Platform font instance returned invalid type.");
		OP_DELETE(font);
		font = NULL;
	}

	return font;
}

BOOL WebFontManager::AllowedInDocument(UINT32 fontnr, FramesDocument* frm_doc)
{
	FontValue* fv = GetFontValue(fontnr, frm_doc);
	// Either we don't have a webfont with this fontnumber, or we do but for another document.
	// We should allow systemfonts to be used still, even if some other document has an overriding webfont.
	return (!fv || (fv->AllowedInDocument(frm_doc) || styleManager->IsSystemFont(fontnr)));
}

BOOL WebFontManager::HasFont(UINT32 fontnr)
{
	return (GetFontValue(fontnr) != NULL);
}

BOOL WebFontManager::SupportsFormat(int format)
{
	// wOFF fonts will be converted to sfnt by the WebFontManager, so
	// when it reaches the font engine it's either truetype or
	// opentype.
	if (format == CSS_WebFont::FORMAT_WOFF)
		format = CSS_WebFont::FORMAT_TRUETYPE | CSS_WebFont::FORMAT_OPENTYPE;

	return
#ifdef SVG_SUPPORT
		(format & CSS_WebFont::FORMAT_SVG) ||
#endif // SVG_SUPPORT
		styleManager->GetFontManager()->SupportsFormat(format);
}

#if 0
// This is left here as a reminder, we may need something like this in the future, to rebuild OpFontInfo structs when faces are removed
OP_STATUS WebFontManager::GetFontInfo(UINT32 fontnr, OpFontInfo* fontinfo)
{
	OpFontInfo* our_fontinfo = NULL;
	FontValue* fv = GetFontValue(fontnr);
	if (fv)
	{
		our_fontinfo = fv->fontinfo;
	}
	else
	{
		return OpStatus::ERR;
	}

	OP_ASSERT(our_fontinfo);
	RETURN_IF_ERROR(fontinfo->SetFace(fv->family_name));
	fontinfo->SetFontNumber(styleManager->GetFontNumber(fv->family_name)));
	fontinfo->SetMonospace(our_fontinfo->Monospace());
	fontinfo->SetItalic(our_fontinfo->HasItalic());
	fontinfo->SetOblique(our_fontinfo->HasOblique());
	fontinfo->SetNormal(our_fontinfo->HasNormal());
	fontinfo->SetSmallcaps(our_fontinfo->Smallcaps());
	int i;
	for (i = 0; i < 9 ;i++)
	{
		fontinfo->SetWeight(0, our_fontinfo->HasWeight(i));
	}
	for (int block = 0; block < 128; block++)
	{
		fontinfo->SetBlock(block, our_fontinfo->HasBlock(block));
	}

	for (i = 0; i < (int)WritingSystem::NumScripts; ++i)
	{
		const WritingSystem::Script s = (WritingSystem::Script)i;
		if (our_fontinfo->HasScript(s))
			fontinfo->SetScript(s);
		fontinfo->SetScriptPreference(s, our_fontinfo->GetScriptPreference(s));
	}

#ifdef SVG_SUPPORT
	// We should copy over the glyph tables, but note that it means that all svg ligatures fail if the glyphs of the decomposed ligature are not in the font.
	fontinfo->MergeFontInfo(*our_fontinfo);
#endif // SVG_SUPPORT

	return OpStatus::OK;
}
#endif

WebFontManager::FontValue* WebFontManager::GetFontValue(UINT32 fontnr, FramesDocument* frm_doc)
{
	int font_count = m_font_list.GetCount();
	for (int i = 0; i < font_count; i++)
	{
		FontValue* fv = m_font_list.Get(i);
		if (fv->font_number == fontnr)
		{
			return fv;
		}
	}
	return NULL;
}

BOOL WebFontManager::FontValue::AllowedInDocument(FramesDocument* frm_doc)
{
	for (unsigned int i = 0; i < fontvariants.GetCount(); i++)
	{
		FontVariant* variant = fontvariants.Get(i);
		if (variant->frm_doc == frm_doc)
			return TRUE;
	}

	return FALSE;
}

OP_STATUS WebFontManager::FontValue::AddFontVariant(int size, int font_weight, BOOL smallcaps, BOOL italic, FramesDocument* frm_doc, HTML_Element* elm,
													OpFontManager::OpWebFontRef webfontref, URL url)
{
	OP_ASSERT(size > 0 || size == -1);
	OP_ASSERT(font_weight != 0 && (font_weight & ~0x3ff) == 0);
	FontVariant* variant = OP_NEW(FontVariant, (size, font_weight, smallcaps, italic, frm_doc, elm, webfontref, url));

	if (!variant)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	// The order is important.
	return fontvariants.Add(variant);
}

WebFontManager::FontValue::~FontValue()
{
	if(styleManager)
	{
		for (UINT32 i = 0; i < fontvariants.GetCount(); i++)
		{
			OP_ASSERT(m_trailing_webfont_refs.Find(&fontvariants.Get(i)->webfontref) < 0);
			if (FontVariant* variant = fontvariants.Get(i))
				if (variant->webfontref)
					styleManager->GetFontManager()->RemoveWebFont(variant->webfontref);
		}
	}

	op_free(family_name);

	while (m_trailing_webfont_refs.GetCount())
		// FIXME: OOM!
		OpStatus::Ignore(RemoveTrailingWebFontRef(0));

	OP_DELETE(original_platform_fontinfo);
}

OP_STATUS WebFontManager::FontValue::RemoveTrailingWebFontRef(int trailing_idx)
{
	OP_ASSERT(trailing_idx >= 0);
	OpFontManager::OpWebFontRef* ref = m_trailing_webfont_refs.Remove(trailing_idx);
	OP_ASSERT(ref);
	OP_STATUS status = styleManager->GetFontManager()->RemoveWebFont(*ref);
	OP_DELETE(ref);
	return status;
}

static int GetWeightDiff(int weight, int weight_bits)
{
	int weight_points[10] = { -1, 1, 2, 3, 4, 5, 41, 42, 43, 44 }; // So that we prefer bold fonts for bold fonts and normal fonts for normal fonts.
	int best_diff = INT_MAX;
	for (int i = 1; i <= 9; i++)
	{
		int this_diff = op_abs(weight_points[weight] - weight_points[i]);
		if (this_diff < best_diff)
		{
			if (((1<<i) & weight_bits) != 0)
			{
				// Has this weight
				best_diff = this_diff;
			}
		}
	}
	OP_ASSERT(best_diff != INT_MAX);
	return best_diff;
}

WebFontManager::FontValue::FontVariant* WebFontManager::FontValue::GetFontVariant(int size, int weight, BOOL smallcaps, BOOL italic, FramesDocument* frm_doc)
{
	// This is an ad-hoc algorithm to choose the best font element based
	// on http://www.w3.org/TR/REC-CSS2/fonts.html#font-selection


	//	The per-descriptor matching rules from (2) above are as follows:
	// 'font-style' is tried first. 'italic' will be satisfied if there is either a face in the UA's font database labeled with the CSS keyword 'italic' (preferred) or 'oblique'. Otherwise the values must be matched exactly or font-style will fail.
	// 'font-variant' is tried next. 'normal' matches a font not labeled as 'small-caps'; 'small-caps' matches (1) a font labeled as 'small-caps', (2) a font in which the small caps are synthesized, or (3) a font where all lowercase letters are replaced by uppercase letters. A small-caps font may be synthesized by electronically scaling uppercase letters from a normal font.
	// 'font-weight' is matched next, it will never fail. (See 'font-weight' below.)
	// 'font-size' must be matched within a UA-dependent margin of tolerance. (Typically, sizes for scalable fonts are rounded to the nearest whole pixel, while the tolerance for bitmapped fonts could be as large as 20%.) Further computations, e.g., by 'em' values in other properties, are based on the 'font-size' value that is used, not the one that is specified
	//OP_ASSERT(size > 0);
	int elm_count = fontvariants.GetCount();
	OP_ASSERT(elm_count > 0);
	// We give 16 points for matching italic, 16/2=8 for matching smallcaps, 8/2=4 for font weight and so on to guarantee that we get the order correctly.
	int* potential_font_score = OP_NEWA(int, elm_count);
	if (!potential_font_score)
	{
		// fallback
		OP_ASSERT(FALSE); // OOM, How to signal?
		return fontvariants.Get(0);
	}

	int font_no;
	for (font_no = 0; font_no < elm_count; font_no++)
	{
		potential_font_score[font_no] = 0;
	}

	int best_score = 0;
	for (font_no = 0; font_no < elm_count; font_no++)
	{
		if (fontvariants.Get(font_no)->frm_doc != frm_doc)
			continue;

		// First match by style
		if (fontvariants.Get(font_no)->is_italic == italic)
		{
			potential_font_score[font_no] += 16;
		}

		if (fontvariants.Get(font_no)->is_smallcaps == smallcaps)
		{
			potential_font_score[font_no] += 8;
		}

		best_score = MAX(best_score, potential_font_score[font_no]);
	}

	if (best_score < 16)
	{
		// We are not really allowed to return a font with this wrong unless we can synthezise it by shearing the font so that it looks italic/normal
		//return NULL;
	}

	if (best_score < 24)
	{
		// We should not return a font with this wrong unless we can synthezise it, by changing the text somehow.
		//return NULL;
	}

	// Then match by weight
//	int best_weight_found = fontelms.Get(0)->font_weight;
	int best_weight_diff = INT_MAX;
	int best_fontelm_found = 0; // dummy value, will be replaced
	for (font_no = 0; font_no < elm_count; font_no++)
	{
		if (fontvariants.Get(font_no)->frm_doc != frm_doc)
			continue;

		if (potential_font_score[font_no] == best_score)
		{
			int this_font_weight = fontvariants.Get(font_no)->font_weight;
			int this_weight_diff = GetWeightDiff(weight, this_font_weight);
			if (this_weight_diff < best_weight_diff)
			{
				best_fontelm_found = font_no;
				best_weight_diff = this_weight_diff;
			}
		}
	}

	int old_best_score = best_score; // since we update best_score in the loop
	for (font_no = 0; font_no < elm_count; font_no++)
	{
		if (fontvariants.Get(font_no)->frm_doc != frm_doc)
			continue;

		if (potential_font_score[font_no] == old_best_score)
		{
			int this_font_weight = fontvariants.Get(font_no)->font_weight;
			int this_weight_diff = GetWeightDiff(weight, this_font_weight);
			if (this_weight_diff == best_weight_diff)
			{
				potential_font_score[font_no] += 4;
				best_score = MAX(best_score, potential_font_score[font_no]);
			}
		}
	}

	// Now we try to find the best font-size with the same weight
	int best_size_found = fontvariants.Get(best_fontelm_found)->font_size;
	int best_size_diff = best_size_found == -1 ? INT_MAX : op_abs(size - best_size_found);

	old_best_score = best_score; // since we update best_score in the loop
	for (font_no = 0; font_no < elm_count; font_no++)
	{
		if (fontvariants.Get(font_no)->frm_doc != frm_doc)
			continue;

		int this_font_size = fontvariants.Get(font_no)->font_size;
		if (potential_font_score[font_no] == old_best_score)
		{
			int size_diff = this_font_size == -1 ? INT_MAX : op_abs(size - this_font_size);
			// It must be better to match, since if two similar
			// fonts exists the first declared should be used.
			if (size_diff < best_size_diff)
			{
				best_size_diff = size_diff;
				best_fontelm_found = font_no;
			}
		}
	}
	OP_DELETEA(potential_font_score);
	// Should only pick font variant from the correct document
	OP_ASSERT(fontvariants.Get(best_fontelm_found)->frm_doc == frm_doc);
	return fontvariants.Get(best_fontelm_found);
}

static
void MergeWebFontInfo(OpFontInfo& platform_webfont_info, OpFontInfo* out_combined_fontinfo)
{
	for (int block = 0; block < 128; block++)
	{
		// FIXME: unicode-range clipping.
		// We want the @font-face rule for 'unicode-range' to clip the blocks in the font.
		// We need the blocks specified in the font to be in the OpFontInfo
		// for fontswitching to work.
		// However, until style handles 'unicode-range' we must set this to whatever the
		// fontfile specified.
		//if(!pinfo.HasBlock(block) && fontinfo.HasBlock(block))
		//	fontinfo->SetBlock(block, pinfo.HasBlock(block));

		if(platform_webfont_info.HasBlock(block))
			out_combined_fontinfo->SetBlock(block, platform_webfont_info.HasBlock(block));
	}

	for (int i = 0; i < (int)WritingSystem::NumScripts; ++i)
	{
		const WritingSystem::Script s = (WritingSystem::Script)i;
		if (platform_webfont_info.HasScript(s))
			out_combined_fontinfo->SetScript(s);
		out_combined_fontinfo->SetScriptPreference(s, platform_webfont_info.GetScriptPreference(s));
	}
#ifdef _GLYPHTESTING_SUPPORT_
	styleManager->SetupFontScriptsFromGlyphs(out_combined_fontinfo);
#endif
}

static
OP_STATUS GetWebFontInfo(OpFontManager::OpWebFontRef webfontref, OpFontInfo* fontinfo)
{
	// Get information from platform about the webfont, then mask the unicode ranges against what was specified in @font-face
	OpFontInfo pinfo;
	RETURN_IF_ERROR( styleManager->GetFontManager()->GetWebFontInfo(webfontref, &pinfo) );
	MergeWebFontInfo(pinfo, fontinfo);
	return OpStatus::OK;
}

OP_STATUS WebFontManager::AddLocalFont(FramesDocument* frm_doc, OpFontInfo* fontinfo, OpFontManager::OpWebFontRef localfont)
{
	if(!fontinfo)
		return OpStatus::ERR_NULL_POINTER;

	OP_ASSERT(fontinfo->Type() == OpFontInfo::PLATFORM_LOCAL_WEBFONT);

	// The following is necessary for local fonts when the font-family was not one of the ones installed on the system
	BOOL add_to_fontdb = (styleManager->GetFontInfo(fontinfo->GetFontNumber()) == NULL);
	if(add_to_fontdb)
		RETURN_IF_ERROR(styleManager->AddWebFontInfo(fontinfo));

	fontinfo->SetWebFont(localfont);

	RETURN_IF_ERROR( GetWebFontInfo(localfont, fontinfo) );

	URL url;
	return AddWebFontInternal(frm_doc, 0, url, localfont, fontinfo, add_to_fontdb);
}

OP_STATUS WebFontManager::GetWebFontFile(URL& resolved_url, OpString& fontfile)
{
	unsigned long err = resolved_url.PrepareForViewing(TRUE, TRUE, TRUE, TRUE);
	if (err)
		return err == MSG_OOM_CLEANUP ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR;

# if defined _LOCALHOST_SUPPORT_ || !defined RAMCACHE_ONLY
	TRAPD(stat, resolved_url.GetAttributeL(URL::KFilePathName_L, fontfile, TRUE));
# endif // _LOCALHOST_SUPPORT_ || !RAMCACHE_ONLY

	if (fontfile.IsEmpty())
		return OpStatus::ERR;

#ifdef USE_ZLIB
	// wOFF files need to be converted to sfnt format before using
	const OP_BOOLEAN is_woff = IswOFF(fontfile);
	RETURN_IF_ERROR(is_woff);
	if (is_woff == OpBoolean::IS_TRUE)
	{
		// get the sfnt respresentation of the font
		OpAutoPtr<OpFile> sfnt_file(resolved_url.OpenAssociatedFile(URL::ConvertedwOFFFont));

		// no sfnt version present - generate
		if (!sfnt_file.get())
		{
			// create associated file
			sfnt_file.reset(resolved_url.CreateAssociatedFile(URL::ConvertedwOFFFont));
			if (!sfnt_file.get())
				return OpStatus::ERR_NO_MEMORY;

			// convert wOFF file to sfnt format
			sfnt_file->Close();
			RETURN_IF_ERROR(wOFF2sfnt(fontfile, sfnt_file->GetFullPath()));
		}

		// use sfnt representation
		OP_ASSERT(sfnt_file.get());
		RETURN_IF_ERROR(fontfile.Set(sfnt_file->GetFullPath()));
	}
#endif // USE_ZLIB

	return OpStatus::OK;
}

#ifdef OPERA_CONSOLE
static OP_STATUS PostMessage(const OpConsoleEngine::Message *msg)
{
	TRAPD(status, g_console->PostMessageL(msg));
	return status;
}
#endif // OPERA_CONSOLE

OP_STATUS WebFontManager::AddCSSWebFont(URL resolved_url, FramesDocument* frm_doc, HTML_Element* fontelm, OpFontInfo* fontinfo)
{
	if(!fontinfo)
		return OpStatus::ERR_NULL_POINTER;

	UINT32 fontnumber = fontinfo->GetFontNumber();
	OpFontManager::OpWebFontRef webfontref = 0;

	OP_STATUS status = OpStatus::OK;
	if(fontinfo->Type() != OpFontInfo::SVG_WEBFONT)
	{
		OpString fontfile;
		RETURN_IF_ERROR(GetWebFontFile(resolved_url, fontfile));

		status = CheckSfnt(fontfile.CStr(), &resolved_url, frm_doc);
		if(OpStatus::IsError(status))
		{
			return status;
		}

		status = styleManager->GetFontManager()->AddWebFont(webfontref, fontfile.CStr());

		if(OpStatus::IsSuccess(status))
		{
			fontinfo->SetWebFont(webfontref);
			status = GetWebFontInfo(webfontref, fontinfo);

			if (OpStatus::IsError(status))
			{
				OpStatus::Ignore(styleManager->GetFontManager()->RemoveWebFont(webfontref));
			}
		}
	}
#ifdef SVG_SUPPORT
	else if (!resolved_url.IsEmpty()) //if (fontinfo->Type() == OpFontInfo::SVG)
	{
		// We only need to do this when an @font-face svgfont rule was given,
		// not when a pure markup svg-font was specified since in that case they will be the same.
		OpFontInfo* svg_fontinfo;
		status = g_svg_manager->GetOpFontInfo(fontelm, &svg_fontinfo);
		if (OpStatus::IsSuccess(status))
		{
			MergeWebFontInfo(*svg_fontinfo, fontinfo);
			fontinfo->MergeFontInfo(*svg_fontinfo);
			OP_DELETE(svg_fontinfo);
		}
	}
#endif // SVG_SUPPORT

	// failures may be OOM, missing disk support, unsupported font formats etc
	if(OpStatus::IsError(status))
	{
#ifdef OPERA_CONSOLE
		if (g_console && g_console->IsLogging())
		{
			OpConsoleEngine::Severity severity = OpConsoleEngine::Information;
			OpConsoleEngine::Source source = OpConsoleEngine::Internal;

			OpConsoleEngine::Message cmessage(source, severity);

			// Set the window id.
			if (frm_doc && frm_doc->GetWindow())
				cmessage.window = frm_doc->GetWindow()->Id();

			RETURN_IF_ERROR(cmessage.message.Set(UNI_L("Unable to use webfont")));
			RETURN_IF_ERROR(resolved_url.GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, 0, cmessage.url));
			RETURN_IF_ERROR(PostMessage(&cmessage));
	    }
#endif // OPERA_CONSOLE

		return status;
	}

	BOOL added_to_fontdb = FALSE;

	// add the font to the font database if we don't already have an entry for the fontnumber
	if(styleManager->GetFontInfo(fontnumber) == NULL)
	{
		status = styleManager->AddWebFontInfo(fontinfo);
		if(OpStatus::IsError(status))
		{
			return status;
		}

		added_to_fontdb = TRUE;
	}

	status = AddWebFontInternal(frm_doc, fontelm, resolved_url, webfontref, fontinfo, added_to_fontdb);

	if(added_to_fontdb && OpStatus::IsError(status))
	{
		styleManager->RemoveWebFontInfo(fontinfo);
	}

	return status;
}

BOOL WebFontManager::FontValue::RemoveFontVariants(FramesDocument* frm_doc, HTML_Element* fontelm, URL url)
{
	OP_NEW_DBG("RemoveFontVariants", "webfonts");
	unsigned int i = 0;
	FontVariant* variant = NULL;
	while(i < fontvariants.GetCount())
	{
		variant = fontvariants.Get(i);

		if (variant->frm_doc == frm_doc &&
			((variant->element && variant->element == fontelm) ||
			 (!variant->element && variant->url == url)))
		{
			if (styleManager)
			{
				BOOL has_instance = g_font_cache->HasCachedInstance(font_number);
				OP_DBG(("(%d) exists in fontcache: %s", font_number, has_instance ? "yes" : "no"));

				if (has_instance)
					has_instance = !g_font_cache->PurgeWebFont(font_number);

				//OP_ASSERT(m_trailing_webfont_refs.Find(&variant->webfontref) < 0);
				// font is not in use, so safe to remove it from font manager

				RebuildFontInfo(variant);

				if(variant->webfontref)
				{
					if (!has_instance)
					{
						styleManager->GetFontManager()->RemoveWebFont(variant->webfontref);
						OpFontInfo* fi = styleManager->GetFontInfo(font_number);
						if (fi)
							fi->SetWebFont(0);
					}
					// font is still in use, store in a vector for later removal
					else
					{
						OP_STATUS status;
						OpFontManager::OpWebFontRef* ref = OP_NEW(OpFontManager::OpWebFontRef, ());
						if (ref)
						{
							*ref = variant->webfontref;
							status = m_trailing_webfont_refs.Insert(m_trailing_webfont_refs.GetCount(), ref);
						}
						else
							status = OpStatus::ERR_NO_MEMORY;
						// FIXME: OOM!
						OpStatus::Ignore(status);
					}
				}

#ifdef _DEBUG
				OP_DBG(("(%d) still exists in fontcache: %s", font_number, has_instance ? "yes" : "no"));
#endif // _DEBUG

			}

			fontvariants.Delete(i);
		}
		else
		{
			i++;
		}
	}

	return (fontvariants.GetCount() == 0);
}

BOOL WebFontManager::IsBeingDeleted(UINT32 fontnr, FramesDocument* frm_doc)
{
	// Do we need the framesdoc here?
	FontValue* fv = GetFontValue(fontnr, frm_doc);
	if(fv)
	{
		return fv->IsBeingDeleted();
	}

	return FALSE;
}

void WebFontManager::RemoveCSSWebFont(FramesDocument* frm_doc, HTML_Element* font_elm, URL resolved_url)
{
	OP_NEW_DBG("RemoveCSSWebFont", "webfonts");

	unsigned int i = 0;

	while(i < m_font_list.GetCount())
	{
		FontValue* fv = m_font_list.Get(i);

		if(fv->RemoveFontVariants(frm_doc, font_elm, resolved_url))
		{
			// note, order is important
			BOOL has_instance = g_font_cache->HasCachedInstance(fv->font_number);

			OP_DBG(("Deleting fontvalue corresponding to fontnumber: %d (%s)",
					fv->font_number,
					has_instance ? "has instance" : "not in cache"));

			int font_number = fv->font_number;
			m_font_list.Delete(i); // fv no longer valid
#ifdef _DEBUG
			fv = NULL; // to catch any unintentional uses of fv after this point
#endif // _DEBUG

			OpFontInfo* fi = styleManager->GetFontInfo(font_number);
			if(fi)
				OpStatus::Ignore(styleManager->DeleteWebFontInfo(fi, !has_instance));
		}
		else
		{
			i++;
		}
	}
}

OP_STATUS WebFontManager::AddWebFontInternal(FramesDocument* frm_doc, HTML_Element* font_elm, URL resolved_url,
											 OpFontManager::OpWebFontRef webfontref,
											 OpFontInfo* fontinfo, BOOL added_to_fontdb)
{
	OP_ASSERT(fontinfo);

	UINT32 font_number = fontinfo->GetFontNumber();

	// Mark existing cached font instances with the same fontnumber as dirty,
	// to make overloading of platform fonts work properly. See CORE-27257.
	if (g_font_cache->HasCachedInstance(font_number))
	{
		g_font_cache->PurgeWebFont(font_number);
	}

	FontValue* existant_fv = GetFontValue(font_number, frm_doc);

	int parsed_font_size = fontinfo->ParsedFontSize();
	int font_weight = 0;
	BOOL is_smallcaps = fontinfo->Smallcaps();
	BOOL is_italic = fontinfo->HasItalic();
	for(int i = 0; i < 10; i++)
	{
		if(fontinfo->HasWeight(i))
		{
			font_weight |= 1 << i;
			break;
		}
	}
	if(font_weight == 0)
	{
		font_weight = 1 << 4; // normal
	}

	if (existant_fv)
	{
		// There is already a font with that number in the database
		// Someone is declaring the same font several times, maybe
		// with different glyphs or font-sizes. We have to merge this
		// data with the current font.  It can also happen with
		// external SVG Fonts.

		// This can also happen if we parse fonts twice without
		// clearing the font database in between
		OP_STATUS status = existant_fv->AddFontVariant(parsed_font_size, font_weight, is_smallcaps, is_italic, frm_doc, font_elm,
													   webfontref, resolved_url);
		if (OpStatus::IsSuccess(status))
		{
#ifdef _GLYPHTESTING_SUPPORT_
			// force a glyphmask update, since the webfontref may change later on and we need it because of the way OpFontManager::UpdateGlyphMask works
			if(webfontref)
				fontinfo->UpdateGlyphTableIfNeeded();
#endif // _GLYPHTESTING_SUPPORT_

			// We merge the glyph tables
			OpFontInfo* existing_fontinfo = styleManager->GetFontInfo(font_number);
			existing_fontinfo->MergeFontInfo(*fontinfo);

			// FIXME: We may need to keep the fontinfo around for regenerating the merged (font-family) fontinfo if some faces are removed later on
			OP_DELETE(fontinfo);
		}

		return status;
	}

	FontValue* fv = OP_NEW(FontValue, (font_number));
	if(!fv)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS status = fv->AddFontVariant(parsed_font_size, font_weight, is_smallcaps, is_italic, frm_doc, font_elm, webfontref, resolved_url);
	if (OpStatus::IsSuccess(status))
	{
		fv->family_name = uni_strdup(fontinfo->GetFace());
		if(!fv->family_name)
		{
			status = OpStatus::ERR_NO_MEMORY;
		}
		else
		{
			status = m_font_list.Add(fv);
			if (OpStatus::IsSuccess(status))
			{
				// This case happens when a platform font family is "overridden" by a webfont
				if (!added_to_fontdb)
				{
#ifdef _GLYPHTESTING_SUPPORT_
					// force a glyphmask update, since the webfontref may change later on and we need it because of the way OpFontManager::UpdateGlyphMask works
					if(webfontref)
						fontinfo->UpdateGlyphTableIfNeeded();
#endif // _GLYPHTESTING_SUPPORT_

					// We merge the glyph tables
					OpFontInfo* existing_fontinfo = styleManager->GetFontInfo(font_number);

					status = fv->SetOriginalFontInfo(existing_fontinfo); // save it for later restore
					if (OpStatus::IsError(status))
					{
						OP_DELETE(fontinfo);
						OP_DELETE(fv);
						return status;
					}

					existing_fontinfo->MergeFontInfo(*fontinfo);

					OP_DELETE(fontinfo);
				}

				return OpStatus::OK;
			}
		}
	}

	OP_ASSERT(status == OpStatus::ERR_NO_MEMORY); // The only possible error
	OP_DELETE(fv);
	return status;
}

static OP_STATUS CopyFontInfo(OpFontInfo* dst, OpFontInfo* src)
{
	RETURN_IF_ERROR(dst->SetFace(src->GetFace()));
	dst->SetFontNumber(src->GetFontNumber());
	dst->SetMonospace(src->Monospace());
	dst->SetItalic(src->HasItalic());
	dst->SetOblique(src->HasOblique());
	dst->SetNormal(src->HasNormal());
	dst->SetSmallcaps(src->Smallcaps());
	dst->SetHasLigatures(src->HasLigatures());
	int i;
	for (i = 0; i < 9 ;i++)
	{
		dst->SetWeight(0, src->HasWeight(i));
	}
	
	for (int block = 0; block < 128; block++)
	{
		dst->SetBlock(block, src->HasBlock(block));
	}

	for (i = 0; i < (int)WritingSystem::NumScripts; ++i)
	{
		const WritingSystem::Script s = (WritingSystem::Script)i;
		if (src->HasScript(s))
			dst->SetScript(s);
		dst->SetScriptPreference(s, src->GetScriptPreference(s));
	}

	return OpStatus::OK;
}

OP_STATUS WebFontManager::FontValue::SetOriginalFontInfo(OpFontInfo* in_original)
{
	if (original_platform_fontinfo)
	{
		OP_DELETE(original_platform_fontinfo);
		original_platform_fontinfo = NULL;
	}

	OpFontInfo* fontinfo = OP_NEW(OpFontInfo, ());
	if (!fontinfo)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = CopyFontInfo(fontinfo, in_original);
	if (OpStatus::IsError(status))
		OP_DELETE(fontinfo);
	else
		original_platform_fontinfo = fontinfo;

	return status;
}

OP_STATUS WebFontManager::FontValue::RebuildFontInfo(FontValue::FontVariant* removed_face)
{
	OP_STATUS status = OpStatus::OK;
	OpFontInfo* current = styleManager->GetFontInfo(font_number);
	if (current)
	{
		// if this was a composite family (systemfont with additional overriding webfont faces)
		// we should restore the opfontinfo that was provided by the OpFontDatabase
		if (styleManager->IsSystemFont(font_number) && original_platform_fontinfo &&
			fontvariants.GetCount() == 1)
		{
#ifdef _GLYPHTESTING_SUPPORT_
			current->InvalidateGlyphTable(); // let it be recreated next time it's needed
#endif // _GLYPHTESTING_SUPPORT_
			CopyFontInfo(current, original_platform_fontinfo);
		}
	}

	return status;
}

