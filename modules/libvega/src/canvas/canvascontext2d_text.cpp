/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef CANVAS_SUPPORT
#ifdef CANVAS_TEXT_SUPPORT

#include "modules/libvega/src/canvas/canvascontext2d.h"

#include "modules/pi/OpScreenInfo.h"
#include "modules/pi/ui/OpUiInfo.h"

#include "modules/logdoc/htm_ldoc.h"
#include "modules/logdoc/htm_elm.h"

#include "modules/widgets/optextfragmentlist.h"
#include "modules/widgets/optextfragment.h"

#include "modules/svg/svg_path.h"

#include "modules/doc/frm_doc.h"

#include "modules/layout/cssprops.h"
#include "modules/layout/cascade.h"

#if defined USE_TEXTSHAPER_INTERNALLY && defined TEXTSHAPER_SUPPORT
# include "modules/display/textshaper.h"
#endif // USE_TEXTSHAPER_INTERNALLY && TEXTSHAPER_SUPPORT

class TextFragment
{
public:
	void Set(const uni_char* fragtext, unsigned fraglen)
	{
		text = fragtext;
		textlen = fraglen;
	}

	OP_STATUS ApplyTransforms(TempBuffer& textbuf, bool reversed);

	const uni_char* Text() const { return text; }
	unsigned Length() const { return textlen; } 

private:
	const uni_char* text;
	unsigned textlen;
};

OP_STATUS TextFragment::ApplyTransforms(TempBuffer& textbuf, bool reversed)
{
	textbuf.Clear();
	RETURN_IF_ERROR(textbuf.Expand(textlen));

	size_t st_len = textlen;
	text = VisualDevice::TransformText(text, textbuf.GetStorage(), st_len,
									   reversed ? TEXT_FORMAT_REVERSE_WORD : 0);
	textlen = (unsigned)st_len;
	return OpStatus::OK;
}

class CanvasCSSLineBoxTraverser
{
public:
	CanvasCSSLineBoxTraverser(CanvasContext2D::CanvasTextBaseline bline)
		: baseline_adjustment(0), baseline(bline) {}

	virtual OP_STATUS beforeTraverse(OpFont* font) { return OpStatus::OK; }
	virtual void updateBaseline(OpFont* font);
	virtual OP_STATUS handleFragment(OpFont* font, TextFragment& frag) = 0;

protected:
	int baseline_adjustment;
	CanvasContext2D::CanvasTextBaseline baseline;
};

void CanvasCSSLineBoxTraverser::updateBaseline(OpFont* font)
{
	switch (baseline)
	{
	default:
	case CanvasContext2D::CANVAS_TEXTBASELINE_TOP:
	case CanvasContext2D::CANVAS_TEXTBASELINE_HANGING: // Emulated by 'top'
		baseline_adjustment = -(int)(font->Ascent());
		break;

	case CanvasContext2D::CANVAS_TEXTBASELINE_MIDDLE:
		baseline_adjustment = -((int)font->Ascent() - (int)font->InternalLeading()) / 2;
		break;

	case CanvasContext2D::CANVAS_TEXTBASELINE_ALPHABETIC:
		baseline_adjustment = 0;
		break;

	case CanvasContext2D::CANVAS_TEXTBASELINE_IDEOGRAPHIC:
		// Slightly below the alphabetic baseline.
		baseline_adjustment = (int)font->Descent() / 8;
		break;

	case CanvasContext2D::CANVAS_TEXTBASELINE_BOTTOM:
		baseline_adjustment = (int)font->Descent();
		break;
	}
}

class ExtentCalculator : public CanvasCSSLineBoxTraverser
{
public:
	ExtentCalculator() :
		CanvasCSSLineBoxTraverser(CanvasContext2D::CANVAS_TEXTBASELINE_ALPHABETIC) {}

	virtual OP_STATUS handleFragment(OpFont* font, TextFragment& frag);

	void reset() { extent = 0.0; }
	double getExtent() const { return extent; }

private:
	double extent;
};

OP_STATUS ExtentCalculator::handleFragment(OpFont* font, TextFragment& frag)
{
	extent += font->StringWidth(frag.Text(), frag.Length());
	return OpStatus::OK;
}

