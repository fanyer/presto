/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Anders Karlsson (pugo)
 */

#include "core/pch.h"

#ifdef VEGA_POSTSCRIPT_PRINTING

#include "modules/util/opfile/opfile.h"
#include "modules/libvega/src/postscript/postscriptprolog.h"
#include "modules/libvega/src/postscript/truetypeconverter.h"
#include "modules/libvega/src/oppainter/vegaopfont.h"


PostScriptProlog::PostScriptProlog() 
	: font_count(0)
	, current_handled_font(NULL)
	, current_opfont(NULL)
	, current_font_data(NULL)
	, current_font_data_size(0)
{
}


PostScriptProlog::~PostScriptProlog()
{
	if (current_opfont && current_font_data )
	{
		current_opfont->ReleaseFontData(current_font_data);
		g_font_cache->ReleaseFont(current_opfont);
	}
}


PostScriptProlog::HandledFont::~HandledFont()
{
	if (font_converter)
		OP_DELETE(font_converter);
}


// Init function mainly outputs a large chunk of constant PostScript prolog data
OP_STATUS PostScriptProlog::Init(OpFileDescriptor& file, PostScriptOutputMetrics* screen_metrics, PostScriptOutputMetrics* paper_metrics)
{
	this->file = &file;
	this->screen_metrics = screen_metrics;
	this->paper_metrics = paper_metrics;

	RETURN_IF_ERROR(this->file->Open(OPFILE_WRITE));

	// Initial header block.
	RETURN_IF_ERROR(addCommand("%!PS-Adobe-3.0\n"
							   "%%Creator: Opera\n"
							   "%%BoundingBox: (atend)\n"
							   "%%LanguageLevel: 3\n"
							   "%%DocumentData: Clean7Bit\n"
							   "%%Pages: (atend)\n"
							   "%%Orientation: (atend)\n"
							   "%%PageOrder: Ascend\n"
							   "%%EndComments\n"
							   "%%BeginProlog"));

	// Small optimizations of common commands. 
	RETURN_IF_ERROR(addCommand("/mt { moveto } bind def\n"
							   "/lt { lineto } bind def\n"
							   "/sc { setrgbcolor } bind def\n"
							   "/rf { rectfill } bind def\n"
							   "/rs { rectstroke } bind def\n"));

	// Create an italic version of a non italic font by slanting it. Slant is done by changing FontMatrix.
	RETURN_IF_ERROR(addCommand("/make_slanted_font_copy { % fontname newname --\n"
							   "   /newname exch def\n"
							   "   /fontdict exch findfont def\n"
							   "   /newfont fontdict maxlength dict def\n"
							   "   fontdict { exch dup /FID eq\n"
							   "               { pop pop }\n"
							   "               { exch newfont 3 1 roll put } ifelse\n"
							   "            } forall\n"
							   "   newfont /FontName newname put\n"
							   "   newfont /FontMatrix [1 0 0.2 1 0 0] put\n"
							   "   newname newfont definefont pop\n"
							   "} def\n"));

	// Every image needs an image Type3 dictionary with two image Type1 dictionaries,
	// one for image and one for mask. Instead of writing such a dictionary for each
	// image in the output we call this function with just source width and source
	// height as input. It will then generate the dictionaries for us.
	RETURN_IF_ERROR(addCommand("/GenerateImageDict { % srcw srch -- type 3 image dict\n"
							   "   /srch exch def\n"
							   "   /srcw exch def\n"
							   "   /ImageMatrix [srcw 0 0 srch neg 0 srch] def\n"
							   "   /ImageDataDict <<\n"
							   "      /ImageType 1\n"
							   "      /Width srcw /Height srch\n"
							   "      /BitsPerComponent 8\n"
							   "      /Decode [0 1 0 1 0 1]\n"
							   "      /ImageMatrix ImageMatrix\n"
							   "      /DataSource currentfile /ASCII85Decode filter\n"
							   "   >> def\n"
							   "   /ImageMaskDict <<\n"
							   "      /ImageType 1\n"
							   "      /Width srcw /Height srch\n"
							   "      /BitsPerComponent 8\n"
							   "      /Decode [1 0]\n"
							   "      /ImageMatrix ImageMatrix\n"
							   "   >> def\n"
							   "   <<\n"
							   "      /ImageType 3 /InterleaveType 1\n"
							   "      /MaskDict ImageMaskDict\n"
							   "      /DataDict ImageDataDict\n"
							   "   >>\n"
							   "} bind def\n"));

	// Ellipses in PostScript are done by scaling the graphics contect to wanted size 
	// (horizontally and vertically) and then draw a normal circle (360 degree arc) with size 1.
	RETURN_IF_ERROR(addCommand("/ellipse { % x y xrad yrad dofill --\n"
							   "    gsave newpath\n"
							   "        5 1 roll           % rotate dofill to last\n"
							   "        4 2 roll translate % translate to (x, y)\n"
							   "        2 copy scale       % scale to xrad, yrad\n"
							   "        0 0 1 0 360 arc\n"
							   "        1 exch div exch 1 exch div scale\n"
							   "        closepath {fill} {stroke} ifelse\n"
							   "    grestore\n"
							   "} bind def\n"));

	// Print strings for a list of [x y /glyphname] or [x y <charcode>] lists
	RETURN_IF_ERROR(addCommand("/opshow {{ aload pop 3 -2 roll moveto dup type /nametype eq {glyphshow} {show} ifelse } forall} bind def\n"));

	// Definitions of our page metrics and functions that does translation and scaling to allow
	// usage of Opera pixel coordinates in the PostScript documents. 
	RETURN_IF_ERROR(addFormattedCommand("/paperwidth %d def /paperheight %d def", paper_metrics->width, paper_metrics->height));
	RETURN_IF_ERROR(addFormattedCommand("/operawidth %d def /operaheight %d def", screen_metrics->width, screen_metrics->height));
	RETURN_IF_ERROR(addFormattedCommand("/margleft %d def /margtop %d def", paper_metrics->offset_left, paper_metrics->offset_top));	
	RETURN_IF_ERROR(addFormattedCommand("/margright %d def /margbottom %d def", paper_metrics->offset_right, paper_metrics->offset_bottom));	
	RETURN_IF_ERROR(addCommand("/scalescreen { paperwidth margleft margright add sub operawidth div paperheight margtop margbottom add sub operaheight div scale } bind def"));
	RETURN_IF_ERROR(addCommand("/translatescreen { margleft margbottom translate } bind def"));
	RETURN_IF_ERROR(addCommand("/changefont { exch findfont exch scalefont setfont } def"));
	RETURN_IF_ERROR(addCommand("%%EndProlog\n\n"));

	RETURN_IF_ERROR(addCommand("%%BeginSetup"));

	document_supplied_resources.Set("");

	return OpStatus::OK;
}


