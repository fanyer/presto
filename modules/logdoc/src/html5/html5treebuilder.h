/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef HTML5TREEBUILDER_H
#define HTML5TREEBUILDER_H

#include "modules/logdoc/markup.h"
#include "modules/logdoc/elementref.h"
#include "modules/logdoc/src/html5/html5base.h"
#include "modules/logdoc/src/html5/html5tokenwrapper.h"
#include "modules/logdoc/src/html5/html5tokenbuffer.h"
#include "modules/logdoc/src/html5/html5treestate.h"
#include "modules/util/simset.h"

#include HTML5NODE_INCLUDE

#ifndef HTML5_STANDALONE
# include "modules/logdoc/logdoc_constants.h" // for SplitTextLen
#endif // !HTML5_STANDALONE

class HTML5Parser;
class HTML5Tokenizer;
class HTML5AttrCopy;
class HTML5ELEMENT;
class HTML5NODE;
#ifndef HTML5_STANDALONE
class ES_LoadManager;
#endif // !HTML5_STANDALONE


/**
 * HTML 5 tree builder. Builds a tree of elements using the tokenizer
 * on a buffer of HTML code.
 */
class HTML5TreeBuilder
{
public:

	enum InsertionMode
	{
		INITIAL,
		BEFORE_HTML,
		BEFORE_HEAD,
		IN_HEAD,
		IN_HEAD_NOSCRIPT,
		AFTER_HEAD,
		IN_BODY,
		TEXT,
		IN_TABLE,
		IN_TABLE_TEXT,
		IN_CAPTION,
		IN_COLUMN_GROUP,
		IN_TABLE_BODY,
		IN_ROW,
		IN_CELL,
		IN_SELECT,
		IN_SELECT_IN_TABLE,
		IN_FOREIGN_CONTENT,
		AFTER_BODY,
		IN_FRAMESET,
		AFTER_FRAMESET,
		AFTER_AFTER_BODY,
		AFTER_AFTER_FRAMESET
	};


	HTML5TreeBuilder()
		: m_parser(NULL)
		, m_tokenizer(NULL)
		, m_current_state(HTML5TreeState::ILLEGAL_STATE)
		, m_pending_state(HTML5TreeState::ILLEGAL_STATE)
		, m_script_nesting_level(0)
		, m_bool_init(0)
		, m_tag_nesting_level(0)
		, m_current_node(NULL)
		, m_current_insertion_node(NULL)
		, m_pending_table_text(1024)
		, m_text_accumulator(SplitTextLen * 2)
		, m_im(INITIAL)
		, m_before_using_im(INITIAL)
		, m_original_im(INITIAL)
		, m_p_elements_in_button_scope(0)
		{}
	~HTML5TreeBuilder();

	void	InitL(HTML5Parser *parser, HTML5ELEMENT *root);

	/**
	 * Parse a buffer and build a tree from it
	 */
	void	BuildTreeL();

	/**
	 * Parse a buffer and build a tree fragment from it
	 * @param[IN] context The context element to parse under
	 */
	void	BuildTreeFragmentL(HTML5ELEMENT *context);

	/**
	 * Abort ongoing tree building and set the builder in
	 * a finished state.
	 * @param abort If TRUE, the builder will not try to put the logical
	 *              tree in a well-formed state, but just stop building
	 *              and clean up allocated memory.
	 */
	void	StopBuildingL(BOOL abort);

#ifdef HTML5_STANDALONE
	/**
	 * @see HTML5Parser::SignalScriptFinished()
	 */
	BOOL		SignalScriptFinished(HTML5ELEMENT *script_element);
#else // HTML5_STANDALONE
	BOOL		RemoveScriptElement(HTML5ELEMENT *script_element, BOOL is_finished);
	OP_STATUS	ExternalScriptReady(HTML5ELEMENT *script_element);
	OP_STATUS	AddBlockingScript(HTML5ELEMENT *script_element);
	/**
	 * @see HTML5Parser::SignalScriptFinished()
	 */
	BOOL		SignalScriptFinished(HTML5ELEMENT *script_element, ES_LoadManager *loadman, BOOL remove_element);
#endif // HTML5_STANDALONE

	BOOL		CurrentNodeIsInHTML_Namespace() const { return m_current_node->GetNs() == Markup::HTML; }

	InsertionMode	GetInsertionMode() const { return m_before_using_im != INITIAL ? m_before_using_im : m_im; }

