/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/logdoc/src/html5/html5treebuilder.h"

#include "modules/logdoc/html5parser.h"
#include "modules/logdoc/html5namemapper.h"
#include "modules/logdoc/src/html5/html5treestate.h"
#include "modules/logdoc/src/html5/html5tokenizer.h"
#include "modules/logdoc/src/html5/transitions.h"

#include "modules/logdoc/src/html5/html5dse_restore_attr.h"
#include "modules/logdoc/source_position_attr.h"

#include "modules/logdoc/logdoc.h"
#include "modules/doc/frm_doc.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"

#ifdef DELAYED_SCRIPT_EXECUTION
# include "modules/prefs/prefsmanager/collections/pc_js.h"
#endif // DELAYED_SCRIPT_EXECUTION

// Various defines to unclutter the huge tree building switch
#define GET_TYPE(type) \
	if (m_token.GetElementType() == Markup::HTE_UNKNOWN) \
		m_token.SetElementType(g_html5_name_mapper->GetTypeFromTokenBuffer(m_token.GetName())); \
	Markup::Type type = m_token.GetElementType()

#define EMIT_ERROR(error) m_parser->SignalErrorL(HTML5Parser::error, m_token.GetLineNum(), m_token.GetLinePos())

#define SWITCH_TO_AND_REPROCESS(mode) \
	SetInsertionMode(mode); \
	SetNewState(); \
	continue

#define PROCESS_USING(mode) { \
	StartUsingIM(mode); \
	SetNewState(); \
	continue; \
	}

#define RESTORE_ORIGINAL_AND_REPROCESS \
	SetInsertionMode(m_original_im); \
	SetNewState(); \
	continue

#define TERMINATE_IF_P_INSCOPE \
	if (HasElementTypeInScope(Markup::HTE_P, SCOPE_BUTTON)) \
		HandleSpecialEndTag(Markup::HTE_P, FALSE)


class HTML5TreeBuilder::ActiveListBookmark
{
public:
	ActiveListBookmark(List<HTML5TreeBuilder::HTML5ActiveElement> *list, HTML5TreeBuilder::HTML5ActiveElement *list_entry)
		: m_list(list)
		, m_pred(list_entry->Pred())
		, m_suc(list_entry->Suc()) {}

	void	SetBookmark(HTML5TreeBuilder::HTML5ActiveElement *list_entry)
	{
		if (list_entry)
		{
			m_pred = list_entry->Pred();
			m_suc = list_entry;
		}
		else
		{
			m_pred = NULL;
			m_suc = NULL;
		}
	}

	void	InsertAtBookmark(HTML5TreeBuilder::HTML5ActiveElement *list_entry)
	{
		OP_ASSERT(list_entry && list_entry->Pred() == NULL && list_entry->Suc() == NULL);
		OP_ASSERT(m_pred == NULL || m_pred->Suc() == m_suc);
		if (m_pred)
			list_entry->Follow(m_pred);
		else if (m_suc)
			list_entry->Precede(m_suc);
		else
			list_entry->Into(m_list);
	}

private:
	List<HTML5TreeBuilder::HTML5ActiveElement>*	m_list;
	HTML5TreeBuilder::HTML5ActiveElement*		m_pred;
	HTML5TreeBuilder::HTML5ActiveElement*		m_suc;
};

class HTML5TreeBuilder::ParserScriptElm : public ListElement<HTML5TreeBuilder::ParserScriptElm>
{
public:
	ParserScriptElm(HTML5ELEMENT *elm, BOOL is_blocking) : m_script_elm(elm), m_is_blocking(is_blocking), m_is_ready(FALSE), m_is_started(FALSE) {}

	HTML5ELEMENT*	GetElm() { return m_script_elm.GetElm(); }

	BOOL	IsBlocking() { return m_is_blocking; }

	void	SetIsReady() { m_is_ready = TRUE; }
	BOOL	IsReady() { return m_is_ready; }

	void	SetIsStarted() { m_is_started = TRUE; m_is_blocking = FALSE; }
	BOOL	IsStarted() { return m_is_started; }

private:

	class ParserScriptElmRef : public ElementRef
	{
	public:
		ParserScriptElmRef(HTML5ELEMENT *elm) : ElementRef(elm) {}

		virtual	void	OnDelete(FramesDocument *document)
		{
#ifndef HTML5_STANDALONE
			if (document)
			{
				if (LogicalDocument *logdoc = document->GetLogicalDocument())
					if (HTML5Parser *parser = logdoc->GetParser())
					{
						parser->GetTreeBuilder()->RemoveScriptElement(GetElm(), FALSE);
						return;
					}
				OP_ASSERT(!"Deleting a script from the parser in a document without a HTML parser is strange.");
			}
#endif // HTML5_STANDALONE
		}

		virtual	void	OnRemove(FramesDocument *document)
		{
#ifndef HTML5_STANDALONE
			if (document)
			{
				if (LogicalDocument *logdoc = document->GetLogicalDocument())
					if (HTML5Parser *parser = logdoc->GetParser())
					{
						parser->GetTreeBuilder()->RemoveScriptElement(GetElm(), FALSE);
						return;
					}
				OP_ASSERT(!"Deleting a script from the parser in a document without a HTML parser is strange.");
			}
#endif // HTML5_STANDALONE
		}
	};

	ParserScriptElmRef	m_script_elm;
	BOOL				m_is_blocking;
	BOOL				m_is_ready;
	BOOL				m_is_started;
};

HTML5TreeBuilder::~HTML5TreeBuilder()
{
	ClearElementLists();
}

void HTML5TreeBuilder::InitL(HTML5Parser *parser, HTML5ELEMENT *root)
{
	if (m_parser != parser)
	{
		m_parser = parser;
		m_tokenizer = m_parser->GetTokenizer();
	}

	m_token.InitializeL();
	m_token.Reset(HTML5Token::INVALID);

	m_root.Reset();
	m_root.SetNode(root);
	m_current_node = &m_root;

	m_pending_table_text.Clear();
	m_text_accumulator.ResetL();

	m_current_state = m_pending_state = HTML5TreeState::ILLEGAL_STATE;
	m_script_nesting_level = 0;
	m_script_elms.Clear();
	m_bool_init = 0;
	m_tag_nesting_level = 0;
	m_p_elements_in_button_scope = 0;

	SetInsertionMode(INITIAL);
#ifdef HTML5_STANDALONE
	SetScriptingEnabled(TRUE);
#else // HTML5_STANDALONE
	SetScriptingEnabled(parser->GetLogicalDocument()->GetHLDocProfile()->GetESEnabled());
#endif // HTML5_STANDALONE
	SetFramesetOk(TRUE);
}

