/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef IRC_SUPPORT

#include "color-parser.h"

//****************************************************************************
//
//	ColorParserHandler
//
//****************************************************************************

BOOL ColorParserHandler::IsValidColorValue(int Value)
{
	BOOL valid = false;

	if (Value >= 0 && Value <= 15)
		valid = true;
	else if (Value == 99)
		valid = true;

	return valid;
}


//****************************************************************************
//
//	mIRCColorParser
//
//****************************************************************************

mIRCColorParser::mIRCColorParser(ColorParserHandler &handler)
:	m_handler(handler)
{
}


mIRCColorParser::~mIRCColorParser()
{
}


uni_char mIRCColorParser::m_plain_code		= '\017';
uni_char mIRCColorParser::m_bold_code		= '\002';
uni_char mIRCColorParser::m_color_code		= '\003';
uni_char mIRCColorParser::m_reverse_code	= '\026';
uni_char mIRCColorParser::m_underline_code	= '\037';


OP_STATUS mIRCColorParser::Parse(const OpStringC& text)
{
	ParseState cur_state = STATE_INSIDE_PLAIN_TEXT;
	RETURN_IF_ERROR(StackPush(cur_state));

	m_handler.OnPlainTextBegin();

	OpString text_color_string;
	OpString background_color_string;

	const int text_length = text.Length();
	for (int index = 0; index < text_length; ++index)
	{
		const uni_char cur_char = text[index];

		if (cur_state == STATE_INSIDE_COLOR_TEXT_CODE)
		{
			if ((text_color_string.Length() < 2) && uni_isdigit(cur_char))
				text_color_string.Append(&cur_char, 1);
			else if (cur_char == ',')
			{
				cur_state = STATE_INSIDE_COLOR_BACKGROUND_CODE;
				RETURN_IF_ERROR(StackPush(cur_state));
			}
			else
			{
				const ColorParserHandler::color_type text_color = 
					text_color_string.HasContent() ? ColorParserHandler::color_type(uni_atoi(text_color_string.CStr())) :
					ColorParserHandler::COLOR_NONE;
				
				if (ColorParserHandler::IsValidColorValue(text_color))
				{
					cur_state = STATE_INSIDE_COLORED_TEXT;
					RETURN_IF_ERROR(StackPush(cur_state));

					m_handler.OnColorTextBegin(text_color);
				}
				else
				{
					StackPop();
					cur_state = StackTop();

					if (cur_state == STATE_INSIDE_COLORED_TEXT)
						m_handler.OnColorTextEnd();
				}

				text_color_string.Empty();
			}
		}
		else if (cur_state == STATE_INSIDE_COLOR_BACKGROUND_CODE)
		{
			if ((background_color_string.Length() < 2) && uni_isdigit(cur_char))
				background_color_string.Append(&cur_char, 1);
			else
			{
				const ColorParserHandler::color_type text_color =
					text_color_string.HasContent() ? ColorParserHandler::color_type(uni_atoi(text_color_string.CStr())) :
					ColorParserHandler::COLOR_NONE;

				const ColorParserHandler::color_type background_color =
					background_color_string.HasContent() ? ColorParserHandler::color_type(uni_atoi(background_color_string.CStr())) :
					ColorParserHandler::COLOR_NONE;

				if (ColorParserHandler::IsValidColorValue(text_color) &&
					ColorParserHandler::IsValidColorValue(background_color))
				{
					cur_state = STATE_INSIDE_COLORED_TEXT;
					RETURN_IF_ERROR(StackPush(cur_state));

					m_handler.OnColorTextBegin(text_color, background_color);
				}
				else
				{
					StackPop();
					cur_state = StackTop();

					if (cur_state == STATE_INSIDE_COLORED_TEXT)
						m_handler.OnColorTextEnd();
				}

				background_color_string.Empty();
			}
		}

		if (cur_char == m_plain_code)
		{
			// A plain code should clear all other states.
			ClearStack();

			cur_state = STATE_INSIDE_PLAIN_TEXT;
			RETURN_IF_ERROR(StackPush(cur_state));

			m_handler.OnPlainTextBegin();
		}
		else if (cur_char == m_bold_code)
		{
			if (cur_state == STATE_INSIDE_BOLD_TEXT)
			{
				StackPop();
				cur_state = StackTop();

				m_handler.OnBoldTextEnd();
			}
			else
			{
				cur_state = STATE_INSIDE_BOLD_TEXT;
				RETURN_IF_ERROR(StackPush(cur_state));

				m_handler.OnBoldTextBegin();
			}
		}
		else if (cur_char == m_color_code)
		{
			text_color_string.Empty();
			background_color_string.Empty();

			cur_state = STATE_INSIDE_COLOR_TEXT_CODE;
			RETURN_IF_ERROR(StackPush(cur_state));
		}
		else if (cur_char == m_reverse_code)
		{
			if (cur_state == STATE_INSIDE_REVERSED_TEXT)
			{
				StackPop();
				cur_state = StackTop();

				m_handler.OnReverseTextEnd();
			}
			else
			{
				cur_state = STATE_INSIDE_REVERSED_TEXT;
				RETURN_IF_ERROR(StackPush(cur_state));

				m_handler.OnReverseTextBegin();
			}
		}
		else if (cur_char == m_underline_code)
		{
			if (cur_state == STATE_INSIDE_UNDERLINED_TEXT)
			{
				StackPop();
				cur_state = StackTop();

				m_handler.OnUnderlineTextEnd();
			}
			else
			{
				cur_state = STATE_INSIDE_UNDERLINED_TEXT;
				RETURN_IF_ERROR(StackPush(cur_state));

				m_handler.OnUnderlineTextBegin();
			}
		}
		else
		{
			if (cur_state != STATE_INSIDE_COLOR_TEXT_CODE &&
				cur_state != STATE_INSIDE_COLOR_BACKGROUND_CODE)
			{
				m_handler.OnCharacter(cur_char);
			}
		}
	}


	ClearStack();
	return OpStatus::OK;
}


void mIRCColorParser::ClearStack()
{
	while (!StackIsEmpty())
	{
		const ParseState state = StackTop();
		switch (state)
		{
			case STATE_INSIDE_BOLD_TEXT :
				m_handler.OnBoldTextEnd();
				break;
			case STATE_INSIDE_UNDERLINED_TEXT :
				m_handler.OnUnderlineTextEnd();
				break;
			case STATE_INSIDE_REVERSED_TEXT :
				m_handler.OnReverseTextEnd();
				break;
			case STATE_INSIDE_COLORED_TEXT :
				m_handler.OnColorTextEnd();
				break;
			case STATE_INSIDE_PLAIN_TEXT :
				m_handler.OnPlainTextEnd();
				break;
		}

		StackPop();
	}
}


OP_STATUS mIRCColorParser::StackPush(ParseState state)
{
	RETURN_IF_ERROR(m_state_stack.Insert(0, INT32(state)));
	return OpStatus::OK;
}


void mIRCColorParser::StackPop()
{
	OP_ASSERT(!StackIsEmpty());
	m_state_stack.Remove(0, 1);
}


mIRCColorParser::ParseState mIRCColorParser::StackTop() const
{
	OP_ASSERT(!StackIsEmpty());
	return ParseState(m_state_stack.Get(0));
}


//****************************************************************************
//
//	IRCColorStripper
//
//****************************************************************************

OP_STATUS IRCColorStripper::Init(const OpStringC& text)
{
	mIRCColorParser color_parser(*this);
	RETURN_IF_ERROR(color_parser.Parse(text));

	return OpStatus::OK;
}

#endif // IRC_SUPPORT