	void	SetCurrentTokenizer(HTML5Tokenizer* tokenizer) { m_tokenizer = tokenizer; }

	void	ResetToken();

	BOOL	IsFinished();
	BOOL	IsWaitingForScript() { return m_script_nesting_level > 0; }
	BOOL	IsParsingUnderElm(HTML5ELEMENT *elm) { return HasElementInScope(elm, SCOPE_ALL); }

#ifdef DELAYED_SCRIPT_EXECUTION
	HTML5ELEMENT*	GetLastNonFosterParentedElement() const;
	OP_STATUS		StoreParserState(HTML5ParserState* state);
	OP_STATUS		RestoreParserState(HTML5ParserState* state, HTML5ELEMENT *script_element, BOOL script_has_completed);
	BOOL			HasParserStateChanged(HTML5ParserState* state);
#endif // DELAYED_SCRIPT_EXECUTION

#ifdef _DEBUG
	unsigned	GetScriptNestingLevel() { return m_script_nesting_level; }
#endif // _DEBUG

private:

	static const unsigned	MaxTagNestingLevel = MAX_TREE_DEPTH;

	friend class HTML5Parser;

	enum ScopeType
	{
		SCOPE_NORMAL, /// stops at markers
		SCOPE_LIST_ITEM, /// stops at lists
		SCOPE_BUTTON, /// stops at button
		SCOPE_TABLE, /// stops at tables
		SCOPE_SELECT, /// stops at everything except option and optgroup
		SCOPE_ALL /// doesn't stop at markers
	};

	enum StackType
	{
		STACK_TABLE, /// stops at table or html
		STACK_TBODY, /// stops at tbody, tfoot, thead or html
		STACK_TR /// stops at tr or html
	};

	enum DataState
	{
		DATA_RAWTEXT,
		DATA_RCDATA,
		DATA_SCRIPT
	};

	class HTML5OpenElement : public ListElement<HTML5OpenElement>
	{
	public:
		BOOL			IsEqual(HTML5OpenElement &other);

		void			Reset() { m_node.Reset(); m_status_init = 0; }

		HTML5ELEMENT*	GetNode() const { return m_node.GetElm(); }
		void			SetNode(HTML5ELEMENT *node) { m_node.SetElm(node); if (node) m_status.ns_type = static_cast<unsigned>(node->GetNs()); }
		Markup::Type	GetElementType() const { return m_node.GetElm() ? m_node->Type() : Markup::HTE_UNKNOWN; }

		Markup::Ns		GetNs() const { return static_cast<Markup::Ns>(m_status.ns_type); }
		BOOL			IsNamed(const uni_char* name, Markup::Type type) { return m_node.GetElm() && m_node.GetElm()->IsNamed(name, type); }

		BOOL			IsHTMLIntegrationPoint() { return m_status.is_html_integration_point; }
		void			SetIsHTMLIntegrationPoint() { m_status.is_html_integration_point = 1; }

		BOOL			HasBeenFosterParented() const { return m_status.has_been_foster_parented; }
		void			SetHasBeenFosterParented(BOOL foster_parented) { m_status.has_been_foster_parented = foster_parented; }
		BOOL			IsInFosterParentedSubTree() const { return HasBeenFosterParented() || (Pred() && Pred()->IsInFosterParentedSubTree()); }

		BOOL			IsDeleted() { return m_status.is_deleted; }
		void			SetIsDeleted() { m_status.is_deleted = 1; }

		BOOL			HasSourcePositionBeenAdded() { return m_status.source_position_added; }
		void			SetSourcePositionAdded() { m_status.source_position_added = TRUE; }

		void			DeleteNodeOnPop() { m_node.FreeElementOnDelete(); }

	private:
		KeepAliveElementRef	m_node;
		union
		{
			struct
			{
				unsigned	ns_type:4;
				unsigned	is_html_integration_point:1;
				unsigned	has_been_foster_parented:1;
				unsigned	is_deleted:1;
				unsigned	source_position_added:1;
			} m_status;
			unsigned m_status_init;
		};
	};

	class HTML5ActiveElement : public ListElement<HTML5ActiveElement>
	{
	public:
		void				Reset() { m_node.Reset(); m_token.ResetWrapper(); }

		void				InitL(HTML5TokenWrapper &token);

		HTML5ELEMENT*		GetNode() const { return m_node.GetElm(); }
		void				SetNode(HTML5ELEMENT *node) { m_node.SetElm(node); }
		Markup::Type		GetElementType() const { return m_node->Type(); }

