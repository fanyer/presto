/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

# include "modules/logdoc/logdoc_module.h"

# include "modules/logdoc/src/html5/html5base.h"

#include "modules/logdoc/html5parser.h"
#include "modules/logdoc/src/html5/html5token.h"
#include "modules/logdoc/src/html5/html5tokenizer.h"
#include "modules/logdoc/html5namemapper.h"

#ifndef HTML5_STANDALONE
# include "modules/doc/frm_doc.h"
# include "modules/console/opconsoleengine.h"
# include "modules/url/url2.h"
# include "modules/layout/traverse/traverse.h"
#endif // HTML5_STANDALONE


HTML5Parser::~HTML5Parser()
{
	ClearFinishedTokenizer();

#ifdef HTML5_STANDALONE
	OP_DELETE(m_token);
#endif // HTML5_STANDALONE
}

void HTML5Parser::ParseL(HTML5ELEMENT *root, HTML5ELEMENT *context)
{
	OP_PROBE7_L(OP_PROBE_HTML5PARSER_PARSEL);

	OP_ASSERT(m_tokenizer);
	if (!m_is_parsing)
	{
		m_builder.InitL(this, root);
		m_is_parsing = TRUE;
		m_is_pausing = FALSE;
		m_is_blocking = FALSE;

		if (m_is_plaintext)
			m_tokenizer->InsertPlaintextPreambleL();

		if (g_pcparsing->GetIntegerPref(PrefsCollectionParsing::ShowHTMLParsingErrors))
			m_report_errors = TRUE;

#ifdef HTML5_STANDALONE
		if (m_output_tokenizer_results)
		{
			m_token = OP_NEW_L(HTML5Token, ());
			m_token->InitializeL();
		}
#endif // HTML5_STANDALONE
	}

#ifdef HTML5_STANDALONE
	if (m_output_tokenizer_results)
	{
		do
		{
			m_tokenizer->GetNextTokenL(*m_token);
		} while (m_token->GetType() != HTML5Token::ENDOFFILE);
	}
	else
#endif // HTML5_STANDALONE
	if (context) // parsing fragment
		m_builder.BuildTreeFragmentL(context);
	else
		m_builder.BuildTreeL();

	ClearFinishedTokenizer();
	FlushErrorsToConsoleL();
	m_is_parsing = FALSE;
}

void HTML5Parser::ContinueParsingL()
{
#ifdef HTML5_STANDALONE
	if (m_output_tokenizer_results)
	{
		do
		{
			m_tokenizer->GetNextTokenL(*m_token);
		} while (m_token->GetType() != HTML5Token::ENDOFFILE);

		return;
	}
#endif // HTML5_STANDALONE

	if (IsBlocked())
		LEAVE(HTML5ParserStatus::EXECUTE_SCRIPT);

	m_builder.BuildTreeL();

	ClearFinishedTokenizer();
	FlushErrorsToConsoleL();
	m_is_parsing = FALSE;
}

void HTML5Parser::StopParsingL(BOOL abort)
{
	// just finish of the tokenization to put the parser and tree in correct state
	OP_PARSER_STATUS pstat = OpStatus::OK;
	if (m_is_parsing)
	{
		do
		{
			SignalNoMoreDataL(TRUE);
			TRAP(pstat, m_builder.StopBuildingL(abort));
		}
		while (!IsFinished() && !OpStatus::IsMemoryError(pstat));
	}

	FlushErrorsToConsoleL();
	ResetParser();
}

void HTML5Parser::AppendDataL(const uni_char *buffer, unsigned length, BOOL end_of_data, BOOL is_fragment)
{
	OP_PROBE7_L(OP_PROBE_HTML5PARSER_APPENDDATA);
	if (!m_tokenizer)
		CreateTokenizerL();

	m_tokenizer->AddDataL(buffer, length, end_of_data, FALSE, is_fragment);
}

void HTML5Parser::InsertDataL(const uni_char *buffer, unsigned length, BOOL add_newline)
{
	if (!m_tokenizer)
	{
		// add a fake terminating buffer for cases where the entire
		// document is written using doc.write, just to make the
		// algorithm orthogonal.
		CreateTokenizerL();
		m_tokenizer->AddDataL(UNI_L(""), 0, TRUE, TRUE, FALSE);
	}

	m_builder.SetState(HTML5TreeState::ILLEGAL_STATE); // to make the builder set the right state

	m_tokenizer->InsertDataL(buffer, length, add_newline);
}