void HTML5TreeBuilder::BuildTreeL()
{
	OP_PROBE7_L(OP_PROBE_HTML5TREEBUILDER_BUILDTREE);
	m_token.ResetWrapper();

	if (m_current_state != HTML5TreeState::ABORT)
	{
		m_tokenizer->GetNextTokenL(m_token);
		if ((m_token.GetType() != HTML5Token::CHARACTER
				|| m_text_accumulator.Length() >= SplitTextLen)
			&& m_text_accumulator.Length() > 0)
			InsertTextL();
		SetNextState();
	}

	while (m_current_state != HTML5TreeState::STOP)
	{
		switch (m_current_state)
		{
		case HTML5TreeState::IGNORE_SPACE:
			if (!m_token.IsWSOnly(FALSE))
			{
				if (!m_token.SkippedSpaces())
					m_token.SkipSpaces(GetNumberOfLeadingSpaces(m_token));

				if (m_im == INITIAL)
					SetState(HTML5TreeState::NO_DOCTYPE);
				else if (m_im == BEFORE_HTML)
					SetState(HTML5TreeState::IMPLICIT_HTML);
				else
					SetState(HTML5TreeState::IMPLICIT_HEAD);
				continue;
			}
			break;

		case HTML5TreeState::PROCESS_DOCTYPE:
			ProcessDoctypeL();
			SetInsertionMode(BEFORE_HTML);
			break;

		case HTML5TreeState::INSERT_COMMENT:
			InsertCommentL(m_token);
			break;

		case HTML5TreeState::NO_DOCTYPE:
#ifdef HTML5_STANDALONE
			if (!IsIframe())
#else // HTML5_STANDALONE
			if (!IsIframe()
				|| !m_parser->GetLogicalDocument()->IsSrcdoc())
#endif // HTML5_STANDALONE
			{
				EMIT_ERROR(CONT_BEFORE_DOCTYPE);
				SetInQuirksMode(TRUE);
			}
			SWITCH_TO_AND_REPROCESS(BEFORE_HTML);

		case HTML5TreeState::IGNORE_TOKEN_ERR:
			if (m_token.GetType() == HTML5Token::DOCTYPE)
				EMIT_ERROR(MALPLACED_DOCTYPE);
			else if (m_token.GetType() == HTML5Token::START_TAG)
				EMIT_ERROR(ILLEGAL_CONTENT);
			else
				EMIT_ERROR(ILLEGAL_ENDTAG);
			break;

		case HTML5TreeState::START_BEFORE_HTML:
			{
				GET_TYPE(type);
				if (type == Markup::HTE_HTML)
				{
					InsertStartTagL(m_token);
					m_html_elm.SetElm(GetCurrentNode());;
					SetInsertionMode(BEFORE_HEAD);
				}
				else
				{
					SetState(HTML5TreeState::IMPLICIT_HTML);
					continue;
				}
			}
			break;

		case HTML5TreeState::EARLY_END_TAG:
			{
				GET_TYPE(type);
				if (type == Markup::HTE_HEAD
					|| type == Markup::HTE_BODY
					|| type == Markup::HTE_HTML
					|| type == Markup::HTE_BR)
				{
					if (m_im == BEFORE_HTML)
						SetState(HTML5TreeState::IMPLICIT_HTML);
					else
						SetState(HTML5TreeState::IMPLICIT_HEAD);
				}
				else
					SetState(HTML5TreeState::IGNORE_TOKEN_ERR);
			}
			continue;

		case HTML5TreeState::IMPLICIT_HTML:
			InsertHtmlL();
			SWITCH_TO_AND_REPROCESS(BEFORE_HEAD);

		case HTML5TreeState::START_BEFORE_HEAD:
			{
				GET_TYPE(type);
				if (type == Markup::HTE_HTML)
					PROCESS_USING(IN_BODY)
				else if (type == Markup::HTE_HEAD)
				{
					InsertStartTagL(m_token);
					m_head_elm.SetElm(GetCurrentNode());
					SetInsertionMode(IN_HEAD);
					break;
				}

				SetState(HTML5TreeState::IMPLICIT_HEAD);
			}
			continue;

		case HTML5TreeState::IMPLICIT_HEAD:
			InsertHeadL();
			SWITCH_TO_AND_REPROCESS(IN_HEAD);

		case HTML5TreeState::WS_BEFORE_BODY:
			SetSourceCodePositionL();

			if (m_token.IsWSOnly(FALSE))
			{
				AddTextFromTokenL(m_text_accumulator, m_token.GetData(), 0, kAllText, FALSE);
				InsertTextL();
			}
			else
			{
				if (!m_token.SkippedSpaces())
				{
					unsigned leading = GetNumberOfLeadingSpaces(m_token);
					if (leading)
					{
						AddTextFromTokenL(m_text_accumulator, m_token.GetData(), 0, leading, FALSE);
						InsertTextL();
						m_token.SkipSpaces(leading);
					}
				}

				if (m_im == IN_HEAD)
					SetState(HTML5TreeState::IMPLICIT_END_HEAD);
				else
					SetState(HTML5TreeState::IMPLICIT_BODY);
				continue;
			}
			break;

		case HTML5TreeState::START_IN_HEAD:
			{
				GET_TYPE(type);
				switch (type)
				{
				case Markup::HTE_HTML:
					PROCESS_USING(IN_BODY);

				case Markup::HTE_BASE:
				case Markup::HTE_BASEFONT:
				case Markup::HTE_BGSOUND:
				case Markup::HTE_COMMAND:
				case Markup::HTE_LINK:
					InsertSelfclosingL();
					break;

				case Markup::HTE_META:
					InsertSelfclosingL();
					break;

				case Markup::HTE_TITLE:
					ProcessData(DATA_RCDATA);
					break;

				case Markup::HTE_NOSCRIPT:
					if (IsScriptingEnabled())
					{
						ProcessData(DATA_RAWTEXT);
					}
					else
					{
						InsertStartTagL(m_token);
						SetInsertionMode(IN_HEAD_NOSCRIPT);
					}
					break;

				case Markup::HTE_NOFRAMES:
				case Markup::HTE_STYLE:
					ProcessData(DATA_RAWTEXT);
					break;

				case Markup::HTE_SCRIPT:
					// The fragment case is handled by not LEAVing when the end tag is found
					ProcessData(DATA_SCRIPT);
					break;

				case Markup::HTE_HEAD:
					SetState(HTML5TreeState::IGNORE_TOKEN_ERR);
					break;

				default:
					SetState(HTML5TreeState::IMPLICIT_END_HEAD);
					continue;
				}
			}
			break;

		case HTML5TreeState::END_IN_HEAD:
			{
				GET_TYPE(type);
				if (type == Markup::HTE_HEAD)
				{
					PopCurrentElement();
					SetInsertionMode(AFTER_HEAD);
					break;
				}
				else if (type == Markup::HTE_BODY
					|| type == Markup::HTE_HTML
					|| type == Markup::HTE_BR)
				{
					SetState(HTML5TreeState::IMPLICIT_END_HEAD);
				}
				else
					SetState(HTML5TreeState::IGNORE_TOKEN_ERR);
			}
			continue;

		case HTML5TreeState::IMPLICIT_END_HEAD:
			PopCurrentElement();
			SWITCH_TO_AND_REPROCESS(AFTER_HEAD);

		case HTML5TreeState::START_IN_NOSCRIPT:
			{
				GET_TYPE(type);
				if (type == Markup::HTE_HTML)
					PROCESS_USING(IN_BODY)
				else if (type == Markup::HTE_LINK
					|| type == Markup::HTE_META
					|| type == Markup::HTE_NOFRAMES
					|| type == Markup::HTE_STYLE)
				{
					PROCESS_USING(IN_HEAD);
				}
				else if (type == Markup::HTE_HEAD
					|| type == Markup::HTE_NOSCRIPT)
				{
					SetState(HTML5TreeState::IGNORE_TOKEN_ERR);
				}
				else
				{
					EMIT_ERROR(ILLEGAL_CONTENT);
					PopCurrentElement();
					SWITCH_TO_AND_REPROCESS(IN_HEAD);
				}
			}
			continue;

		case HTML5TreeState::END_IN_NOSCRIPT:
			{
				GET_TYPE(type);
				if (type == Markup::HTE_NOSCRIPT)
				{
					PopCurrentElement();
					SetInsertionMode(IN_HEAD);
					break;
				}
				else if (type == Markup::HTE_BR)
					SetState(HTML5TreeState::AS_IN_HEAD_ERR);
				else
					SetState(HTML5TreeState::IGNORE_TOKEN_ERR);
			}
			continue;

		case HTML5TreeState::WS_IN_NOSCRIPT:
			if (m_token.IsWSOnly(FALSE))
				PROCESS_USING(IN_HEAD)
			else
				SetState(HTML5TreeState::AS_IN_HEAD_ERR);
			continue;

		case HTML5TreeState::AS_IN_HEAD:
			PROCESS_USING(IN_HEAD);

		case HTML5TreeState::AS_IN_HEAD_ERR:
			EMIT_ERROR(GENERIC);
			PopCurrentElement();
			SWITCH_TO_AND_REPROCESS(IN_HEAD);

		case HTML5TreeState::START_AFTER_HEAD:
			{
				GET_TYPE(type);
				switch (type)
				{
				case Markup::HTE_HTML:
					PROCESS_USING(IN_BODY);

				case Markup::HTE_BODY:
					InsertStartTagL(m_token);
					m_body_elm.SetElm(GetCurrentNode());
					SetFramesetOk(FALSE);
					SetInsertionMode(IN_BODY);
					break;

				case Markup::HTE_FRAMESET:
					{
#ifdef DELAYED_SCRIPT_EXECUTION
						HLDocProfile* hld_profile = GetHLDocProfile(); 
						if (hld_profile->ESIsExecutingDelayedScript())
						{
							/* If we've inserted a body element while parsing ahead 
							 * we need to reset the status that says this isn't
							 * a frames document.	 */
							HTML5ELEMENT* body = hld_profile->GetBodyElm();
							if (body && body->GetInserted() == HE_INSERTED_BY_PARSE_AHEAD)
								hld_profile->GetFramesDocument()->CheckInternalReset();
						}
#endif // DELAYED_SCRIPT_EXECUTION
						InsertStartTagL(m_token);
						SetInsertionMode(IN_FRAMESET);
						break;
					}

				case Markup::HTE_BASE:
				case Markup::HTE_LINK:
				case Markup::HTE_META:
				case Markup::HTE_NOFRAMES:
				case Markup::HTE_SCRIPT:
				case Markup::HTE_STYLE:
				case Markup::HTE_TITLE:
					EMIT_ERROR(ILLEGAL_STARTTAG);
					PushOpenElementL(m_head_elm.GetElm(), NULL);
					m_pending_state = HTML5TreeState::REMOVE_REOPENED_HEAD;
					PROCESS_USING(IN_HEAD);

				case Markup::HTE_HEAD:
					SetState(HTML5TreeState::IGNORE_TOKEN_ERR);
					continue;

				default:
					SetState(HTML5TreeState::IMPLICIT_BODY);
					continue;
				}
			}
			break;

		case HTML5TreeState::REMOVE_REOPENED_HEAD:
			RemoveFromOpenElements(m_head_elm.GetElm());
			break;

		case HTML5TreeState::END_AFTER_HEAD:
			{
				GET_TYPE(type);
				if (type == Markup::HTE_BODY
					|| type == Markup::HTE_HTML
					|| type == Markup::HTE_BR)
				{
					SetState(HTML5TreeState::IMPLICIT_BODY);
				}
				else
					SetState(HTML5TreeState::IGNORE_TOKEN_ERR);
			}
			continue;

		case HTML5TreeState::IMPLICIT_BODY:
			InsertBodyL();
			SetFramesetOk(TRUE);
			SWITCH_TO_AND_REPROCESS(IN_BODY);

		case HTML5TreeState::TEXT_IN_BODY:
			ReconstructActiveFormattingElements();
			SetSourceCodePositionL();
			AddTextFromTokenL(m_text_accumulator, m_token.GetData(), m_token.SkippedSpaces(), kAllText, FALSE);
			if (!m_token.IsWSOnly(TRUE))
				SetFramesetOk(FALSE);
			break;

		case HTML5TreeState::START_IN_BODY:
			{
				GET_TYPE(type);
				switch (type)
				{
				case Markup::HTE_HTML:
					EMIT_ERROR(STRAY_ELEMENT);
					CopyMissingAttributesL(m_html_elm.GetElm());
					break;

				case Markup::HTE_BASE:
				case Markup::HTE_BASEFONT:
				case Markup::HTE_BGSOUND:
				case Markup::HTE_COMMAND:
				case Markup::HTE_LINK:
				case Markup::HTE_META:
				case Markup::HTE_NOFRAMES:
				case Markup::HTE_SCRIPT:
				case Markup::HTE_STYLE:
				case Markup::HTE_TITLE:
					PROCESS_USING(IN_HEAD);

				case Markup::HTE_BODY:
					{
						EMIT_ERROR(STRAY_ELEMENT);
						HTML5OpenElement *node = m_open_elements.First();
						if (!node->Suc()
							|| node->Suc()->GetElementType() != Markup::HTE_BODY)
						{
							OP_ASSERT(IsFragment());
						}
						else
						{
							SetFramesetOk(FALSE);
							CopyMissingAttributesL(m_body_elm.GetElm());
						}
					}
					break;

				case Markup::HTE_FRAMESET:
					{
						EMIT_ERROR(ILLEGAL_FRAMESET);
						HTML5OpenElement *node = m_open_elements.First();
						HTML5OpenElement *body_node = node->Suc();
						if (!body_node || body_node->GetElementType() != Markup::HTE_BODY)
						{
							OP_ASSERT(IsFragment());
						}
						else if (IsFramesetOk())
						{
#ifdef DELAYED_SCRIPT_EXECUTION
							/* The HTML 5 parser when encountering a frameset element will remove any
							   implicitly body elements and any elements under it.  However, with DSE
 	                           any scripts there will expect that part of the tree to exist when they
 	                           run.
 	                           Recover then in order to turn DSE (parsing ahead) off. Hopefully this 
 	                           should be a rare case. */
							HLDocProfile* hld_profile = GetHLDocProfile();
							if (hld_profile->ESIsParsingAhead() || hld_profile->ESIsExecutingDelayedScript())
							{
								hld_profile->ESSetNeedRecover();

								// No point in parsing the rest of the document, we will be recovering in any case
								SetState(HTML5TreeState::STOP);
								continue;
							}
#endif // DELAYED_SCRIPT_EXECUTION

							HTML_Element* body_elm = body_node->GetNode();
							FramesDocument *frm_doc = m_parser->GetLogicalDocument()->GetFramesDocument();
							body_elm->OutSafe(frm_doc);
							PopUpToIncludingNode(body_elm);
							InsertStartTagL(m_token);
							SetInsertionMode(IN_FRAMESET);
						}
					}
					break;

				case Markup::HTE_ADDRESS:
				case Markup::HTE_ARTICLE:
				case Markup::HTE_ASIDE:
				case Markup::HTE_BLOCKQUOTE:
				case Markup::HTE_CENTER:
				case Markup::HTE_DETAILS:
				case Markup::HTE_DIR:
				case Markup::HTE_DIV:
				case Markup::HTE_DL:
				case Markup::HTE_FIELDSET:
				case Markup::HTE_FIGCAPTION:
				case Markup::HTE_FIGURE:
				case Markup::HTE_FOOTER:
				case Markup::HTE_HEADER:
				case Markup::HTE_HGROUP:
				case Markup::HTE_MENU:
				case Markup::HTE_NAV:
				case Markup::HTE_OL:
				case Markup::HTE_P:
				case Markup::HTE_SECTION:
				case Markup::HTE_SUMMARY:
				case Markup::HTE_UL:
					TERMINATE_IF_P_INSCOPE;
					InsertStartTagL(m_token);
					break;

				case Markup::HTE_H1:
				case Markup::HTE_H2:
				case Markup::HTE_H3:
				case Markup::HTE_H4:
				case Markup::HTE_H5:
				case Markup::HTE_H6:
					{
						TERMINATE_IF_P_INSCOPE;

						Markup::Type current_type = m_current_node->GetElementType();
						if (current_type == Markup::HTE_H1
							|| current_type == Markup::HTE_H2
							|| current_type == Markup::HTE_H3
							|| current_type == Markup::HTE_H4
							|| current_type == Markup::HTE_H5
							|| current_type == Markup::HTE_H6)
						{
							EMIT_ERROR(ILLEGAL_NESTING);
							PopCurrentElement();
						}

						InsertStartTagL(m_token);
					}
					break;

				case Markup::HTE_PRE:
				case Markup::HTE_LISTING:
					TERMINATE_IF_P_INSCOPE;
					InsertStartTagL(m_token);
					SetIsSkippingFirstNewline(TRUE);
					SetFramesetOk(FALSE);
					if (m_parser->IsPlaintext())
						m_tokenizer->SetState(HTML5Tokenizer::PLAINTEXT);
					break;

				case Markup::HTE_FORM:
					if (GetFormElement())
						EMIT_ERROR(ILLEGAL_NESTING);
					else
					{
						TERMINATE_IF_P_INSCOPE;
						InsertStartTagL(m_token);
						SetFormElement(GetCurrentNode());
					}
					break;

				case Markup::HTE_DD:
				case Markup::HTE_DT:
				case Markup::HTE_LI:
					HandleListElementL(type);
					break;

				case Markup::HTE_PLAINTEXT:
					TERMINATE_IF_P_INSCOPE;
					InsertStartTagL(m_token);
					m_tokenizer->SetState(HTML5Tokenizer::PLAINTEXT);
					break;

				case Markup::HTE_A:
					if (HasActiveFormattingElement(Markup::HTE_A, TRUE))
					{
						EMIT_ERROR(ILLEGAL_NESTING);
						HandleAdoptionL(Markup::HTE_A);
						if (HasElementTypeInScope(Markup::HTE_A, SCOPE_ALL))
						{
							HTML5ActiveElement *active_node = m_active_formatting_elements.Last();
							while (active_node && !active_node->HasFollowingMarker())
							{
								if (active_node->GetElementType() == Markup::HTE_A)
								{
									RemoveFromOpenElements(active_node->GetNode());
									DeleteActiveElement(active_node);
									break;
								}
								active_node = active_node->Pred();
							}
						}
					}

					ReconstructActiveFormattingElements();
					InsertStartTagL(m_token);
					AddFormattingElementL(FALSE);
					break;

				case Markup::HTE_B:
				case Markup::HTE_BIG:
				case Markup::HTE_CODE:
				case Markup::HTE_EM:
				case Markup::HTE_FONT:
				case Markup::HTE_I:
				case Markup::HTE_S:
				case Markup::HTE_SMALL:
				case Markup::HTE_STRIKE:
				case Markup::HTE_STRONG:
				case Markup::HTE_TT:
				case Markup::HTE_U:
					ReconstructActiveFormattingElements();
					InsertStartTagL(m_token);
					AddFormattingElementL(FALSE);
					break;

				case Markup::HTE_NOBR:
					ReconstructActiveFormattingElements();
					if (HasElementTypeInScope(Markup::HTE_NOBR, SCOPE_NORMAL))
					{
						EMIT_ERROR(ILLEGAL_NESTING);
						HandleAdoptionL(Markup::HTE_NOBR);
						ReconstructActiveFormattingElements();
					}
					InsertStartTagL(m_token);
					AddFormattingElementL(FALSE);
					break;

				case Markup::HTE_BUTTON:
					if (HasElementTypeInScope(Markup::HTE_BUTTON, SCOPE_NORMAL))
					{
						EMIT_ERROR(ILLEGAL_NESTING);
						HandleSpecialEndTag(Markup::HTE_BUTTON, TRUE);
						continue;
					}
					else
					{
						ReconstructActiveFormattingElements();
						InsertStartTagL(m_token);
						SetFramesetOk(FALSE);
					}
					break;

				case Markup::HTE_APPLET:
				case Markup::HTE_MARQUEE:
				case Markup::HTE_OBJECT:
					ReconstructActiveFormattingElements();
					InsertStartTagL(m_token);
					AddFormattingElementL(TRUE);
					SetFramesetOk(FALSE);
					break;

				case Markup::HTE_TABLE:
					if (!IsInQuirksMode() && HasElementTypeInScope(Markup::HTE_P, SCOPE_BUTTON))
						HandleSpecialEndTag(Markup::HTE_P, FALSE);
					InsertStartTagL(m_token);
					SetFramesetOk(FALSE);
					SetInsertionMode(IN_TABLE);
					break;

				case Markup::HTE_AREA:
				case Markup::HTE_BR:
				case Markup::HTE_EMBED:
				case Markup::HTE_IMG:
				case Markup::HTE_INPUT:
				case Markup::HTE_KEYGEN:
				case Markup::HTE_WBR:
					ReconstructActiveFormattingElements();
					InsertSelfclosingL();
					if (type == Markup::HTE_INPUT)
					{
						unsigned attr_index;
						if (m_token.HasAttribute(UNI_L("type"), attr_index))
						{
							if (m_token.AttributeHasValue(attr_index, UNI_L("hidden")))
								break;
						}
					}
					SetFramesetOk(FALSE);
					break;

				case Markup::HTE_PARAM:
				case Markup::HTE_SOURCE:
				case Markup::HTE_TRACK:
					InsertSelfclosingL();
					break;

				case Markup::HTE_HR:
					TERMINATE_IF_P_INSCOPE;
					InsertSelfclosingL();
					SetFramesetOk(FALSE);
					break;

				case Markup::HTE_IMAGE:
					EMIT_ERROR(DEPRECATED);
					ReconstructActiveFormattingElements();
					m_token.SetNameL(UNI_L("img"), 3);
					m_token.SetElementType(Markup::HTE_IMG);
					InsertSelfclosingL();
					SetFramesetOk(FALSE);
					break;

				case Markup::HTE_ISINDEX:
					EMIT_ERROR(DEPRECATED);
					if (GetFormElement())
						break;
					else
						HandleIsindexL();
					break;

				case Markup::HTE_TEXTAREA:
					SetIsSkippingFirstNewline(TRUE);
					SetFramesetOk(FALSE);
					ProcessData(DATA_RCDATA);
					break;

				case Markup::HTE_XMP:
					TERMINATE_IF_P_INSCOPE;
					ReconstructActiveFormattingElements();
					SetFramesetOk(FALSE);
					ProcessData(DATA_RAWTEXT);
					break;

				case Markup::HTE_IFRAME:
					SetFramesetOk(FALSE);
					ProcessData(DATA_RAWTEXT);
					break;

				case Markup::HTE_NOSCRIPT:
					if (!IsScriptingEnabled())
						break;
					// fall through
				case Markup::HTE_NOEMBED:
					ProcessData(DATA_RAWTEXT);
					break;

				case Markup::HTE_SELECT:
					ReconstructActiveFormattingElements();
					InsertStartTagL(m_token);
					SetFramesetOk(FALSE);
					{
						InsertionMode im = GetInsertionMode();
						if (im == IN_TABLE
							|| im == IN_CAPTION
							|| im == IN_COLUMN_GROUP
							|| im == IN_TABLE_BODY
							|| im == IN_ROW
							|| im == IN_CELL)
						{
							SetInsertionMode(IN_SELECT_IN_TABLE);
						}
						else
							SetInsertionMode(IN_SELECT);
					}
					break;

				case Markup::HTE_OPTGROUP:
				case Markup::HTE_OPTION:
					if (m_current_node->GetElementType() == Markup::HTE_OPTION)
						HandleEndTagInBody(m_token.GetNameStr(), Markup::HTE_OPTION);
					ReconstructActiveFormattingElements();
					InsertStartTagL(m_token);
					break;

				case Markup::HTE_RP:
				case Markup::HTE_RT:
					if (HasElementTypeInScope(Markup::HTE_RUBY, SCOPE_NORMAL))
					{
						GenerateImpliedEndTags(Markup::HTE_DOC_ROOT, UNI_L(""));
						if (m_current_node->GetElementType() != Markup::HTE_RUBY)
						{
							EMIT_ERROR(ILLEGAL_CONTENT);
						}
					}
					InsertStartTagL(m_token);
					break;

				case Markup::HTE_MATH:
				case Markup::HTE_SVG:
					ReconstructActiveFormattingElements();
					InsertForeignL(type == Markup::HTE_SVG ? Markup::SVG : Markup::MATH);
					break;

				case Markup::HTE_CAPTION:
				case Markup::HTE_COL:
				case Markup::HTE_COLGROUP:
				case Markup::HTE_FRAME:
				case Markup::HTE_HEAD:
				case Markup::HTE_TBODY:
				case Markup::HTE_TD:
				case Markup::HTE_TFOOT:
				case Markup::HTE_TH:
				case Markup::HTE_THEAD:
				case Markup::HTE_TR:
					EMIT_ERROR(ILLEGAL_CONTENT);
					break;

				default:
					ReconstructActiveFormattingElements();
					InsertStartTagL(m_token);
					break;
				}
			}
			break;

		case HTML5TreeState::END_IN_BODY:
			{
				GET_TYPE(type);
				switch (type)
				{
				case Markup::HTE_BODY:
				case Markup::HTE_HTML:
					if (!HasElementTypeInScope(Markup::HTE_BODY, SCOPE_NORMAL))
						EMIT_ERROR(ILLEGAL_ENDTAG);
					else
					{
						CheckIllegalOpenAfterBody(FALSE);
						SetInsertionMode(AFTER_BODY);
						if (type == Markup::HTE_HTML)
						{
							SetNewState();
							continue;
						}
					}
					break;

				case Markup::HTE_ADDRESS:
				case Markup::HTE_ARTICLE:
				case Markup::HTE_ASIDE:
				case Markup::HTE_BLOCKQUOTE:
				case Markup::HTE_BUTTON:
				case Markup::HTE_CENTER:
				case Markup::HTE_DETAILS:
				case Markup::HTE_DIR:
				case Markup::HTE_DIV:
				case Markup::HTE_DL:
				case Markup::HTE_FIELDSET:
				case Markup::HTE_FIGCAPTION:
				case Markup::HTE_FIGURE:
				case Markup::HTE_FOOTER:
				case Markup::HTE_HEADER:
				case Markup::HTE_HGROUP:
				case Markup::HTE_LISTING:
				case Markup::HTE_MENU:
				case Markup::HTE_NAV:
				case Markup::HTE_OL:
				case Markup::HTE_PRE:
				case Markup::HTE_SECTION:
				case Markup::HTE_SUMMARY:
				case Markup::HTE_UL:
					if (!HasElementTypeInScope(type, SCOPE_NORMAL))
						EMIT_ERROR(ILLEGAL_ENDTAG);
					else
						HandleSpecialEndTag(type, TRUE);
					break;

				case Markup::HTE_FORM:
					{
						HTML5NODE *node = GetFormElement();
						SetFormElement(NULL);
						if (!node || !HasElementInScope(node, SCOPE_NORMAL))
							EMIT_ERROR(ILLEGAL_ENDTAG);
						else
						{
							GenerateImpliedEndTags(Markup::HTE_DOC_ROOT, UNI_L(""));
							if (GetCurrentNode() != node)
								EMIT_ERROR(ILLEGAL_ENDTAG);
							RemoveFromOpenElements(node);
						}
					}
					break;

				case Markup::HTE_P:
					if (!HasElementTypeInScope(Markup::HTE_P, SCOPE_BUTTON))
					{
						EMIT_ERROR(ILLEGAL_ENDTAG);
						InsertElementL(UNI_L("p"), Markup::HTE_P);
						continue;
					}
					else
						HandleSpecialEndTag(Markup::HTE_P, FALSE);
					break;

				case Markup::HTE_LI:
					if (!HasElementTypeInScope(type, SCOPE_LIST_ITEM))
						EMIT_ERROR(ILLEGAL_ENDTAG);
					else
						HandleSpecialEndTag(Markup::HTE_LI, FALSE);
					break;

				case Markup::HTE_DD:
				case Markup::HTE_DT:
					if (!HasElementTypeInScope(type, SCOPE_NORMAL))
						EMIT_ERROR(ILLEGAL_ENDTAG);
					else
						HandleSpecialEndTag(type, FALSE);
					break;

				case Markup::HTE_H1:
				case Markup::HTE_H2:
				case Markup::HTE_H3:
				case Markup::HTE_H4:
				case Markup::HTE_H5:
				case Markup::HTE_H6:
					{
						HTML5OpenElement *iter = m_open_elements.Last();
						while (iter)
						{
							Markup::Type iter_type = iter->GetElementType();
							if (iter_type == Markup::HTE_H1
								|| iter_type == Markup::HTE_H2
								|| iter_type == Markup::HTE_H3
								|| iter_type == Markup::HTE_H4
								|| iter_type == Markup::HTE_H5
								|| iter_type == Markup::HTE_H6)
							{
								break;
							}
							else if (IsEndingScope(iter_type, iter->GetNs(), SCOPE_NORMAL))
							{
								iter = NULL;
								break;
							}

							iter = iter->Pred();
						}

						if (!iter)
							EMIT_ERROR(ILLEGAL_ENDTAG);
						else
						{
							GenerateImpliedEndTags(Markup::HTE_DOC_ROOT, UNI_L(""));
							if (m_current_node->GetElementType() != type)
								EMIT_ERROR(ILLEGAL_ENDTAG);
							HTML5OpenElement *iter_node = m_current_node;
							while (iter_node
								&& iter_node->GetElementType() != Markup::HTE_H1
								&& iter_node->GetElementType() != Markup::HTE_H2
								&& iter_node->GetElementType() != Markup::HTE_H3
								&& iter_node->GetElementType() != Markup::HTE_H4
								&& iter_node->GetElementType() != Markup::HTE_H5
								&& iter_node->GetElementType() != Markup::HTE_H6)
							{
								PopCurrentElement();
								iter_node = m_current_node;
							}
							PopCurrentElement(); // pop the h* node as well
						}
					}
					break;

				case Markup::HTE_A:
				case Markup::HTE_B:
				case Markup::HTE_BIG:
				case Markup::HTE_CODE:
				case Markup::HTE_EM:
				case Markup::HTE_FONT:
				case Markup::HTE_I:
				case Markup::HTE_NOBR:
				case Markup::HTE_S:
				case Markup::HTE_SMALL:
				case Markup::HTE_STRIKE:
				case Markup::HTE_STRONG:
				case Markup::HTE_TT:
				case Markup::HTE_U:
					HandleAdoptionL(type);
					break;

				case Markup::HTE_APPLET:
				case Markup::HTE_MARQUEE:
				case Markup::HTE_OBJECT:
					if (!HasElementTypeInScope(type, SCOPE_NORMAL))
						EMIT_ERROR(ILLEGAL_ENDTAG);
					else
					{
						HandleSpecialEndTag(type, TRUE);
						ClearActiveFormattingElements();
					}
					break;

				case Markup::HTE_BR:
					EMIT_ERROR(ILLEGAL_ENDTAG);
					ReconstructActiveFormattingElements();
					InsertElementL(UNI_L("br"), Markup::HTE_BR);
					PopCurrentElement();
					m_token.AckSelfClosing();
					SetFramesetOk(FALSE);
					break;

				default:
					HandleEndTagInBody(m_token.GetNameStr(), type);
					break;
				}
			}
			break;

		case HTML5TreeState::EOF_IN_BODY:
			CheckIllegalOpenAfterBody(TRUE);
			SetState(HTML5TreeState::STOP);
			continue;

		case HTML5TreeState::HANDLE_TEXT:
			SetSourceCodePositionL();
			AddTextFromTokenL(m_text_accumulator, m_token.GetData(), m_token.SkippedSpaces(), kAllText, FALSE);
			break;

		case HTML5TreeState::EOF_IN_TEXT:
			EMIT_ERROR(UNEXPECTED_EOF);
#ifndef HTML5_STANDALONE
			if (m_current_node && m_current_node->GetElementType() == Markup::HTE_SCRIPT)
			{
				SetSourceCodePositionL();

				HLDocProfile *hld_profile = GetHLDocProfile();
				hld_profile->SetHandleScript(FALSE); // to avoid running script when ending element
				PopCurrentElement();
				hld_profile->SetHandleScript(TRUE);
			}
			else
#endif // HTML5_STANDALONE
				PopCurrentElement();
			RESTORE_ORIGINAL_AND_REPROCESS;

		case HTML5TreeState::END_TAG_IN_TEXT:
			{
				SetInsertionMode(m_original_im);

				GET_TYPE(type);
				if (type == Markup::HTE_SCRIPT)
				{
					m_tokenizer->PushInsertionPointL(NULL);
					m_script_nesting_level++;

					HTML5ELEMENT *popped_elm = GetCurrentNode();
					unsigned level_before_popping = m_script_nesting_level;
#ifndef HTML5_STANDALONE
					HLDocProfile *hld_profile = GetHLDocProfile();
					ES_LoadManager *loadman = hld_profile->GetESLoadManager();
					BOOL had_parser_blocking = loadman->HasParserBlockingScript();
#endif // HTML5_STANDALONE
					PopCurrentElement();

					if (level_before_popping == m_script_nesting_level) // Check that popping the element did not finish the script as a side-effect
					{
#ifdef HTML5_STANDALONE
						if (IsFragment())
							SignalScriptFinished(popped_elm);
						else
						{
							SignalScriptFinished(popped_elm);
							LEAVE(HTML5ParserStatus::EXECUTE_SCRIPT);
						}
#else // HTML5_STANDALONE
						if (IsFragment()
# ifdef DELAYED_SCRIPT_EXECUTION
							// If we're delaying scripts, just go on parsing
							|| hld_profile->ESDelayScripts()
# endif // DELAYED_SCRIPT_EXECUTION
							|| !loadman->HasScript(popped_elm))
						{
							SignalScriptFinished(popped_elm, loadman, FALSE);
						}
						else
						{
							BOOL has_parser_blocking = loadman->HasParserBlockingScript();
							ParserScriptElm *script_elm = InsertScriptElementL(popped_elm, !had_parser_blocking && has_parser_blocking);

							OP_ASSERT(loadman->HasScript(popped_elm));
							if (has_parser_blocking)
								SignalScriptFinished(popped_elm, loadman, FALSE);
							else
								script_elm->SetIsStarted();

							LEAVE(HTML5ParserStatus::EXECUTE_SCRIPT);
						}
#endif // HTML5_STANDALONE
					}
				}
				else
					PopCurrentElement();
			}
			break;

		case HTML5TreeState::START_IN_TABLE:
			{
				GET_TYPE(type);
				switch (type)
				{
				case Markup::HTE_CAPTION:
					ClearTableStack(STACK_TABLE);
					AddFormattingElementL(TRUE);
					InsertStartTagL(m_token);
					SetInsertionMode(IN_CAPTION);
					break;

				case Markup::HTE_COLGROUP:
					ClearTableStack(STACK_TABLE);
					InsertStartTagL(m_token);
					SetInsertionMode(IN_COLUMN_GROUP);
					break;

				case Markup::HTE_COL:
					ClearTableStack(STACK_TABLE);
					InsertElementL(UNI_L("colgroup"), Markup::HTE_COLGROUP);
					SWITCH_TO_AND_REPROCESS(IN_COLUMN_GROUP);

				case Markup::HTE_TBODY:
				case Markup::HTE_TFOOT:
				case Markup::HTE_THEAD:
					ClearTableStack(STACK_TABLE);
					InsertStartTagL(m_token);
					SetInsertionMode(IN_TABLE_BODY);
					break;

				case Markup::HTE_TD:
				case Markup::HTE_TH:
				case Markup::HTE_TR:
					ClearTableStack(STACK_TABLE);
					InsertElementL(UNI_L("tbody"), Markup::HTE_TBODY);
					SWITCH_TO_AND_REPROCESS(IN_TABLE_BODY);

				case Markup::HTE_TABLE:
					EMIT_ERROR(ILLEGAL_NESTING);
					if (!HasElementTypeInScope(Markup::HTE_TABLE, SCOPE_TABLE))
					{
						OP_ASSERT(IsFragment());
						EMIT_ERROR(ILLEGAL_ENDTAG);
					}
					else
					{
						PopUpToIncludingType(Markup::HTE_TABLE);
						ResetInsertionMode();
						SetNewState();
						continue;
					}
					break;

				case Markup::HTE_STYLE:
				case Markup::HTE_SCRIPT:
					PROCESS_USING(IN_HEAD);

				case Markup::HTE_INPUT:
					{
						HTML5ELEMENT *new_elm = CreateElementL(m_token);
						ANCHOR_PTR(HTML5ELEMENT, new_elm);
#ifdef HTML5_STANDALONE
						const uni_char *type = new_elm->GetAttribute(UNI_L("type"));
#else // HTML5_STANDALONE
						const uni_char *type = new_elm->GetAttrValue(Markup::HA_TYPE, NS_IDX_HTML);
#endif // HTML5_STANDALONE
						if (type && uni_stri_eq(UNI_L("hidden"), type))
						{
							EMIT_ERROR(ILLEGAL_CONTENT);
							InsertNodeL(new_elm);
							ANCHOR_PTR_RELEASE(new_elm);
						}
						else
						{
							SetIsFosterParenting(TRUE);
							PROCESS_USING(IN_BODY);
						}
					}
					break;

				case Markup::HTE_FORM:
					EMIT_ERROR(ILLEGAL_CONTENT);
					if (!GetFormElement())
					{
						InsertStartTagL(m_token);
						SetFormElement(GetCurrentNode());
						PopCurrentElement();
					}
					break;

				default:
					SetIsFosterParenting(TRUE);
					PROCESS_USING(IN_BODY);
				}
			}
			break;

		case HTML5TreeState::END_IN_TABLE:
			{
				GET_TYPE(type);
				switch (type)
				{
				case Markup::HTE_TABLE:
					if (!HasElementTypeInScope(Markup::HTE_TABLE, SCOPE_TABLE))
						EMIT_ERROR(ILLEGAL_ENDTAG);
					else
					{
						PopUpToIncludingType(Markup::HTE_TABLE);
						ResetInsertionMode();
					}
					break;

				case Markup::HTE_BODY:
				case Markup::HTE_CAPTION:
				case Markup::HTE_COL:
				case Markup::HTE_COLGROUP:
				case Markup::HTE_HTML:
				case Markup::HTE_TBODY:
				case Markup::HTE_TD:
				case Markup::HTE_TFOOT:
				case Markup::HTE_TH:
				case Markup::HTE_THEAD:
				case Markup::HTE_TR:
					EMIT_ERROR(ILLEGAL_ENDTAG);
					break;

				default:
					SetIsFosterParenting(TRUE);
					PROCESS_USING(IN_BODY);
				}
			}
			break;

		case HTML5TreeState::TEXT_IN_TABLE:
			{
				SetSourceCodePositionL();

				if (GetInsertionMode() != IN_TABLE_TEXT)
				{
					StoreOriginalIM();
					SetInsertionMode(IN_TABLE_TEXT);
				}

				AddTextFromTokenL(m_pending_table_text, m_token.GetData(), m_token.SkippedSpaces(), kAllText, FALSE);
				if (!m_token.IsWSOnly(FALSE))
					SetIsNotPendingWsOnly(TRUE);

				if (IsNotPendingWsOnly())
					InsertTableTextL();
			}
			break;

		case HTML5TreeState::EOF_IN_TABLE:
			if (m_current_node && GetCurrentNode() != m_html_elm.GetElm())
				EMIT_ERROR(UNEXPECTED_EOF);
			SetState(HTML5TreeState::STOP);
			continue;

		case HTML5TreeState::INSERT_TABLE_TEXT:
			InsertTableTextL();
			SetIsNotPendingWsOnly(FALSE);
			RESTORE_ORIGINAL_AND_REPROCESS;

		case HTML5TreeState::START_IN_CAPTION:
			{
				GET_TYPE(type);
				switch (type)
				{
				case Markup::HTE_CAPTION:
				case Markup::HTE_COL:
				case Markup::HTE_COLGROUP:
				case Markup::HTE_TBODY:
				case Markup::HTE_TD:
				case Markup::HTE_TFOOT:
				case Markup::HTE_TH:
				case Markup::HTE_THEAD:
				case Markup::HTE_TR:
					EMIT_ERROR(MALPLACED_TABLE_CONTENT);
					SetState(HTML5TreeState::TERMINATE_CAPTION);
					continue;

				default:
					PROCESS_USING(IN_BODY);
				}
			}
			break;

		case HTML5TreeState::END_IN_CAPTION:
			{
				GET_TYPE(type);
				switch (type)
				{
				case Markup::HTE_CAPTION:
					SetState(HTML5TreeState::TERMINATE_CAPTION);
					SetIsNotPendingReprocessing(TRUE);
					continue;

				case Markup::HTE_TABLE:
					EMIT_ERROR(ILLEGAL_ENDTAG);
					SetState(HTML5TreeState::TERMINATE_CAPTION);
					continue;

				case Markup::HTE_BODY:
				case Markup::HTE_COL:
				case Markup::HTE_COLGROUP:
				case Markup::HTE_HTML:
				case Markup::HTE_TBODY:
				case Markup::HTE_TD:
				case Markup::HTE_TFOOT:
				case Markup::HTE_TH:
				case Markup::HTE_THEAD:
				case Markup::HTE_TR:
					EMIT_ERROR(ILLEGAL_ENDTAG);
					break;

				default:
					PROCESS_USING(IN_BODY);
				}
			}
			break;

		case HTML5TreeState::TERMINATE_CAPTION:
			if (!HasElementTypeInScope(Markup::HTE_CAPTION, SCOPE_TABLE))
				EMIT_ERROR(ILLEGAL_ENDTAG);
			else
			{
				GenerateImpliedEndTags(Markup::HTE_DOC_ROOT, UNI_L(""));
				if (m_current_node->GetElementType() != Markup::HTE_CAPTION)
					EMIT_ERROR(ILLEGAL_ENDTAG);
				PopUpToIncludingType(Markup::HTE_CAPTION);
				ClearActiveFormattingElements();
				SetInsertionMode(IN_TABLE);
				if (IsNotPendingReprocessing())
				{
					SetIsNotPendingReprocessing(FALSE);
					break;
				}
				SetNewState();
				continue;
			}
			break;

		case HTML5TreeState::AS_IN_BODY:
			PROCESS_USING(IN_BODY);

		case HTML5TreeState::START_IN_COLGROUP:
			{
				GET_TYPE(type);
				if (type == Markup::HTE_HTML)
					PROCESS_USING(IN_BODY)
				else if (type == Markup::HTE_COL)
					InsertSelfclosingL();
				else
				{
					SetState(HTML5TreeState::TERMINATE_COLGROUP);
					continue;
				}
			}
			break;

		case HTML5TreeState::END_IN_COLGROUP:
			{
				GET_TYPE(type);
				if (type == Markup::HTE_COLGROUP)
				{
					SetState(HTML5TreeState::TERMINATE_COLGROUP);
					SetIsNotPendingReprocessing(TRUE);
					continue;
				}
				else if (type == Markup::HTE_COL)
					EMIT_ERROR(ILLEGAL_ENDTAG);
				else
				{
					SetState(HTML5TreeState::TERMINATE_COLGROUP);
					continue;
				}
			}
			break;

		case HTML5TreeState::SPACE_IN_COLGROUP:
			SetSourceCodePositionL();

			if (m_token.IsWSOnly(FALSE))
			{
				AddTextFromTokenL(m_text_accumulator, m_token.GetData(), 0, kAllText, FALSE);
				InsertTextL();
			}
			else
			{
				if (!m_token.SkippedSpaces())
				{
					unsigned leading = GetNumberOfLeadingSpaces(m_token);
					if (leading)
					{
						AddTextFromTokenL(m_text_accumulator, m_token.GetData(), 0, leading, FALSE);
						InsertTextL();
						m_token.SkipSpaces(leading);
					}
				}

				SetState(HTML5TreeState::TERMINATE_COLGROUP);
				continue;
			}
			break;

		case HTML5TreeState::EOF_IN_COLGROUP:
			if (m_current_node && GetCurrentNode() == m_html_elm.GetElm())
				SetState(HTML5TreeState::STOP);
			else
				SetState(HTML5TreeState::TERMINATE_COLGROUP);
			continue;

		case HTML5TreeState::TERMINATE_COLGROUP:
			if (m_current_node && GetCurrentNode() == m_html_elm.GetElm())
			{
				OP_ASSERT(IsFragment());
				EMIT_ERROR(ILLEGAL_ENDTAG);
			}
			else
			{
				PopCurrentElement();
				SetInsertionMode(IN_TABLE);
				if (IsNotPendingReprocessing())
				{
					SetIsNotPendingReprocessing(FALSE);
					break;
				}
				SetNewState();
				continue;
			}
			break;

		case HTML5TreeState::AS_IN_TABLE:
			PROCESS_USING(IN_TABLE);

		case HTML5TreeState::START_IN_TBODY:
			{
				GET_TYPE(type);
				switch (type)
				{
				case Markup::HTE_TR:
					ClearTableStack(STACK_TBODY);
					InsertStartTagL(m_token);
					SetInsertionMode(IN_ROW);
					break;

				case Markup::HTE_TH:
				case Markup::HTE_TD:
					EMIT_ERROR(ILLEGAL_CONTENT);
					ClearTableStack(STACK_TBODY);
					InsertElementL(UNI_L("tr"), Markup::HTE_TR);
					SWITCH_TO_AND_REPROCESS(IN_ROW);

				case Markup::HTE_CAPTION:
				case Markup::HTE_COL:
				case Markup::HTE_COLGROUP:
				case Markup::HTE_TBODY:
				case Markup::HTE_TFOOT:
				case Markup::HTE_THEAD:
					SetState(HTML5TreeState::TERMINATE_TBODY);
					continue;

				default:
					PROCESS_USING(IN_TABLE);
				}
			}
			break;

		case HTML5TreeState::END_IN_TBODY:
			{
				GET_TYPE(type);
				switch (type)
				{
				case Markup::HTE_TBODY:
				case Markup::HTE_TFOOT:
				case Markup::HTE_THEAD:
					if (!HasElementTypeInScope(type, SCOPE_TABLE))
						EMIT_ERROR(ILLEGAL_ENDTAG);
					else
					{
						ClearTableStack(STACK_TBODY);
						PopCurrentElement();
						SetInsertionMode(IN_TABLE);
					}
					break;

				case Markup::HTE_TABLE:
					SetState(HTML5TreeState::TERMINATE_TBODY);
					continue;

				case Markup::HTE_BODY:
				case Markup::HTE_CAPTION:
				case Markup::HTE_COL:
				case Markup::HTE_COLGROUP:
				case Markup::HTE_HTML:
				case Markup::HTE_TH:
				case Markup::HTE_TD:
				case Markup::HTE_TR:
					EMIT_ERROR(ILLEGAL_ENDTAG);
					break;

				default:
					PROCESS_USING(IN_TABLE);
				}
			}
			break;

		case HTML5TreeState::TERMINATE_TBODY:
			if (!HasElementTypeInScope(Markup::HTE_TBODY, SCOPE_TABLE)
				&& !HasElementTypeInScope(Markup::HTE_TFOOT, SCOPE_TABLE)
				&& !HasElementTypeInScope(Markup::HTE_THEAD, SCOPE_TABLE))
			{
				OP_ASSERT(IsFragment());
				EMIT_ERROR(ILLEGAL_CONTENT);
			}
			else
			{
				ClearTableStack(STACK_TBODY);
				PopCurrentElement();
				SWITCH_TO_AND_REPROCESS(IN_TABLE);
			}
			break;

		case HTML5TreeState::START_IN_ROW:
			{
				GET_TYPE(type);
				switch (type)
				{
				case Markup::HTE_TH:
				case Markup::HTE_TD:
					ClearTableStack(STACK_TR);
					InsertStartTagL(m_token);
					AddFormattingElementL(TRUE);
					SetInsertionMode(IN_CELL);
					break;

				case Markup::HTE_CAPTION:
				case Markup::HTE_COL:
				case Markup::HTE_COLGROUP:
				case Markup::HTE_TBODY:
				case Markup::HTE_TFOOT:
				case Markup::HTE_THEAD:
				case Markup::HTE_TR:
					SetState(HTML5TreeState::TERMINATE_ROW);
					continue;

				default:
					PROCESS_USING(IN_TABLE);
				}
			}
			break;

		case HTML5TreeState::END_IN_ROW:
			{
				GET_TYPE(type);
				switch (type)
				{
				case Markup::HTE_TR:
					SetIsNotPendingReprocessing(TRUE);
					// fall through
				case Markup::HTE_TABLE:
					SetState(HTML5TreeState::TERMINATE_ROW);
					continue;

				case Markup::HTE_TBODY:
				case Markup::HTE_TFOOT:
				case Markup::HTE_THEAD:
					if (!HasElementTypeInScope(type, SCOPE_TABLE))
						EMIT_ERROR(ILLEGAL_ENDTAG);
					else
					{
						SetState(HTML5TreeState::TERMINATE_ROW);
						continue;
					}
					break;

				case Markup::HTE_BODY:
				case Markup::HTE_CAPTION:
				case Markup::HTE_COL:
				case Markup::HTE_COLGROUP:
				case Markup::HTE_HTML:
				case Markup::HTE_TD:
				case Markup::HTE_TH:
					EMIT_ERROR(ILLEGAL_ENDTAG);
					break;

				default:
					PROCESS_USING(IN_TABLE);
				}
			}
			break;

		case HTML5TreeState::TERMINATE_ROW:
			if (!HasElementTypeInScope(Markup::HTE_TR, SCOPE_TABLE))
				EMIT_ERROR(ILLEGAL_ENDTAG);
			else
			{
				ClearTableStack(STACK_TR);
				PopCurrentElement();
				SetInsertionMode(IN_TABLE_BODY);
				if (IsNotPendingReprocessing())
				{
					SetIsNotPendingReprocessing(FALSE);
					break;
				}
				SetNewState();
				continue;
			}
			break;

		case HTML5TreeState::START_IN_CELL:
			{
				GET_TYPE(type);
				switch (type)
				{
				case Markup::HTE_CAPTION:
				case Markup::HTE_COL:
				case Markup::HTE_COLGROUP:
				case Markup::HTE_TBODY:
				case Markup::HTE_TD:
				case Markup::HTE_TFOOT:
				case Markup::HTE_TH:
				case Markup::HTE_THEAD:
				case Markup::HTE_TR:
					if (!HasElementTypeInScope(Markup::HTE_TD, SCOPE_TABLE)
						&& !HasElementTypeInScope(Markup::HTE_TH, SCOPE_TABLE))
					{
						OP_ASSERT(IsFragment());
						EMIT_ERROR(ILLEGAL_CONTENT);
					}
					else
					{
						CloseCell();
						SetNewState();
						continue;
					}
					break;

				default:
					PROCESS_USING(IN_BODY);
				}
			}
			break;

		case HTML5TreeState::END_IN_CELL:
			{
				GET_TYPE(type);
				switch (type)
				{
				case Markup::HTE_TD:
				case Markup::HTE_TH:
					if (!HasElementTypeInScope(type, SCOPE_TABLE))
						EMIT_ERROR(ILLEGAL_ENDTAG);
					else
						TerminateCell(type);
					break;

				case Markup::HTE_BODY:
				case Markup::HTE_CAPTION:
				case Markup::HTE_COL:
				case Markup::HTE_COLGROUP:
				case Markup::HTE_HTML:
					EMIT_ERROR(ILLEGAL_ENDTAG);
					break;

				case Markup::HTE_TABLE:
				case Markup::HTE_TBODY:
				case Markup::HTE_TFOOT:
				case Markup::HTE_THEAD:
				case Markup::HTE_TR:
					if (!HasElementTypeInScope(type, SCOPE_TABLE))
						EMIT_ERROR(ILLEGAL_ENDTAG);
					else
					{
						CloseCell();
						SetNewState();
						continue;
					}
					break;

				default:
					PROCESS_USING(IN_BODY);
				}
			}
			break;

		case HTML5TreeState::START_IN_SELECT:
			{
				GET_TYPE(type);
				switch (type)
				{
				case Markup::HTE_HTML:
					PROCESS_USING(IN_BODY);

				case Markup::HTE_OPTION:
					if (m_current_node->GetElementType() == Markup::HTE_OPTION)
						PopCurrentElement();
					InsertStartTagL(m_token);
					break;

				case Markup::HTE_OPTGROUP:
					if (m_current_node->GetElementType() == Markup::HTE_OPTION)
						PopCurrentElement();
					if (m_current_node->GetElementType() == Markup::HTE_OPTGROUP)
						PopCurrentElement();
					InsertStartTagL(m_token);
					break;

				case Markup::HTE_SELECT:
					EMIT_ERROR(ILLEGAL_NESTING);
					SetIsNotPendingReprocessing(TRUE);
					SetState(HTML5TreeState::TERMINATE_SELECT);
					continue;

				case Markup::HTE_INPUT:
				case Markup::HTE_KEYGEN:
				case Markup::HTE_TEXTAREA:
					if (!HasElementTypeInScope(Markup::HTE_SELECT, SCOPE_SELECT))
					{
						OP_ASSERT(IsFragment());
						EMIT_ERROR(ILLEGAL_CONTENT);
					}
					else
					{
						EMIT_ERROR(ILLEGAL_CONTENT_CLOSING);
						PopUpToIncludingType(Markup::HTE_SELECT);
						ResetInsertionMode();
						SetNewState();
						continue;
					}

					break;

				case Markup::HTE_SCRIPT:
					PROCESS_USING(IN_HEAD);

				default:
					EMIT_ERROR(ILLEGAL_CONTENT);
					break;
				}
			}
			break;

		case HTML5TreeState::END_IN_SELECT:
			{
				GET_TYPE(type);
				switch (type)
				{
				case Markup::HTE_OPTGROUP:
					{
						HTML5OpenElement *current = m_open_elements.Last();
						if (current->GetElementType() == Markup::HTE_OPTION
							&& current->Pred() && current->Pred()->GetElementType() == Markup::HTE_OPTGROUP)
						{
							PopCurrentElement();
						}
						if (m_current_node->GetElementType() == Markup::HTE_OPTGROUP)
							PopCurrentElement();
						else
							EMIT_ERROR(ILLEGAL_ENDTAG);
					}
					break;

				case Markup::HTE_OPTION:
					if (m_current_node->GetElementType() == Markup::HTE_OPTION)
						PopCurrentElement();
					else
						EMIT_ERROR(ILLEGAL_ENDTAG);
					break;

				case Markup::HTE_SELECT:
					SetIsNotPendingReprocessing(TRUE);
					SetState(HTML5TreeState::TERMINATE_SELECT);
					continue;

				default:
					EMIT_ERROR(ILLEGAL_ENDTAG);
					break;
				}
			}
			break;

		case HTML5TreeState::EARLY_EOF:
			if (m_tokenizer) // not set in case of HTML5TreeState::ABORT
			{
				OP_ASSERT(m_open_elements.Last());
				if (m_current_node && m_current_node->GetElementType() != Markup::HTE_HTML)
					EMIT_ERROR(UNEXPECTED_EOF);
				else
				{
					OP_ASSERT(IsFragment());
				}
			}
			SetState(HTML5TreeState::STOP);
			continue;

		case HTML5TreeState::TERMINATE_SELECT:
			if (!HasElementTypeInScope(Markup::HTE_SELECT, SCOPE_SELECT))
			{
				OP_ASSERT(IsFragment());
				EMIT_ERROR(ILLEGAL_ENDTAG);
			}
			else
			{
				PopUpToIncludingType(Markup::HTE_SELECT);
				ResetInsertionMode();
			}
			if (IsNotPendingReprocessing())
			{
				SetIsNotPendingReprocessing(FALSE);
				break;
			}
			SetNewState();
			continue;

		case HTML5TreeState::AS_IN_SELECT:
			PROCESS_USING(IN_SELECT);

		case HTML5TreeState::START_IN_SELECT_TABLE:
			{
				GET_TYPE(type);
				switch (type)
				{
				case Markup::HTE_CAPTION:
				case Markup::HTE_TABLE:
				case Markup::HTE_TBODY:
				case Markup::HTE_TFOOT:
				case Markup::HTE_THEAD:
				case Markup::HTE_TR:
				case Markup::HTE_TD:
				case Markup::HTE_TH:
					EMIT_ERROR(ILLEGAL_CONTENT);
					SetState(HTML5TreeState::TERMINATE_SELECT);
					continue;

				default:
					PROCESS_USING(IN_SELECT);
				}
			}
			break;

		case HTML5TreeState::END_IN_SELECT_TABLE:
			{
				GET_TYPE(type);
				switch (type)
				{
				case Markup::HTE_CAPTION:
				case Markup::HTE_TABLE:
				case Markup::HTE_TBODY:
				case Markup::HTE_TFOOT:
				case Markup::HTE_THEAD:
				case Markup::HTE_TR:
				case Markup::HTE_TD:
				case Markup::HTE_TH:
					EMIT_ERROR(ILLEGAL_ENDTAG);
					if (HasElementTypeInScope(type, SCOPE_TABLE))
					{
						SetState(HTML5TreeState::TERMINATE_SELECT);
						continue;
					}
					break;

				default:
					PROCESS_USING(IN_SELECT);
				}
			}
			break;

		case HTML5TreeState::START_IN_FOREIGN:
			{
				if (!m_current_node->IsHTMLIntegrationPoint())
				{
					BOOL is_any_other = FALSE;
					GET_TYPE(type);
					if (m_current_node->GetNs() == Markup::MATH)
					{
						is_any_other = (type == Markup::MATHE_MGLYPH
								|| type == Markup::MATHE_MALIGNMARK
								|| !IsMathMLTextIntegrationPoint(GetCurrentNode()))
							&& (type != Markup::HTE_SVG
								|| m_current_node->GetElementType() != Markup::MATHE_ANNOTATION_XML);
					}
					else if (m_current_node->GetNs() == Markup::SVG)
						is_any_other = TRUE;

					if (is_any_other)
					{
						switch (type)
						{
						case Markup::HTE_B:
						case Markup::HTE_BIG:
						case Markup::HTE_BLOCKQUOTE:
						case Markup::HTE_BODY:
						case Markup::HTE_BR:
						case Markup::HTE_CENTER:
						case Markup::HTE_CODE:
						case Markup::HTE_DD:
						case Markup::HTE_DIV:
						case Markup::HTE_DL:
						case Markup::HTE_DT:
						case Markup::HTE_EM:
						case Markup::HTE_EMBED:
						case Markup::HTE_H1:
						case Markup::HTE_H2:
						case Markup::HTE_H3:
						case Markup::HTE_H4:
						case Markup::HTE_H5:
						case Markup::HTE_H6:
						case Markup::HTE_HEAD:
						case Markup::HTE_HR:
						case Markup::HTE_I:
						case Markup::HTE_IMG:
						case Markup::HTE_LI:
						case Markup::HTE_LISTING:
						case Markup::HTE_MENU:
						case Markup::HTE_META:
						case Markup::HTE_NOBR:
						case Markup::HTE_OL:
						case Markup::HTE_P:
						case Markup::HTE_PRE:
						case Markup::HTE_RUBY:
						case Markup::HTE_S:
						case Markup::HTE_SMALL:
						case Markup::HTE_SPAN:
						case Markup::HTE_STRONG:
						case Markup::HTE_STRIKE:
						case Markup::HTE_SUB:
						case Markup::HTE_SUP:
						case Markup::HTE_TABLE:
						case Markup::HTE_TT:
						case Markup::HTE_U:
						case Markup::HTE_UL:
						case Markup::HTE_VAR:
							SetState(HTML5TreeState::TERMINATE_FOREIGN);
							continue;

						case Markup::HTE_FONT:
							{
								unsigned dummy;
								if (m_token.HasAttribute(UNI_L("color"), dummy)
									|| m_token.HasAttribute(UNI_L("face"), dummy)
									|| m_token.HasAttribute(UNI_L("size"), dummy))
								{
									SetState(HTML5TreeState::TERMINATE_FOREIGN);
									continue;
								}
								else
									is_any_other = TRUE;
							}
							break;
						}
					}

					if (is_any_other)
					{
						InsertForeignL(m_current_node->GetNs());
						break;
					}
				}
			}

			SetNewState();
			continue;

		case HTML5TreeState::END_IN_FOREIGN:
			{
				GET_TYPE(type);
				if (type == Markup::SVGE_SCRIPT && m_current_node->GetNs() == Markup::SVG && m_current_node->GetElementType() == Markup::HTE_SCRIPT)
				{
					m_tokenizer->PushInsertionPointL(NULL);
					m_script_nesting_level++;
					m_parser->SetIsPausing(TRUE);

					HTML5ELEMENT *popped_elm = GetCurrentNode();
					unsigned level_before_popping = m_script_nesting_level;
#ifndef HTML5_STANDALONE
					HLDocProfile *hld_profile = GetHLDocProfile();
					ES_LoadManager *loadman = hld_profile->GetESLoadManager();
					BOOL had_parser_blocking = loadman->HasParserBlockingScript();
#endif // HTML5_STANDALONE
					PopCurrentElement();

					if (level_before_popping == m_script_nesting_level) // Check that popping the element did not finish the script as a side-effect
					{
#ifdef HTML5_STANDALONE
						if (IsFragment())
							SignalScriptFinished(popped_elm);
						else
						{
							SignalScriptFinished(popped_elm);
							LEAVE(HTML5ParserStatus::EXECUTE_SCRIPT);
						}
#else // HTML5_STANDALONE
						if (IsFragment()
# ifdef DELAYED_SCRIPT_EXECUTION
							// If we're delaying scripts, just go on parsing
							|| hld_profile->ESDelayScripts()
# endif // DELAYED_SCRIPT_EXECUTION
							|| !loadman->HasScript(popped_elm))
						{
							SignalScriptFinished(popped_elm, loadman, FALSE);
						}
						else
						{
							BOOL has_parser_blocking = loadman->HasParserBlockingScript();
							ParserScriptElm *script_elm = InsertScriptElementL(popped_elm, !had_parser_blocking && has_parser_blocking);

							OP_ASSERT(loadman->HasScript(popped_elm));
							if (loadman->HasParserBlockingScript())
								SignalScriptFinished(popped_elm, loadman, FALSE);
							else
								script_elm->SetIsStarted();

							LEAVE(HTML5ParserStatus::EXECUTE_SCRIPT);
						}
#endif // HTML5_STANDALONE
					}
				}
				else
				{
					HTML5OpenElement *node = m_open_elements.Last();

					if (!node->GetNode()->IsNamed(m_token.GetNameStr(), type))
						EMIT_ERROR(ILLEGAL_ENDTAG);

					BOOL process_in_html = FALSE;
					while (node)
					{
						if (node->IsNamed(m_token.GetNameStr(), type))
						{
							PopUpToIncludingNode(node->GetNode());
							break;
						}
						node = node->Pred();

						if (node->GetNode()->GetNs() == Markup::HTML)
						{
							process_in_html = TRUE;
							break;
						}
					}

					if (process_in_html)
					{
						SetNewState();
						continue;
					}
				}
			}
			break;

		case HTML5TreeState::TEXT_IN_FOREIGN:
			SetSourceCodePositionL();

			if (!m_token.IsWSOnly(TRUE))
				SetFramesetOk(FALSE);
			AddTextFromTokenL(m_text_accumulator, m_token.GetData(), m_token.SkippedSpaces(), kAllText, TRUE);
			break;

		case HTML5TreeState::TERMINATE_FOREIGN:
			{
				if (m_token.GetType() == HTML5Token::ENDOFFILE)
					PROCESS_USING(IN_BODY)
				else
					EMIT_ERROR(ILLEGAL_CONTENT);

				while (m_current_node
					&& m_current_node->GetNs() != Markup::HTML
					&& !m_current_node->IsHTMLIntegrationPoint()
					&& !IsMathMLTextIntegrationPoint(GetCurrentNode()))
				{
					PopCurrentElement();
				}

				SetNewState();
			}
			continue;

		case HTML5TreeState::START_AFTER_BODY:
			{
				GET_TYPE(type);
				if (type == Markup::HTE_HTML)
					PROCESS_USING(IN_BODY)
				else
					SetState(HTML5TreeState::AFTER_BODY_ERR);
			}
			continue;

		case HTML5TreeState::END_AFTER_BODY:
			{
				GET_TYPE(type);
				if (type == Markup::HTE_HTML)
				{
					if (IsFragment())
						EMIT_ERROR(ILLEGAL_ENDTAG);
					else
						SetInsertionMode(AFTER_AFTER_BODY);
					break;
				}
				else
					SetState(HTML5TreeState::AFTER_BODY_ERR);
			}
			continue;

		case HTML5TreeState::WS_AFTER_BODY:
			if (m_token.IsWSOnly(FALSE))
				PROCESS_USING(IN_BODY)
			else
				SetState(HTML5TreeState::AFTER_BODY_ERR);
			continue;

		case HTML5TreeState::AFTER_BODY_ERR:
			EMIT_ERROR(GENERIC);
			SWITCH_TO_AND_REPROCESS(IN_BODY);

		case HTML5TreeState::START_IN_FRAMESET:
			{
				GET_TYPE(type);
				if (type == Markup::HTE_HTML)
					PROCESS_USING(IN_BODY)
				else if (type == Markup::HTE_FRAMESET)
					InsertStartTagL(m_token);
				else if (type == Markup::HTE_FRAME)
					InsertSelfclosingL();
				else if (type == Markup::HTE_NOFRAMES)
					PROCESS_USING(IN_HEAD)
				else
					EMIT_ERROR(ILLEGAL_CONTENT);
			}
			break;

		case HTML5TreeState::END_IN_FRAMESET:
			{
				GET_TYPE(type);
				if (type == Markup::HTE_FRAMESET)
				{
					if (GetCurrentNode() == m_html_elm.GetElm())
					{
						OP_ASSERT(IsFragment());
						EMIT_ERROR(ILLEGAL_ENDTAG);
					}
					else
					{
						PopCurrentElement();
						if (!IsFragment() && m_current_node->GetElementType() != Markup::HTE_FRAMESET)
							SetInsertionMode(AFTER_FRAMESET);
					}
				}
				else
					EMIT_ERROR(ILLEGAL_ENDTAG);
			}
			break;

		case HTML5TreeState::WS_OUT_OF_BODY:
			SetSourceCodePositionL();

			if (m_token.IsWSOnly(FALSE))
			{
				AddTextFromTokenL(m_text_accumulator, m_token.GetData(), 0, kAllText, FALSE);
				InsertTextL();
			}
			else
			{
				if (!m_token.SkippedSpaces())
				{
					unsigned leading = GetNumberOfLeadingSpaces(m_token);
					if (leading)
					{
						AddTextFromTokenL(m_text_accumulator, m_token.GetData(), 0, leading, FALSE);
						InsertTextL();
						m_token.SkipSpaces(leading);
					}
				}

				EMIT_ERROR(ILLEGAL_CONTENT);
			}
			break;

		case HTML5TreeState::START_AFTER_FRAMESET:
			{
				GET_TYPE(type);
				if (type == Markup::HTE_HTML)
					PROCESS_USING(IN_BODY)
				else if (type == Markup::HTE_NOFRAMES)
					PROCESS_USING(IN_HEAD)
				else
					EMIT_ERROR(ILLEGAL_CONTENT);
			}
			break;

		case HTML5TreeState::END_AFTER_FRAMESET:
			{
				GET_TYPE(type);
				if (type == Markup::HTE_HTML)
					SetInsertionMode(AFTER_AFTER_FRAMESET);
				else
					EMIT_ERROR(ILLEGAL_ENDTAG);
			}
			break;


		case HTML5TreeState::START_AFTER_AFTER_BODY:
			{
				GET_TYPE(type);
				if (type == Markup::HTE_HTML)
					PROCESS_USING(IN_BODY)
				else
					SetState(HTML5TreeState::AS_IN_BODY_ERR);
			}
			continue;

		case HTML5TreeState::AS_IN_BODY_ERR:
			EMIT_ERROR(GENERIC);
			PROCESS_USING(IN_BODY);

		case HTML5TreeState::START_AFTER_AFTER_FRAMSET:
			{
				GET_TYPE(type);
				if (type == Markup::HTE_HTML)
					PROCESS_USING(IN_BODY)
				else if (type == Markup::HTE_NOFRAMES)
					PROCESS_USING(IN_HEAD)
				else
					EMIT_ERROR(ILLEGAL_CONTENT);
			}
			break;

		case HTML5TreeState::EOF_AFTER_AFTER:
			SetState(HTML5TreeState::STOP);
			continue;

		case HTML5TreeState::ABORT:
			m_token.ResetWrapper();
			m_token.Reset(HTML5Token::ENDOFFILE);
			if (m_tokenizer)
				m_tokenizer->SetFinished();
			SetNewState();
			continue;

		case HTML5TreeState::STOP:
		case HTML5TreeState::ILLEGAL_STATE:
		default:
			OP_ASSERT(!"This should not happen.");
			SetState(HTML5TreeState::STOP);
			return;
		}

		if (m_pending_state != HTML5TreeState::ILLEGAL_STATE)
		{
			SetState(m_pending_state);
			m_pending_state = HTML5TreeState::ILLEGAL_STATE;
			continue;
		}

		if (IsFosterParenting())
			SetIsFosterParenting(FALSE);

		if (m_before_using_im != INITIAL)
			SetInsertionMode(m_before_using_im);

		if (m_token.IsSelfClosing() && !m_token.IsSelfClosingAcked())
			EMIT_ERROR(ILLEGAL_SELF_CLOSING);

		m_token.ResetWrapper();
		m_tokenizer->GetNextTokenL(m_token);

		if (m_token.GetType() != HTML5Token::CHARACTER
			|| m_text_accumulator.Length() >= SplitTextLen)
		{
			if (m_text_accumulator.Length() > 0)
				InsertTextL();
			else if (IsSkippingFirstNewline())
				SetIsSkippingFirstNewline(FALSE);
		}

		SetNextState();
	}

	while (m_current_node)
		PopCurrentElement();
	m_free_open_elements.Clear();

	HTML5ActiveElement *iter = m_active_formatting_elements.Last();
	while (iter)
	{
		DeleteActiveElement(iter);
		iter = m_active_formatting_elements.Last();
	}
	m_free_active_formatting_elements.Clear();

	if (IsFragment())
		// It is likely that there are more than one innerHTML on a page
		// so we want to keep the buffer here for a while to avoid
		// reallocation and performance hits.
		m_text_accumulator.ResetL();
	else
		m_text_accumulator.FreeBuffer();

	SetFormElement(NULL);
}

