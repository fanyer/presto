/** IndexGroup
 *
 * @author Owner:    Alexander Remen
 *
 * An IndexGroup can combine together several indexes and make a result index. 
 *
 * There are 3 main types of IndexGroups:
 * - UnionIndexGroup: messages in at least one of index are present in the result index. Eg: feed folders, mailing list folders, Category view
 * - IntersectionIndexGroup: messages present in all indexes + base index are present in the result index. Eg: POP Sent view and Filters that search in certain indexes
 * - ComplementIndexGroup: messages present in the base index but not in any of the other indexes appear in the result index. Eg: POP Inbox view
 *
 * Variants of the UnionIndexGroup exist, these automatically add indexes based on a range, on child indexes or a certain type.
 *
 * Important: If an Index* is not passed when creating an IndexGroup, then it will be added to the list of indexes in Indexer::m_index_container. When deleting the IndexGroup, 
 * the Index* m_index will not be deleted automatically. You need to call Indexer::RemoveIndex() if you want to delete it. It will be destructed on exit with the other indexes otherwise.
 *
 */
#ifndef INDEXGROUP_H
#define INDEXGROUP_H

#ifdef M2_SUPPORT

#include "adjunct/m2/src/engine/listeners.h"
#include "adjunct/m2/src/engine/index.h"
#include "adjunct/desktop_util/adt/opsortedvector.h"

class IndexGroup : public Index::Observer, public IndexerListener
{
	public:
		IndexGroup(index_gid_t index_id, Index* index_to_use = NULL); ///< sets the id for the internal index
		~IndexGroup();

		Index*				GetIndex() { return m_index; };

		virtual OP_STATUS	SetBase(index_gid_t index_id);

		virtual OP_STATUS	AddIndex(index_gid_t index_id) = 0;
		
		void				Empty(); ///< removes all listeners etc.

		bool				IsIndexInGroup(index_gid_t index_id) { return m_indexes.Contains(index_id) ? true : false; }

		/**	Useful functions when the messages in the m_index us saved to disk on Opera restart
		  */
		virtual OP_STATUS	AddIndexWithoutAddingMessages(index_gid_t index_id);
		virtual OP_STATUS	SetBaseWithoutAddingMessages(index_gid_t index_id);

		// Index::Observer
		virtual OP_STATUS MessageAdded(Index* index, message_gid_t message, BOOL setting_keyword) = 0;
		virtual OP_STATUS MessageRemoved(Index* index, message_gid_t message, BOOL setting_keyword) = 0;
		virtual OP_STATUS StatusChanged(Index* index) { return OpStatus::OK; }
		virtual OP_STATUS NameChanged(Index* index) { return OpStatus::OK; }
		virtual OP_STATUS VisibleChanged(Index* index) { return OpStatus::OK; }
		virtual OP_STATUS IconChanged(Index* index) { return OpStatus::OK; }

		// Indexer::IndexerListener
		virtual OP_STATUS IndexAdded(Indexer *indexer, UINT32 index_id);
		virtual OP_STATUS IndexRemoved(Indexer *indexer, UINT32 index_id);
		virtual OP_STATUS IndexChanged(Indexer *indexer, UINT32 index_id) { return OpStatus::OK; }
		virtual OP_STATUS IndexNameChanged(Indexer *indexer, UINT32 index_id) { return OpStatus::OK; }
		virtual OP_STATUS IndexIconChanged(Indexer* indexer, UINT32 index_id) { return OpStatus::OK; }
		virtual OP_STATUS IndexVisibleChanged(Indexer *indexer, UINT32 index_id) { return OpStatus::OK; }
		virtual OP_STATUS IndexKeywordChanged(Indexer *indexer, UINT32 index_id, const OpStringC8& old_keyword, const OpStringC8& new_keyword) { return OpStatus::OK; }
		virtual OP_STATUS KeywordAdded(Indexer* indexer, message_gid_t message_id, const OpStringC8& keyword) { return OpStatus::OK; }
		virtual OP_STATUS KeywordRemoved(Indexer* indexer, message_gid_t message_id, const OpStringC8& keyword) { return OpStatus::OK; }

