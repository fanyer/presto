/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef SPEEDDIAL_SUGGESTIONS_H
#define SPEEDDIAL_SUGGESTIONS_H

#ifdef SUPPORT_SPEED_DIAL

#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/desktop_util/treemodel/optreemodelitem.h"

class SpeedDialSuggestionsModel;	//need to declare to use in SpeedDialSuggestionsModelItem

/* Generic class for Treemodel with speed dial suggestions*/
class SpeedDialSuggestionsModelItem : public TreeModelItem<SpeedDialSuggestionsModelItem, SpeedDialSuggestionsModel>
{
public:
	enum ItemType
	{
		LINK_TYPE,
		FOLDER_TYPE,
		LABEL_TYPE
	};

	SpeedDialSuggestionsModelItem(ItemType type);

	Type				GetType() 	{return OpTypedObject::UNKNOWN_TYPE;}
	INT32				GetID()		{return m_id;}
	OP_STATUS			GetItemData(ItemData* item_data);
	BOOL				HasDisplayURL() const { return m_display_url.HasContent(); }

	const OpString&		GetTitle() const { return m_title; }
	const OpString&		GetURL() const { return m_url; }
	const OpString&		GetDisplayURL() const { return m_display_url.HasContent() ? m_display_url : m_url; }
	ItemType			GetItemType() const { return m_item_type; }

	OP_STATUS			SetTitle(const OpStringC &title) { return m_title.Set(title); }
	OP_STATUS			SetImage(const OpStringC8 &image) { return m_image.Set(image); }
	void				SetBitmap(const Image &bitmap) { m_bitmap = bitmap; }
	OP_STATUS			SetLinkData(const OpStringC &title, const OpStringC &url, const OpStringC &display_url = NULL);


private:
	INT32				m_id;
	OpString			m_title;
	OpString			m_url;
	OpString			m_display_url;
	OpString8			m_image;
	Image				m_bitmap;
	ItemType			m_item_type;
};

/* Treemodel with suggestions to add to speed dial */
class SpeedDialSuggestionsModel : public TreeModel<SpeedDialSuggestionsModelItem>
{
public:
	/*** Main API ***/
	/**
	 * Add collections of links to the treemodel. Currently History top 10, open tabs and some Opera sites
	 * are added.
	 * @param items_under_parent_folder TRUE means items will be shown under its parent folder. FALSE means
	 *                  items will be added under root which results in invisible parent folder.
	 */
	void				Populate(BOOL items_under_parent_folder = FALSE);


private:
	/*** Helper functions for adding items to the model ***/

	/**
	 * Add a folder to the treemodel
	 * @param title			Title shown in the treemodel.
	 * @param skin_image	Name of the skin image shown next to the title. If empty, no image is shown.
	 * @return index		Index of the new folder. If -1 is returned, an error occured.
	 */
	INT32				AddFolder(const OpStringC &title, const OpStringC8 &skin_image);

	/**
	 * Add a label to the treemodel
	 * @param title		Title shown in the treemodel.
	 * @param parent	The parent (folder) to which the label should be added.
	 * @return index	Index of the new label. If -1 is returned, an error occured.
	 */
	INT32				AddLabel(const OpStringC &title, const INT32 parent);

	/**
	 * Add a suggestion (a link) to the model. IF a favicon for the link exists in the cache, it will
	 * be show. Otherwise a default document icon will be shown.
	 * @param title		Title of the link
	 * @param url		The url of the link
	 * @param parent	The parent (folder) to which the suggestion should be added.
	 * @return index	Index of the new suggestion. If -1 is returned, no suggestion was added (possibly an error occurred).
	 */
	INT32				AddSuggestion(const OpStringC &title, const OpStringC &url, const INT32 parent=-1);

	// Add a folder with the top 10 most frequently accessed site from history
	void				AddHistorySuggestions(BOOL items_under_parent_folder = FALSE, INT32 num_of_item_to_add = -1);
	// Add a folder with the currently open tabs
	void				AddOpenTabs(BOOL items_under_parent_folder = FALSE, INT32 num_of_item_to_add = -1);
	// Add a folder with some site from the opera.com family
	void				AddOperaSites(BOOL items_under_parent_folder = FALSE, INT32 num_of_item_to_add = -1);
	//Add a folder with the closed tabs
	void				AddClosedTabs(BOOL items_under_parent_folder = FALSE, INT32 num_of_item_to_add = -1);

	void				AddClosedTabsHelperL(BOOL items_under_parent_folder, INT32 num_of_item_to_add);

	// simple implementaion fo Treemodel
	INT32				GetColumnCount() {return 1;}
	OP_STATUS			GetColumnData(OpTreeModel::ColumnData* column_data) { return OpStatus::OK; }

};

#endif // SUPPORT_SPEED_DIAL

#endif // SPEEDDIAL_SUGGESTIONS_H
