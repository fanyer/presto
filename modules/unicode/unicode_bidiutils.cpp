/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Magnus Gasslander
*/

#include "core/pch.h"

#if defined SUPPORT_TEXT_DIRECTION

#include "modules/unicode/unicode_bidiutils.h"
#include "modules/style/css.h"

// BidiLevelStack

BidiCategory BidiLevelStack::GetOverrideStatus() const
{
	OP_ASSERT(stack_top >= 0 && static_cast<size_t>(stack_top) < ARRAY_SIZE(stack));
	if (SlotOverride(stack[stack_top]))
	{
		if (SlotLevel(stack[stack_top]) % 2 == 0)
			return BIDI_L;
		return BIDI_R;
	}

	return BIDI_UNDEFINED;
}

int BidiLevelStack::GetResolvedLevel(BidiCategory type) const
{
	int current_level = GetCurrentLevel();

	if (type == BIDI_AN || type == BIDI_EN)
	{
		if (current_level % 2 == 0)
			return current_level + 2;
		else
			return current_level + 1;
	}

	if ((type == BIDI_L && current_level % 2 == 1) ||
		(type == BIDI_R && current_level % 2 == 0))
		return current_level + 1;

	return current_level;
}

BidiCategory BidiLevelStack::GetEmbeddingDir() const
{
	int current_level = GetCurrentLevel();

	if (current_level % 2 == 0)
		return BIDI_L;
	else
		return BIDI_R;
}

void BidiLevelStack::PushLevel(int direction, bool is_override)
{
	// Check for maximum explicit depth level according to standard.
	if (stack_top >= MaximumDepth)
		return;

	// X2 - X5
	stack_top++;
	int previous_level = SlotLevel(stack[stack_top - 1]);

	OP_ASSERT(direction == CSS_VALUE_rtl || direction == CSS_VALUE_ltr);

	bool prev_level_is_even = (previous_level % 2) == 0;

	int level_increment;
	if (direction == CSS_VALUE_rtl)
	{
		// X2 & X4
		if (prev_level_is_even)
			level_increment = 1;
		else
			level_increment = 2;
	}
	else /* direction == CSS_VALUE_ltr */
	{
		// X3 & X5
		if (!prev_level_is_even)
			level_increment = 1;
		else
			level_increment = 2;
	}

	stack[stack_top] = EncodeSlot(previous_level + level_increment, is_override);
}

// BidiCalculation

OP_STATUS
BidiCalculation::PushLevel(int direction, BOOL is_override)
{
	previous_embedding_level = level_stack.GetCurrentLevel();

	level_stack.PushLevel(direction, !!is_override);

	// eor is calculated after the push - the highest value.

	eor_level = level_stack.GetCurrentLevel();
	eor_type = (eor_level % 2 == 0) ? BIDI_L : BIDI_R;

	OP_STATUS status = HandleNeutralsAtRunChange();

	if (status != OpStatus::OK)
		return status;

	last_segment_type = last_strong_type = last_type = W1_last_type = W2_last_strong_type = W7_last_strong_type = eor_type;

	return OpStatus::OK;
}

OP_STATUS BidiCalculation::PopLevel()
{
	previous_embedding_level = level_stack.GetCurrentLevel();

    // eor is calculated before the pop. It should always be based on the highest value.

	eor_level = level_stack.GetCurrentLevel();
	eor_type = (eor_level % 2 == 0) ? BIDI_L : BIDI_R;

	level_stack.PopLevel();

	OP_STATUS status = HandleNeutralsAtRunChange();

	if (status != OpStatus::OK)
		return status;

	last_segment_type = last_strong_type = last_type = W1_last_type = W2_last_strong_type = W7_last_strong_type = eor_type;

	return OpStatus::OK;
}


/**
 * Set the paragraph level.
 *
 * We _don't_ set it according to the first strong type as per the
 * BiDi spec. Instead we only look at the direction property in the
 * tag, to be compatible with browsers.
 */
