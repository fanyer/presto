// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 2005-2011 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#include "core/pch.h"

#ifdef WEBFEEDS_BACKEND_SUPPORT

#include "modules/webfeeds/src/webfeed-parsestates.h"
#include "modules/webfeeds/src/webfeedparser.h"
#include "modules/webfeeds/src/webfeedutil.h"

#include "modules/stdlib/util/opdate.h"

// ***************************************************************************
//
//	RSSFormat
//
// ***************************************************************************

class RSSFormat   // Really a namespace
{
public:
	// Version enumeration.
	enum Version
	{
		RSSUnknown,
		RSS091,		// Netscape / userland rss 0.91.
		RSS092,		// Userland rss 0.92.
		RSS093,		// Userland rss 0.93.
		RSS094,		// Userland rss 0.94.
		RSS10,		// RSS-DEV rss 1.0.
		RSS20,		// Userland rss 2.x.
	};

	// Functions.
	static BOOL IsRSSElement(const OpStringC& name) { return name.CompareI("rss") == 0; }
	static BOOL IsRDFElement(const OpStringC& name) { return name.CompareI("rdf:RDF") == 0; }
	static BOOL IsChannel(const OpStringC& name) { return (name.CompareI("channel") == 0) || (name.CompareI("rss:channel") == 0); }
	static BOOL IsItem(const OpStringC& name) { return (name.CompareI("item") == 0) || (name.CompareI("rss:item") == 0); }
	static BOOL IsTitle(const OpStringC& name) { return (name.CompareI("title") == 0) || (name.CompareI("rss:title") == 0); }
	static BOOL IsLink(const OpStringC& name) { return (name.CompareI("link") == 0) || (name.CompareI("rss:link") == 0); }
	static BOOL IsDescription(const OpStringC& name) { return (name.CompareI("description") == 0) || (name.CompareI("rss:description") == 0); }
	static BOOL IsEncodedContent(const OpStringC& name) { return name.CompareI("content:encoded") == 0; }
	static BOOL IsXMLContent(const OpStringC& name);
	static BOOL IsAuthorName(const OpStringC& name) { return name.CompareI("dc:creator") == 0; }
	static BOOL IsAuthorEmail(const OpStringC& name) { return (name.CompareI("author") == 0) || (name.CompareI("managingEditor") == 0); }
	static BOOL IsWebmasterEmail(const OpStringC& name) { return name.CompareI("webMaster") == 0; }
	static BOOL IsUpdatePeriod(const OpStringC& name) { return (name.CompareI("ttl") == 0) || (name.CompareI("sy:updatePeriod") == 0); }
	static BOOL IsUpdateFrequency(const OpStringC& name) { return name.CompareI("sy:updateFrequency") == 0; }
	static BOOL IsGUID(const OpStringC& name) { return name.CompareI("guid") == 0; }
	static BOOL IsPublicationDate(const OpStringC& name);
	static BOOL IsSource(const OpStringC& name) { return name.CompareI("source") == 0; }
	static BOOL IsEnclosure(const OpStringC& name) { return name.CompareI("enclosure") == 0; }
	static BOOL IsImage(const OpStringC& name) { return name.CompareI("image") == 0; }
	static OP_STATUS DefaultContentTypeFromVersion(const Version version, OpString& mime_type);

private:
	// No construction, copy or assignment.
	RSSFormat();
	RSSFormat(const RSSFormat& other);
	RSSFormat& operator=(const RSSFormat& other);
};


BOOL RSSFormat::IsXMLContent(const OpStringC& name)
{
	const BOOL is_xml_content = (name.CompareI("xhtml:body") == 0) ||
		(name.CompareI("xhtml:div") == 0) || (name.CompareI("body") == 0) ||
		(name.CompareI("div") == 0);
	return is_xml_content;
}


BOOL RSSFormat::IsPublicationDate(const OpStringC& name)
{
	const BOOL is_publication_date = (name.CompareI("pubDate") == 0) ||
		(name.CompareI("lastBuildDate") == 0) ||
		(name.CompareI("dc:date") == 0);
	return is_publication_date;
}


OP_STATUS RSSFormat::DefaultContentTypeFromVersion(const Version version,
	OpString& mime_type)
{
	// According to the standards, rss 0.91 and rss 1.0 have a MIME type of
	// text/plain, while the rest are text/html.
	if (version == RSS091 || version == RSS10)
		RETURN_IF_ERROR(mime_type.Set(UNI_L("text/plain")));
	else
		RETURN_IF_ERROR(mime_type.Set(UNI_L("text/html")));

	return OpStatus::OK;
}


// ***************************************************************************
//
//	RSSContext
//
// ***************************************************************************

class RSSContext
:	public WebFeedParseState::WebFeedContext
{
public:
	// Construction.
	RSSContext(RSSFormat::Version version)
	:	m_version(version),
		m_items_outside_channel(FALSE)
	{}

	// Methods.
	RSSFormat::Version Version() const { return m_version; }
	
	void SetItemsOutsideChannel(BOOL outside) { m_items_outside_channel = outside; }
	BOOL ItemsOutsideChannel() const { return m_items_outside_channel; }

private:
	// Members.
	RSSFormat::Version m_version;
	BOOL m_items_outside_channel;
};


// ***************************************************************************
//
//	AtomFormat
//
// ***************************************************************************

class AtomFormat  // really a namespace
{
public:
	// Version enumeration.
	enum Version
	{
		AtomUnknown,
		Atom03,		// The Atom Syndication Format 0.3 (PRE-DRAFT).
		Atom10		// The Atom Syndication Format (draft-ietf-atompub-format-11).
	};

