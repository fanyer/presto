/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "modules/widgets/optextfragmentlist.h"
#include "modules/widgets/optextfragment.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/unicode/unicode_bidiutils.h"
#include "modules/unicode/unicode_stringiterator.h"
#include "modules/widgets/OpTextCollection.h"

OpTextFragmentList::~OpTextFragmentList()
{
	Clear();
}

void OpTextFragmentList::Clear()
{
	OP_DELETEA(m_fragments);
	m_fragments = NULL;

#ifdef COLOURED_MULTIEDIT_SUPPORT
	OP_DELETEA(m_tag_type_array);
	m_tag_type_array = NULL;
#endif

	m_num_fragments = 0;
}

OP_TEXT_FRAGMENT* OpTextFragmentList::Get(UINT32 idx)
{
	OP_ASSERT(idx < m_num_fragments);

	return &m_fragments[idx];
}

OP_TEXT_FRAGMENT* OpTextFragmentList::GetByVisualOrder(UINT32 idx)
{
	OP_ASSERT(idx < m_num_fragments);

	return &m_fragments[m_fragments[idx].nextidx];
}

OP_TEXT_FRAGMENT_VISUAL_OFFSET OpTextFragmentList::LogicalToVisualOffset(OP_TEXT_FRAGMENT_LOGICAL_OFFSET logical_offset)
{
	OP_ASSERT(!m_num_fragments || logical_offset.offset >= 0 && logical_offset.offset <= m_fragments[m_num_fragments - 1].start + (int) m_fragments[m_num_fragments - 1].wi.GetLength());

	OP_TEXT_FRAGMENT_VISUAL_OFFSET visual_offset;
	visual_offset.idx = 0;
	visual_offset.offset = logical_offset.offset;

	UINT32 i;
	int f_vis_start = 0;
	for(i = 0; i < m_num_fragments; i++)
	{
		OP_TEXT_FRAGMENT* frag = GetByVisualOrder(i);

		int start_of_frag = frag->start;
		if (logical_offset.snap_forward || frag == Get(0))
			start_of_frag--;

		if (logical_offset.offset > start_of_frag)
		{
			int end_of_frag = frag->start + frag->wi.GetLength();
			// If we are at the last fragment (with the highest offset) we should increase end_of_frag so we don't risk missing it.
			// (Or if we should not snap forward)
			if (!logical_offset.snap_forward || frag == Get(m_num_fragments - 1))
				end_of_frag++;

			if (logical_offset.offset < end_of_frag)
			{
				if (frag->packed.bidi == BIDI_FRAGMENT_MIRRORED)
					visual_offset.offset = f_vis_start + (frag->start + frag->wi.GetLength() - logical_offset.offset);
				else
					visual_offset.offset = f_vis_start + logical_offset.offset - frag->start;
				visual_offset.idx = i;
				break;
			}
		}
		f_vis_start += frag->wi.GetLength();
	}
	return visual_offset;
}

OP_TEXT_FRAGMENT_LOGICAL_OFFSET OpTextFragmentList::VisualToLogicalOffset(OP_TEXT_FRAGMENT_VISUAL_OFFSET visual_offset)
{
	OP_ASSERT(!m_num_fragments || visual_offset.offset >= 0 && visual_offset.offset <= m_fragments[m_num_fragments - 1].start + (int) m_fragments[m_num_fragments - 1].wi.GetLength());

	OP_TEXT_FRAGMENT_LOGICAL_OFFSET logical_offset;
	logical_offset.snap_forward = FALSE;
	logical_offset.offset = visual_offset.offset;

	UINT32 i;
	int f_vis_start = 0;
	for(i = 0; i < m_num_fragments; i++)
	{
		OP_TEXT_FRAGMENT* frag = GetByVisualOrder(i);

		if (visual_offset.offset >= f_vis_start &&
			visual_offset.offset <= f_vis_start + (int) frag->wi.GetLength())
		{
			if (frag->packed.bidi == BIDI_FRAGMENT_MIRRORED)
				logical_offset.offset = frag->start + (f_vis_start + frag->wi.GetLength() - visual_offset.offset);
			else
				logical_offset.offset = frag->start + visual_offset.offset - f_vis_start;
			logical_offset.snap_forward = (logical_offset.offset == frag->start);

			if (i == (UINT32) visual_offset.idx)
				break;
		}
		f_vis_start += frag->wi.GetLength();
	}
	return logical_offset;
}

