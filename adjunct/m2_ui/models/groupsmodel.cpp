
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "groupsmodel.h"
#include "adjunct/m2/src/engine/message.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/engine/store.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/quick/Application.h"
#include "modules/locale/oplanguagemanager.h"

/***********************************************************************************
**
**	GroupsModelItem
**
***********************************************************************************/

OP_STATUS GroupsModelItem::GetItemData(ItemData* item_data)
{
	if (item_data->query_type == INIT_QUERY)
	{
		if (m_subscribed)
		{
			item_data->flags |= FLAG_INITIALLY_CHECKED;
		}
		if (!IsEditable())
		{
			item_data->flags |= FLAG_INITIALLY_DISABLED;
		}
		return OpStatus::OK;
	}

	// note that for chat rooms, m_name = topic and m_path = room name

	OpString& name = m_type == CHATROOM_TYPE ? m_path : m_name;

	if (item_data->query_type == MATCH_QUERY)
	{
		if (item_data->match_query_data.match_text->HasContent() && uni_stristr(name.CStr(), item_data->match_query_data.match_text->CStr()))
		{
			item_data->flags |= FLAG_MATCHED;
		}

		return OpStatus::OK;
	}

	if (item_data->query_type != COLUMN_QUERY)
	{
		return OpStatus::OK;
	}

	switch (item_data->column_query_data.column)
	{
		case 0:			// name
			item_data->column_query_data.column_text->Set(name);
			break;
		case 1:			// status
		{
			if (m_type == CHATROOM_TYPE)
			{
				if (m_subscribed)
				{
					uni_char users[16];
					uni_ltoa(m_subscribed, users, 10);
					item_data->column_query_data.column_text->Set(users);
				}
			}
			else
			{
				if (m_subscribed)
				{
					OpString subscribed;
					RETURN_IF_ERROR(g_languageManager->GetString(Str::S_SUBSCRIBED, subscribed));
					item_data->column_query_data.column_text->Set(subscribed);
				}
				else
				{
					item_data->column_query_data.column_text->Empty();
				}
			}
			break;
		}
		case 2:			// topic
		{
			if (m_type == CHATROOM_TYPE)
			{
				item_data->column_query_data.column_text->Set(m_name);
			}
			break;
		}

	}

	return OpStatus::OK;
}

void GroupsModelItem::SetIsSubscribed(INT32 subscribed)
{
	m_subscribed = subscribed;

	if (m_old_subscribed==-1)
		m_old_subscribed=m_subscribed;
}

OP_STATUS GroupsModelItem::SetName(const OpStringC& name)
{
	OP_STATUS ret;
	if (m_old_name.IsEmpty() && name.Compare(m_name)!=0)
	{
		if ((ret=m_old_name.Set(m_name)) != OpStatus::OK)
			return ret;
	}

	return m_name.Set(name);
}

OP_STATUS GroupsModelItem::SetPath(const OpStringC& path)
{
	OP_STATUS ret;
	if (m_old_path.IsEmpty() && path.Compare(m_path)!=0)
	{
		if ((ret=m_old_path.Set(m_path)) != OpStatus::OK)
			return ret;
	}

	return m_path.Set(path);
}


UINT32 GroupsModelItem::GetIndexId()
{
	Index* index = MessageEngine::GetInstance()->GetIndexer()->GetSubscribedFolderIndex(MessageEngine::GetInstance()->GetAccountById(m_account_id), m_path, 0, m_name, FALSE, FALSE);
	return index ? index->GetId() : 0;
}


/***********************************************************************************
**
**	GroupsModel
**
***********************************************************************************/

GroupsModel::GroupsModel() :
	// m_account_id ?
	m_next_id(0),
	m_folder_type(OpTypedObject::UNKNOWN_TYPE),
	m_read_only(FALSE),
	m_ref_count(0),
	m_folder_loading_completed(FALSE)
{
}


GroupsModel::~GroupsModel()
{
	MessageEngine::GetInstance()->RemoveAccountListener(this);
}

OP_STATUS GroupsModel::Init(UINT16 account_id, BOOL read_only)
{
	m_account_id = account_id;
	m_read_only = read_only;

	Account* account = MessageEngine::GetInstance()->GetAccountById(account_id);

	if (account)
	{
		RETURN_IF_ERROR(MessageEngine::GetInstance()->AddAccountListener(this));

		switch (account->GetIncomingProtocol())
		{
			case AccountTypes::NEWS:
				m_folder_type = OpTypedObject::NEWSGROUP_TYPE;
				break;
			case AccountTypes::IMAP:
				m_folder_type = OpTypedObject::IMAPFOLDER_TYPE;
				break;
			case AccountTypes::RSS:
				m_folder_type = OpTypedObject::NEWSFEED_TYPE;
				break;
			case AccountTypes::IRC:
				m_folder_type = OpTypedObject::CHATROOM_TYPE;
				break;
		}

		account->GetAllFolders();
	}

	return OpStatus::OK;
}