	// Methods.
	static BOOL IsFeedElement(const OpStringC& name) { return (name.CompareI("feed") == 0) || (name.CompareI("atom:feed") == 0); }
	static BOOL IsTitle(const OpStringC& name) { return (name.CompareI("title") == 0) || (name.CompareI("atom:title") == 0); }
	static BOOL IsLink(const OpStringC& name) { return (name.CompareI("link") == 0) || (name.CompareI("atom:link") == 0); }
	static BOOL IsAuthor(const OpStringC& name) { return (name.CompareI("author") == 0) || (name.CompareI("atom:author") == 0); }
	static BOOL IsAuthorName(const OpStringC& name) { return (name.CompareI("name") == 0) || name.CompareI("atom:name") == 0; }
	static BOOL IsAuthorEmail(const OpStringC& name) { return (name.CompareI("email") == 0) || (name.CompareI("atom:email") == 0); }
	static BOOL IsSubtitle(const OpStringC& name);
	static BOOL IsSummary(const OpStringC& name) { return (name.CompareI("summary") == 0) || (name.CompareI("atom:summary") == 0); }
	static BOOL IsPublished(const OpStringC& name);
	static BOOL IsUpdated(const OpStringC& name);
	static BOOL IsId(const OpStringC& name) { return (name.CompareI("id") == 0) || (name.CompareI("atom:id") == 0); } 
	static BOOL IsIcon(const OpStringC& name) { return (name.CompareI("icon") == 0) || (name.CompareI("atom:icon") == 0); } 	
	static BOOL IsEntry(const OpStringC& name) { return (name.CompareI("entry") == 0) || (name.CompareI("atom:entry") == 0); }
	static BOOL IsContent(const OpStringC& name) { return (name.CompareI("content") == 0) || (name.CompareI("atom:content") == 0); }
	static BOOL IsSource(const OpStringC& name) { return (name.CompareI("source") == 0) || (name.CompareI("atom:source") == 0); }
	
private:
	// No construction, copy or assignment.
	AtomFormat();
	AtomFormat(const AtomFormat& other);
	AtomFormat& operator=(const AtomFormat& other);
};


BOOL AtomFormat::IsSubtitle(const OpStringC& name)
{
	const BOOL is_tagline = (name.CompareI("tagline") == 0) ||
		(name.CompareI("subtitle") == 0) ||
		(name.CompareI("atom:tagline") == 0) ||
		(name.CompareI("atom:subtitle") == 0);
	return is_tagline;
}


BOOL AtomFormat::IsPublished(const OpStringC& name)
{
	const BOOL is_published = (name.CompareI("created") == 0) ||
		(name.CompareI("published") == 0) ||
		(name.CompareI("atom:created") == 0) ||
		(name.CompareI("atom:published") == 0);
	return is_published;
}


BOOL AtomFormat::IsUpdated(const OpStringC& name)
{
	const BOOL is_updated = (name.CompareI("modified") == 0) ||
		(name.CompareI("atom:modified") == 0) ||
		(name.CompareI("updated") == 0) ||
		(name.CompareI("atom:updated") == 0);
	return is_updated;
}


// ***************************************************************************
//
//	AtomContext
//
// ***************************************************************************

class AtomContext
:	public WebFeedParseState::WebFeedContext
{
public:
	// Construction.
	AtomContext(AtomFormat::Version version)
	:	m_version(version)
	{}

	// Methods.
	AtomFormat::Version Version() const { return m_version; }

private:
	// Members.
	AtomFormat::Version m_version;
};


// ***************************************************************************
//
//	WebFeedParseState
//
// ***************************************************************************

WebFeedParseState::WebFeedParseState(WebFeedParser& parser)
:	m_parser(parser),
	m_ancestor_count(UINT_MAX)
{}


// ***************************************************************************
//
//	WebFeedParseState::WebFeedContext
//
// ***************************************************************************

RSSContext& WebFeedParseState::WebFeedContext::AsRSSContext()
{
	RSSContext& context = (RSSContext &)(*this);
	return context;
}


const RSSContext& WebFeedParseState::WebFeedContext::AsRSSContext() const
{
	const RSSContext& context = (const RSSContext &)(*this);
	return context;
}


AtomContext& WebFeedParseState::WebFeedContext::AsAtomContext()
{
	AtomContext& context = (AtomContext &)(*this);
	return context;
}


const AtomContext& WebFeedParseState::WebFeedContext::AsAtomContext() const
{
	const AtomContext& context = (const AtomContext &)(*this);
	return context;
}


// ***************************************************************************
//
//	InitialFeedParseState
//
// ***************************************************************************

InitialFeedParseState::InitialFeedParseState(WebFeedParser& parser)
:	WebFeedParseState(parser)
{
}


OP_STATUS InitialFeedParseState::OnStartElement(const OpStringC& namespace_uri,
	const OpStringC& local_name, const OpStringC& qualified_name,
	OpAutoStringHashTable<XMLAttributeQN> &attributes, StateType& next_state)
{
	if (RSSFormat::IsRSSElement(qualified_name))
	{
		// Attempt to detect the rss version used.
		RSSFormat::Version version = RSSFormat::RSS20;
		if (attributes.Contains(UNI_L("version")))
		{
			XMLAttributeQN* attribute = 0;
			RETURN_IF_ERROR(attributes.GetData(UNI_L("version"), &attribute));

			OP_ASSERT(attribute != 0);
			const OpStringC& value = attribute->Value();

			if (value.Compare("0.91") == 0)
				version = RSSFormat::RSS091;
			else if (value.Compare("0.92") == 0)
				version = RSSFormat::RSS092;
			else if (value.Compare("0.93") == 0)
				version = RSSFormat::RSS093;
			else if (value.Compare("0.94") == 0)
				version = RSSFormat::RSS094;
			else if (value.Find("2.") != KNotFound)
				version = RSSFormat::RSS20;
		}

		// Create a context and switch state.
		OP_ASSERT(!HasContext());
		RSSContext* context = OP_NEW(RSSContext, (version));
		if (!context)
			return OpStatus::ERR_NO_MEMORY;
		SetContext(context);

		next_state = StateRSSRoot;
	}
	else if (RSSFormat::IsRDFElement(qualified_name))
	{
		// Create a context and switch state.
		OP_ASSERT(!HasContext());
		RSSContext* context = OP_NEW(RSSContext, (RSSFormat::RSS10));
		if (!context)
			return OpStatus::ERR_NO_MEMORY;
		SetContext(context);

		next_state = StateRSSRoot;
	}
	else if (AtomFormat::IsFeedElement(qualified_name))
	{
		// Detect the Atom version.
		AtomFormat::Version version = AtomFormat::Atom10;
		if (attributes.Contains(UNI_L("version")))
		{
			XMLAttributeQN* attribute = 0;
			RETURN_IF_ERROR(attributes.GetData(UNI_L("version"), &attribute));

			OP_ASSERT(attribute != 0);
			const OpStringC& value = attribute->Value();

			if (value.Compare("0.3") == 0)
				version = AtomFormat::Atom03;
		}

		// Create a context and switch state.
		OP_ASSERT(!HasContext());
		AtomContext* context = OP_NEW(AtomContext, (version));
		if (!context)
			return OpStatus::ERR_NO_MEMORY;
		SetContext(context);

		next_state = StateAtomRoot;
	}

	if (next_state == StateRSSRoot || next_state == StateAtomRoot)
		RETURN_IF_ERROR(Parser().CreateFeed());

	return OpStatus::OK;
}