BOOL OpTextFragmentList::GetNextTextFragment(const uni_char*& txt,
											int &length,
											UnicodePoint& previous_character,
											BOOL leading_whitespace,
											WordInfo& word_info,
											CSSValue white_space,
											BOOL stop_at_whitespace_separated_words,
											BOOL treat_nbsp_as_space,
											FontSupportInfo* font_info,
											FramesDocument* doc,
											int *in_tag, int *in_tag_out,
											 BOOL convert_ampersand,
											 const WritingSystem::Script script)
{
	BOOL done;
	// Hold the original start point
	const uni_char* wstart = txt;
	int 			fragment_length;
	
	// Don't let the words become wider than WORD_INFO_WIDTH_MAX, which is the limit of WordInfo's bitfield.
	// It would ideally be done by measuring the width here and use SplitWord as layout does, but
	// that would require either more CPU or moving the measuring from its current places.
	const int max_word_length = WORD_INFO_WIDTH_MAX / 32; ///< Assume no wider characters than 32px
	fragment_length = MIN(length, max_word_length);

	done = !::GetNextTextFragment(txt, fragment_length,
								  previous_character, leading_whitespace,
								  word_info, white_space, stop_at_whitespace_separated_words,
								  treat_nbsp_as_space, *font_info, doc, script);

	// Quickfix for Bug 190641. The & character could theoretically be anywhere in the fragment. Not just at the end.
	if (convert_ampersand && word_info.GetLength() > 1 && wstart[word_info.GetLength() - 1] == '&')
	{
		txt = wstart + 1;
		word_info.SetLength(1);
	}

	OP_ASSERT((int) word_info.GetLength() <= max_word_length);
	if (fragment_length < length)
	{
		// More data left
		done = FALSE;
	}

	fragment_length = (txt - wstart);
	length -= fragment_length;

#ifdef COLOURED_MULTIEDIT_SUPPORT
	if (in_tag)
	{
		// "Adjust" the fragment in case it contains "<" or ">"

		const uni_char* wend = wstart;
		int 			divide_tag;
		BOOL 			has_baseline = TRUE, next_has_baseline;

		// Loop again!
		while (fragment_length && ((!uni_html_space(*wend) || *wend == 0x200b || *wend == 0xfeff)|| uni_isidsp(*wend) || (!treat_nbsp_as_space && uni_isnbsp(*wend))))
		{
			next_has_baseline = has_baseline(Unicode::GetUnicodePoint(wend, fragment_length));

			// Divide the fragment based on html tags
			divide_tag = DivideHTMLTagFragment(in_tag, wstart, &wend, &fragment_length, &has_baseline, next_has_baseline);

			if (divide_tag == DIVIDETAG_AGAIN)
				continue;
			else if (divide_tag == DIVIDETAG_DONE)
			{
				// If there is still something left this is an extra break required
				if (fragment_length)
				{
					// Adjust everything for the extra break
					word_info.SetLength(wend - wstart);
					length += fragment_length;
					txt = wend;
				}
				break;
			}

			has_baseline = next_has_baseline;

			wend++;
			fragment_length--;
		}

		// Assign the tag types if the array has been allocated
		if (in_tag_out)
			*in_tag_out = *in_tag;

		// Need to add the ending tag breaks
		if (in_tag && *in_tag > HTT_TAG_SWITCH)
			*in_tag = HTT_NONE;
	}
#endif // COLOURED_MULTIEDIT_SUPPORT

	return done;
}

/** Temporary fragment data used by TempFragmentBlock. */

class TempFragment : public OP_TEXT_FRAGMENT
{
public:
	~TempFragment()
	{
	}
#ifdef COLOURED_MULTIEDIT_SUPPORT
	int tag_type;
#endif
};

/** A block of TempFragment objects. Used by TempFragmentList to reduce allocations. */

