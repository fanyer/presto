/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_MEDIA_H
#define CSS_MEDIA_H

#include "modules/hardcore/mem/mem_man.h"
#include "modules/util/simset.h"
#include "modules/util/tempbuf.h"
#include "modules/style/css_types.h"
#include "modules/style/css_value_types.h"

class CSS;
class FramesDocument;

// Media types

enum CSS_MediaType {
	CSS_MEDIA_TYPE_MASK			= 0x0fff,
	CSS_MEDIA_TYPE_NONE			= 0x0000,
	CSS_MEDIA_TYPE_PRINT		= 0x0001,
	CSS_MEDIA_TYPE_SCREEN		= 0x0002,
	CSS_MEDIA_TYPE_PROJECTION	= 0x0004,
	CSS_MEDIA_TYPE_HANDHELD		= 0x0008,
	CSS_MEDIA_TYPE_SPEECH		= 0x0010,
	CSS_MEDIA_TYPE_TV			= 0x0020,
	CSS_MEDIA_TYPE_UNKNOWN		= 0x0040,
	CSS_MEDIA_TYPE_ALL			= 0x0080
};

inline BOOL CSS_IsPagedMedium(CSS_MediaType medium) { return (medium & (CSS_MEDIA_TYPE_PRINT|CSS_MEDIA_TYPE_PROJECTION|CSS_MEDIA_TYPE_TV)) != 0; }
inline BOOL CSS_IsContinuousMedium(CSS_MediaType medium) { return (medium & (CSS_MEDIA_TYPE_SCREEN|CSS_MEDIA_TYPE_HANDHELD|CSS_MEDIA_TYPE_SPEECH|CSS_MEDIA_TYPE_TV)) != 0; }
inline BOOL CSS_IsVisualMedium(CSS_MediaType medium) { return medium != CSS_MEDIA_TYPE_SPEECH; }

enum CSS_MediaFeature {
	MEDIA_FEATURE_unknown = -1,
	MEDIA_FEATURE_grid,
	MEDIA_FEATURE_scan,
	MEDIA_FEATURE_color,
	MEDIA_FEATURE_width,
	MEDIA_FEATURE_height,
	MEDIA_FEATURE__o_paged,
	MEDIA_FEATURE_max_color,
	MEDIA_FEATURE_max_width,
	MEDIA_FEATURE_min_color,
	MEDIA_FEATURE_min_width,
	MEDIA_FEATURE_view_mode,
	MEDIA_FEATURE_max_height,
	MEDIA_FEATURE_min_height,
	MEDIA_FEATURE_monochrome,
	MEDIA_FEATURE_resolution,
	MEDIA_FEATURE_color_index,
	MEDIA_FEATURE_orientation,
	MEDIA_FEATURE_aspect_ratio,
	MEDIA_FEATURE_device_width,
	MEDIA_FEATURE_device_height,
	MEDIA_FEATURE_max_monochrome,
	MEDIA_FEATURE_max_resolution,
	MEDIA_FEATURE_min_monochrome,
	MEDIA_FEATURE_min_resolution,
	MEDIA_FEATURE__o_widget_mode,
	MEDIA_FEATURE_max_color_index,
	MEDIA_FEATURE_min_color_index,
	MEDIA_FEATURE_max_aspect_ratio,
	MEDIA_FEATURE_max_device_width,
	MEDIA_FEATURE_min_aspect_ratio,
	MEDIA_FEATURE_min_device_width,
	MEDIA_FEATURE_max_device_height,
	MEDIA_FEATURE_min_device_height,
	MEDIA_FEATURE_device_aspect_ratio,
	MEDIA_FEATURE__o_device_pixel_ratio,
	MEDIA_FEATURE_max_device_aspect_ratio,
	MEDIA_FEATURE_min_device_aspect_ratio,
	MEDIA_FEATURE__o_max_device_pixel_ratio,
	MEDIA_FEATURE__o_min_device_pixel_ratio,
	MEDIA_FEATURE_COUNT
};

#define MEDIA_FEATURE_NAME_MAX_SIZE 25

CSS_MediaFeature GetMediaFeature(const uni_char* str, int len);
CSS_MediaType GetMediaType(const uni_char* text, int text_len, BOOL case_sensitive);

/// The return values are from WindowViewMode enum.
int CSS_ValueToWindowViewMode(short keyword);

class CSS_MediaFeatureExpr : public Link
{
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT_L

public:

	CSS_MediaFeatureExpr(CSS_MediaFeature feature)
	{
		m_packed.feature = feature;
		m_packed.has_value = FALSE;
	}