void GroupsModel::Commit()
{
	if (m_read_only)
		return;

	Account* account = MessageEngine::GetInstance()->GetAccountById(m_account_id);

	BOOL account_changed = FALSE;

	if (account)
	{
		account->StopFolderLoading();

		UINT32 index_to_show = 0;

		for (int pos = 0; pos < GetItemCount(); pos++)
		{
			GroupsModelItem* item = GetItemByIndex(pos);
			if (item)
			{
				OpString path, name;
				OpStatus::Ignore(item->GetPath(path));
				OpStatus::Ignore(item->GetName(name));

				if (item->PathIsChanged() && !item->IsManuallyAdded())
				{
					OpString old_path;
					OpStatus::Ignore(item->GetOldPath(old_path));
					OpStatus::Ignore(account->RenameFolder(old_path, path));
				}

				if (item->SubscriptionIsChanged())
				{
					if (item->IsManuallyAdded())
					{
						OpStatus::Ignore(account->CreateFolder(path, item->GetIsSubscribed()));
					}

					if (item->GetIsSubscribed())
					{
						OpStatus::Ignore(account->AddSubscribedFolder(path));
                        account->SetIsTemporary(FALSE);
						if (m_folder_type != OpTypedObject::IMAPFOLDER_TYPE)
						{
							Index* index = MessageEngine::GetInstance()->GetIndexer()->GetSubscribedFolderIndex(account, path, 0, name, TRUE, FALSE);
							if (index)
							{
								MessageEngine::GetInstance()->RefreshFolder(index->GetId());
								if (index_to_show)
									index_to_show = IndexTypes::FIRST_ACCOUNT + m_account_id;
								else
									index_to_show = index->GetId();
							}
						}
					}
					else
					{
						Index* index = MessageEngine::GetInstance()->GetIndexer()->GetSubscribedFolderIndex(account, path, 0, name, FALSE, FALSE);

						if (index)
							OpStatus::Ignore(account->RemoveSubscribedFolder(index->GetId()));
						else
							OpStatus::Ignore(account->RemoveSubscribedFolder(path));
					}
					account_changed = TRUE;
				}
				else if (item->GetIsSubscribed() && m_folder_type != OpTypedObject::IMAPFOLDER_TYPE)
				{
					MessageEngine::GetInstance()->GetIndexer()->GetSubscribedFolderIndex(account, path, 0, name, TRUE, FALSE);
				}
			}
		}
		if (account_changed)
		{
			account->CommitSubscribedFolders();
		}

		if (index_to_show)
		{
			g_application->GoToMailView(index_to_show, NULL, NULL, TRUE, FALSE, TRUE);
		}
	}
}


void GroupsModel::Refresh()
{
	// Clear up internal state.
	DeleteAll();
	m_groups_hash_table.RemoveAll();
	m_folder_loading_completed = FALSE;

	// Tell the account to fetch all folders.
	Account* account = MessageEngine::GetInstance()->GetAccountById(m_account_id);
	OP_ASSERT(account != 0);
	account->GetAllFolders();
}


INT32 GroupsModel::GetColumnCount()
{
	return 3;
}


OP_STATUS GroupsModel::SubscribeFolder(UINT32 id, BOOL subscribe)
{
	GroupsModelItem* item = GetItemByID(id);

	if (item)
	{
		item->SetIsSubscribed(subscribe);
		item->Change();

	}
	return OpStatus::OK;
}


OP_STATUS GroupsModel::GetColumnData(ColumnData* column_data)
{
	OpString group, status, topic;

	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_STATUS, status));

	if (m_folder_type == OpTypedObject::NEWSGROUP_TYPE)
	{
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_GROUP, group));
	}
	else if (m_folder_type == OpTypedObject::NEWSFEED_TYPE)
	{
		//CHECK THIS
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_RSS_TYPE, group));
	}
	else if (m_folder_type == OpTypedObject::IMAPFOLDER_TYPE)
	{
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_FOLDER_TYPE, group));

	}
	else if (m_folder_type == OpTypedObject::CHATROOM_TYPE)
	{
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_ROOM_TYPE, group));
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_USERS_TYPE, status));
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_TOPIC_TYPE, topic));
	}

	uni_char* strings[] = {group.CStr(), status.CStr(), topic.CStr()};

	column_data->text.Set(strings[column_data->column]);
	column_data->custom_sort = TRUE;

	return OpStatus::OK;
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
OP_STATUS GroupsModel::GetTypeString(OpString& type_string)
{
	return g_languageManager->GetString(Str::S_GROUP, type_string);
}
#endif

INT32 GroupsModel::CompareItems(INT32 column, OpTreeModelItem* opitem1, OpTreeModelItem* opitem2)
{
	GroupsModelItem* item1 = (GroupsModelItem*) opitem1;
	GroupsModelItem* item2 = (GroupsModelItem*) opitem2;

	INT32 result = 0;

	const uni_char* string1 = NULL;
	const uni_char* string2 = NULL;

	switch (column)
	{
		case 0:
		{
			if (m_folder_type == OpTypedObject::CHATROOM_TYPE)
			{
				string1 = item1->GetPathCStr();
				string2 = item2->GetPathCStr();
			}
			else
			{
				string1 = item1->GetNameCStr();
				string2 = item2->GetNameCStr();
			}

			break;
		}
		case 1:
			result = item1->GetIsSubscribed() - item2->GetIsSubscribed();
			break;

		case 2:
			if (m_folder_type == OpTypedObject::CHATROOM_TYPE)
			{
				string1 = item1->GetNameCStr();
				string2 = item2->GetNameCStr();
			}
			break;
	}

	if (result == 0 && string1 && string2)
	{
		result = uni_stricmp(string1, string2);
	}

	return result;
}