class GlyphOutlineBuilder : public CanvasCSSLineBoxTraverser
{
public:
	GlyphOutlineBuilder(VEGAPath* outl,
						CanvasContext2D::CanvasTextBaseline bline)
		: CanvasCSSLineBoxTraverser(bline), voutline(outl) {}

	virtual OP_STATUS beforeTraverse(OpFont* font);
	virtual OP_STATUS handleFragment(OpFont* font, TextFragment& frag);

private:
	VEGAPath* voutline;
	VEGA_FIX gofs_x;
};

struct CanvasTextContext
{
	HLDocProfile* hld_profile;

	LayoutFixed font_size;
	int font_weight;
	int font_number;
	BOOL font_italic;
	BOOL font_smallcaps;

	enum Direction
	{
		DIR_LTR,
		DIR_RTL
	};
	Direction direction;

	bool isRTL() const { return direction == DIR_RTL; }
	void resolveFont(CSS_property_list* font_props, FontAtt& fontatt) const;
};

static CanvasTextContext::Direction CSSDirToCanvasDir(short cssdir)
{
	if (cssdir == CSS_VALUE_rtl)
		return CanvasTextContext::DIR_RTL;

	return CanvasTextContext::DIR_LTR;
}

static OP_STATUS SetupTextContext(FramesDocument* frm_doc, HTML_Element* element,
								  CanvasTextContext& textctx)
{
	int direction, font_weight, font_number;
	LayoutFixed font_size = IntToLayoutFixed(10); // Default
	BOOL font_italic, font_smallcaps;

	HLDocProfile* hld_profile = frm_doc ? frm_doc->GetHLDocProfile() : NULL;

	if (hld_profile && element && hld_profile->GetRoot() &&
		hld_profile->GetRoot()->IsAncestorOf(element))
	{
		Head prop_list;
		LayoutProperties* cascade = LayoutProperties::CreateCascade(element, prop_list, hld_profile);
		if (!cascade)
			return OpStatus::ERR_NO_MEMORY;

		const HTMLayoutProperties& props = *cascade->GetProps();

		direction = props.direction;

		if (props.decimal_font_size_constrained >= LayoutFixed(0))
			font_size = props.decimal_font_size_constrained;
		font_weight = props.font_weight;
		font_italic = props.font_italic == FONT_STYLE_ITALIC;
		font_smallcaps = props.small_caps == CSS_VALUE_small_caps;
		font_number = props.font_number;

		cascade = NULL;
		prop_list.Clear();
	}
	else
	{
		direction = (element && element->HasAttr(ATTR_DIR)) ?
			element->GetNumAttr(ATTR_DIR) : (int)CSS_VALUE_ltr;

		font_weight = 4;
		font_italic = FALSE;
		font_smallcaps = FALSE;
		font_number = -1;
	}

	textctx.hld_profile = hld_profile;

	textctx.font_size = font_size;
	textctx.font_weight = font_weight;
	textctx.font_italic = font_italic;
	textctx.font_smallcaps = font_smallcaps;
	textctx.font_number = font_number;

	textctx.direction = CSSDirToCanvasDir(direction);

	return OpStatus::OK;
}

class CanvasCSSLineBox
{
public:
	CanvasCSSLineBox(HLDocProfile* hld_profile)
	{
		frames_doc =  hld_profile ? hld_profile->GetFramesDocument() : NULL;
	}

	OP_STATUS setText(const CanvasTextContext& ctx,
					  CSS_property_list* font_props,
					  const uni_char* text);

	OP_STATUS traverse(CanvasCSSLineBoxTraverser* trav);

	void determineMaxWidthFont(double maxwidth, VEGA_FIX& hscale);

private:
	static void replaceSpaces(uni_char* text, unsigned len);

	FontAtt fontatt;
	OpTextFragmentList frag_list;
	TempBuffer buffer;
	FramesDocument* frames_doc;
};

/* static */
void CanvasCSSLineBox::replaceSpaces(uni_char* text, unsigned len)
{
	// U+0009 U+000A U+000C U+000D => U+0020
	while (len-- > 0)
	{
		if (*text == '\x09' || *text == '\x0a' ||
			*text == '\x0c' || *text == '\x0d')
			*text = ' ';

		text++;
	}
}

