// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef _WEBFEEDPARSER_H_
#define _WEBFEEDPARSER_H_

#ifdef WEBFEEDS_BACKEND_SUPPORT

#include "modules/url/url2.h"

#include "modules/webfeeds/webfeeds_api.h"
#include "modules/webfeeds/src/webfeed-parsestates.h"
#include "modules/webfeeds/src/webfeed.h"

# include "modules/xmlutils/xmltokenhandler.h"
# include "modules/xmlutils/xmlparser.h"

class WebFeedEntry;
class WebFeedListener;
class WebFeedLinkElement;
class WebFeedContentElement;
class XMLNamespacePrefix;
class LoadingFeed;

class WebFeedParserListener
{
public:
	virtual ~WebFeedParserListener();

	virtual void OnEntryLoaded(OpFeedEntry* entry, OpFeedEntry::EntryStatus, BOOL new_entry) = 0;
	virtual void ParsingDone(WebFeed*, OpFeed::FeedStatus) = 0;
	virtual void ParsingStarted() = 0;
};

class WebFeedParser
	: public XMLTokenHandler, public Link, public XMLParser::Listener
{
public:
	// Construction / destruction.
	// The parser will delete itself if the delete_when_done parameter is TRUE
	WebFeedParser(WebFeedParserListener* =NULL, BOOL delete_when_done = FALSE);
	virtual ~WebFeedParser();


	/// copied from XMLParserTracker in M2:
	OP_STATUS Init(const OpString& feed_content_location);

	// Parse a buffer containing a RSS/ATOM feed. is_final should be FALSE
	// if this is not the entire document.
	OP_STATUS Parse(const char* buffer, UINT buffer_length, BOOL is_final);
	OP_STATUS Parse(const uni_char *buffer, UINT buffer_length, BOOL is_final, unsigned* consumed = 0);
	OP_STATUS LoadFeed(URL& feed_url, WebFeed* existing_feed = NULL);
	OP_STATUS ResetParser(URL url = URL(), WebFeed* existing_feed = NULL);
	
	BOOL IsFailed() { return m_xml_parser.get() && m_xml_parser->IsFailed(); }
	
	// Callbacks from XML parser
	Result HandleToken(XMLToken &token);
	void Continue(XMLParser* parser);
	void Stopped(XMLParser* parser);
	BOOL Redirected(XMLParser *parser);
	void ParsingStopped(XMLParser *parser);

	// Methods called by the parser items.
	OP_STATUS CreateFeed();   // Called by parser	
	BOOL HasFeed() const { return m_feed != NULL; }
	WebFeed* Feed() { return m_feed; }
	const WebFeed* Feed() const { return m_feed; }

	OP_STATUS CreateEntry();
	BOOL HasEntry() const { return m_entry != NULL; }
	WebFeedEntry* Entry() { return m_entry; }
	const WebFeedEntry* Entry() const { return m_entry; }

	OP_STATUS AddEntryToFeed();
	OP_STATUS ResolveAndUpdateLink(WebFeedLinkElement& link, const OpStringC& uri);
	OP_STATUS UpdateContentBaseURIIfNeeded(WebFeedContentElement& content);
	OP_STATUS BaseURI(OpString& base_uri, const OpStringC& document_uri = OpStringC()) const;
	OP_STATUS QualifiedName(OpString& qualified_name, const OpStringC& namespace_uri, const OpStringC& local_name) const;
	OP_STATUS ParseAuthorInformation(const OpStringC& input, OpString& email, OpString& name) const;
	
	BOOL HasNamespacePrefix(const OpStringC& uri) const;
	OP_STATUS AddNamespacePrefix(const OpStringC& prefix, const OpStringC& uri);
	OP_STATUS NamespacePrefix(OpString& prefix, const OpStringC& uri) const;

	OP_STATUS SetKeepTags(const OpStringC& namespace_uri, const OpStringC& local_name, BOOL ignore_first_tag = FALSE);
	BOOL IsKeepingTags() const { return m_keep_tags.get() != 0; }
	const uni_char* ElementContent() { return m_content_buffer.GetContent(); }
private:
	/** Internal class for information about when we should keep tags.
	 * (for when HTML/XML markup is included as content in the XML feed) */
	class KeepTags
	{
	public:
		// Construction.
		KeepTags(BOOL ignore_first_tag);
		OP_STATUS Init(const OpStringC& namespace_uri, const OpStringC& local_name);

		// Methods.
		BOOL IsEndTag(const OpStringC& namespace_uri, const OpStringC& local_name) const;
		BOOL KeepThisTag() const { return !(m_ignore_first_tag && m_tag_depth == 0); }

		UINT IncTagDepth() { return ++m_tag_depth; }
		UINT DecTagDepth();

	private:
		// No copy or assignment.
		KeepTags(const KeepTags& other);
		KeepTags& operator=(const KeepTags& other);

		// Members.
		UINT m_tag_depth;
		BOOL m_ignore_first_tag;
		OpString m_namespace_uri;
		OpString m_local_name;
	};
	
	class ContentBuffer
	{
	public:
		ContentBuffer() : buffer(NULL), length(0), buffer_length(0) {}
		~ContentBuffer() { OP_DELETEA(buffer); }
		void Clear() { length = 0; }
		OP_STATUS EnsureSpace(size_t size) { size_t required_space = length + size + 1; return SetSize(required_space); }
		OP_STATUS SetSize(size_t);
		inline void Append(const uni_char*, size_t string_length);
		uni_char* TakeOver();
		const uni_char* GetContent();

	private:
		uni_char* buffer;
		size_t length;
		size_t buffer_length;
	};
	
	// local handlers used by HandleToken:
	void HandleStartTagTokenL(const XMLToken& token);
	void HandleEndTagTokenL(const XMLToken& token);
	OP_STATUS HandleTextToken(const XMLToken& token);
	OP_STATUS HandleCDATAToken(const XMLToken& token);

	// Methods.

	BOOL ChangeStateIfNeededL(WebFeedParseState::StateType next_state);
	OP_STATUS HandleEntryParsed(WebFeedEntry& entry);
	OP_STATUS StripCharactersNotCommonInNameAndEmail(OpString& text) const;
	
	///< \return Number of XML elements not closed yet (not counting current)
	UINT AncestorCount() const { return MAX(m_element_stack.GetCount() - 1, 0); }

	// Element stack operations.
	BOOL ElementStackHasContent() const { return m_element_stack.GetCount() > 0; }
	inline void PushElementStackL(XMLElement* element);   // Push element onto stack. Stack takes over ownership of object.
	inline XMLElement* PopElementStackL();  // Returns and removes top element. Caller takes over ownership of XMLElement
	inline XMLElement* TopElementStackL() const; // Returns top element, but it still remains on the stack


	// Members.
	OpAutoPtr<WebFeedParseState> m_parse_state;
	OpAutoPtr<KeepTags> m_keep_tags;
	OpAutoPtr<XMLParser> m_xml_parser;
	URL m_feed_url;  // the URL where the feed is downloaded from

	OpAutoVector<XMLElement> m_element_stack;
	OpAutoStringHashTable<XMLNamespacePrefix> m_namespace_prefixes;

	WebFeed* m_feed;
	WebFeedEntry* m_entry;
	WebFeedParserListener* m_listener;
	BOOL m_delete_when_done;
	BOOL m_parser_is_being_deleted;
	BOOL m_parsing_started;
	BOOL m_is_expecting_more_data;
	ContentBuffer m_content_buffer;

	OpString m_feed_content_location;
};

//****************************************************************************
//
//	XMLNamespacePrefix
//
//****************************************************************************

class XMLNamespacePrefix
{
public:
	// Construction / destruction.
	XMLNamespacePrefix();
	~XMLNamespacePrefix();

	OP_STATUS Init(const OpStringC& prefix, const OpStringC& uri);

	// Methods.
	const OpStringC& Prefix() const { return m_prefix; }
	const OpStringC& URI() const { return m_uri; }

private:
	// No copy or assignment.
	XMLNamespacePrefix(const XMLNamespacePrefix& prefix);
	XMLNamespacePrefix& operator=(const XMLNamespacePrefix& prefix);

	// Members.
	OpString m_prefix;
	OpString m_uri;
};

void WebFeedParser::ContentBuffer::Append(const uni_char* src, size_t string_length)
{
	if (length + string_length + 1 > buffer_length)
	{
		OP_ASSERT(FALSE);
		return;
	}

	op_memcpy(&buffer[length], src, string_length * sizeof(uni_char));
	length += string_length;
	buffer[length] = '\0';
}
#endif // WEBFEEDS_BACKEND_SUPPORT
#endif //_WEBFEEDPARSER_H_