// ***************************************************************************
//
//	RSSParseStateBase
//
// ***************************************************************************

RSSParseStateBase::RSSParseStateBase(WebFeedParser& parser)
:	WebFeedParseState(parser)
{
}


OP_STATUS RSSParseStateBase::UpdateContentElement(WebFeedContentElement& element,
	const OpStringC& namespace_uri, const OpStringC& local_name,
	const OpStringC& qualified_name, OpAutoStringHashTable<XMLAttributeQN>& attributes)
{
	// Update the content type.
	OpString content_type;

	if (RSSFormat::IsDescription(qualified_name))
	{
		// Description might, for RSS 0.94, have a type attribute, telling
		// the MIME type of the content. To be on the safe side we will
		// respect it no matter the RSS version.
		if (attributes.Contains(UNI_L("type")))
		{
			XMLAttributeQN* attribute = NULL;
			RETURN_IF_ERROR(attributes.GetData(UNI_L("type"), &attribute));

			RETURN_IF_ERROR(content_type.Set(attribute->Value()));
		}
	}
	else if (RSSFormat::IsEncodedContent(qualified_name))
	{
		// Always use text/html for encoded content.
		RETURN_IF_ERROR(content_type.Set("text/html"));
	}
	else if (RSSFormat::IsXMLContent(qualified_name))
	{
		// Always use application/xhtml+xml for embedded html / xml.
		RETURN_IF_ERROR(content_type.Set("application/xhtml+xml"));
		RETURN_IF_ERROR(Parser().SetKeepTags(namespace_uri, local_name));
	}

	if (content_type.IsEmpty())
	{
		RETURN_IF_ERROR(RSSFormat::DefaultContentTypeFromVersion(
			Context().AsRSSContext().Version(), content_type));
	}

	OP_ASSERT(content_type.HasContent());
	RETURN_IF_ERROR(element.SetType(content_type));

	// Update the base uri.
	RETURN_IF_ERROR(Parser().UpdateContentBaseURIIfNeeded(element));

	return OpStatus::OK;
}


OP_STATUS RSSParseStateBase::UpdateContentElementValue(WebFeedContentElement& element)
{
	const RSSFormat::Version version = Context().AsRSSContext().Version();
	if (version == RSSFormat::RSS091 || version == RSSFormat::RSS10)
	{
		// Final attempt to determine if the content type should be
		// text/html.
		if (WebFeedUtil::ContentContainsHTML(Parser().ElementContent()))
			RETURN_IF_ERROR(element.SetType(UNI_L("text/html")));
	}

	RETURN_IF_ERROR(element.SetValue(Parser().ElementContent()));
	return OpStatus::OK;
}


OP_STATUS RSSParseStateBase::UpdateAuthorInfo(WebFeedPersonElement& author)
{
	OpString email;
	OpString name;

	RETURN_IF_ERROR(Parser().ParseAuthorInformation(Parser().ElementContent(), email, name));

	if (email.HasContent())
		RETURN_IF_ERROR(author.SetEmail(email));
	if (name.HasContent())
		RETURN_IF_ERROR(author.SetName(name));

	return OpStatus::OK;
}


// ***************************************************************************
//
//	RSSRootParseState
//
// ***************************************************************************

RSSRootParseState::RSSRootParseState(WebFeedParser& parser)
:	RSSParseStateBase(parser)
{
}


OP_STATUS RSSRootParseState::OnStartElement(const OpStringC& namespace_uri,
	const OpStringC& local_name, const OpStringC& qualified_name,
	OpAutoStringHashTable<XMLAttributeQN> &attributes, StateType& next_state)
{
	if (RSSFormat::IsChannel(qualified_name))
		next_state = StateRSSChannel;

	// Support feeds that close the channel element before the items.
	else if (RSSFormat::IsItem(qualified_name))
	{
		Context().AsRSSContext().SetItemsOutsideChannel(TRUE);
		next_state = StateRSSItem;
	}

	return OpStatus::OK;
}


OP_STATUS RSSRootParseState::OnEndElement(const OpStringC& namespace_uri,
	const OpStringC& local_name, const OpStringC& qualified_name,
	StateType& next_state)
{
	if (Context().AsRSSContext().Version() == RSSFormat::RSS10 &&
		RSSFormat::IsRDFElement(qualified_name))
		next_state = StateDone;
	else if (RSSFormat::IsRSSElement(qualified_name))
		next_state = StateDone;

	return OpStatus::OK;
}


// ***************************************************************************
//
//	RSSChannelParseState
//
// ***************************************************************************

RSSChannelParseState::RSSChannelParseState(WebFeedParser& parser)
:	RSSParseStateBase(parser)
{
}


