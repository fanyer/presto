/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/** @file element_search.h
 *
 * interface needed for document element searching
 *
 */

#ifndef LAYOUT_ELEMENT_SEARCH_H
#define LAYOUT_ELEMENT_SEARCH_H

/** Instances of this class are used to customize the behavior of element searching
	In particular it provides a method for filtering which elements to accept and which not.
	See LayoutWorkplace::SearchForElements. */

class ElementSearchCustomizer
{
public:

	/** Constructor. Sets several flags that will affect the behavior of element searching.
	 * @param accept_scrollbars when TRUE, scrollbars will be reported as found elements/objects
	 * @param check_for_overlapped_elements when TRUE, the element searching will care for the situations, where an element can be
	 *			visually covered by other, effectively discarding the covered ones.
	 *			NOTE: This flag ensures, that areas corresponding to a layout box will be considered visually covering basing only
	 *			on it's background, content, borders and not the opacity property.
	 * @param care_for_opacity When check_for_overlapped_elements is TRUE and this one is TRUE, elements,
	 *			that have opacity < 1, will not be considered as visually covering the beneath content.
	 *			This flag must not be TRUE, when check_for_overlapped_elements is FALSE. */

	 ElementSearchCustomizer(BOOL accept_scrollbars, BOOL check_for_overlapped_elements, BOOL care_for_opacity)
		: accept_scrollbar(accept_scrollbars),
		  notify_overlapping_rects(check_for_overlapped_elements),
		  care_for_opacity(care_for_opacity) {}

	/** Should we accept the given element? */

	virtual BOOL AcceptElement(HTML_Element* elm, FramesDocument* doc) = 0;

private:
	friend class ElementSearchObject;

	BOOL		accept_scrollbar;
	BOOL		notify_overlapping_rects;
	BOOL		care_for_opacity;
};

enum DocumentElementType
{
	DOCUMENT_ELEMENT_TYPE_HTML_ELEMENT,
	DOCUMENT_ELEMENT_TYPE_VERTICAL_SCROLLBAR,
	DOCUMENT_ELEMENT_TYPE_HORIZONTAL_SCROLLBAR
};

/** Simple structure to store one parallelogram of one Element's region.
	Consists of a rect in local coordinates of a transform context and a pointer to a transformation/translation
	to the document root coordinates. In particular the rect may be in document root coordinates.
	In such case affine_pos is either (0, 0) translation or NULL. */

struct OpTransformedRect
{
	OpRect rect;
	const AffinePos* affine_pos;
};

typedef OpAutoVector<OpTransformedRect> DocumentElementRegion;

/**	One element, stores it's data.
	The instances of this class are read-only. They are passed in the output
	of LayoutWorkplace::SearchForElements. Any copying, constructing and assigning
	is not possible. */

class OpDocumentElement
{
public:
	HTML_Element* GetHtmlElement() const {return elm;}
	DocumentElementType GetType() const {return type;}

	/** Gets the DocumentElementRegion.
		NOTE: The OpTransformedRect objects stored in DocumentElementRegion do not own the AffinePos pointers. */

	const DocumentElementRegion& GetRegion() const {return region;}

private:
	friend class ElementCollectingObject;

	OpDocumentElement(HTML_Element* elm, DocumentElementType type) : elm(elm), type(type) {}
	OpDocumentElement(const OpDocumentElement&);
	OpDocumentElement& operator=(const OpDocumentElement&);

	HTML_Element*		elm;
	DocumentElementType type;
	DocumentElementRegion region;
};

/** The collection of found elements and their regions. Apart of the OpDocumentElement vector,
	is additionaly has a vector of transform roots, that owns the AffinePos pointers. */

class DocumentElementCollection
	: public OpAutoVector<OpDocumentElement>
{
public:
	DocumentElementCollection() : OpAutoVector<OpDocumentElement>() {}
private:
	friend class ElementCollectingObject;

	DocumentElementCollection(const DocumentElementCollection&);
	DocumentElementCollection& operator=(const DocumentElementCollection&);

	OpAutoVector<AffinePos>	transforms;
};

#endif // not included
