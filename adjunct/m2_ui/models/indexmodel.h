/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef INDEXMODEL_H
#define INDEXMODEL_H

#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/index.h"
#include "adjunct/m2/src/engine/listeners.h"
#include "adjunct/m2/src/engine/store.h"
#include "adjunct/m2/src/engine/store/storemessage.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/desktop_util/adt/opsortedvector.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/widgets/OpButton.h"
#include "modules/skin/OpSkinManager.h"

class IndexModel;

// ---------------------------------------------------------------------------------

class GenericIndexModelItem : public TreeModelItem<GenericIndexModelItem, IndexModel>
{
public:
						GenericIndexModelItem() : m_is_open(FALSE) {}
	virtual				~GenericIndexModelItem() {}

	virtual OP_STATUS	GetItemData(ItemData* item_data) = 0;

	Type				GetType() { return INDEX_TYPE; }
	virtual bool		IsHeader() const { return false; }
	virtual INT32		GetID()	= 0;
	virtual	time_t		GetDate() = 0;
	virtual BOOL		IsFlagSet(StoreMessage::Flags flag) =0;

	bool				operator>  (GenericIndexModelItem& item) { return CompareWith(item) > 0; };
	bool				operator<  (GenericIndexModelItem& item) { return CompareWith(item) == 0; };
	bool				operator== (GenericIndexModelItem& item) { return CompareWith(item) < 0; };

	virtual int			CompareWith (GenericIndexModelItem& item) = 0;

	virtual INTPTR		GetGroup(INT32 grouping_method) const { void* ret; m_groups.GetData(grouping_method, &ret); return reinterpret_cast<INT64>(ret); }
protected:
	OpINT32HashTable<void> m_groups;
	friend class IndexModel;
	
	static INT32 s_item_id;

	BOOL m_is_open;
	GenericIndexModelItem(const GenericIndexModelItem& item);
	GenericIndexModelItem& operator=(const GenericIndexModelItem&);

};

// ---------------------------------------------------------------------------------

class IndexModelItem : public GenericIndexModelItem
{
public:

	IndexModelItem(int id) : m_id(id), m_is_fetched(false), m_is_fetching(false) { m_date = g_m2_engine->GetStore()->GetMessageDate(id); }
	virtual				~IndexModelItem();

	virtual int			GetNumLines();
	virtual OP_STATUS	GetItemData(ItemData* item_data);

	INT32				GetID()	{return m_id;}

	void				SetIsFetched(bool fetched) { m_is_fetched = fetched; m_is_fetching = m_is_fetching && !m_is_fetched; }

	virtual int			CompareWith(GenericIndexModelItem& item);
	virtual	time_t		GetDate() { return m_date; }
	virtual BOOL		IsFlagSet(StoreMessage::Flags flag) { return  g_m2_engine->GetStore()->GetMessageFlag(GetID(), flag); }

	bool				operator>  (const IndexModelItem& item) const { return m_id >  item.m_id; };
	bool				operator<  (const IndexModelItem& item) const { return m_id <  item.m_id; };
	bool				operator== (const IndexModelItem& item) const { return m_id == item.m_id; };
private:
	friend class IndexModel;

	OP_STATUS GetInfo(Message& message, ItemData* item_data);
	OP_STATUS GetAssociatedText(Message& message, UINT32 text_color, UINT32 associated_text_color, UINT32 unread_count, ItemData* item_data);
	void GetStatusColumnData(Message& message, ItemData* item_data);
	OP_STATUS GetFromColumnData(Message& message, ItemData* item_data);
	OP_STATUS GetSubjectColumnData(Index* in_thread, Message& message, UINT32 unread_count, ItemData* item_data);
	OP_STATUS GetLabelColumnData(Message& message, ItemData* item_data);
	OP_STATUS GetSizeColumnData(Message& message, ItemData* item_data);
	OP_STATUS GetSentDateColumnData(Message& message, ItemData* item_data);
	const message_gid_t m_id;

	bool m_is_fetched;
	bool m_is_fetching;

	time_t m_date;

	IndexModelItem(const IndexModelItem& item);
	IndexModelItem& operator=(const IndexModelItem&);
};

// ---------------------------------------------------------------------------------

class HeaderModelItem : public GenericIndexModelItem
{
	class CompareItems
	{
	public:
		virtual int		CompareWith(GenericIndexModelItem& item, IndexModel& model) = 0;
		virtual bool	IsInGroup(GenericIndexModelItem& item, IndexModel& model) = 0;
		virtual time_t	GetDate() { return 0; }
		virtual BOOL	IsFlagSet(StoreMessage::Flags flag) { return FALSE; }
	};

	class DateHeaderData : public CompareItems
	{
	public:
		DateHeaderData(time_t begin_time, time_t end_time) : m_begin_time(begin_time), m_end_time(end_time) {}
		virtual int		CompareWith(GenericIndexModelItem& item, IndexModel& model);
		virtual bool	IsInGroup(GenericIndexModelItem& item, IndexModel& model);
		virtual time_t	GetDate() { return m_end_time; }
	private:
		friend class HeaderModelItem;