		Markup::Ns			GetNs() const { return m_node->GetNs(); }
		BOOL				IsNamed(const uni_char* name, Markup::Type type) { return m_node.GetElm()->IsNamed(name, type); }

		HTML5TokenWrapper&	GetToken() { return m_token; }

		BOOL				IsEqual(HTML5TokenWrapper &token) { return m_token.IsEqual(token); }
		BOOL				IsEqual(HTML5ActiveElement &other) { return m_node.GetElm() && m_node.GetElm() == other.m_node.GetElm(); }

		void				PushMarker() { m_following_marker_count++; }
		void				PopMarker() { m_following_marker_count--; }
		BOOL				HasFollowingMarker() const { return m_following_marker_count > 0; }
		unsigned			GetFollowingMarkerCount() { return m_following_marker_count; }
		void				SetMarkerCount(unsigned count) { m_following_marker_count = count; }

	private:
		KeepAliveElementRef	m_node;
		HTML5TokenWrapper	m_token;
		unsigned			m_following_marker_count;
	};

	class HTML5ElementAnchor;
	class RemoveNamedElementAnchor;
	friend class HTML5ElementAnchor;
	friend class RemoveNamedElementAnchor;

	/**
	 * Class to use in leaving functions where the HTML5ELEMENT may
	 * have been added to the named element list in LogicalDocument
	 * without being added to the tree. This will when a function
	 * leaves remove the element from the named element list in
	 * LogicalDocument and delete the element.
	 */
	class RemoveNamedElementAnchor
	{
		HTML5TreeBuilder *m_tree_builder;
		HTML5ELEMENT *m_elm;
	public:
		RemoveNamedElementAnchor(HTML5TreeBuilder *tree_builder, HTML5ELEMENT *elm) :
			m_tree_builder(tree_builder),
			m_elm(elm)
			{}
		~RemoveNamedElementAnchor();
		void Release() { m_elm = NULL; }
	};

	/**
	 * Class for use when having an element on the stack in a leaving
	 * function to clear all references to it before calling its
	 * destructor if the function leaves.
	 */
	class HTML5ElementAnchor : public CleanupItem
	{
	public:
		HTML5TreeBuilder *m_tree_builder;
		KeepAliveElementRef *m_html_ref;
		KeepAliveElementRef *m_head_ref;
		KeepAliveElementRef *m_body_ref;
		KeepAliveElementRef *m_context_ref;
		HTML5ELEMENT *m_elm;
		HTML5ELEMENT *m_root;
		HTML5ELEMENT *m_old_form;
		HTML5ElementAnchor(HTML5TreeBuilder *tree_builder, KeepAliveElementRef *html_ref, KeepAliveElementRef *head_ref, KeepAliveElementRef *body_ref, KeepAliveElementRef *context_ref, HTML5ELEMENT *elm, HTML5ELEMENT *root, HTML5ELEMENT *old_form) :
			m_tree_builder(tree_builder),
			m_html_ref(html_ref),
			m_head_ref(head_ref),
			m_body_ref(body_ref),
			m_context_ref(context_ref),
			m_elm(elm),
			m_root(root),
			m_old_form(old_form)
			{}
		virtual void Cleanup(int error);
	};

	class ActiveListBookmark;
	friend class ActiveListBookmark;
	class ParserScriptElm;

	HTML5Parser*		m_parser;
	HTML5Tokenizer*		m_tokenizer;

	HTML5TokenWrapper	m_token;

	HTML5TreeState::State	m_current_state;
	HTML5TreeState::State	m_pending_state;

	unsigned			m_script_nesting_level;
	List<ParserScriptElm>	m_script_elms;

	union
	{
		struct
		{
			unsigned	is_iframe:1;
			unsigned	is_fragment:1;
			unsigned	scripting_enabled:1;
			unsigned	frameset_ok:1;
			unsigned	in_quirks_mode:1;
			unsigned	is_foster_parenting:1;
			unsigned	not_pending_reprocessing:1;
			unsigned	foster_parent_before:1;
			unsigned	not_pending_table_text_is_ws:1;
			unsigned	skip_first_newline:1;
		} m_bools;
		unsigned m_bool_init;
	};

	unsigned				m_tag_nesting_level;
	HTML5OpenElement*		m_current_node;  // points to an element in m_open_elements
	HTML5OpenElement*		m_current_insertion_node; /// points to the node where we went over the nesting limit
	HTML5OpenElement		m_root;
	KeepAliveElementRef		m_context_elm;