class TempFragmentBlock : public Link
{
public:
	TempFragment *fragments;
	UINT32 next_free_fragment;
	UINT32 max_fragment_count;
	TempFragmentBlock(Head *blocklist, UINT32 max_fragment_count) : fragments(NULL), next_free_fragment(0), max_fragment_count(max_fragment_count)
	{
		Into(blocklist);
	}
	~TempFragmentBlock()
	{
		OP_DELETEA(fragments);
		Out();
	}
	bool Init()
	{
		fragments = OP_NEWA(TempFragment, max_fragment_count);
		return fragments ? true : false;
	}
	TempFragment *GetFreeFragment()
	{
		if (next_free_fragment == max_fragment_count)
			return NULL;
		return &fragments[next_free_fragment++];
	}
};

/** Temporary list of OP_TEXT_FRAGMENT for use when getting text fragments, without knowing how many is needed.
	It will use TempFragmentBlock to allocate blocks in larger and larger chunks to avoid many small allocations.
	It would be very convenient to get this into some kind of util pool class */

class TempFragmentList
{
public:
	~TempFragmentList()
	{
		while (blocklist.First())
		{
			TempFragmentBlock *block = (TempFragmentBlock *) blocklist.First();
			OP_DELETE(block);
		}
	}
	TempFragmentBlock *NewBlock(UINT32 max_fragment_count)
	{
		TempFragmentBlock *block = OP_NEW(TempFragmentBlock, (&blocklist, max_fragment_count));
		if (!block || !block->Init())
		{
			OP_DELETE(block);
			return NULL;
		}
		return block;
	}
	TempFragment *Append()
	{
		TempFragmentBlock *block = (TempFragmentBlock *) blocklist.Last();
		if (!block)
		{
			// Create first block
			block = NewBlock(4);
			if (!block)
				return NULL;
		}

		// Get fragment from block
		TempFragment *frag = block->GetFreeFragment();
		if (frag)
			return frag;

		// There was no more room in the block. Get a new block.
		block = NewBlock(MIN(block->max_fragment_count * 2, 16384));
		if (!block)
			return NULL;

		return block->GetFreeFragment();
	}
	void CopyTo(OP_TEXT_FRAGMENT* arr)
	{
		UINT32 i = 0;
		TempFragmentBlock *block = (TempFragmentBlock *) blocklist.First();
		while (block)
		{
			for (UINT32 t = 0; t < block->next_free_fragment; t++)
				arr[i + t] = block->fragments[t];
			i += block->next_free_fragment;
			block = (TempFragmentBlock *) block->Suc();
		}
	}
#ifdef COLOURED_MULTIEDIT_SUPPORT
	void CopyTagTypeTo(UINT32 *arr)
	{
		UINT32 i = 0;
		TempFragmentBlock *block = (TempFragmentBlock *) blocklist.First();
		while (block)
		{
			for (UINT32 t = 0; t < block->next_free_fragment; t++)
				arr[i + t] = block->fragments[t].tag_type;
			i += block->next_free_fragment;
			block = (TempFragmentBlock *) block->Suc();
		}
	}
#endif // COLOURED_MULTIEDIT_SUPPORT
	Head blocklist;
};