void BidiCalculation::SetParagraphLevel(BidiCategory type, BOOL override)
{
	level_stack.SetParagraphLevel(type, !!override);
	W1_last_type = W2_last_strong_type = W7_last_strong_type = last_strong_type = paragraph_type = type;
}




/**
 * This is the basic method used to create a new segment
 */

OP_STATUS BidiCalculation::CreateSegment(long width, HTML_Element* start_element, unsigned short level, long virtual_position)
{
	ParagraphBidiSegment* seg = OP_NEW(ParagraphBidiSegment, (width, start_element, level, virtual_position));
	if (!seg)
		return OpStatus::ERR_NO_MEMORY;

	OP_ASSERT(segments);
	seg->Into(segments);
	previous_segment_level = level;
	return OpStatus::OK;
}

void BidiCalculation::ExpandCurrentSegment(int width)
{
	GetCurrentSegment()->width += width;
}

/**
 * Will expand the current segment.
 *
 * If this is the very first segment or if the embedding level has changed,
 * and this is the first segment of the new level, we will create a new segment.
 *
 */

OP_STATUS BidiCalculation::ExpandCurrentSegmentOrCreateFirst(BidiCategory type, int width, HTML_Element* start_element, long virtual_position)
{
	if (segments->Empty() ||
		GetCurrentSegment()->level == 63 ||
		previous_embedding_level != level_stack.GetCurrentLevel() ||
		last_segment_type != type)
	{
		// We are at the first run or at a (embedding level ||
		// resolved level) that differs from the last.
		OP_STATUS status = CreateSegment(width, start_element, level_stack.GetResolvedLevel(type), virtual_position);
		if (status != OpStatus::OK)
			return status;

		previous_embedding_level = level_stack.GetCurrentLevel();
	}
	else
	{
		ExpandCurrentSegment(width);
	}


	last_segment_type = type;
	return OpStatus::OK;
}



/**
 * Will create a brand new segment, using the type and the current embedding level.
 */

OP_STATUS BidiCalculation::CreateBidiSegment(BidiCategory type, int width, HTML_Element* start_element, long virtual_position)
{
	OP_STATUS status = CreateSegment(width, start_element, level_stack.GetResolvedLevel(type), virtual_position);

	if (status != OpStatus::OK)
		return status;

	previous_embedding_level = level_stack.GetCurrentLevel();
	last_segment_type = type;

	return OpStatus::OK;
}


/**
 * previous_embedding_level is the level before the push/pop. This is where the segment actually belongs
 * eor_type is the type of the eor that was determined by the push/pop
 * previous_segment_level is the level of the last vreated segment. This is used to determine if we can just add
 *         to the last segment or if we need to create a new segment.
 *
 * FIXME too complicated!!
 */

