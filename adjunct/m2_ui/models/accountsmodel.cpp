
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "accountsmodel.h"
#include "chattersmodel.h"
#include "modules/inputmanager/inputaction.h"
#include "adjunct/m2/src/engine/message.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/store.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "modules/locale/oplanguagemanager.h"
//#include "modules/prefs/prefsmanager/prefstypes.h"

/***********************************************************************************
**
**	AccountsModelItem
**
***********************************************************************************/

OP_STATUS AccountsModelItem::GetItemData(ItemData* item_data)
{
	Account* account = MessageEngine::GetInstance()->GetAccountById(m_account_id);
	if (!account)
	{
		return OpStatus::ERR;
	}

	if (item_data->query_type == MATCH_QUERY)
	{
		AccountTypes::AccountType account_type = account->GetIncomingProtocol();

		const BOOL is_mail = (account_type == AccountTypes::POP) || (account_type == AccountTypes::IMAP) || (account_type == AccountTypes::NEWS);
		const BOOL is_newsfeed = (account_type == AccountTypes::RSS);

		if (!item_data->match_query_data.match_type || (int)item_data->match_query_data.match_type == account_type
			|| (item_data->match_query_data.match_type == MATCH_STANDARD && !is_newsfeed)
			|| (item_data->match_query_data.match_type == MATCH_MAIL && is_mail)
			|| (item_data->match_query_data.match_type == MATCH_MAIL_AND_NEWSFEEDS && (is_mail || is_newsfeed)))
		{
			item_data->flags |= FLAG_MATCHED;
		}
		return OpStatus::OK;
	}

	if (item_data->query_type == INFO_QUERY)
	{
		OpString label, text;
		//OpStatus::Ignore(account->GetAccountProgressText(text, &progress));
		item_data->info_query_data.info_text->SetStatusText(text.CStr());

		OpStatus::Ignore(g_languageManager->GetString(Str::S_ACCOUNT_ACCOUNT, label));
		OpStatus::Ignore(text.Set(account->GetAccountName()));

		item_data->info_query_data.info_text->AddTooltipText(label.CStr(), text.CStr());

		OpStatus::Ignore(g_languageManager->GetString(Str::D_M2_ACCOUNT_PROPERTIES_INCOMING, label));
		GetBackendStatusText(account, TRUE, text);

		item_data->info_query_data.info_text->AddTooltipText(label.CStr(), text.CStr());

		OpStatus::Ignore(g_languageManager->GetString(Str::D_M2_ACCOUNT_PROPERTIES_OUTGOING, label));
		GetBackendStatusText(account, FALSE, text);

		item_data->info_query_data.info_text->AddTooltipText(label.CStr(), text.CStr());

		return OpStatus::OK;
	}

	if (item_data->query_type != COLUMN_QUERY)
	{
		return OpStatus::OK;
	}

	switch (item_data->column_query_data.column)
	{
		case 0:			// name
			switch (account->GetIncomingProtocol())
			{
				case AccountTypes::POP:		item_data->column_query_data.column_image = account->GetLowBandwidthMode() ? "Account POP LBM" : "Account POP"; break;
				case AccountTypes::IMAP:	item_data->column_query_data.column_image = account->GetLowBandwidthMode() ? "Account IMAP LBM" : "Account IMAP"; break;
				case AccountTypes::NEWS:	item_data->column_query_data.column_image = "Account News"; break;
				case AccountTypes::IMPORT:	item_data->column_query_data.column_image = "Account Import"; break;
#ifdef OPERAMAIL_SUPPORT
				case AccountTypes::WEB:		item_data->column_query_data.column_image = "Account Operamail"; break;
#endif // OPERAMAIL_SUPPORT
				case AccountTypes::IRC:		item_data->column_query_data.column_image = "Account IRC"; break;
#ifdef JABBER_SUPPORT
				case AccountTypes::JABBER:	item_data->column_query_data.column_image = "Account IRC"; break;
#endif // JABBER_SUPPORT
				default:	break;
			}
			item_data->column_query_data.column_text->Set(account->GetAccountName());
			break;
		case 1:			// type
		{
			RETURN_IF_ERROR(item_data->column_query_data.column_text->Set(Account::GetProtocolName(account->GetIncomingProtocol())));
			break;
		}
	}

	return OpStatus::OK;
}

void AccountsModelItem::GetBackendStatusText(const Account* account_ptr, BOOL incoming, OpString& text) const
{
	text.Empty();
	if (!account_ptr)
		return;

	if (incoming)
		OpStatus::Ignore(text.Set(Account::GetProtocolName(account_ptr->GetIncomingProtocol())));
	else
		OpStatus::Ignore(text.Set(Account::GetProtocolName(account_ptr->GetOutgoingProtocol())));

	if (text.HasContent())
		OpStatus::Ignore(text.Append(UNI_L(": ")));
}

/***********************************************************************************
**
**	AccountsModel
**
***********************************************************************************/

AccountsModel::AccountsModel()
{
}


AccountsModel::~AccountsModel()
{
	MessageEngine::GetInstance()->RemoveAccountListener(this);
}


OP_STATUS AccountsModel::Init()
{
	return MessageEngine::GetInstance()->AddAccountListener(this);
}

INT32 AccountsModel::GetColumnCount()
{
	return 3;
}

OP_STATUS AccountsModel::GetColumnData(ColumnData* column_data)
{
	if (column_data->column < 2)
	{
		OpString account, type;

		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_ACCOUNT_ACCOUNT, account));
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_ACCOUNT_TYPE, type));
		uni_char* strings[] = {account.CStr(), type.CStr()};

		column_data->text.Set(strings[column_data->column]);
	}

	return OpStatus::OK;
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
OP_STATUS AccountsModel::GetTypeString(OpString& type_string)
{
	return g_languageManager->GetString(Str::S_ACCOUNT_ACCOUNT, type_string);
}
#endif

/***********************************************************************************
**
**	OnAccountAdded
**
***********************************************************************************/

void AccountsModel::OnAccountAdded(UINT16 account_id)
{
	AccountsModelItem* item = OP_NEW(AccountsModelItem, ());
	if (!item)
		return;

	item->m_type = OpTreeModelItem::ACCOUNT_TYPE;
	item->m_account_id = account_id;

	AddLast(item);
}

/***********************************************************************************
**
**	OnAccountRemoved
**
***********************************************************************************/

void AccountsModel::OnAccountRemoved(UINT16 account_id, AccountTypes::AccountType account_type)
{
	AccountsModelItem* item = GetItemByID(account_id);

	if (item)
		item->Delete();
}

/***********************************************************************************
**
**	OnAccountStatusChanged
**
***********************************************************************************/

void AccountsModel::OnAccountStatusChanged(UINT16 account_id)
{
	AccountsModelItem* item = GetItemByID(account_id);

	if (item)
		item->Change();
}

#endif //M2_SUPPORT
