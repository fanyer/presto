/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef CSS_ANIMATIONS

#include "modules/style/css.h"
#include "modules/style/css_property_list.h"
#include "modules/style/src/css_lexer.h"
#include "modules/style/src/css_grammar.h"
#include "modules/style/src/css_buffer.h"
#include "modules/style/src/css_keyframes.h"

CSS_KeyframeRule::CSS_KeyframeRule(CSS_KeyframesRule* keyframes,
								   List<CSS_KeyframeSelector>& positions,
								   CSS_property_list* prop_list) :
	m_keyframes(keyframes),
	m_prop_list(NULL)
{
	Replace(positions, prop_list);
}

CSS_KeyframeRule::~CSS_KeyframeRule()
{
	if (m_prop_list)
		m_prop_list->Unref();
	m_positions.Clear();
}

BOOL CSS_KeyframeRule::operator==(const CSS_KeyframeRule& other)
{
	CSS_KeyframeSelector* selector = NULL;
	CSS_KeyframeSelector* other_selector = NULL;

	for (selector = GetFirstSelector(), other_selector = other.GetFirstSelector();
		 selector && other_selector;
		 selector = selector->Suc(), other_selector = other_selector->Suc())
	{
		if (selector->GetPosition() != other_selector->GetPosition())
			return FALSE;
	}

	return !selector && !other_selector;
}

void
CSS_KeyframeRule::Replace(List<CSS_KeyframeSelector>& positions, CSS_property_list* props)
{
	m_positions.Clear();

	while (!positions.Empty())
	{
		CSS_KeyframeSelector* keyframe_sel = positions.First();
		keyframe_sel->Out();

		if (keyframe_sel->GetPosition() >= 0.0 && keyframe_sel->GetPosition() <= 1.0)
			keyframe_sel->Into(&m_positions);
		else
			OP_DELETE(keyframe_sel);
	}

	if (props)
		props->Ref();

	if (m_prop_list)
		m_prop_list->Unref();

	m_prop_list = props;
}

/* virtual */ OP_STATUS
CSS_KeyframeRule::GetCssText(CSS* stylesheet, TempBuffer* buf, unsigned int indent_level)
{
	GetKeyText(buf);
	RETURN_IF_ERROR(buf->Append(" { "));
	TRAP_AND_RETURN(stat, m_prop_list->AppendPropertiesAsStringL(buf));
	return buf->Append(" }");
}

/* virtual */ CSS_PARSE_STATUS
CSS_KeyframeRule::SetCssText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len)
{
	if (m_keyframes)
		return stylesheet->ParseAndInsertRule(hld_prof, this, NULL, m_keyframes, TRUE, CSS_TOK_DOM_KEYFRAME_RULE, text, len);
	else
		return OpStatus::OK;
}

OP_STATUS
CSS_KeyframeRule::GetKeyText(TempBuffer* buf) const
{
	for (CSS_KeyframeSelector* selector = GetFirstSelector(); selector; selector = selector->Suc())
	{
		RETURN_IF_ERROR(buf->AppendDouble(selector->GetPosition() * 100));
		RETURN_IF_ERROR(buf->Append("%"));

		if (selector->Suc())
			RETURN_IF_ERROR(buf->AppendFormat(UNI_L(", ")));
	}

	return OpStatus::OK;
}