void HTML5Parser::ResetParser()
{
	m_err_idx = 0;

	m_is_parsing = FALSE;
	m_is_pausing = FALSE;
	m_is_blocking = FALSE;
	m_is_plaintext = FALSE;
	m_report_errors = FALSE;

	if (m_tokenizer)
		ClearFinishedTokenizer();
}

/*static*/ HTML5TreeBuilder::InsertionMode HTML5Parser::GetInsertionModeFromElementType(HTML_ElementType elm_type, BOOL is_fragment)
{
	switch (elm_type)
	{
	case Markup::HTE_SELECT:
		OP_ASSERT(is_fragment);
		return HTML5TreeBuilder::IN_SELECT;

	case Markup::HTE_TD:
	case Markup::HTE_TH:
		if (is_fragment)
			return HTML5TreeBuilder::IN_BODY;
		else
			return HTML5TreeBuilder::IN_CELL;

	case Markup::HTE_TR:
		return HTML5TreeBuilder::IN_ROW;

	case Markup::HTE_TBODY:
	case Markup::HTE_TFOOT:
	case Markup::HTE_THEAD:
		return HTML5TreeBuilder::IN_TABLE_BODY;

	case Markup::HTE_CAPTION:
		return HTML5TreeBuilder::IN_CAPTION;

	case Markup::HTE_COLGROUP:
		OP_ASSERT(is_fragment);
		return HTML5TreeBuilder::IN_COLUMN_GROUP;

	case Markup::HTE_TABLE:
		return HTML5TreeBuilder::IN_TABLE;

	case Markup::HTE_HEAD:
		OP_ASSERT(is_fragment);
		return HTML5TreeBuilder::IN_BODY;

	case Markup::HTE_BODY:
		return HTML5TreeBuilder::IN_BODY;

	case Markup::HTE_FRAMESET:
		OP_ASSERT(is_fragment);
		return HTML5TreeBuilder::IN_FRAMESET;

	case Markup::HTE_HTML:
		OP_ASSERT(is_fragment);
		return HTML5TreeBuilder::BEFORE_HEAD;

	default:
		if (is_fragment)
			return HTML5TreeBuilder::IN_BODY;
		break;
	}

	return HTML5TreeBuilder::INITIAL;
}

#ifdef SPECULATIVE_PARSER
unsigned HTML5Parser::GetStreamPosition()
{
	return m_tokenizer->GetLastBufferStartOffset() + m_tokenizer->GetLastBufferPosition();
}
#endif // SPECULATIVE_PARSER

#if defined DELAYED_SCRIPT_EXECUTION || defined SPECULATIVE_PARSER
unsigned HTML5Parser::GetLastBufferStartOffset()
{
	return m_tokenizer ? m_tokenizer->GetLastBufferStartOffset() : 0;
}

OP_STATUS HTML5Parser::StoreParserState(HTML5ParserState* state)
{
	OP_ASSERT(!GetLogicalDocument()->GetHLDocProfile()->ESIsExecutingDelayedScript()); // With DSE we should have stored state before we started the script
	RETURN_IF_ERROR(m_tokenizer->StoreTokenizerState(state));
#ifdef DELAYED_SCRIPT_EXECUTION
	RETURN_IF_ERROR(m_builder.StoreParserState(state));
#endif // DELAYED_SCRIPT_EXECUTION

	return OpStatus::OK;
}

OP_STATUS HTML5Parser::RestoreParserState(HTML5ParserState* state, HTML5ELEMENT* script_element, unsigned buffer_stream_position, BOOL script_has_completed)
{
	// The tokenizer has probably been deleted, so restore it
	if (!m_tokenizer)
		RETURN_IF_LEAVE(CreateTokenizerL());
	else
	{
		m_tokenizer->Reset();
		m_builder.ResetToken();
	}

	// It will ask for more data later by leaving.  But for now we need
	// something in the buffer for the doc.write to work (and we're almost certainly getting
	// a doc.write, otherwise we wouldn't need to restore in the first place).
	RETURN_IF_LEAVE(m_tokenizer->AddDataL(UNI_L(""), 0, FALSE, TRUE, TRUE));

	RETURN_IF_ERROR(m_tokenizer->RestoreTokenizerState(state, buffer_stream_position));
#ifdef DELAYED_SCRIPT_EXECUTION
	RETURN_IF_ERROR(m_builder.RestoreParserState(state, script_element, script_has_completed));
#endif // DELAYED_SCRIPT_EXECUTION

	return OpStatus::OK;
}
#endif // defined DELAYED_SCRIPT_EXECUTION || defined SPECULATIVE_PARSER