		time_t m_begin_time;
		time_t m_end_time;
	};

	class FlagHeaderData : public CompareItems
	{
	public:
		FlagHeaderData(StoreMessage::Flags flag, BOOL is_set, BOOL is_main_header) : m_flag(flag), m_is_set(is_set), m_is_main_header(is_main_header) {}
		virtual int		CompareWith(GenericIndexModelItem& item, IndexModel& model) { return item.IsHeader() ? m_is_main_header ? 1 : -1 : m_is_set - (item.IsFlagSet(m_flag) ? 1 : 0); }
		virtual bool	IsInGroup(GenericIndexModelItem& item, IndexModel& model) { return CompareWith(item, model) == 0; }
		virtual BOOL	IsFlagSet(StoreMessage::Flags flag) { return (flag == m_flag && m_is_set == TRUE); }
	private:
		friend class HeaderModelItem;

		StoreMessage::Flags m_flag;
		BOOL m_is_set;
		BOOL m_is_main_header;
	};

public:
	static const time_t DayDuration = 24 * 3600;
	static const time_t WeekDuration = 7 * DayDuration;
public:
	static			HeaderModelItem* ConstructDateHeader(Str::LocaleString name, time_t begin_time, time_t end_time, IndexTypes::GroupingMethods grouping_method);
	static			HeaderModelItem* ConstructFlagHeader(Str::LocaleString name, StoreMessage::Flags, BOOL is_set, BOOL is_main_header, IndexTypes::GroupingMethods grouping_method);
	virtual			~HeaderModelItem() {}

	virtual bool		IsHeader() const { return true; }
	virtual OP_STATUS	GetItemData(ItemData* item_data);
	
	INT32			GetID() { return m_id; }
	virtual time_t	GetDate();
	virtual BOOL	IsFlagSet(StoreMessage::Flags flag) { return m_data->IsFlagSet(flag); }
	virtual int		CompareWith(GenericIndexModelItem& item);
	bool			IsHeaderForItem(GenericIndexModelItem& item) { return m_data->IsInGroup(item, *GetModel()); }
	INT32			GetGroupingMethod() const { return m_grouping_method; }
private:
	OpString					m_name;
	OpAutoPtr<CompareItems>		m_data;
	INT32						m_grouping_method;
	INT32						m_id;

	HeaderModelItem(CompareItems* data, int grouping_method, float ratio = 0): GenericIndexModelItem(), m_data(data), m_grouping_method(grouping_method){ m_is_open = true; m_id = s_item_id--; }
	HeaderModelItem(const HeaderModelItem& item);
	HeaderModelItem& operator=(const HeaderModelItem&);
};

// ---------------------------------------------------------------------------------

