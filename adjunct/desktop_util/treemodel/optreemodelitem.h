/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

// ---------------------------------------------------------------------------------

#ifndef ADJUNCT_DESKTOP_UTIL_TREEMODEL_OPTREEMODELITEM_H
#define ADJUNCT_DESKTOP_UTIL_TREEMODEL_OPTREEMODELITEM_H

#include "modules/util/OpTypedObject.h"
#include "modules/util/opstring.h"

class OpInfoText;
class Image;
class OpTreeView;
class OpWidget;

class OpThumbnailText
{
public:
	OpThumbnailText() : fixed_image(FALSE) {}

public:
	Image		thumbnail;	// thumbnail image
	OpString	url;
	OpString	title;
	BOOL		fixed_image;	// TRUE if a fixed skin image, otherwise FALSE
};

// ---------------------------------------------------------------------------------

class OpTreeModelItem : public OpTypedObject
{
	public:

		enum QueryType
		{
			INIT_QUERY,
			COLUMN_QUERY,
			MATCH_QUERY,
			PREPAINT_QUERY,
			UNREAD_QUERY,
			INFO_QUERY,
			URL_QUERY,				// query item for URL, title and thumbnail
			ASSOCIATED_TEXT_QUERY,	// query item for getting the associated text of an item
			ASSOCIATED_IMAGE_QUERY,	// query item for getting the associated image of an item
			BOLD_QUERY,				// query item to check if it should be bold
			WIDGET_QUERY,			// query item for a custom OpWidget to be used instead of a regular item
			SKIN_QUERY				// query item for getting the skin of the item
		};

		// use negative values for predefined match types.. positive ones are model specific

		enum MatchType
		{
			MATCH_ALL					= 0,	// Show all
			MATCH_STANDARD				= -1,	// Almost show all (you want a MATCH_QUERY, since MATCH_ALL doesn't send one)
			MATCH_FOLDERS				= -2,	// Only match and show folders
			MATCH_IMPORTANT				= -3,	// Only match and show important stuff (what this really means is up to the model)
			MATCH_MAIL					= -4,	// Only match mail and news, not chat or rss etc
			MATCH_MAIL_AND_NEWSFEEDS	= -5	// Only match mail, news and newsfeeds
		};

		enum
		{
			FLAG_FOCUSED				=	(1 << 0),		// Set by view if item is focused
			FLAG_OPEN					=	(1 << 1),		// Set by view if item is open
			FLAG_SELECTED				=	(1 << 2),		// Set by view if item is selected
			FLAG_CHECKED				=	(1 << 3),		// Set by view if item is checked
			FLAG_DISABLED				=	(1 << 4),		// Set by view if item is disabled
			FLAG_NO_PAINT				=	(1 << 5),		// Set by view if item is not to be painted, in which case COLUMN_QUERY only needs sort order and string (ie. optmize model data fetching if it makes sense)
			FLAG_INITIALLY_OPEN			=	(1 << 6),		// Set by model in a INIT_QUERY if item is initially open
			FLAG_INITIALLY_CHECKED		=	(1 << 7),		// Set by model in a INIT_QUERY if item is initially checked
			FLAG_INITIALLY_DISABLED		=	(1 << 8),		// Set by model in a INIT_QUERY if item is initially disabled
			FLAG_SEPARATOR				=	(1 << 9),		// Set by model in a INIT_QUERY if item is considered a seperator
			FLAG_BOLD					=	(1 << 10),		// Set by model in a COLUMN_QUERY if item is to be considered bold in this column
			FLAG_ITALIC					=   (1 << 11),		// Set by model in a COLUMN_QUERY if item is to be considered italic in this column
			FLAG_RIGHT_ALIGNED			=	(1 << 12),		// Set by model in a COLUMN_QUERY if item is to be considered right aligned in this column
			FLAG_MATCHED				=	(1 << 13),		// Set by model in a MATCH_QUERY if item matched the text and/or type
			FLAG_UNREAD					=	(1 << 14),		// Set by model in a UNREAD_QUERY if item is to be considered unread
			FLAG_TEXT_SEPARATOR			=	(1 << 15),		// Set by model in a INIT_QUERY if item is considered a text separator
			FLAG_FORMATTED_TEXT			=	(1 << 16),		// Set by model in a INIT_QUERY if item has text with formatting
			FLAG_CUSTOM_WIDGET			=	(1 << 17),		// Set by model in a INIT_QUERY if item has a custom widget to be used
			FLAG_NO_SELECT				=	(1 << 18),		// Set by model in a INIT_QUERY if item should not be selected
			FLAG_CUSTOM_CELL_WIDGET		=	(1 << 19),		// Set by model in a COLUMN_QUERY if item has a custom cell widget to be used
			FLAG_ASSOCIATED_TEXT_ON_TOP =	(1 << 20),		// Set by model in a INIT_QUERY if item wants the associated text line above the line with columns
			FLAG_CENTER_ALIGNED			=	(1 << 21),		// Set by model in a COLUMN_QUERY if item is to be considered center aligned in this column
			FLAG_VERTICAL_ALIGNED		=	(1 << 22),		// Set by model in a COLUMN_QUERY if item is to be considered vertical center aligned in this column

