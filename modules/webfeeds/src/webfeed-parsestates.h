// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef WEBFEED_PARSESTATES_H
#define WEBFEED_PARSESTATES_H

#include "modules/webfeeds/src/xmlelement.h"

#ifdef WEBFEEDS_BACKEND_SUPPORT

class WebFeedParser;
class RSSContext;
class AtomContext;
class XMLAttributeQN;

class WebFeedContentElement;
class WebFeedLinkElement;
class WebFeedPersonElement;

// ***************************************************************************
//
//	WebFeedParseState
//
// ***************************************************************************

class WebFeedParseState
{
public:
	// Destruction.
	virtual ~WebFeedParseState() {};

	// State types.
	enum StateType
	{
		StateUnknown,
		StateInitial,
		StateDone,
		// RSS states.
		StateRSSRoot,
		StateRSSChannel,
		StateRSSItem,
		StateRSSImage,
		// Atom states.
		StateAtomRoot,
		StateAtomAuthor,
		StateAtomEntry
	};

	// Virtual Methods.
	virtual StateType Type() const { return StateUnknown; }
	virtual OP_STATUS OnStartElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, OpAutoStringHashTable<XMLAttributeQN> &attributes, StateType& next_state) { return OpStatus::OK; }
	virtual OP_STATUS OnEndElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, StateType& next_state) { return OpStatus::OK; }

	// Context info for the feed, that the states can use.
	class WebFeedContext
	{
	public:
		// Methods.
		RSSContext& AsRSSContext();
		const RSSContext& AsRSSContext() const;
		AtomContext& AsAtomContext();
		const AtomContext& AsAtomContext() const;

	protected:
		// Construction.
		WebFeedContext() { }

	private:
		// No copy or assignment.
		WebFeedContext(const WebFeedContext& other);
		WebFeedContext& operator=(const WebFeedContext& other);
	};

	// Methods.
	UINT AncestorCount() { return m_ancestor_count; }
	void UpdateAncestorCount(UINT ancestor_count) { m_ancestor_count = ancestor_count; }

	void SetContext(WebFeedContext* context) { m_context = context; }
	// TODO: check memory allocation
	OpAutoPtr<WebFeedContext> ReleaseContext() { return m_context; }

	BOOL HasContext() const { return m_context.get() != NULL; }
	WebFeedContext& Context() { return *m_context; }
	const WebFeedContext& Context() const { return *m_context; }

protected:
	// Construction.
	WebFeedParseState(WebFeedParser& parser);

	// Parser access.
	WebFeedParser& Parser() { return m_parser; }
	const WebFeedParser& Parser() const { return m_parser; }

private:
	// No copy or assignment.
	WebFeedParseState(const WebFeedParseState& other);
	WebFeedParseState& operator=(const WebFeedParseState& other);

	// Members.
	WebFeedParser& m_parser;
	OpAutoPtr<WebFeedContext> m_context;
	UINT m_ancestor_count;
};


// ***************************************************************************
//
//	InitialFeedParseState
//
// ***************************************************************************

class InitialFeedParseState
:	public WebFeedParseState
{
public:
	// Construction.
	InitialFeedParseState(WebFeedParser& parser);
	~InitialFeedParseState() {}

private:
	// WebFeedParseState.
	virtual StateType Type() const { return WebFeedParseState::StateInitial; }
	virtual OP_STATUS OnStartElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, OpAutoStringHashTable<XMLAttributeQN> &attributes, StateType& next_state);
};


// ***************************************************************************
//
//	RSSParseStateBase
//
// ***************************************************************************

class RSSParseStateBase
:	public WebFeedParseState
{
protected:
	// Construction.
	RSSParseStateBase(WebFeedParser& parser);
	~RSSParseStateBase() {}

	// Methods.
	OP_STATUS UpdateContentElement(WebFeedContentElement& element, const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, OpAutoStringHashTable<XMLAttributeQN>& attributes);
	OP_STATUS UpdateContentElementValue(WebFeedContentElement& element);
	OP_STATUS UpdateAuthorInfo(WebFeedPersonElement& author);
};

// ***************************************************************************
//
//	RSSRootParseState
//
// ***************************************************************************

class RSSRootParseState
:	public RSSParseStateBase
{
public:
	// Construction.
	RSSRootParseState(WebFeedParser& parser);

private:
	// WebFeedParseState.
	virtual StateType Type() const { return WebFeedParseState::StateRSSRoot; }
	virtual OP_STATUS OnStartElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, OpAutoStringHashTable<XMLAttributeQN> &attributes, StateType& next_state);
	virtual OP_STATUS OnEndElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, StateType& next_state);
};


// ***************************************************************************
//
//	RSSChannelParseState
//
// ***************************************************************************

class RSSChannelParseState
:	public RSSParseStateBase
{
public:
	// Construction.
	RSSChannelParseState(WebFeedParser& parser);

private:
	// WebFeedParseState.
	virtual StateType Type() const { return WebFeedParseState::StateRSSChannel; }
	virtual OP_STATUS OnStartElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, OpAutoStringHashTable<XMLAttributeQN> &attributes, StateType& next_state);
	virtual OP_STATUS OnEndElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, StateType& next_state);
};


