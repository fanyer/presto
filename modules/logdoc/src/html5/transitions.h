/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifdef HAS_COMPLEX_GLOBALS
# define TRANS_START(name) const HTML5TreeState::State name[HTML5TreeState::kNumberOfTransitionStates][HTML5TreeState::kNumberOfTransitionSubStates] = {
# define TRANS_BLOCK_START() {
# define TRANS_ENTRY(s1)  HTML5TreeState::s1
# define TRANS_BLOCK_END() }
# define TRANS_END() }
#else // HAS_COMPLEX_GLOBALS
# define TRANS_START(name) void HTML5TransitionsInitL() { HTML5TreeState::State **local = name; int i = 0; int j = 0;
# define TRANS_BLOCK_START() j = 0,
# define TRANS_ENTRY(s1) local[i][j++] = HTML5TreeState::s1
# define TRANS_BLOCK_END() ; i++
# define TRANS_END() ;}
#endif // HAS_COMPLEX_GLOBALS

TRANS_START(g_html5_transitions)
/* INITIAL */
TRANS_BLOCK_START()
TRANS_ENTRY(PROCESS_DOCTYPE),		// DOCTYPE
TRANS_ENTRY(NO_DOCTYPE),			// START_TAG
TRANS_ENTRY(NO_DOCTYPE),			// END_TAG
TRANS_ENTRY(INSERT_COMMENT),		// COMMENT
TRANS_ENTRY(IGNORE_SPACE),			// CHARACTER
TRANS_ENTRY(NO_DOCTYPE)				// EOF
TRANS_BLOCK_END(),

/* BEFORE_HTML */
TRANS_BLOCK_START()
TRANS_ENTRY(IGNORE_TOKEN_ERR),		// DOCTYPE
TRANS_ENTRY(START_BEFORE_HTML),		// START_TAG
TRANS_ENTRY(EARLY_END_TAG),			// END_TAG
TRANS_ENTRY(INSERT_COMMENT),		// COMMENT
TRANS_ENTRY(IGNORE_SPACE),			// CHARACTER
TRANS_ENTRY(IMPLICIT_HTML)			// EOF
TRANS_BLOCK_END(),

/* BEFORE_HEAD */
TRANS_BLOCK_START()
TRANS_ENTRY(IGNORE_TOKEN_ERR),		// DOCTYPE
TRANS_ENTRY(START_BEFORE_HEAD),		// START_TAG
TRANS_ENTRY(EARLY_END_TAG),			// END_TAG
TRANS_ENTRY(INSERT_COMMENT),		// COMMENT
TRANS_ENTRY(IGNORE_SPACE),			// CHARACTER
TRANS_ENTRY(IMPLICIT_HEAD)			// EOF
TRANS_BLOCK_END(),

/* IN_HEAD */
TRANS_BLOCK_START()
TRANS_ENTRY(IGNORE_TOKEN_ERR),		// DOCTYPE
TRANS_ENTRY(START_IN_HEAD),			// START_TAG
TRANS_ENTRY(END_IN_HEAD),			// END_TAG
TRANS_ENTRY(INSERT_COMMENT),		// COMMENT
TRANS_ENTRY(WS_BEFORE_BODY),		// CHARACTER
TRANS_ENTRY(IMPLICIT_END_HEAD)		// EOF
TRANS_BLOCK_END(),

/* IN_HEAD_NOSCRIPT */
TRANS_BLOCK_START()
TRANS_ENTRY(IGNORE_TOKEN_ERR),		// DOCTYPE
TRANS_ENTRY(START_IN_NOSCRIPT),		// START_TAG
TRANS_ENTRY(END_IN_NOSCRIPT),		// END_TAG
TRANS_ENTRY(AS_IN_HEAD),			// COMMENT
TRANS_ENTRY(WS_IN_NOSCRIPT),		// CHARACTER
TRANS_ENTRY(AS_IN_HEAD_ERR)			// EOF
TRANS_BLOCK_END(),

/* AFTER_HEAD */
TRANS_BLOCK_START()
TRANS_ENTRY(IGNORE_TOKEN_ERR),		// DOCTYPE
TRANS_ENTRY(START_AFTER_HEAD),		// START_TAG
TRANS_ENTRY(END_AFTER_HEAD),		// END_TAG
TRANS_ENTRY(INSERT_COMMENT),		// COMMENT
TRANS_ENTRY(WS_BEFORE_BODY),		// CHARACTER
TRANS_ENTRY(IMPLICIT_BODY)			// EOF
TRANS_BLOCK_END(),