OP_STATUS RSSChannelParseState::OnStartElement(const OpStringC& namespace_uri,
	const OpStringC& local_name, const OpStringC& qualified_name,
	OpAutoStringHashTable<XMLAttributeQN> &attributes, StateType& next_state)
{
	if (RSSFormat::IsItem(qualified_name))
		next_state = StateRSSItem;
	else if (RSSFormat::IsImage(qualified_name))
		next_state = StateRSSImage; 
	else if (RSSFormat::IsTitle(qualified_name))
	{
		OP_ASSERT(Parser().Feed());
		WebFeedContentElement* title = (WebFeedContentElement*)Parser().Feed()->GetTitle();
		RETURN_IF_ERROR(UpdateContentElement(*title, namespace_uri, local_name, qualified_name, attributes));
	}
	else if (RSSFormat::IsDescription(qualified_name))
	{
		WebFeedContentElement* tagline = Parser().Feed()->Tagline();
		RETURN_IF_ERROR(UpdateContentElement(*tagline, namespace_uri, local_name, qualified_name, attributes));
	}

	return OpStatus::OK;
}


OP_STATUS RSSChannelParseState::OnEndElement(const OpStringC& namespace_uri,
	const OpStringC& local_name, const OpStringC& qualified_name,
	StateType& next_state)
{
	if (RSSFormat::IsTitle(qualified_name))
	{
		WebFeedContentElement* title = (WebFeedContentElement*)Parser().Feed()->GetTitle();
		RETURN_IF_ERROR(UpdateContentElementValue(*title));
	}
	else if (RSSFormat::IsLink(qualified_name))
	{
		WebFeedLinkElement* link = Parser().Feed()->FeedLink();
		RETURN_IF_ERROR(Parser().ResolveAndUpdateLink(*link, Parser().ElementContent()));
	}
	else if (RSSFormat::IsDescription(qualified_name))
	{
		WebFeedContentElement* tagline = Parser().Feed()->Tagline();
		RETURN_IF_ERROR(UpdateContentElementValue(*tagline));
	}
	else if (RSSFormat::IsAuthorName(qualified_name) ||
		RSSFormat::IsAuthorEmail(qualified_name))
	{
		WebFeedPersonElement* author = Parser().Feed()->AuthorElement();
		RETURN_IF_ERROR(UpdateAuthorInfo(*author));
	}
	else if (RSSFormat::IsWebmasterEmail(qualified_name))
	{
		// Only use the webmaster information if we don't have anything else.
		WebFeedPersonElement* author = Parser().Feed()->AuthorElement();
		if (!author->HasEmail())
			RETURN_IF_ERROR(UpdateAuthorInfo(*author));
	}
	else if (RSSFormat::IsUpdatePeriod(qualified_name))
	{
		if (Parser().ElementContent())
		{
			UINT32 frequency = 0;  // in minutes

			if (uni_stri_eq(Parser().ElementContent(), "hourly"))
				frequency = 60;
			else if (uni_stri_eq(Parser().ElementContent(), "daily"))
				frequency = 1440;
			else if (uni_stri_eq(Parser().ElementContent(), "weekly") ||
				uni_stri_eq(Parser().ElementContent(), "monthly") ||
				uni_stri_eq(Parser().ElementContent(), "yearly"))
			{
				frequency = 10080; // Maximum of one week.
			}
			else
				frequency = uni_atoi(Parser().ElementContent()); // TTL

			if (frequency != 0)
			{
				Parser().Feed()->SetMinUpdateInterval(frequency);
				Parser().Feed()->SetUpdateInterval(frequency);
			}
		}
	}
	else if (RSSFormat::IsUpdateFrequency(qualified_name))
	{
		if (Parser().ElementContent())
		{
			UINT32 frequency = uni_atoi(Parser().ElementContent());
			if (frequency > 0)
			{
				UINT time_to_live = Parser().Feed()->GetUpdateInterval();
				Parser().Feed()->SetMinUpdateInterval(time_to_live / frequency);
				Parser().Feed()->SetUpdateInterval(time_to_live / frequency);
			}
		}
	}
	else if (RSSFormat::IsPublicationDate(qualified_name))
	{
//		RETURN_IF_ERROR(Parser().Feed()->SetDateCreated(Parser().ElementContent()));
	}
	else if (RSSFormat::IsChannel(qualified_name))
		next_state = StateRSSRoot;

	return OpStatus::OK;
}


// ***************************************************************************
//
//	RSSItemParseState
//
// ***************************************************************************

RSSItemParseState::RSSItemParseState(WebFeedParser& parser)
:	RSSParseStateBase(parser),
	m_guid_is_permalink(NO),
	m_have_good_author_info(FALSE),
	m_have_xml_content(FALSE)
{
}


OP_STATUS RSSItemParseState::Init()
{
	// Create an entry that we'll add to the feed when it's complete.
	RETURN_IF_ERROR(Parser().CreateEntry());
	return OpStatus::OK;
}