OP_STATUS CanvasCSSLineBox::setText(const CanvasTextContext& ctx,
									CSS_property_list* font_props,
									const uni_char* text)
{
	unsigned text_len = uni_strlen(text);
	RETURN_IF_ERROR(buffer.Expand(text_len + 1));
	buffer.Append(text);

	replaceSpaces(buffer.GetStorage(), text_len);

	ctx.resolveFont(font_props, fontatt);

	return frag_list.Update(buffer.GetStorage(), buffer.Length(),
							ctx.direction == CanvasTextContext::DIR_RTL,
							FALSE /* bidi override */,
							TRUE /* preserve spaces */,
							FALSE /* nbsp as space */,
							NULL /* frames doc. */,
							fontatt.GetFontNumber());
}

void CanvasCSSLineBox::determineMaxWidthFont(double maxwidth, VEGA_FIX& hscale)
{
	ExtentCalculator calculator;
	bool force_scale = false;
	while (true)
	{
		calculator.reset();

		RETURN_VOID_IF_ERROR(traverse(&calculator));

		double extent = calculator.getExtent();

		double diff = extent - maxwidth;
		if (diff <= 0.0)
		{
			// The text fits - done.
			hscale = VEGA_INTTOFIX(1);
			break;
		}

		double f = maxwidth / extent;

		// Is is reasonable to just apply horizontal scaling?
		if (force_scale || f >= 0.95)
		{
			// Apply scale maxw. / ext.
			hscale = DOUBLE_TO_VEGA(f);
			break;
		}

		int old_height = fontatt.GetHeight();

		// Try to approximate how much to reduce the current size
		int reduction = MAX((int)(old_height * (1.0 - f)), 1);

		// Size will reach zero => set to one and force scaling
		force_scale = (reduction >= old_height);

		fontatt.SetHeight(MAX(1, old_height - reduction));

		if (!fontatt.GetChanged())
			force_scale = true;
	}

	fontatt.ClearChanged();
}

static bool alignmentNeedsExtent(const CanvasTextContext& textctx,
								 CanvasContext2D::CanvasTextAlign align)
{
	switch (align)
	{
	default:
	case CanvasContext2D::CANVAS_TEXTALIGN_START:
		return textctx.isRTL();

	case CanvasContext2D::CANVAS_TEXTALIGN_END:
		return !textctx.isRTL();

	case CanvasContext2D::CANVAS_TEXTALIGN_LEFT:
		break;

	case CanvasContext2D::CANVAS_TEXTALIGN_RIGHT:
	case CanvasContext2D::CANVAS_TEXTALIGN_CENTER:
		return true;
	}
	return false;
}

OP_STATUS GlyphOutlineBuilder::beforeTraverse(OpFont* font)
{
	gofs_x = 0;

	SVGNumber advance;
	UINT32 curr_pos = 0;
	return font->GetOutline(NULL, 0, curr_pos, curr_pos, TRUE, advance, NULL);
}

OP_STATUS GlyphOutlineBuilder::handleFragment(OpFont* font, TextFragment& frag)
{
	const uni_char* fragtext = frag.Text();
	unsigned fraglen = frag.Length();

	UINT32 last_pos = 0, curr_pos = 0;
	SVGNumber advance;

	OP_STATUS status = OpStatus::OK;
	while (curr_pos < fraglen)
	{
		SVGPath* outline = NULL;
		UINT32 prev_pos = curr_pos;

		// Fetch glyph (uncached, slow, et.c et.c)
		status = font->GetOutline(fragtext, fraglen, curr_pos, last_pos, TRUE,
								  advance, &outline);
		if (OpStatus::IsError(status))
		{
			if (OpStatus::IsMemoryError(status) ||
				status == OpStatus::ERR_NOT_SUPPORTED)
				break;

			curr_pos++;
			continue;
		}

		last_pos = prev_pos;

		// Append glyph outline to path
		status = ConvertSVGPathToVEGAPath(outline, gofs_x, VEGA_INTTOFIX(baseline_adjustment),
										  CANVAS_FLATNESS, voutline);

		OP_DELETE(outline);

		if (OpStatus::IsError(status))
			break;

		gofs_x += advance.GetVegaNumber();
	}

	return status;
}

