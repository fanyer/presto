/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */
#ifndef ADJUNCT_DESKTOP_UTIL_TREEMODEL_SIMPLETREEMODEL_H
#define ADJUNCT_DESKTOP_UTIL_TREEMODEL_SIMPLETREEMODEL_H

#include "optreemodel.h"

class SimpleTreeModel;
class Image;

/***********************************************************************************
**
**	SimpleTreeModelItem
**
***********************************************************************************/

class SimpleTreeModelItem
	: public TreeModelItem<SimpleTreeModelItem, SimpleTreeModel>
{

	friend class SimpleTreeModel;

public:

	/**
	 * @param tree_model
	 * @param user_data
	 * @param id
	 * @param type
	 * @param initially_checked
	 * @param separator
	 */
	SimpleTreeModelItem(OpTreeModel* tree_model,
						void* user_data,
						INT32 id,
						Type type,
						BOOL initially_checked = FALSE,
						BOOL separator = FALSE);

	/**
	 *
	 */
	virtual	~SimpleTreeModelItem();

	BOOL IsOOM() { return m_item_data == NULL; }

	/**
	 * @param column
	 * @param string
	 * @param image
	 * @param sort_order
	 */
	OP_STATUS SetItemData(INT32 column,
						  const uni_char* string,
						  const char* image = NULL,
						  INT32 sort_order = 0);

	/**
	 * @param column
	 * @param color
	 */
	void SetItemColor(INT32 column,
					  INT32 color)
	{m_item_data[column].m_color = color;}

	/**
	 * @return the favicon of the item
	 */
	Image GetFavIcon();

	/**
	 * @param column
	 * @param image
	 */
	OP_STATUS SetItemImage(INT32 column,
						   const char * image)
	{ return m_item_data[column].m_image.Set(image); }

	const OpStringC8 GetItemImage(INT32 column)
	{ return m_item_data[column].m_image;}

	/**
	 * @return
	 */
	BOOL IsChecked() {return ((m_item_data->m_flags & OpTreeModelItem::FLAG_CHECKED) != 0);}

	/**
	 * @return
	 */
	BOOL IsImportant() {return m_is_important;}

	/**
	 * @param value
	 */
	void SetInitiallyDisabled(BOOL value) { m_initially_disabled = value; }

	/**
	 * @param value
	 */
	void SetInitiallyOpen(BOOL value) { m_initially_open = value; }

	/**
	 * @param value
	 */
	void SetIsTextSeparator(BOOL value) { m_is_text_separator = value; }

	/**
	 * @param value
	 */
	void SetIsImportant(BOOL value) { m_is_important = value; }

	/**
	 * @param value
	 */
	void SetHasFormattedText(BOOL value) { m_has_formatted_text = value; }

	/**
	 * @param value TRUE if the item should have its associated text cleared
	 *        when it is removed from a model. This is interesting for items
	 *        where the associated text is context dependent and does not make
	 *        automatically make sense in other contexts.
	 */
	void SetCleanOnRemove(BOOL value) { m_clean_on_remove = value; }


	/**
	 * @return
	 */
	virtual void* GetUserData() {return m_user_data;}

	/**
	 * @param user_data
	 */
	void SetUserData(void * user_data) { m_user_data = user_data; }

	/**
	 * @param favicon_key to be used to get the corresponding favicon
	 */
	OP_STATUS SetFavIconKey(const OpStringC & favicon_key) { return m_favicon_key.Set(favicon_key.CStr()); }

	/**
	 * Will associate the text with this item and it will display
	 * as associated text by the OpTreeView
	 *
	 * @param associated_text
	 */
	OP_STATUS SetAssociatedText(const OpStringC & associated_text);

	// == TreeModelItem ======================

	virtual OP_STATUS			GetItemData(ItemData* item_data);
	virtual Type				GetType() {return m_type;}
	virtual int					GetID() {return m_id;}
	virtual void			    OnRemoved();
	virtual INT32				GetNumLines() { return m_associated_text ? 2 : 1; }

	struct ItemColumnData
	{
		ItemColumnData() : m_sort_order(0), m_color(-1) {}

		OpString		m_string;
		OpString8		m_image;
		INT32			m_sort_order;
		INT32			m_color;
		UINT32			m_flags;
	};

private:

	OpTreeModel*				m_tree_model;
	Type						m_type;
	INT32						m_id;
	ItemColumnData*				m_item_data;
	void*						m_user_data;
	BOOL						m_initially_checked;
	BOOL						m_separator;
	BOOL						m_initially_disabled;
	BOOL						m_initially_open;
	BOOL						m_is_text_separator;
	BOOL						m_is_important;
	BOOL						m_has_formatted_text;
	BOOL						m_clean_on_remove;
	OpString*					m_associated_text;
	OpString                    m_favicon_key;
};

/***********************************************************************************
**
**	SimpleTreeModel
**
***********************************************************************************/