/* IN_BODY */
TRANS_BLOCK_START()
TRANS_ENTRY(IGNORE_TOKEN_ERR),		// DOCTYPE
TRANS_ENTRY(START_IN_BODY),			// START_TAG
TRANS_ENTRY(END_IN_BODY),			// END_TAG
TRANS_ENTRY(INSERT_COMMENT),		// COMMENT
TRANS_ENTRY(TEXT_IN_BODY),			// CHARACTER
TRANS_ENTRY(EOF_IN_BODY)			// EOF
TRANS_BLOCK_END(),

/* TEXT */
TRANS_BLOCK_START()
TRANS_ENTRY(ILLEGAL_STATE),			// DOCTYPE
TRANS_ENTRY(ILLEGAL_STATE),			// START_TAG
TRANS_ENTRY(END_TAG_IN_TEXT),		// END_TAG
TRANS_ENTRY(ILLEGAL_STATE),			// COMMENT
TRANS_ENTRY(HANDLE_TEXT),			// CHARACTER
TRANS_ENTRY(EOF_IN_TEXT)			// EOF
TRANS_BLOCK_END(),

/* IN_TABLE */
TRANS_BLOCK_START()
TRANS_ENTRY(IGNORE_TOKEN_ERR),		// DOCTYPE
TRANS_ENTRY(START_IN_TABLE),		// START_TAG
TRANS_ENTRY(END_IN_TABLE),			// END_TAG
TRANS_ENTRY(INSERT_COMMENT),		// COMMENT
TRANS_ENTRY(TEXT_IN_TABLE),			// CHARACTER
TRANS_ENTRY(EOF_IN_TABLE)			// EOF
TRANS_BLOCK_END(),

/* IN_TABLE_TEXT */
TRANS_BLOCK_START()
TRANS_ENTRY(INSERT_TABLE_TEXT),		// DOCTYPE
TRANS_ENTRY(INSERT_TABLE_TEXT),		// START_TAG
TRANS_ENTRY(INSERT_TABLE_TEXT),		// END_TAG
TRANS_ENTRY(INSERT_TABLE_TEXT),		// COMMENT
TRANS_ENTRY(TEXT_IN_TABLE),			// CHARACTER
TRANS_ENTRY(INSERT_TABLE_TEXT)		// EOF
TRANS_BLOCK_END(),

/* IN_CAPTION */
TRANS_BLOCK_START()
TRANS_ENTRY(AS_IN_BODY),			// DOCTYPE
TRANS_ENTRY(START_IN_CAPTION),		// START_TAG
TRANS_ENTRY(END_IN_CAPTION),		// END_TAG
TRANS_ENTRY(AS_IN_BODY),			// COMMENT
TRANS_ENTRY(AS_IN_BODY),			// CHARACTER
TRANS_ENTRY(AS_IN_BODY)				// EOF
TRANS_BLOCK_END(),

/* IN_COLUMN_GROUP */
TRANS_BLOCK_START()
TRANS_ENTRY(IGNORE_TOKEN_ERR),		// DOCTYPE
TRANS_ENTRY(START_IN_COLGROUP),		// START_TAG
TRANS_ENTRY(END_IN_COLGROUP),		// END_TAG
TRANS_ENTRY(INSERT_COMMENT),		// COMMENT
TRANS_ENTRY(SPACE_IN_COLGROUP),		// CHARACTER
TRANS_ENTRY(EOF_IN_COLGROUP)		// EOF
TRANS_BLOCK_END(),

/* IN_TABLE_BODY */
TRANS_BLOCK_START()
TRANS_ENTRY(AS_IN_TABLE),			// DOCTYPE
TRANS_ENTRY(START_IN_TBODY),		// START_TAG
TRANS_ENTRY(END_IN_TBODY),			// END_TAG
TRANS_ENTRY(AS_IN_TABLE),			// COMMENT
TRANS_ENTRY(AS_IN_TABLE),			// CHARACTER
TRANS_ENTRY(AS_IN_TABLE)			// EOF
TRANS_BLOCK_END(),

/* IN_ROW */
TRANS_BLOCK_START()
TRANS_ENTRY(AS_IN_TABLE),			// DOCTYPE
TRANS_ENTRY(START_IN_ROW),			// START_TAG
TRANS_ENTRY(END_IN_ROW),			// END_TAG
TRANS_ENTRY(AS_IN_TABLE),			// COMMENT
TRANS_ENTRY(AS_IN_TABLE),			// CHARACTER
TRANS_ENTRY(AS_IN_TABLE)			// EOF
TRANS_BLOCK_END(),

/* IN_CELL */
TRANS_BLOCK_START()
TRANS_ENTRY(AS_IN_BODY),			// DOCTYPE
TRANS_ENTRY(START_IN_CELL),			// START_TAG
TRANS_ENTRY(END_IN_CELL),			// END_TAG
TRANS_ENTRY(AS_IN_BODY),			// COMMENT
TRANS_ENTRY(AS_IN_BODY),			// CHARACTER
TRANS_ENTRY(AS_IN_BODY)				// EOF
TRANS_BLOCK_END(),