OP_STATUS CanvasCSSLineBox::traverse(CanvasCSSLineBoxTraverser* trav)
{
	const uni_char* text = buffer.GetStorage();

	OpFont* font = g_font_cache->GetFont(fontatt, 100, frames_doc);
	if (!font)
		return OpStatus::ERR;

	OP_STATUS status = trav->beforeTraverse(font);
	if (OpStatus::IsError(status))
	{
		g_font_cache->ReleaseFont(font);
		return status;
	}

	trav->updateBaseline(font);

	int starting_fontnumber = fontatt.GetFontNumber();

	TextFragment tfrag;
	TempBuffer buf;
	for (unsigned int frag = 0; frag < frag_list.GetNumFragments(); ++frag)
	{
		OP_TEXT_FRAGMENT* fragment = frag_list.GetByVisualOrder(frag);
		bool mirrored = !!(fragment->packed.bidi & BIDI_FRAGMENT_ANY_MIRRORED);

		fontatt.SetFontNumber(fragment->wi.GetFontNumber());

		if (fontatt.GetChanged())
		{
			g_font_cache->ReleaseFont(font);

			font = g_font_cache->GetFont(fontatt, 100, frames_doc);
			if (!font)
			{
				status = OpStatus::ERR;
				break;
			}

			fontatt.ClearChanged();

			trav->updateBaseline(font);
		}

		// Since we don't collapse whitespace, there is no need to
		// look at has_trailing_whitespace here, since any trailing
		// whitespace should be included in the fragment length
		// according to GetNextTextFragment documentation.
		if (fragment->wi.GetLength())
		{
			tfrag.Set(text + fragment->wi.GetStart(), fragment->wi.GetLength());

			status = tfrag.ApplyTransforms(buf, mirrored);
			if (OpStatus::IsError(status))
				break;

			if (tfrag.Length() > 0)
			{
				status = trav->handleFragment(font, tfrag);
				if (OpStatus::IsError(status))
					break;
			}
		}
	}

	if (font)
		g_font_cache->ReleaseFont(font);

	fontatt.SetFontNumber(starting_fontnumber);
	return status;
}

OP_STATUS CanvasContext2D::getTextOutline(const CanvasTextContext& ctx,
										  const uni_char* text,
										  double x, double y, double maxwidth,
										  VEGAPath& voutline,
										  VEGATransform& outline_transform)
{
	CanvasCSSLineBox linebox(ctx.hld_profile);
	RETURN_IF_ERROR(linebox.setText(ctx, m_current_state.font, text));

	VEGA_FIX hori_scale = VEGA_INTTOFIX(1);
	if (!op_isnan(maxwidth))
		linebox.determineMaxWidthFont(maxwidth, hori_scale);

	GlyphOutlineBuilder builder(&voutline, m_current_state.textBaseline);
	RETURN_IF_ERROR(linebox.traverse(&builder));

	outline_transform.loadTranslate(DOUBLE_TO_VEGA(x), DOUBLE_TO_VEGA(y));

	if (alignmentNeedsExtent(ctx, m_current_state.textAlign))
	{
		ExtentCalculator calculator;
		calculator.reset();

		linebox.traverse(&calculator);

		VEGA_FIX tx = VEGA_INTTOFIX((int)calculator.getExtent());

		if (m_current_state.textAlign == CANVAS_TEXTALIGN_CENTER)
			tx /= 2;

		outline_transform[2] -= tx;
	}

	outline_transform[0] = hori_scale;
	outline_transform[4] = VEGA_INTTOFIX(-1);
	return OpStatus::OK;
}

static short GetGenericFontFamily(short gen_font_value)
{
	const uni_char* family_name = CSS_GetKeywordString(gen_font_value);

	StyleManager::GenericFont gf = StyleManager::GetGenericFont(family_name);
	return styleManager->GetGenericFontNumber(gf, WritingSystem::Unknown);
}