class IndexModel
  : public TreeModel<GenericIndexModelItem>
  , public Index::Observer
  , public MessageListener
  , public MessageObject
  , public OpTreeModel::TreeModelGrouping
  , public OpTimerListener
{
	public:
		IndexModel();
		~IndexModel();

		class SortItem
		{
		public:
			INT32					m_pos;
			INT32					m_value;

			bool					operator>  (const SortItem& item) { return m_value >  item.m_value; };
			bool					operator<  (const SortItem& item) { return m_value <  item.m_value; };
			bool					operator== (const SortItem& item) { return m_value == item.m_value; };
		};

		virtual OP_STATUS			Init(Index* index) {return ReInit(index);}
		virtual OP_STATUS			ReInit(Index* index = NULL);
		void						Empty();

		virtual UINT32				IncRefCount() { return ++m_refcount; };
		virtual UINT32				DecRefCount() { return --m_refcount; };

		virtual UINT32				GetIndexId() { return (m_index ? m_index->GetId() : 0); };
		virtual Index*				GetIndex() { return m_index; };
		virtual UINT32				GetUnreadChildCount(INT32 pos, BOOL& unseen_messages);
		virtual INT32				GetStartPos() { return m_start_pos; };

		virtual INT32				GetPosition(message_gid_t message_id);

		OP_STATUS					GetThreadIds(message_gid_t id, OpINT32Vector& thread_ids) const;

		GenericIndexModelItem*				GetLastInThread(GenericIndexModelItem* root, GenericIndexModelItem* last = NULL);
		void						SetOpenState(message_gid_t id, BOOL open);
		void						SetTwoLinedItems(bool two_lined_items);
		bool						HasItemTwoLines(GenericIndexModelItem* item);
		bool						HasTwoLinedItems() { return m_two_lined_items; }

		BOOL						DelayedItemData(message_gid_t message_id);
		BOOL						IsThreaded(){ return m_threaded; }
		BOOL						Matches(message_gid_t message_id)
			{ return m_current_match.get() && m_current_match->Contains(message_id); }
		void						SetCurrentMatch(OpINTSortedVector* matches) { m_current_match.reset(matches); }
		int							GetTimeFormat(time_t date);

		// Functions implementing the OpTreeModel interface

		virtual INT32				GetColumnCount() { return COLUMN_COUNT; }
		virtual OP_STATUS			GetColumnData(ColumnData* column_data);
		virtual INT32				CompareItems(INT32 column, OpTreeModelItem* item1, OpTreeModelItem* item2);
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		virtual OP_STATUS			GetTypeString(OpString& type_string);
#endif

		// Functions implementing the Index::Observer interface

		virtual OP_STATUS			MessageAdded(Index* index, message_gid_t message, BOOL setting_keyword = FALSE);
		virtual OP_STATUS			MessageRemoved(Index* index, message_gid_t message, BOOL setting_keyword = FALSE);
		virtual OP_STATUS			StatusChanged(Index* index) { return OpStatus::OK; }
		virtual OP_STATUS			NameChanged(Index* index) { return OpStatus::OK; }
		virtual OP_STATUS			VisibleChanged(Index* index) { return OpStatus::OK; }
		virtual OP_STATUS			IconChanged(Index* index) { return OpStatus::OK; }

		// Functions implementing the MessageEngine::MessageListener

		virtual void				OnMessageBodyChanged(message_gid_t message_id) {}
		virtual void				OnMessageChanged(message_gid_t message_id);

		// From MessageObject
		virtual void				HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
		
		enum Columns
		{
			StatusColumn = 0,
			FromColumn,
			SubjectColumn,
			TwoLineFromSubjectColumn,
			LabelColumn,
			LabelIconColumn,
			SizeColumn,
			SentDateColumn,
			COLUMN_COUNT // always at the end
		};

		Store::SortType				SortBy() { return m_sort_by; }
		INT32						GetGroupingMethod() const { return m_grouping_method; }

		// From TreeViewGrouping

		virtual INT32				GetGroupForItem(const OpTreeModelItem* item) const;
		virtual INT32				GetGroupCount() const { return m_groups_map.Get(GetGroupingMethod()) ? m_groups_map.Get(GetGroupingMethod())->GetCount() : 0; }
		virtual OpTreeModelItem*	GetGroupHeader(UINT32 group_idx) const {return m_groups_map.Get(GetGroupingMethod()) ? m_groups_map.Get(GetGroupingMethod())->Get(group_idx) : NULL; }
		virtual BOOL				IsVisibleHeader(const OpTreeModelItem* header) const;
		virtual BOOL				HasGrouping() const { return GetGroupingMethod() != IndexTypes::GROUP_BY_NONE;}

		// From OpTimerListener
		virtual void OnTimeOut(OpTimer* timer) { ReInit(); }
	private:

		OpSortedVector<IndexModelItem>	m_copy;	///< balanced search tree of internal list for faster threading
		OpVector< OpVector<HeaderModelItem> > m_groups_map; ///< contains items which aren't messages e.g headers and empty rows

		Index*							m_index;	///< pointer to internal index
		Index*							m_trash;	///< pointer to trash index

		UINT32							m_refcount;	///< number of views using this model currently
		INT32							m_start_pos;///< item to show as top item in view (typically first unread)

		OP_STATUS						FindThreadedMessages(message_gid_t id, INT32 &got_tree_index, INT32 insert_before = -1);

		message_gid_t					FindVisibleMessage(message_gid_t id);

		BOOL							IndexHidden(index_gid_t index_id);
		BOOL							HideableIndex(index_gid_t index_id);

		void							SetParentForMails(UINT32 start, UINT32 stop);

		void							ManageHeaders();
		void							CreateHeadersForGroupingByDate();
		void							AddHeaderToGroup(HeaderModelItem* item);
		void							AddDays(time_t &date, int num_of_days);
		
		INTPTR							FindGroupForItem(GenericIndexModelItem* item, OpVector<HeaderModelItem>* groups, GenericIndexModelItem* exclude_item = NULL, BOOL exclude_tree = FALSE);
		INTPTR							FindGroup(GenericIndexModelItem* item, OpVector<HeaderModelItem>* groups);
		void							CacheGroupIds();
		void							CacheGroupIdsForItem(GenericIndexModelItem* item, BOOL exclude_item = FALSE, BOOL exclude_item_subtree = FALSE);


		// dummy copy constructor and assignment operator
		IndexModel(const IndexModel&);
		IndexModel& operator=(const IndexModel&);

		INT32							m_model_flags;
		IndexTypes::GroupingMethods		m_grouping_method;
		IndexTypes::ModelType			m_model_type;
		IndexTypes::ModelAge			m_model_age;
		IndexTypes::ModelSort			m_model_sort;
		BOOL							m_model_sort_ascending;
		bool							m_is_waiting_for_delayed_message;
		bool							m_two_lined_items;		///< if each item should have two lines
		bool							m_threaded;				///< threaded view or not

		time_t							m_delayed_item_check;
		UINT32							m_delayed_item_count;
		OpINT32Vector					m_delayed_items;
		OpAutoPtr<OpINTSortedVector>	m_current_match;
		Store::SortType					m_sort_by;
		OpTimer							m_timer;
		time_t							m_begin_of_last_week;
		time_t							m_begin_of_this_week;
		time_t							m_begin_of_this_day;
};

#endif
