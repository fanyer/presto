/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

#include "core/pch.h"
#include "modules/mdefont/processedstring.h"

BOOL AdjustToWidth(ProcessedString& processed_string, const short word_width)
{
	OP_ASSERT(word_width > 0);
	int adjust = word_width - processed_string.m_advance;
	OP_ASSERT(adjust);

	processed_string.m_advance = word_width;

	if (adjust < 0)
	{
		int adjust_step;
		unsigned adjust_count;
		adjust_count = processed_string.m_length / 2;

		adjust = -adjust;
		adjust_step = -1;

		ProcessedGlyph* glyphs = processed_string.m_processed_glyphs;
		int ex = 0;
		for (size_t i = 0; i < processed_string.m_length; ++i)
		{
			adjust_count += adjust;
			while (adjust_count >= processed_string.m_length)
			{
				ex += adjust_step;
				adjust_count -= processed_string.m_length;
			}
			glyphs[i].m_pos.x += ex;
		}

		return TRUE;
	}
	else
	{
		/*if we need to put the pixels between words, let's do it in this way:
			- we only increase spaces between the letters for all letters or none.
			  those can't be put between letters go before or after the word.
			e.g.:
			  if you have word abcde. and you need to add 7 pixels to the string.
			what we do is :
			  add 1 pixel to the space between each letters for the rest 3 pixels, 1 go to
			the front and 2 go to the end.
		*/
		size_t slot = processed_string.m_length - 1;
		size_t pixels_to_add = (slot <=1 ) ? slot : (adjust / slot);
		size_t pixels_at_front = (adjust - pixels_to_add*slot) >> 1;
		if (pixels_to_add || pixels_at_front)
		{
			ProcessedGlyph* glyphs = processed_string.m_processed_glyphs;
			glyphs[0].m_pos.x += pixels_at_front;
			size_t pixels_adjust = pixels_at_front + pixels_to_add;
			for (size_t i = 1; i < processed_string.m_length; ++i)
			{
				glyphs[i].m_pos.x += pixels_adjust;
				if (pixels_to_add)pixels_adjust += pixels_to_add;
			}
		}

		return FALSE;
	}
}