void CanvasTextContext::resolveFont(CSS_property_list* font_props, FontAtt& fontatt) const
{
	fontatt.SetHasGetOutline(TRUE);

	fontatt.SetHeight(10);
	short fnum = GetGenericFontFamily(CSS_VALUE_sans_serif);
	fontatt.SetFontNumber(fnum != -1 ? fnum : 0);

	if (!font_props)
		return;

	FontAtt sys_font_att;
	BOOL sys_font_att_loaded = FALSE;

	CSS_decl* decl = font_props->GetFirstDecl();
	while (decl)
	{
		switch (decl->GetProperty())
		{
		case CSS_PROPERTY_font_size:
			{
#define LOAD_SYSTEM_FONT(kw) (sys_font_att_loaded = sys_font_att_loaded || g_op_ui_info->GetUICSSFont(kw, sys_font_att))

				int fsize = fontatt.GetHeight();

				if (decl->GetDeclType() == CSS_DECL_NUMBER)
				{
					/* We need to get fixed point resolved font size and then round it to
					   integer, so that the rounding rules are the same as when the font size
					   is computed in a cascade. */

					CSSLengthResolver length_resolver(hld_profile->GetVisualDevice(),
													  TRUE,
													  LayoutFixedToFloat(font_size),
													  font_size,
													  font_number,
													  hld_profile->GetFramesDocument()->RootFontSize(),
													  FALSE);

					LayoutFixed fixed_fsize = length_resolver.GetLengthInPixels(decl->GetNumberValue(0), decl->GetValueType(0));
					fsize = LayoutFixedToIntNonNegative(fixed_fsize);
				}
				else if (decl->GetDeclType() == CSS_DECL_TYPE)
				{
					short fsize_type = decl->TypeValue();
					if (fsize_type == CSS_VALUE_inherit)
						fsize = LayoutFixedToIntNonNegative(font_size);
					else if (fsize_type == CSS_VALUE_smaller)
						fsize = LayoutFixedToIntNonNegative(LayoutFixed(font_size * 0.8f));
					else if (fsize_type == CSS_VALUE_larger)
						fsize = LayoutFixedToIntNonNegative(LayoutFixed(font_size * 1.2f));
					else if (fsize_type == CSS_VALUE_use_lang_def)
						/* ignore? */;
					else if (CSS_is_fontsize_val(fsize_type))
					{
						int size = fsize_type - CSS_VALUE_xx_small;

						const WritingSystem::Script script =
#ifdef FONTSWITCHING
							hld_profile ? hld_profile->GetPreferredScript() :
#endif // FONTSWITCHING
							WritingSystem::Unknown;
						FontAtt* font = styleManager->GetStyle(HE_DOC_ROOT)->GetPresentationAttr().GetPresentationFont(script).Font;
						OP_ASSERT(font);
						fsize = styleManager->GetFontSize(font, size);
					}
					else if (CSS_is_font_system_val(fsize_type))
					{
						if (LOAD_SYSTEM_FONT(fsize_type))
							fsize = sys_font_att.GetSize();
					}
				}
				fontatt.SetHeight(fsize);
			}
			break;

		case CSS_PROPERTY_font_weight:
			{
				int fweight = 4;
				if (decl->GetDeclType() == CSS_DECL_NUMBER)
				{
					fweight = (int) decl->GetNumberValue(0);
				}
				else if (decl->GetDeclType() == CSS_DECL_TYPE)
				{
					short wtype = decl->TypeValue();
					switch (wtype)
					{
					case CSS_VALUE_normal:
						break;
					case CSS_VALUE_inherit:
						fweight = font_weight;
						break;
					case CSS_VALUE_bold:
						fweight = 7;
						break;
					case CSS_VALUE_bolder:
						fweight = font_weight + 1;
						break;
					case CSS_VALUE_lighter:
						fweight = font_weight - 1;
						break;
					default:
						if (CSS_is_font_system_val(wtype))
						{
							if (LOAD_SYSTEM_FONT(wtype))
							{
								if (sys_font_att.GetWeight() >= 1 &&
									sys_font_att.GetWeight() <= 9)
									fweight = sys_font_att.GetWeight();
							}
						}
					}
				}

				if (fweight < 1)
					fweight = 1;
				else if (fweight > 9)
					fweight = 9;

				fontatt.SetWeight(fweight);
			}
			break;

		case CSS_PROPERTY_font_variant:
			if (decl->GetDeclType() == CSS_DECL_TYPE)
			{
				short variant = decl->TypeValue();
				switch (variant)
				{
				case CSS_VALUE_normal:
					break;

				case CSS_VALUE_inherit:
					fontatt.SetSmallCaps(font_smallcaps);
					break;
				case CSS_VALUE_small_caps:
					fontatt.SetSmallCaps(TRUE);
					break;
				default:
					if (CSS_is_font_system_val(variant) &&
						LOAD_SYSTEM_FONT(variant))
						if (sys_font_att.GetSmallCaps())
							fontatt.SetSmallCaps(TRUE);
				}
			}
			break;

		case CSS_PROPERTY_font_style:
			if (decl->GetDeclType() == CSS_DECL_TYPE)
			{
				short fstyle = decl->TypeValue();
				switch (fstyle)
				{
				case CSS_VALUE_normal:
					break;

				case CSS_VALUE_inherit:
					fontatt.SetItalic(font_italic);
					break;
				case CSS_VALUE_oblique:
				case CSS_VALUE_italic:
					fontatt.SetItalic(TRUE);
					break;
				default:
					if (CSS_is_font_system_val(fstyle) &&
						LOAD_SYSTEM_FONT(fstyle))
						if (sys_font_att.GetItalic())
							fontatt.SetItalic(TRUE);
				}
			}
			break;

		case CSS_PROPERTY_font_family:
			{
				if (decl->GetDeclType() == CSS_DECL_GEN_ARRAY)
				{
					const CSS_generic_value* arr = decl->GenArrayValue();
					short arr_len = decl->ArrayValueLen();
					for (int i = 0; i < arr_len; i++)
					{
						if (arr[i].value_type == CSS_IDENT)
						{
							if (arr[i].value.type & CSS_GENERIC_FONT_FAMILY)
							{
								short fnum = GetGenericFontFamily(arr[i].value.type & CSS_GENERIC_FONT_FAMILY_mask);
								if (fnum != -1)
								{
									fontatt.SetFontNumber(fnum);
									break;
								}
							}
							else if (styleManager->HasFont(arr[i].value.type))
							{
								fontatt.SetFontNumber(arr[i].value.type);
								break;
							}
						}
						else if (arr[i].value_type == CSS_STRING_LITERAL)
						{
							short fnum = styleManager->LookupFontNumber(hld_profile, arr[i].value.string, CSS_MEDIA_TYPE_NONE/* FIXME: media_type */);
							if (fnum != -1)
							{
								fontatt.SetFontNumber(fnum);
								break;
							}
						}
					}
				}
				else if (decl->GetDeclType() == CSS_DECL_TYPE)
				{
					if (decl->TypeValue() == CSS_VALUE_inherit)
						if (font_number >= 0)
							fontatt.SetFontNumber(font_number);
					else if (decl->TypeValue() == CSS_VALUE_use_lang_def)
						;
					else
					{
						if (LOAD_SYSTEM_FONT(decl->TypeValue()))
						{
							short fnum = styleManager->GetFontNumber(sys_font_att.GetFaceName());
							if (fnum >= 0)
								fontatt.SetFontNumber(fnum);
						}
					}
				}
			}
			break;
		}

#undef LOAD_SYSTEM_FONT

		decl = decl->Suc();
	}
}