void HTML5TreeBuilder::BuildTreeFragmentL(HTML5ELEMENT *context)
{
	OP_PROBE7_L(OP_PROBE_HTML5TREEBUILDER_BUILDTREEFRAGMENTL);
	m_context_elm.SetElm(context);
	SetIsFragment(TRUE);
	m_tokenizer->SetUpdatePosition(FALSE);

#ifdef HTML5_STANDALONE
	HTML5ELEMENT new_html_elm(Markup::HTE_HTML, Markup::HTML);
	ANCHOR(HTML5ELEMENT, new_html_elm);
#else // HTML5_STANDALONE
	HTML5ELEMENT new_html_elm;
	HTML5ElementAnchor new_html_elm_anchor(this, &m_html_elm, &m_head_elm, &m_body_elm, &m_context_elm, &new_html_elm, m_root.GetNode(), GetFormElement());
	new_html_elm.ConstructL(m_parser->GetLogicalDocument(), Markup::HTE_HTML, Markup::HTML, NULL);
#endif // HTML5_STANDALONE

	InsertNodeL(&new_html_elm, FALSE);
	PushOpenElementL(&new_html_elm, NULL);
	m_html_elm.SetElm(&new_html_elm);

	ResetInsertionMode();

	HTML5ELEMENT *form_iter = context;
#ifdef HTML5_STANDALONE
	while (form_iter && (form_iter->Type() != Markup::HTE_FORM || form_iter->GetNs() != Markup::HTML))
		form_iter = form_iter->Parent();
#else // HTML5_STANDALONE
	while (form_iter && !form_iter->IsMatchingType(Markup::HTE_FORM, NS_HTML))
		form_iter = form_iter->Parent();
	HTML5ELEMENT *old_form = GetHLDocProfile()->GetCurrentForm();
#endif // HTML5_STANDALONE
	SetFormElement(form_iter); // to update HLDocProfile

	HTML5Tokenizer::State tokenizer_state = HTML5Tokenizer::DATA;
	switch (m_context_elm->Type())
	{
	case Markup::HTE_TITLE:
	case Markup::HTE_TEXTAREA:
		tokenizer_state = HTML5Tokenizer::RCDATA;
		break;

	case Markup::HTE_STYLE:
	case Markup::HTE_XMP:
	case Markup::HTE_IFRAME:
	case Markup::HTE_NOEMBED:
	case Markup::HTE_NOFRAMES:
		tokenizer_state = HTML5Tokenizer::RAWTEXT;
		break;

	case Markup::HTE_SCRIPT:
		tokenizer_state = HTML5Tokenizer::SCRIPT_DATA;
		break;

	case Markup::HTE_NOSCRIPT:
		if (IsScriptingEnabled())
			tokenizer_state = HTML5Tokenizer::RAWTEXT;
		break;

	case Markup::HTE_PLAINTEXT:
		tokenizer_state = HTML5Tokenizer::PLAINTEXT;
		break;
	}

	m_tokenizer->SetState(tokenizer_state);

	BuildTreeL();

	HTML5NODE *child = new_html_elm.FirstChild();
	while (child)
	{
		HTML5NODE *next_child = child->Suc();
		child->Out();
		child->Under(m_root.GetNode());
		child = next_child;
	}
	new_html_elm.Out(); // remove from root

	m_context_elm.Reset();
	m_html_elm.Reset();

#ifdef HTML5_STANDALONE
	m_form_elm.Reset();
#else // HTML5_STANDALONE
	// Reset it, so it doesn't leak into the main document if changed by this parser instance.
	SetFormElement(old_form);
#endif // HTML5_STANDALONE
}

