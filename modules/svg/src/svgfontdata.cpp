/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"

#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGCanvasState.h"
#include "modules/svg/src/SVGVector.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGFontTraversalObject.h"
#include "modules/svg/src/OpBpath.h"

#include "modules/svg/src/svgfontdata.h"

// For unicode-range parsing for kerning
#include "modules/svg/src/parser/svgnumberparser.h"
#include "modules/unicode/unicode.h"

#include "modules/logdoc/htm_elm.h"

#ifdef SVG_STORE_SHAPED_GLYPH
uni_char GetShapedGlyph(uni_char unicode, SVGArabicForm arabic_form)
{
	if (IsArabic(unicode))
		switch (arabic_form)
		{
		case SVGARABICFORM_INITIAL:
			return TextShaper::GetInitialForm(unicode);
		case SVGARABICFORM_MEDIAL:
			return TextShaper::GetMedialForm(unicode);
		case SVGARABICFORM_TERMINAL:
			return TextShaper::GetFinalForm(unicode);
		case SVGARABICFORM_ISOLATED:
			return TextShaper::GetIsolatedForm(unicode);
		}
	return 0;
}
#endif // SVG_STORE_SHAPED_GLYPH

SVGGlyphData::~SVGGlyphData()
{
	if (packed.is_simple == 0)
		OP_DELETEA(m_unicode.composite.unicode);
	OP_DELETEA(m_glyphname);
	SVGObject::DecRef(m_glyph);
	SVGObject::DecRef(m_lang);
	OP_DELETE(m_next);
}

OP_STATUS SVGGlyphData::SetOutline(OpBpath* glyph)
{
	OP_ASSERT(!m_glyph);
	SVGObject::IncRef(m_glyph = glyph);
	if (m_glyph)
		packed.has_path = TRUE;
	return OpStatus::OK;
}

void SVGGlyphData::SetAdvanceX(SVGNumber x)
{
	m_advance_x = x;
	packed.has_adv_x = TRUE;
}
void SVGGlyphData::SetAdvanceY(SVGNumber y)
{
	m_advance_y = y;
	packed.has_adv_y = TRUE;
}

void SVGGlyphData::SetLang(SVGVector* lang_vec)
{
	SVGObject::IncRef(m_lang = lang_vec);
}

BOOL SVGGlyphData::MatchLang(const uni_char* lang, unsigned int lang_len) const
{
	if (!m_lang)
		return TRUE;

	if (!lang)
		return FALSE;

	for (unsigned int i = 0; i < m_lang->GetCount(); ++i)
	{
		SVGObject* o = m_lang->Get(i);
		if (o->Type() != SVGOBJECT_STRING)
			continue;

		SVGString* s = static_cast<SVGString*>(o);
		if (s->GetLength() >= lang_len &&
			uni_strnicmp(s->GetString(), lang, lang_len) == 0 &&
			(s->GetLength() == lang_len || s->GetString()[lang_len] == '-'))
			return TRUE;
	}
	return FALSE;
}

/* static */
SVGFontData* SVGFontData::IncRef(SVGFontData* fdata)
{
	if (fdata)
		++fdata->m_refcount;

	return fdata;
}

/* static */
void SVGFontData::DecRef(SVGFontData* fdata)
{
	if (fdata && --fdata->m_refcount == 0)
		OP_DELETE(fdata);
}

void SVGFontData::SetDefaultAttributes()
{
	SetSVGFontAttribute(ASCENT, m_units_per_em);
	SetSVGFontAttribute(DESCENT, 0);

	SetSVGFontAttribute(UNDERLINE_POSITION, -m_units_per_em / 8);
	SetSVGFontAttribute(OVERLINE_POSITION, m_units_per_em * 9 / 10);
	SetSVGFontAttribute(STRIKETHROUGH_POSITION, m_units_per_em / 3);

	SetSVGFontAttribute(UNDERLINE_THICKNESS, m_units_per_em / 12);
	SetSVGFontAttribute(OVERLINE_THICKNESS, m_units_per_em / 12);
	SetSVGFontAttribute(STRIKETHROUGH_THICKNESS, m_units_per_em / 12);
}