/** A simple implementation of a TreeModel.
 *
 * To add items to a SimpleTreeModel, ALWAYS use the methods declared
 * in this class to ensure consistency between items.
 *
 * All items in a SimpleTreeModel SHOULD have unique, non-0 IDs.  If
 * an item is created by a call to a method of this class using the
 * default id value (0), an ID will automatically be generated.  Such
 * automatic IDs will have values greater or equal to 2^30
 * (0x40000000).  To avoid conflicts, all manually chosen ids should
 * be less than that.
 */
class SimpleTreeModel
	: public TreeModel<SimpleTreeModelItem>
{
public:

	/**
	 * @param column_count
	 */
	SimpleTreeModel(INT32 column_count = 1);

	/**
	 *
	 */
	virtual ~SimpleTreeModel();


	/**
	 * @param column
	 * @param string
	 * @param image
	 * @param sort_by_string_first
	 * @param matchable
	 */
	void SetColumnData(INT32 column,
					   const uni_char* string,
					   const char* image = NULL,
					   BOOL sort_by_string_first = FALSE,
					   BOOL matchable = FALSE);


	/**
	 * @param string
	 * @param image
	 * @param sort_order
	 * @param parent
	 * @param user_data
	 * @param id If 0, an ID will be automatically generated.
	 * @param type
	 * @param initially_checked
	 *
	 * @return index of item added
	 */
	INT32 AddItem(const uni_char* string,
				  const char* image = NULL,
				  INT32 sort_order = 0,
				  INT32 parent= -1,
				  void* user_data = NULL,
				  INT32 id = 0,
				  OpTypedObject::Type type = OpTypedObject::UNKNOWN_TYPE,
				  BOOL initially_checked = FALSE);

	/**
	 * @param parent
	 *
	 * @return index
	 */
	INT32 AddSeparator(INT32 parent= -1);


	/**
	 * @param sibling
	 * @param string
	 * @param image
	 * @param sort_order
	 * @param user_data
	 * @param id If 0, an ID will be automatically generated.
	 * @param type
	 * @param initially_checked
	 *
	 * @return index
	 */
	INT32 InsertBefore(INT32 sibling,
					   const uni_char* string,
					   const char* image = NULL,
					   INT32 sort_order = 0,
					   void* user_data = NULL,
					   INT32 id = 0,
					   OpTypedObject::Type type = OpTypedObject::UNKNOWN_TYPE,
					   BOOL initially_checked = FALSE);

	/**
	 * @param sibling
	 * @param string
	 * @param image
	 * @param sort_order
	 * @param user_data
	 * @param id If 0, an ID will be automatically generated.
	 * @param type
	 * @param initially_checked
	 *
	 * @return
	 */
	INT32 InsertAfter(INT32 sibling,
					  const uni_char* string,
					  const char* image = NULL,
					  INT32 sort_order = 0,
					  void* user_data = NULL,
					  INT32 id = 0,
					  OpTypedObject::Type type = OpTypedObject::UNKNOWN_TYPE,
					  BOOL initially_checked = FALSE);

	/**
	 * @param sibling
	 *
	 * @return
	 */
	INT32 InsertAfter(INT32 sibling);

	/**
	 * @param sibling
	 *
	 * @return
	 */
	INT32 InsertBefore(INT32 sibling);

	INT32 InsertBefore(SimpleTreeModelItem * item, INT32 sibling_index)
		{ return TreeModel<SimpleTreeModelItem>::InsertBefore(item, sibling_index); }

	/** This method will return a valid ID in the "automatic" range
	 * which is not currently used by any item in this model.
	 *
	 * If this method fails, it returns 0 (which is an invalid ID).
	 */
	INT32 MakeAutomaticID();

	/** This method will return a valid ID in the "automatic" range
	 * which is not currently used by any item in this model.
	 *
	 * This method is slow, and is automatically used by
	 * MakeAutomaticID() as a fallback.  Outside testing, there are
	 * probably no other reason to ever call this method.
	 *
	 * If this method fails, it returns 0 (which is an invalid ID).
	 */
	INT32 MakeAutomaticIDBySearching();

	/**
	 * @param pos
	 *
	 * @depricated Do not use - use Delete
	 */
	void RemoveItem(INT32 pos) { Delete(pos); }

#ifdef DEBUG_ENABLE_OPASSERT
	/** Reimplementation of inherited method for debugging purposes.
	 */
	void Delete(INT32 pos, BOOL all_children = TRUE);
#endif

	/**
	 * @param pos
	 * @param column
	 * @param string
	 * @param image
	 * @param sort_order
	 */
	void SetItemData(INT32 pos,
					 INT32 column,
					 const uni_char* string,
					 const char* image = NULL,
					 INT32 sort_order = 0);

	/**
	 * @param pos
	 * @param column
	 * @param color
	 */
	void SetItemColor(INT32 pos,
					  INT32 column,
					  INT32 color);

	/**
	 * @param pos
	 *
	 * @return
	 */
	void* GetItemUserData(INT32 pos);

	/**
	 * @param pos
	 *
	 * @return
	 */
	void* GetItemUserDataByID(INT32 id);

	/**
	 * @param user_data
	 *
	 * @return
	 */
	INT32 FindItemUserData(void* user_data);

	/**
	 * @param pos
	 * @param column
	 *
	 * @return
	 */
	const uni_char*	GetItemString(INT32 pos, INT32 column = 0);
								  
								  
	/**
	 * @param id
	 * @param column
	 *
	 * @return The item with that ID, or NULL if not found.
	 */							  
	const uni_char*	GetItemStringByID(INT32 id, INT32 column = 0);

	// == OpTreeModel ======================

	virtual INT32				GetColumnCount() {return m_column_count;}
	virtual OP_STATUS			GetColumnData(OpTreeModel::ColumnData* column_data);
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	virtual OP_STATUS			GetTypeString(OpString& type_string) { return OpStatus::ERR; }
#endif

protected:

	struct ColumnData
	{
		ColumnData() : m_sort_by_string_first(FALSE), m_matchable(FALSE) {}
		OpString		m_string;
		OpString8		m_image;
		BOOL			m_sort_by_string_first;
		BOOL			m_matchable;
	};

	INT32			m_column_count;
	ColumnData*		m_column_data;

private:
	/* This value is used to quickly generate automatic IDs.  It shall
	 * always be at least as large as the largest id of any item that
	 * is currently stored in the model.  There is no upper limit to
	 * the value.
	 */
	INT32						m_largest_used_id;
#ifdef DEBUG_ENABLE_OPASSERT
	INT32						m_last_seen_item_count;
#endif

	void AboutToAddItem(SimpleTreeModelItem * item);
};