OP_STATUS OpTextFragmentList::Update(const uni_char* str, size_t strlen,
									BOOL isRTL, BOOL isOverride,
									BOOL preserveSpaces,
									BOOL treat_nbsp_as_space,
									FramesDocument* frm_doc,
									int original_fontnumber,
									 const WritingSystem::Script script,
									BOOL resolve_order,
									int *start_in_tag,
									BOOL convert_ampersand
									)
{
	m_has_been_split = FALSE;
	if(strlen == 0)
	{
		OP_DELETEA(m_fragments);
		m_fragments = NULL;

#ifdef COLOURED_MULTIEDIT_SUPPORT
		OP_DELETEA(m_tag_type_array);
		m_tag_type_array = NULL;
#endif
		m_num_fragments = 0;

		return OpStatus::OK;
	}

	// Get all textfragments for the string.
    {
		TempFragmentList temp_fragments;
		int len = strlen;
	    BOOL stop_at_whitespace_separated_words = TRUE;
		int num_fragments = 0;
		const uni_char* tmp = str, *word = str;
        BOOL done = FALSE;
        num_fragments = 0;
#ifdef COLOURED_MULTIEDIT_SUPPORT
		int in_tag = HTT_NONE;

		 if (start_in_tag)
		 	in_tag = *start_in_tag;
#endif

		UnicodePoint previous_character = 0;
		BOOL leading_whitespace = FALSE;

		while(!done && len)
		{
			TempFragment *tmpfrag = temp_fragments.Append();
			if (!tmpfrag)
			{
				Clear();
				return OpStatus::ERR_NO_MEMORY;
			}

			FontSupportInfo fsi(original_fontnumber);
			WordInfo& wi = tmpfrag->wi;
			word = tmp;
			int start = word - str;
			tmpfrag->start = start;
			wi.Reset();
			wi.SetStart(MIN(WORD_INFO_START_MAX, start));
#ifdef INTERNAL_SPELLCHECK_SUPPORT
			wi.SetIsMisspelling(FALSE);
#endif // INTERNAL_SPELLCHECK_SUPPORT
			wi.SetFontNumber(fsi.current_font ? fsi.current_font->GetFontNumber() : original_fontnumber);

			{
				done = GetNextTextFragment(tmp, len, previous_character, leading_whitespace,
											wi, preserveSpaces ? CSS_VALUE_pre_wrap : CSS_VALUE_nowrap, stop_at_whitespace_separated_words,
											treat_nbsp_as_space, &fsi, frm_doc,
#ifdef COLOURED_MULTIEDIT_SUPPORT
											start_in_tag ? &in_tag : NULL, &tmpfrag->tag_type,
#else
											NULL, NULL,
#endif
											convert_ampersand,
											script);
			}

			leading_whitespace = wi.HasTrailingWhitespace();

			tmpfrag->wi = wi;
			tmpfrag->nextidx = num_fragments;
			num_fragments++;
		}

		// Allocate new fragments
		OP_TEXT_FRAGMENT *new_fragment = OP_NEWA(OP_TEXT_FRAGMENT, num_fragments);
		if (new_fragment == NULL)
		{
			Clear();
			return OpStatus::ERR_NO_MEMORY;
		}
		temp_fragments.CopyTo(new_fragment);

#ifdef COLOURED_MULTIEDIT_SUPPORT
		// Only use text colouring if a start_in_tag object exists
		if (start_in_tag)
		{
			// Allocate the colours
			UINT32 *new_colour_array = OP_NEWA(UINT32, num_fragments);
			if (new_colour_array == NULL)
			{
				Clear();
				OP_DELETEA(new_fragment);
				return OpStatus::ERR_NO_MEMORY;
			}

			temp_fragments.CopyTagTypeTo(new_colour_array);

			OP_DELETEA(m_tag_type_array);
			m_tag_type_array = new_colour_array;
		}
#endif

#ifdef INTERNAL_SPELLCHECK_SUPPORT
		if(m_fragments) // copy the previous is_misspelling flags for preventing visual "flickering" (when the red line dissapears, and soon re-appears after spellchecking)
		{
			int i, count = MIN(m_num_fragments, (UINT32)num_fragments);
			for(i=0;i<count;i++)
				new_fragment[i].wi.SetIsMisspelling(m_fragments[i].wi.IsMisspelling());
		}
#endif // INTERNAL_SPELLCHECK_SUPPORT

		// Replace old fragments
		OP_DELETEA(m_fragments);
		m_fragments = NULL;

		m_fragments = new_fragment;
		m_num_fragments = num_fragments;
	}

#ifdef SUPPORT_TEXT_DIRECTION
	Head paragraph_bidi_segments;
	BidiCalculation calc;
	UINT32 i = 0;

	calc.Reset();
	calc.SetSegments(&paragraph_bidi_segments);
	calc.SetParagraphLevel(isRTL ? BIDI_R : BIDI_L, isOverride);

	for(i = 0; i < m_num_fragments; i++)
	{
		WordInfo& wi = m_fragments[i].wi;
		const uni_char* word = &str[m_fragments[i].start];
		BidiCategory bidicategory = Unicode::GetBidiCategory(word[0]);
		calc.AppendStretch(bidicategory, m_fragments[i].wi.GetLength() + (wi.HasTrailingWhitespace() ? 1 : 0), NULL, m_fragments[i].start);
	}

	calc.PopLevel();	// Should it be here or not?
	calc.HandleNeutralsAtRunChange();

#ifdef WIDGETS_ELLIPSIS_SUPPORT
	unsigned num_mirrored_fragments = 0;
#endif

	ParagraphBidiSegment* segment = (ParagraphBidiSegment*) paragraph_bidi_segments.First();

	for(i = 0; i < m_num_fragments; i++)
	{
		if(segment && segment->Suc() && m_fragments[i].start >= ((ParagraphBidiSegment*)segment->Suc())->virtual_position)
		{
			segment = (ParagraphBidiSegment*)segment->Suc();
		}

		if (segment)
		{
			m_fragments[i].packed.level = segment->level;
			m_fragments[i].packed.bidi = segment->level % 2 == 1 ? BIDI_FRAGMENT_MIRRORED : BIDI_FRAGMENT_NORMAL;
		}
		else
		{
			// Crashfix. We get no segments in some cases (when they are not needed?)
			m_fragments[i].packed.level = 0;
			m_fragments[i].packed.bidi = BIDI_FRAGMENT_NORMAL;
		}
#ifdef WIDGETS_ELLIPSIS_SUPPORT
		num_mirrored_fragments += m_fragments[i].packed.bidi == BIDI_FRAGMENT_MIRRORED;
#endif
	}

#ifdef WIDGETS_ELLIPSIS_SUPPORT
	m_mostly_mirrored = num_mirrored_fragments > m_num_fragments / 2;
#endif

	paragraph_bidi_segments.Clear();

	if (resolve_order)
		ResolveOrder(isRTL, 0, m_num_fragments);
#endif // SUPPORT_TEXT_DIRECTION
	return OpStatus::OK;
}