void HTML5TreeBuilder::StopBuildingL(BOOL abort)
{
	SetState(HTML5TreeState::ABORT);

	if (m_tokenizer)
		m_tokenizer->SetFinished();

	if (abort)
	{
		m_current_node = NULL;
		m_current_insertion_node = NULL;

		m_context_elm.Reset();
		m_html_elm.Reset();
		m_head_elm.Reset();
		m_body_elm.Reset();
		SetFormElement(NULL);

		ClearElementLists();

		m_text_accumulator.ResetL();
	}
	else
		BuildTreeL();
}

#ifdef HTML5_STANDALONE

BOOL HTML5TreeBuilder::SignalScriptFinished(HTML5ELEMENT *script_element)
{
	OP_ASSERT(m_script_nesting_level > 0);
	m_script_nesting_level--;

	if (m_script_nesting_level == 0)
		m_parser->SetIsPausing(FALSE);

	m_tokenizer->PopInsertionPointL();

	return TRUE;
}

#else // HTML5_STANDALONE

BOOL HTML5TreeBuilder::RemoveScriptElement(HTML5ELEMENT *script_element, BOOL is_finished)
{
	ParserScriptElm *parser_script_elm = GetParserScriptElm(script_element);
	if (parser_script_elm)
	{
		BOOL was_started = parser_script_elm->IsStarted();
		if (was_started)
		{
			if (!is_finished)
				return TRUE;
		}
		else if (parser_script_elm->IsBlocking())
			m_parser->SetIsBlocking(FALSE);

		parser_script_elm->Out();
		OP_DELETE(parser_script_elm);
		return was_started;
	}
	else
		return FALSE;
}