// ***************************************************************************
//
//	RSSItemParseState
//
// ***************************************************************************

class RSSItemParseState
:	public RSSParseStateBase
{
public:
	// Construction / destruction.
	RSSItemParseState(WebFeedParser& parser);
	OP_STATUS Init();

private:
	// WebFeedParseState.
	virtual StateType Type() const { return WebFeedParseState::StateRSSItem; }
	virtual OP_STATUS OnStartElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, OpAutoStringHashTable<XMLAttributeQN> &attributes, StateType& next_state);
	virtual OP_STATUS OnEndElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, StateType& next_state);

	// Methods.
	OP_STATUS ItemLink(WebFeedLinkElement*& link);

	// Members.
	BOOL3 m_guid_is_permalink;
	BOOL m_have_good_author_info;
	BOOL m_have_xml_content;
};

// ***************************************************************************
//
//	RSSImageParseState
//
// ***************************************************************************

class RSSImageParseState
:	public RSSParseStateBase
{
public:
	// Construction / destruction.
	RSSImageParseState(WebFeedParser& parser, StateType previous_state);
	OP_STATUS Init();

private:
	// WebFeedParseState.
	virtual StateType Type() const { return WebFeedParseState::StateRSSImage; }
	// We maybe should do something more useful for image. But for now we ignore it, and only look for the closing </image> tag.  But at least its elements won't interfere with the similarily names elements in channel/item
	virtual OP_STATUS OnStartElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, OpAutoStringHashTable<XMLAttributeQN> &attributes, StateType& next_state) { return OpStatus::OK; }
	virtual OP_STATUS OnEndElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, StateType& next_state);

	StateType m_previous_state;
};

// ***************************************************************************
//
//	AtomParseStateBase
//
// ***************************************************************************

class AtomParseStateBase
:	public WebFeedParseState
{
protected:
	// Construction.
	AtomParseStateBase(WebFeedParser& parser);

	// Enum describing the mode a content construct is stored in.
	enum ContentMode
	{
		ContentUnknown,
		ContentXML,
		ContentEscaped,
		ContentBase64,
		ContentText
	};

	// Methods.
	OP_STATUS UpdateContentConstruct(WebFeedContentElement& element, const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, OpAutoStringHashTable<XMLAttributeQN>& attributes);
	OP_STATUS UpdateContentConstructValue(WebFeedContentElement& element);
	OP_STATUS UpdateLinkElement(WebFeedLinkElement& link, OpAutoStringHashTable<XMLAttributeQN>& attributes);

private:
	// Members.
	ContentMode m_content_mode;
};


// ***************************************************************************
//
//	AtomRootParseState
//
// ***************************************************************************

class AtomRootParseState
:	public AtomParseStateBase
{
public:
	// Construction.
	AtomRootParseState(WebFeedParser& parser);

private:
	// WebFeedParseState.
	virtual StateType Type() const { return WebFeedParseState::StateAtomRoot; }
	virtual OP_STATUS OnStartElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, OpAutoStringHashTable<XMLAttributeQN> &attributes, StateType& next_state);
	virtual OP_STATUS OnEndElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, StateType& next_state);

	// Methods.
	OP_STATUS HandleLinkElement(WebFeedLinkElement& link, OpAutoStringHashTable<XMLAttributeQN>& attributes);
};


// ***************************************************************************
//
//	AtomAuthorParseState
//
// ***************************************************************************

class AtomAuthorParseState
:	public AtomParseStateBase
{
public:
	// Construction.
	AtomAuthorParseState(WebFeedParser& parser, WebFeedPersonElement& author, StateType previous_state);

private:
	// WebFeedParseState.
	virtual StateType Type() const { return WebFeedParseState::StateAtomAuthor; }
	virtual OP_STATUS OnEndElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, StateType& next_state);

	// Members.
	WebFeedPersonElement& m_author;
	StateType m_previous_state;
};


// ***************************************************************************
//
//	AtomEntryParseState
//
// ***************************************************************************

class AtomEntryParseState
:	public AtomParseStateBase
{
public:
	// Construction.
	AtomEntryParseState(WebFeedParser& parser);
	OP_STATUS Init();

private:
	// WebFeedParseState.
	virtual StateType Type() const { return WebFeedParseState::StateAtomEntry; }
	virtual OP_STATUS OnStartElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, OpAutoStringHashTable<XMLAttributeQN> &attributes, StateType& next_state);
	virtual OP_STATUS OnEndElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, StateType& next_state);
	BOOL m_is_in_source_element;
	
	// Methods.
	OP_STATUS HandleLinkElement(OpAutoStringHashTable<XMLAttributeQN> &attributes);
};

#endif // WEBFEEDS_BACKEND_SUPPORT
#endif // WEBFEED_PARSESTATES_H