#ifdef DELAYED_SCRIPT_EXECUTION
OP_STATUS HTML5Parser::Recover()
{
	// The tokenizer has probably been deleted, so restore it
	if (!m_tokenizer)
		RETURN_IF_LEAVE(CreateTokenizerL());

	return OpStatus::OK;
}

BOOL HTML5Parser::HasParserStateChanged(HTML5ParserState* state)
{
	if (m_tokenizer->HasTokenizerStateChanged(state))
		return TRUE;

	return m_builder.HasParserStateChanged(state);
}

BOOL HTML5Parser::HasUnfinishedWriteData()
{
	if (m_tokenizer)
		return m_tokenizer->HasUnfinishedWriteData();
	return FALSE;
}
#endif // DELAYED_SCRIPT_EXECUTION

static BOOL ShouldSerializeChildren(Markup::Type type)
{
	switch (type)
	{
	case Markup::HTE_AREA:
	case Markup::HTE_BASE:
	case Markup::HTE_BASEFONT:
	case Markup::HTE_BGSOUND:
	case Markup::HTE_BR:
	case Markup::HTE_COL:
	case Markup::HTE_EMBED:
	case Markup::HTE_FRAME:
	case Markup::HTE_HR:
	case Markup::HTE_IMG:
	case Markup::HTE_INPUT:
	case Markup::HTE_KEYGEN:
	case Markup::HTE_LINK:
	case Markup::HTE_META:
	case Markup::HTE_PARAM:
	case Markup::HTE_WBR:
		return FALSE;
	}

	return TRUE;
}

static BOOL AttributeNeedToEscapeQuote(Markup::Type type, const uni_char* name, const uni_char* value, uni_char& quote_to_use)
{
	BOOL check_value = FALSE;
	quote_to_use = '"';

	// only for some URI attributes for specific elements should we not escape
	// list based on what Webkit does, see CT-371
	switch (type)
	{
	case Markup::HTE_IMG:
		if (uni_str_eq(name, "longdesc"))
			check_value = TRUE;
		else if (uni_str_eq(name, "usemap"))
			check_value = TRUE;
		else
		// fall through
	case Markup::HTE_SCRIPT:
	case Markup::HTE_INPUT:
	case Markup::HTE_FRAME:
	case Markup::HTE_IFRAME:
			check_value = uni_str_eq(name, "src");
		break;

	case Markup::HTE_A:
	case Markup::HTE_AREA:
	case Markup::HTE_LINK:
	case Markup::HTE_BASE:
		check_value = uni_str_eq(name, "href");
		break;

	case Markup::HTE_OBJECT:
		check_value = uni_str_eq(name, "usemap");
		break;

	case Markup::HTE_FORM:
		check_value = uni_str_eq(name, "action");
		break;

	case Markup::HTE_BLOCKQUOTE:
	case Markup::HTE_Q:
	case Markup::HTE_DEL:
	case Markup::HTE_INS:
		check_value = uni_str_eq(name, "cite");
		break;
	}

	if (check_value)
	{
		BOOL contains_quot = FALSE, contains_apos = FALSE;

		// skip leading whitespace
		while (HTML5Parser::IsHTML5WhiteSpace(*value))
			value++;

		if (!uni_strni_eq(value, "javascript:", 11))
			return TRUE;

		// javascript URLs shouldn't be quoted unless they have to
		value += 11;  // skip "javascript:"
		for (; *value; value++)
		{
			if (*value == '"')
				if (contains_apos)
					return TRUE;
				else
					contains_quot = TRUE;
			else if (*value == '\'')
				if (contains_quot)
					return TRUE;
				else
					contains_apos = TRUE;
		}

		if (contains_quot)
			quote_to_use = '\'';

		return FALSE;
	}

	return TRUE;
}

static void QuoteInnerHTMLValuesL(const uni_char *value, TempBuffer *buf, BOOL in_attr, BOOL escape_quote, BOOL escape_nbsp)
{
	for (const uni_char *c = value; *c; c++)
	{
		switch (*c)
		{
		case '&':
			buf->AppendL("&amp;");
			break;
		case 0xa0:
			if (escape_nbsp)
				buf->AppendL("&nbsp;");
			else
				buf->AppendL(*c);
			break;
		case '"':
			if (in_attr && escape_quote)
				buf->AppendL("&quot;");
			else
				buf->AppendL(*c);
			break;
		case '<':
			if (!in_attr)
				buf->AppendL("&lt;");
			else
				buf->AppendL(*c);
			break;
		case '>':
			if (!in_attr)
				buf->AppendL("&gt;");
			else
				buf->AppendL(*c);
			break;
		default:
			buf->AppendL(*c);
		}
	}
}