OP_STATUS HTML5TreeBuilder::ExternalScriptReady(HTML5ELEMENT *script_element)
{
	if (script_element->GetInserted() == HE_NOT_INSERTED)
	{
		ParserScriptElm *blocking_elm = GetParserBlockingScript();
		if (blocking_elm && blocking_elm->GetElm() == script_element)
		{
			blocking_elm->SetIsReady();
			TRAPD(oom, StartParserBlockingScriptL(blocking_elm));
			if (OpStatus::IsMemoryError(oom))
				return oom;
		}
		else
		{
#ifdef DELAYED_SCRIPT_EXECUTION
			OP_ASSERT(!m_tokenizer || GetHLDocProfile()->ESIsExecutingDelayedScript() || !"Weird external elm");
#else // DELAYED_SCRIPT_EXECUTION
			OP_ASSERT(!m_tokenizer || !"Weird external elm");
#endif // DELAYED_SCRIPT_EXECUTION
		}
	}

	return OpStatus::OK;
}

OP_STATUS HTML5TreeBuilder::AddBlockingScript(HTML5ELEMENT *script_element)
{
	OP_STATUS oom = OpStatus::OK;
	if (m_tokenizer)
		TRAP(oom, InsertScriptElementL(script_element, TRUE));
	return oom;
}

BOOL HTML5TreeBuilder::SignalScriptFinished(HTML5ELEMENT *script_element, ES_LoadManager *loadman, BOOL remove_element)
{
	if (!script_element)
		return FALSE;

	HE_InsertType insert_type = script_element->GetInserted();
	if (insert_type != HE_NOT_INSERTED && insert_type != HE_INSERTED_BY_PARSE_AHEAD)
	{
		if (!GetParserScriptElm(script_element))
			return FALSE;
	}

# ifdef DELAYED_SCRIPT_EXECUTION
	HLDocProfile* hld_profile = GetHLDocProfile();
	if (hld_profile->ESIsExecutingDelayedScript())
	{
		hld_profile->ESMarkLastDelayedScriptCompleted();

		if (!m_tokenizer)  // A delayed script finishes after we're done parsing.  Nothing to do here.
		{
			OP_ASSERT(m_script_nesting_level == 0 || hld_profile->ESGetNeedRecover());  // If we're done parsing nesting level should be 0 or we're recovering
			return TRUE;
		}
	}
# endif // DELAYED_SCRIPT_EXECUTION

	BOOL started_by_parser = TRUE;
	if (remove_element)
		started_by_parser = RemoveScriptElement(script_element, TRUE);

	if (started_by_parser)
	{
		OP_ASSERT(m_script_nesting_level > 0);
		m_script_nesting_level--;

		if (m_script_nesting_level == 0)
			m_parser->SetIsPausing(FALSE);

		if (!m_tokenizer)
			// We have stopped loading, already reset the parser and deleted the
			// tokenizer.
			return TRUE;

		m_tokenizer->PopInsertionPointL();

		// we only care about the elements inserted by the parser
		if (insert_type == HE_INSERTED_BY_PARSE_AHEAD)
			return TRUE;

		OP_ASSERT(m_script_nesting_level == 0 || loadman->HasScripts());
		OP_ASSERT(m_script_nesting_level == m_tokenizer->GetIPStackDepth());
		ParserScriptElm *blocking_script = GetParserBlockingScript();
		if (blocking_script)
		{
			if (m_script_nesting_level > 0)
			{
				m_parser->SetIsPausing(TRUE);
				if (m_tokenizer->ResetEOBSignallingForNextBuffer())
					loadman->SignalWriteFinished(); // to avoid further blocking of the writing script
			}
			else
				StartParserBlockingScriptL(blocking_script);
		}
		else
			m_parser->SetIsPausing(FALSE);
	}

	return started_by_parser;
}

#endif // HTML5_STANDALONE

BOOL HTML5TreeBuilder::IsFinished()
{
	return !m_tokenizer || m_tokenizer->IsFinished();
}

void HTML5TreeBuilder::ResetToken()
{
	m_token.Reset(HTML5Token::INVALID);
}

void HTML5TreeBuilder::HTML5ActiveElement::InitL(HTML5TokenWrapper &token)
{
	m_token.CopyL(token);
}

void HTML5TreeBuilder::SetFramesetOk(BOOL ok)
{
	m_bools.frameset_ok = static_cast<unsigned>(ok);
#ifndef HTML5_STANDALONE
	if (!IsFragment())
		m_parser->GetLogicalDocument()->SetHasContent(!ok);
#endif // HTML5_STANDALONE
}

/*static*/ BOOL HTML5TreeBuilder::IsSpecial(HTML5ELEMENT *node)
{
	switch (node->Type())
	{
	case Markup::HTE_ADDRESS:
	case Markup::HTE_APPLET:
	case Markup::HTE_AREA:
	case Markup::HTE_ARTICLE:
	case Markup::HTE_ASIDE:
	case Markup::HTE_BASE:
	case Markup::HTE_BASEFONT:
	case Markup::HTE_BGSOUND:
	case Markup::HTE_BLOCKQUOTE:
	case Markup::HTE_BODY:
	case Markup::HTE_BR:
	case Markup::HTE_BUTTON:
	case Markup::HTE_CAPTION:
	case Markup::HTE_CENTER:
	case Markup::HTE_COL:
	case Markup::HTE_COLGROUP:
	case Markup::HTE_COMMAND:
	case Markup::HTE_DD:
	case Markup::HTE_DETAILS:
	case Markup::HTE_DIR:
	case Markup::HTE_DIV:
	case Markup::HTE_DL:
	case Markup::HTE_DT:
	case Markup::HTE_EMBED:
	case Markup::HTE_FIELDSET:
	case Markup::HTE_FIGCAPTION:
	case Markup::HTE_FIGURE:
	case Markup::HTE_FOOTER:
	case Markup::HTE_FORM:
	case Markup::HTE_FRAME:
	case Markup::HTE_FRAMESET:
	case Markup::HTE_H1:
	case Markup::HTE_H2:
	case Markup::HTE_H3:
	case Markup::HTE_H4:
	case Markup::HTE_H5:
	case Markup::HTE_H6:
	case Markup::HTE_HEAD:
	case Markup::HTE_HEADER:
	case Markup::HTE_HGROUP:
	case Markup::HTE_HR:
	case Markup::HTE_HTML:
	case Markup::HTE_IFRAME:
	case Markup::HTE_IMG:
	case Markup::HTE_INPUT:
	case Markup::HTE_ISINDEX:
	case Markup::HTE_LI:
	case Markup::HTE_LINK:
	case Markup::HTE_LISTING:
	case Markup::HTE_MARQUEE:
	case Markup::HTE_MENU:
	case Markup::HTE_META:
	case Markup::HTE_NAV:
	case Markup::HTE_NOEMBED:
	case Markup::HTE_NOFRAMES:
	case Markup::HTE_NOSCRIPT:
	case Markup::HTE_OBJECT:
	case Markup::HTE_OL:
	case Markup::HTE_P:
	case Markup::HTE_PARAM:
	case Markup::HTE_PLAINTEXT:
	case Markup::HTE_PRE:
	case Markup::HTE_SCRIPT:
	case Markup::HTE_SECTION:
	case Markup::HTE_SELECT:
	case Markup::HTE_STYLE:
	case Markup::HTE_SUMMARY:
	case Markup::HTE_TABLE:
	case Markup::HTE_TBODY:
	case Markup::HTE_TD:
	case Markup::HTE_TEXTAREA:
	case Markup::HTE_TFOOT:
	case Markup::HTE_TH:
	case Markup::HTE_THEAD:
	case Markup::HTE_TITLE:
	case Markup::HTE_TR:
	case Markup::HTE_UL:
	case Markup::HTE_WBR:
	case Markup::HTE_XMP:
		return 	node->GetNs() == Markup::HTML;
	}

	return FALSE;
}

/*static*/ BOOL HTML5TreeBuilder::IsFormatting(Markup::Type elm_type)
{
	switch (elm_type)
	{
	case Markup::HTE_A:
	case Markup::HTE_B:
	case Markup::HTE_BIG:
	case Markup::HTE_CODE:
	case Markup::HTE_EM:
	case Markup::HTE_FONT:
	case Markup::HTE_I:
	case Markup::HTE_NOBR:
	case Markup::HTE_S:
	case Markup::HTE_SMALL:
	case Markup::HTE_STRIKE:
	case Markup::HTE_STRONG:
	case Markup::HTE_TT:
	case Markup::HTE_U:
		return TRUE;
	}

	return FALSE;
}

/*static*/ BOOL HTML5TreeBuilder::IsAllowedToBeOpenAfterBody(Markup::Type elm_type, BOOL is_eof)
{
	switch (elm_type)
	{
	case Markup::HTE_DD:
	case Markup::HTE_DT:
	case Markup::HTE_LI:
	case Markup::HTE_P:
	case Markup::HTE_TBODY:
	case Markup::HTE_TD:
	case Markup::HTE_TFOOT:
	case Markup::HTE_TH:
	case Markup::HTE_THEAD:
	case Markup::HTE_TR:
		return TRUE;

	case Markup::HTE_OPTGROUP:
	case Markup::HTE_OPTION:
	case Markup::HTE_RP:
	case Markup::HTE_RT:
		return !is_eof;
	}

	return FALSE;
}

/*static*/ BOOL	HTML5TreeBuilder::IsMathMLTextIntegrationPoint(HTML5ELEMENT *elm)
{
	switch (elm->Type())
	{
	case Markup::MATHE_MI:
	case Markup::MATHE_MN:
	case Markup::MATHE_MO:
	case Markup::MATHE_MS:
	case Markup::MATHE_MTEXT:
		return elm->GetNs() == Markup::MATH;
	}

	return FALSE;
}

void HTML5TreeBuilder::HandleListElementL(Markup::Type type)
{
	BOOL is_list = type == Markup::HTE_LI;
	SetFramesetOk(FALSE);
	HTML5OpenElement *node = m_open_elements.Last();
	while (node)
	{
		Markup::Type elm_type = node->GetElementType();
		if (is_list && elm_type == Markup::HTE_LI
			|| !is_list && (elm_type == Markup::HTE_DD || elm_type == Markup::HTE_DT))
		{
			// Terminate the matched list element
			HandleSpecialEndTag(elm_type, FALSE);
			break;
		}

		if (IsSpecial(node->GetNode())
			&& elm_type != Markup::HTE_ADDRESS
			&& elm_type != Markup::HTE_DIV
			&& elm_type != Markup::HTE_P)
		{
			break;
		}

		node = node->Pred();
	}

	TERMINATE_IF_P_INSCOPE;

	InsertStartTagL(m_token);
}

void HTML5TreeBuilder::HandleSpecialEndTag(Markup::Type type, BOOL generate_endtag_for_self)
{
	OP_PROBE7(OP_PROBE_HTML5TREEBUILDER_HANDLESPECIALENDTAG);

	GenerateImpliedEndTags(generate_endtag_for_self ? Markup::HTE_DOC_ROOT : type, UNI_L(""));

	if (m_current_node->GetElementType() != type)
		EMIT_ERROR(ILLEGAL_ENDTAG);

	PopUpToIncludingType(type);
}

void HTML5TreeBuilder::HandleEndTagInBody(const uni_char *name, Markup::Type type)
{
	HTML5OpenElement *node = m_open_elements.Last();
	BOOL was_current_node = TRUE;
	while (node)
	{
		if (node->GetNode()->IsNamed(name, type))
		{
			GenerateImpliedEndTags(type, name);

			if (m_current_node->GetElementType() != type)
				EMIT_ERROR(ILLEGAL_ENDTAG);

			if (!was_current_node || node == m_open_elements.Last())
				PopUpToIncludingNode(node->GetNode());
			break;
		}
		else if (IsSpecial(node->GetNode()))
		{
			EMIT_ERROR(ILLEGAL_ENDTAG);
			break;
		}

		was_current_node = FALSE;
		node = node->Pred();
	}
}

void HTML5TreeBuilder::AddAttributeL(HTML5ELEMENT *elm, const uni_char *name, unsigned name_len, const uni_char *value, unsigned value_len)
{
#ifdef HTML5_STANDALONE
	elm->AddAttributeL(name, name_len, value, value_len);
#else // HTML5_STANDALONE
	HTML5TokenBuffer name_buffer(name_len + 1);
	ANCHOR(HTML5TokenBuffer, name_buffer);
	name_buffer.SetHashBase(HTML5_ATTR_HASH_BASE);
	name_buffer.AppendAndHashL(name, name_len);
	name_buffer.TerminateL();

	elm->AddAttributeL(m_parser->GetLogicalDocument(), &name_buffer, value, value_len);
#endif // HTML5_STANDALONE
}

void HTML5TreeBuilder::AddAttributeL(HTML5ELEMENT *elm, unsigned index)
{
#ifdef HTML5_STANDALONE
	elm->AddAttributeL(&m_token, index);
#else // HTML5_STANDALONE
	uni_char *attr_val;
	unsigned value_len;
	HTML5Attr *attr = m_token.GetAttribute(index);
	BOOL must_free = attr->GetValue().GetBufferL(attr_val, value_len, TRUE);
	ANCHOR_ARRAY(uni_char, attr_val);
	if (!must_free)
		ANCHOR_ARRAY_RELEASE(attr_val);

	elm->AddAttributeL(m_parser->GetLogicalDocument(), attr->GetName(), attr_val, value_len);
#endif // HTML5_STANDALONE
}

void HTML5TreeBuilder::HandleIsindexL()
{
	m_token.AckSelfClosing();
	TERMINATE_IF_P_INSCOPE;

	InsertElementL(UNI_L("form"), Markup::HTE_FORM);
	SetFormElement(GetCurrentNode());
	unsigned attr_idx;
	if (m_token.HasAttribute(UNI_L("action"), attr_idx))
		AddAttributeL(GetFormElement(), attr_idx);

	SetFramesetOk(FALSE);

	InsertElementL(UNI_L("hr"), Markup::HTE_HR);
	PopCurrentElement(); // hr

	ReconstructActiveFormattingElements();
	InsertElementL(UNI_L("label"), Markup::HTE_LABEL);

	if (m_token.HasAttribute(UNI_L("prompt"), attr_idx))
	{
		HTML5Attr *prompt = m_token.GetAttribute(attr_idx);
		AddTextFromTokenL(m_text_accumulator, prompt->GetValue(), 0, kAllText, FALSE);
	}
	else
	{
#ifdef HTML5_STANDALONE
		const uni_char *stream = UNI_L("This is a searchable index. Enter search keywords: ");
		m_text_accumulator.AppendL(stream, uni_strlen(stream));
#else // HTML5_STANDALONE
		OpString use_prompt;
		g_languageManager->GetStringL(Str::SI_ISINDEX_TEXT, use_prompt);

		m_text_accumulator.AppendL(use_prompt.CStr(), use_prompt.Length());
#endif // HTML5_STANDALONE
	}
	InsertTextL();

	ReconstructActiveFormattingElements();

	// the input element should have all the attributes of isindex, except action, name and prompt
	// so create a copy of the isindex token with those attributes removed, and use that
	// when creating the input element
	HTML5TokenWrapper input_token;
	input_token.CopyL(m_token);
	input_token.RemoveAttributeL(UNI_L("action"));
	input_token.RemoveAttributeL(UNI_L("name"));
	input_token.RemoveAttributeL(UNI_L("prompt"));

	HTML5ELEMENT *input_elm = CreateElementL(UNI_L("input"), Markup::HTE_INPUT, &input_token);
	RemoveNamedElementAnchor named_element_anchor(this, input_elm);
	ANCHOR(RemoveNamedElementAnchor, named_element_anchor);
	AddAttributeL(input_elm, UNI_L("name"), 4, UNI_L("isindex"), 7);

	InsertNodeL(input_elm);
	named_element_anchor.Release();
	PopCurrentElement(); // input

	HandleEndTagInBody(UNI_L("label"), Markup::HTE_LABEL);

	InsertElementL(UNI_L("hr"), Markup::HTE_HR);
	PopCurrentElement(); // hr

	SetFormElement(NULL);
	PopCurrentElement(); // form
}

void HTML5TreeBuilder::MoveNode(HTML5NODE *node, HTML5ELEMENT *new_parent)
{
#ifdef HTML5_STANDALONE
	if (node->Parent())
		node->Out();

	node->Under(new_parent);
#else // !HTML5_STANDALONE
	FramesDocument *frm_doc = m_parser->GetLogicalDocument()->GetFramesDocument();

	if (node->Parent())
		node->OutSafe(frm_doc, FALSE);

	node->UnderSafe(frm_doc, new_parent, TRUE);
#endif // HTML5_STANDALONE
}