OP_STATUS PostScriptProlog::finish()
{
	OpHashIterator* handled_iter = handled_fonts.GetIterator();
	OpAutoPtr<OpHashIterator> handled_iter_ap(handled_iter);

	if (handled_iter)
	{
		OP_STATUS ret = handled_iter->First();
        while (ret == OpStatus::OK)
        {
			HandledFont* handled_font = static_cast<HandledFont*>(handled_iter->GetData());

			RETURN_IF_ERROR(generateFontResource(handled_font->font_converter, handled_font->ps_fontname));
			RETURN_IF_ERROR(addSuppliedResource(handled_font->ps_fontname));

			ret = handled_iter->Next();
		}
	}

	if (document_supplied_resources.HasContent() )
		RETURN_IF_ERROR(write(document_supplied_resources.CStr()));

	if (document_slanted_fonts.HasContent() )
		RETURN_IF_ERROR(write(document_slanted_fonts.CStr()));

	RETURN_IF_ERROR(addCommand("%%EndSetup"));
	return file->Close();
}


OP_STATUS PostScriptProlog::addFont(OpFont* font, const char*& ps_fontname)
{
	UINT8* font_data;
	UINT32 font_data_size;
	OP_STATUS status = font->GetFontData(font_data, font_data_size);
	if (OpStatus::IsSuccess(status))
	{
		status = addTrueTypeFont(font, font_data, font_data_size, ps_fontname);
		if (OpStatus::IsError(status))
			font->ReleaseFontData(font_data);
	}
	return status;
}


