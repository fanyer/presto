// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef _WEBFEEDENTRY_H_
#define _WEBFEEDENTRY_H_

#ifdef WEBFEEDS_BACKEND_SUPPORT

#include "modules/webfeeds/webfeeds_api.h"
#include "modules/webfeeds/src/webfeedutil.h"

class URL;
class WebFeedEntry;
class WebFeed;
class WriteBuffer;
class WebFeedContentElement;
class DOM_Environment;


// ***************************************************************************
//
//	WebFeedPropElm
//
// ***************************************************************************

/// Holds a reference to the content of a special property
class WebFeedPropElm : public Link
{
public:
	~WebFeedPropElm();

	OP_STATUS	Init(const uni_char* property_name, WebFeedContentElement* content);

	BOOL		IsEqual(const uni_char* property_name) { return uni_strcmp(property_name, m_prop) == 0; }

	WebFeedContentElement*	GetContent() { return m_content; }
	void					SetContent(WebFeedContentElement* new_content);
	const uni_char*			GetName() { return m_prop; }

private:
	WebFeedContentElement*	m_content;
	uni_char*				m_prop;
};


// ***************************************************************************
//
//	WebFeedContentElement
//
// ***************************************************************************

/**
\brief Class for storing the content of a feed element.
 *
 * Has methods for setting and getting the content and MIME type of content.
 * Setting content will be done from parser, it should probably not be done from anywhere else.
 *
 * The content may be binary in Atom (transfered as base 64), so the value is stored internally as a
 * unsigned char buffer. But has methods for setting and returning it as an
 * OpString. */

class WebFeedContentElement : public OpFeedContent
{
protected:
	~WebFeedContentElement(); // Use DecRef when done with object, don't delete
public:
	// Construction.
	WebFeedContentElement() : m_value(NULL), m_value_length(0), m_reference_count(1) {}
	// Methods.
	/** Increment the reference counter */
	OpFeedContent*	IncRef();
	/** Decrement the reference counter */
	void			DecRef();

	/** Get the DOM object associated with this ContentElement */
	ES_Object* 	GetDOMObject(DOM_Environment*);
	/** Set the DOM object associated with this ContentElement */
	void			SetDOMObject(ES_Object *dom_obj, DOM_Environment*);

	/** Set value from an OpString. */
	OP_STATUS SetValue(const OpStringC& value);
	
	/** Set value from a byte buffer. Takes over ownership of the buffer, 
	 *   which should be dynamicly allocated (it will be deleted).
	 * 		\param data buffer to take over
	 * 		\param data_length length (in bytes) of the data buffer */
	void SetValue(unsigned char* data, UINT data_length);
	void SetValue(uni_char* data, UINT data_length);

	/** Set MIME type of content
	 * 		\param type MIME type in an OpString */
	OP_STATUS SetType(const OpStringC& type) { return m_type.Set(type); }
	 
	OP_STATUS SetBaseURI(const OpStringC& base_uri) { return m_base_uri.Set(base_uri); }

	/** Returns the content as a uni_char string. Only a valid return if
	 * 	IsBinary() returns FALSE */
	const uni_char* Data() const;
	/** Returns content as a buffer and length. Useful when content is binary.
	 *  Use MIME_Type to figure out what to do with data */
	void Data(const unsigned char*& buffer, UINT& buffer_length) const;

#ifdef OLD_FEED_DISPLAY
#ifdef WEBFEEDS_DISPLAY_SUPPORT
	void WriteAsHTMLL(URL& out_url) const;
	void WriteAsStrippedHTMLL(URL& out_url) const;
#endif // WEBFEEDS_DISPLAY_SUPPORT
#endif // OLD_FEED_DISPLAY

	/** \return MIME type of content as a string */
	const uni_char* MIME_Type() const;

	const uni_char* GetBaseURI() const;

	BOOL HasValue() const { return m_value_length != 0; }
	/** A MIME type has been set for this content */
	BOOL HasType() const { return m_type.HasContent(); }
	BOOL HasBaseURI() const { return m_base_uri.HasContent(); }

	/** \return MIME type is of plain text type */
	BOOL IsPlainText() const;
	/** \return MIME type is of HTML or XHTML type */
	BOOL IsMarkup() const;
	/** \return MIME type is of valid XML (XHTML) type */
	BOOL IsXML() const;
	/** \return Has a MIME type which is not plain or markup text */
	BOOL IsBinary() const;

#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
	UINT 		GetApproximateSaveSize();  // Approximate size used for saving this entry (in characters)
	void		SaveL(WriteBuffer &buf, BOOL force_tags);
#endif // WEBFEEDS_SAVED_STORE_SUPPORT

private:
	// No copy or assignment.
	WebFeedContentElement(const WebFeedContentElement& other);
	WebFeedContentElement& operator=(const WebFeedContentElement& other);

