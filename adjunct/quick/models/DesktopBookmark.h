/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author: karie
 * 
 */

#ifndef _DESKTOP_BOOKMARK_H_
#define _DESKTOP_BOOKMARK_H_

#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "modules/bookmarks/bookmark_item.h"
#include "modules/bookmarks/bookmark_manager.h"
#include "adjunct/quick/hotlist/HotlistModelItem.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/sync/sync_util.h"
#include "modules/locale/oplanguagemanager.h"

class BookmarkModel;
class Bookmark;
class BookmarkFolder;
class BookmarkItemData;

#define BookmarkWrapper(ptr) CoreBookmarkWrapper::CastToWrapper(ptr)

OP_STATUS GetAttribute(BookmarkItem* item, BookmarkAttributeType, OpString& value);
OP_STATUS SetAttribute(BookmarkItem* item, BookmarkAttributeType type, const OpStringC&  str);
OP_STATUS SetAttribute(BookmarkItem* item, BookmarkAttributeType type, INT32 value);
INT32	  GetAttribute(BookmarkItem* item, BookmarkAttributeType type);

void SetDataFromItemData(const BookmarkItemData* item_data, BookmarkItem* bookmark);
UINT GetFlagFromAttribute(BookmarkAttributeType attribute);

class CoreBookmarkWrapper : public BookmarkItemListener
{
public:
	explicit CoreBookmarkWrapper(BookmarkItem* core_item);
	virtual ~CoreBookmarkWrapper();

	// move to protected
	void SetCoreItem(BookmarkItem* item) { m_core_item = item;}
	BookmarkItem* GetCoreItem() { return m_core_item; }
	void DetachCoreItem();
	OP_STATUS RemoveBookmark(BOOL real_delete, BOOL do_sync);

	void SetIgnoreNextNotify(BOOL val) {m_ignore_next_notify = val;}

	// BookmarkItem::Listener
	virtual void OnBookmarkDeleted();
	virtual void OnBookmarkRemoved();
	virtual void OnBookmarkChanged(BookmarkAttributeType attr_type, BOOL moved);

	const uni_char* GetParentGUID() const;

	// Helper functions
	virtual HotlistModelItem*		CastToHotlistModelItem()		= 0;		 // must return a valid value
	virtual Bookmark*				CastToBookmark()				{return 0;}
	virtual BookmarkFolder*			CastToBookmarkFolder()			{return 0;}

	// static
	static CoreBookmarkWrapper* CastToWrapper(HotlistModelItem*);

	void	SetWasInTrash(BOOL in_trash){m_was_in_trash = in_trash;}
	BOOL	GetWasInTrash(){return m_was_in_trash;}

protected:
	OP_STATUS GetStringAttribute(BookmarkAttributeType type, OpString& value) const {return ::GetAttribute(m_core_item,type,value);}
	OP_STATUS SetAttribute(BookmarkAttributeType type, const OpStringC& str);
	OP_STATUS SetIntAttribute(BookmarkAttributeType type, INT32 value	);
	INT32	  GetIntAttribute(BookmarkAttributeType type) const						{return ::GetAttribute(m_core_item,type);}

	CoreBookmarkWrapper(const CoreBookmarkWrapper& right);

	BookmarkItem* m_core_item;	
	BOOL m_was_in_trash;
	BOOL m_ignore_next_notify;
};

// T must be a sub class of HotlistGenericItem
template <typename T>
class DesktopBookmark : public T, public CoreBookmarkWrapper
{
 public:
	 explicit DesktopBookmark(BookmarkItem* core_item)
		:CoreBookmarkWrapper(core_item)
	 {}
	 ~DesktopBookmark(){}

	// Subclassing HotlistModelItem