#ifdef VEGA_OPPAINTER_SUPPORT
#include "modules/libvega/src/oppainter/vegaopfont.h"
#include "modules/mdefont/processedstring.h"

class RasterTextPainter : public CanvasCSSLineBoxTraverser
{
public:
	RasterTextPainter(VEGARenderer* r, VEGAStencil* c,
					  CanvasContext2D::CanvasTextBaseline bline,
					  int x, int y)
		: CanvasCSSLineBoxTraverser(bline), renderer(r), clip(c), pos_x(x), pos_y(y) {}

	virtual void updateBaseline(OpFont* font);
	virtual OP_STATUS handleFragment(OpFont* font, TextFragment& frag);

private:
	void drawFragmentWithOutlines(OpFont* font, TextFragment& frag);

	VEGARenderer* renderer;
	VEGAStencil* clip;
	int pos_x, pos_y, ascent;
};

void RasterTextPainter::updateBaseline(OpFont* font)
{
	CanvasCSSLineBoxTraverser::updateBaseline(font);

	ascent = (int)font->Ascent();
}

void RasterTextPainter::drawFragmentWithOutlines(OpFont* font, TextFragment& frag)
{
	const uni_char* fragtext = frag.Text();
	unsigned fraglen = frag.Length();

	UINT32 last_pos = 0, curr_pos = 0;
	SVGNumber advance;
	VEGA_FIX gofs_y = VEGA_INTTOFIX(pos_y - ascent - baseline_adjustment);

	OP_STATUS status = OpStatus::OK;
	while (curr_pos < fraglen)
	{
		SVGPath* outline = NULL;
		UINT32 prev_pos = curr_pos;

		// Fetch glyph (uncached, slow, et.c et.c)
		status = font->GetOutline(fragtext, fraglen, curr_pos, last_pos, TRUE,
								  advance, &outline);
		if (OpStatus::IsError(status))
		{
			if (OpStatus::IsMemoryError(status) ||
				status == OpStatus::ERR_NOT_SUPPORTED)
				break;

			curr_pos++;
			continue;
		}

		last_pos = prev_pos;

		VEGA_FIX gofs_x = VEGA_INTTOFIX(pos_x);

		VEGAPath voutline;
		status = ConvertSVGPathToVEGAPath(outline, gofs_x, gofs_y,
										  CANVAS_FLATNESS, &voutline);

		OP_DELETE(outline);

		if (OpStatus::IsError(status))
			break;

		renderer->fillPath(&voutline, clip);

		pos_x += VEGA_FIXTOINT(advance.GetVegaNumber());
	}
}