OP_STATUS PostScriptProlog::addTrueTypeFont(OpFont* font, UINT8* font_data, UINT32 font_data_size, const char*& ps_fontname)
{
	OpAutoPtr<TrueTypeConverter> font_converter_ptr (OP_NEW(TrueTypeConverter, ()));
	if (!font_converter_ptr.get())
		return OpStatus::ERR_NO_MEMORY;

	// Create a TrueType converter and init it with font data to get glyph table checksum.
	RETURN_IF_ERROR(font_converter_ptr->Init(font_data, font_data_size));
	UINT32 checksum = font_converter_ptr->getGLYFChecksum();
	bool glyphs_are_italic = font_converter_ptr->isItalic();

	HandledFont* handled_font;
	if (handled_fonts.Contains(checksum))
	{
		// Font already handled, return cached font name
		RETURN_IF_ERROR(handled_fonts.GetData(checksum, &handled_font));
	}
	else
	{
		OpString8 font_name;
		RETURN_IF_ERROR(font_name.AppendFormat("OperaFont%d", font_count++));

		TrueTypeConverter* font_converter = font_converter_ptr.release();
		RETURN_IF_ERROR(addHandledFont(font_converter, checksum, font_name, handled_font));
	}
	ps_fontname = handled_font->ps_fontname.CStr();

	// Free data from previously used font if one exists
	if (current_opfont && current_font_data )
	{
		current_opfont->ReleaseFontData(current_font_data);
		g_font_cache->ReleaseFont(current_opfont);
	}

	g_font_cache->ReferenceFont(font);

	current_handled_font = handled_font;
	current_opfont = font;
	current_font_data = font_data;
	current_font_data_size = font_data_size;

	// Add a manually slanted copy of the font if needed (mainly done by PostScript code
	// cloning the font and setting a different font transformation matrix).
	if (checkNeedsManualSlant(font, glyphs_are_italic))
	{
		// if this fails ps_fontname will not be updated, which should
		// mean that the non-italic version of the font is used
		OP_STATUS status = getSlantedFontCopy(handled_font, ps_fontname);
		OpStatus::Ignore(status);
	}

	return OpStatus::OK;
}


OP_STATUS PostScriptProlog::addHandledFont(TrueTypeConverter* font_converter, UINT32 checksum, OpString8& font_name, HandledFont*& handled_font)
{
	OP_STATUS result = font_converter->parseFont();
	if (OpStatus::IsError(result))
	{
		OP_DELETE(font_converter);
		return result;
	}

	HandledFont* _handled_font = OP_NEW(HandledFont, (checksum, font_converter, font_name));
	if (!_handled_font)
	{
		OP_DELETE(font_converter);
		return OpStatus::ERR_NO_MEMORY;
	}

	result = handled_fonts.Add(checksum, _handled_font);
	if (OpStatus::IsError(result))
	{
		OP_DELETE(_handled_font); // font_converter now owned and deleted by handled_font
		return result;
	}

	handled_font = _handled_font;
	return OpStatus::OK;
}


OP_STATUS PostScriptProlog::addSuppliedResource(OpString8& resource_name)
{
	// Add font name to supplied resources string in PostScript output
	if (document_supplied_resources.IsEmpty())
		return document_supplied_resources.AppendFormat("%%%%DocumentSuppliedResources: font %s\n", resource_name.CStr());
	else
		return document_supplied_resources.AppendFormat("%%%%+ font %s\n", resource_name.CStr());
}


bool PostScriptProlog::checkNeedsManualSlant(OpFont* font, bool glyphs_are_italic)
{
	VEGAOpFont* vf = static_cast<VEGAOpFont*>(font);
	if (vf->getVegaFont()->isItalic() && !glyphs_are_italic)
		return true;
	return false;
}


OP_STATUS PostScriptProlog::getSlantedFontCopy(HandledFont* handled_font, const char*& slanted_fontname)
{
	if (!handled_font->has_slanted_copy)
	{
		RETURN_IF_ERROR(handled_font->ps_fontname_slanted.AppendFormat("%s-slanted", handled_font->ps_fontname.CStr()));
		RETURN_IF_ERROR(document_slanted_fonts.AppendFormat("/%s /%s make_slanted_font_copy\n", handled_font->ps_fontname.CStr(), handled_font->ps_fontname_slanted.CStr()));
		handled_font->has_slanted_copy = true;
	}

	slanted_fontname = handled_font->ps_fontname_slanted.CStr();
	return OpStatus::OK;
}


OP_STATUS PostScriptProlog::generateFontResource(TrueTypeConverter* converter, OpString8& ps_fontname)
{
	return converter->generatePostScript(*this, ps_fontname);
}


TrueTypeConverter* PostScriptProlog::getCurrentFontConverter()
{
	if (!current_handled_font)
		return NULL;

	return current_handled_font->font_converter;
}


void PostScriptProlog::getCurrentFontData(UINT8*& font_data, UINT32& font_data_size)
{
	font_data = current_font_data;
	font_data_size = current_font_data_size;
}


#endif // VEGA_POSTSCRIPT_PRINTING