void SVGFontData::SetUnitsPerEm(SVGNumber upem)
{
	m_units_per_em = upem;
	SetDefaultAttributes();
}

SVGXMLFontData::SVGXMLFontData()
	: SVGFontData(),
	  m_fontelm(NULL),
	  m_current_glyph_idx(1), // Reserving 0 for 'unknown' just in case
	  m_kern_tables_built(FALSE)
{
	m_units_per_em = 1000;
	// Now we have units_per_em so now we can set default attributes
	SetDefaultAttributes();
}

OP_STATUS SVGXMLFontData::Construct(HTML_Element* fontelm)
{
	if (!fontelm)
		return OpStatus::ERR_NULL_POINTER;

	OP_ASSERT(m_fontelm == NULL);
	m_fontelm = fontelm;

	return OpStatus::OK;
}

OP_STATUS SVGXMLFontData::CreateFontContents(SVGDocumentContext* doc_ctxt)
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(m_fontelm);
	if (!doc_ctx)
		return OpStatus::ERR;

	SVGFontTraversalObject font_traversal_object(doc_ctx, this);
	RETURN_IF_ERROR(font_traversal_object.CreateResolver());

	OP_STATUS status = SVGSimpleTraverser::TraverseElement(&font_traversal_object, m_fontelm);

	// Different errors are possible that not really affects how the
	// font is created. Only OOM is necessary to propagate.
	RETURN_IF_MEMORY_ERROR(status);

	m_packed.is_fully_created = 1;

	return OpStatus::OK;
}

OP_STATUS SVGXMLFontData::SetPathAndAdvance(SVGGlyphData* data, OpBpath* path,
											SVGNumberObject* advX, SVGNumberObject* advY)
{
	data->SetOutline(path);

	if (advX)
		data->SetAdvanceX(advX->value);
	if (advY)
		data->SetAdvanceY(advY->value);

	return OpStatus::OK;
}

unsigned int SVGXMLFontData::FindCMapIndex(unsigned c)
{
	unsigned int start = 0;
	unsigned int end = m_cmap.GetCount();

	while (end > start)
	{
		unsigned int mid = start + (end - start) / 2;

		if (m_cmap.Get(mid)->Compare(c) < 0)
			start = mid + 1;
		else
			end = mid;
	}
	return start;
}

SVGGlyphData* SVGXMLFontData::FindInCMap(unsigned c)
{
	if (m_cmap.GetCount() == 0)
		return NULL;

	unsigned int index = FindCMapIndex(c);

	if (index < m_cmap.GetCount() && m_cmap.Get(index)->Compare(c) == 0)
		return m_cmap.Get(index);

	return NULL;
}

OP_STATUS SVGXMLFontData::InsertIntoCMap(unsigned c, SVGGlyphData* data)
{
	unsigned int index = FindCMapIndex(c);

	if (index < m_cmap.GetCount() && m_cmap.Get(index)->Compare(c) == 0)
	{
		SVGGlyphData* gd = m_cmap.Get(index);
		while (gd->Suc())
			gd = gd->Suc();

		gd->SetSuc(data);
		OP_ASSERT(data->Suc() == NULL);
	}
	else
	{
		RETURN_IF_ERROR(m_cmap.Insert(index, data));
	}
	return OpStatus::OK;
}

