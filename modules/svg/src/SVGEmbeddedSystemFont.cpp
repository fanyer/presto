/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"

#ifdef SVG_SUPPORT_EMBEDDED_FONTS

#include "modules/display/styl_man.h"
#include "modules/display/FontAtt.h"

#include "modules/svg/src/SVGEmbeddedSystemFont.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/SVGUtils.h"

#define CMD_TYPE_MOVE_TO            0x0
#define CMD_TYPE_LINE_TO            0x1
#define CMD_TYPE_QUADRATIC_CURVE_TO 0x2
#define CMD_TYPE_CUBIC_CURVE_TO     0x3
#define CMD_TYPE_END                0x0991
#define MAGIC_TOKEN_START           0x4712
#define FILE_VERSION                0x0001
#define MAGIC_TOKEN_SEPARATOR       0x0992

/*
 * SVGFontFile
 */

void SVGFontFile::ReadL(INT16& result)
{
	short ans;
	LEAVE_IF_ERROR(m_filedesc->ReadBinShort(ans));
	result = ans;
}

void SVGFontFile::ReadL(INT32& result)
{
	long buffer;
	LEAVE_IF_ERROR(m_filedesc->ReadBinLong(buffer));
	result = buffer;
}

void SVGFontFile::ReadL(float& result)
{
	long buffer;
	LEAVE_IF_ERROR(m_filedesc->ReadBinLong(buffer));
	result = buffer / 65536.0f;
}

void SVGFontFile::ReadL(OpString& str)
{
	long len;
	LEAVE_IF_ERROR(m_filedesc->ReadBinLong(len));
	if (len > 0)
	{
		char* buffer = OP_NEWA(char, len + 1);
		if (buffer == 0)
			LEAVE(OpStatus::ERR_NO_MEMORY);

		OpFileLength read;
		OP_STATUS status = m_filedesc->Read(buffer, len, &read);
		if (OpStatus::IsSuccess(status))
		{
			OP_ASSERT(read <= (OpFileLength)len);
			buffer[read] = '\0';
			status = str.Set(buffer);
		}
		OP_DELETEA(buffer);
		LEAVE_IF_ERROR(status);
	}
}

/*
 * SVGGenerateGlyph ****************************************************
 */

class SVGGenerateGlyph
{
public:
	SVGGenerateGlyph(SVGGlyphData* glyph)
		: m_glyph(glyph) {}
	
	OP_STATUS		GenerateSquare(SVGNumber width);

	/**
	 * Generating interface
	 */
	OP_STATUS		MoveTo(SVGNumber x, SVGNumber y)
	{
		return m_glyph->GetOutline()->MoveTo(x, y, FALSE);
	}
	OP_STATUS		LineTo(SVGNumber x, SVGNumber y)
	{
		return m_glyph->GetOutline()->LineTo(x, y, FALSE);
	}
	OP_STATUS		QuadraticCurveTo(SVGNumber x1, SVGNumber y1, SVGNumber x, SVGNumber y)
	{
		return m_glyph->GetOutline()->QuadraticCurveTo(x1, y1, x, y, FALSE, FALSE);
	}
	OP_STATUS		CubicCurveTo(SVGNumber x1, SVGNumber y1, SVGNumber x2, SVGNumber y2, SVGNumber x, SVGNumber y)
	{
		return m_glyph->GetOutline()->CubicCurveTo(x1, y1, x2, y2, x, y, FALSE, FALSE);
	}

private:
	/**
	 * The glyph to generate
	 */
	SVGGlyphData*	m_glyph;
};

OP_STATUS
SVGGenerateGlyph::GenerateSquare(SVGNumber width)
{
	SVGNumber x0 = 0;
	SVGNumber x1 = width / 10;
	SVGNumber x2 = width - x1;
	SVGNumber x3 = width;

	SVGNumber y0 = 0;
	SVGNumber y1 = width / 10;
	SVGNumber y2 = width - x1;
	SVGNumber y3 = width;

	SVGNumber points[11][2] = { { x0, y0 }, { x0, y3 }, { x3, y3 }, { x3, y0 },
								{ x1, y0 }, { x1, y1 }, { x2, y1 }, { x2, y2 },
								{ x1, y2 }, { x1, y0 }, { x0, y0 } };

	OP_STATUS status = OpStatus::OK;
	status = MoveTo((SVGNumber)points[0][0],(SVGNumber)points[0][1]);

	for (unsigned int i=1; OpStatus::IsSuccess(status) && i<sizeof(points) / (2 * sizeof(SVGNumber)); i++)
		status = LineTo(points[i][0],points[i][1]);

	if (OpStatus::IsSuccess(status))
	{
		m_glyph->SetAdvanceX(width * SVGNumber(1.2));
		return OpStatus::OK;
	}
	else
		return status;
}

