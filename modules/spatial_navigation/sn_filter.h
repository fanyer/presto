/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
**
*/

#ifndef SN_FILTER_H
#define SN_FILTER_H


class ElementTypeCheck;
class HTML_Element;
class FramesDocument;

/**
 * Describes an element which can be spatnav:ed to.
 */
class OpSnElement
{
  public:
	virtual ~OpSnElement() { }

	virtual BOOL IsFormObject() const = 0;
	virtual BOOL IsClickable() const = 0;
	virtual BOOL IsHeading() const = 0;
	virtual BOOL IsParagraph() const = 0;
	virtual BOOL IsImage() const = 0;
	virtual BOOL IsLink() const = 0;
	virtual BOOL IsMailtoLink() const = 0;
	virtual BOOL IsTelLink() const = 0;
	virtual BOOL IsWtaiLink() const = 0;
	virtual BOOL IsObject() const = 0;
	virtual BOOL IsScrollable() const = 0;

#ifdef _PLUGIN_NAVIGATION_SUPPORT_
	virtual BOOL IsNavigatePlugin() const = 0;
#endif
};


/**
 * Navigation Mode filter, to decide wether to accept an element
 * or not for spatial navigation.
 */

class OpSnNavFilter
{
  public:

	enum { MAX_NUMBERS_OF_FILTERS = 7 };

	virtual ~OpSnNavFilter() { }

	/* Accept this element for spatial navigation? */
	virtual BOOL AcceptElement(HTML_Element* element, FramesDocument* doc) = 0;

	/* Return TRUE if the navigated links shall be selected
	 * in Picker mode , that is:
	 *  When activated they shall be selected as by right-click
	 *  rather than activated as by left-click/enter/space
	 */
	virtual BOOL IsPickerMode () { return FALSE; };
	virtual BOOL AcceptsPlugins() { return FALSE; }
};

/**
 * Default navigation filter, which navigates to all clickable things
 */

class SnDefaultNavFilter : public OpSnNavFilter
{
  public:
	virtual ~SnDefaultNavFilter() { }

	virtual BOOL AcceptElement(HTML_Element* element, FramesDocument* doc);  ///< @override OpSnNavFilter
	virtual BOOL AcceptsPlugins() { return TRUE; }  ///< @override OpSnNavFilter
};

/*
 * Special navigation filters for:
 * - Headings
 * - Paragraphs
 * - Forms
 * - ATags
 * - Images
 */

/**
 * Filter which accepts headings (h1, h2 etc)
 */
class SnHeadingNavFilter : public OpSnNavFilter
{
	public:
	virtual BOOL AcceptElement(HTML_Element* element, FramesDocument* doc);  ///< @override OpSnNavFilter
};

/**
 * Filter which should accept paragraphs, currently accepts nothing
 */
class SnParagraphNavFilter : public OpSnNavFilter
{
	public:
	virtual BOOL AcceptElement(HTML_Element* element, FramesDocument* doc);  ///< @override OpSnNavFilter
};

/**
 * Filter which accepts (enabeled) form elements
 */
class SnFormNavFilter : public OpSnNavFilter
{
	public:
	virtual BOOL AcceptElement(HTML_Element* element, FramesDocument* doc);  ///< @override OpSnNavFilter
};

/**
 * Filter which accepts regular links.  I.e. a-elements with a href attribute
 * Also accepts WML links in WML documents
 */
class SnATagNavFilter : public OpSnNavFilter
{
	public:
	virtual BOOL AcceptElement(HTML_Element* element, FramesDocument* doc);  ///< @override OpSnNavFilter
};

/**
 * Filter which accepts image elements (has img tag)
 */
class SnImageNavFilter : public OpSnNavFilter
{
	public:
	virtual BOOL AcceptElement(HTML_Element* element, FramesDocument* doc);  ///< @override OpSnNavFilter
};

/**
 * Filter which selects all elements which are not links. I.e. the oposite 
 * of SnLinkNavFilter
 */
class SnNonLinkNavFilter : public OpSnNavFilter
{
	public:
	virtual BOOL AcceptElement(HTML_Element* element, FramesDocument* doc);  ///< @override OpSnNavFilter
};

/**
 * Configurable filter where you can make a new filter by combining any other 
 * filter
 */
class SnConfigurableNavFilter : public OpSnNavFilter
{
	public:
	SnConfigurableNavFilter() : accepts_plugins(FALSE) {}
	
	/**
	 * Sets the list of filters to use.  This filter will accept any element
	 * which is accepted by any of the filters in the array.  So it acts like
	 * an or operation of the other filters.
	 * @param filter_array an array pointers to all the filters to use.  Last
	 *        element in array must be a NULL pointer, and it must contain no
	 *        more than MAX_NUMBERS_OF_FILTERS filters (not including the NULL 
	 * 		  pointer at the end)
	 */
	void SetFilterArray(OpSnNavFilter* const * filter_array);
	virtual BOOL AcceptElement(HTML_Element* element, FramesDocument* doc);  ///< @override OpSnNavFilter
	virtual BOOL AcceptsPlugins() { return accepts_plugins; }  ///< @override OpSnNavFilter

	private:
	OpSnNavFilter* filters[MAX_NUMBERS_OF_FILTERS+1];
	BOOL accepts_plugins;
};

/**
 * The picker mode filter accepts links, images, mailto-links, tel-links, 
 * objects and wtai-links.
 * Useful for platforms where you want to be able to use spatnav for more than
 * just navigating, but also for e.g. opening context menus over elements
 */
class SnPickerModeFilter : public OpSnNavFilter
{
	public:
	virtual BOOL AcceptElement(HTML_Element* element, FramesDocument* doc);  ///< @override OpSnNavFilter
	virtual const char* Name() { return "Picker mode filter"; };
	virtual BOOL IsPickerMode() { return TRUE; };  ///< @override OpSnNavFilter
};

#endif // SN_FILTER_H
