/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef HTML5BASE_H
#define HTML5BASE_H

#ifdef HTML5_STANDALONE

# define HTML5NODE		H5Node
# define HTML5ELEMENT	H5Element
# define HTML5TEXT		H5Text
# define HTML5COMMENT	H5Comment
# define HTML5DOCTYPE	H5Doctype

# define HTML5NODE_INCLUDE	"modules/logdoc/src/html5/h5node.h"

# include "modules/logdoc/src/html5/standalone/standalone.h"
# include "modules/util/simset.h"

#else // HTML5_STANDALONE

# define HTML5NODE		HTML_Element
# define HTML5ELEMENT	HTML_Element
# define HTML5TEXT		HTML_Element
# define HTML5COMMENT	HTML_Element
# define HTML5DOCTYPE	HTML_Element

# define HTML5NODE_INCLUDE	"modules/logdoc/htm_elm.h"

#endif // HTML5_STANDALONE

// used for calculating hash values for element and attribute names
// hash = hash*33 + c (lowercased)
#define HTML5_HASH_FUNCTION(hash, c) hash = ((hash << 5) + hash) + (c | 0x20)

#if defined DELAYED_SCRIPT_EXECUTION || defined SPECULATIVE_PARSER
/**
 * HTML5ParserState is used by Delayed Script Execution to store
 * the current state of the parser when a script has been found
 * and delayed so that we can restore the state if it turns out
 * that we need to start over after a recovery situation when
 * running the script after the parser has finished.
 */
struct HTML5ParserState
{
	HTML5ParserState() : tokenizer_state(0), stream_offset(0), 
			line_number(0), character_position(0), parser_insertion_mode(0),
			tag_nesting_level(0), p_elements_in_button_scope(0) {}
	
	AutoDeleteHead 	open_elements;
	AutoDeleteHead 	active_formatting_elements;
	unsigned		tokenizer_state;
	unsigned		stream_offset;
	unsigned		line_number;
	unsigned		character_position;

	unsigned		parser_insertion_mode;
	unsigned		tag_nesting_level;
	unsigned		p_elements_in_button_scope;
};
#endif // defined DELAYED_SCRIPT_EXECUTION || defined SPECULATIVE_PARSER

#endif // HTML5BASE_H