/*
 * SVGEmbeddedFontData ***********************************************
 */
SVGEmbeddedFontData::SVGEmbeddedFontData() : m_font_file(NULL)
{
}

SVGEmbeddedFontData::~SVGEmbeddedFontData()
{
	OP_DELETE(m_font_file);
}

/* virtual */
OP_STATUS SVGEmbeddedFontData::FetchGlyphData(const uni_char* in_str, UINT32 in_len,
											  UINT32& io_str_pos, BOOL in_writing_direction_horizontal,
											  const uni_char* lang,
											  SVGGlyphData** out_data)
{
	SVGGlyphData* data = m_cache.GetGlyph(in_str[io_str_pos++]);
	if (!data)
		data = &m_unknown_glyph;

	*out_data = data;
	return OpStatus::OK;
}

OP_STATUS SVGEmbeddedFontData::Load(SVGFontFile* font_file)
{
	TRAPD(rc, LoadL(font_file));
	return rc;
}

void SVGEmbeddedFontData::LoadL(SVGFontFile* font_file)
{
	short magic_token = 0x0;
	font_file->ReadL(magic_token);

	if (magic_token != MAGIC_TOKEN_START) // Check magic token
		LEAVE(OpStatus::ERR);

	short file_version = 0x0;
	font_file->ReadL(file_version);

	if (file_version != FILE_VERSION) // Check version
		LEAVE(OpStatus::ERR);

	m_packed.has_kerning = 0;
	m_packed.has_ligatures = 0;

	float ascent, descent;
	INT32 units_per_em;
	font_file->ReadL(ascent);
	font_file->ReadL(descent);
	font_file->ReadL(units_per_em);

	SetUnitsPerEm(units_per_em);
	SetSVGFontAttribute(SVGFontData::ASCENT, ascent);
	SetSVGFontAttribute(SVGFontData::DESCENT, descent);

	font_file->ReadL(m_family_name);
	font_file->ReadL(m_style_name);

	OpFileLength glyph_table_offset;
	font_file->GetFilePos(glyph_table_offset);

	LEAVE_IF_ERROR(m_cache.Init(font_file, glyph_table_offset));

	OpBpath* missing_glyph = NULL;
	LEAVE_IF_ERROR(OpBpath::Make(missing_glyph, FALSE));
	OpStatus::Ignore(m_unknown_glyph.SetOutline(missing_glyph));

	SVGGenerateGlyph glyph_gen(&m_unknown_glyph);
	LEAVE_IF_ERROR(glyph_gen.GenerateSquare(m_units_per_em * 0.7));

	m_font_file = font_file;

	m_packed.is_fully_created = 1;
}

/*
 * SVGSystemFontCache **************************************************
 */

SVGSystemFontCache::SVGSystemFontCache()
		: m_misses(0),
		  m_requests(0),
		  m_cache_idx(0),
		  m_cache(NULL),
		  m_index(NULL)
{
}

SVGSystemFontCache::~SVGSystemFontCache()
{
	OP_NEW_DBG("SVGSystemFontCache::~SVGSystemFontCache", "svg_systemfontcache");
	if (m_requests > 0)
	{
		OP_DBG(("Cache results: %g%% misses\n", 100.0 * (float)m_misses / (float) m_requests));
	}

	for (int i = 0; i < CacheSize; ++i)
		OP_DELETE(m_cache[i]);

	OP_DELETEA(m_cache);
	OP_DELETEA(m_index);
}