OP_STATUS RSSItemParseState::OnStartElement(const OpStringC& namespace_uri,
	const OpStringC& local_name, const OpStringC& qualified_name,
	OpAutoStringHashTable<XMLAttributeQN> &attributes, StateType& next_state)
{
	if (RSSFormat::IsTitle(qualified_name))
	{
		WebFeedContentElement* title = (WebFeedContentElement*)Parser().Entry()->GetTitle();
		RETURN_IF_ERROR(UpdateContentElement(*title, namespace_uri, local_name, qualified_name, attributes));
	}
	else if (RSSFormat::IsGUID(qualified_name))
	{
		// Check if the guid is a permalink or not.
		// we consider it a permalink unless the permaLink-attribute is set to false,
		// but need to distinguish between which is just default, and one which is set to true
		m_guid_is_permalink = MAYBE;
		if (attributes.Contains(UNI_L("isPermaLink")))
		{
			XMLAttributeQN* attribute = NULL;
			RETURN_IF_ERROR(attributes.GetData(UNI_L("isPermaLink"), &attribute));

			OP_ASSERT(attribute);
			const OpStringC& value = attribute->Value();

			if ((value.CompareI("false") == 0) || (value.Compare("0") == 0))
				m_guid_is_permalink = NO;
			else if ((value.CompareI("true") == 0) || (value.Compare("1") == 0))
				m_guid_is_permalink = YES;
		}
	}
	else if (RSSFormat::IsDescription(qualified_name))
	{
		WebFeedContentElement* content = (WebFeedContentElement*)Parser().Entry()->GetContent();
		if (!content->HasType())
			RETURN_IF_ERROR(UpdateContentElement(*content, namespace_uri, local_name, qualified_name, attributes));
	}
	else if (RSSFormat::IsEncodedContent(qualified_name))
	{
		if (!m_have_xml_content)
		{
			WebFeedContentElement* content = (WebFeedContentElement*)Parser().Entry()->GetContent();
			RETURN_IF_ERROR(UpdateContentElement(*content, namespace_uri, local_name, qualified_name, attributes));
		}
	}
	else if (RSSFormat::IsXMLContent(qualified_name))
	{
		WebFeedContentElement* content = (WebFeedContentElement*)Parser().Entry()->GetContent();

		RETURN_IF_ERROR(UpdateContentElement(*content, namespace_uri, local_name, qualified_name, attributes));
		m_have_xml_content = TRUE;
	}
	else if (RSSFormat::IsEnclosure(qualified_name))
	{
		WebFeedLinkElement* enclosure = NULL;
		RETURN_IF_ERROR(Parser().Entry()->AddEnclosureLink(enclosure));

		// An enclosure must contain 'url', 'length' and 'type'.
		XMLAttributeQN* url_attribute = NULL;
		OpStatus::Ignore(attributes.GetData(UNI_L("url"), &url_attribute));

		if (url_attribute)
			RETURN_IF_ERROR(Parser().ResolveAndUpdateLink(*enclosure, url_attribute->Value()));

		XMLAttributeQN* type_attribute = NULL;
		OpStatus::Ignore(attributes.GetData(UNI_L("type"), &type_attribute));

		if (type_attribute)
			RETURN_IF_ERROR(enclosure->SetType(type_attribute->Value()));
	}

	return OpStatus::OK;
}


OP_STATUS RSSItemParseState::OnEndElement(const OpStringC& namespace_uri,
	const OpStringC& local_name, const OpStringC& qualified_name,
	StateType& next_state)
{
	// This adds the content to the element just ended.

	if (RSSFormat::IsTitle(qualified_name))
	{
		WebFeedContentElement* title = (WebFeedContentElement*)Parser().Entry()->GetTitle();
		RETURN_IF_ERROR(UpdateContentElementValue(*title));
	}
	else if (RSSFormat::IsLink(qualified_name))
	{
		// Don't add a link if we allready have a guid used as link.
		if (m_guid_is_permalink != YES)
		{
			WebFeedLinkElement* link = NULL;
			RETURN_IF_ERROR(ItemLink(link));
			
			// TODO: check that we haven't saved as id
			if (!link->HasURI())
				RETURN_IF_ERROR(Parser().ResolveAndUpdateLink(*link, Parser().ElementContent()));
		}
	}
	else if (RSSFormat::IsAuthorName(qualified_name) ||
		RSSFormat::IsAuthorEmail(qualified_name))
	{
		WebFeedPersonElement* author = Parser().Entry()->AuthorElement();
		RETURN_IF_ERROR(UpdateAuthorInfo(*author));

		m_have_good_author_info = TRUE;
	}
	else if (RSSFormat::IsWebmasterEmail(qualified_name))
	{
		// Only use the webmaster information if we don't have good author
		// information allready.
		if (!m_have_good_author_info)
		{
			WebFeedPersonElement* author = Parser().Entry()->AuthorElement();
			RETURN_IF_ERROR(UpdateAuthorInfo(*author));
		}
	}
	else if (RSSFormat::IsSource(qualified_name))
	{
		// Do we have a valid author name?
		WebFeedPersonElement* author = Parser().Entry()->AuthorElement();
		if (!author->HasName())
		{
			// Use the source, Luke.
			RETURN_IF_ERROR(UpdateAuthorInfo(*author));
		}
	}
	else if (RSSFormat::IsGUID(qualified_name))
	{
		RETURN_IF_ERROR(Parser().Entry()->SetGuid(Parser().ElementContent()));

		// use link if we have one unless the guid has the permaLink-attribute set to yes
		if (m_guid_is_permalink == YES || (m_guid_is_permalink == MAYBE && Parser().Entry()->GetPrimaryLink().IsEmpty()))
		{
			WebFeedLinkElement* link = 0;
			RETURN_IF_ERROR(ItemLink(link));

			RETURN_IF_ERROR(Parser().ResolveAndUpdateLink(*link, Parser().ElementContent()));
		}
	}
	else if (RSSFormat::IsPublicationDate(qualified_name))
	{
		// try first to parse as RFC3339, some feeds use that format, and the parser is very strict
		double time = OpDate::ParseRFC3339Date(Parser().ElementContent());
		if (!op_isnan(time))
			Parser().Entry()->SetPublicationDate(time);
		else
		{
			// try the general date parser
			double time = OpDate::ParseDate(Parser().ElementContent());
			if (!op_isnan(time))
				Parser().Entry()->SetPublicationDate(time);
		}
	}
	else if (RSSFormat::IsDescription(qualified_name))
	{
		// Set content if we don't allready have content (thus making
		// description the least valuable content element).
		WebFeedContentElement* content = (WebFeedContentElement*)Parser().Entry()->GetContent();
		if (!content->HasValue())
			RETURN_IF_ERROR(UpdateContentElementValue(*content));
	}
	else if (RSSFormat::IsEncodedContent(qualified_name))
	{
		// Set this type of content if we don't have xml content.
		if (!m_have_xml_content)
		{
			WebFeedContentElement* content = (WebFeedContentElement*)Parser().Entry()->GetContent();
			RETURN_IF_ERROR(UpdateContentElementValue(*content));
		}
	}
	else if (RSSFormat::IsXMLContent(qualified_name))
	{
		// Always set this type of content.
		WebFeedContentElement* content = (WebFeedContentElement*)Parser().Entry()->GetContent();
		RETURN_IF_ERROR(UpdateContentElementValue(*content));
	}
	else if (RSSFormat::IsItem(qualified_name))
	{
		// Next state depends on whether this feed has items outside of the
		// channel or not.
		if (Context().AsRSSContext().ItemsOutsideChannel())
			next_state = StateRSSRoot;
		else
			next_state = StateRSSChannel;

		// Add the entry we have to the list of feed items.
		RETURN_IF_ERROR(Parser().AddEntryToFeed());
	}

	return OpStatus::OK;
}