	// Members.
	unsigned char* m_value;   // buffer containing data
	UINT m_value_length;      // length of buffer in bytes (note that each uni_char takes two bytes)
	OpString m_type;
	OpString m_base_uri;
	UINT m_reference_count;		///< Number of external objects referencing this one
	EnvToESObjectHash  m_dom_objects;
};


// ***************************************************************************
//
//	WebFeedPersonElement
//
// ***************************************************************************

/**
\brief Class for name and e-mail about a person. Used for author of entries. */
class WebFeedPersonElement
{
public:
	// Construction.
	WebFeedPersonElement() {}

	// Methods.
	OP_STATUS SetName(const OpStringC& name);
	OP_STATUS SetEmail(const OpStringC& email) { return m_email.Set(email); }

	const OpStringC& Name() const { return m_name; }
	const OpStringC& Email() const { return m_email; }

	BOOL HasName() const { return m_name.HasContent(); }
	BOOL HasEmail() const { return m_email.HasContent(); }

private:
	// No copy or assignment.
	WebFeedPersonElement(const WebFeedPersonElement& other);
	WebFeedPersonElement& operator=(const WebFeedPersonElement& other);

	// Members.
	OpString m_name;
	OpString m_email;
};


// ***************************************************************************
//
//	WebFeedLinkElement
//
// ***************************************************************************

class WebFeedLinkElement : public OpFeedLinkElement
{
public:
	// Construction.
	WebFeedLinkElement(LinkRel relationship) : m_relationship(relationship) {}

	// Methods.
	OP_STATUS SetTitle(const OpStringC& title) { return m_title.Set(title); }
	OP_STATUS SetType(const OpStringC& type) { return m_type.Set(type); }
	void SetURI(URL& uri) { m_uri = uri; }
	void SetURI(const OpStringC& uri) { m_uri = g_url_api->GetURL(uri); }

	const OpStringC& Title() { return m_title; }
	const OpStringC& Type() { return m_type; }
	URL& URI() { return m_uri; }
	LinkRel Relationship() { return m_relationship; }

	BOOL HasTitle() { return m_title.HasContent(); }
	BOOL HasType() { return m_type.HasContent(); }
	BOOL HasURI() { return !m_uri.IsEmpty(); }

private:
	// No copy or assignment.
	WebFeedLinkElement(const WebFeedLinkElement& other);
	WebFeedLinkElement& operator=(const WebFeedLinkElement& other);

	// Members.
	OpString m_title;
	OpString m_type;
	URL m_uri;
	LinkRel m_relationship;
};



// ***************************************************************************
//
//	WebFeedEntry
//
// ***************************************************************************

/**
\brief Class for storing a feed entry. */
class WebFeedEntry : public OpFeedEntry, public Link
{
	friend class WebFeed; // So that it can delete this object
protected:
	virtual ~WebFeedEntry(); // Use MarkForRemoval(), only WebFeed objects should do any deleting when they're sure no one references it

public:
	WebFeedEntry(WebFeed* parent_feed = NULL);
	OP_STATUS		Init();
	
	OpFeedEntry*	IncRef();
	void			DecRef();
	void			MarkForRemoval();
	BOOL			MarkedForRemoval() { return m_mark_for_removal; }

	ES_Object*		GetDOMObject(DOM_Environment*);
	void			SetDOMObject(ES_Object *dom_obj, DOM_Environment*);

	EntryId			GetId();
	void			SetId(EntryId id);
#ifdef OLD_FEED_DISPLAY
	OP_STATUS		GetEntryIdURL(OpString&);
#endif // OLD_FEED_DISPLAY

	const uni_char* GetGuid();
	OP_STATUS		SetGuid(const uni_char* guid);

	ReadStatus 		GetReadStatus();
	void 			SetReadStatus(ReadStatus status);

	OpFeedEntry*	GetPrevious(BOOL unread_only=FALSE);
	OpFeedEntry*	GetNext(BOOL unread_only=FALSE);

	OpFeed*			GetFeed();
	void			SetFeed(WebFeed* feed) { OP_ASSERT(m_feed == feed || m_feed == NULL); m_feed = feed; }