OP_STATUS
SVGSystemFontCache::Init(SVGFontFile* in_file, OpFileLength glyph_table_offset)
{
	m_font_file = in_file;

	m_cache = OP_NEWA(SVGGlyphData*, CacheSize);
	if (!m_cache)
		return OpStatus::ERR_NO_MEMORY;

	op_memset(m_cache, 0, CacheSize * sizeof(*m_cache));

	TRAPD(rc, ReadIndexL(glyph_table_offset));
	return rc;
}

SVGGlyphData*
SVGSystemFontCache::GetGlyph(uni_char uc)
{
	++m_requests;

	for (int i = 0; i < CacheSize; i++)
	{
		SVGGlyphData* item = m_cache[i];
		if (item && item->IsMatch(&uc, 1))
			return item;
	}
	++m_misses;

	SVGGlyphData* glyph = NULL;
	TRAPD(rc, glyph = ReadGlyphL(uc));
	if (OpStatus::IsError(rc) || !glyph)
		return NULL;

	SVGGlyphData* old_item = m_cache[m_cache_idx];
	if (old_item)
		OP_DELETE(old_item);

	m_cache[m_cache_idx] = glyph;

	m_cache_idx = (m_cache_idx + 1) % CacheSize;

	return glyph;
}

void
SVGSystemFontCache::ReadIndexL(OpFileLength glyph_table_offset)
{
	if (!m_font_file || !glyph_table_offset)
		LEAVE(OpStatus::ERR);

	m_font_file->SetFilePos(glyph_table_offset);

	m_font_file->ReadL(m_glyphs);
	m_index = OP_NEWA(IndexItem, m_glyphs);
	if (!m_index)
		LEAVE(OpStatus::ERR_NO_MEMORY);

	for(int i=0;i<m_glyphs;i++)
	{
		short magic_token;
		m_font_file->ReadL(magic_token);
		if (magic_token != MAGIC_TOKEN_SEPARATOR)
			break;

		INT32 data_size;
		m_font_file->ReadL(data_size);

		short uc;
		m_font_file->ReadL(uc);

		if (m_font_file->Eof())
			break;

		m_index[i].uc = uc;
		m_font_file->GetFilePos(m_index[i].position);

		// Skip data except uc which we have read
		m_font_file->SetFilePos(data_size - 2, SEEK_FROM_CURRENT);
	}
}

SVGGlyphData*
SVGSystemFontCache::ReadGlyphL(uni_char uc)
{
	if (!m_font_file)
		return NULL;

	int i=0;
	for ( ; i < m_glyphs && m_index[i].uc != uc; i++)
		; // Use hash table here?

	if (i == m_glyphs)
		return NULL; // Glyph not in index

	m_font_file->SetFilePos(m_index[i].position); // Skip advance

	OpStackAutoPtr<SVGGlyphData> glyph(OP_NEW(SVGGlyphData, ()));
	if (!glyph.get())
		return NULL;

	OpBpath* outline = NULL;
	LEAVE_IF_ERROR(OpBpath::Make(outline, FALSE));
	glyph->SetOutline(outline);

	float advance = 0;
	m_font_file->ReadL(advance);
	glyph->SetAdvanceX(advance);
	LEAVE_IF_ERROR(glyph->SetUnicode(&uc, 1));

	SVGGenerateGlyph glyph_gen(glyph.get());

	short cmd_done = 0;
	short cmd = 0x0;
	while(!cmd_done)
	{
		m_font_file->ReadL(cmd);

		INT16 x1, y1, x2, y2, x, y;
		switch(cmd)
		{
		case CMD_TYPE_MOVE_TO:
			m_font_file->ReadL(x);
			m_font_file->ReadL(y);
			glyph_gen.MoveTo(x, y);
			break;
		case CMD_TYPE_LINE_TO:
			m_font_file->ReadL(x);
			m_font_file->ReadL(y);
			glyph_gen.LineTo(x, y);
			break;
		case CMD_TYPE_QUADRATIC_CURVE_TO:
			m_font_file->ReadL(x1);
			m_font_file->ReadL(y1);
			m_font_file->ReadL(x);
			m_font_file->ReadL(y);
			glyph_gen.QuadraticCurveTo(x1, y1, x, y);
			break;
		case CMD_TYPE_CUBIC_CURVE_TO:
			m_font_file->ReadL(x1);
			m_font_file->ReadL(y1);
			m_font_file->ReadL(x2);
			m_font_file->ReadL(y2);
			m_font_file->ReadL(x);
			m_font_file->ReadL(y);
			glyph_gen.CubicCurveTo(x1, y1, x2, y2, x, y);
			break;
		case CMD_TYPE_END:
			cmd_done = 1;
			break;
		}
	}

	glyph->GetOutline()->CompactPath();

	return glyph.release();
}