OP_STATUS RSSItemParseState::ItemLink(WebFeedLinkElement*& link)
{
	// RSS feeds only have one link element, accessed through this function
	// instead of going via the more complicated interface in WebFeedEntry.
	
	RETURN_IF_ERROR(Parser().Entry()->SetAlternateLink(link));

	return OpStatus::OK;
}

// ***************************************************************************
//
//	RSSImageParseState
//
// ***************************************************************************

RSSImageParseState::RSSImageParseState(WebFeedParser& parser, StateType previous_state) : 
    RSSParseStateBase(parser), 
    m_previous_state(previous_state) 
{
}


OP_STATUS RSSImageParseState::OnEndElement(const OpStringC& namespace_uri, const OpStringC& local_name, const OpStringC& qualified_name, StateType& next_state)
{
	if (RSSFormat::IsImage(qualified_name))
		next_state = m_previous_state;

	return OpStatus::OK;
}

// ***************************************************************************
//
//	AtomParseStateBase
//
// ***************************************************************************

AtomParseStateBase::AtomParseStateBase(WebFeedParser& parser)
:	WebFeedParseState(parser),
	m_content_mode(ContentUnknown)
{
}


OP_STATUS AtomParseStateBase::UpdateContentConstruct(WebFeedContentElement& element,
	const OpStringC& namespace_uri, const OpStringC& local_name,
	const OpStringC& qualified_name, OpAutoStringHashTable<XMLAttributeQN>& attributes)
{
	// Ignore <content> with the attribute src (we don't load the content from that
	// source anyway, so).
	if (AtomFormat::IsContent(qualified_name) && attributes.Contains(UNI_L("src")))
	{
		m_content_mode = ContentUnknown;
		return OpStatus::OK;
	}

	// All content constructs in Atom may have a "type" attribute. If it is not
	// present, we should assume the media type to be "text/plain".
	if (attributes.Contains(UNI_L("type")))
	{
		XMLAttributeQN* attribute = NULL;
		RETURN_IF_ERROR(attributes.GetData(UNI_L("type"), &attribute));
		
		OP_ASSERT(attribute);

		const uni_char* type_value = attribute->Value().CStr();

		// In Atom 0.3, the type is given directly. For Atom 1.0 we must check
		// for a few special values, that also affects the mode somewhat.
		if (uni_stricmp(type_value, UNI_L("text")) == 0 ||
			uni_stricmp(type_value, UNI_L("text/plain")) == 0)
		{
			RETURN_IF_ERROR(element.SetType(UNI_L("text/plain")));
			m_content_mode = ContentText;
		}
		else if (uni_stricmp(type_value, UNI_L("html")) == 0 ||
			uni_stricmp(type_value, UNI_L("text/html")) == 0)
		{
			RETURN_IF_ERROR(element.SetType(UNI_L("text/html")));
			m_content_mode = ContentEscaped;
		}
		else if (uni_stricmp(type_value, UNI_L("xhtml")) == 0 ||
			(uni_strni_eq_lower(type_value, UNI_L("application/"), 12) && // Starts with "application/"
			uni_stri_eq(type_value + (attribute->Value().Length() - 3), "XML"))) // Ends with "xml"
		{
			RETURN_IF_ERROR(element.SetType(UNI_L("application/xhtml+xml")));
			m_content_mode = ContentXML;
		}
		else
		{
			RETURN_IF_ERROR(element.SetType(attribute->Value()));
			m_content_mode = ContentUnknown;
		}
	}
	else
	{
		RETURN_IF_ERROR(element.SetType(UNI_L("text/plain")));

		// Valid for 1.0 feeds, will be overwritten for 0.3.
		m_content_mode = ContentText;
	}

	// Also determine the mode the content is encoded in. This attribute will
	// normally only be present in Atom 0.3 feeds.
	if (attributes.Contains(UNI_L("mode")))
	{
		XMLAttributeQN* attribute = 0;
		RETURN_IF_ERROR(attributes.GetData(UNI_L("mode"), &attribute));


		OP_ASSERT(attribute != 0);
		const OpStringC& value = attribute->Value();

		if (value.CompareI("xml") == 0)
			m_content_mode = ContentXML;
		else if (value.CompareI("escaped") == 0)
			m_content_mode = ContentEscaped;
		else if (value.CompareI("base64") == 0)
			m_content_mode = ContentBase64;
	}
	else if (Context().AsAtomContext().Version() == AtomFormat::Atom03)
	{
		if (m_content_mode == ContentUnknown)
		{
			// The Atom 0.3 specifications says that the default mode for content
			// constructs is to be considered as "xml". However, as a special hack
			// to support feeds such as http://groovymother.com/atom.xml, we
			// assume that the default mode for anything but the <content> element
			// is "escaped". This is actually what Mark Pilgrim does in Universal
			// Feed Parser as well.
			if (AtomFormat::IsContent(qualified_name))
				m_content_mode = ContentXML;
			else
				m_content_mode = ContentEscaped;
		}
	}

	// We must store tags if the content is xml. We should always ignore the
	// first child tag though.
	if (m_content_mode == ContentXML)
		RETURN_IF_ERROR(Parser().SetKeepTags(namespace_uri, local_name, TRUE));

	RETURN_IF_ERROR(Parser().UpdateContentBaseURIIfNeeded(element));
	
	return OpStatus::OK;
}


