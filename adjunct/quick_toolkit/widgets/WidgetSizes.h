/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef WIDGET_SIZES_H
#define WIDGET_SIZES_H

/** @brief Constants and types for use in QuickWidget
  */
struct WidgetSizes
{
	/** Infinity can be used as a preferred size, in which case it means that
	  * the widget is 'greedy' - it will take an infinitely big size if it can.
	  * This usually affects the size of the container as well - if there is at
	  * least one widget with infinite preferred size in a container, the
	  * container itself will have an infinite preferred size.
	  *
	  * Example: edit boxes or text input field can usually be infinitely big;
	  * by increasing the size of the dialog, the user can make it easier to
	  * input more text.
	  */
	static const unsigned Infinity = UINT_MAX;

	/** Fill can be used as a preferred size. It is similar to Infinity
	  * in the sense that widgets can grow, but instead of always claiming
	  * infinite space, they grow with other widgets in their environment.
	  * This usually doesn't affect the size of the container, just the layout
	  * of widgets inside the current container if more space is made
	  * available.
	  *
	  * Example: this is useful for spacers, where an element should be big
	  * enough to fill the available space, but it doesn't have to claim more.
	  */
	static const unsigned Fill = UINT_MAX - 1;

	/** UseDefault means to use the default size instead of an overridden value
	  */
	static const unsigned UseDefault = UINT_MAX - 2;

	/** @brief Struct with left-right-top-bottom elements */
	struct Margins
	{
		unsigned left;
		unsigned right;
		unsigned top;
		unsigned bottom;

		Margins(unsigned init = 0)
			: left(init)
			, right(init)
			, top(init)
			, bottom(init) {}

		bool IsDefault() const { return left == UseDefault && right == UseDefault && top == UseDefault && bottom == UseDefault; }

		bool operator==(const Margins& other) const {
			return left == other.left &&
					right == other.right &&
					top == other.top &&
					bottom == other.bottom;
		}

		bool operator!=(const Margins& other) const { return !(*this == other); }
	};
};


#endif // WIDGET_SIZES_H
