/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _FORMSUGGESTION_H_
#define _FORMSUGGESTION_H_

class FramesDocument;
class HTML_Element;

/**
 * Remembers things to allow for easy fill in.
 *
 * @author Daniel Bratell
 */
class FormSuggestion
{
public:
	enum SuggestionType
	{
		/**
			* Select suggestion based on context.
			*/
		AUTOMATIC,
		/**
			* Suggest things from the personal info database.
			*/
		PERSONAL_INFO,
		/**
			* Suggest things written in forms earlier.
			*/
		FORM_HISTORY
	};


private:
	FramesDocument* m_frames_doc;
	HTML_Element* m_elm;
	SuggestionType m_suggestion_type;

public:
	/**
	 * @param frames_doc The docuemnt.
	 * @param elm The element to suggest data for.
	 * @param type What to suggest.
	 */
	FormSuggestion(FramesDocument* frames_doc,
				   HTML_Element* elm,
				   SuggestionType type)
		: m_frames_doc(frames_doc), m_elm(elm), m_suggestion_type(type)
	{
		OP_ASSERT(frames_doc);
		OP_ASSERT(elm);
	}

	/**
	 * Calculate possible matches.
	 *
	 * @param in_MatchString The string written in the field so far.
	 *
	 * @param out_Items A pointer that will be pointing to a uni_char
	 * array if the method succeeds. The strings in the array, as well
	 * as the array itself is owned by the caller if this method
	 * exists with OpStatus::OK. The data will be in the order:
	 * row_1_column_1, row_1_column_2, ..., row_1_column_N,
	 * row_2_column_2, ...
	 *
	 * @param out_ItemCount Must be filled with the number of rows in
	 * the out array.
	 *
	 * @param out_num_columns Must be filled with the number of
	 * columns in the out array.
	 *
	 * @param out_contains_private_data Will be filled with a TRUE if
	 * the returned data conatins anything that would be unsuitable to
	 * expose to a script without the user's explicit command.
	 *
	 * @returns The status. OpStatus::OK is successful.
	 */
	OP_STATUS GetItems(const uni_char* in_MatchString,
					   uni_char*** out_Items,
					   int* out_ItemCount,
						int* out_num_columns,
						BOOL* out_contains_private_data = NULL);

private:
	/**
	 * Calculate possible matches.
	 *
	 * @param in_MatchString The string written in the field so far.
	 *
	 * @param out_Items A pointer that will be pointing to a uni_char
	 * array if the method succeeds. The strings in the array, as well
	 * as the array itself is owned by the caller if this method
	 * exists with OpStatus::OK. The data will be in the order:
	 * row_1_column_1, row_1_column_2, ..., row_1_column_N,
	 * row_2_column_2, ...
	 *
	 * @param out_ItemCount Must be filled with the number of rows in
	 * the out array.
	 *
	 * @param out_num_columns Must be filled with the number of
	 * columns in the out array.
	 *
	 * @returns The status. OpStatus::OK is successful.
	 */
	OP_STATUS GetDataListItems(const uni_char* in_MatchString,
					   uni_char*** out_Items,
					   int* out_ItemCount,
					   int* out_num_columns);
};

#endif // _FORMSUGGESTION_H_