	virtual const OpStringC&  GetUniqueGUID() const	  		{ const_cast<OpString&>(this->m_unique_guid).Set(m_core_item->GetUniqueId()); return this->m_unique_guid; }
	virtual const OpStringC&  GetDescription() const		{ GetStringAttribute(BOOKMARK_DESCRIPTION, const_cast<OpString&>(this->m_description)); return this->m_description; }
	virtual const OpStringC&  GetShortName() const			{ GetStringAttribute(BOOKMARK_SHORTNAME, const_cast<OpString&>(this->m_shortname)); return this->m_shortname; }
	virtual const OpStringC&  GetUrl() const				{ GetStringAttribute(BOOKMARK_URL, const_cast<OpString&>(this->m_url)); return this->m_url; }
	virtual const OpStringC&  GetPartnerID() const			{ GetStringAttribute(BOOKMARK_PARTNER_ID, const_cast<OpString&>(this->m_partner_id)); return this->m_partner_id; }
	virtual const OpStringC&  GetDisplayUrl() const			{ GetStringAttribute(BOOKMARK_DISPLAY_URL, const_cast<OpString&>(this->m_display_url)); return this->m_display_url.HasContent() ? this->m_display_url : GetUrl(); }

	virtual BOOL  GetIsActive() const						{ return GetIntAttribute(BOOKMARK_ACTIVE); }
	virtual BOOL  GetIsExpandedFolder() const				{ return GetIntAttribute(BOOKMARK_EXPANDED); }
	virtual BOOL  GetSmallScreen() const					{ return GetIntAttribute(BOOKMARK_SMALLSCREEN); }
	virtual INT32 GetPersonalbarPos() const					{ return GetIntAttribute(BOOKMARK_SHOW_IN_PERSONAL_BAR) ? GetIntAttribute(BOOKMARK_PERSONALBAR_POS) : -1;}
	virtual INT32 GetPanelPos() const						{ return GetIntAttribute(BOOKMARK_SHOW_IN_PANEL) ? GetIntAttribute(BOOKMARK_PANEL_POS) : -1;}

	virtual void	SetName( const uni_char* str )			{ SetAttribute(BOOKMARK_TITLE, str); }
	virtual void	SetDescription( const uni_char* str)	{ SetAttribute(BOOKMARK_DESCRIPTION, str); }
	virtual void	SetShortName( const uni_char* str )		{ SetAttribute(BOOKMARK_SHORTNAME, str); }
	virtual void	SetUrl(const uni_char* str)				{ this->m_resolved_url.Empty(); SetAttribute(BOOKMARK_URL, str); }
	virtual void	SetDisplayUrl(const uni_char* str)		{ SetAttribute(BOOKMARK_DISPLAY_URL, str); }
	virtual void	SetPartnerID( const uni_char* str )		{ SetAttribute(BOOKMARK_PARTNER_ID, str);}
	virtual void	SetIsActive( BOOL state )				{ SetIntAttribute(BOOKMARK_ACTIVE, state); }
	virtual void	SetIsExpandedFolder( BOOL state )		{ SetIntAttribute(BOOKMARK_EXPANDED, state); }
	virtual void	SetSmallScreen( BOOL state )			{ SetIntAttribute(BOOKMARK_SMALLSCREEN, state); }
	virtual void	SetPanelPos( INT32 pos )				{ SetIntAttribute(BOOKMARK_PANEL_POS, pos); SetIntAttribute(BOOKMARK_SHOW_IN_PANEL, pos >= 0 );}
	virtual void	SetPersonalbarPos( INT32 pos )			{ SetIntAttribute(BOOKMARK_PERSONALBAR_POS, pos); SetIntAttribute(BOOKMARK_SHOW_IN_PERSONAL_BAR, pos >= 0 ); }

	virtual BOOL	ClearVisited()							{ return OpStatus::IsSuccess(SetAttribute(BOOKMARK_VISITED, UNI_L("")));}
	virtual BOOL	ClearCreated()							{ return OpStatus::IsSuccess(SetAttribute(BOOKMARK_CREATED, UNI_L("")));}