	CSS_MediaFeatureExpr(CSS_MediaFeature feature, short token, float value)
	{
		m_packed.feature = feature;
		m_packed.token = token;
		m_packed.has_value = TRUE;
		m_value.number = value;
	}

	CSS_MediaFeatureExpr(CSS_MediaFeature feature, short token, short ratio_num, short ratio_denom)
	{
		m_packed.feature = feature;
		m_packed.token = token;
		m_packed.has_value = TRUE;
		m_value.ratio.numerator = ratio_num;
		m_value.ratio.denominator = ratio_denom;
	}

	CSS_MediaFeatureExpr(CSS_MediaFeature feature, short token, CSSValue ident)
	{
		m_packed.feature = feature;
		m_packed.token = token;
		m_packed.has_value = TRUE;
		m_value.ident = ident;
	}

	CSS_MediaFeatureExpr(CSS_MediaFeature feature, int integer)
	{
		m_packed.feature = feature;
		m_packed.token = CSS_INT_NUMBER;
		m_packed.has_value = TRUE;
		m_value.integer = integer;
	}

	/** Find out if this media feature expression currently evaluates to true. */
	BOOL Evaluate(FramesDocument* doc) const;

	/** Compare two media feature expressions. */
	BOOL Equals(const CSS_MediaFeatureExpr& expr) const;

	/** Stringify a media feature expression for output in DOM. */
	void AppendFeatureExprStringL(TempBuffer* buf) const;

	OP_STATUS AddQueryLengths(CSS* stylesheet) const;

private:
	struct
	{
		unsigned int feature:6;
		unsigned int has_value:1;
		unsigned int token:16;
	} m_packed;

	union {
		float number;
		struct {
			short numerator;
			short denominator;
		} ratio;
		short ident;
		int integer;
	} m_value;
};

class CSS_MediaQuery : public Link
{
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT_L

public:
	/** Constructor. */
	CSS_MediaQuery(BOOL invert, short type) : m_packed_init(0)
	{
		m_packed.type = type;
		m_packed.invert = invert;
	}

	/** Destructor. Delete all feature expressions. */
	virtual ~CSS_MediaQuery() { m_features.Clear(); }

	/** Return the media type. */
	CSS_MediaType GetMediaType() const { return CSS_MediaType(m_packed.type); }

	/** Add a media feature expression to the query.
		@param expr The feature expression to add. */
	void AddFeatureExpr(CSS_MediaFeatureExpr* expr) { expr->Into(&m_features); }

	/** Compare two media queries.
		@param query Query to compare with.
		@return TRUE if equal. */
	BOOL Equals(const CSS_MediaQuery& query) const;

	/** Evaluate the media query.
		@param doc The document.
		@return The resulting media type. */
	short Evaluate(FramesDocument* doc) const;

	/** Append a string representation of the media query to a TempBuffer.
		@param buf TempBuffer to append string to. */
	void AppendQueryStringL(TempBuffer* buf) const;

	/** Add min/max length feature lengths, for which the media query will
		change its evaluation, to the stylesheet.
		@param stylesheet The stylesheet this media query belongs to.
		@return OK or ERR_NO_MEMORY on OOM. */
	OP_STATUS AddQueryLengths(CSS* stylesheet) const;

private:

	Head m_features;

	union
	{
		struct
		{
			unsigned int type:12;
			unsigned int invert:1;
		} m_packed;
		int m_packed_init;
	};
};

class CSS_MediaObject
{
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT

public:

	/** Destructor. Deletes the media queries. */
	~CSS_MediaObject() { m_queries.Clear(); }

	/** Add a media query to this object. */
	void AddMediaQuery(CSS_MediaQuery* query) { query->Into(&m_queries); }

	/** Remove a media query from the list. */
	BOOL RemoveMediaQuery(const CSS_MediaQuery& query);

	/** Calculate the matching media types from evaluating the media queries.
		The result is a bitset of matching media.
		@param doc The document. Must be non-NULL.
		@return A set of matching media types. */
	short EvaluateMediaTypes(FramesDocument* doc) const;

	/** Return the specified media types of the media queries.
		If a query is preceded by a "not", that medium will still be included. */
	short GetMediaTypes() const;

	/** Append a string representation of the list of media queries to a TempBuffer.
		@param buf The TempBuffer to append to. */
	void GetMediaStringL(TempBuffer* buf) const;

	/** Parse a list of media.
		@param text Source text.
		@param len Number of characters in text.
		@param html_attribute Set to TRUE if the text should be parsed according
			   to the HTML spec for the media attribute */
	CSS_PARSE_STATUS SetMediaText(const uni_char* text, int len, BOOL html_attribute=TRUE);

	/** Return the number of media queries in this media list. */
	unsigned int GetLength() const { return m_queries.Cardinal(); }