OP_STATUS SVGXMLFontData::AddGlyph(const uni_char* unicode, const uni_char* glyphname,
								   OpBpath* path, SVGNumberObject* advX, SVGNumberObject* advY,
								   SVGArabicForm arabic_form, SVGGlyphData::GlyphDirectionMask gdir_mask,
								   SVGVector* lang_vector)
{
	if (!unicode || !*unicode)
		return OpStatus::OK;

	OpAutoPtr<SVGGlyphData> data(OP_NEW(SVGGlyphData, ()));
	if (!data.get())
		return OpStatus::ERR_NO_MEMORY;

	OP_ASSERT(unicode || glyphname); // if we don't have one of these there's no way to select the glyph

	SetPathAndAdvance(data.get(), path, advX, advY);

	if (unicode)
	{
		UINT32 unicode_len = uni_strlen(unicode);
		if (unicode_len > 1)
		{
			m_packed.has_ligatures = 1;
		}

		RETURN_IF_ERROR(data->SetUnicode(unicode, unicode_len));
	}

	if (glyphname)
	{
		RETURN_IF_ERROR(data->SetGlyphName(glyphname, uni_strlen(glyphname)));
	}

	data->SetArabicForm(arabic_form); // Irrelevant for anything but arabic letters
	data->SetDirection(gdir_mask);
	data->SetLang(lang_vector);

	unsigned cmap_key = data->Compare(0);

	RETURN_IF_ERROR(InsertIntoCMap(cmap_key, data.get()));

	data->SetIndex(m_current_glyph_idx++);

	data.release();

	return OpStatus::OK;
}

OP_STATUS SVGXMLFontData::FetchGlyphData(const uni_char* in_str, UINT32 in_len,
										 UINT32& io_str_pos, BOOL in_writing_direction_horizontal,
										 const uni_char* lang,
										 SVGGlyphData** out_data)
{
	OP_STATUS result = OpStatus::ERR;
	const uni_char* str = in_str+io_str_pos;
	OP_ASSERT(in_len > io_str_pos);
	UINT32 str_len = in_len - io_str_pos;
	unsigned int lang_len = lang ? uni_strlen(lang) : 0;
	SVGGlyphData::GlyphDirectionMask gdir =
		in_writing_direction_horizontal ? SVGGlyphData::HORIZONTAL : SVGGlyphData::VERTICAL;

	SVGGlyphData* data = FindInCMap(*str);
	for ( ; data ; data = data->Suc())
	{
		if (data->MatchUnicode(str, str_len) &&
			data->MatchDirection(gdir) &&
			data->MatchLang(lang, lang_len))
		{
			UINT32 match_len = data->GetUnicodeLen();
			if (IsArabic(*str))
			{
				BOOL matches_arabic = FALSE;
				switch (data->GetArabicForm())
				{
				case SVGARABICFORM_INITIAL:
					matches_arabic = io_str_pos == 0 && match_len != in_len;
					break;
				case SVGARABICFORM_MEDIAL:
					matches_arabic = io_str_pos > 0 && io_str_pos + match_len < in_len;
					break;
				case SVGARABICFORM_TERMINAL:
					matches_arabic = io_str_pos > 0 && io_str_pos + match_len == in_len;
					break;
				case SVGARABICFORM_ISOLATED:
					matches_arabic = io_str_pos == 0 && match_len == in_len;
					break;
				}
				if (!matches_arabic)
				{
					continue;
				}
			}
			io_str_pos += match_len;
			result = OpStatus::OK;
			break;
		}
	}

	if (OpStatus::IsSuccess(result))
	{
		*out_data = data;
	}
	else
	{
		// No match
		*out_data = &m_missing_glyph;
		io_str_pos++;
	}
	return result;
}

OP_STATUS SVGXMLFontData::SetMissingGlyph(OpBpath* path, SVGNumberObject* advX, SVGNumberObject* advY)
{
	return SetPathAndAdvance(&m_missing_glyph, path, advX, advY);
}

SVGKernData::~SVGKernData()
{
	SVGObject::DecRef(m_g1);
	SVGObject::DecRef(m_g2);
	SVGObject::DecRef(m_u1);
	SVGObject::DecRef(m_u2);
}

OP_STATUS SVGKernData::SetData(SVGVector* g1, SVGVector* g2, SVGVector* u1, SVGVector* u2, SVGNumber kerning)
{
	OP_ASSERT(!m_g1 && !m_g2 && !m_u1 && !m_u2);
	SVGObject::IncRef(m_g1 = g1);
	SVGObject::IncRef(m_g2 = g2);
	SVGObject::IncRef(m_u1 = u1);
	SVGObject::IncRef(m_u2 = u2);

	m_kerning = kerning;
	return OpStatus::OK;
}