/*
 * SVGSystemFontManager *************************************************
 */

SVGSystemFontManager::SVGSystemFontManager() :
	m_monospace_font(NULL),
	m_serif_font(NULL),
	m_sans_serif_font(NULL),
	m_bold_monospace_font(NULL),
	m_bold_serif_font(NULL),
	m_bold_sans_serif_font(NULL),
	m_create_called(FALSE),
	m_hasfonts(FALSE)
{}

SVGSystemFontManager::~SVGSystemFontManager()
{
	SVGFontData::DecRef(m_monospace_font);
	SVGFontData::DecRef(m_serif_font);
	SVGFontData::DecRef(m_sans_serif_font);
	SVGFontData::DecRef(m_bold_monospace_font);
	SVGFontData::DecRef(m_bold_serif_font);
	SVGFontData::DecRef(m_bold_sans_serif_font);
}

OP_STATUS
SVGSystemFontManager::LoadFont(const uni_char* filename, SVGEmbeddedFontData** font)
{
	OpAutoPtr<OpFile> op_file(OP_NEW(OpFile, ()));
	if (!op_file.get())
		return OpStatus::ERR_NO_MEMORY;

	OP_ASSERT(sizeof(long) == 4);
	RETURN_IF_ERROR(op_file->Construct(filename, OPFILE_RESOURCES_FOLDER));
	RETURN_IF_ERROR(op_file->Open(OPFILE_READ));

	OpAutoPtr<SVGFontFile> font_file(OP_NEW(SVGFontFile, (op_file.get())));
	if (!font_file.get())
		return OpStatus::ERR_NO_MEMORY;

	op_file.release();

	SVGEmbeddedFontData *new_font = OP_NEW(SVGEmbeddedFontData, ());
	if (!new_font)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = new_font->Load(font_file.get());
	if (OpStatus::IsError(status))
	{
		OP_DELETE(new_font);
		*font = NULL;
		return status;
	}

	font_file.release();
	SVGFontData::IncRef(*font = new_font);
	return OpStatus::OK;
}

OP_STATUS SVGSystemFontManager::Create()
{
	m_create_called = TRUE;
	RETURN_IF_ERROR(LoadFont(UNI_L("svg-mo.dat"), &m_monospace_font));
	RETURN_IF_ERROR(LoadFont(UNI_L("svg-sa.dat"), &m_sans_serif_font));
	RETURN_IF_ERROR(LoadFont(UNI_L("svg-se.dat"), &m_serif_font));
	RETURN_IF_ERROR(LoadFont(UNI_L("svg-mobd.dat"), &m_bold_monospace_font));
	RETURN_IF_ERROR(LoadFont(UNI_L("svg-sabd.dat"), &m_bold_sans_serif_font));
	RETURN_IF_ERROR(LoadFont(UNI_L("svg-sebd.dat"), &m_bold_serif_font));
	m_hasfonts = TRUE;
	return OpStatus::OK;
}

SVGEmbeddedFontData*
SVGSystemFontManager::GetMatchingSystemFont(FontAtt &fontatt)
{
	if(!m_hasfonts)
		return NULL;

	BOOL use_bold = FALSE;
	const OpFontInfo* info = styleManager->GetFontInfo(fontatt.GetFontNumber());

	if (fontatt.GetWeight() >= 6)
	{
		use_bold = TRUE;
	}

	if(info != NULL)
	{
		if (info->Monospace())
			return use_bold ? m_bold_monospace_font : m_monospace_font;
		else if (info->GetSerifs() == OpFontInfo::WithSerifs)
			return use_bold ? m_bold_serif_font : m_serif_font;
	}

	/* Default */
	return use_bold ? m_bold_sans_serif_font : m_sans_serif_font;
}

#endif // SVG_SUPPORT_EMBEDDED_FONTS
#endif // SVG_SUPPORT
