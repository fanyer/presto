/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef MARKUP_H
#define MARKUP_H

class Markup
{
public:
#include "modules/logdoc/src/html5/elementtypes.inl"
#include "modules/logdoc/src/html5/attrtypes.inl"

	enum Ns
	{
		HTML,
		MATH,
		SVG,
		XLINK,
		XML,
		XMLNS,
		WML
	};

	/**
	 * Used to check if an element type is a special internal element inserted
	 * into the logical tree by Opera that will not be visible in the DOM
	 * from script and does not directly reflect markup in a document.
	 * @returns TRUE if the element is an internal element type.
	 */
	static BOOL		IsSpecialElement(Markup::Type type) { return type >= Markup::HTE_FIRST_SPECIAL; }
	/**
	 * Used to check if an element type can occur as a normal tag in markup or is
	 * an special internal element (see documentation for IsSpecialElement()) or
	 * another type of markup than tags, like HTE_TEXT or HTE_DOCTYPE.
	 * @returns TRUE if the element is an element, and not a just a node like
	 *          text or doctype.
	 */
	static BOOL		IsRealElement(Markup::Type type) { return type >= Markup::HTE_FIRST && type <= Markup::HTE_LAST; }
	/**
	 * Used to check if an element type is not a special internal element (see
	 * documentation for IsSpecialElement()) and not a regular markup tag.
	 * @returns TRUE if the element is a node that comes from the markup, like
	 *          text or doctype, but is not a normal element with a name. */
	static BOOL		IsNonElementNode(Markup::Type type) { return type > Markup::HTE_LAST && type < Markup::HTE_FIRST_SPECIAL; }
	/**
	 * Used to check if the canonical name of an element type can be looked up
	 * in the table of predefined elements. Most elements in the specifications
	 * we support are included in the tables. Special internal element (see
	 * documentation for IsSpecialElement()) types and some elements that occur
	 * naturally in markup, like HTE_TEXT or HTE_DOCTYPE, does not have a string
	 * representation.
	 * @returns TRUE if the element type has a name entry in the element name table. */
	static BOOL		HasNameEntry(Markup::Type type) { return type >= Markup::HTE_FIRST && type < Markup::HTE_LAST; }

	/**
	 * Used to check if an attribute type is a special internal attribute that
	 * will not be visible in the DOM from script and does not directly reflect
	 * markup in a document. See logdoc/documentation/special_attrs.html
	 * @returns TRUE if the attribute isn't usually laid out.
	 */
	static BOOL		IsSpecialAttribute(Markup::AttrType type) { return type >= Markup::HA_FIRST_SPECIAL; }
	/**
	 * Used to check if the canonical name of an attribute type can be looked up
	 * in the table of predefined attributes. Most attributes in the specifications 
	 * we support are included in the tables. Special internal attribute types does
	 * not have a string representation.
	 * @returns TRUE if the attribute type has a name entry in the attribute name table. */
	static BOOL		HasAttrNameEntry(Markup::AttrType type) { return type >= Markup::HA_FIRST && type < Markup::HA_LAST; }
};

#endif // MARKUP_H
