/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SVG_TREE_ITERATOR_H
#define SVG_TREE_ITERATOR_H

#ifdef SVG_SUPPORT

class HTML_Element;

class SVGTreeIterator
{
public:
	virtual ~SVGTreeIterator() {}

	/**
	 * Move to the next element in iterator order.
	 *
	 * @return Element if one was found, otherwise NULL.
	 */
	virtual HTML_Element* Next() = 0;

	/**
	 * Move to the previous element in iterator order.
	 *
	 * @return Element if one was found, otherwise NULL.
	 */
	virtual HTML_Element* Prev() = 0;
};

#endif // SVG_SUPPORT

#endif // SVG_TREE_ITERATOR_H