	/** Append the media query at the provided index to a buffer.
		@param buf The buffer to append the string to.
		@param index The index of the query. Starts from 0. */
	OP_STATUS GetItemString(TempBuffer* buf, unsigned int index) const;

	/** Parse and add a media query.
		@param text The media query string to append.
		@param len The number of characters in text.
		@param medium_deleted Return status. Set to TRUE if medium was found and
							  deleted before it was added, FALSE otherwise. */
	CSS_PARSE_STATUS AppendMedium(const uni_char* text, int len, BOOL& medium_deleted);

	/** Delete the media query matching this string.
		@param text The media query string.
		@param len The number of characters in text.
		@param medium_deleted Return status. Set to TRUE if medium was found, FALSE otherwise. */
	CSS_PARSE_STATUS DeleteMedium(const uni_char* text, int len, BOOL& medium_deleted);

	/** Check if the media object contains no media queries.
		@return TRUE if empty, FALSE otherwise. */
	BOOL IsEmpty() const { return m_queries.Empty(); }

	/** Called by CSS_Parser to do post-processing after parsing a media query list. */
	void EndMediaQueryListL();

	/** For this media object add the width and height pixel lengths at which the object's
		media queries will change evaluation.

		@param stylesheet The stylesheet to add lengths to. */
	OP_STATUS AddQueryLengths(CSS* stylesheet) const;

private:
	/** Parse a media query string.
		@param text The media query string.
		@param len The number of characters in text.
		@param start_token Start token used to select correct start-rule in the css grammar.
						   For this method, use CSS_TOK_DOM_MEDIUM or CSS_TOK_DOM_MEDIA_LIST.
		@param delete_only If the start_token is CSS_TOK_DOM_MEDIUM, delete_only is set to TRUE
						   for when we're parsing to delete a medium without append it afterwards.
						   delete_only is set to TRUE upon return if a medium was deleted (found),
						   FALSE otherwise. */
	CSS_PARSE_STATUS ParseMedia(const uni_char* text, int len, int start_token, BOOL& delete_only);

	/** Copy the media queries from one media object to another.
		Used in ParseMedia to replace the whole media query list. */
	void SetFrom(CSS_MediaObject* copy_from);

	/** A list of the comma separated media queries in the medium list. */
	Head m_queries;
};

/** Class used by media query lists returned from Window.matchMedia() in DOM. */
class CSS_MediaQueryList
	: public ListElement<CSS_MediaQueryList>
{
public:

	/** Listener interface for the DOM code. */
	class Listener
	{
	public:
		/** The evaluation of a media query list changed.
			@param media_query_list The media query list that changed. */
		virtual void OnChanged(const CSS_MediaQueryList* media_query_list) = 0;

		/** A media query list is about to be deleted because the document it
			is connected to is being deleted.
			@param media_query_list The media query list that is about to be
									deleted. */
		virtual void OnRemove(const CSS_MediaQueryList* media_query_list) = 0;
	};

	/** Constructor.
		@param media_object The object representing the media query list.
							must be non-NULL. Ownership is transferred to this
							object.
		@param listener The object implementing the Listener interface. Must
						be non-NULL.
		@param doc The document the media query list is connected to. Used to
				   evaluate the media query list to initialize the match
				   state. */
	CSS_MediaQueryList(CSS_MediaObject* media_object, Listener* listener, FramesDocument* doc)
		: m_media_object(media_object), m_listener(NULL), m_matches(FALSE)
	{
		OP_ASSERT(media_object);
		OP_ASSERT(listener);
		Evaluate(doc);
		m_listener = listener;
	}

	/** Destructor. Remove the object from the CSSCollection and notify the
		listener. */
	~CSS_MediaQueryList()
	{
		Out();
		m_listener->OnRemove(this);
		OP_DELETE(m_media_object);
	}

	/** @return TRUE if the the media query list currently matches, otherwise FALSE. */
	BOOL Matches() const { return m_matches; }

	/** Append the string serialization of the media query list to a buffer.
		@param buffer The buffer to append the string to.
		@return ERR_NO_MEMORY on OOM, otherwise OK. */
	OP_STATUS GetMedia(TempBuffer* buffer) const
	{
		TRAPD(ret, m_media_object->GetMediaStringL(buffer));
		return ret;
	}

	/** Update the match state of the media query list.
		@param doc The document that gives the context for the media query list
				   evaluation. */
	void Evaluate(FramesDocument* doc);

private:
	/** The media query list. */
	CSS_MediaObject* m_media_object;

	/** The listener object. */
	Listener* m_listener;

	/** The current match state. */
	BOOL m_matches;
};

#endif // CSS_MEDIA_H