static BOOL IsTextWithFirstCharacter(HTML_Element *elm, uni_char character)
{
	if (elm->Type() == Markup::HTE_TEXT && elm->Content())
		return  elm->Content()[0] == character;
	else if (elm->Type() == Markup::HTE_TEXTGROUP && elm->FirstChild() && elm->FirstChild()->Content())
		return elm->FirstChild()->Content()[0] == character;
	return FALSE;
}

static const uni_char *GetTagNameFromElement(HTML5ELEMENT *elm)
{
#ifdef HTML5_STANDALONE
	return elm->GetName();
#else // HTML5_STANDALONE
	Markup::Type type = elm->Type();
	if (type == Markup::HTE_UNKNOWN)
		return elm->GetTagName();
	else
	{
		Markup::Ns ns = elm->GetNs();
		return g_html5_name_mapper->GetNameFromType(type, ns, FALSE);
	}
#endif // HTML5_STANDALONE
}

/**
 * A helper method for serializing a tree.
 *
 * @param[in] root - The root node of the tree to be serialized.
 * @param[out] buffer - The buffer the serialized tree should be written to.
 * @param[in] options - The serialization options.
 * @param[in] real_parent - Used when serializing text groups. See SerializeTextgroupL() for datails.
 *
 * @see HTML5Parser::SerializeTreeOptions.
 * @see SerializeTextgroupL()
 */
static void SerializeTreeInternalL(HTML5NODE *root, TempBuffer* buffer, const HTML5Parser::SerializeTreeOptions &options, HTML5NODE* real_parent = NULL);

/**
 * A helper method for serializing a text group.
 *
 * @param[in] textgroup - The textgroup element to be serialized.
 * @param[out] buffer - The buffer the serialized text group should be written to.
 * @param[in] options - The serialization options.
 * @param[in] real_parent - The real parent of the text element being under the text group now.
 * It's needed becase the serialization algorithm must sometimes do special things depending on it
 * e.g. when serializing a text element being a child of <xmp>. Note that HTML_Element::Parent()
 * is no help here since the text group is the parent of the text element now i.e. because we serialize
 * <textgroup> the tree must have been changed from:
 *
 * <the real parent>
 *       |
 * <a text element with more than 32k characters>
 *
 * to:
 *
 * <the real parent>
 *        |
 *   <text group> ------------------------------------------+
 *      |                                                   |
 * <a text element 32k characters long at most>      <a text element 32k characters long at most> ...
 *
 * @see HTML5Parser::SerializeTreeOptions.
 * @see SerializeTreeInternalL()
 */
static void SerializeTextgroupL(HTML5NODE *textgroup, TempBuffer* buffer, const HTML5Parser::SerializeTreeOptions &options, HTML5NODE* real_parent = NULL)
{
	HTML5NODE *child_node = textgroup->NextActual();
	HTML5NODE *stop_node = textgroup->NextSiblingActual();
	while (child_node != stop_node)
	{
#ifdef PHONE2LINK_SUPPORT
		/* Normally we insert HTE_TEXT elements only as children of an HTE_TEXTGROUP.
		   However with the PHONE2LINK feature an HTE_A may end up under an HTE_TEXTGROUP
		   and we don't want such to be visible in .innerHTML.
		*/
		if (child_node->Type() == Markup::HTE_TEXT)
#endif // PHONE2LINK_SUPPORT
			SerializeTreeInternalL(child_node, buffer, options, real_parent ? real_parent : textgroup->Parent());
		child_node = child_node->NextActual();
	}
}