	BOOL			GetKeep();
	void			SetKeep(BOOL keep);

#ifdef WEBFEEDS_DISPLAY_SUPPORT
# ifdef OLD_FEED_DISPLAY
	OP_STATUS	WriteEntry(URL& out_url, BOOL complete_document=TRUE, BOOL mark_read=TRUE);
	void		WriteEntryL(URL& out_url, BOOL complete_document=TRUE, BOOL mark_read=TRUE);
# endif // OLD_FEED_DISPLAY
#endif // WEBFEEDS_DISPLAY_SUPPORT

	OpFeedContent*	GetTitle();
	/// Does NOT transfer ownership, caller must still do DecRef when done with pointer
	void			SetTitle(WebFeedContentElement* title);
	const uni_char*	GetAuthor();
	const uni_char*	GetAuthorEmail();
	WebFeedPersonElement*
					AuthorElement();
	OpFeedContent*	GetContent();
	/// Does NOT transfer ownership, caller must still do DecRef when done with pointer
	void			SetContent(WebFeedContentElement* content);
	const uni_char*	GetProperty(const uni_char* property);
	OpFeedContent*	GetPropertyContent(const uni_char* property);
	/// Does NOT transfer ownership, caller must still do DecRef when done with pointer
	OP_STATUS		AddProperty(const uni_char* property, WebFeedContentElement* prop);

	double			GetPublicationDate();
	void			SetPublicationDate(double date);

#ifdef WEBFEEDS_SAVED_STORE_SUPPORT
	UINT 		GetApproximateSaveSize();  // Approximate size used for saving this entry (in characters)
	void		SaveL(WriteBuffer &buf);
#endif // WEBFEEDS_SAVED_STORE_SUPPORT
	
	/** Add new Link to entry. Caller will still have to fill link with useful content (URI, title etc. )
	 *  \param new_link[out] newly created link object if return is OpStatus::OK */
	OP_STATUS	AddNewLink(WebFeedLinkElement::LinkRel relationship, WebFeedLinkElement*& new_link);
	OP_STATUS	AddAlternateLink(WebFeedLinkElement*& alternate_link) { return AddNewLink(WebFeedLinkElement::Alternate, alternate_link); }
	OP_STATUS   SetAlternateLink(WebFeedLinkElement*& alternate_link);
	OP_STATUS	AddRelatedLink(WebFeedLinkElement*& related_link) { return AddNewLink(WebFeedLinkElement::Related, related_link); }
	OP_STATUS	AddViaLink(WebFeedLinkElement*& via_link) { return AddNewLink(WebFeedLinkElement::Via, via_link); }
	OP_STATUS	AddEnclosureLink(WebFeedLinkElement*& enclosure_link) { return AddNewLink(WebFeedLinkElement::Enclosure, enclosure_link); }
	/** Number of links
	 * 		\return number of links of given type. */
	unsigned	LinkCount() const;
	/** Return link at index */
	OpFeedLinkElement* GetLink(UINT32 index) const;
	/** Return primary link adress (URL where entry might be seen as HTML */
	URL					GetPrimaryLink();

	/** Prefetches (caches) the URL given by primary link (if any) */
	OP_STATUS 			PrefetchPrimaryLink();

protected:
	// No copy or assignment.
	WebFeedEntry(const WebFeedEntry& other);
	WebFeedEntry& operator=(const WebFeedEntry& other);

	WebFeedPropElm*	GetPropEntry(const uni_char* prop);
#ifdef WEBFEEDS_DISPLAY_SUPPORT
# ifdef OLD_FEED_DISPLAY
	void WriteNavigationHeaderL(URL& out_url);
# endif // OLD_FEED_DISPLAY
#endif // WEBFEEDS_DISPLAY_SUPPORT

	// Members.
	UINT 					m_reference_count;

	EnvToESObjectHash  m_dom_objects;

	WebFeedContentElement*	m_title;
	WebFeedPersonElement*	m_author;
	WebFeedContentElement*	m_content;
	OpAutoVector<WebFeedLinkElement> m_links;

	WebFeed*	m_feed;
	EntryId		m_id;
	OpString	m_guid;
	BOOL		m_keep_item;
	BOOL		m_mark_for_removal;
	ReadStatus	m_status;
	double		m_pubdate;

	AutoDeleteHead*	m_properties;
};

#endif // WEBFEEDS_BACKEND_SUPPORT
#endif //_WEBFEEDENTRY_H_