#ifdef DELAYED_SCRIPT_EXECUTION
void HTML5TreeBuilder::RecordOldParentL(HTML5ELEMENT* elm, HTML5ELEMENT* parent)
{
	// If we're moving a node which was inserted before we started with parse ahead, and
	// now we're parsing ahead then we need to remember where its original position was
	// so that we can restore the position if we need to recover
	if (elm->GetInserted() != HE_INSERTED_BY_PARSE_AHEAD && GetHLDocProfile()->ESIsParsingAhead())
	{
		// We shouldn't be moved twice
		if (!elm->GetSpecialAttr(ATTR_DSE_RESTORE_PARENT, ITEM_TYPE_COMPLEX, (void*)NULL, SpecialNs::NS_LOGDOC))
		{
			// store original parent in a special attribute
			HTML5DSE_RestoreAttr* parent_attr = OP_NEW_L(HTML5DSE_RestoreAttr, (parent));
			int idx = elm->SetSpecialAttr(ATTR_DSE_RESTORE_PARENT, ITEM_TYPE_COMPLEX, (void*)parent_attr, TRUE, SpecialNs::NS_LOGDOC);
			if (idx == -1)
			{
				OP_DELETE(parent_attr);
				LEAVE(OpStatus::ERR_NO_MEMORY);
			}
		}
	}
}
#endif // DELAYED_SCRIPT_EXECUTION


BOOL HTML5TreeBuilder::HandleAdoptionL(Markup::Type type)
{
	if (HasReachedMaxTagNestingLevel())
		return TRUE;

#ifdef DELAYED_SCRIPT_EXECUTION
	BOOL is_parsing_ahead = GetHLDocProfile()->ESIsParsingAhead();
#endif // DELAYED_SCRIPT_EXECUTION

	unsigned outer_loop_counter = 0;
	HTML5ActiveElement *formatting_element = m_active_formatting_elements.Last();
	while (formatting_element && outer_loop_counter < 8)
	{
		outer_loop_counter++;

		while (formatting_element)
		{
			if (formatting_element->HasFollowingMarker() || formatting_element->GetElementType() == type)
				break;
			formatting_element = formatting_element->Pred();
		}

		if (!formatting_element || formatting_element->GetElementType() != type || formatting_element->HasFollowingMarker())
		{
			EMIT_ERROR(ILLEGAL_ENDTAG);
			return FALSE;
		}
		else
		{
			HTML5ELEMENT *formatting_node = formatting_element->GetNode();
			BOOL is_in_open_elms = HasElementInScope(formatting_node, SCOPE_ALL);

			if (!is_in_open_elms)
			{
				DeleteActiveElement(formatting_element);
				return TRUE;
			}
			else if (!HasElementInScope(formatting_node, SCOPE_NORMAL))
			{
				EMIT_ERROR(ILLEGAL_ENDTAG);
				return FALSE;
			}

			if (formatting_node != GetCurrentNode())
				EMIT_ERROR(ILLEGAL_ENDTAG);

			// Restructure tree
			HTML5OpenElement *open_iter = GetStackEntry(formatting_node);
			HTML5OpenElement *formatting_stack_entry = open_iter;
			// Get the one after the formatting entry
			open_iter = open_iter->Suc();

			// Find the topmost that is not phrasing or formatting
			while (open_iter && !IsSpecial(open_iter->GetNode()))
				open_iter = open_iter->Suc();

			if (!open_iter)
			{
				PopUpToIncludingNode(formatting_node);
				DeleteActiveElement(formatting_element);
				return TRUE;
			}

			HTML5OpenElement *furthest_block = open_iter;
			HTML5OpenElement *common_ancestor = formatting_stack_entry->Pred();
			ActiveListBookmark bookmark(&m_active_formatting_elements, formatting_element);
			HTML5OpenElement *node = furthest_block->Pred(), *last_node = furthest_block;
			unsigned inner_loop_counter = 0;

			while (node && inner_loop_counter < 3)
			{
				inner_loop_counter++;

				HTML5OpenElement *next_node = node->Pred();
				HTML5ActiveElement *list_entry = GetFormattingListEntry(node->GetNode());
				if (!list_entry)
				{
					DeleteOpenElement(node);
					node = next_node;
					continue;
				}
				else if (list_entry == formatting_element)
					break;
				else if (last_node == furthest_block)
					bookmark.SetBookmark(list_entry->Suc());

				HTML5ELEMENT *new_elm = CreateElementL(list_entry->GetToken());
				list_entry->SetNode(new_elm); // replace old formatting element with the new
				node->SetNode(new_elm);

				HTML5ELEMENT *last_elm = last_node->GetNode();
#ifdef DELAYED_SCRIPT_EXECUTION
				if (is_parsing_ahead)
				{
					new_elm->SetInserted(HE_INSERTED_BY_PARSE_AHEAD);
					RecordOldParentL(last_elm, last_elm->Parent());
				}
#endif // DELAYED_SCRIPT_EXECUTION

				MoveNode(last_elm, new_elm);

				last_node = node;
				node = next_node;
			}

			Markup::Type common_ancestor_type = common_ancestor->GetElementType();
			if (common_ancestor_type == Markup::HTE_TABLE
				|| common_ancestor_type == Markup::HTE_TBODY
				|| common_ancestor_type == Markup::HTE_TFOOT
				|| common_ancestor_type == Markup::HTE_THEAD
				|| common_ancestor_type == Markup::HTE_TR)
			{
				HTML5ELEMENT *last_elm = last_node->GetNode();
				RemoveNamedElementAnchor named_element_anchor(this, last_elm);
				ANCHOR(RemoveNamedElementAnchor, named_element_anchor);
				if (last_elm->Parent())
				{
#ifdef DELAYED_SCRIPT_EXECUTION
					RecordOldParentL(last_elm, last_elm->Parent());
#endif // DELAYED_SCRIPT_EXECUTION
					last_elm->Out();
				}
				BOOL prev_foster_parenting = IsFosterParenting();
				SetIsFosterParenting(TRUE);
				// to force foster parenting, even if current node isn't table-like
				m_current_node = common_ancestor;
				InsertNodeL(last_elm, FALSE);
				named_element_anchor.Release();
				m_current_node = m_open_elements.Last();
				SetIsFosterParenting(prev_foster_parenting);
			}
			else
			{
				HTML5ELEMENT *last_elm = last_node->GetNode();
#ifdef DELAYED_SCRIPT_EXECUTION
				RecordOldParentL(last_elm, last_elm->Parent());
#endif // DELAYED_SCRIPT_EXECUTION
				MoveNode(last_elm, common_ancestor->GetNode());
			}

			HTML5ELEMENT *new_elm = CreateElementL(formatting_element->GetToken());
#ifdef DELAYED_SCRIPT_EXECUTION
			if (is_parsing_ahead)
				new_elm->SetInserted(HE_INSERTED_BY_PARSE_AHEAD);
#endif // DELAYED_SCRIPT_EXECUTION

			HTML5NODE *child = furthest_block->GetNode()->FirstChild();
			while (child)
			{
				HTML5NODE *next_child = child->Suc();
				MoveNode(child, new_elm);
				child = next_child;
			}

			MoveNode(new_elm, furthest_block->GetNode());

			formatting_element->Out();
			formatting_element->SetNode(new_elm);
			bookmark.InsertAtBookmark(formatting_element);

			formatting_stack_entry->Out();
			formatting_stack_entry->SetNode(new_elm);
			formatting_stack_entry->Follow(furthest_block);
			m_current_node = m_open_elements.Last();
		}

		formatting_element = m_active_formatting_elements.Last();
	}

	return TRUE;
}

void HTML5TreeBuilder::CheckIllegalOpenAfterBody(BOOL is_eof)
{
	if (IsFragment())
		return;

	HTML5OpenElement *open_iter = m_open_elements.Last();
	while (open_iter)
	{
		if (!IsAllowedToBeOpenAfterBody(open_iter->GetElementType(), is_eof)
			&& open_iter->GetNode() != m_body_elm.GetElm()
			&& open_iter->GetNode() != m_html_elm.GetElm())
		{
			EMIT_ERROR(ILLEGAL_UNCLOSED);
			break;
		}

		open_iter = open_iter->Pred();
	}
}

void HTML5TreeBuilder::SetNewState()
{
	OP_ASSERT(m_token.GetType() >= HTML5Token::DOCTYPE && m_token.GetType() <= HTML5Token::ENDOFFILE);
	SetState(g_html5_transitions[m_im][m_token.GetType()]);
}

void HTML5TreeBuilder::SetNextState()
{
	OP_ASSERT(m_token.GetType() >= HTML5Token::DOCTYPE && m_token.GetType() <= HTML5Token::ENDOFFILE);
	HTML5Token::TokenType token_type = m_token.GetType();
	Markup::Type token_elm_type = Markup::HTE_UNKNOWN;
	if (token_type == HTML5Token::START_TAG)
	{
		token_elm_type = g_html5_name_mapper->GetTypeFromTokenBuffer(m_token.GetName());
		m_token.SetElementType(token_elm_type);
	}

	if (!m_current_node)
		SetState(g_html5_transitions[m_im][token_type]);
	else if (m_current_node->GetNs() == Markup::HTML)
		SetState(g_html5_transitions[m_im][token_type]);
	else if (token_type == HTML5Token::START_TAG
			&& (IsMathMLTextIntegrationPoint(GetCurrentNode()) && token_elm_type != Markup::MATHE_MALIGNMARK && token_elm_type != Markup::MATHE_MGLYPH)
				|| (m_current_node->GetElementType() == Markup::MATHE_ANNOTATION_XML && token_elm_type == Markup::SVGE_SVG)
				|| m_current_node->IsHTMLIntegrationPoint())
	{
		SetState(g_html5_transitions[m_im][token_type]);
	}
	else if (token_type == HTML5Token::CHARACTER &&
			 (m_current_node->IsHTMLIntegrationPoint() || IsMathMLTextIntegrationPoint(GetCurrentNode())))
		SetState(g_html5_transitions[m_im][token_type]);
	else
		SetState(g_html5_transitions[IN_FOREIGN_CONTENT][token_type]);
}

void HTML5TreeBuilder::SetInsertionMode(InsertionMode new_mode)
{
	m_before_using_im = INITIAL;
	m_im = new_mode;
}

void HTML5TreeBuilder::StartUsingIM(InsertionMode new_mode)
{
	if (m_before_using_im == INITIAL)
		m_before_using_im = m_im;
	m_im = new_mode;
}

void HTML5TreeBuilder::StoreOriginalIM()
{
	if (m_before_using_im != INITIAL) {
		m_original_im = m_before_using_im;
		m_before_using_im = INITIAL;
	}
	else
		m_original_im = m_im;
}

void HTML5TreeBuilder::ResetInsertionMode()
{
	HTML5OpenElement *node = m_open_elements.Last();
	HTML5ELEMENT *node_elm = NULL;
	InsertionMode new_mode = INITIAL;

	while (node)
	{
		if (!node->Pred())
		{
			OP_ASSERT(m_context_elm.GetElm() || !IsFragment());
			node_elm = m_context_elm.GetElm();
		}
		else
			node_elm = node->GetNode();

		new_mode = HTML5Parser::GetInsertionModeFromElementType(node_elm->Type(), node_elm == m_context_elm.GetElm());
		if (new_mode != INITIAL)
			break;

		node = node->Pred();
	}

	OP_ASSERT(new_mode != INITIAL);

	SetInsertionMode(new_mode);
}

/*static*/ BOOL HTML5TreeBuilder::IsEndingScope(Markup::Type iter_type, Markup::Ns iter_ns, HTML5TreeBuilder::ScopeType scope)
{
	if (scope == SCOPE_SELECT)
		return iter_ns != Markup::HTML || (iter_type != Markup::HTE_OPTGROUP && iter_type != Markup::HTE_OPTION);

	switch (iter_type)
	{
	case Markup::HTE_DOC_ROOT:
		return TRUE;

	case Markup::HTE_BUTTON:
		if (scope == SCOPE_BUTTON)
			return iter_ns == Markup::HTML;
		break;

	case Markup::HTE_APPLET:
	case Markup::HTE_CAPTION:
	case Markup::HTE_TD:
	case Markup::HTE_TH:
	case Markup::HTE_MARQUEE:
	case Markup::HTE_OBJECT:
		if (scope == SCOPE_NORMAL || scope == SCOPE_BUTTON || scope == SCOPE_LIST_ITEM)
			return iter_ns == Markup::HTML;
		break;

	case Markup::HTE_OL:
	case Markup::HTE_UL:
		if (scope == SCOPE_LIST_ITEM)
			return iter_ns == Markup::HTML;
		break;

	case Markup::HTE_HTML:
	case Markup::HTE_TABLE:
		if (scope != SCOPE_ALL)
			return iter_ns == Markup::HTML;
		break;

	case Markup::MATHE_MI:
	case Markup::MATHE_MO:
	case Markup::MATHE_MN:
	case Markup::MATHE_MS:
	case Markup::MATHE_MTEXT:
		if (scope == SCOPE_NORMAL || scope == SCOPE_BUTTON || scope == SCOPE_LIST_ITEM)
			return iter_ns == Markup::MATH;
		break;

	case Markup::SVGE_FOREIGNOBJECT:
	case Markup::SVGE_DESC:
	case Markup::SVGE_TITLE:
		if (scope == SCOPE_NORMAL || scope == SCOPE_BUTTON || scope == SCOPE_LIST_ITEM)
			return iter_ns == Markup::SVG;
		break;
	}

	return FALSE;
}

BOOL HTML5TreeBuilder::HasElementTypeInScope(Markup::Type elm_type, ScopeType scope) const
{
	if (elm_type == Markup::HTE_P && scope == SCOPE_BUTTON)
	{
		return HasPElementInButtonScope();
	}
	return ComputeElementTypeInScope(elm_type, scope);
}

BOOL HTML5TreeBuilder::ComputeElementTypeInScope(Markup::Type elm_type, ScopeType scope) const
{
	HTML5OpenElement *iter = m_open_elements.Last();
	while (iter)
	{
		Markup::Type iter_type = iter->GetElementType();
		if (iter_type == elm_type)
			return TRUE;
		else if (IsEndingScope(iter_type, iter->GetNs(), scope))
			return FALSE;

		iter = iter->Pred();
	}

	return FALSE;
}

unsigned HTML5TreeBuilder::NumberElementTypeInScope(Markup::Type elm_type, ScopeType scope) const
{
	unsigned rv = 0;
	HTML5OpenElement *iter = m_open_elements.Last();
	while (iter)
	{
		Markup::Type iter_type = iter->GetElementType();
		if (iter_type == elm_type)
			rv++;
		else if (IsEndingScope(iter_type, iter->GetNs(), scope))
			return rv;

		iter = iter->Pred();
	}

	return rv;
}

BOOL HTML5TreeBuilder::HasElementInScope(HTML5NODE *elm, ScopeType scope) const
{
	HTML5OpenElement *iter = m_open_elements.Last();
	while (iter)
	{
		HTML5ELEMENT *iter_node = iter->GetNode();
		if (iter_node == elm)
			return TRUE;
		else if (IsEndingScope(iter_node->Type(), iter_node->GetNs(), scope))
			return FALSE;

		iter = iter->Pred();
	}

	return FALSE;
}

void HTML5TreeBuilder::ClearElementLists()
{
	ClearOpenElementList(FALSE);
	m_free_open_elements.Clear();
	m_active_formatting_elements.Clear();
	m_free_active_formatting_elements.Clear();
	m_script_elms.Clear();
}

void HTML5TreeBuilder::ClearOpenElementList(BOOL keep)
{
	if (keep)
	{
		while (HTML5OpenElement *iter = m_open_elements.First())
			DeleteOpenElement(iter);
	}
	else
		m_open_elements.Clear();
}

BOOL HTML5TreeBuilder::PushOpenElementL(HTML5ELEMENT *node, HTML5TokenWrapper *token)
{
	OP_PROBE7_L(OP_PROBE_HTML5TREEBUILDER_PUSHOPENELML);	

	HTML5OpenElement *new_elm = GetNewOpenElement(node, IsFosterParenting());
	LEAVE_IF_NULL(new_elm);

	Markup::Type elm_type = node->Type();
	Markup::Ns elm_ns = node->GetNs();
	if (elm_ns == Markup::MATH)
	{
		if (node->Type() == Markup::MATHE_ANNOTATION_XML && token)
		{
			unsigned attr_idx;
			if (token->HasAttribute(UNI_L("encoding"), attr_idx))
			{
				if (token->AttributeHasValue(attr_idx, UNI_L("text/html"))
					|| token->AttributeHasValue(attr_idx, UNI_L("application/xhtml+xml")))
				{
					new_elm->SetIsHTMLIntegrationPoint();
				}
			}
		}
	}
	else if (elm_ns == Markup::SVG)
	{
		if (elm_type == Markup::SVGE_FOREIGNOBJECT
			|| elm_type == Markup::SVGE_DESC
			|| elm_type == Markup::SVGE_TITLE)
		{
			new_elm->SetIsHTMLIntegrationPoint();
		}
	}

	m_current_node = new_elm;
	new_elm->Into(&m_open_elements);

	if (HasReachedMaxTagNestingLevel() && !node->IsScriptElement())
		new_elm->DeleteNodeOnPop();

	m_tag_nesting_level++; // must be done after the level reached check

	if (m_tag_nesting_level == MaxTagNestingLevel || (m_tag_nesting_level > MaxTagNestingLevel && node->IsScriptElement()))
		m_current_insertion_node = new_elm;

	if (elm_type == Markup::HTE_P)
		HandlePElementPush();
	else if (IsEndingScope(elm_type, elm_ns, SCOPE_BUTTON))
		HandleButtonScopeMarkerPush();

	return TRUE;
}

void HTML5TreeBuilder::CloseOpenElement(HTML5OpenElement *node)
{
#ifndef HTML5_STANDALONE
	// if we reached max nesting level, the element was never inserted in tree
	// unless it was a script element, so no point doing this
	if (!HasReachedMaxTagNestingLevel() || node->GetNode()->IsScriptElement())
	{
		HTML5ELEMENT *elm = node->GetNode();
		OP_ASSERT(elm);
		if (elm)
		{
			elm->SetEndTagFound(TRUE);
			GetHLDocProfile()->EndElement(elm);
		}
	}
#endif // HTML5_STANDALONE

	if (node == m_current_insertion_node)
	{
		if (m_current_insertion_node->GetNode()->IsScriptElement())
			m_current_insertion_node = GetStackEntry(m_current_insertion_node->GetNode()->Parent());
		else
			m_current_insertion_node = NULL;
	}

	DeleteOpenElement(node);
	m_tag_nesting_level--;
}

void HTML5TreeBuilder::PopCurrentElement()
{
	HTML5OpenElement *top = m_open_elements.Last();
	OP_ASSERT(top == m_current_node);

	Markup::Type elm_type = top->GetElementType();
	Markup::Ns elm_ns = top->GetNs();

	if (top)
	{
		OP_ASSERT(m_tag_nesting_level > 0);
		CloseOpenElement(top);
		m_current_node = m_open_elements.Last();
	}
	if (elm_type == Markup::HTE_P) {
		HandlePElementPop();
	}
	else if (IsEndingScope(elm_type, elm_ns, SCOPE_BUTTON)) {
		HandleButtonScopeMarkerPop();
	}
}

void HTML5TreeBuilder::PopUpToIncludingType(Markup::Type elm_type)
{
	while (m_current_node->GetElementType() != elm_type)
		PopCurrentElement();
	PopCurrentElement(); // pop the target elm as well
}

void HTML5TreeBuilder::PopUpToIncludingNode(HTML5NODE *node)
{
	while (m_current_node->GetNode() != node)
		PopCurrentElement();
	PopCurrentElement(); // pop the target node as well
}

void HTML5TreeBuilder::RemoveFromOpenElements(HTML5NODE *node)
{
	HTML5OpenElement *iter = m_open_elements.Last();
	while (iter && iter->GetNode() != node)
		iter = iter->Pred();

	if (iter)
		CloseOpenElement(iter);

	m_current_node = m_open_elements.Last();
}

HTML5TreeBuilder::HTML5OpenElement* HTML5TreeBuilder::GetStackEntry(HTML5ELEMENT *node)
{
	HTML5OpenElement *iter = m_open_elements.Last();
	while (iter && iter->GetNode() != node)
		iter = iter->Pred();
	return iter;
}