OP_STATUS AtomParseStateBase::UpdateContentConstructValue(WebFeedContentElement& element)
{
	// Decide what to to with the content, based on the content mode.
	switch (m_content_mode)
	{	
		case ContentXML :
		case ContentEscaped :  // Unescaping has been done by the xml parser.
		{
			// Just store the content we have gathered.
			RETURN_IF_ERROR(element.SetValue(Parser().ElementContent()));
			break;
		}
		case ContentBase64 :
		{
			// Decode, then store.
			unsigned char* decoded;
			UINT decoded_length;
			RETURN_IF_ERROR(WebFeedUtil::DecodeBase64(Parser().ElementContent(), decoded, decoded_length));
			element.SetValue(decoded, decoded_length);

			break;
		}
		case ContentText :
		{
			// Store the content after stripping it for spaces, newlines etc.
			OpString content;
			RETURN_IF_ERROR(content.Set(Parser().ElementContent()));

			content.Strip(TRUE, TRUE);
			RETURN_IF_ERROR(element.SetValue(content));
			break;
		}
		default:
			OP_ASSERT(!"Unknown Content Mode!");
	}

	return OpStatus::OK;
}


OP_STATUS AtomParseStateBase::UpdateLinkElement(WebFeedLinkElement& link,
	OpAutoStringHashTable<XMLAttributeQN>& attributes)
{
	// All important information about the link will be in the 'href', 'type'
	// and 'title' attributes.
	XMLAttributeQN* title_attribute = NULL;
	OpStatus::Ignore(attributes.GetData(UNI_L("title"), &title_attribute));

	if (title_attribute)
		RETURN_IF_ERROR(link.SetTitle(title_attribute->Value()));

	XMLAttributeQN* type_attribute = NULL;
	OpStatus::Ignore(attributes.GetData(UNI_L("type"), &type_attribute));

	if (type_attribute)
		RETURN_IF_ERROR(link.SetType(type_attribute->Value()));

	XMLAttributeQN* href_attribute = NULL;
	OpStatus::Ignore(attributes.GetData(UNI_L("href"), &href_attribute));

	if (href_attribute)
		RETURN_IF_ERROR(Parser().ResolveAndUpdateLink(link, href_attribute->Value()));

	return OpStatus::OK;
}


// ***************************************************************************
//
//	AtomRootParseState
//
// ***************************************************************************

AtomRootParseState::AtomRootParseState(WebFeedParser& parser)
:	AtomParseStateBase(parser)
{
}


OP_STATUS AtomRootParseState::OnStartElement(const OpStringC& namespace_uri,
	const OpStringC& local_name, const OpStringC& qualified_name,
	OpAutoStringHashTable<XMLAttributeQN> &attributes, StateType& next_state)
{
	if (AtomFormat::IsTitle(qualified_name))
	{
		WebFeedContentElement* title = (WebFeedContentElement*)Parser().Feed()->GetTitle();
		RETURN_IF_ERROR(UpdateContentConstruct(*title, namespace_uri, local_name, qualified_name, attributes));
	}
	else if (AtomFormat::IsLink(qualified_name))
	{
		WebFeedLinkElement* alternate_link = Parser().Feed()->FeedLink();
		RETURN_IF_ERROR(HandleLinkElement(*alternate_link, attributes));
	}
	else if (AtomFormat::IsAuthor(qualified_name))
		next_state = StateAtomAuthor;
	else if (AtomFormat::IsSubtitle(qualified_name))
	{
		WebFeedContentElement* tagline = Parser().Feed()->Tagline();
		RETURN_IF_ERROR(UpdateContentConstruct(*tagline, namespace_uri, local_name, qualified_name, attributes));
	}
	else if (AtomFormat::IsEntry(qualified_name))
		next_state = StateAtomEntry;

	return OpStatus::OK;
}


OP_STATUS AtomRootParseState::OnEndElement(const OpStringC& namespace_uri,
	const OpStringC& local_name, const OpStringC& qualified_name,
	StateType& next_state)
{
	if (AtomFormat::IsTitle(qualified_name))
	{
		WebFeedContentElement* title = (WebFeedContentElement*)Parser().Feed()->GetTitle();
		RETURN_IF_ERROR(UpdateContentConstructValue(*title));
	}
	else if (AtomFormat::IsSubtitle(qualified_name))
	{
		WebFeedContentElement* tagline = Parser().Feed()->Tagline();
		RETURN_IF_ERROR(UpdateContentConstructValue(*tagline));
	}
	else if (AtomFormat::IsIcon(qualified_name))
	{
		Parser().Feed()->SetIcon(Parser().ElementContent());
	}
/*	else if (AtomFormat::IsUpdated(qualified_name))
	{
		RETURN_IF_ERROR(Parser().Feed()->SetDateCreated(Parser().ElementContent()));
	}*/
	else if (AtomFormat::IsFeedElement(qualified_name))
	{
		next_state = StateDone;
	}
	return OpStatus::OK;
}


OP_STATUS AtomRootParseState::HandleLinkElement(WebFeedLinkElement& link,
	OpAutoStringHashTable<XMLAttributeQN>& attributes)
{
	XMLAttributeQN* rel_attribute = NULL;
	RETURN_IF_ERROR(attributes.GetData(UNI_L("rel"), &rel_attribute));

	// We only want the alternate link for the feed itself. For Atom 1.0, the
	// 'self' attribute might be used instead.
	if (rel_attribute && rel_attribute->Value().CompareI("self") == 0)
		RETURN_IF_ERROR(UpdateLinkElement(link, attributes));
	else if (!rel_attribute || rel_attribute->Value().CompareI("alternate") == 0)
		RETURN_IF_ERROR(UpdateLinkElement(link, attributes));

	return OpStatus::OK;
}

// ***************************************************************************
//
//	AtomAuthorParseState
//
// ***************************************************************************

AtomAuthorParseState::AtomAuthorParseState(WebFeedParser& parser,
	WebFeedPersonElement& author, StateType previous_state)
:	AtomParseStateBase(parser),
	m_author(author),
	m_previous_state(previous_state)
{
}


OP_STATUS AtomAuthorParseState::OnEndElement(const OpStringC& namespace_uri,
	const OpStringC& local_name, const OpStringC& qualified_name,
	StateType& next_state)
{
	if (AtomFormat::IsAuthorName(qualified_name))
		RETURN_IF_ERROR(m_author.SetName(Parser().ElementContent()));
	else if (AtomFormat::IsAuthorEmail(qualified_name))
	{
		OpString email;
		OpString name;
		RETURN_IF_ERROR(Parser().ParseAuthorInformation(Parser().ElementContent(), email, name));

		if (email.HasContent())
			RETURN_IF_ERROR(m_author.SetEmail(email));
	}
	else if (AtomFormat::IsAuthor(qualified_name))
		next_state = m_previous_state;

	return OpStatus::OK;
}