/*static*/ void SerializeTreeInternalL(HTML5NODE *root, TempBuffer* buffer, const HTML5Parser::SerializeTreeOptions &options, HTML5NODE* real_parent)
{
#ifndef HTML5_STANDALONE
	if (root->GetInserted() > HE_INSERTED_BYPASSING_PARSER)
		return;
#endif // HTML5_STANDALONE

	Markup::Type type = root->Type();
	switch (type)
	{
	case Markup::HTE_TEXTGROUP:
		SerializeTextgroupL(root, buffer, options, real_parent);
		break;

	case Markup::HTE_TEXT:
	case Markup::HTE_CDATA:
		{
			const uni_char *content = static_cast<HTML5TEXT*>(root)->Content();
			HTML5ELEMENT *parent = real_parent ? real_parent : root->Parent();
			if (parent)
			{
				if (options.m_text_only
					|| parent->GetNs() == Markup::HTML
					&& (parent->Type() == Markup::HTE_STYLE
					|| parent->Type() == Markup::HTE_SCRIPT
					|| parent->Type() == Markup::HTE_XMP
					|| parent->Type() == Markup::HTE_IFRAME
					|| parent->Type() == Markup::HTE_NOEMBED
					|| parent->Type() == Markup::HTE_NOFRAMES
					|| parent->Type() == Markup::HTE_PLAINTEXT
					|| parent->Type() == Markup::HTE_NOSCRIPT))
				{
					buffer->AppendL(content);
				}
				else
					QuoteInnerHTMLValuesL(content, buffer, FALSE, FALSE, !options.m_is_xml);
			}
			else
				buffer->AppendL(content);
		}
		break;

	case Markup::HTE_COMMENT:
		if (!options.m_text_only)
		{
			buffer->AppendL(UNI_L("<!--"));
#ifdef HTML5_STANDALONE
			buffer->AppendL(static_cast<HTML5COMMENT*>(root)->GetData());
#else // HTML5_STANDALONE
			buffer->AppendL(root->GetStringAttr(Markup::HA_CONTENT));
#endif // HTML5_STANDALONE
			buffer->AppendL(UNI_L("-->"));
		}
		break;

#ifndef HTML5_STANDALONE
	case Markup::HTE_PROCINST:
		if (!options.m_text_only)
		{
			buffer->AppendL(UNI_L("<?"));
			buffer->AppendL(root->GetStringAttr(Markup::HA_TARGET));
			buffer->AppendL(' ');
			buffer->AppendL(root->GetStringAttr(Markup::HA_CONTENT));
			buffer->AppendL('>');
		}
		break;
#endif // HTML5_STANDALONE

	case Markup::HTE_DOCTYPE:
		if (!options.m_text_only)
		{
			buffer->AppendL(UNI_L("<!DOCTYPE "));
			const uni_char *name = static_cast<HTML5DOCTYPE*>(root)->GetName();
			buffer->AppendL(name ? name : UNI_L("html"));
			buffer->AppendL(UNI_L(">"));
		}
		break;

	case Markup::HTE_DOC_ROOT:
		{
			HTML5ELEMENT *elm = static_cast<HTML5ELEMENT*>(root);
			HTML5NODE *child_node = elm->FirstChildActual();
			while (child_node)
			{
				SerializeTreeInternalL(child_node, buffer, options);
				child_node = child_node->SucActual();
			}
		}
		break;

	case Markup::HTE_ENTITY:
	case Markup::HTE_ENTITYREF:
	case Markup::HTE_UNKNOWNDECL:
		// ignore
		break;

	case Markup::HTE_BR:
		if (options.m_text_only)
		{
			// See SerializeTree() comment why this is done.
			buffer->AppendL('\n');
			break;
		}

	default:
		{
			HTML5ELEMENT *elm = static_cast<HTML5ELEMENT*>(root);
			const uni_char *elm_name = NULL;
			if (!options.m_text_only)
			{
				buffer->AppendL('<');

				elm_name = GetTagNameFromElement(elm);
				buffer->AppendL(elm_name);

#define APPEND_ATTRIBUTE(name, value) do {\
	uni_char quote; \
	BOOL escape_quote = AttributeNeedToEscapeQuote(type, name, value, quote); \
	buffer->AppendL(' '); \
	buffer->AppendL(name); \
	buffer->AppendL(UNI_L("=")); \
	buffer->AppendL(quote); \
	QuoteInnerHTMLValuesL(value, buffer, TRUE, escape_quote, options.m_is_xml); \
	buffer->AppendL(quote); } while (FALSE)

				if (!options.m_skip_attributes)
				{
#ifdef HTML5_STANDALONE
					unsigned attr_count = elm->GetAttributeCount();
					for (unsigned i = 0; i < attr_count; i++)
					{
						HTML5AttrCopy *attr = elm->GetAttributeByIndex(i);
						APPEND_ATTRIBUTE(attr->GetName()->GetBuffer(), attr->GetValue());
					}
#else // HTML5_STANDALONE
					const uni_char *attr_name, *attr_value;
					HTML_AttrIterator iter(elm);
					while (iter.GetNext(attr_name, attr_value))
					{
						APPEND_ATTRIBUTE(attr_name, attr_value);
					}
#endif // HTML5_STANDALONE
				}

				buffer->AppendL('>');

				if (type == Markup::HTE_PRE || type == Markup::HTE_TEXTAREA || type == Markup::HTE_LISTING)
				{
					HTML5NODE *child_node = root->FirstChildActual();
					if (child_node && IsTextWithFirstCharacter(child_node, 0x0a))
						buffer->AppendL(0x0a);
				}
			}

			if (ShouldSerializeChildren(type))
			{
				HTML5NODE *child_node = elm->FirstChildActual();
				while (child_node)
				{
					SerializeTreeInternalL(child_node, buffer, options);
					child_node = child_node->SucActual();
				}

				if (!options.m_text_only)
				{
					buffer->AppendL("</");
					buffer->AppendL(elm_name);
					buffer->AppendL(">");
				}
			}
		}
		break;
	}
}

