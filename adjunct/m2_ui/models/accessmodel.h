
#ifndef ACCESSMODEL_H
#define ACCESSMODEL_H

#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/engine/listeners.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"

class AccessModel;

/***********************************************************************************
**
**	AccessModelItem
**
***********************************************************************************/

class AccessModelItem : public TreeModelItem<AccessModelItem, AccessModel>
{
	public:

							AccessModelItem() {}
							AccessModelItem(int id) { m_id = id; m_is_sorting = FALSE;}
		virtual				~AccessModelItem() {}

		virtual OP_STATUS	GetItemData(ItemData* item_data);

		virtual Type		GetType() { return (INDEX_TYPE); }

		virtual int			GetID()
		{
			return m_id;
		}
		virtual void		PrepareForSorting() { m_is_sorting = TRUE; }

	private:
		BOOL		IsCategory(index_gid_t index_id);
		OP_STATUS	GetItemDescription(OpString& description);

		int			m_id;
		BOOL		m_is_sorting;

		AccessModelItem(const AccessModelItem&);
		AccessModelItem& operator=(const AccessModelItem&);
};

/***********************************************************************************
**
**	AccessModel
**
***********************************************************************************/

class AccessModel
  : public TreeModel<AccessModelItem>
  , public IndexerListener
  , public EngineListener
  , public ProgressInfoListener
{
	public:
		AccessModel(UINT32 category, Indexer* indexer);

		void						ShowHiddenIndexes() { m_show_hidden_indexes = TRUE; }

		UINT32						GetCategoryID() { return m_category_id; }
		~AccessModel();

		virtual OP_STATUS			Init();
		void						ReInit();

		virtual INT32				GetColumnCount();
		virtual OP_STATUS			GetColumnData(ColumnData* column_data);
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		virtual OP_STATUS			GetTypeString(OpString& type_string);
#endif

		// Functions implementing the Indexer::Observer interface

		virtual OP_STATUS			IndexAdded(Indexer *indexer, UINT32 index_id);
		virtual OP_STATUS			IndexRemoved(Indexer *indexer, UINT32 index_id);
		virtual OP_STATUS			IndexChanged(Indexer *indexer, UINT32 index_id) { return OpStatus::OK; }
		virtual OP_STATUS			IndexNameChanged(Indexer *indexer, UINT32 index_id);
		virtual OP_STATUS			IndexVisibleChanged(Indexer *indexer, UINT32 index_id);
		virtual OP_STATUS			IndexParentIdChanged(Indexer *indexer, UINT32 index_id,  UINT32 old_parent_id, UINT32 new_parent_id);
		virtual OP_STATUS			IndexKeywordChanged(Indexer *indexer, UINT32 index_id, const OpStringC8& old_keyword, const OpStringC8& new_keyword) { return OpStatus::OK; }
		virtual OP_STATUS			IndexIconChanged(Indexer* indexer, UINT32 index_id) { return AccessModel::IndexNameChanged(indexer, index_id); }
		virtual OP_STATUS			KeywordAdded(Indexer* indexer, message_gid_t message_id, const OpStringC8& keyword) { return OpStatus::OK; }
		virtual OP_STATUS			KeywordRemoved(Indexer* indexer, message_gid_t message_id, const OpStringC8& keyword) { return OpStatus::OK; }

		// Functions implementing the MessageEngine::EngineListener interface

		virtual void				OnImporterProgressChanged(const Importer* importer, const OpStringC& infoMsg, OpFileLength current, OpFileLength total, BOOL simple = FALSE) {}
		virtual void				OnImporterFinished(const Importer* importer, const OpStringC& infoMsg) {}
		virtual void				OnIndexChanged(UINT32 index_id);
		virtual void				OnActiveAccountChanged();
		virtual void				OnReindexingProgressChanged(INT32 progress, INT32 total) {};

		// Functions implementing the ProgressInfoListener interface
		virtual void				OnProgressChanged(const ProgressInfo& progress) {}
		virtual void				OnSubProgressChanged(const ProgressInfo& progress) {}
		virtual void				OnStatusChanged(const ProgressInfo& progress);

	private:
		
		BOOL						BelongsToThisModel(Index* index);

		UINT32						m_category_id;
		BOOL						m_show_hidden_indexes;

		Indexer*					m_indexer;

		BOOL						IsHiddenAccount(Index* index);	///< Temporarily hide POP-accounts

		void						AddFolderItem(Index* index);

		AccessModel();

		// dummy copy constructor and assignment operator
		AccessModel(const AccessModel&);
		AccessModel& operator=(const AccessModel&);
};

#endif