OP_STATUS
CSS_KeyframeRule::SetKeyText(const uni_char* key_text, unsigned key_text_length)
{
	// Parse something like '45%, 21.2%', a comma-separated list of
	// percentage values and set them as the list of selectors.

	AutoDeleteList<CSS_KeyframeSelector> new_positions;

	CSS_Buffer css_buf;
	if (css_buf.AllocBufferArrays(1))
	{
		int token;
		css_buf.AddBuffer(key_text, uni_strlen(key_text));
		CSS_Lexer lexer(&css_buf, FALSE);
		YYSTYPE value;

		while ((token = lexer.Lex(&value)) == CSS_TOK_SPACE)
			;

		while (token != CSS_TOK_EOF)
		{
			double position = 0;

			switch (token)
			{
			case CSS_TOK_IDENT:
				position = css_buf.GetKeyframePosition(value.str.start_pos, value.str.str_len);
				break;
			case CSS_TOK_PERCENTAGE:
				position = value.number.number / 100.0;
				break;
			default:
				return OpStatus::OK;
			}

			if (position >= 0.0 && position <= 1.0)
			{
				CSS_KeyframeSelector* keyframe_selector = OP_NEW(CSS_KeyframeSelector, (position));
				if (!keyframe_selector)
					return OpStatus::ERR_NO_MEMORY;

				keyframe_selector->Into(&new_positions);
			}
			else
				return OpStatus::OK;

			while ((token = lexer.Lex(&value)) == CSS_TOK_SPACE)
				;

			if (token == CSS_TOK_EOF)
				break;

			if (token != CSS_COMMA)
				return OpStatus::OK;

			while ((token = lexer.Lex(&value)) == CSS_TOK_SPACE)
				;
		}

		m_positions.Clear();
		while (CSS_KeyframeSelector* keyframe_selector = new_positions.First())
		{
			keyframe_selector->Out();
			keyframe_selector->Into(&m_positions);
		}

		return OpStatus::OK;
	}
	else
		return OpStatus::ERR_NO_MEMORY;
}

/* virtual */ OP_STATUS
CSS_KeyframesRule::GetCssText(CSS* stylesheet, TempBuffer* buf, unsigned int indent_level)
{
	RETURN_IF_ERROR(buf->AppendFormat(UNI_L("@keyframes %s {\n"), m_name));

	CSS_KeyframeRule* iter = First();
	while (iter)
	{
		for (unsigned int i = 0; i < indent_level + 1 ; i++)
			RETURN_IF_ERROR(buf->Append("  "));
		RETURN_IF_ERROR(iter->GetCssText(stylesheet, buf, indent_level + 1));
		RETURN_IF_ERROR(buf->Append("\n"));

		iter = iter->Suc();
	}

	for (unsigned int i = 0; i < indent_level; i++)
		RETURN_IF_ERROR(buf->Append("  "));
	return buf->Append("}");
}

/* virtual */ CSS_PARSE_STATUS
CSS_KeyframesRule::SetCssText(HLDocProfile* hld_prof, CSS* stylesheet, const uni_char* text, int len)
{
	return stylesheet->ParseAndInsertRule(hld_prof, this, NULL, this, TRUE, CSS_TOK_DOM_KEYFRAMES_RULE, text, len);
}

CSS_PARSE_STATUS
CSS_KeyframesRule::AppendRule(HLDocProfile* hld_prof, CSS* sheet, const uni_char* rule, unsigned rule_length)
{
	return sheet->ParseAndInsertRule(hld_prof, this, GetConditionalRule(), this, FALSE, CSS_TOK_DOM_KEYFRAME_RULE, rule, rule_length);
}

OP_STATUS
CSS_KeyframesRule::DeleteRule(const uni_char* key_text, unsigned key_text_length)
{
	List<CSS_KeyframeSelector> dummy;
	CSS_KeyframeRule comparison_rule(NULL, dummy, NULL);

	RETURN_IF_ERROR(comparison_rule.SetKeyText(key_text, key_text_length));

	TempBuffer buf;

	CSS_KeyframeRule* iter = First();
	while (iter)
	{
		buf.Clear();

		if (comparison_rule == *iter)
		{
			CSS_KeyframeRule* delete_rule = iter;
			iter = iter->Suc();

			delete_rule->Out();
			OP_DELETE(delete_rule);
		}
		else
			iter = iter->Suc();
	}

	return OpStatus::OK;
}

OP_STATUS
CSS_KeyframesRule::FindRule(CSS_KeyframeRule*& rule, const uni_char* key_text, unsigned key_text_length)
{
	List<CSS_KeyframeSelector> dummy;
	CSS_KeyframeRule comparison_rule(NULL, dummy, NULL);

	rule = NULL;

	RETURN_IF_ERROR(comparison_rule.SetKeyText(key_text, key_text_length));

	TempBuffer buf;

	CSS_KeyframeRule* iter = First();
	while (iter)
	{
		buf.Clear();

		if (comparison_rule == *iter)
		{
			rule = iter;
			return OpStatus::OK;
		}
		else
			iter = iter->Suc();
	}

	return OpStatus::OK;
}

#endif // CSS_ANIMATIONS
