/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef IRC_COLOR_PARSER_H
#define IRC_COLOR_PARSER_H

# include "modules/util/adt/opvector.h"

//****************************************************************************
//
//	ColorParserHandler
//
//****************************************************************************

class ColorParserHandler
{
public:
	virtual ~ColorParserHandler() {}
	enum color_type
	{
		COLOR_NONE			= -1,
		COLOR_WHITE			=  0,
		COLOR_BLACK			=  1,
		COLOR_BLUE			=  2,
		COLOR_GREEN			=  3,
		COLOR_LIGHTRED		=  4,
		COLOR_BROWN			=  5,
		COLOR_PURPLE		=  6,
		COLOR_ORANGE		=  7,
		COLOR_YELLOW		=  8,
		COLOR_LIGHTGREEN	=  9,
		COLOR_CYAN			= 10,
		COLOR_LIGHTCYAN		= 11,
		COLOR_LIGHTBLUE		= 12,
		COLOR_PINK			= 13,
		COLOR_GREY			= 14,
		COLOR_LIGHTGREY		= 15,
		COLOR_TRANSPARENT	= 99
	};

	static BOOL IsValidColorValue(int Value);

	virtual void OnPlainTextBegin() { }
	virtual void OnPlainTextEnd() { }
	virtual void OnBoldTextBegin() { }
	virtual void OnBoldTextEnd() { }
	virtual void OnColorTextBegin(color_type text_color, color_type background_color = COLOR_NONE) { }
	virtual void OnColorTextEnd() { }
	virtual void OnReverseTextBegin() { }
	virtual void OnReverseTextEnd() { }
	virtual void OnUnderlineTextBegin() { }
	virtual void OnUnderlineTextEnd() { }
	virtual void OnCharacter(uni_char character) { }
};


//****************************************************************************
//
//	mIRCColorParser
//
//****************************************************************************

class mIRCColorParser
{
public:
	// Construction / destruction.
	mIRCColorParser(ColorParserHandler &handler);
	~mIRCColorParser();

	// Methods.
	OP_STATUS Parse(const OpStringC& text);

private:
	// No copy or assignment.
	mIRCColorParser(const mIRCColorParser& other);
	mIRCColorParser& operator=(const mIRCColorParser& other);

	// Enumerations.
	enum ParseState
	{
		STATE_INSIDE_PLAIN_TEXT,
		STATE_INSIDE_BOLD_TEXT,
		STATE_INSIDE_UNDERLINED_TEXT,
		STATE_INSIDE_REVERSED_TEXT,
		STATE_INSIDE_COLOR_TEXT_CODE,
		STATE_INSIDE_COLOR_BACKGROUND_CODE,
		STATE_INSIDE_COLORED_TEXT
	};

	// Methods.
	void ClearStack();

	OP_STATUS StackPush(ParseState state);
	void StackPop();
	ParseState StackTop() const;
	BOOL StackIsEmpty() const { return m_state_stack.GetCount() == 0; }

	// Static members.
	static uni_char m_plain_code;
	static uni_char m_bold_code;
	static uni_char m_color_code;
	static uni_char m_reverse_code;
	static uni_char m_underline_code;

	// Members.
	ColorParserHandler &m_handler;
	OpINT32Vector m_state_stack;
};


//****************************************************************************
//
//	IRCColorStripper
//
//****************************************************************************

class IRCColorStripper : public ColorParserHandler
{
public:
	virtual ~IRCColorStripper() {}
	// Construction.
	IRCColorStripper() { }
	OP_STATUS Init(const OpStringC& text);

	// Accessor.
	const OpStringC& StrippedText() const { return m_stripped_text; }

private:
	// ColorParserHandler.
	virtual void OnCharacter(uni_char character) { m_stripped_text.Append(&character, 1); }

	// Members.
	OpString m_stripped_text;
};


#endif