	KeepAliveElementRef		m_html_elm;
	KeepAliveElementRef		m_head_elm;
	KeepAliveElementRef		m_body_elm;
#ifdef HTML5_STANDALONE
	KeepAliveElementRef		m_form_elm;
#endif // HTML5_STANDALONE

	HTML5TokenBuffer		m_pending_table_text;
	HTML5TokenBuffer		m_text_accumulator;

	List<HTML5OpenElement>	m_open_elements;
	List<HTML5OpenElement>	m_free_open_elements;
	List<HTML5ActiveElement>
							m_active_formatting_elements;
	List<HTML5ActiveElement>
							m_free_active_formatting_elements;

	InsertionMode	m_im;
	InsertionMode	m_before_using_im;
	InsertionMode	m_original_im;

	unsigned m_p_elements_in_button_scope;

	static const unsigned kAllText = UINT_MAX;

	BOOL	IsIframe() const { return static_cast<BOOL>(m_bools.is_iframe); }
	BOOL	IsFragment() const { return static_cast<BOOL>(m_bools.is_fragment); }
	BOOL	IsScriptingEnabled() const { return static_cast<BOOL>(m_bools.scripting_enabled); }
	BOOL	IsFramesetOk() const { return static_cast<BOOL>(m_bools.frameset_ok); }
	BOOL	IsInQuirksMode() const { return static_cast<BOOL>(m_bools.in_quirks_mode); }
	BOOL	IsFosterParenting() const { return static_cast<BOOL>(m_bools.is_foster_parenting); }
	BOOL	IsNotPendingReprocessing() const { return static_cast<BOOL>(m_bools.not_pending_reprocessing); }
	BOOL	IsFosterParentingBefore() const { return static_cast<BOOL>(m_bools.foster_parent_before); }
	BOOL	IsNotPendingWsOnly() const { return static_cast<BOOL>(m_bools.not_pending_table_text_is_ws); }
	BOOL	IsSkippingFirstNewline() const { return static_cast<BOOL>(m_bools.skip_first_newline); }

	void	SetIsFragment(BOOL is) { m_bools.is_fragment = static_cast<unsigned>(is); }
	void	SetScriptingEnabled(BOOL enabled) { m_bools.scripting_enabled = static_cast<unsigned>(enabled); }
	void	SetFramesetOk(BOOL ok);
	void	SetInQuirksMode(BOOL in_quirks) { m_bools.in_quirks_mode = static_cast<unsigned>(in_quirks); }
	void	SetIsFosterParenting(BOOL is) { m_bools.is_foster_parenting = static_cast<unsigned>(is); }
	void	SetIsNotPendingReprocessing(BOOL is_not) { m_bools.not_pending_reprocessing = static_cast<unsigned>(is_not); }
	void	SetIsFosterParentingBefore(BOOL is) { m_bools.foster_parent_before = static_cast<unsigned>(is); }
	void	SetIsNotPendingWsOnly(BOOL is) { m_bools.not_pending_table_text_is_ws = static_cast<unsigned>(is); }
	void	SetIsSkippingFirstNewline(BOOL skip) { m_bools.skip_first_newline = static_cast<unsigned>(skip); }

	static BOOL	IsSpecial(HTML5ELEMENT *elm);
	static BOOL	IsFormatting(Markup::Type elm_type);
	static BOOL	IsAllowedToBeOpenAfterBody(Markup::Type elm_type, BOOL is_eof);
	static BOOL	IsMathMLTextIntegrationPoint(HTML5ELEMENT *elm);

	void	HandleListElementL(Markup::Type type);
	void	HandleSpecialEndTag(Markup::Type type, BOOL generate_endtag_for_self);
	void	HandleEndTagInBody(const uni_char *name, Markup::Type type);
	void	AddAttributeL(HTML5ELEMENT *elm, const uni_char *name, unsigned name_len, const uni_char *value, unsigned value_len);
	void	AddAttributeL(HTML5ELEMENT *elm, unsigned index);
	void	HandleIsindexL();
	/**
	 * Move a node from one location in the tree to another
	 * @param node The node to be moved.
	 * @param new_parent The new parent that the element will be
	 */
	void	MoveNode(HTML5NODE *node, HTML5ELEMENT *new_parent);

#ifdef DELAYED_SCRIPT_EXECUTION
	void 	RecordOldParentL(HTML5ELEMENT* elm, HTML5ELEMENT* parent);
	void	CopyOpenElementsL(Head& from, Head& into);
	void	CopyFormattingElementsL(Head& from, Head& into);
#endif // DELAYED_SCRIPT_EXECUTION
	/**
	 * Called for end tags of formatting elements in body.
	 * @param type Type of the tag to end.
	 * @returns FALSE if the token should be ignored.
	 */
	BOOL	HandleAdoptionL(Markup::Type type);
	void	CheckIllegalOpenAfterBody(BOOL is_eof);