/*static*/ void	HTML5Parser::SerializeTreeL(HTML5NODE *root, TempBuffer* buffer, const SerializeTreeOptions &options)
{
#ifndef HTML5_STANDALONE
	if (root->GetInserted() > HE_INSERTED_BYPASSING_PARSER)
		return;
#endif // HTML5_STANDALONE

	if (options.m_include_this)
		SerializeTreeInternalL(root, buffer, options);
	else
	{
		Markup::Type type = root->Type();
		switch (type)
		{
		case Markup::HTE_TEXT:
		case Markup::HTE_CDATA:
		case Markup::HTE_COMMENT:
#ifndef HTML5_STANDALONE
		case Markup::HTE_PROCINST:
#endif // HTML5_STANDALONE
		case Markup::HTE_DOCTYPE:
		case Markup::HTE_ENTITY:
		case Markup::HTE_ENTITYREF:
		case Markup::HTE_UNKNOWNDECL:
			// ignore
			break;

		case Markup::HTE_TEXTGROUP:
			SerializeTextgroupL(root, buffer, options);
			break;

		case Markup::HTE_BR:
			if (options.m_text_only)
			{
				// HTMLElement.innerText pecularity. <br> and '\n' convert to each other. This is
				// not fully supported by any specification but it is what old Operas did and what
				// WebKit does. See CORE-46682.
				buffer->AppendL('\n');
			}
			break;

		default:
			if (ShouldSerializeChildren(type))
			{
				HTML5ELEMENT *elm = static_cast<HTML5ELEMENT*>(root);
				HTML5NODE *child_node = elm->FirstChildActual();
				while (child_node)
				{
					SerializeTreeInternalL(child_node, buffer, options);
					child_node = child_node->SucActual();
				}
			}
			break;
		}
	}
}

#ifndef HTML5_STANDALONE
class SerializeSelectionState
{
public:
	SerializeSelectionState(TempBuffer *buffer, const HTML5Parser::SerializeTreeOptions &options)
		: m_start_elm(NULL)
		, m_end_elm(NULL)
		, m_start_unit(NULL)
		, m_end_unit(NULL)
		, m_common_ancestor(NULL)
		, m_buffer(buffer)
		, m_options(options) {}

	OP_STATUS	Init(const TextSelection &selection)
	{
		// make an empty buffer, so if there is no content in the range we
		// return an empty string instead of NULL.
		RETURN_IF_MEMORY_ERROR(m_buffer->Expand(64));
		m_start_elm = selection.GetStartElement();
		m_end_elm = selection.GetEndElement();

		// TODO later: refactor out common ancestor code from DOM_Range
		if (m_start_elm == m_end_elm)
			m_common_ancestor = m_start_elm;
		else
		{
			unsigned start_height = 0;
			unsigned end_height = 0;
			HTML5NODE *start_iter = m_start_elm;
			HTML5NODE *end_iter = m_end_elm;

			OP_ASSERT(start_iter && end_iter);

			HTML5NODE *parent = start_iter;
			while (parent)
			{
				++start_height;
				parent = parent->ParentActual();
			}

			parent = end_iter;
			while (parent)
			{
				++end_height;
				parent = parent->ParentActual();
			}

			while (end_height < start_height - 1)
			{
				--start_height;
				start_iter = start_iter->ParentActual();
			}

			while (start_height < end_height - 1)
			{
				--end_height;
				end_iter = end_iter->ParentActual();
			}

			while (start_iter != end_iter)
			{
				if (start_height >= end_height)
					start_iter = start_iter->ParentActual();

				if (end_height >= start_height)
					end_iter = end_iter->ParentActual();

				start_height = end_height = 0;
			}

			m_common_ancestor = start_iter;
		}

		m_start_offset = selection.GetStartSelectionPoint().GetElementCharacterOffset();
		if ((m_start_unit = m_start_elm->FirstChild()) != NULL)
		{
			unsigned index = 0;
			while (m_start_unit && index < m_start_offset)
			{
				m_start_unit = m_start_unit->SucActual();
				index++;
			}

			m_start_offset = 0;
		}

		m_end_offset = selection.GetEndSelectionPoint().GetElementCharacterOffset();
		if ((m_end_unit = m_end_elm->FirstChild()) != NULL)
		{
			unsigned index = 0;
			while (m_end_unit && index < m_end_offset)
			{
				m_end_unit = m_end_unit->SucActual();
				index++;
			}

			m_end_offset = UINT_MAX;
		}

		return OpStatus::OK;
	}