OP_STATUS RasterTextPainter::handleFragment(OpFont* font, TextFragment& frag)
{
	if (font->Type() == OpFontInfo::SVG_WEBFONT)
	{
		// Draw with outlines
		drawFragmentWithOutlines(font, frag);
	}
	else
	{
		const uni_char* fragtext = frag.Text();
		unsigned fraglen = frag.Length();

		VEGAOpFont* vegaopfont = static_cast<VEGAOpFont*>(font);
		VEGAFont* vegafont = vegaopfont->getVegaFont();

		renderer->drawString(vegafont, pos_x, pos_y - ascent - baseline_adjustment,
							 fragtext, fraglen, 0, -1, clip);

		pos_x += font->StringWidth(fragtext, fraglen);
	}
	return OpStatus::OK;
}

void CanvasContext2D::fillRasterText(const CanvasTextContext& ctx,
									 const uni_char* text, int px, int py)
{
	CanvasCSSLineBox linebox(ctx.hld_profile);
	RETURN_VOID_IF_ERROR(linebox.setText(ctx, m_current_state.font, text));

	UINT32 col = m_current_state.fill.color;
	if (m_current_state.alpha != 0xff)
	{
		unsigned int alpha = col >> 24;
		alpha *= m_current_state.alpha;
		alpha >>= 8;
		col = (col&0xffffff) | (alpha<<24);
	}
	m_vrenderer->setColor(col);
	m_vrenderer->setRenderTarget(m_buffer);

	px += VEGA_FIXTOINT(m_current_state.transform[2]);
	py += VEGA_FIXTOINT(m_current_state.transform[5]);

	if (alignmentNeedsExtent(ctx, m_current_state.textAlign))
	{
		ExtentCalculator calculator;
		calculator.reset();

		linebox.traverse(&calculator);

		int extent = (int)calculator.getExtent();
		if (m_current_state.textAlign == CANVAS_TEXTALIGN_CENTER)
			extent /= 2;

		px -= extent;
	}

	RasterTextPainter tpainter(m_vrenderer,
							   m_current_state.clip,
							   m_current_state.textBaseline,
							   px, py);
	RETURN_VOID_IF_ERROR(linebox.traverse(&tpainter));
}
#endif // VEGA_OPPAINTER_SUPPORT