	void		SetNewState();
	void		SetNextState();
	void		SetState(HTML5TreeState::State new_state) { m_current_state = new_state; }
	HTML5TreeState::State
				GetState() const { return m_current_state; }

	void		SetInsertionMode(InsertionMode new_mode);
	void		StartUsingIM(InsertionMode new_mode);
	void		StoreOriginalIM();
	void		ResetInsertionMode();

	static BOOL	IsEndingScope(Markup::Type iter_type, Markup::Ns iter_ns, ScopeType scope);
	BOOL		HasElementTypeInScope(Markup::Type elm_type, ScopeType scope) const;
	BOOL		ComputeElementTypeInScope(Markup::Type elm_type, ScopeType scope) const;
	unsigned	NumberElementTypeInScope(Markup::Type elm_type, ScopeType scope) const;
	BOOL		HasElementInScope(HTML5NODE *elm, ScopeType scope) const;

	void	HandlePElementPush() { m_p_elements_in_button_scope++; }
	void	HandlePElementPop() { m_p_elements_in_button_scope--; }
	void	HandleButtonScopeMarkerPush() { m_p_elements_in_button_scope = 0; }
	void	HandleButtonScopeMarkerPop() { m_p_elements_in_button_scope = NumberElementTypeInScope(Markup::HTE_P, SCOPE_BUTTON); }
	BOOL	HasPElementInButtonScope() const { return m_p_elements_in_button_scope > 0; }

	BOOL	HasReachedMaxTagNestingLevel() const { return m_tag_nesting_level > MaxTagNestingLevel; }

	void	ClearElementLists();

	HTML5OpenElement*
			GetNewOpenElement(HTML5ELEMENT *node, BOOL foster_parented)
	{
		HTML5OpenElement *new_elm;
		if (!m_free_open_elements.Empty())
		{
			new_elm = m_free_open_elements.First();
			new_elm->Out();
		}
		else
		{
			new_elm = OP_NEW(HTML5OpenElement, ());
			if (!new_elm)
				return NULL;
			new_elm->Reset();
		}

		new_elm->SetNode(node);
		new_elm->SetHasBeenFosterParented(foster_parented);

		return new_elm;
	}
	void	DeleteOpenElement(HTML5OpenElement *open_elm)
	{
		open_elm->Out();
		open_elm->Reset();
		open_elm->Into(&m_free_open_elements);
	}
	void	ClearOpenElementList(BOOL keep);
	BOOL	PushOpenElementL(HTML5ELEMENT *node, HTML5TokenWrapper *token);
	void	CloseOpenElement(HTML5OpenElement *node);
	void	PopCurrentElement();
	void	PopUpToIncludingType(Markup::Type elm_type);
	void	PopUpToIncludingNode(HTML5OpenElement *node) { PopUpToIncludingNode(node->GetNode());}
	void	PopUpToIncludingNode(HTML5NODE *node);
	void	RemoveFromOpenElements(HTML5NODE *node);
	HTML5OpenElement*
			GetStackEntry(HTML5ELEMENT *node);
	void	GenerateImpliedEndTags(Markup::Type skip, const uni_char *skip_name);

	void	ClearTableStack(StackType type);
	void	TerminateCell(Markup::Type elm_type);
	void	CloseCell();

	HTML5ELEMENT*	GetFormElement();
	void			SetFormElement(HTML5ELEMENT *elm);

	HTML5ELEMENT*	GetCurrentNode();
	HTML5ELEMENT*	GetCurrentInsertionNode() { return m_current_insertion_node ? m_current_insertion_node->GetNode() : GetCurrentNode(); }
	HTML5ELEMENT*	GetFosterParent(BOOL &insert_before);

