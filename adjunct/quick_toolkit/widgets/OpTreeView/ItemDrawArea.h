/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef ITEM_COLUMN_CACHE_H
#define ITEM_COLUMN_CACHE_H

#include "modules/util/tree.h"
#include "modules/widgets/OpWidget.h"

/**
 * @brief
 * @author Arjan van Leeuwen
 *
 *
 */

class ItemDrawArea
{
public:

//  -------------------------
//  Public member functions:
//  -------------------------

	/**
	 *
	 *
	 */
	ItemDrawArea()
		: m_string_prefix_length(0)
		, m_color(OP_RGB(0, 0, 0))
		, m_flags(0)
		, m_image(NULL)
		, m_image_state(-1)
		, m_image_2(NULL)
		, m_sort_order(0)
		, m_progress(0)
		, m_indentation(-1)
		, m_column_span(1)
		, m_custom_cell_widget(NULL)
		{ }

	/**
	 *
	 *
	 */
	~ItemDrawArea()
	{
		if (m_custom_cell_widget && m_custom_cell_widget->GetWidgetContainer()) 
			m_custom_cell_widget->Delete();
	}

	enum StringFlags
	{
		FLAG_BOLD   = 1 << 0,
		FLAG_ITALIC = 1 << 1,
		FLAG_COLOR  = 1 << 2
	};

	class StringList
	{
	public:

		//  -------------------------
		//  Public member functions:
		//  -------------------------

		/**
		 *
		 *
		 */
		StringList()
			: m_string_flags(0)
		    , m_string_color(OP_RGB(0, 0, 0))
			, m_next(NULL)
			{ }

		/**
		 *
		 *
		 */
		~StringList() { OP_DELETE(m_next); }

		//  -------------------------
		//  Public member variables:
		//  -------------------------

		int					m_string_flags;
		COLORREF			m_string_color;
		OpWidgetString		m_string;
		StringList*			m_next;
	};

//  -------------------------
//  Public member variables:
//  -------------------------

	int						m_string_prefix_length;
	UINT32					m_color;
	UINT32					m_flags;
	StringList				m_strings;
	Image					m_bitmap_image;
	const char*				m_image;
	INT32					m_image_state;
	const char*				m_image_2;
	INT32					m_sort_order;
	INT32					m_progress;
	INT32					m_indentation;
	INT32					m_column_span;
	OpWidget*				m_custom_cell_widget;
};

#endif // ITEM_COLUMN_CACHE_H
