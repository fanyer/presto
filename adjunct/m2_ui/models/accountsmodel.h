
#ifndef ACCOUNTSMODEL_H
#define ACCOUNTSMODEL_H

#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/index.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"

// ---------------------------------------------------------------------------------

class AccountsModelItem : public TreeModelItem<AccountsModelItem, AccountsModel>
{
	public:

									AccountsModelItem() {}
		virtual						~AccountsModelItem() {};

		virtual OP_STATUS			GetItemData(ItemData* item_data);
		virtual Type				GetType() {return m_type;}
		virtual int					GetID() {return m_account_id;}

		Type						m_type;
		UINT32						m_account_id;
		AccountTypes::AccountStatus	m_account_status;

	private:
		void GetBackendStatusText(const Account* account_ptr, BOOL incoming, OpString& text) const;
};

// ---------------------------------------------------------------------------------

class AccountsModel : public TreeModel<AccountsModelItem>, public AccountListener
{
	public:
									AccountsModel();
									~AccountsModel();

		virtual OP_STATUS			Init();

		virtual INT32				GetColumnCount();
		virtual OP_STATUS			GetColumnData(ColumnData* column_data);
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		virtual OP_STATUS			GetTypeString(OpString& type_string);
#endif

		// implmenting MessageEngine::AccountListener

		virtual void				OnAccountAdded(UINT16 account_id);
		virtual void				OnAccountRemoved(UINT16 account_id, AccountTypes::AccountType account_type);
		virtual void				OnAccountStatusChanged(UINT16 account_id);
		virtual void				OnFolderAdded(UINT16 account_id, const OpStringC& name, const OpStringC& path, BOOL subscribedLocally, INT32 subscribedOnServer, BOOL editable) {}
		virtual void				OnFolderRemoved(UINT16 account_id, const OpStringC& path) {}
		virtual void				OnFolderRenamed(UINT16 account_id, const OpStringC& old_path, const OpStringC& new_path) {}
		virtual void				OnFolderLoadingCompleted(UINT16 account_id) { }
};

#endif