OP_STATUS SVGXMLFontData::AddKern(BOOL horizontal,
								  SVGVector* g1, SVGVector* g2,
								  SVGVector* u1, SVGVector* u2, SVGNumber k)
{
	OpAutoPtr<SVGKernData> kerndata(OP_NEW(SVGKernData, ()));
	if(!kerndata.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(kerndata->SetData(g1, g2, u1, u2, k));
	RETURN_IF_ERROR(horizontal ? m_hkern.Add(kerndata.get()) : m_vkern.Add(kerndata.get()));

	kerndata.release();

	m_packed.has_kerning = 1;

	return OpStatus::OK;
}

#ifdef INDEXBASED_KERN_TABLES
OP_STATUS SVGKerningTable::Resize(unsigned int new_size)
{
	OP_ASSERT(new_size > 0);
	KernPair* new_pairs = OP_NEWA(KernPair, new_size);
	if (!new_pairs)
		return OpStatus::ERR_NO_MEMORY;

	if (m_pairs && m_count)
		op_memcpy(new_pairs, m_pairs, sizeof(*m_pairs) * m_count);

	OP_DELETEA(m_pairs);
	m_pairs = new_pairs;
	m_allocated = new_size;
	return OpStatus::OK;
}

OP_STATUS SVGKerningTable::Prepare(unsigned int size_hint)
{
	if (size_hint)
		return Resize(size_hint);

	return OpStatus::OK;
}

OP_STATUS SVGKerningTable::Append(SVGGlyphIdx gi1, SVGGlyphIdx gi2, SVGNumber value)
{
	if (m_count == m_allocated)
		RETURN_IF_ERROR(Resize(m_count + GROWTH_AMOUNT));

	KernPair* kp = m_pairs + m_count;
	kp->first = gi1;
	kp->second = gi2;
	kp->value = value;

	m_count++;
	return OpStatus::OK;
}

/* static */
int SVGKerningTable::Compare(const void* a, const void* b)
{
	UINT32 a_val = reinterpret_cast<const KernPair*>(a)->SearchValue();
	UINT32 b_val = reinterpret_cast<const KernPair*>(b)->SearchValue();
	return (int)(a_val - b_val);
}

OP_STATUS SVGKerningTable::Finalize()
{
	// Potentially shrink the table
	if (m_allocated - m_count > GROWTH_AMOUNT)
		OpStatus::Ignore(Resize(m_count));

	// Sort to allow efficient searching
	if (m_pairs != NULL)
		op_qsort(m_pairs, m_count, sizeof(*m_pairs), Compare);
	
	return OpStatus::OK;
}

OP_STATUS SVGKerningTable::Lookup(SVGGlyphIdx gi1, SVGGlyphIdx gi2, SVGNumber& kern_value)
{
	UINT32 search_val = (gi1 << 16) | gi2;

	unsigned int start = 0;
	unsigned int end = m_count;

	while (end > start)
	{
		unsigned int mid = start + (end - start) / 2;

		UINT32 val = m_pairs[mid].SearchValue();
		if (val < search_val)
			start = mid + 1;
		else
			end = mid;
	}

	if (start < m_count)
	{
		KernPair* kp = m_pairs + start;
		if (kp->first == gi1 && kp->second == gi2)
		{
			kern_value = kp->value;
			return OpStatus::OK;
		}
	}
	return OpStatus::ERR;
}

SVGGlyphIdx SVGXMLFontData::UnicodeToIdx(const uni_char* str, unsigned int str_len)
{
	for (SVGGlyphData* data = FindInCMap(*str); data ; data = data->Suc())
		if (data->MatchUnicode(str, str_len))
			return data->GetIndex();

	return 0;
}

SVGGlyphIdx SVGXMLFontData::GlyphNameToIdx(const uni_char* name, unsigned int name_len)
{
	for (unsigned int i = 0; i < m_cmap.GetCount(); ++i)
	{
		for (SVGGlyphData* gd = m_cmap.Get(i); gd; gd = gd->Suc())
			if (gd->MatchGlyphName(name, name_len))
				return gd->GetIndex();
	}
	return 0;
}

OP_STATUS SVGXMLFontData::MapAndAppendIndices(const SVGVector* v, OpINT32Vector& indices, BOOL map_names)
{
	OP_ASSERT(v && v->VectorType() == SVGOBJECT_STRING);

	// Lookup glyph based on unicode char(s)
	SVGNumberParser parser;
	for (unsigned int j = 0; j < v->GetCount(); ++j)
	{
		SVGObject* o = v->Get(j);
		if (o->Type() != SVGOBJECT_STRING)
			continue;

		SVGString* s = static_cast<SVGString*>(o);
		if (map_names)
		{
			SVGGlyphIdx gi = GlyphNameToIdx(s->GetString(), s->GetLength());
			if (gi > 0)
				RETURN_IF_ERROR(indices.Add(gi));
		}
		else
		{
			SVGGlyphIdx gi;
			unsigned int uc_start, uc_end;
			if (OpStatus::IsSuccess(parser.ParseUnicodeRange(s->GetString(), s->GetLength(),
															 uc_start, uc_end)))
			{
				if (uc_start <= uc_end)
				{
					uni_char ch[2]; /* ARRAY OK 2008-04-09 ed */ 
					unsigned len;

					for (unsigned int uc = uc_start; uc <= uc_end; ++uc)
					{
						if (uc >= 0x10000)
						{
							Unicode::MakeSurrogate(uc, ch[0], ch[1]);
							len = 2;
						}
						else
						{
							ch[0] = (uni_char)uc;
							len = 1;
						}

						gi = UnicodeToIdx(ch, len);
						if (gi > 0)
							RETURN_IF_ERROR(indices.Add(gi));
					}
				}
			}
			else if (s->GetLength() == 1 ||
					 (s->GetLength() == 2 &&
					  Unicode::IsHighSurrogate(s->GetString()[0]) &&
					  Unicode::IsLowSurrogate(s->GetString()[1])))
			{
				gi = UnicodeToIdx(s->GetString(), s->GetLength());
				if (gi > 0)
					RETURN_IF_ERROR(indices.Add(gi));
			}
		}

	}
	return OpStatus::OK;
}

OP_STATUS SVGXMLFontData::BuildKernTable(OpAutoVector<SVGKernData>& kerndatatable,
										 SVGKerningTable& kerntab)
{
	OpINT32Vector first_indices;
	OpINT32Vector second_indices;

	// Using number of SVGKernData's as size heuristic (assuming 1:1
	// which appears to be fairly common)
	RETURN_IF_ERROR(kerntab.Prepare(kerndatatable.GetCount()));

	for (unsigned int i = 0; i < kerndatatable.GetCount(); ++i)
	{
		SVGKernData* k = kerndatatable.Get(i);

		if (k->GetG1())
			RETURN_IF_ERROR(MapAndAppendIndices(k->GetG1(), first_indices, TRUE));
		if (k->GetU1())
			RETURN_IF_ERROR(MapAndAppendIndices(k->GetU1(), first_indices, FALSE));

		if (k->GetG2())
			RETURN_IF_ERROR(MapAndAppendIndices(k->GetG2(), second_indices, TRUE));
		if (k->GetU2())
			RETURN_IF_ERROR(MapAndAppendIndices(k->GetU2(), second_indices, FALSE));

		if (first_indices.GetCount() != 0 &&
			second_indices.GetCount() != 0)
		{
			// Outer product of [first] and [second]
			for (unsigned int f = 0; f < first_indices.GetCount(); ++f)
			{
				for (unsigned int s = 0; s < second_indices.GetCount(); ++s)
				{
					RETURN_IF_ERROR(kerntab.Append(first_indices.Get(f),
												   second_indices.Get(s),
												   k->GetKerningValue()));
				}
			}
		}

		first_indices.Clear();
		second_indices.Clear();
	}

	RETURN_IF_ERROR(kerntab.Finalize());

	return OpStatus::OK;
}

OP_STATUS SVGXMLFontData::GetKern(SVGGlyphData* g1data, SVGGlyphData* g2data,
								  BOOL horizontal, SVGNumber &out_kerning)
{
	if (!g1data || !g2data)
		return OpStatus::ERR;

	if (!m_kern_tables_built)
	{
		RETURN_IF_ERROR(BuildKernTable(m_hkern, m_kern_pairs_h));
		RETURN_IF_ERROR(BuildKernTable(m_vkern, m_kern_pairs_v));

		m_kern_tables_built = TRUE;

		// Tables are built, go ahead and drop the original data
		m_hkern.DeleteAll();
		m_vkern.DeleteAll();
	}

	SVGKerningTable* kerntable = horizontal ? &m_kern_pairs_h : &m_kern_pairs_v;

	return kerntable->Lookup(g1data->GetIndex(), g2data->GetIndex(), out_kerning);
}
#else
BOOL SVGKernData::IsMatch(const SVGVector* v1, SVGGlyphData* g1, BOOL isGlyphNameVector)
{
	if(v1 && g1)
	{
		for(UINT32 i = 0; i < v1->GetCount(); i++)
		{
			OP_ASSERT(v1->VectorType() == SVGOBJECT_STRING);
			SVGObject* o = v1->Get(i);

			if(o->Type() == SVGOBJECT_STRING)
			{
				SVGString* s = static_cast<SVGString*>(o);
				if(g1->IsMatch(s->GetString(), s->GetLength(), isGlyphNameVector))
				{
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

OP_STATUS SVGXMLFontData::GetKern(SVGGlyphData* g1data, SVGGlyphData* g2data,
								  BOOL horizontal, SVGNumber &out_kerning)
{
	if(!g1data || !g2data)
		return OpStatus::ERR;

	OpAutoVector<SVGKernData>* kerntable = horizontal ? &m_hkern : &m_vkern;
	SVGKernData* kern = NULL;

	for(UINT32 i = 0; i < kerntable->GetCount(); i++)
	{
		kern = kerntable->Get(i);

#if defined _DEBUG && defined EXPENSIVE_DEBUG_CHECKS
		BOOL match1 = SVGKernData::IsMatch(kern->GetG1(), g1data, TRUE);
		BOOL match2 = SVGKernData::IsMatch(kern->GetU1(), g1data);
		BOOL match3 = SVGKernData::IsMatch(kern->GetG2(), g2data, TRUE);
		BOOL match4 = SVGKernData::IsMatch(kern->GetU2(), g2data);

		OP_NEW_DBG("SVGFontImpl::GetKern", "svg_kerning");
		OP_DBG((UNI_L("GlyphName1: %s"),
				g1data->GetGlyphName() ? g1data->GetGlyphName() : UNI_L("")));
		OP_DBG((UNI_L("GlyphName2: %s"),
				g2data->GetGlyphName() ? g2data->GetGlyphName() : UNI_L("")));
		OP_DBG((UNI_L("Unicode1: %s"),
				g1data->GetUnicode() ?
				g1data->GetUnicode()));
		OP_DBG((UNI_L("Unicode2: %s Match: %s%s%s%s"),
				g2data->GetUnicode() ? g2data->GetUnicode() : UNI_L(""),
				match1 ? UNI_L("Y") : UNI_L("N"), match2 ? UNI_L("Y") : UNI_L("N"),
				match3 ? UNI_L("Y") : UNI_L("N"), match4 ? UNI_L("Y") : UNI_L("N")));
#endif // _DEBUG && EXPENSIVE_DEBUG_CHECKS

		if ((SVGKernData::IsMatch(kern->GetG1(), g1data, TRUE) || SVGKernData::IsMatch(kern->GetU1(), g1data)) &&
			(SVGKernData::IsMatch(kern->GetG2(), g2data, TRUE) || SVGKernData::IsMatch(kern->GetU2(), g2data)))
		{
			out_kerning = kern->GetKerningValue();
			return OpStatus::OK;
		}
	}
	return OpStatus::ERR;
}
#endif // INDEXBASED_KERN_TABLES

#endif // SVG_SUPPORT