	HTML5ELEMENT*	CreateElementL(HTML5TokenWrapper &token);
	/** If token is provided as arugument, then only it will only be used for setting the attributes */
	HTML5ELEMENT*	CreateElementL(const uni_char *elm_name, Markup::Type elm_type, HTML5TokenWrapper *token = NULL);
	void		InsertElementL(const uni_char *name, Markup::Type elm_type);
	void		InsertNodeL(HTML5NODE *new_node, BOOL run_insert_element = TRUE);
	void		InsertStartTagL(HTML5TokenWrapper &token);
	void		InsertSelfclosingL();
	void		InsertCommentL(HTML5TokenWrapper &token);
	void		InsertHtmlL();
	void		InsertHeadL();
	void		InsertBodyL();

#ifdef HTML5_STANDALONE
	void		AdjustForeignAttributesL();
#endif // HTML5_STANDALONE
	HTML5ELEMENT*	CreateForeignElementL(HTML5TokenWrapper &token, Markup::Ns ns);
	void		InsertForeignL(Markup::Ns ns);

	unsigned	GetNumberOfLeadingSpaces(HTML5TokenWrapper &token);
	/**
	 * Adds text content to an internal text accumulator from a token buffer
	 * to be insert into the tree at a later stage.
	 *
	 * @param to_buffer The text buffer to append to.
	 * @param buffer The token buffer to insert.
	 * @param offset The offset into the the token buffer to start inserting from.
	 * @param count The number of characters from the offset to insert. kAllText for the rest of the buffer.
	 * @param replace_nulls If TRUE, all NULL characters will be replaced by the replacement character.
	 */
	void	AddTextFromTokenL(HTML5TokenBuffer &to_buffer, const HTML5StringBuffer &from_buffer, unsigned offset, unsigned count, BOOL replace_nulls);
	/**
	 * Inserts the table text in the accumulator into the tree.
	 */
	void	InsertTableTextL();
	/**
	 * Inserts the text in the accumulator into the tree.
	 */
	void	InsertTextL();

	void	CopyMissingAttributesL(HTML5ELEMENT *existing_elm);

	void	ProcessDoctypeL();
	void	ProcessData(DataState state);

	HTML5ActiveElement*
			GetNewActiveElement(HTML5ELEMENT *node, unsigned marker_count)
	{
		HTML5ActiveElement *new_elm;
		if (!m_free_active_formatting_elements.Empty())
		{
			new_elm = m_free_active_formatting_elements.First();
			new_elm->Out();
		}
		else
		{
			new_elm = OP_NEW(HTML5ActiveElement, ());
			if (!new_elm)
				return NULL;
			new_elm->Reset();
		}

		new_elm->SetNode(node);
		new_elm->SetMarkerCount(marker_count);

		return new_elm;
	}
	void	DeleteActiveElement(HTML5ActiveElement *active_elm)
	{
		active_elm->Out();
		active_elm->Reset();
		active_elm->Into(&m_free_active_formatting_elements);
	}
	void	AddFormattingElementL(BOOL is_marker);
	HTML5ActiveElement*
			GetFormattingListEntry(HTML5ELEMENT *node);
	BOOL	HasActiveFormattingElement(Markup::Type elm_type, BOOL stop_at_marker) const;
	void	ReconstructActiveFormattingElements();
	void	ClearActiveFormattingElements();

	void	SetSourceCodePositionAttributeL(HTML5OpenElement* top);
	void	SetSourceCodePositionL()
	{
		HTML5OpenElement* top = m_open_elements.Last();
		if (!top)
			return;

		Markup::Type type = top->GetElementType();
		if (type != Markup::HTE_SCRIPT && type != Markup::HTE_STYLE)
			return;

		SetSourceCodePositionAttributeL(top);
	}
#ifndef HTML5_STANDALONE
	ParserScriptElm*	InsertScriptElementL(HTML5ELEMENT *script_element, BOOL is_blocking);
	ParserScriptElm*	GetParserBlockingScript();
	ParserScriptElm*	GetParserScriptElm(HTML5ELEMENT *script_element);

	void	StartParserBlockingScriptL(ParserScriptElm* blocking_script);

	HLDocProfile* GetHLDocProfile();
#endif // HTML5_STANDALONE
};

#ifdef HTML5_STANDALONE
extern const HTML5TreeState::State g_html5_transitions[HTML5TreeState::kNumberOfTransitionStates][HTML5TreeState::kNumberOfTransitionSubStates];
#endif // HTML5_STANDALONE

#endif // HTML5TREEBUILDER_H
