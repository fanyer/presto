/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_VIEWPORT_META_H
#define CSS_VIEWPORT_META_H

#ifdef CSS_VIEWPORT_SUPPORT

#include "modules/logdoc/complex_attr.h"
#include "modules/style/css_collection.h"
#include "modules/style/css_viewport.h"

/** width/height clamping range. */
#define VIEWPORT_META_LENGTH_MIN 1
#define VIEWPORT_META_LENGTH_MAX 10000

/** min/max/initial-scale clamping range. */
#define VIEWPORT_META_SCALE_MIN 0.1
#define VIEWPORT_META_SCALE_MAX 10

/** Class for representing viewport properties from the content attribute
	of a viewport*/

class CSS_ViewportMeta : public ComplexAttr, public CSS_ViewportDefinition, public CSSCollectionElement
{
public:

	/** Enumerated type for user-scalable values. */
	enum UserScalable
	{
		Unset,
		Yes,
		No,
	};

	CSS_ViewportMeta(HTML_Element* elm) : m_html_element(elm) { Reset(); }

	/** Reset to initial property values. */

	void Reset()
	{
		m_width = CSS_ViewportLength();
		m_height = CSS_ViewportLength();
		m_init_scale = CSS_VIEWPORT_ZOOM_AUTO;
		m_min_scale = CSS_VIEWPORT_ZOOM_AUTO;
		m_max_scale = CSS_VIEWPORT_ZOOM_AUTO;
		m_user_scalable = Unset;
	}

	/** Parse the content attribute of the viewport meta element and set the
		member values accordingly. Before parsing the members are set to their
		initial values.

		@param content The content attribute value string. NULL and empty
					   string will reset the members to their initial values. */

	void ParseContent(const uni_char* content);

	/** From the CSS_ViewportDefinition interface. */

	virtual void AddViewportProperties(CSS_Viewport* viewport);

	/** From ComplexAttr interface. */

	virtual OP_STATUS CreateCopy(ComplexAttr** copy_to);

	/** From CSSCollectionElement interface. */

	virtual Type GetType() { return VIEWPORT_META; }
	virtual HTML_Element* GetHtmlElement() { return m_html_element; }
	virtual void Added(CSSCollection* collection, FramesDocument* doc) { collection->StyleChanged(CSSCollection::CHANGED_VIEWPORT); }
	virtual void Removed(CSSCollection* collection, FramesDocument* doc) { collection->StyleChanged(CSSCollection::CHANGED_VIEWPORT); }

private:

	/** Copy constructor. Needed by CreateCopy method.

		@param copy The object to create a copy from. */

	CSS_ViewportMeta(const CSS_ViewportMeta& copy) : m_html_element(NULL)
	{
		m_width = copy.m_width;
		m_height = copy.m_height;
		m_min_scale = copy.m_min_scale;
		m_max_scale = copy.m_max_scale;
		m_init_scale = copy.m_init_scale;
		m_user_scalable = copy.m_user_scalable;
	}

	/** Parse a single property in the content string.

		@param content The start of the input string. After a property/value pair is parsed,
					   the content pointer is updated upon return. */

	void ParseProperty(const uni_char*& content);

	/** Pointer to the META element, to be used by the CSSCollection to get cascading order. */
	HTML_Element* m_html_element;

	/** The parsed width. */
	CSS_ViewportLength m_width;

	/** The parsed height. */
	CSS_ViewportLength m_height;

	/** The parsed initial-scale. */
	double m_init_scale;

	/** The parsed minimum-scale. */
	double m_min_scale;

	/** The parsed maximum-scale. */
	double m_max_scale;

	/** The parsed user-scalable. */
	UserScalable m_user_scalable;
};

#endif // CSS_VIEWPORT_SUPPORT

#endif // CSS_VIEWPORT_META_H