template<class T>
class TemplateTreeModel : public SimpleTreeModel
{
public:

	/**
	 * @param
	 */
	TemplateTreeModel(INT32 column_count = 1)
		: SimpleTreeModel(column_count) {}

	/**
	 * @param string
	 * @param image
	 * @param sort_order
	 * @param parent
	 * @param user_data
	 * @param id
	 * @param type
	 * @param initially_checked
	 *
	 * @return
	 */
	INT32 AddItem(const uni_char* string,
				  const char* image = NULL,
				  INT32 sort_order = 0,
				  INT32 parent= -1,
				  T* user_data = NULL,
				  INT32 id = 0,
				  OpTypedObject::Type type = OpTypedObject::UNKNOWN_TYPE,
				  BOOL initially_checked = FALSE)
	{
		return SimpleTreeModel::AddItem(string,
										image,
										sort_order,
										parent,
										user_data,
										id,
										type,
										initially_checked);
	}


	/**
	 * @param sibling
	 * @param string
	 * @param image
	 * @param sort_order
	 * @param user_data
	 * @param id
	 * @param type
	 * @param initially_checked
	 *
	 * @return
	 */
	INT32 InsertBefore(INT32 sibling,
					   const uni_char* string,
					   const char* image = NULL,
					   INT32 sort_order = 0,
					   T* user_data = NULL,
					   INT32 id = 0,
					   OpTypedObject::Type type = OpTypedObject::UNKNOWN_TYPE,
					   BOOL initially_checked = FALSE)
	{
		return SimpleTreeModel::InsertBefore(sibling,
											 string,
											 image,
											 sort_order,
											 user_data,
											 id,
											 type,
											 initially_checked);
	}


	/**
	 * @param sibling
	 * @param string
	 * @param image
	 * @param sort_order
	 * @param user_data
	 * @param id
	 * @param type
	 * @param initially_checked
	 *
	 * @return
	 */
	INT32 InsertAfter(INT32 sibling,
					  const uni_char* string,
					  const char* image = NULL,
					  INT32 sort_order = 0,
					  T* user_data = NULL,
					  INT32 id = 0,
					  OpTypedObject::Type type = OpTypedObject::UNKNOWN_TYPE,
					  BOOL initially_checked = FALSE)
	{
		return SimpleTreeModel::InsertAfter(sibling,
											string,
											image,
											sort_order,
											user_data,
											id,
											type,
											initially_checked);
	}


	/**
	 * @param pos
	 *
	 * @return
	 */
	T*	GetItem(INT32 pos)
	{
		return (T*) SimpleTreeModel::GetItemUserData(pos);
	}

	/**
	 * @param pos
	 *
	 * @return
	 */
	T*	GetItemByID(INT32 id)
	{
		return (T*) SimpleTreeModel::GetItemUserDataByID(id);
	}

	/**
	 * @param user_data
	 *
	 * @return
	 */
	INT32 FindItem(T* user_data)
	{
		return SimpleTreeModel::FindItemUserData(user_data);
	}


	/**
	 * @param pos
	 */
	void DeleteItem(INT32 pos)
	{
		OP_DELETE(GetItem(pos)); Delete(pos);
	}


	/**
	 *
	 */
	void DeleteAll()
	{
		while (GetCount()) DeleteItem(GetCount()-1);
	}
};

/**
 * OpAutoVector works exactly like OpVector but it deletes the elements automatically.
 */
template<class T>
class AutoTreeModel : public TemplateTreeModel<T>
{
public:

	AutoTreeModel(INT32 column_count = 1) : TemplateTreeModel<T>(column_count) {}

	~AutoTreeModel()
	{
		TemplateTreeModel<T>::DeleteAll();
	}
};

#endif // !ADJUNCT_DESKTOP_UTIL_TREEMODEL_SIMPLETREEMODEL_H