void CanvasContext2D::setFont(CSS_property_list* font, FramesDocument* frm_doc, HTML_Element* element)
{
	if (m_current_state.font)
		m_current_state.font->Unref();

	m_current_state.font = font;

	CanvasTextContext ctx;
	if (OpStatus::IsSuccess(SetupTextContext(frm_doc, element, ctx)))
	{
		FontAtt att;
		ctx.resolveFont(font, att); // Load font.
	}
}

void CanvasContext2D::fillText(FramesDocument* frm_doc, HTML_Element* element,
							   const uni_char* text,
							   double x, double y, double maxwidth)
{
	if (!m_canvas)
		return;

	CanvasTextContext ctx;
	RETURN_VOID_IF_ERROR(SetupTextContext(frm_doc, element, ctx));

#ifdef VEGA_OPPAINTER_SUPPORT
	// Fast-path check:
	// - color fill
	// - no maxwidth specified
	// - src-over composite
	// - 'simple CTM'
	// - no shadow
	// - (x, y) aligned to pixelgrid
	if (isFillColor() &&
		op_isnan(maxwidth) &&
		m_current_state.compOperation == CANVAS_COMP_SRC_OVER &&
		m_current_state.transform.isIntegralTranslation() &&
		!shouldDrawShadow())
	{
		fillRasterText(ctx, text, (int)x, (int)y);
		return;
	}
#endif // VEGA_OPPAINTER_SUPPORT

	VEGAPath outline;
	VEGATransform outline_transform;
	RETURN_VOID_IF_ERROR(getTextOutline(ctx, text, x, y, maxwidth,
										outline, outline_transform));

	VEGATransform xfrm;
	xfrm.copy(m_current_state.transform);
	xfrm.multiply(outline_transform);

	outline.transform(&xfrm);

	FillParams parms;
	parms.paint = &m_current_state.fill;

	fillPath(parms, outline);
}

void CanvasContext2D::strokeText(FramesDocument* frm_doc, HTML_Element* element,
								 const uni_char* text,
								 double x, double y, double maxwidth)
{
	if (!m_canvas)
		return;

	CanvasTextContext ctx;
	RETURN_VOID_IF_ERROR(SetupTextContext(frm_doc, element, ctx));

	VEGAPath outline;
	VEGATransform outline_transform;
	RETURN_VOID_IF_ERROR(getTextOutline(ctx, text, x, y, maxwidth,
										outline, outline_transform));

	VEGAPath stroked_outline;
	outline.setLineWidth(m_current_state.lineWidth);
	outline.setLineCap(m_current_state.lineCap);
	outline.setLineJoin(m_current_state.lineJoin);
	outline.setMiterLimit(m_current_state.miterLimit);
	outline.createOutline(&stroked_outline, CANVAS_FLATNESS, 0); // FIXME:OOM

	VEGATransform xfrm;
	xfrm.copy(m_current_state.transform);
	xfrm.multiply(outline_transform);

	stroked_outline.transform(&xfrm);

	FillParams parms;
	parms.paint = &m_current_state.stroke;

	fillPath(parms, stroked_outline);
}

OP_STATUS CanvasContext2D::measureText(FramesDocument* frm_doc, HTML_Element* element,
									   const uni_char* text,
									   double* text_extent)
{
	*text_extent = 0.0;

	if (!m_canvas)
		return OpStatus::OK;

	CanvasTextContext ctx;
	RETURN_IF_ERROR(SetupTextContext(frm_doc, element, ctx));

	CanvasCSSLineBox linebox(ctx.hld_profile);
	RETURN_IF_ERROR(linebox.setText(ctx, m_current_state.font, text));

	ExtentCalculator calculator;
	calculator.reset();

	RETURN_IF_ERROR(linebox.traverse(&calculator));

	*text_extent = calculator.getExtent();

	return OpStatus::OK;
}

#endif // CANVAS_TEXT_SUPPORT
#endif // CANVAS_SUPPORT