void HTML5TreeBuilder::GenerateImpliedEndTags(Markup::Type skip, const uni_char *skip_name)
{
	HTML5OpenElement *iter = m_open_elements.Last();
	while (iter)
	{
		Markup::Type iter_type = iter->GetElementType();
		if ((iter_type == Markup::HTE_DD
				|| iter_type == Markup::HTE_DT
				|| iter_type == Markup::HTE_LI
				|| iter_type == Markup::HTE_OPTION
				|| iter_type == Markup::HTE_OPTGROUP
				|| iter_type == Markup::HTE_P
				|| iter_type == Markup::HTE_RP
				|| iter_type == Markup::HTE_RT)
			&& !iter->IsNamed(skip_name, skip))
		{
			PopCurrentElement();
			iter = m_open_elements.Last();
		}
		else
			break;
	}
}

void HTML5TreeBuilder::ClearTableStack(StackType type)
{
	while (m_current_node)
	{
		switch (m_current_node->GetElementType())
		{
		case Markup::HTE_HTML:
			OP_ASSERT(IsFragment());
			return;

		case Markup::HTE_TABLE:
			if (type == STACK_TABLE)
				return;
			break;

		case Markup::HTE_TBODY:
		case Markup::HTE_TFOOT:
		case Markup::HTE_THEAD:
			if (type == STACK_TBODY)
				return;
			break;

		case Markup::HTE_TR:
			if (type == STACK_TR)
				return;
			break;
		}

		PopCurrentElement();
	}
}

void HTML5TreeBuilder::TerminateCell(Markup::Type elm_type)
{
	HandleSpecialEndTag(elm_type, TRUE);
	ClearActiveFormattingElements();
	SetInsertionMode(IN_ROW);
}

void HTML5TreeBuilder::CloseCell()
{
	if (HasElementTypeInScope(Markup::HTE_TD, SCOPE_TABLE))
		TerminateCell(Markup::HTE_TD);
	else
		TerminateCell(Markup::HTE_TH);
}

HTML5NODE* HTML5TreeBuilder::GetFormElement()
{
#ifdef HTML5_STANDALONE
	return m_form_elm.GetElm();
#else // HTML5_STANDALONE
	return GetHLDocProfile()->GetCurrentForm();
#endif // HTML5_STANDALONE
}

void HTML5TreeBuilder::SetFormElement(HTML5ELEMENT *elm)
{
#ifdef HTML5_STANDALONE
	if (elm)
		m_form_elm.SetElm(elm);
	else
		m_form_elm.Reset();
#else // HTML5_STANDALONE
	GetHLDocProfile()->SetNewForm(elm);
#endif // HTML5_STANDALONE
}

HTML5ELEMENT* HTML5TreeBuilder::GetCurrentNode()
{
	HTML5ELEMENT *ret_node = NULL;
	if (HasReachedMaxTagNestingLevel())
	{
		HTML5OpenElement *iter = m_current_node;
		while (iter && !iter->GetNode())
			iter = iter->Pred();

		ret_node = iter->GetNode();
	}
	else
		ret_node = m_current_node->GetNode();

	OP_ASSERT(ret_node);
	return ret_node;
}

HTML5ELEMENT* HTML5TreeBuilder::GetFosterParent(BOOL &insert_before)
{
	OP_ASSERT(IsFosterParenting());

	insert_before = FALSE;

	HTML5OpenElement *foster_parent = m_current_node;
	if (m_current_node->GetElementType() == Markup::HTE_TABLE
		|| m_current_node->GetElementType() == Markup::HTE_TBODY
		|| m_current_node->GetElementType() == Markup::HTE_TFOOT
		|| m_current_node->GetElementType() == Markup::HTE_THEAD
		|| m_current_node->GetElementType() == Markup::HTE_TR)
	{
		HTML5OpenElement *open_iter = m_open_elements.Last();
		while (open_iter && open_iter->GetElementType() != Markup::HTE_TABLE)
			open_iter = open_iter->Pred();

		if (open_iter)
		{
			if (open_iter->GetNode()->Parent())
			{
				// The foster parent is really the parent of the table
				// but since we are inserting before the table it is
				// simpler to store the table instead of finding the
				// table among the children of the parent when inserting
				foster_parent = open_iter;
				insert_before = TRUE;
			}
			else
				foster_parent = open_iter->Pred();
		}
		else
			foster_parent = m_open_elements.First();
	}

	return foster_parent->GetNode();
}

HTML5ELEMENT* HTML5TreeBuilder::CreateElementL(HTML5TokenWrapper &token)
{
	OP_PROBE7_L(OP_PROBE_HTML5TREEBUILDER_CREATEELEMENTL1);

#ifdef HTML5_STANDALONE
	OpStackAutoPtr<HTML5ELEMENT> new_node(OP_NEW(HTML5ELEMENT, (token.GetElementType(), token.GetElementNs())));
	LEAVE_IF_NULL(new_node.get());

	new_node->SetNameL(token.GetNameStr(), token.GetNameLength());
	new_node->InitAttributesL(&token);
#else // HTML5_STANDALONE
	OpStackAutoPtr<HTML5ELEMENT> new_node(NEW_HTML_Element());
	LEAVE_IF_NULL(new_node.get());

	new_node->ConstructL(m_parser->GetLogicalDocument(), token.GetElementType(), token.GetElementNs(), &token);
#endif // HTML5_STANDALONE

	return new_node.release();
}

HTML5ELEMENT* HTML5TreeBuilder::CreateElementL(const uni_char *elm_name, Markup::Type elm_type, HTML5TokenWrapper* token /* = NULL */)
{
#ifdef HTML5_STANDALONE
	OpStackAutoPtr<HTML5ELEMENT> new_node(OP_NEW(HTML5ELEMENT, (elm_type, Markup::HTML)));
	LEAVE_IF_NULL(new_node.get());

	new_node->SetNameL(elm_name, uni_strlen(elm_name));
#else // HTML5_STANDALONE
	OpStackAutoPtr<HTML5ELEMENT> new_node(NEW_HTML_Element());
	LEAVE_IF_NULL(new_node.get());

	new_node->ConstructL(m_parser->GetLogicalDocument(), elm_type, Markup::HTML, token);
#endif // HTML5_STANDALONE

	return new_node.release();
}

void HTML5TreeBuilder::InsertElementL(const uni_char *name, Markup::Type elm_type)
{
	HTML5ELEMENT *new_node = CreateElementL(name, elm_type);
	RemoveNamedElementAnchor named_element_anchor(this, new_node);
	ANCHOR(RemoveNamedElementAnchor, named_element_anchor);
	InsertNodeL(new_node);
	named_element_anchor.Release();
	if (!PushOpenElementL(new_node, NULL))
		OP_DELETE(new_node);
}

void HTML5TreeBuilder::InsertNodeL(HTML5NODE *new_node, BOOL run_insert_element /* = TRUE */)
{
	OP_PROBE7_L(OP_PROBE_HTML5TREEBUILDER_INSERTNODEL);

	HTML5ELEMENT* parent = NULL;
	BOOL insert_before = FALSE;

	if (HasReachedMaxTagNestingLevel() && !new_node->IsText() && !new_node->IsScriptElement())
		return;

	if (IsFosterParenting())
		parent = GetFosterParent(insert_before);
	else
		parent = GetCurrentInsertionNode();

#ifndef HTML5_STANDALONE
	HLDocProfile* hld_profile = GetHLDocProfile();

	if (run_insert_element && !IsFragment())
	{
		OP_HEP_STATUS oom_stat;
		if (new_node->IsText())
		{
			if (insert_before)
				oom_stat = hld_profile->InsertTextElementBefore(parent, new_node);
			else
				oom_stat = hld_profile->InsertTextElement(parent, new_node);
		}
		else
		{
			if (insert_before)
				oom_stat = hld_profile->InsertElementBefore(parent, new_node);
			else
				oom_stat = hld_profile->InsertElement(parent, new_node);
		}

		if (OpStatus::IsMemoryError(oom_stat))
		{
			if (new_node->Parent())
				new_node->Out();

			new_node->Clean(m_parser->GetLogicalDocument());

			LEAVE(oom_stat);
		}
	}
	else
#endif // HTML5_STANDALONE
		if (insert_before)
			new_node->Precede(parent);
		else
			new_node->Under(parent);

#ifdef DELAYED_SCRIPT_EXECUTION
	if (hld_profile->ESIsParsingAhead() && (IsFosterParenting() || m_current_node->IsInFosterParentedSubTree()))
		LEAVE_IF_ERROR(hld_profile->ESAddFosterParentedElement(new_node));
#endif // DELAYED_SCRIPT_EXECUTION
}

#ifdef DELAYED_SCRIPT_EXECUTION
HTML_Element* HTML5TreeBuilder::GetLastNonFosterParentedElement() const
{
	if (!m_open_elements.Last())
		return NULL;

	for (HTML5OpenElement* elm = m_open_elements.Last(); elm; elm = elm->Pred())
	{
		if (elm->HasBeenFosterParented())  // an ancestor has been foster parented
		{
			// find last ancestor which hasn't been foster parented
			for (elm = elm->Pred(); elm->HasBeenFosterParented(); elm = elm->Pred())
				/* empty body */;

			HTML_Element* last_biological_ancestor = elm->GetNode();  // opposite of foster parent would be biological parent, right? :)
			OP_ASSERT(!last_biological_ancestor->IsAncestorOf(m_open_elements.Last()->GetNode()));  // last open element should not be an actual decendant of the biological parent.  It wouldn't be foster parented if it was!

			if (last_biological_ancestor->LastLeafActualStyle())
				return last_biological_ancestor->LastLeafActualStyle();
			else
				return last_biological_ancestor;
		}
	}

	HTML_Element* elm = m_open_elements.Last()->GetNode();

	return elm->LastLeafActualStyle() ? elm->LastLeafActualStyle() : elm;
}

void HTML5TreeBuilder::CopyOpenElementsL(Head& from, Head& into)
{
	for (HTML5OpenElement* elm = static_cast<HTML5OpenElement*>(from.First()); elm && elm != m_open_elements.Last(); elm = elm->Suc())
	{
		HTML5OpenElement* copy = GetNewOpenElement(elm->GetNode(), elm->HasBeenFosterParented());
		if (!copy)
			LEAVE(OpStatus::ERR_NO_MEMORY);
		static_cast<Link*>(copy)->Into(&into);
	}
}

void HTML5TreeBuilder::CopyFormattingElementsL(Head& from, Head& into)
{
	for (HTML5ActiveElement* elm = static_cast<HTML5ActiveElement*>(from.First()); elm; elm = elm->Suc())
	{
		OpStackAutoPtr<HTML5ActiveElement> copy(GetNewActiveElement(elm->GetNode(), elm->GetFollowingMarkerCount()));
		if (!copy.get())
			LEAVE(OpStatus::ERR_NO_MEMORY);

		copy->InitL(elm->GetToken());

		static_cast<Link*>(copy.release())->Into(&into);
	}
}

OP_STATUS HTML5TreeBuilder::StoreParserState(HTML5ParserState* state)
{
	// We need to be in the same insertion mode when we restore.  However,
	// the insertion mode will be reset to original insertion mode not
	// long after this call, so rather store that.  Then, when restoring,
	// use this as the insertion mode.
	state->parser_insertion_mode = m_original_im;
	state->tag_nesting_level = m_tag_nesting_level > 0 ? m_tag_nesting_level - 1 : 0;
	state->p_elements_in_button_scope = m_p_elements_in_button_scope;

	RETURN_IF_LEAVE(CopyOpenElementsL(m_open_elements, state->open_elements));
	RETURN_IF_LEAVE(CopyFormattingElementsL(m_active_formatting_elements, state->active_formatting_elements));

	return OpStatus::OK;
}

OP_STATUS HTML5TreeBuilder::RestoreParserState(HTML5ParserState* state, HTML5ELEMENT *script_element, BOOL script_has_completed)
{
	SetInsertionMode(static_cast<InsertionMode>(state->parser_insertion_mode));

	ClearOpenElementList(TRUE);
	RETURN_IF_LEAVE(CopyOpenElementsL(state->open_elements, m_open_elements));

	m_active_formatting_elements.Clear();
	RETURN_IF_LEAVE(CopyFormattingElementsL(state->active_formatting_elements, m_active_formatting_elements));

	m_current_node = m_open_elements.Last();
	m_tag_nesting_level = state->tag_nesting_level;
	m_p_elements_in_button_scope = state->p_elements_in_button_scope;

	m_parser->SetParsingStarted();

	if (!script_has_completed)
	{
		// A delayed script has written.  That originally called for a insertion point
		// to be pushed, and nesting level to be increased.  Those were reset when we
		// parsed ahead, but now we're trying to recreate the condition just as it was
		// when the script should have been run originally, i.e. get these back where they were
		m_tokenizer->PushInsertionPointL(NULL);
		m_script_nesting_level++;

		HLDocProfile *hld_profile = GetHLDocProfile();
		ES_LoadManager *loadman = hld_profile->GetESLoadManager();

		ParserScriptElm* pscript_elm = NULL;
		RETURN_IF_LEAVE(pscript_elm = InsertScriptElementL(script_element, loadman->HasParserBlockingScript()));
		pscript_elm->SetIsStarted();
	}

	return OpStatus::OK;
}

BOOL HTML5TreeBuilder::HasParserStateChanged(HTML5ParserState* state)
{
	if (GetInsertionMode() != static_cast<InsertionMode>(state->parser_insertion_mode))
		return TRUE;

	HTML5OpenElement* open_elm = m_open_elements.First();
	HTML5OpenElement* open_stored_elm = static_cast<HTML5OpenElement*>(state->open_elements.First());
	for (; open_elm && open_stored_elm; open_elm = open_elm->Suc(), open_stored_elm = open_stored_elm->Suc())
		if (!open_elm->IsEqual(*open_stored_elm))
			return TRUE;

	if (open_elm != open_stored_elm)  // Both should be NULL
		return TRUE;

	HTML5ActiveElement* active_elm = m_active_formatting_elements.First();
	HTML5ActiveElement* active_stored_elm = static_cast<HTML5ActiveElement*>(state->active_formatting_elements.First());
	for (; active_elm && active_stored_elm; active_elm = active_elm->Suc(), active_stored_elm = active_stored_elm->Suc())
		if (!active_elm->IsEqual(*active_stored_elm))
			return TRUE;

	if (active_elm != active_stored_elm)  // Both should be NULL
		return TRUE;

	return FALSE;
}

#endif // DELAYED_SCRIPT_EXECUTION

void HTML5TreeBuilder::InsertStartTagL(HTML5TokenWrapper &token)
{
	OP_PROBE7_L(OP_PROBE_HTML5TREEBUILDER_INSERTSTARTTAGL);

	OP_ASSERT(token.GetType() == HTML5Token::START_TAG);

	HTML5ELEMENT *new_node = CreateElementL(token);
	RemoveNamedElementAnchor named_element_anchor(this, new_node);
	ANCHOR(RemoveNamedElementAnchor, named_element_anchor);
	InsertNodeL(new_node);
	named_element_anchor.Release();
	if (!PushOpenElementL(new_node, &token))
		OP_DELETE(new_node);
}

void HTML5TreeBuilder::InsertSelfclosingL()
{
	InsertStartTagL(m_token);
	PopCurrentElement();
	if (m_token.IsSelfClosing())
		m_token.AckSelfClosing();
}

void HTML5TreeBuilder::InsertCommentL(HTML5TokenWrapper &token)
{
	OP_ASSERT(token.GetType() == HTML5Token::COMMENT);

#ifdef HTML5_STANDALONE
	HTML5COMMENT *new_node = OP_NEW(HTML5COMMENT, ());
	LEAVE_IF_NULL(new_node);
	ANCHOR_PTR(HTML5COMMENT, new_node);
	new_node->InitDataL(&token);
#else // HTML5_STANDALONE
	HTML5COMMENT *new_node = NEW_HTML_Element();
	LEAVE_IF_NULL(new_node);
	ANCHOR_PTR(HTML5COMMENT, new_node);
	token.SetElementType(Markup::HTE_COMMENT);
	token.SetElementNs(Markup::HTML);
	new_node->ConstructL(m_parser->GetLogicalDocument(), Markup::HTE_COMMENT, Markup::HTML, &token);
#endif // HTML5_STANDALONE
	ANCHOR_PTR_RELEASE(new_node);

	HTML5ELEMENT *parent = NULL;
	if (m_im == AFTER_BODY)
		parent = m_html_elm.GetElm();
	else if (m_im == INITIAL
		|| m_im == AFTER_AFTER_BODY
		|| m_im == AFTER_AFTER_FRAMESET)
	{
		parent = m_root.GetNode();
	}
	else
		parent = GetCurrentNode();

#ifdef HTML5_STANDALONE
	new_node->Under(parent);
#else // HTML5_STANDALONE
	OP_STATUS oom_stat = GetHLDocProfile()->InsertElement(parent, new_node);
	if (OpStatus::IsMemoryError(oom_stat))
		LEAVE(oom_stat);
#endif // HTML5_STANDALONE
}

void HTML5TreeBuilder::InsertHtmlL()
{
	InsertElementL(UNI_L("html"), Markup::HTE_HTML);
	m_html_elm.SetElm(GetCurrentNode());
}

void HTML5TreeBuilder::InsertHeadL()
{
	InsertElementL(UNI_L("head"), Markup::HTE_HEAD);
	m_head_elm.SetElm(GetCurrentNode());
}

void HTML5TreeBuilder::InsertBodyL()
{
	InsertElementL(UNI_L("body"), Markup::HTE_BODY);
	m_body_elm.SetElm(GetCurrentNode());
}

#ifdef HTML5_STANDALONE
void HTML5TreeBuilder::AdjustForeignAttributesL()
{
	Markup::Ns ns = m_current_node->GetNs();

	unsigned attr_count = GetCurrentNode()->GetAttributeCount();
	for (unsigned i = 0; i < attr_count; i++)
	{
		HTML5AttrCopy *attr = GetCurrentNode()->GetAttributeByIndex(i);

		BOOL substituted = FALSE;
		HTML5TokenBuffer *attr_name = attr->GetName();
		if (ns == Markup::MATH)
		{
			// Adjust MathML attributes
			Markup::AttrType attr_type = g_html5_name_mapper->GetAttrTypeFromTokenBuffer(attr_name);
			if (attr_type != Markup::HA_XML)
			{
				const uni_char *subst = g_html5_name_mapper->GetAttrNameFromType(attr_type, Markup::MATH, FALSE);
				if (!attr_name->IsEqualTo(subst, uni_strlen(subst)))
				{
					attr->SetNameL(subst, uni_strlen(subst));
					substituted = TRUE;
				}
			}
		}
		else if (ns == Markup::SVG)
		{
			// Adjust SVG attributes
			Markup::AttrType attr_type = g_html5_name_mapper->GetAttrTypeFromTokenBuffer(attr_name);
			if (attr_type != Markup::HA_XML)
			{
				const uni_char *subst = g_html5_name_mapper->GetAttrNameFromType(attr_type, Markup::SVG, FALSE);
				if (!attr_name->IsEqualTo(subst, uni_strlen(subst))) // both are constant strings from the attribute table
				{
					attr->SetNameL(subst, uni_strlen(subst));
					substituted = TRUE;
				}
			}
		}

		const uni_char *attr_name_str = attr_name->GetBuffer();
		if (!substituted && *attr_name_str == 'x')
		{
			const uni_char *prefix_end = uni_strchr(attr_name_str, ':');
			if (prefix_end)
			{
				Markup::Ns attr_ns = Markup::HTML;
				if (uni_strncmp(attr_name_str, UNI_L("xlink"), prefix_end - attr_name_str) == 0)
					attr_ns = Markup::XLINK;
				else if (uni_strncmp(attr_name_str, UNI_L("xml"), prefix_end - attr_name_str) == 0)
					attr_ns = Markup::XML;
				else if (uni_strncmp(attr_name_str, UNI_L("xmlns"), prefix_end - attr_name_str) == 0)
					attr_ns = Markup::XMLNS;

				prefix_end++; // drop the ':'

				BOOL recognized = FALSE;
				if (attr_ns == Markup::XLINK)
				{
					recognized = uni_str_eq(prefix_end, UNI_L("actuate"))
						|| uni_str_eq(prefix_end, UNI_L("arcrole"))
						|| uni_str_eq(prefix_end, UNI_L("href"))
						|| uni_str_eq(prefix_end, UNI_L("role"))
						|| uni_str_eq(prefix_end, UNI_L("show"))
						|| uni_str_eq(prefix_end, UNI_L("title"))
						|| uni_str_eq(prefix_end, UNI_L("type"));
				}
				else if (attr_ns == Markup::XML)
				{
					recognized = uni_str_eq(prefix_end, UNI_L("base"))
						|| uni_str_eq(prefix_end, UNI_L("lang"))
						|| uni_str_eq(prefix_end, UNI_L("space"));
				}
				else if (attr_ns == Markup::XMLNS)
					recognized = uni_str_eq(prefix_end, UNI_L("xlink"));

				if (recognized)
				{
					attr->SetNameL(prefix_end, uni_strlen(prefix_end));
					attr->SetNs(attr_ns);
				}
			}
			else if (uni_str_eq(attr_name_str, UNI_L("xmlns")))
				attr->SetNs(Markup::XMLNS);
		}
	}
}
#endif // HTML5_STANDALONE