OP_STATUS BidiCalculation::HandleNeutralsAtRunChange()
{
	if (W4_pending_type != BIDI_UNDEFINED)
	{
		neutral_width = W4_width;
		neutral_start_element = W4_start_element;
		neutral_virtual_position_start = W4_virtual_position;
		W4_pending_type = BIDI_UNDEFINED;
	}

	if (W5_type_before != BIDI_UNDEFINED)
	{
		if (IsNeutral(W5_type_before))
			neutral_width += W5_width;
		else
		{
			neutral_width = W5_width;
			neutral_start_element = W5_start_element;
			neutral_virtual_position_start = W5_virtual_position;
		}
		W5_type_before = BIDI_UNDEFINED;
	}

	if (!IsNeutral(last_type) && last_type != BIDI_CS && last_type != BIDI_ES && last_type != BIDI_ET)
		return OpStatus::OK;


	if (last_segment_type == BIDI_EN || last_segment_type == BIDI_AN)
	{
		// make the a BIDI_R segment from the neutral

		int level_to_create = previous_embedding_level;

		if (eor_type == BIDI_R && previous_embedding_level % 2 == 0) // two "R's (or EN/AN)" in a row should not follow the embedding level
			level_to_create++;

		OP_STATUS status = CreateSegment(neutral_width, neutral_start_element, level_to_create, neutral_virtual_position_start);

		if (status != OpStatus::OK)
			return status;

		last_segment_type = (previous_embedding_level % 2 == 0) ? BIDI_L : BIDI_R;
	}
	/* either eor_type is the same as the last strong type or the
	   embedding direction is the same as the segment created last.

	   Note: this will not be triggered if the last strong type is
	   BIDI_UNDEFINED. That is good. */
	else
		if (( (eor_type == last_strong_type && previous_segment_level == eor_level) ||
			 previous_embedding_level == previous_segment_level) &&
			!segments->Empty() &&
			GetCurrentSegment()->level != 63)
		{
			ExpandCurrentSegment(neutral_width);
		}
		else
		{
			// make a segment with the previous embedding direction/level

			int segment_level;

			if (eor_type == last_strong_type)
			{
				if (previous_embedding_level % 2 == 0 && eor_type == BIDI_L || previous_embedding_level % 2 == 1 && eor_type == BIDI_R)
					segment_level = previous_embedding_level;
				else
					segment_level = previous_embedding_level + 1;
			}
			else
				segment_level = previous_embedding_level;

			OP_STATUS status = CreateSegment(neutral_width, neutral_start_element, segment_level, neutral_virtual_position_start);

			if (status != OpStatus::OK)
				return status;

			last_segment_type = (previous_embedding_level % 2 == 0) ? BIDI_L : BIDI_R;
		}

	return OpStatus::OK;
}





