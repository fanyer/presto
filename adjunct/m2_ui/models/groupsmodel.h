
#ifndef GROUPSMODEL_H
#define GROUPSMODEL_H

#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/index.h"
#include "adjunct/m2/src/engine/listeners.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"

// ---------------------------------------------------------------------------------

class GroupsModelItem : public TreeModelItem<GroupsModelItem, GroupsModel>
{
	public:
									GroupsModelItem(Type type) : m_type(type), m_manually_added(FALSE), m_old_subscribed(-1), m_editable(TRUE) {}
		virtual						~GroupsModelItem() {};

		virtual OP_STATUS			GetItemData(ItemData* item_data);
		virtual Type				GetType() {return m_type;}
		virtual int					GetID() {return m_id;}

		virtual BOOL				IsManuallyAdded() const {return m_manually_added;}
		virtual void				SetIsManuallyAdded(BOOL manually) {m_manually_added = manually;}

		virtual UINT32				GetIsSubscribed() const {return m_subscribed;}
		virtual void				SetIsSubscribed(INT32 subscribed);
		virtual void				SetWasSubscribed(INT32 subscribed) {m_old_subscribed=subscribed;}
		virtual BOOL				SubscriptionIsChanged() const {return m_subscribed!=m_old_subscribed && m_old_subscribed!=-1;}

		virtual BOOL				IsEditable() const { return m_editable; }
		virtual void				SetEditable(BOOL editable) { m_editable = editable; }

		virtual OP_STATUS			GetName(OpString& name) const {return name.Set(m_name);}
		virtual const uni_char*		GetNameCStr() const {return m_name.CStr();}
		virtual OP_STATUS			SetName(const OpStringC& name);
		virtual int					CompareNameI(const OpStringC& name) {return m_name.CompareI(name);}
		virtual BOOL				NameIsChanged() const {return m_old_name.HasContent() && m_name.Compare(m_old_name)!=0;}

		virtual OP_STATUS			GetPath(OpString& path) const {return path.Set(m_path);}
		virtual const uni_char*		GetPathCStr() const {return m_path.CStr();}
		virtual OP_STATUS			SetPath(const OpStringC& path);
		virtual int					ComparePathI(const OpStringC& path) {return m_path.CompareI(path);}
		virtual BOOL				PathIsChanged() const {return m_old_path.HasContent() && m_path.Compare(m_old_path)!=0;}
		virtual OP_STATUS			GetOldPath(OpString& old_path) const {return old_path.Set(m_old_path);}

		virtual UINT32				GetIndexId();

		Type						m_type;
		UINT32						m_id;
		UINT16						m_account_id;

	private:
		BOOL						m_manually_added;
		INT32 						m_subscribed;
		INT32						m_old_subscribed;
		BOOL						m_editable;
		OpString					m_name;
		OpString					m_old_name; //Empty if m_name is not changed
		OpString					m_path;
		OpString					m_old_path; //Empty if m_path is not changed
};

// ---------------------------------------------------------------------------------

class GroupsModel : public TreeModel<GroupsModelItem>, public AccountListener
{
	public:
									GroupsModel();
									~GroupsModel();

		virtual OP_STATUS			Init(UINT16 account_id, BOOL read_only);

		virtual INT32				GetColumnCount();
		virtual OP_STATUS			GetColumnData(ColumnData* column_data);
		virtual INT32				CompareItems(INT32 column, OpTreeModelItem* item1, OpTreeModelItem* item2);
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		virtual OP_STATUS			GetTypeString(OpString& type_string);
#endif

		virtual INT32				IncRefCount() { return ++m_ref_count; };
		virtual INT32				DecRefCount() { return --m_ref_count; };

		virtual INT32				AddFolder(const OpStringC& name, const OpStringC& path, BOOL subscribedLocally, INT32 subscribedOnServer, BOOL editable, BOOL manually = TRUE, BOOL force_create = FALSE);
		virtual UINT16			GetAccountId() { return m_account_id; };
		INT32						FindParent(const OpStringC& path);
		virtual OP_STATUS			SubscribeFolder(UINT32 id, BOOL subscribe);
		INT32						GetItemByPath(const OpStringC& path);
		virtual OP_STATUS			UpdateFolder(UINT32 item_id, const OpStringC& path, const OpStringC& name);

		virtual void				Commit();

		BOOL						FolderLoadingCompleted() const { return m_folder_loading_completed; }
		void						SetFolderLoadingCompleted(BOOL completed) { m_folder_loading_completed = completed; }

		void						Refresh();

		// AccountListener
		virtual void				OnAccountAdded(UINT16 account_id) {};
		virtual void				OnAccountRemoved(UINT16 account_id, AccountTypes::AccountType account_type) {};
		virtual void				OnAccountStatusChanged(UINT16 account_id) {};
		virtual void				OnFolderAdded(UINT16 account_id, const OpStringC& name, const OpStringC& path, BOOL subscribedLocally, INT32 subscribedOnServer, BOOL editable);
		virtual void				OnFolderRemoved(UINT16 account_id, const OpStringC& path);
		virtual void				OnFolderRenamed(UINT16 account_id, const OpStringC& old_path, const OpStringC& new_path);
		virtual void				OnFolderLoadingCompleted(UINT16 account_id);

	private:
		OpStringHashTable<GroupsModelItem>	m_groups_hash_table;
		UINT16							m_account_id;
		UINT32							m_next_id;
		OpTypedObject::Type					m_folder_type;
		BOOL								m_read_only;
		INT32								m_ref_count;

		BOOL								m_folder_loading_completed;
};

#endif