	void	AppendL(const uni_char *s, size_t n = static_cast<size_t>(-1))
	{
		m_buffer->AppendL(s, n);
	}

	void	AppendCharacterL(const uni_char c)
	{
		m_buffer->AppendL(c);
	}

	HTML5NODE*	m_start_elm;
	HTML5NODE*	m_end_elm;
	HTML5NODE*	m_start_unit;
	HTML5NODE*	m_end_unit;
	HTML5NODE*	m_common_ancestor;
	TempBuffer*	m_buffer;
	const HTML5Parser::SerializeTreeOptions&
				m_options;
	unsigned	m_start_offset;
	unsigned	m_end_offset;
};

static void SerializeParentChainL(SerializeSelectionState &state, HTML5NODE *elm, BOOL is_end_branch)
{
	if (elm != state.m_common_ancestor)
	{
		if (!is_end_branch)
			SerializeParentChainL(state, elm->ParentActual(), FALSE);

		const uni_char *elm_name = GetTagNameFromElement(elm);
		if (elm_name && (!is_end_branch || ShouldSerializeChildren(elm->Type())))
		{
			state.AppendCharacterL('<');
			if (is_end_branch)
				state.AppendCharacterL('/');
			state.AppendL(elm_name);
			state.AppendCharacterL('>');
		}

		if (is_end_branch)
			SerializeParentChainL(state, elm->ParentActual(), TRUE);
	}
}

static void SerializeTextL(SerializeSelectionState &state, HTML5NODE *elm, unsigned start_offset, unsigned end_offset)
{
	HTML5NODE *stop_at = elm->NextSibling();
	while (elm != stop_at)
	{
		const uni_char *content = elm->Content();
		unsigned content_len = elm->ContentLength();
		if (content_len > start_offset)
		{
			if (content_len > end_offset)
			{
				state.AppendL(content + start_offset, end_offset - start_offset);
				break;
			}
			else
			{
				state.AppendL(content + start_offset);
				end_offset -= content_len;
			}

			start_offset = 0;
		}
		else
		{
			start_offset -= content_len;
			end_offset -= content_len;
		}

		elm = elm->Next();
	}
}

static void SerializeStartBranchL(SerializeSelectionState &state, HTML5NODE *elm, HTML5NODE *child)
{
	if (elm != state.m_common_ancestor)
	{
		if (elm == state.m_start_elm)
			SerializeParentChainL(state, elm, FALSE);

		if (elm->IsText())
			SerializeTextL(state, elm, state.m_start_offset, UINT_MAX);
		else
		{
			if (child != state.m_start_unit)
				child = child->SucActual();

			while (child)
			{
				if (child->IsText())
					SerializeTextL(state, child, 0, UINT_MAX);
				else
					SerializeTreeInternalL(child, state.m_buffer, state.m_options);

				child = child->SucActual();
			}

			const uni_char *elm_name = GetTagNameFromElement(elm);
			if (ShouldSerializeChildren(elm->Type()) && elm_name)
			{
				state.AppendCharacterL('<');
				state.AppendCharacterL('/');
				state.AppendL(elm_name);
				state.AppendCharacterL('>');
			}
		}

		SerializeStartBranchL(state, elm->ParentActual(), elm);
	}
}

static void SerializeEndBranchL(SerializeSelectionState &state, HTML5NODE *elm, HTML5NODE *child)
{
	if (elm != state.m_common_ancestor)
	{
		SerializeEndBranchL(state, elm->ParentActual(), elm);

		if (elm->IsText())
			SerializeTextL(state, elm, 0, state.m_end_offset);
		else
		{
			const uni_char *elm_name = GetTagNameFromElement(elm);
			if (elm_name)
			{
				state.AppendCharacterL('<');
				state.AppendL(elm_name);
				state.AppendCharacterL('>');
			}

			HTML5NODE *iter = elm->FirstChildActual();
			while (iter != child)
			{
				if (iter->IsText())
					SerializeTextL(state, iter, 0, UINT_MAX);
				else
					SerializeTreeInternalL(iter, state.m_buffer, state.m_options);

				iter = iter->SucActual();
			}
		}

		if (elm == state.m_end_elm)
			SerializeParentChainL(state, elm, TRUE);
	}
}