void OpTextFragmentList::MoveSpellFlagsForwardAfter(UINT32 after_ofs, int steps)
{
	int i,j;
	OP_ASSERT(steps > 0);
	for(i = m_num_fragments-1; i >= 0 ; i--)
	{
		if(i < steps || Get(i)->start <= (INT32)after_ofs)
		{
			for(j = steps; i >= 0 && j > 0; j--, i--)
				Get(i)->wi.SetIsMisspelling(FALSE);
			break;
		}
		Get(i)->wi.SetIsMisspelling(Get(i-steps)->wi.IsMisspelling());
	}
}

void OpTextFragmentList::ResetOrder()
{
	for(UINT32 i = 0; i < m_num_fragments; i++)
		m_fragments[i].nextidx = i;
}

void OpTextFragmentList::ResolveOrder(BOOL isRTL, int idx, int num)
{
#ifdef SUPPORT_TEXT_DIRECTION
	if (!m_fragments)
		return;

	int i, num_levels = 0;
	for(i = 0; i < num; i++)
		num_levels = MAX(num_levels, m_fragments[i + idx].packed.level);

	// Turn around all fragments for each level
	for(int k = num_levels; k >= 1; k--)
	{
		for(int frag = 0; frag < num; )
		{
			int i = frag + idx;
			if (m_fragments[i].packed.level >= k)
			{
				int count = 0;
				while(i + count < idx + num && m_fragments[i + count].packed.level >= k)
					count++;
				for(int j = 0; j < count/2; j++)
				{
					int idx1 = i + j;
					int idx2 = i - j + count - 1;
					int tmp = m_fragments[idx1].nextidx;
					m_fragments[idx1].nextidx = m_fragments[idx2].nextidx;
					m_fragments[idx2].nextidx = tmp;
				}
				frag += count;
			}
			else
				frag++;
		}
	}
#endif // SUPPORT_TEXT_DIRECTION
}

unsigned int FragmentExtentFromTo(OP_TEXT_FRAGMENT* frag,
                                  const uni_char* word,
                                  unsigned int from, unsigned int to,
                                  VisualDevice* vis_dev, int text_format);

