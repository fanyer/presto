/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/widgets/ColouredMultiEdit.h"

#ifdef COLOURED_MULTIEDIT_SUPPORT

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// DivideHTMLTagFragment is used by GetNextTextFragment in layout/box.cpp to add in the extra breaking up
// of text fragments so we can make sure that fragments break on the < and >, and it also updates the in_tag so
// that it reflects the current tag being processed. 
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int DivideHTMLTagFragment(int *in_tag, const uni_char *wstart, const uni_char **wend, int *length, BOOL *has_baseline, BOOL next_has_baseline)
{
	if (in_tag)
	{
		if (**wend == '<' && (*in_tag == HTT_NONE || *in_tag == HTT_STYLE_OPEN_START || *in_tag == HTT_SCRIPT_OPEN_START || *in_tag == HTT_COMMENT_START)) {
			// If we are not at the start then just hop out
			if (*wend != wstart)
				return DIVIDETAG_DONE;

			// Is this a html comment tag?
			if (*in_tag != HTT_STYLE_OPEN_START && *in_tag != HTT_SCRIPT_OPEN_START && !uni_strnicmp(*wend, UNI_L("<!--"), 4))
				*in_tag = HTT_COMMENT_START; // Start html comment tag

			// Is this a style tag?
			if (*in_tag != HTT_SCRIPT_OPEN_START && *in_tag != HTT_COMMENT_START && !uni_strnicmp(*wend, UNI_L("<style"), 6))
				*in_tag = HTT_STYLE_OPEN_START; // Start style tag
			if (*in_tag != HTT_SCRIPT_OPEN_START && *in_tag != HTT_COMMENT_START && !uni_strnicmp(*wend, UNI_L("</style"), 7))
				*in_tag = HTT_STYLE_CLOSE_START; // End style tag

			// Is this a script tag?
			if (*in_tag != HTT_STYLE_OPEN_START && *in_tag != HTT_COMMENT_START && !uni_strnicmp(*wend, UNI_L("<script"), 7))
				*in_tag = HTT_SCRIPT_OPEN_START; // Start script tag
			if (*in_tag != HTT_STYLE_OPEN_START && *in_tag != HTT_COMMENT_START && !uni_strnicmp(*wend, UNI_L("</script"), 8))
				*in_tag = HTT_SCRIPT_CLOSE_START; // End script tag

			// Only set the tag to 1 if it's not already running something else
			if (!*in_tag)
				*in_tag = HTT_TAG_START;

			*has_baseline = next_has_baseline;
			(*wend)++;
			(*length)--;

			return DIVIDETAG_AGAIN;				
		}
		else if (**wend == '>' && *in_tag != HTT_STYLE_OPEN_START && *in_tag != HTT_SCRIPT_OPEN_START)
		{
			// Is a html comment tag currently running?
			if (*in_tag == HTT_COMMENT_START)
			{
				// Are the previous 2 char's dashes? i.e. a html comment closing tag -->
				if (*(*wend - 1) != '-' && *(*wend - 2) != '-') 
				{
					// something else so go around again!
					*has_baseline = next_has_baseline;
					(*wend)++;
					(*length)--;

					return DIVIDETAG_AGAIN;				
				}
			}
			
			*has_baseline = next_has_baseline;
			(*wend)++;
			(*length)--;
			if (*in_tag)
				(*in_tag) += HTT_TAG_SWITCH;

			return DIVIDETAG_DONE;
		}
		else if (*in_tag)
		{
			*has_baseline = next_has_baseline;
			(*wend)++;
			(*length)--;

			return DIVIDETAG_AGAIN;				
		}
	}			

	return DIVIDETAG_NOTHING;
}

#endif // COLOURED_MULTIEDIT_SUPPORT