/*static*/ void	HTML5Parser::SerializeSelectionL(const TextSelection &selection, TempBuffer* buffer, const SerializeTreeOptions &options)
{
	OP_ASSERT(options.m_include_this || !"All contents of the selection must be serialized.");
	SerializeSelectionState state(buffer, options);
	LEAVE_IF_ERROR(state.Init(selection));

	if (state.m_start_elm == state.m_end_elm)
	{
		if (state.m_start_elm->IsText())
			SerializeTextL(state, state.m_start_elm, state.m_start_offset, state.m_end_offset);
		else
			SerializeTreeInternalL(state.m_start_elm, state.m_buffer, state.m_options);
	}
	else
	{
		SerializeStartBranchL(state, state.m_start_elm, state.m_start_unit);

		HTML5NODE *iter = state.m_start_unit;
		HTML5NODE *stop = state.m_end_unit;
		HTML5NODE *container = state.m_start_elm;

		while (container != state.m_common_ancestor)
		{
			iter = container;
			container = container->ParentActual();
		}

		container = state.m_end_elm;
		while (container != state.m_common_ancestor)
		{
			stop = container;
			container = container->ParentActual();
		}

		if (iter != state.m_start_unit)
			// Leave the tree extracted by SerializeStartBranchL().
			iter = iter->SucActual();

#ifdef _DEBUG
		if (stop && iter && iter != stop)
			OP_ASSERT(iter->Precedes(stop));
#endif // _DEBUG

		while (iter != stop)
		{
			SerializeTreeInternalL(iter, state.m_buffer, state.m_options);
			iter = iter->SucActual();
		}

		SerializeEndBranchL(state, state.m_end_elm, state.m_end_unit);
	}
}
#endif // !HTML5_STANDALONE

void HTML5Parser::SignalNoMoreDataL(BOOL stopping)
{
	if (m_tokenizer)
		m_tokenizer->CloseLastBuffer();
	else if (!stopping)
		AppendDataL(UNI_L(""), 0, TRUE, FALSE);
}

void HTML5Parser::SignalErrorL(ErrorCode code, unsigned line, unsigned pos)
{
	if (m_report_errors)
	{
		if (m_err_idx == kErrorArraySize)
			FlushErrorsToConsoleL();

		m_errors[m_err_idx].m_code = code;
		m_errors[m_err_idx].m_line = line;
		m_errors[m_err_idx++].m_pos = pos;
	}
}

HTML5Parser::ErrorCode	HTML5Parser::GetError(unsigned i, unsigned &line, unsigned &pos) const
{
	line = m_errors[i].m_line;
	pos = m_errors[i].m_pos;
	return m_errors[i].m_code;
}

#include "modules/logdoc/src/html5/errordescs.h"

/*static*/ const char* HTML5Parser::GetErrorString(ErrorCode code)
{
	return g_html5_error_descs[code];
}

void HTML5Parser::FlushErrorsToConsoleL()
{
#if !defined HTML5_STANDALONE && defined OPERA_CONSOLE
	if (m_err_idx > 0)
	{
		if (FramesDocument *doc = m_logdoc->GetFramesDocument())
		{
			OpConsoleEngine::Message cmessage(OpConsoleEngine::HTML, OpConsoleEngine::Error);
			ANCHOR(OpConsoleEngine::Message, cmessage);
			cmessage.window = doc->GetWindow()->Id();
			doc->GetURL().GetAttribute(URL::KUniName, cmessage.url);
			cmessage.context.SetL(UNI_L("During HTML parsing..."));

			for (unsigned i = 0; i < m_err_idx; i++)
			{
				cmessage.message.SetL(GetErrorString(m_errors[i].m_code));
				LEAVE_IF_ERROR(cmessage.message.AppendFormat(UNI_L("\n\nLine %d, position %d:\n  "), m_errors[i].m_line, m_errors[i].m_pos));
				g_console->PostMessageL(&cmessage);
			}
		}
	}
#endif // !HTML5_STANDALONE && OPERA_CONSOLE

	m_err_idx = 0;
}

void HTML5Parser::CreateTokenizerL()
{
	OP_ASSERT(!m_tokenizer);

	m_tokenizer = OP_NEW_L(HTML5Tokenizer, (this));

	m_builder.SetCurrentTokenizer(m_tokenizer);
	m_builder.ResetToken();
}

void HTML5Parser::ClearFinishedTokenizer()
{
	m_builder.SetCurrentTokenizer(NULL);
	OP_DELETE(m_tokenizer);
	m_tokenizer = NULL;
}