// ***************************************************************************
//
//	AtomEntryParseState
//
// ***************************************************************************

AtomEntryParseState::AtomEntryParseState(WebFeedParser& parser)
:	AtomParseStateBase(parser), m_is_in_source_element(FALSE)
{
}


OP_STATUS AtomEntryParseState::Init()
{
	if (!Parser().HasEntry())
		RETURN_IF_ERROR(Parser().CreateEntry());

	return OpStatus::OK;
}


OP_STATUS AtomEntryParseState::OnStartElement(const OpStringC& namespace_uri,
	const OpStringC& local_name, const OpStringC& qualified_name,
	OpAutoStringHashTable<XMLAttributeQN> &attributes, StateType& next_state)
{
	if (m_is_in_source_element)
		return OpStatus::OK;
	
	if (AtomFormat::IsTitle(qualified_name))
	{
		WebFeedContentElement* title = (WebFeedContentElement*)Parser().Entry()->GetTitle();
		RETURN_IF_ERROR(UpdateContentConstruct(*title, namespace_uri, local_name, qualified_name, attributes));
	}
	else if (AtomFormat::IsLink(qualified_name))
	{
		RETURN_IF_ERROR(HandleLinkElement(attributes));
	}
	else if (AtomFormat::IsSummary(qualified_name))
	{
		// We prefer full content, so only update if this is the first content
		// we have seen.
		WebFeedContentElement* content = (WebFeedContentElement*)Parser().Entry()->GetContent();
		if (!content->HasType())
			RETURN_IF_ERROR(UpdateContentConstruct(*content, namespace_uri, local_name, qualified_name, attributes));
	}
	else if (AtomFormat::IsContent(qualified_name))
	{
		WebFeedContentElement* content = (WebFeedContentElement*)Parser().Entry()->GetContent();
		RETURN_IF_ERROR(UpdateContentConstruct(*content, namespace_uri, local_name, qualified_name, attributes));
	}
	else if (AtomFormat::IsAuthor(qualified_name))
		next_state = StateAtomAuthor;
	else if (AtomFormat::IsSource(qualified_name))
		m_is_in_source_element = TRUE;

	return OpStatus::OK;
}


OP_STATUS AtomEntryParseState::OnEndElement(const OpStringC& namespace_uri,
	const OpStringC& local_name, const OpStringC& qualified_name,
	StateType& next_state)
{
	if (AtomFormat::IsSource(qualified_name))
		m_is_in_source_element = FALSE;
	else if (m_is_in_source_element)
		return OpStatus::OK;
	else if (AtomFormat::IsTitle(qualified_name))
	{
		WebFeedContentElement* title = (WebFeedContentElement*)Parser().Entry()->GetTitle();
		RETURN_IF_ERROR(UpdateContentConstructValue(*title));
	}
	else if (AtomFormat::IsSummary(qualified_name))
	{
		// Only update the content if we don't have anything else (if we have
		// anything else it's probably full content, and we prefer that).
		WebFeedContentElement* content = (WebFeedContentElement*)Parser().Entry()->GetContent();
		if (!content->HasValue())
			RETURN_IF_ERROR(UpdateContentConstructValue(*content));
	}
	else if (AtomFormat::IsUpdated(qualified_name))
	{
		// Always update if we have a value for updated.
		double time = OpDate::ParseRFC3339Date(Parser().ElementContent());
		if (!op_isnan(time))
			Parser().Entry()->SetPublicationDate(time);
	}
	else if (AtomFormat::IsPublished(qualified_name))
	{
		// Prefer <updated> in favor of this value.
		if (Parser().Entry()->GetPublicationDate() == 0.0)
		{
			double time = OpDate::ParseRFC3339Date(Parser().ElementContent());
			if (!op_isnan(time))
				Parser().Entry()->SetPublicationDate(time);
		}
	}
	else if (AtomFormat::IsId(qualified_name))
	{
		RETURN_IF_ERROR(Parser().Entry()->SetGuid(Parser().ElementContent()));
	}
	else if (AtomFormat::IsContent(qualified_name))
	{
		// Always store.
		WebFeedContentElement* content = (WebFeedContentElement*)Parser().Entry()->GetContent();
		RETURN_IF_ERROR(UpdateContentConstructValue(*content));
	}
	else if (AtomFormat::IsEntry(qualified_name))
	{
		next_state = StateAtomRoot;
		RETURN_IF_ERROR(Parser().AddEntryToFeed());
	}

	return OpStatus::OK;
}


OP_STATUS AtomEntryParseState::HandleLinkElement(OpAutoStringHashTable<XMLAttributeQN> &attributes)
{
	XMLAttributeQN* rel_attribute = 0;
	OpStatus::Ignore(attributes.GetData(UNI_L("rel"), &rel_attribute));
	
	// Atom 0.3 feeds must have a rel attribute, atom 1.0 assumes that
	// 'alternate' is the default.
	WebFeedLinkElement* new_link = 0;

	if (rel_attribute == 0 || rel_attribute->Value().CompareI("alternate") == 0)
		RETURN_IF_ERROR(Parser().Entry()->AddAlternateLink(new_link));
	else if (rel_attribute->Value().CompareI("related") == 0)
		RETURN_IF_ERROR(Parser().Entry()->AddRelatedLink(new_link));
	else if (rel_attribute->Value().CompareI("via") == 0)
		RETURN_IF_ERROR(Parser().Entry()->AddViaLink(new_link));
	else if (rel_attribute->Value().CompareI("enclosure") == 0)
		RETURN_IF_ERROR(Parser().Entry()->AddEnclosureLink(new_link));

	if (new_link != 0)
		RETURN_IF_ERROR(UpdateLinkElement(*new_link, attributes));

	return OpStatus::OK;
}

#endif  // WEBFEEDS_BACKEND_SUPPORT