INT32 GroupsModel::FindParent(const OpStringC& path)
{
	// FIXME: currently, no threading:
	{
		return -1;
	}

/*	OpString s;
	OpStatus::Ignore(s.Set("."));
	uni_char c = s[0];

	int last_added = GetItemCount() - 1;
	if (last_added >= 0)
	{
		int pos = path.FindLastOf(c);
		if (pos != KNotFound)
		{
			GroupsModelItem *last = GetItemByIndex(last_added);
			if (last)
			{
				if (!path.Compare(last->GetPathCStr(), pos))
				{
					// same parent, find it.
					int last_parent = GetItemParent(last_added);
					if (last_parent == -1)
					{
						return last_added;
					}
					else
					{
						return last_parent;
					}
				}
				else
				{
					return -1;
				}
			}
		}
	}
	return -1;*/
}

void GroupsModel::OnFolderAdded(UINT16 account_id, const OpStringC& name, const OpStringC& path, BOOL subscribedLocally, INT32 subscribedOnServer, BOOL editable)
{
	if (account_id != m_account_id)
	{
		return;
	}

	AddFolder(name, path, subscribedLocally, subscribedOnServer, editable, FALSE);
}

INT32 GroupsModel::AddFolder(const OpStringC& name, const OpStringC& path, BOOL subscribedLocally, INT32 subscribedOnServer, BOOL editable, BOOL manually, BOOL force_create)
{
	// Note that for irc: (will clean up later)
	// name = topic
	// path = room name
	// subscribedOnServer = INT32 number of users

	BOOL exists = m_groups_hash_table.Contains(path.CStr());

	if (exists && !force_create)
		return 0;

	GroupsModelItem* item = OP_NEW(GroupsModelItem, (m_folder_type));
	if (!item)
		return 0;

	item->SetName(name);
	item->SetEditable(editable);

	if (!exists)
	{
		item->SetPath(path);
	}
	else //force_create
	{
		int i=2; //Start with appending '#2' to string
		while (exists)
		{
			OpString probe_path;
			probe_path.AppendFormat(UNI_L("%s#%i"), path.CStr(), i++);
			if (probe_path.IsEmpty()) //Out of memory?
				return 0;

			exists = m_groups_hash_table.Contains(probe_path.CStr());
			if (!exists)
			{
				if (name.Compare(path)==0)
					item->SetName(probe_path);

				item->SetPath(probe_path);
			}
		}
	}

	item->SetWasSubscribed(subscribedLocally);
	item->SetIsSubscribed(subscribedOnServer);
	item->m_id = m_next_id++;
	item->SetIsManuallyAdded(manually);
	item->m_account_id = m_account_id;

	m_groups_hash_table.Add(item->GetPathCStr(), item);
	AddLast(item, FindParent(path));

	return item->m_id;
}

OP_STATUS GroupsModel::UpdateFolder(UINT32 item_id, const OpStringC& path, const OpStringC& name)
{
	GroupsModelItem* item = GetItemByID(item_id);

	OP_STATUS ret = OpStatus::OK;
	if (item)
	{
		ret = item->SetPath(path);

		if (ret==OpStatus::OK)
			ret=item->SetName(name);

		item->Change();
	}
	return ret;
}

void GroupsModel::OnFolderRemoved(UINT16 account_id, const OpStringC& path)
{
	if (account_id != m_account_id)
		return;

	INT32 pos = GetItemByPath(path);

	if (pos == -1)
		return;

	GroupsModelItem* item = GetItemByIndex(pos);

	m_groups_hash_table.Remove(item->GetPathCStr(), &item);

	Delete(pos);
}


void GroupsModel::OnFolderRenamed(UINT16 account_id, const OpStringC& old_path, const OpStringC& new_path)
{
	if (account_id != m_account_id)
		return;

	INT32 pos = GetItemByPath(old_path);
	if (pos == -1)
		return;

	GroupsModelItem* item = GetItemByIndex(pos);
	item->SetPath(new_path);
	item->Change();
}


void GroupsModel::OnFolderLoadingCompleted(UINT16 account_id)
{
	if (account_id == m_account_id)
		m_folder_loading_completed = TRUE;
}


INT32 GroupsModel::GetItemByPath(const OpStringC& path)
{
	INT32 count = GetItemCount();

	for (int pos = 0; pos < count; pos++)
	{
		GroupsModelItem* item = GetItemByIndex(pos);

		if (item && item->ComparePathI(path) == 0)
		{
			return pos;
		}
	}

	return -1;
}

#endif //M2_SUPPORT