long
BidiCalculation::AppendStretch(BidiCategory initial_bidi_type, int width, HTML_Element* element, long virtual_position)
{
	BOOL word_handled = FALSE;
	OP_STATUS status = OpStatus::OK;
	BidiCategory bidi_type = initial_bidi_type; // needed to record some states

	long retval = LONG_MAX - 1;


	if (bidi_type != BIDI_RLO &&
		bidi_type != BIDI_RLE &&
		bidi_type != BIDI_LRO &&
		bidi_type != BIDI_LRE &&
		bidi_type != BIDI_PDF)
	{
		BidiCategory override = level_stack.GetOverrideStatus();

		if (override != BIDI_UNDEFINED)
			bidi_type = override;
	}

	while (!word_handled)
	{
		word_handled = TRUE; // by default all words are handled in the first run

		switch (bidi_type)
		{
        case BIDI_UNDEFINED:
//			OP_ASSERT(0); // dont really fancy undefined types
			if (last_type != BIDI_UNDEFINED)
				bidi_type = last_type;
			else
				bidi_type = BIDI_L;

			word_handled = FALSE;
			break;

			// strong types
		case BIDI_RLO:
			PushLevel(CSS_VALUE_rtl, TRUE);
			break;

		case BIDI_RLE:
			PushLevel(CSS_VALUE_rtl, FALSE);
			break;

		case BIDI_LRO:
			PushLevel(CSS_VALUE_ltr, TRUE);
			break;

		case BIDI_LRE:
			PushLevel(CSS_VALUE_ltr, FALSE);
			break;

		case BIDI_L:
			if (paragraph_type == BIDI_UNDEFINED)
				SetParagraphLevel(BIDI_L, FALSE);

			switch (last_type)
			{
			case BIDI_ET: // any pending W4 & W5 stretches should be converted to neutrals
			case BIDI_CS:
			case BIDI_ES:
				if (last_type == BIDI_ET)
				{
					if (IsNeutral(W5_type_before))
						neutral_width += W5_width;
					else
					{
						neutral_width = W5_width;
						neutral_start_element = W5_start_element;
						neutral_virtual_position_start = W5_virtual_position;
					}
				}

				else
					if (W4_pending_type != BIDI_UNDEFINED)
					{
						neutral_width = W4_width;
						neutral_start_element = W4_start_element;
						neutral_virtual_position_start = W4_virtual_position;
						last_type = BIDI_ON;
					}

				// fall through
			case BIDI_S:
			case BIDI_WS:
			case BIDI_B:
			case BIDI_ON:
				if (level_stack.GetEmbeddingDir() != last_strong_type)
					retval = neutral_virtual_position_start;

				if (last_strong_type == BIDI_L)
				{
					status = ExpandCurrentSegmentOrCreateFirst(BIDI_L, width + neutral_width, neutral_start_element, neutral_virtual_position_start);

					if (status != OpStatus::OK)
						return LONG_MAX;
				}
				else
					if (last_strong_type == BIDI_R)
					{
						if (level_stack.GetEmbeddingDir() == BIDI_L)
						{
							status = CreateBidiSegment(BIDI_L, width + neutral_width, neutral_start_element, neutral_virtual_position_start);

							if (status != OpStatus::OK)
								return LONG_MAX;
						}
						else
						{
							status = ExpandCurrentSegmentOrCreateFirst(BIDI_R, neutral_width, neutral_start_element, neutral_virtual_position_start);

							if (status == OpStatus::OK)
								status = CreateBidiSegment(BIDI_L, width, element, virtual_position);

							if (status != OpStatus::OK)
								return LONG_MAX;
						}

					}
					else
						OP_ASSERT(0); // this means that last_strong_type is undefined. or worse...
				break;

			case BIDI_UNDEFINED:
			case BIDI_L:
				// add to the current segment
				status = ExpandCurrentSegmentOrCreateFirst(BIDI_L, width, element, virtual_position);

				if (status != OpStatus::OK)
					return LONG_MAX;

				break;

			case BIDI_EN:
			case BIDI_AN:
			case BIDI_R:
				// create a new segment
				status = CreateBidiSegment(BIDI_L, width, element, virtual_position);

				if (status != OpStatus::OK)
					return LONG_MAX;

				break;

			default:
				break;
			}

			W2_last_strong_type = last_strong_type = BIDI_L;
			last_type = BIDI_L;
			break;

		case BIDI_AL:
			// W3

			bidi_type = BIDI_R;
			word_handled = FALSE;
			break;

		case BIDI_R:
			if (paragraph_type == BIDI_UNDEFINED)
				SetParagraphLevel(BIDI_R, FALSE); //FIXME mg should this be allowed ?

			switch (last_type)
			{
			case BIDI_ES: // any pending W4 & W5 stretches should be converted to neutrals
			case BIDI_CS:
			case BIDI_ET:
				if (last_type == BIDI_ET)
				{
					if (IsNeutral(W5_type_before))
						neutral_width += W5_width;
					else
					{
						neutral_width = W5_width;
						neutral_start_element = W5_start_element;
						neutral_virtual_position_start = W5_virtual_position;
					}
				}
				else
					if (W4_pending_type != BIDI_UNDEFINED) // which means that the word before this was a
					{
						neutral_width = W4_width;
						neutral_start_element = W4_start_element;
						neutral_virtual_position_start = W4_virtual_position;
						last_type = BIDI_ON;
					}

				// fall through (W4 & W5)

			case BIDI_S:
			case BIDI_WS:
			case BIDI_B:
			case BIDI_ON:
				if(level_stack.GetEmbeddingDir() != last_strong_type)
					retval = neutral_virtual_position_start;
				if (last_strong_type == BIDI_R)
				{
					status = ExpandCurrentSegmentOrCreateFirst(BIDI_R, width + neutral_width, neutral_start_element, neutral_virtual_position_start);

					if (status != OpStatus::OK)
						return LONG_MAX;
				}
				else
					if (last_strong_type == BIDI_L)
					{
						if (level_stack.GetEmbeddingDir() == BIDI_R)
						{
							status = CreateBidiSegment(BIDI_R, width + neutral_width, neutral_start_element, neutral_virtual_position_start);

							if (status != OpStatus::OK)
								return LONG_MAX;
						}
						else
						{
							status = ExpandCurrentSegmentOrCreateFirst(BIDI_L, neutral_width, neutral_start_element, neutral_virtual_position_start);

							if (status == OpStatus::OK)
								status = CreateBidiSegment(BIDI_R, width, element, virtual_position);

							if (status != OpStatus::OK)
								return LONG_MAX;
						}
					}
					else
						OP_ASSERT(0); // this means that last strong type is undefined (or worse...)
				// resolve the neutral
				break;

			case BIDI_EN:
			case BIDI_AN:
			case BIDI_L:
				status = CreateBidiSegment(BIDI_R, width, element, virtual_position);

				if (status != OpStatus::OK)
					return LONG_MAX;

				// create a new segment
				break;

			case BIDI_UNDEFINED:
				// fall through
			case BIDI_R:
				// add to the current segment

				status = ExpandCurrentSegmentOrCreateFirst(BIDI_R, width, element, virtual_position);

				if (status != OpStatus::OK)
					return LONG_MAX;

				break;

			default:
				break;
			}

			last_strong_type = BIDI_R;
			last_type = BIDI_R;

			break;

			// weak
		case BIDI_PDF:
			//PopLevel();
			break;

		case BIDI_EN:
			if (W2_last_strong_type == BIDI_AL)
			{
				bidi_type = BIDI_AN;
				word_handled = FALSE;
			}
			else
				if (W7_last_strong_type == BIDI_L)
				{
					// W7
					bidi_type = BIDI_L;
					word_handled = FALSE;
				}
				else
				{
					switch (last_type)
					{
					case BIDI_ET:
						width += W5_width;
						element = W5_start_element;
						virtual_position = W5_virtual_position;

						if (IsNeutral(W5_type_before))
						{
							if (last_strong_type == BIDI_R || level_stack.GetEmbeddingDir() == BIDI_R)
							{
								// FIXME mg maybe make sure this only creates a segment when necessary

								status = CreateBidiSegment(BIDI_R, neutral_width, neutral_start_element, neutral_virtual_position_start);

								if (status == OpStatus::OK)
									status = CreateBidiSegment(BIDI_EN, width, element, virtual_position);

								if (status != OpStatus::OK)
									return LONG_MAX;
							}
							else
							{
								// FIXME mg maybe make sure this only creates a segment when necessary

								status = CreateBidiSegment(BIDI_L, neutral_width, neutral_start_element, neutral_virtual_position_start);

								if (status == OpStatus::OK)
									status = CreateBidiSegment(BIDI_EN, width, element, virtual_position);

								if (status != OpStatus::OK)
									return LONG_MAX;
							}
						}
						else
						{
							// AN, L or R (can be no EN here)

							status = CreateBidiSegment(BIDI_EN, width, element, virtual_position);

							if (status != OpStatus::OK)
								return LONG_MAX;
						}
						break;

					case BIDI_ES:
					case BIDI_CS:
						if (W4_pending_type == BIDI_EN)
						{
							status = ExpandCurrentSegmentOrCreateFirst(BIDI_EN, W4_width + width, W4_start_element, W4_virtual_position);

							if (status != OpStatus::OK)
								return LONG_MAX;
						}
						else
						{
							OP_ASSERT(W4_pending_type == BIDI_AN);

							if (last_strong_type == BIDI_R || level_stack.GetEmbeddingDir() == BIDI_R)
							{
								// FIXME mg maybe make sure this only creates a segment when necessary

								status = CreateBidiSegment(BIDI_R, W4_width, W4_start_element, W4_virtual_position);

								if (status == OpStatus::OK)
									status = CreateBidiSegment(BIDI_EN, width, element, virtual_position);

								if (status != OpStatus::OK)
									return LONG_MAX;
							}
							else
							{
								// FIXME mg maybe make sure this only creates a segment when necessary

								status = CreateBidiSegment(BIDI_L, W4_width, W4_start_element, W4_virtual_position);

								if (status == OpStatus::OK)
									status = CreateBidiSegment(BIDI_EN, width, element, virtual_position);

								if (status != OpStatus::OK)
									return LONG_MAX;
							}
						}
						break;

					case BIDI_S:
					case BIDI_WS:
					case BIDI_B:
					case BIDI_ON:
						if (level_stack.GetEmbeddingDir() != last_strong_type)
							retval = neutral_virtual_position_start;
						if (last_strong_type == BIDI_R || level_stack.GetEmbeddingDir() == BIDI_R)
						{
							// FIXME mg maybe make sure this only creates a segment when necessary

							status = CreateBidiSegment(BIDI_R, neutral_width, neutral_start_element, neutral_virtual_position_start);

							if (status == OpStatus::OK)
								status = CreateBidiSegment(BIDI_EN, width, element, virtual_position);

							if (status != OpStatus::OK)
								return LONG_MAX;
						}
						else
						{
							// FIXME mg maybe make sure this only creates a segment when necessary

							status = CreateBidiSegment(BIDI_L, neutral_width, neutral_start_element, neutral_virtual_position_start);

							if (status == OpStatus::OK)
								status = CreateBidiSegment(BIDI_EN, width, element, virtual_position);

							if (status != OpStatus::OK)
								return LONG_MAX;
						}
						break;

					case BIDI_L:
						status = CreateBidiSegment(BIDI_EN, width, element, virtual_position);

						if (status != OpStatus::OK)
							return LONG_MAX;

						// create a new segment
						break;

					case BIDI_EN:
						// add to the current segment
						status = ExpandCurrentSegmentOrCreateFirst(BIDI_EN, width, element, virtual_position);

						if (status != OpStatus::OK)
							return LONG_MAX;

						break;

					case BIDI_UNDEFINED:
						// fall through
					case BIDI_AN:
					case BIDI_R:
						status = CreateBidiSegment(BIDI_EN, width, element, virtual_position);

						if (status != OpStatus::OK)
							return LONG_MAX;

						break;

					default:
						break;
					}

					last_strong_type = BIDI_R;
					last_type = BIDI_EN;
				}

			// W2 & W7
			break;

		case BIDI_ES:
			if (last_type == BIDI_EN)
			{
				W4_width = width;
				W4_start_element = element;
				W4_virtual_position = virtual_position;
				W4_pending_type = last_type;
				last_type = BIDI_ES;
			}
			else
			{
				bidi_type = BIDI_ON;
				word_handled = FALSE;
			}	// W4
			break;

		case BIDI_ET:
			if (W2_last_strong_type == BIDI_L)  // W7 in effect - there can be no W5
			{
				if (W5_last_type == BIDI_EN)
					bidi_type = BIDI_EN;
				else
					bidi_type = BIDI_ON;

				word_handled = FALSE;
			}
			else
			{
				switch (last_type)
				{
				case BIDI_EN:
					bidi_type = BIDI_EN;
					word_handled = FALSE;
					break;

				case BIDI_ET:
					W5_width += width;
					break;

				case BIDI_ES:
				case BIDI_CS: // cancel W4 rule here
					if (W4_pending_type != BIDI_UNDEFINED)
					{
						neutral_width = W4_width;
						neutral_start_element = W4_start_element;
						neutral_virtual_position_start = W4_virtual_position;
						W5_type_before = BIDI_ON;
						W5_width = width;
						W5_start_element = element;
						W5_virtual_position = virtual_position;
					}
					last_type = BIDI_ET;
					break;

				default:
					W5_type_before = last_type;
					W5_width = width;
					W5_start_element = element;
					W5_virtual_position = virtual_position;
					last_type = BIDI_ET;
					break;
				}
			}
			break;

		case BIDI_AN:
			switch (last_type)
			{
			case BIDI_ES:
				if (last_strong_type == BIDI_R || level_stack.GetEmbeddingDir() == BIDI_R)
				{
					// FIXME mg maybe make sure this only creates a segment when necessary

					status = CreateBidiSegment(BIDI_R, W4_width, W4_start_element, W4_virtual_position);

					if (status == OpStatus::OK)
						status = CreateBidiSegment(BIDI_AN, width, element, virtual_position);

					if (status != OpStatus::OK)
						return LONG_MAX;
				}
				else
				{
					// FIXME mg maybe make sure this only creates a segment when necessary

					status = CreateBidiSegment(BIDI_L, W4_width, W4_start_element, W4_virtual_position);

					if (status == OpStatus::OK)
						status = CreateBidiSegment(BIDI_AN, width, element, virtual_position);

					if (status != OpStatus::OK)
						return LONG_MAX;
				}
				break;

			case BIDI_CS:
				if (W4_pending_type == BIDI_AN)
				{
					status = ExpandCurrentSegmentOrCreateFirst(BIDI_AN, W4_width + width, W4_start_element, W4_virtual_position);

					if (status != OpStatus::OK)
						return LONG_MAX;
				}
				else
				{
					OP_ASSERT(W4_pending_type == BIDI_EN);

					if (last_strong_type == BIDI_R || level_stack.GetEmbeddingDir() == BIDI_R)
					{
						// FIXME mg maybe make sure this only creates a segment when necessary

						status = CreateBidiSegment(BIDI_R, W4_width, W4_start_element, W4_virtual_position);

						if (status == OpStatus::OK)
							status = CreateBidiSegment(BIDI_AN, width, element, virtual_position);

						if (status != OpStatus::OK)
							return LONG_MAX;
					}
					else
					{
						// FIXME mg maybe make sure this only creates a segment when necessary

						status = CreateBidiSegment(BIDI_L, W4_width, W4_start_element, W4_virtual_position);

						if (status == OpStatus::OK)
							status = CreateBidiSegment(BIDI_AN, width, element, virtual_position);

						if (status != OpStatus::OK)
							return LONG_MAX;
					}
				}
				break;

			case BIDI_ET:
				// convert pending W5 stretches to neutrals

				if (IsNeutral(W5_type_before))
					neutral_width += W5_width;
				else
				{
					neutral_width = W5_width;
					neutral_start_element = W5_start_element;
					neutral_virtual_position_start = W5_virtual_position;
				}
				// fall through

			case BIDI_S:
			case BIDI_WS:
			case BIDI_B:
			case BIDI_ON:
				if (level_stack.GetEmbeddingDir() != last_strong_type)
					retval = neutral_virtual_position_start;
				if (last_strong_type == BIDI_R || level_stack.GetEmbeddingDir() == BIDI_R)
				{
					// FIXME mg maybe make sure this only creates a segment when necessary

					status = CreateBidiSegment(BIDI_R, neutral_width, neutral_start_element, neutral_virtual_position_start);

					if (status == OpStatus::OK)
						status = CreateBidiSegment(BIDI_AN, width, element, virtual_position);

					if (status != OpStatus::OK)
						return LONG_MAX;
				}
				else
				{
					// FIXME mg maybe make sure this only creates a segment when necessary

					status = CreateBidiSegment(BIDI_L, neutral_width, neutral_start_element, neutral_virtual_position_start);

					if (status == OpStatus::OK)
						status = CreateBidiSegment(BIDI_AN, width, element, virtual_position);

					if (status != OpStatus::OK)
						return LONG_MAX;
				}
				break;

			case BIDI_L:
				status = CreateBidiSegment(BIDI_AN,width, element, virtual_position);

				if (status != OpStatus::OK)
					return LONG_MAX;

				// create a new segment
				break;

			case BIDI_AN:
				// add to the current segment
				status = ExpandCurrentSegmentOrCreateFirst(BIDI_AN, width, element, virtual_position);

				if (status != OpStatus::OK)
					return LONG_MAX;

				break;

			case BIDI_UNDEFINED:
				// fall through
			case BIDI_EN:
			case BIDI_R:
				status = CreateBidiSegment(BIDI_AN, width, element, virtual_position);

				if (status != OpStatus::OK)
					return LONG_MAX;

				break;

			default:
				break;
			}

			last_strong_type = BIDI_R;
			last_type = BIDI_AN;
			break;

		case BIDI_CS:
			if (last_type == BIDI_EN || last_type == BIDI_AN)
			{
				W4_width = width;
				W4_start_element = element;
				W4_virtual_position = virtual_position;
				W4_pending_type = last_type; // need to save this here to match the types
				last_type = BIDI_CS;
			}
			else
			{
				bidi_type = BIDI_ON;
				word_handled = FALSE; //FIXME
			}	// W4
			break;

		case BIDI_NSM:
			bidi_type = W1_last_type;
			word_handled = FALSE;
			break;

		case BIDI_BN:
			bidi_type = last_strong_type; //FIXME
			word_handled = FALSE;
			break;

			//neutrals
		case BIDI_S:
		case BIDI_WS:
		case BIDI_B:
		case BIDI_ON:
			switch (last_type)
			{
			case BIDI_S: //L1
			case BIDI_WS:
			case BIDI_B:
			case BIDI_ON:
				neutral_width += width;
				break;

			case BIDI_ET: //W5
				if (IsNeutral(W5_type_before))
					neutral_width += W5_width + width;
				else
				{
					neutral_width = W5_width + width;
					neutral_start_element = W5_start_element;
					neutral_virtual_position_start = W5_virtual_position;
				}
				break;

			case BIDI_ES:
			case BIDI_CS:
				if (W4_pending_type != BIDI_UNDEFINED)
				{
					neutral_width = W4_width + width;
					neutral_start_element = W4_start_element;
					neutral_virtual_position_start = W4_virtual_position;
				}
				break;

			case BIDI_UNDEFINED:
			case BIDI_L:
			case BIDI_R:
			case BIDI_AN:
			case BIDI_EN:
				neutral_width = width;
				neutral_start_element = element;
				neutral_virtual_position_start = virtual_position;
				break;

			default:
				break;
			}

			last_type = bidi_type;
			break;

		default:
			OP_ASSERT(0);
			break;

		}
	}

	if (initial_bidi_type == BIDI_PDF)
		PopLevel();

	/* set the states of some variables needed for weak types */

	// W1
	if (initial_bidi_type != BIDI_NSM)
		W1_last_type = initial_bidi_type;

	// W2
	if (initial_bidi_type == BIDI_R || initial_bidi_type == BIDI_L || initial_bidi_type == BIDI_AL)
		W2_last_strong_type = initial_bidi_type;

	// W4
	// reset this here so I dont have to do it for every type
	if (bidi_type != BIDI_CS && bidi_type != BIDI_ES)
		W4_pending_type = BIDI_UNDEFINED;

	// W5
	if (bidi_type != BIDI_ET)
		W5_type_before = BIDI_UNDEFINED;

	if (initial_bidi_type == BIDI_EN)
		W5_last_type = BIDI_EN;
	else
		W5_last_type = BIDI_UNDEFINED;

	// W7
	if (initial_bidi_type == BIDI_R || initial_bidi_type == BIDI_AL)
		W7_last_strong_type = BIDI_R;
	else
		if (initial_bidi_type == BIDI_L)
			W7_last_strong_type = BIDI_L;


	return retval;

}

/** Reset the calculation object for reuse. */

void
BidiCalculation::Reset()
{
	previous_embedding_level = -1;
	last_type = BIDI_UNDEFINED;
	paragraph_type = BIDI_UNDEFINED;
	last_strong_type = BIDI_UNDEFINED;
	neutral_width = 0;
	neutral_start_element = NULL;
	neutral_virtual_position_start = 0;
	last_appended_element = NULL;
	previous_segment_level = -1;
	last_segment_type = BIDI_UNDEFINED;
	eor_type = BIDI_UNDEFINED;
	eor_level = -1;
	W1_last_type = BIDI_UNDEFINED;
	W2_last_strong_type = BIDI_UNDEFINED;
	W4_pending_type = BIDI_UNDEFINED;
	W4_virtual_position = 0;
	W4_width = 0;
	W4_start_element = NULL;
	W5_type_before = BIDI_UNDEFINED;
	W5_virtual_position = 0;
	W5_width = 0;
	W5_start_element = NULL;
	W5_last_type = BIDI_UNDEFINED;
	W7_last_strong_type = BIDI_UNDEFINED;

	level_stack.Reset();
}



// end of BidiStatus

#endif //SUPPORT_TEXT_DIRECTION