			FLAG_MATCHED_CHILD			=	(1 << 29),		// Internal flag used by OpTreeView
			FLAG_MATCH_DISABLED			=	(1 << 30),		// Internal flag used by OpTreeView
			FLAG_FETCHED				=	(1 << 31)		// Internal flag used by OpTreeView
		};

		enum TreeModelItemType
		{
			TREE_MODEL_ITEM_TYPE_NORMAL			= 0,
			TREE_MODEL_ITEM_TYPE_HEADER			= 1,
		};

		class ItemData
		{
			public:

			ItemData(QueryType type, OpTreeView* view = NULL):
				query_type(type),
				flags(0),
				treeview(view),
				item_type(TREE_MODEL_ITEM_TYPE_NORMAL)
			{
				switch(query_type)
				{
					case MATCH_QUERY:
						match_query_data.match_text			= NULL;
						match_query_data.match_column		= -1;
						match_query_data.match_type			= MATCH_ALL;
						break;
					case COLUMN_QUERY:
						column_query_data.column             = 0;
						column_query_data.column_text        = NULL;
						column_query_data.column_text_color  = OP_RGB(0, 0, 0);
						column_query_data.column_text_prefix = 0;
						column_query_data.column_image       = NULL;
						column_query_data.column_image_2     = NULL;
						column_query_data.column_sort_order  = 0;
						column_query_data.column_progress    = 0;
						column_query_data.column_indentation = -1;
						column_query_data.column_span		 = 1;
						column_query_data.custom_cell_widget = NULL;
						break;
					case INFO_QUERY:
						info_query_data.info_text			= NULL;
						break;
					case URL_QUERY:
						url_query_data.thumbnail_text		= NULL;
						break;
					case ASSOCIATED_TEXT_QUERY:
						associated_text_query_data.associated_text_indentation = -1;
						associated_text_query_data.text		= NULL;
						associated_text_query_data.text_color = OP_RGB(0, 0, 0);

						break;
					case ASSOCIATED_IMAGE_QUERY:
						associated_image_query_data.image	= NULL;
						associated_image_query_data.skin_state = -1;
						break;
					case WIDGET_QUERY:
						widget_query_data.widget			= NULL;
						break;
					case SKIN_QUERY:
						skin_query_data.skin				= NULL;
						break;
				}
			}

			// Query type
			QueryType query_type;

			// Various flags that are set or not.. see above.
			UINT32 flags;

			// In a COLUMN_QUERY, model can specify a bitmap image
			// Cannot be in the column struct since it is not a pointer
			Image column_bitmap;

			OpTreeView* treeview;

			TreeModelItemType item_type;

			union
			{
				struct // MATCH_QUERY
				{
					// Model should use this string to decide if the match flag should be set or not
					OpString * match_text;
					// What column match_text should be compared to (-1 is default = "all")
					INT32 match_column;
					// Model should use this filter type to decide if the match flag should be set or not
					MatchType match_type;
				} match_query_data;

				struct // COLUMN_QUERY
				{
					// This states what column (0 -> column count - 1)
					INT32 column;

					// This will point to a OpString that model should Set()
					OpString * column_text;

					// Model can specify text color
					UINT32 column_text_color;

					// Model can specify if one should skip a prefix when sorting
					INT32 column_text_prefix;

					// Model can specify a skin image name (string must exist as long as model exist)
					const char *column_image;

					// Model can specify a second skin image name to be drawn on top
					// (string must exist as long as model exist)
					const char *column_image_2;

					// Model can specify a custom sort order value
					INT32 column_sort_order;

					// Model can specify a progress value (1 -> 100%)
					INT32 column_progress;

					// Model can specify custom indentation for this column
					INT32 column_indentation;

					// Model that specify that this item should span more than its own column
					INT32 column_span;

					// Model can specify custom widget for this column
					OpWidget* custom_cell_widget;

				} column_query_data;

				struct // INFO_QUERY
				{
					// This will point to a OpInfoText
					OpInfoText * info_text;
				} info_query_data;

				struct // URL_QUERY
				{
					// This will point to a OpThumbnailText
					OpThumbnailText * thumbnail_text;
				} url_query_data;

				struct // ASSOCIATED_TEXT_QUERY
				{
					OpString * text;

					// Model can specify custom indentation for this associated text
					INT32 associated_text_indentation;
					// Model can specify associated text color
					UINT32 text_color;
				} associated_text_query_data;

				struct // ASSOCIATED_IMAGE_QUERY
				{
					OpString8 * image;
					INT32		skin_state;
				} associated_image_query_data;
				
				struct // WIDGET_QUERY
				{
					OpWidget* widget;
				} widget_query_data;

				struct // SKIN_QUERY
				{
					const char *skin;
				} skin_query_data;
			};
		};

		// Get item data

		virtual OP_STATUS GetItemData(ItemData* item_data) = 0;

		virtual int		  GetNumLines() { return 1; }
};

// ---------------------------------------------------------------------------------

#endif // !ADJUNCT_DESKTOP_UTIL_TREEMODEL_OPTREEMODELITEM_H
