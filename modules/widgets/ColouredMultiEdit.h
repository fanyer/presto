/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef COLOURED_MULTIEDIT_H
#define COLOURED_MULTIEDIT_H

#ifdef COLOURED_MULTIEDIT_SUPPORT

// Tag type enum
enum DivideTagFragment
{
	DIVIDETAG_NOTHING		= 0,
	DIVIDETAG_AGAIN 		= 1,
	DIVIDETAG_DONE		 	= 2
};

int DivideHTMLTagFragment(int *in_tag, const uni_char *wstart, const uni_char **wend, int *length, BOOL *has_baseline, BOOL next_has_baseline);

// Colour defines
#define TEXT_SYNTAX_COLOUR 			OP_RGB(0x00, 0x00, 0x00)	///< Text/Other
#define TAG_SYNTAX_COLOUR 			OP_RGB(0x00, 0x00, 0xCC)	///< Tags
#define SCRIPT_SYNTAX_COLOUR 		OP_RGB(0x93, 0x0F, 0x1A)	///< Script
#define STYLE_SYNTAX_COLOUR 		OP_RGB(0xAA, 0xAA, 0x00)	///< Style
#define COMMENT_SYNTAX_COLOUR 		OP_RGB(0x29, 0x6F, 0x28)	///< Html Comments

// Tag type enum
enum HtmlTagType
{
	HTT_NONE 				= 0,
	HTT_TAG_START 			= 1,
	HTT_STYLE_OPEN_START 	= 2,
	HTT_STYLE_CLOSE_START	= 3,
	HTT_SCRIPT_OPEN_START	= 4,
	HTT_SCRIPT_CLOSE_START	= 5,
	HTT_COMMENT_START 		= 6,
	HTT_TAG_SWITCH 			= 100,
	HTT_TAG_END				= 101,
	HTT_STYLE_OPEN_END		= 102,
	HTT_STYLE_CLOSE_END		= 103,
	HTT_SCRIPT_OPEN_END		= 104,
	HTT_SCRIPT_CLOSE_END	= 105,
	HTT_COMMENT_END			= 106
};

// Which tag is which ???
//	<html>
//	^    ^
//	|    |
//	1   101
//
//	<style>   </style>
//	^     ^   ^      ^
//	|     |   |      |
//	2    102  3     103
//
//	<script>   </script>
//	^      ^   ^       ^
//	|      |   |       |
//	4     104  5      105
//
//	<!--   -->
//	^        ^
//	|        |
//	6       106
//

#endif // COLOURED_MULTIEDIT_SUPPORT

#endif // COLOURED_MULTIEDIT_H