	protected:
		Index*				m_index;
		UINT32				m_index_id;
		Index*				m_base;
		UINT32				m_base_id;
		Indexer*			m_indexer;
		OpINTSortedVector	m_indexes;
};

class UnionIndexGroup : public IndexGroup
{
	public:
		UnionIndexGroup(index_gid_t index_id = 0) : IndexGroup(index_id) {}
		virtual OP_STATUS	SetBase(index_gid_t index_id) { return AddIndex(index_id); }
		virtual OP_STATUS	AddIndex(index_gid_t index_id);

		virtual OP_STATUS	MessageAdded(Index* index, message_gid_t message, BOOL setting_keyword);
		virtual OP_STATUS	MessageRemoved(Index* index, message_gid_t message, BOOL setting_keyword);
};

class IntersectionIndexGroup : public IndexGroup
{
	public:
		IntersectionIndexGroup(index_gid_t index_id = 0, Index* index_to_use = NULL) : IndexGroup(index_id, index_to_use) {}

		virtual OP_STATUS	AddIndex(index_gid_t index_id);
		
		virtual OP_STATUS	MessageAdded(Index* index, message_gid_t message, BOOL setting_keyword);
		virtual OP_STATUS	MessageRemoved(Index* index, message_gid_t message, BOOL setting_keyword);

		index_gid_t			GetIntersectingIndexid() { return m_indexes.GetByIndex(0); }
		index_gid_t			GetBaseId() { return m_base_id; }
};

class ComplementIndexGroup : public IndexGroup
{
	public:
		ComplementIndexGroup(index_gid_t index_id = 0, Index* index_to_use = NULL) : IndexGroup(index_id, index_to_use) {}

		virtual OP_STATUS	AddIndex(index_gid_t index_id);

		virtual OP_STATUS	MessageAdded(Index* index, message_gid_t message, BOOL setting_keyword);
		virtual OP_STATUS	MessageRemoved(Index* index, message_gid_t message, BOOL setting_keyword);
};

class IndexGroupRange : public UnionIndexGroup
{
	public:
		/** Constructor; creates a group that follows a range of indexes
		  */
		IndexGroupRange(index_gid_t index_id, index_gid_t range_start, index_gid_t range_end);

		// Indexer::IndexerListener
		OP_STATUS IndexAdded(Indexer *indexer, UINT32 index_id);

	private:
		index_gid_t m_or_range_start;
		index_gid_t m_or_range_end;
};

class IndexGroupWatched : public UnionIndexGroup
{
	public:
		/** Constructor; creates a group that follows a range of indexes that are watched
		  */
		IndexGroupWatched(index_gid_t index_id, index_gid_t range_start, index_gid_t range_end);

		// Indexer::IndexerListener
		OP_STATUS IndexAdded(Indexer *indexer, UINT32 index_id);
		OP_STATUS IndexVisibleChanged(Indexer *indexer, UINT32 index_id);

	private:
		index_gid_t m_or_range_start;
		index_gid_t m_or_range_end;
};

class IndexGroupMailingLists : public UnionIndexGroup
{
	public:
		/** Constructor
		  */
		IndexGroupMailingLists(index_gid_t index_id);
	
		// Indexer::IndexerListener
		OP_STATUS IndexAdded(Indexer *indexer, UINT32 index_id);
};

class FolderIndexGroup : public UnionIndexGroup
{
	public:
		/** Constructor
		  */
		FolderIndexGroup(index_gid_t index_id);

		virtual OP_STATUS IndexParentIdChanged(Indexer *indexer, UINT32 index_id, UINT32 old_parent_id, UINT32 new_parent_id);
};

#endif //M2_SUPPORT
#endif // INDEXGROUP_H