	virtual BOOL  GetHasMoveIsCopy() const					{ return m_core_item->MoveIsCopy(); }
	virtual BOOL  GetIsDeletable()   const					{ return m_core_item->Deletable(); }
	virtual BOOL  GetSubfoldersAllowed() const				{ return m_core_item->SubFoldersAllowed(); }
	virtual BOOL  GetSeparatorsAllowed()					{ return m_core_item->SeparatorsAllowed(); }
	virtual INT32 GetMaxItems() const						{ return m_core_item->GetMaxCount(); }
	virtual BOOL  GetIsTrashFolder() const					{ return m_core_item == g_bookmark_manager->GetTrashFolder(); }
	virtual BOOL  GetHasDisplayUrl() const					{  GetStringAttribute(BOOKMARK_DISPLAY_URL, const_cast<OpString&>(this->m_display_url)); return this->m_display_url.HasContent(); }

	//	we don't set these attributes 
	virtual void SetIsDeletable(BOOL deletable) {OP_ASSERT(FALSE);}
	virtual void SetHasMoveIsCopy(BOOL copy){OP_ASSERT(FALSE);}
	virtual void SetSubfoldersAllowed(BOOL subfolders_allowed) {OP_ASSERT(FALSE);}
	virtual void SetSeparatorsAllowed(BOOL separators_allowed) {OP_ASSERT(FALSE);}
	virtual void SetMaxItems(INT32 max_num_items) {OP_ASSERT(FALSE);}

	virtual HotlistModelItem* CastToHotlistModelItem() {return this;}
	
	virtual const OpStringC&  GetName() const;

	virtual const OpStringC&  GetCreatedString();
	virtual const OpStringC& GetVisitedString();

	virtual time_t GetCreated() const;
	virtual time_t GetVisited() const;
	virtual void SetVisited(time_t value);
	virtual void SetCreated(time_t value);

protected:

	DesktopBookmark() : CoreBookmarkWrapper(0) {}

private:
	// Not implemented
	DesktopBookmark& operator=(const DesktopBookmark& item);

	OpString m_partner_id;
};

template<typename T>
time_t DesktopBookmark<T>::GetCreated() const
{
	OpString created;
	GetStringAttribute(BOOKMARK_CREATED, created);
	if(created.HasContent())
	{
		return OpDate::ParseDate(created.CStr())/1000;
	}
	else
		return 0;
}

template<typename T>
time_t DesktopBookmark<T>::GetVisited() const
{
	OpString visited;
	GetStringAttribute(BOOKMARK_VISITED, visited);
	if(visited.HasContent())
		return OpDate::ParseDate(visited.CStr())/1000;		
	else 
		return 0;
}

template<typename T>
void DesktopBookmark<T>::SetVisited(time_t value)
{
	if(value == 0)
	{
		ClearVisited();
	}
	else
	{
		OpString date;
		SyncUtil::CreateRFC3339Date(value, date);
		SetAttribute(BOOKMARK_VISITED, date);
	}
}

template<typename T>
void DesktopBookmark<T>::SetCreated( time_t value )
{
	if(value == 0)
	{
		ClearCreated();
	}
	else
	{
		OpString date;
		SyncUtil::CreateRFC3339Date(value, date);
		SetAttribute(BOOKMARK_CREATED, date);
	}
}

template<typename T>
const OpStringC&  DesktopBookmark<T>::GetCreatedString() 
{
	time_t created = GetCreated();

	const_cast<OpString&>(this->m_created_string).Empty();
	if(created != 0)
		BuildTimeString( const_cast<OpString&>(this->m_created_string), created, TRUE );
	return this->m_created_string;
}

template<typename T>
const OpStringC& DesktopBookmark<T>::GetVisitedString()	
{
	time_t visited = GetVisited();

	const_cast<OpString&>(this->m_visited_string).Empty();
	if(visited != 0)
		BuildTimeString( const_cast<OpString&>(this->m_visited_string), visited, TRUE );
	return this->m_visited_string;
}

template<typename T>
const OpStringC& DesktopBookmark<T>::GetName() const
{
	if (this->GetIsTrashFolder())
	{
		g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_TRASH, const_cast<OpString&>(this->m_name));
	}
	else
	{
		GetStringAttribute(BOOKMARK_TITLE, const_cast<OpString&>(this->m_name)); 
	}
	return this->m_name; 
}


#endif // _DESKTOP_BOOKMARK_H_