/* IN_SELECT */
TRANS_BLOCK_START()
TRANS_ENTRY(IGNORE_TOKEN_ERR),		// DOCTYPE
TRANS_ENTRY(START_IN_SELECT),		// START_TAG
TRANS_ENTRY(END_IN_SELECT),			// END_TAG
TRANS_ENTRY(INSERT_COMMENT),		// COMMENT
TRANS_ENTRY(HANDLE_TEXT),			// CHARACTER
TRANS_ENTRY(EARLY_EOF)				// EOF
TRANS_BLOCK_END(),

/* IN_SELECT_IN_TABLE */
TRANS_BLOCK_START()
TRANS_ENTRY(AS_IN_SELECT),			// DOCTYPE
TRANS_ENTRY(START_IN_SELECT_TABLE),	// START_TAG
TRANS_ENTRY(END_IN_SELECT_TABLE),	// END_TAG
TRANS_ENTRY(AS_IN_SELECT),			// COMMENT
TRANS_ENTRY(AS_IN_SELECT),			// CHARACTER
TRANS_ENTRY(AS_IN_SELECT)			// EOF
TRANS_BLOCK_END(),

/* IN_FOREIGN_CONTENT */
TRANS_BLOCK_START()
TRANS_ENTRY(IGNORE_TOKEN_ERR),		// DOCTYPE
TRANS_ENTRY(START_IN_FOREIGN),		// START_TAG
TRANS_ENTRY(END_IN_FOREIGN),		// END_TAG
TRANS_ENTRY(INSERT_COMMENT),		// COMMENT
TRANS_ENTRY(TEXT_IN_FOREIGN),		// CHARACTER
TRANS_ENTRY(TERMINATE_FOREIGN)		// EOF
TRANS_BLOCK_END(),

/* AFTER_BODY */
TRANS_BLOCK_START()
TRANS_ENTRY(IGNORE_TOKEN_ERR),		// DOCTYPE
TRANS_ENTRY(START_AFTER_BODY),		// START_TAG
TRANS_ENTRY(END_AFTER_BODY),		// END_TAG
TRANS_ENTRY(INSERT_COMMENT),		// COMMENT
TRANS_ENTRY(WS_AFTER_BODY),			// CHARACTER
TRANS_ENTRY(STOP)					// EOF
TRANS_BLOCK_END(),

/* IN_FRAMESET */
TRANS_BLOCK_START()
TRANS_ENTRY(IGNORE_TOKEN_ERR),		// DOCTYPE
TRANS_ENTRY(START_IN_FRAMESET),		// START_TAG
TRANS_ENTRY(END_IN_FRAMESET),		// END_TAG
TRANS_ENTRY(INSERT_COMMENT),		// COMMENT
TRANS_ENTRY(WS_OUT_OF_BODY),		// CHARACTER
TRANS_ENTRY(EARLY_EOF)				// EOF
TRANS_BLOCK_END(),

/* AFTER_FRAMESET */
TRANS_BLOCK_START()
TRANS_ENTRY(IGNORE_TOKEN_ERR),			// DOCTYPE
TRANS_ENTRY(START_AFTER_FRAMESET),		// START_TAG
TRANS_ENTRY(END_AFTER_FRAMESET),		// END_TAG
TRANS_ENTRY(INSERT_COMMENT),			// COMMENT
TRANS_ENTRY(WS_OUT_OF_BODY),			// CHARACTER
TRANS_ENTRY(STOP)						// EOF
TRANS_BLOCK_END(),

/* AFTER_AFTER_BODY */
TRANS_BLOCK_START()
TRANS_ENTRY(AS_IN_BODY),				// DOCTYPE
TRANS_ENTRY(START_AFTER_AFTER_BODY),	// START_TAG
TRANS_ENTRY(AS_IN_BODY_ERR),			// END_TAG
TRANS_ENTRY(INSERT_COMMENT),			// COMMENT
TRANS_ENTRY(WS_AFTER_BODY),				// CHARACTER
TRANS_ENTRY(EOF_AFTER_AFTER)			// EOF
TRANS_BLOCK_END(),

/* AFTER_AFTER_FRAMESET */
TRANS_BLOCK_START()
TRANS_ENTRY(AS_IN_BODY),				// DOCTYPE
TRANS_ENTRY(START_AFTER_AFTER_FRAMSET),	// START_TAG
TRANS_ENTRY(IGNORE_TOKEN_ERR),			// END_TAG
TRANS_ENTRY(INSERT_COMMENT),			// COMMENT
TRANS_ENTRY(WS_OUT_OF_BODY)	,			// CHARACTER
TRANS_ENTRY(EOF_AFTER_AFTER)			// EOF
TRANS_BLOCK_END()

TRANS_END();