HTML5ELEMENT* HTML5TreeBuilder::CreateForeignElementL(HTML5TokenWrapper &token, Markup::Ns ns)
{
#ifdef HTML5_STANDALONE
	OpStackAutoPtr<HTML5ELEMENT> new_node(OP_NEW(HTML5ELEMENT, (token.GetElementType(), ns)));
	LEAVE_IF_NULL(new_node.get());

	HTML5TokenBuffer *name = token.GetName();
	const uni_char *name_str = name->GetBuffer();
	if (ns == Markup::SVG)
	{
		Markup::Type elm_type = g_html5_name_mapper->GetTypeFromName(name_str, FALSE, Markup::SVG);
		name_str = g_html5_name_mapper->GetNameFromType(elm_type, Markup::SVG, FALSE);
	}

	new_node->SetNameL(name_str, uni_strlen(name_str));
	new_node->InitAttributesL(&token);
#else // HTML5_STANDALONE
	OpStackAutoPtr<HTML5ELEMENT> new_node(NEW_HTML_Element());
	LEAVE_IF_NULL(new_node.get());

	token.SetElementNs(ns);
	new_node->ConstructL(m_parser->GetLogicalDocument(), m_token.GetElementType(), ns, &m_token);
#endif // HTML5_STANDALONE

	return new_node.release();
}

void HTML5TreeBuilder::InsertForeignL(Markup::Ns ns)
{
	OP_ASSERT(m_token.GetType() == HTML5Token::START_TAG);

	HTML5ELEMENT *new_node = CreateForeignElementL(m_token, ns);
	RemoveNamedElementAnchor named_element_anchor(this, new_node);
	ANCHOR(RemoveNamedElementAnchor, named_element_anchor);
	InsertNodeL(new_node);
	named_element_anchor.Release();
	if (!PushOpenElementL(new_node, &m_token))
		OP_DELETE(new_node);
#ifdef HTML5_STANDALONE
	else
		AdjustForeignAttributesL();
#else // HTML5_STANDALONE
	// Adjustment of attributes is done in HTML5TokenWrapper::GetOrCreateAttrEntries()
#endif // HTML5_STANDALONE

	if (m_token.IsSelfClosing())
	{
		m_token.AckSelfClosing();
		PopCurrentElement();
	}
}

unsigned HTML5TreeBuilder::GetNumberOfLeadingSpaces(HTML5TokenWrapper &token)
{
	unsigned leading = 0;
	HTML5StringBuffer::ContentIterator iter(token.GetData());
	unsigned block_len;
	const uni_char *block;

	block = iter.GetNext(block_len);
	while (block && block_len)
	{
		for (unsigned i = 0; i < block_len; i++)
		{
			if (!HTML5Parser::IsHTML5WhiteSpace(block[i]))
				return leading;
			leading++;
		}
		block = iter.GetNext(block_len);
	}

	return leading;
}

void HTML5TreeBuilder::AddTextFromTokenL(HTML5TokenBuffer &to_buffer, const HTML5StringBuffer &buffer, unsigned offset, unsigned count, BOOL replace_nulls)
{
	OP_PROBE7_L(OP_PROBE_HTML5TREEBUILDER_ADDTEXTFROMTOKENL);

	HTML5StringBuffer::ContentIterator iter(buffer);
	ANCHOR(HTML5StringBuffer::ContentIterator, iter);
	unsigned block_len;
	if (count == kAllText)
		count = buffer.GetContentLength() - offset;
	const uni_char *block = iter.GetNext(block_len);

	if (IsSkippingFirstNewline())
	{
		if (block_len > 0)
		{
			if (block[0] == '\n')
			{
				block++;
				block_len--;
				count--;
				if (offset)
					offset--;
			}

			SetIsSkippingFirstNewline(FALSE);
		}

		if (block_len == 0)
		{
			if (count == 0)
				return;
			else
				block = iter.GetNext(block_len);
		}
	}

	while (block && block_len && count)
	{
		if (offset >= block_len)
			offset -= block_len;
		else
		{
			if (offset + count < block_len)
				block_len = offset + count;

			to_buffer.EnsureCapacityL(block_len - offset);
			const uni_char *block_ptr = block + offset;
			for (unsigned i = offset; i < block_len; i++,block_ptr++)
			{
				if (*block_ptr == 0)
				{
					if (replace_nulls)
						to_buffer.Append(UNICODE_REPLACEMENT_CHARACTER);
				}
				else
					to_buffer.Append(*block_ptr);
			}

			count -= block_len - offset;
			offset = 0;
		}

		block = iter.GetNext(block_len);
	}
}

void HTML5TreeBuilder::InsertTableTextL()
{
	unsigned text_len = m_pending_table_text.Length();
	if (text_len)
	{
		BOOL should_foster_parent = IsNotPendingWsOnly();
		if (should_foster_parent)
		{
			SetIsFosterParenting(TRUE);
			ReconstructActiveFormattingElements();
			SetFramesetOk(FALSE);
		}

		m_text_accumulator.AppendL(m_pending_table_text.GetBuffer(), m_pending_table_text.Length());
		m_pending_table_text.Clear();
		InsertTextL();

		SetIsFosterParenting(FALSE);
	}
}

void HTML5TreeBuilder::InsertTextL()
{
	OP_PROBE7_L(OP_PROBE_HTML5TREEBUILDER_INSERTTEXTL);

#ifdef DELAYED_SCRIPT_EXECUTION
	BOOL use_actual = !GetHLDocProfile()->ESIsParsingAhead();
#endif // DELAYED_SCRIPT_EXECUTION

	unsigned text_len = m_text_accumulator.Length();
	if (text_len > 0)
	{
		HTML5NODE *existing_text;
		if (IsFosterParenting())
		{
			BOOL insert_before;
			HTML5ELEMENT *parent = GetFosterParent(insert_before);
			if (insert_before)
#ifdef DELAYED_SCRIPT_EXECUTION
				if (use_actual)
					existing_text = parent->PredActual();
				else
#endif // DELAYED_SCRIPT_EXECUTION
					existing_text = parent->Pred();
			else
#ifdef DELAYED_SCRIPT_EXECUTION
				if (use_actual)
					existing_text = parent->LastChildActual();
				else
#endif // DELAYED_SCRIPT_EXECUTION
					existing_text = parent->LastChild();
		}
		else
#ifdef DELAYED_SCRIPT_EXECUTION
			if (use_actual)
				existing_text = GetCurrentNode()->LastChildActual();
			else
#endif // DELAYED_SCRIPT_EXECUTION
				existing_text = GetCurrentNode()->LastChild();

		BOOL insert_new = !existing_text || !existing_text->IsText();
		OpStackAutoPtr<HTML5TEXT> text_anchor(NULL);

		if (insert_new)
		{
#ifdef HTML5_STANDALONE
			existing_text = OP_NEW_L(HTML5TEXT, ());
#else // HTML5_STANDALONE
			existing_text = NEW_HTML_Element();
			if (!existing_text
				|| OpStatus::IsMemoryError(existing_text->Construct(GetHLDocProfile(), static_cast<const uni_char*>(NULL), 0)))
			{
				OP_DELETE(existing_text);
				LEAVE(OpStatus::ERR_NO_MEMORY);
			}
#endif // HTML5_STANDALONE
			text_anchor.reset(static_cast<HTML5TEXT*>(existing_text));
		}

		HTML5TEXT *text_node = static_cast<HTML5TEXT*>(existing_text);

#ifdef HTML5_STANDALONE
		text_node->AppendTextL(m_text_accumulator.GetBuffer(), text_len);
#else // HTML5_STANDALONE
		OP_STATUS oom_stat = text_node->AppendText(GetHLDocProfile(),
			m_text_accumulator.GetBuffer(), text_len, FALSE, FALSE, FALSE, IsFosterParenting());
		if (OpStatus::IsMemoryError(oom_stat))
			LEAVE(oom_stat);
#endif // HTML5_STANDALONE

		m_text_accumulator.Clear();
		SetIsSkippingFirstNewline(FALSE);

		if (insert_new)
			InsertNodeL(text_anchor.release());
	}
}

static BOOL HasAttribute(HTML5ELEMENT *elm, const HTML5TokenBuffer *name)
{
#ifdef HTML5_STANDALONE
	return elm->GetAttribute(name->GetBuffer()) != NULL;
#else // HTML5_STANDALONE
	return elm->HasAttribute(name->GetBuffer(), NS_IDX_HTML);
#endif // HTML5_STANDALONE
}

void HTML5TreeBuilder::CopyMissingAttributesL(HTML5ELEMENT *existing_elm)
{
	if (existing_elm)
	{
		unsigned attr_count = m_token.GetAttrCount();
		for (unsigned i = 0; i < attr_count; i++)
		{
			HTML5Attr *attr = m_token.GetAttribute(i);
			if (!HasAttribute(existing_elm, attr->GetName()))
				AddAttributeL(existing_elm, i);
		}
	}
	else
		OP_ASSERT(IsFragment());
}

void HTML5TreeBuilder::ProcessDoctypeL()
{
#ifdef HTML5_STANDALONE
	HTML5DOCTYPE *new_node = OP_NEW(HTML5DOCTYPE, (m_parser->GetLogicalDocument()));
	LEAVE_IF_NULL(new_node);
	ANCHOR_PTR(HTML5DOCTYPE, new_node);

	new_node->InitL(m_token.GetNameStr(), m_token.GetPublicIdentifier(), m_token.GetSystemIdentifier());
	m_parser->GetLogicalDocument()->CheckQuirksMode();

	InsertNodeL(new_node);
	ANCHOR_PTR_RELEASE(new_node);
#else // HTML5_STANDALONE
	OpStackAutoPtr<HTML5DOCTYPE> new_node(OP_NEW(HTML5DOCTYPE, ()));
	LEAVE_IF_NULL(new_node.get());

	m_token.SetElementType(Markup::HTE_DOCTYPE);
	m_token.SetElementNs(Markup::HTML);
	new_node->ConstructL(m_parser->GetLogicalDocument(), Markup::HTE_DOCTYPE, Markup::HTML, &m_token);

	m_parser->GetLogicalDocument()->CheckQuirksMode(m_token.DoesForceQuirks());

	InsertNodeL(new_node.release());
#endif // HTML5_STANDALONE
}

void HTML5TreeBuilder::ProcessData(DataState state)
{
	InsertStartTagL(m_token);
	StoreOriginalIM();
	SetInsertionMode(TEXT);
	if (state == DATA_RAWTEXT)
		m_tokenizer->SetState(HTML5Tokenizer::RAWTEXT);
	else if (state == DATA_RCDATA)
		m_tokenizer->SetState(HTML5Tokenizer::RCDATA);
	else
		m_tokenizer->SetState(HTML5Tokenizer::SCRIPT_DATA);
}

void HTML5TreeBuilder::AddFormattingElementL(BOOL is_marker)
{
	if (HasReachedMaxTagNestingLevel())
		return;

	if (is_marker)
	{
		if (!m_active_formatting_elements.Empty())
			m_active_formatting_elements.Last()->PushMarker();
		return;
	}

	// The Noah's Ark clause. Max 3 equal entries up to the last marker
	unsigned found_matches = 0;
	HTML5ActiveElement *iter = m_active_formatting_elements.Last();
	while (iter && !iter->HasFollowingMarker())
	{
		if (iter->IsEqual(m_token))
			found_matches++;

		if (found_matches == 3)
		{
			DeleteActiveElement(iter);
			break;
		}

		iter = iter->Pred();
	}

	OpStackAutoPtr<HTML5ActiveElement> new_elm(GetNewActiveElement(GetCurrentNode(), 0));
	LEAVE_IF_NULL(new_elm.get());
	new_elm->InitL(m_token);

	new_elm->Into(&m_active_formatting_elements);
	new_elm.release();
}

HTML5TreeBuilder::HTML5ActiveElement* HTML5TreeBuilder::GetFormattingListEntry(HTML5ELEMENT *node)
{
	HTML5ActiveElement *iter = m_active_formatting_elements.Last();
	while (iter && iter->GetNode() != node)
		iter = iter->Pred();
	return iter;
}

BOOL HTML5TreeBuilder::HasActiveFormattingElement(Markup::Type elm_type, BOOL stop_at_marker) const
{
	HTML5ActiveElement *iter = m_active_formatting_elements.Last();
	while (iter && iter->GetElementType() != elm_type && (!stop_at_marker || !iter->HasFollowingMarker()))
		iter = iter->Pred();

	return iter && iter->GetElementType() == elm_type;
}

void HTML5TreeBuilder::ReconstructActiveFormattingElements()
{
	HTML5ActiveElement *entry = m_active_formatting_elements.Last();
	if (!entry || HasReachedMaxTagNestingLevel())
		return;

	if (entry->HasFollowingMarker() || HasElementInScope(entry->GetNode(), SCOPE_ALL))
		return;

	while (entry->Pred())
	{
		entry = entry->Pred();
		if (!entry->HasFollowingMarker() && !HasElementInScope(entry->GetNode(), SCOPE_ALL))
			continue;
		entry = entry->Suc();
		break;
	}

	while (entry)
	{
		HTML5ELEMENT *new_elm = CreateElementL(entry->GetToken());
		RemoveNamedElementAnchor named_element_anchor(this, new_elm);
		ANCHOR(RemoveNamedElementAnchor, named_element_anchor);
		InsertNodeL(new_elm);
		named_element_anchor.Release();
		entry->SetNode(new_elm);
#ifdef _DEBUG
		BOOL was_pushed = PushOpenElementL(new_elm, &entry->GetToken());
		OP_ASSERT(was_pushed);
#else // _DEBUG
		PushOpenElementL(new_elm, &entry->GetToken());
#endif // _DEBUG
		entry = entry->Suc();
	}
}

void HTML5TreeBuilder::ClearActiveFormattingElements()
{
	HTML5ActiveElement *entry = m_active_formatting_elements.Last();
	while (entry)
	{
		if (entry->HasFollowingMarker())
		{
			entry->PopMarker();
			break;
		}

		DeleteActiveElement(entry);

		entry = m_active_formatting_elements.Last();
	}
}

#ifndef HTML5_STANDALONE
HTML5TreeBuilder::ParserScriptElm* HTML5TreeBuilder::InsertScriptElementL(HTML5ELEMENT *script_element, BOOL is_blocking)
{
#ifdef _DEBUG
	ParserScriptElm *iter = m_script_elms.First();
	while (iter)
	{
		OP_ASSERT(!iter->IsBlocking());
		iter = iter->Suc();
	}
#endif // _DEBUG

	ParserScriptElm *new_elm = OP_NEW_L(ParserScriptElm, (script_element, is_blocking));

	new_elm->Into(&m_script_elms);

	return new_elm;
}

HTML5TreeBuilder::ParserScriptElm* HTML5TreeBuilder::GetParserBlockingScript()
{
	ParserScriptElm *iter = m_script_elms.First();
	while (iter && !iter->IsBlocking())
		iter = iter->Suc();

	return iter;
}

HTML5TreeBuilder::ParserScriptElm* HTML5TreeBuilder::GetParserScriptElm(HTML5ELEMENT *script_element)
{
	ParserScriptElm *iter = m_script_elms.First();
	while (iter && iter->GetElm() != script_element)
		iter = iter->Suc();

	return iter;
}

void HTML5TreeBuilder::StartParserBlockingScriptL(ParserScriptElm* blocking_script)
{
	if (blocking_script->IsReady())
	{
		blocking_script->SetIsStarted();

		m_parser->SetIsBlocking(FALSE);
		m_tokenizer->PushInsertionPointL(NULL);
		m_script_nesting_level++;
	}
	else
	{
		m_parser->SetIsBlocking(TRUE);
		if (m_tokenizer->ResetEOBSignallingForNextBuffer())
		{
			ES_LoadManager *loadman = GetHLDocProfile()->GetESLoadManager();
			loadman->SignalWriteFinished(); // to avoid further blocking of the writing script
		}
	}
}

HLDocProfile* HTML5TreeBuilder::GetHLDocProfile()
{
	return m_parser->GetLogicalDocument()->GetHLDocProfile();
}
#endif // HTML5_STANDALONE

void HTML5TreeBuilder::SetSourceCodePositionAttributeL(HTML5OpenElement* top)
{
	if (top->HasSourcePositionBeenAdded())  // already has position, no need to set it again, first one is more correct
		return;

	HTML5ELEMENT* elm = GetCurrentNode();
	OP_ASSERT(elm->Type() == Markup::HTE_SCRIPT || elm->Type() == Markup::HTE_STYLE);

	unsigned line = m_token.GetLineNum(), pos = m_token.GetLinePos();
	SourcePositionAttr* attr = OP_NEW(SourcePositionAttr, (line, pos));
	if (!attr)
		LEAVE(OpStatus::ERR_NO_MEMORY);

	if (elm->SetSpecialAttr(ATTR_SOURCE_POSITION, ITEM_TYPE_COMPLEX, attr, TRUE, SpecialNs::NS_LOGDOC) == -1)
	{
		OP_DELETE(attr);
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}

	top->SetSourcePositionAdded();
}

BOOL HTML5TreeBuilder::HTML5OpenElement::IsEqual(HTML5TreeBuilder::HTML5OpenElement& other)
{
	if (GetNode() != other.GetNode())
		return FALSE;

	if (HasBeenFosterParented() != other.HasBeenFosterParented())
		return FALSE;

	return TRUE;
}

/* virtual */ void HTML5TreeBuilder::HTML5ElementAnchor::Cleanup(int error)
{
	CleanupItem::Cleanup(error);
	m_tree_builder->ClearOpenElementList(TRUE);
	if (m_html_ref->GetElm() == m_elm)
		m_html_ref->Reset();
	if (m_head_ref->GetElm() == m_elm)
		m_head_ref->Reset();
	if (m_body_ref->GetElm() == m_elm)
		m_body_ref->Reset();
	if (m_context_ref->GetElm() == m_elm)
		m_context_ref->Reset();
	HTML5NODE *child = m_elm->FirstChild();
	while (child)
	{
		HTML5NODE *next_child = child->Suc();
		child->Out();
		child->Under(m_root);
		child = next_child;
	}

	LogicalDocument *logdoc = m_tree_builder->m_parser->GetLogicalDocument();
	logdoc->GetHLDocProfile()->SetNewForm(m_old_form);

	logdoc->RemoveNamedElement(m_elm, FALSE);
	if (m_elm->Parent())
		m_elm->Out();
	m_elm->HTML5ELEMENT::~HTML5ELEMENT();
}

HTML5TreeBuilder::RemoveNamedElementAnchor::~RemoveNamedElementAnchor()
{
	if (m_elm)
		m_tree_builder->m_parser->GetLogicalDocument()->RemoveNamedElement(m_elm, FALSE);
}