OP_STATUS OpTextFragmentList::Split(OP_TCINFO* info, const uni_char* str,
									const UINT32 idx, const UINT32 max_width, UINT32& next_width)
{
	if (idx >= m_num_fragments)
		return OpStatus::ERR;

	// not really an error, but have to report that no split can be done
	if (!max_width)
		return (OP_STATUS)1;

	const uni_char* text = str + m_fragments[idx].start;
	const UINT32 length = m_fragments[idx].wi.GetLength();

	// store visual device's font number
	UINT32 old_font = info->vis_dev->GetCurrentFontNumber();
	// set visual device's font to that of this fragment
	info->vis_dev->SetFont(m_fragments[idx].wi.GetFontNumber());

	UINT32 new_width, new_length;
	int text_format = 0;
	if (m_fragments[idx].packed.bidi & BIDI_FRAGMENT_MIRRORED)
		text_format |= TEXT_FORMAT_REVERSE_WORD;
	info->vis_dev->GetTxtExtentSplit(text, length, max_width, text_format, new_width, new_length);
	OP_ASSERT(new_width <= max_width);
	OP_ASSERT(new_length < length);
	next_width = new_length ?
		FragmentExtentFromTo(m_fragments+idx, text, new_length, length, info->vis_dev, text_format) :
		0;

	// restore visual device
	info->vis_dev->SetFont(old_font);

	// couldn't split because first glyph is wider than width - not an
	// error, but need to report that no split can be done
	if (new_length == 0)
		return (OP_STATUS)1;

	UnicodeStringIterator sit(str + m_fragments[idx].start, KAll, new_length);
	OP_ASSERT(!sit.IsAtBeginning());
	sit.Previous(); // Move past end sentinel
	const UnicodePoint last = sit.At();

	OP_STATUS status = Split(idx, new_length, new_width, next_width, last);
	return status;
}

OP_STATUS OpTextFragmentList::Split(const UINT32 idx, const UINT32 offs, const UINT32 new_width, const UINT32 next_width, UnicodePoint last)
{
	// sanity checks
	if (idx >= m_num_fragments)
		return OpStatus::ERR;
	if (!offs || m_fragments[idx].wi.GetLength() <= offs)
		return OpStatus::ERR;

	// new text fragment list
	const UINT32 num_fragments = m_num_fragments + 1;
	OP_TEXT_FRAGMENT* new_fragments = OP_NEWA(OP_TEXT_FRAGMENT, num_fragments);
	if (!new_fragments)
		return OpStatus::ERR_NO_MEMORY;

	// copy contents of old text fragment list to new one
	const UINT32 first = idx + 1;
	op_memcpy(new_fragments, m_fragments, first * sizeof(OP_TEXT_FRAGMENT));
	op_memcpy(new_fragments + first + 1, m_fragments + first,
			  (m_num_fragments - first) * sizeof(OP_TEXT_FRAGMENT));

	// update the split and new fragments
	OP_TEXT_FRAGMENT* splitfrag = &new_fragments[idx];
	OP_TEXT_FRAGMENT* newfrag = &new_fragments[idx+1];
	op_memcpy(newfrag, splitfrag, sizeof(OP_TEXT_FRAGMENT));

	// UGLY: using last character to determine trailing/tab stuff
	splitfrag->wi.SetHasTrailingWhitespace(uni_isspace(last) && !uni_isnbsp(last));
	splitfrag->wi.SetHasEndingNewline(last == '\r' || last == '\n');
	splitfrag->wi.SetIsTabCharacter(last == '\t');

	splitfrag->wi.SetLength(offs);
	newfrag->wi.SetLength(newfrag->wi.GetLength() - offs);
	newfrag->wi.SetCanLinebreakBefore(TRUE);
	newfrag->start += offs;
	newfrag->wi.SetStart(MIN(WORD_INFO_START_MAX, newfrag->start));
	splitfrag->wi.SetWidth(MIN(new_width, WORD_INFO_WIDTH_MAX));

	// Replace old fragments
	OP_DELETEA(m_fragments);
	m_fragments = new_fragments;
	m_num_fragments = num_fragments;

	// fix order
	for (UINT32 i = 0; i < m_num_fragments; ++i)
		if (m_fragments[i].nextidx > idx)
			++m_fragments[i].nextidx;
	splitfrag->nextidx = idx;
	newfrag->nextidx = idx + 1;
	ResolveOrder(FALSE, idx, 2);

	m_has_been_split = TRUE;
	return OpStatus::OK;
}

#ifdef COLOURED_MULTIEDIT_SUPPORT

UINT32 OpTextFragmentList::GetTagType(OP_TEXT_FRAGMENT *frag)
{
	return GetTagType(frag->nextidx);
}

#endif // COLOURED_MULTIEDIT_SUPPORT
