/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/include/mailfiles.h"

#include "engine.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/chatinfo.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/engine/modules.h"
#include "adjunct/m2/src/engine/progressinfo.h"
#include "adjunct/m2/src/engine/store/storemessage.h"
#include "adjunct/m2/src/MessageDatabase/SEMessageDatabase.h"
#include "adjunct/m2/src/backend/rss/rssmodule.h"
#include "adjunct/m2/src/backend/nntp/nntpmodule.h"
#include "adjunct/m2/src/recovery/recovery.h"
#include "adjunct/m2_ui/models/indexmodel.h"
#include "adjunct/m2_ui/models/chattersmodel.h"
#include "adjunct/m2_ui/models/chatroomsmodel.h"
#include "adjunct/m2_ui/models/accountsmodel.h"
#include "adjunct/m2_ui/models/groupsmodel.h"
#include "modules/encodings/encoders/outputconverter.h"

#include "adjunct/m2/src/util/autodelete.h"
#include "adjunct/m2/src/util/qp.h"
#include "adjunct/m2/src/util/misc.h"
#include "adjunct/m2/src/util/str/strutil.h"

#include "adjunct/m2/src/import/ImportFactory.h"
#include "adjunct/m2/src/import/OperaImporter.h"

#include "adjunct/desktop_pi/DesktopMailClientUtils.h"
#include "adjunct/desktop_util/adt/hashiterator.h"
#include "adjunct/desktop_util/prefs/PrefsCollectionM2.h"

#include "adjunct/m2_ui/dialogs/DefaultMailClientDialog.h"
#include "adjunct/m2_ui/dialogs/NewAccountWizard.h"
#include "adjunct/m2_ui/dialogs/NewsfeedSubscribeDialog.h"
#include "adjunct/m2_ui/windows/MailDesktopWindow.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/managers/NotificationManager.h"
#include "adjunct/quick/hotlist/HotlistManager.h"

#ifdef USE_COMMON_RESOURCES
#include "adjunct/desktop_util/resources/ResourceDefines.h"
#endif // USE_COMMON_RESOURCES

#include "adjunct/m2/src/util/quote.h"
//#include "locale/oplanguagemanager.h"

#ifdef IRC_SUPPORT
#include "adjunct/m2/src/backend/irc/irc-urlparser.h"
#include "adjunct/m2/src/backend/irc/chat-networks.h"
#include "adjunct/m2/src/backend/irc/chat-filetransfer.h"
#endif

#include "adjunct/m2/src/backend/rss/opml.h"
#include "adjunct/m2/src/backend/rss/opml_importer.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/OpLocale.h"
#include "modules/prefs/prefsmanager/collections/pc_m2.h"
#include "modules/util/opstrlst.h"

#ifdef USE_COMMON_RESOURCES
#include "adjunct/desktop_util/resources/ResourceDefines.h"
#endif // USE_COMMON_RESOURCES

#ifdef M2_MERLIN_COMPATIBILITY
# include "adjunct/m2/src/engine/store/storeupdater.h"
#endif // M2_MERLIN_COMPATIBILITY

#ifdef M2_MAPI_SUPPORT
#include "adjunct/m2/src/mapi/MapiMessageListener.h"
#endif //M2_MAPI_SUPPORT

#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"

#ifdef ENABLE_USAGE_REPORT
# include "adjunct/quick/usagereport/UsageReport.h"
#endif

// Delay (in ms) for calling listeners when indexes changed
#define INDEX_CHANGE_DELAY 500

MessageEngine* MessageEngine::s_instance = 0;

MessageEngine::MessageEngine()
: m_glue_factory(NULL),
  m_store(NULL),
  m_indexer(NULL),
  m_message_database(NULL),
  m_clipboard_cut(FALSE),
  m_chat_networks(NULL),
  m_chat_file_transfer_manager(NULL),
  m_multiple_message_changes(0),
  // m_sending_queued_messages(FALSE), // unused ?
  m_deleting_trash(FALSE),
  m_posted_index_changed(FALSE),
#ifdef IRC_SUPPORT
  m_chatrooms_model(NULL),
#endif
  m_account_manager(NULL),
  m_accounts_model(NULL),
  m_is_busy_asking_for_security_password(FALSE),
  m_will_do_recover_check_on_exit(TRUE),
  m_has_asked_user_about_recovery(FALSE),
  m_autodelete(NULL),
  m_loop(NULL),
  m_master_progress(NULL),
  m_mail_import_in_progress(0)
#ifdef M2_MAPI_SUPPORT
  , m_check_default_mailer(TRUE)
  , m_mapi_listener(NULL)
#endif //M2_MAPI_SUPPORT
{
}


MessageEngine::~MessageEngine()
{
	OP_PROFILE_METHOD("Destructed MessageEngine");
#ifdef M2_MAPI_SUPPORT
	OP_DELETE(m_mapi_listener);
#endif //M2_MAPI_SUPPORT
	// if we are supposed to run mail db maintenance, we want to run this code to remove ghosts before deleting store and indexer
	if (m_will_do_recover_check_on_exit && m_has_asked_user_about_recovery)
	{
		OP_PROFILE_METHOD("Ghostbuster");
		MailRecovery recovery;
		recovery.GhostBuster();
	}

	{
		OP_PROFILE_METHOD("Destructed MessageEngine - phase 1");

		if (g_application)
			g_application->RemoveSettingsListener(this);

		if (m_message_database && m_message_database->IsInitialized())
			OpStatus::Ignore(m_message_database->Commit());

		// m_indexer may not be created if something failed on Init()
		if( m_indexer )
			m_indexer->PrepareToDie();

		// save messages that are waiting in queue first
		// m_autodelete may not be created in if something failed on Init()
		if( m_autodelete )
			m_autodelete->FlushQueue();

		m_index_models.DeleteAll();
		m_groups_models.DeleteAll();
	}

	{
		OP_PROFILE_METHOD("Destructed Indexer");
		if (m_indexer)
			m_indexer->RemoveIndexerListener(this);

		OP_DELETE(m_indexer);
		m_indexer = NULL;
	}
	{
		OP_PROFILE_METHOD("Destructed AccountManager");
		OP_DELETE(m_account_manager);
		OP_DELETE(m_accounts_model);
	}
	{
		OP_PROFILE_METHOD("Destructed MessageEngine - phase 2");
#ifdef IRC_SUPPORT
		OP_DELETE(m_chatrooms_model);
#endif
		OP_DELETE(m_message_database);


		if (GetGlueFactory())
			GetGlueFactory()->DeleteMessageLoop(m_loop);

#ifdef IRC_SUPPORT
		if (m_chat_networks)
			OP_DELETE(m_chat_networks);

		if (m_chat_file_transfer_manager)
			OP_DELETE(m_chat_file_transfer_manager);
#endif // IRC_SUPPORT
		OP_DELETE(m_autodelete);
		OP_DELETE(m_master_progress);
	}
	{
		OP_PROFILE_METHOD("Destructed Store");
		OP_DELETE(m_store);
		m_store = NULL;
	}

	SetFactories(NULL); // after killing account manager !
		
	// maybe we want to do maintenance on the databases
    if (m_will_do_recover_check_on_exit && m_has_asked_user_about_recovery)
    {
		OP_PROFILE_METHOD("Mail database maintenance");
        MailRecovery recovery;
		// check and fix all database files
        OpStatus::Ignore(recovery.CheckConsistencyAndFixAllMailDBs());
        // lexicon is closed when indexer is deleted, so we can check the lexicon as well
        OpStatus::Ignore(recovery.CheckAndFixLexicon());

		m_will_do_recover_check_on_exit = FALSE;
    }
}

BOOL MessageEngine::NeedToAskAboutDoingMailRecovery ()
{
#ifdef DU_CAP_PREFS
    if (CommandLineManager::GetInstance()->GetArgument(CommandLineManager::WatirTest))
        return FALSE;

	// only check if it's about time to do a check if we haven't asked already
	if (!m_has_asked_user_about_recovery)
	{
		time_t now = g_timecache->CurrentTime();
		// we only ask if we should do a mail database consistency if we have never run the consistency check
		if (g_pcm2->GetIntegerPref(PrefsCollectionM2::LastDatabaseCheck) == 0)
		{
			// we are going to ask to do a mail recovery, store the time
			TRAPD(err, g_pcm2->WriteIntegerL(PrefsCollectionM2::LastDatabaseCheck, now));

			// don't return TRUE if we didn't manage to update the time in the pref
			m_will_do_recover_check_on_exit = err == OpStatus::OK;
			return m_will_do_recover_check_on_exit;
		}
		else
		{
			// we don't need to a consistency check, so don't ask
			m_will_do_recover_check_on_exit = FALSE;
			return FALSE;
		}
	}
#endif // DU_CAP_PREFS

	// we have asked, don't ask again
	return FALSE;
}


OP_STATUS MessageEngine::Init(OpString8& status)
{
    OP_STATUS ret = OpStatus::OK;

    //Start AutoDelete early, can be used by other objects
    m_autodelete = OP_NEW(AutoDelete, ());
    if (!m_autodelete)
        return OpStatus::ERR_NO_MEMORY;

	// Setup master progress
	m_master_progress = OP_NEW(ProgressInfo, ());
	if (!m_master_progress)
		return OpStatus::ERR_NO_MEMORY;

	// Init input converters
	RETURN_IF_ERROR(m_input_converter_manager.Init());
	
	m_message_database = OP_NEW(SEMessageDatabase, (*g_main_message_handler));
	if (!m_message_database)
		return OpStatus::ERR_NO_MEMORY;

#ifdef M2_MERLIN_COMPATIBILITY
	// need to check if we need to upgrade before creating store
	StoreUpdater::NeedsUpdate();
#endif // M2_MERLIN_COMPATIBILITY

	// Create Store and Indexer before initializing the MessageDatabase
	m_store = OP_NEW(Store, (m_message_database));
	if (!m_store)
		return OpStatus::ERR_NO_MEMORY;

	m_indexer = OP_NEW(Indexer, (m_message_database));
	if (!m_indexer)
		return OpStatus::ERR_NO_MEMORY;

	m_store->SetProgressKeeper(m_master_progress);

	OpString error_message;
	ret = m_store->Init(error_message);

	if (OpStatus::IsError(ret))
	{
		if (error_message.HasContent())
			status.SetUTF8FromUTF16(error_message.CStr());
		else
			status.Set("\nStore Init() failed\n");
		return ret;
	}

	if(g_application)
		g_application->AddSettingsListener(this);
	
	ret = InitEnginePart1(status);
#ifdef M2_MERLIN_COMPATIBILITY
	if (!StoreUpdater::NeedsUpdate())
#endif //M2_MERLIN_COMPATIBILITY
	{
		if (OpStatus::IsSuccess(ret))
		{
			ret = InitEnginePart2(status);
		}
	}
	if (OpStatus::IsError(ret) && m_account_manager)
		m_account_manager->M2InitFailed();

	// Setup message loop
	m_loop = GetGlueFactory()->CreateMessageLoop();
	if (!m_loop)
		return OpStatus::ERR_NO_MEMORY;
	RETURN_IF_ERROR(m_loop->SetTarget(this));

	// Setup notification listener
	RETURN_IF_ERROR(m_master_progress->AddNotificationListener(g_notification_manager));
#ifdef M2_MAPI_SUPPORT
	m_mapi_listener = OP_NEW(MapiMessageListener,());
	RETURN_OOM_IF_NULL(m_mapi_listener);
#endif //M2_MAPI_SUPPORT
	return ret;
}


OP_STATUS MessageEngine::InitEnginePart1(OpString8& status)
{
	OP_PROFILE_METHOD("Init mail root dir completed");

	OP_STATUS ret;

    //Initialize accounts.ini
    OpString accounts_filename;
	RETURN_IF_ERROR(MailFiles::GetAccountsINIFilename(accounts_filename));

    if (accounts_filename.IsEmpty())
        return OpStatus::ERR_NO_MEMORY;

	// create chat rooms model
#ifdef IRC_SUPPORT
    m_chatrooms_model = OP_NEW(ChatRoomsModel, ());
    if (!m_chatrooms_model)
        return OpStatus::ERR_NO_MEMORY;

	{
		OP_PROFILE_METHOD("Init chat rooms model");

	    if ((ret=m_chatrooms_model->Init()) != OpStatus::OK)
		    return ret;
	}
#endif

	// create accounts model

    m_accounts_model = OP_NEW(AccountsModel, ());
    if (!m_accounts_model)
        return OpStatus::ERR_NO_MEMORY;

	{
		OP_PROFILE_METHOD("Init accounts model");

	    if ((ret=m_accounts_model->Init()) != OpStatus::OK)
		    return ret;
	}
    //Initialize the one and only account_manager
	{
		OP_PROFILE_METHOD("Init accounts manager");

		m_account_manager = OP_NEW(AccountManager, (*m_message_database));
		if (!m_account_manager)
			return OpStatus::ERR_NO_MEMORY;

		if ((ret=m_account_manager->SetPrefsFile(accounts_filename)) != OpStatus::OK)
			return ret;
		if ((ret=m_account_manager->Init(status)) != OpStatus::OK)
		{
			status.Append("AccountManager Init failed\n");
			return ret;
		}
	}
	
	return OpStatus::OK;
}

OP_STATUS MessageEngine::InitEnginePart2(OpString8& status)
{
	OpFile custom_file;
	if (m_account_manager->GetAccountCount() != 0 || GetCustomDefaultAccountsFile(custom_file))
	{
		RETURN_IF_ERROR(InitMessageDatabase(status));

		OP_PROFILE_METHOD("AccountManager ToOnlineMode");

		RETURN_IF_ERROR(m_account_manager->RemoveTemporaryAccounts());

	}
	return OpStatus::OK;
}

OP_STATUS MessageEngine::InitMessageDatabaseIfNeeded()
{
	if (m_message_database->IsInitialized())
		return OpStatus::OK;

	OpAutoPtr<OpString8> status(OP_NEW(OpString8, ()));
	if (!status.get())
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError(InitMessageDatabase(*status)))
	{
		g_main_message_handler->PostMessage(MSG_QUICK_MAIL_PROBLEM_INITIALIZING, (MH_PARAM_1)g_application, (MH_PARAM_2)status.release(), 10);
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}


OP_STATUS MessageEngine::InitMessageDatabase(OpString8& status)
{
	OP_PROFILE_METHOD("Indexer Init");
	OpString indexer_filename;

	RETURN_IF_ERROR(MailFiles::GetIndexIniFileName(indexer_filename));

	RETURN_IF_ERROR(m_indexer->Init(indexer_filename, status));

	RETURN_IF_ERROR(m_indexer->AddIndexerListener(this));

	return m_message_database->Init(m_store, m_indexer, this, m_indexer->GetLexicon());
}

void MessageEngine::FlushAutodeleteQueue()
{
    if (m_autodelete)
        m_autodelete->FlushQueue();
}

// ----------------------------------------------------

#ifdef USE_COMMON_RESOURCES
#define __ACCOUNTS_DEFAULT	DESKTOP_RES_STD_ACCOUNTS
#else
#define __ACCOUNTS_DEFAULT	UNI_L("accounts_default.ini")
#endif //  USE_COMMON_RESOURCES

BOOL MessageEngine::GetCustomDefaultAccountsFile(OpFile& file)
{
	OpString filename;
	BOOL exists;
	if ((g_application->DetermineFirstRunType() != Application::RUNTYPE_FIRSTCLEAN))
		return FALSE;
	// Get prefsfile that countains default accounts
	OpString icpfolder;
	RETURN_VALUE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_INI_CUSTOM_PACKAGE_FOLDER, icpfolder), FALSE);
	RETURN_VALUE_IF_ERROR(filename.AppendFormat(UNI_L("%s%c%s"),
					icpfolder.CStr(),
					PATHSEPCHAR,__ACCOUNTS_DEFAULT), FALSE);


	// Check if file exists
	RETURN_VALUE_IF_ERROR(file.Construct(filename.CStr()), FALSE);
	RETURN_VALUE_IF_ERROR(file.Exists(exists), FALSE);
	return exists;
}

OP_STATUS MessageEngine::InitDefaultAccounts()
{
	OpFile file;
	if (!GetCustomDefaultAccountsFile(file))
		return OpStatus::OK;

	// Open prefsfile
	OP_STATUS status = OpStatus::OK;
	TRAP(status,
	{
		PrefsFile defaults(PREFS_STD);

		defaults.ConstructL();
		defaults.SetFileL(&file);
		defaults.LoadAllL();

		/* Read default RSS accounts */
		PrefsSection* rss_section = defaults.ReadSectionL(UNI_L("RSS"));
		if (!rss_section)
			return OpStatus::ERR_NULL_POINTER;

		OpAutoPtr<OpString_list> rss_list(rss_section->GetKeyListL());
		if (!rss_list.get())
			return OpStatus::ERR_NULL_POINTER;

		/* Add RSS accounts */
		for(UINT32 i = 0; i < rss_list->Count(); i++)
		{
			URL dummy;
			RETURN_IF_ERROR(LoadRSSFeed(rss_list->Item(i), dummy, FALSE, FALSE));
		}
	} );

	return status;
}

// ----------------------------------------------------

BOOL MessageEngine::InitM2OnStartup()
{
	// Check if M2 has been disabled somewhere
	BOOL init_mail = KioskManager::GetMailEnabled();

	init_mail &= !CommandLineManager::GetInstance()->GetArgument(CommandLineManager::NoMail);
	init_mail &= g_pcm2->GetIntegerPref(PrefsCollectionM2::ShowEmailClient);

#ifdef EMBROWSER_SUPPORT
	//never start M2 if embedded in another application
	extern long gVendorDataID;
	init_mail &= (gVendorDataID == 'OPRA');
#endif

	return init_mail;
}

// ----------------------------------------------------

OP_STATUS MessageEngine::GetGroupsModel(OpTreeModel** accounts_model, UINT16 account_id, BOOL read_only)
{
	GroupsModel* model = NULL;

	if (read_only)
	{
		INT32 count = m_groups_models.GetCount();

		for (INT32 i = 0; i < count; i++)
		{
			GroupsModel* cached_model = m_groups_models.Get(i);

			if (cached_model->GetAccountId() == account_id)
			{
				model = cached_model;
				break;
			}

		}
	}

	if (!model)
	{
		model = OP_NEW(GroupsModel, ());
		RETURN_IF_ERROR(model->Init(account_id, read_only));

		if (read_only)
		{
			m_groups_models.Add(model);
		}
	}

	model->IncRefCount();
	*accounts_model = model;

	return OpStatus::OK;
}

OP_STATUS MessageEngine::ReleaseGroupsModel(OpTreeModel* tree_model, BOOL commit)
{
	GroupsModel* model = (GroupsModel*)tree_model;

	if (commit)
	{
		model->Commit();
	}

	// if found in cache list, it is read only

	INT32 count = m_groups_models.GetCount();

    for (INT32 i = 0; i < count; i++)
	{
		GroupsModel* cached_model = m_groups_models.Get(i);

		if (model == cached_model)
		{
			model->DecRefCount();
			return OpStatus::OK;
		}
	}

	// not found.. it was writable.. delete it

	OP_DELETE(model);

	return OpStatus::OK;
}

// ----------------------------------------------------

OP_STATUS MessageEngine::SubscribeFolder(OpTreeModel* tree_model, UINT32 item_id, BOOL subscribe)
{
	GroupsModel* model = (GroupsModel*)tree_model;

	return model->SubscribeFolder(item_id, subscribe);
}

OP_STATUS MessageEngine::CreateFolder(OpTreeModel* tree_model, UINT32* item_id)
{
	OpString default_folder_name;
	OpString default_folder_path;
	OpString8 protocol;

	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_DEFAULT_IMAP_NEW_FOLDER_NAME, default_folder_name));
	default_folder_path.Set(default_folder_name);

	GroupsModel* model = (GroupsModel*)tree_model;

	Account* account = GetAccountById(model->GetAccountId());
	if (account->GetIncomingProtocol() == AccountTypes::RSS)
	{
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_RSS_NEWSFOLDER, default_folder_name));
		default_folder_path.Set(UNI_L("http://example"));

		Index* index = m_indexer->GetSubscribedFolderIndex(account, default_folder_path, 0, default_folder_name, TRUE, FALSE);

		if (index != 0)
			index->SetUpdateFrequency(-1); // So that a newly created dummy rss feed won't attempt to update itself just quite yet.

		*item_id = model->GetItemByPath(default_folder_path);
		if (1 + *item_id != 0)
		{
			return OpStatus::OK; // exists
		}
	}

	*item_id = model->AddFolder(default_folder_name, default_folder_path, FALSE, TRUE, TRUE, TRUE, TRUE);

	return OpStatus::OK;
}

OP_STATUS MessageEngine::UpdateFolder(OpTreeModel* tree_model, UINT32 item_id, OpStringC& path, OpStringC& name)
{
	GroupsModel* model = (GroupsModel*)tree_model;
	model->UpdateFolder(item_id, path, name);

	return OpStatus::OK;
}

OP_STATUS MessageEngine::DeleteFolder(OpTreeModel* tree_model, UINT32 item_id)
{
	GroupsModel* model = static_cast<GroupsModel*>(tree_model);
	GroupsModelItem* item = model->GetItemByID(item_id);

	OP_STATUS ret;
	OpString path;
	if ((ret=item->GetPath(path)) != OpStatus::OK)
		return ret;

	UINT16 account_id = model->GetAccountId();
	Account* account = GetAccountById(account_id);
//	UINT32 index_id = item->GetIndexId();
//	Index* index = (m_indexer ? m_indexer->GetIndexById(index_id) : NULL);

	if (account && path.HasContent() && path.CompareI("INDEX"))
	{
		// Remove folder from account
		account->DeleteFolder(path);
	}

	model->OnFolderRemoved(account_id, path);

	return OpStatus::OK;
}


OP_STATUS MessageEngine::UpdateNewsGroupIndexList()
{
	return m_indexer->UpdateNewsGroupIndexList();
}


// ----------------------------------------------------

OP_STATUS MessageEngine::GetIndexModel(OpTreeModel** index_model, Index* index, INT32& start_pos)
{
	if (!m_store->HasFinishedLoading())
		return OpStatus::ERR_YIELD;

	start_pos = -1;

    if (index)
    {
        OP_STATUS ret;

		// search list first
        for (UINT m = 0; m < m_index_models.GetCount(); m++)
		{
			IndexModel* model = m_index_models.Get(m);
			if (model && model->GetIndexId() == index->GetId())
			{
				start_pos = model->GetStartPos();

				model->IncRefCount();
				*index_model = model;

				return OpStatus::OK;
			}
		}

		if ((ret=index->PreFetch()) != OpStatus::OK)
            return ret;

		IndexModel* model = OP_NEW(IndexModel, ());

        if (!model)
            return OpStatus::ERR_NO_MEMORY;
        if ((ret=model->Init(index)) != OpStatus::OK)
            return ret;

		start_pos = model->GetStartPos();

		model->IncRefCount(); // set refcount to 1

		*index_model = model;

		m_index_models.Add(model);

        return OpStatus::OK;
    }

    return OpStatus::ERR;
}

// ----------------------------------------------------

#ifdef IRC_SUPPORT
ChatNetworkManager* MessageEngine::GetChatNetworks()
{
	if (m_chat_networks == 0)
	{
		m_chat_networks = OP_NEW(ChatNetworkManager, ());
		if (m_chat_networks != 0 &&
			OpStatus::IsError(m_chat_networks->Init()))
		{
			OP_DELETE(m_chat_networks);
			m_chat_networks = 0;
		}
	}

	return m_chat_networks;
}

ChatFileTransferManager* MessageEngine::GetChatFileTransferManager()
{
	if (m_chat_file_transfer_manager == 0)
		m_chat_file_transfer_manager = OP_NEW(ChatFileTransferManager, ());

	return m_chat_file_transfer_manager;
}
#endif

// ----------------------------------------------------

OP_STATUS MessageEngine::GetIndexModel(OpTreeModel** index_model, index_gid_t index_id, INT32& start_pos)
{
    return GetIndexModel(index_model, m_indexer->GetIndexById(index_id), start_pos);
}

OP_STATUS MessageEngine::WriteViewPrefs(IndexTypes::ModelSort sort, BOOL ascending, IndexTypes::ModelType type, IndexTypes::GroupingMethods grouping_method)
{
  TRAPD(status,
    g_pcm2->WriteIntegerL(PrefsCollectionM2::DefaultMailSorting, sort);
    g_pcm2->WriteIntegerL(PrefsCollectionM2::DefaultMailSortingAscending, ascending);
    g_pcm2->WriteIntegerL(PrefsCollectionM2::DefaultMailFlatThreadedView, type);
    g_pcm2->WriteIntegerL(PrefsCollectionM2::MailGroupingMethod, grouping_method);
    g_prefsManager->CommitL());
  return status;
}

// ----------------------------------------------------

OP_STATUS MessageEngine::SetIndexModelType(index_gid_t index_id, IndexTypes::ModelType type, IndexTypes::ModelAge age, INT32 flags, IndexTypes::ModelSort sort, BOOL ascending, IndexTypes::GroupingMethods grouping_method, message_gid_t selected_message)
{
	Index* index = m_indexer->GetIndexById(index_id);

	if (index)
	{
		BOOL reinit = FALSE, reinit_all = FALSE;
		if (index->GetModelFlags() != flags)
		{
			index->SetModelFlags(flags);
			reinit = TRUE;
		}
		if (index->GetModelAge() != age)
		{
			index->SetModelAge(age);
			reinit = TRUE;
		}

		if (index->GetOverrideDefaultSorting())
		{
			index->SetModelSort(sort);
			index->SetModelSortAscending(ascending != FALSE);
			index->SetModelGroup(grouping_method);
			index->SetModelType(type);
		}
		else
		{
		
			if (g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailSorting) != sort)
			{
				reinit_all = TRUE;
			}

			if (g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailSortingAscending) != ascending)
			{
				reinit_all = TRUE;
			}

			// these indexes should always have a different type from the global
			if (index->IsContact() ||
				index->GetId() == IndexTypes::OUTBOX || 
				index->GetId() == IndexTypes::SENT || 
				index->GetId() == IndexTypes::DRAFTS || 
				index->GetId() == IndexTypes::SPAM || 
				index->GetId() == IndexTypes::TRASH )
			{

				if (index->GetModelType() != type)
				{
					index->SetModelType(type);
					reinit = TRUE;
				}
			}
			else
			{
				if (type != static_cast<IndexTypes::ModelType>(g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailFlatThreadedView)))
				{
					reinit_all = TRUE;
				}
			}

			if (grouping_method != static_cast<IndexTypes::GroupingMethods>(g_pcm2->GetIntegerPref(PrefsCollectionM2::MailGroupingMethod)))
			{
				reinit_all = TRUE;
			}

			RETURN_IF_ERROR(WriteViewPrefs(sort, ascending, type, grouping_method));
		}

		index->SetModelSelectedMessage(selected_message);

		if (reinit || reinit_all)
		{
			RETURN_IF_ERROR(m_indexer->UpdateIndex(index_id));

			UINT32 count = m_index_models.GetCount();

			for (UINT m = 0; m < count; m++)
			{
				IndexModel* model = m_index_models.Get(m);
				if (model && (reinit_all || (model->GetIndexId() == index_id)))
				{
					model->ReInit();
				}
			}
		}

		if (index_id == IndexTypes::UNREAD_UI)
		{
			RETURN_IF_ERROR(SetIndexModelType(IndexTypes::UNREAD, type, age, flags, sort, ascending, grouping_method, selected_message));
			OnIndexChanged(index_id);
		}
		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

// ----------------------------------------------------

OP_STATUS MessageEngine::GetIndexModelType(index_gid_t index_id, IndexTypes::ModelType& type, IndexTypes::ModelAge& age, INT32& flags, IndexTypes::ModelSort& sort, BOOL& ascending, IndexTypes::GroupingMethods& grouping_method, message_gid_t& selected_message)
{
	if (index_id == IndexTypes::UNREAD_UI)
		index_id = IndexTypes::UNREAD;

	Index* index = m_indexer->GetIndexById(index_id);

	if (index)
	{
		age = index->GetModelAge();
		flags = index->GetModelFlags();
		selected_message = index->GetModelSelectedMessage();

		if (index->GetOverrideDefaultSorting())
		{
			sort = index->GetModelSort();
			ascending = index->GetModelSortAscending();
			type = index->GetModelType();
			grouping_method = index->GetModelGrouping();
		}
		else
		{
			sort = static_cast<IndexTypes::ModelSort>(g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailSorting));
			ascending = g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailSortingAscending);

			if (index->IsContact() ||
				index->GetId() == IndexTypes::OUTBOX || 
				index->GetId() == IndexTypes::SENT || 
				index->GetId() == IndexTypes::DRAFTS || 
				index->GetId() == IndexTypes::SPAM || 
				index->GetId() == IndexTypes::TRASH )
			{
				type = index->GetModelType();
			}
			else
			{
				type = static_cast<IndexTypes::ModelType>(g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailFlatThreadedView));
			}
			grouping_method = static_cast<IndexTypes::GroupingMethods>(g_pcm2->GetIntegerPref(PrefsCollectionM2::MailGroupingMethod));
		}
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

// ----------------------------------------------------

index_gid_t MessageEngine::GetIndexIDByAddress(OpString& address)
{
	OpString temp;
	temp.Set(address);

	Index* index = m_indexer->GetCombinedContactIndex(temp);

	return index ? index->GetId() : 0;
}

// ----------------------------------------------------

OP_STATUS MessageEngine::ReleaseIndexModel(OpTreeModel* index_model)
{
    if (index_model)
    {
		// search list first

		UINT32 count = m_index_models.GetCount();

        for (UINT m = 0; m < count; m++)
		{
			IndexModel* model = m_index_models.Get(m);
			if (model && model == index_model)
			{
				if (model->DecRefCount() <= 0 && count > 50)
				{
					m_index_models.Delete(m);
				}
				return OpStatus::OK;
			}
		}
        return OpStatus::OK;
    }
    return OpStatus::ERR;
}

// ----------------------------------------------------

Account* MessageEngine::GetAccountById(UINT16 account_id)
{
	Account* account = NULL;

	m_account_manager->GetAccountById(account_id, account);

	return account;
}

// ----------------------------------------------------

#ifdef IRC_SUPPORT
ChatRoom* MessageEngine::GetChatRoom(UINT32 room_id)
{
	return m_chatrooms_model->GetChatRoom(room_id);
}

ChatRoom* MessageEngine::GetChatRoom(UINT16 account_id, OpString& room)
{
	return m_chatrooms_model->GetRoomItem(account_id, room);
}

ChatRoom* MessageEngine::GetChatter(UINT32 chatter_id)
{
	return m_chatrooms_model->GetChatter(chatter_id);
}

OP_STATUS MessageEngine::DeleteChatRoom(UINT32 room_id)
{
	m_chatrooms_model->DeleteChatRoom(room_id);
    return OpStatus::OK;
}
#endif

UINT32 MessageEngine::GetIndexIdFromFolder(OpTreeModelItem* item)
{
	GroupsModelItem* group_item = (GroupsModelItem*)item;
	return group_item->GetIndexId();
}


// ----------------------------------------------------
#ifdef IRC_SUPPORT
OP_STATUS MessageEngine::GetChattersModel(OpTreeModel** chatters_model,
	UINT16 account_id, const OpStringC& room)
{
	*chatters_model = m_chatrooms_model->GetChattersModel(account_id, room);

	if (*chatters_model)
	{
	    return OpStatus::OK;
	}

    return OpStatus::ERR;
}
#endif // IRC_SUPPORT
// ----------------------------------------------------

OP_STATUS MessageEngine::Connect(UINT16 account_id)
{
	Account* account = GetAccountById(account_id);

	if (!account)
	    return OpStatus::ERR;

	account->Connect();
    return OpStatus::OK;
}

// ----------------------------------------------------

OP_STATUS MessageEngine::JoinChatRoom(UINT16 account_id, const OpStringC& room, const OpStringC& password)
{
	Account* account = GetAccountById(account_id);

	if (!account)
	    return OpStatus::ERR;

	account->JoinChatRoom(room, password);
    return OpStatus::OK;
}

// ----------------------------------------------------

OP_STATUS MessageEngine::LeaveChatRoom(UINT16 account_id, const ChatInfo& room)
{
	Account* account = GetAccountById(account_id);

	if (!account)
	    return OpStatus::ERR;

	account->LeaveChatRoom(room);
    return OpStatus::OK;
}

// ----------------------------------------------------

OP_STATUS MessageEngine::SendChatMessage(UINT16 account_id,
	EngineTypes::ChatMessageType type, const OpString& message, const ChatInfo& room,
	const OpStringC& chatter)
{
	Account* account = GetAccountById(account_id);

	if (!account)
	    return OpStatus::ERR;

	account->SendChatMessage(type, message, room, chatter);
    return OpStatus::OK;
}

// ----------------------------------------------------

OP_STATUS MessageEngine::SetChatProperty(UINT16 account_id,
	const ChatInfo& room, const OpStringC& chatter, EngineTypes::ChatProperty property,
	const OpStringC& property_string, INT32 property_value)
{
	Account* account = GetAccountById(account_id);

	if (!account)
	    return OpStatus::ERR;

	return account->SetChatProperty(room, chatter, property, property_string, property_value);
}

// ----------------------------------------------------

OP_STATUS MessageEngine::SendFile(UINT16 account_id, const OpStringC& chatter, const OpStringC& filename)
{
	Account* account = GetAccountById(account_id);
	if (!account)
	    return OpStatus::ERR;

	return account->SendFile(chatter, filename);
}

/***********************************************************************************
**
**	Implementing the distribution of Engine events to listeners
**
***********************************************************************************/

// ----------------------------------------------------

void MessageEngine::OnImporterProgressChanged(const Importer* importer, const OpStringC& infoMsg, OpFileLength current, OpFileLength total, BOOL simple)
{
	for (UINT32 i = 0; i < m_engine_listeners.GetCount(); i++)
	{
		m_engine_listeners.Get(i)->OnImporterProgressChanged(importer, infoMsg, current, total, simple);
	}
}

// ----------------------------------------------------

void MessageEngine::OnImporterFinished(const Importer* importer, const OpStringC& infoMsg)
{
	for (UINT32 i = 0; i < m_engine_listeners.GetCount(); i++)
	{
		m_engine_listeners.Get(i)->OnImporterFinished(importer, infoMsg);
	}
}

// ----------------------------------------------------

void MessageEngine::OnIndexChanged(UINT32 index_id)
{
	if (m_multiple_message_changes > 0 || m_index_changed.Contains(index_id) || !m_loop)
		return;

	OpStatus::Ignore(m_index_changed.Insert(index_id));

	if (!m_posted_index_changed)
	{
		m_loop->Post(MSG_M2_INDEX_CHANGED, INDEX_CHANGE_DELAY);
		m_posted_index_changed = TRUE;
	}
}

OP_STATUS MessageEngine::ReinitAllIndexModels()
{
	UINT32 count = m_index_models.GetCount();

	for (UINT m = 0; m < count; m++)
	{
		RETURN_IF_ERROR(m_index_models.Get(m)->ReInit());
	}
	return OpStatus::OK;
}

void MessageEngine::OnActiveAccountChanged()
{
	// heavy, but needed

	RETURN_VOID_IF_ERROR(ReinitAllIndexModels());

	for (UINT32 i = 0; i < m_engine_listeners.GetCount(); i++)
	{
		m_engine_listeners.Get(i)->OnActiveAccountChanged();
	}

	Index* index;
	for (INT32 it = -1; (index = m_indexer->GetRange(0, IndexTypes::LAST, it)) != NULL; )
	{
		index->ResetUnreadCount(); // OK, doesn't need optimization
	}

	OnIndexChanged(static_cast<UINT32>(-1));
}


void MessageEngine::OnReindexingProgressChanged(INT32 progress, INT32 total)
{
	for (UINT32 i = 0; i < m_engine_listeners.GetCount(); i++)
	{
		m_engine_listeners.Get(i)->OnReindexingProgressChanged(progress, total);
	}
}


void MessageEngine::OnSettingsChanged(DesktopSettings* settings)
{
	if (settings->IsChanged(SETTINGS_LANGUAGE) && m_message_database->IsInitialized())
	{
		m_indexer->UpdateTranslatedStrings();
	}
}

void MessageEngine::OnAccountAdded(UINT16 account_id)
{
	for (UINT32 i = 0; i < m_account_listeners.GetCount(); i++)
	{
		m_account_listeners.Get(i)->OnAccountAdded(account_id);
	}
}

void MessageEngine::OnAccountRemoved(UINT16 account_id, AccountTypes::AccountType account_type)
{
	for (UINT32 i = 0; i < m_account_listeners.GetCount(); i++)
	{
		m_account_listeners.Get(i)->OnAccountRemoved(account_id, account_type);
	}
}

// ----------------------------------------------------

void MessageEngine::OnFolderAdded(UINT16 account_id, const OpStringC& name, const OpStringC& path, BOOL subscribedLocally, INT32 subscribedOnServer, BOOL editable)
{
	for (UINT32 i = 0; i < m_account_listeners.GetCount(); i++)
	{
		m_account_listeners.Get(i)->OnFolderAdded(account_id, name, path, subscribedLocally, subscribedOnServer, editable);
	}
}

void MessageEngine::OnFolderRemoved(UINT16 account_id, const OpStringC& path)
{
	for (UINT32 i = 0; i < m_account_listeners.GetCount(); i++)
	{
		m_account_listeners.Get(i)->OnFolderRemoved(account_id, path);
	}
}

void MessageEngine::OnFolderRenamed(UINT16 account_id, const OpStringC& old_path, const OpStringC& new_path)
{
	for (UINT32 i = 0; i < m_account_listeners.GetCount(); i++)
	{
		m_account_listeners.Get(i)->OnFolderRenamed(account_id, old_path, new_path);
	}
}

void MessageEngine::OnFolderLoadingCompleted(UINT16 account_id)
{
	for (UINT32 i = 0; i < m_account_listeners.GetCount(); ++i)
	{
		m_account_listeners.Get(i)->OnFolderLoadingCompleted(account_id);
	}
}

/***********************************************************************************
**
**	Implementing the distribution of Message events to listeners
**
***********************************************************************************/

void MessageEngine::OnMessageBodyChanged(StoreMessage& message)
{
	for (UINT32 i = 0; i < m_message_listeners.GetCount(); i++)
	{
		m_message_listeners.Get(i)->OnMessageBodyChanged(message.GetId());
	}
}

// ----------------------------------------------------

void MessageEngine::OnMessageChanged(message_gid_t message_id)
{
	if (m_multiple_message_changes > 0)
	{
		return;
	}

	for (UINT32 i = 0; i < m_message_listeners.GetCount(); i++)
	{
		m_message_listeners.Get(i)->OnMessageChanged(message_id);
	}
}

// ----------------------------------------------------

void MessageEngine::OnMessageMadeAvailable(message_gid_t message_id, BOOL read)
{
	for (INT32 i = (INT32)m_message_listeners.GetCount()-1; i >= 0 ; i--)
	{
		if (!m_multiple_message_changes)
			m_message_listeners.Get(i)->OnMessageChanged(message_id);
		m_message_listeners.Get(i)->OnMessageBodyChanged(message_id);
	}
}

// ----------------------------------------------------

void MessageEngine::OnAllMessagesAvailable()
{
	if (m_account_manager)
	{
		m_account_manager->OnAllMessagesAvailable();
		OpStatus::Ignore(m_account_manager->ToOnlineMode());
	}

	if (g_application->DetermineFirstRunType() == Application::RUNTYPE_FIRSTCLEAN)
		InitDefaultAccounts();

	for (UINT32 i = 0; i < m_message_listeners.GetCount(); i++)
	{
		m_message_listeners.Get(i)->OnMessageChanged(UINT_MAX);
		m_message_listeners.Get(i)->OnMessageBodyChanged(UINT_MAX);
	}

#ifdef M2_MAPI_SUPPORT
	if (m_mapi_listener)
		m_mapi_listener->OperaIsInitialized();
#endif //M2_MAPI_SUPPORT
}

// ----------------------------------------------------

void MessageEngine::OnMessageHidden(message_gid_t message_id, BOOL hidden)
{
	if (m_multiple_message_changes > 0)
	{
		return;
	}

	// update unread status
	Index* index = NULL;

	for (INT32 it = -1; (index = m_indexer->GetRange(0, IndexTypes::LAST, it)) != NULL; )
	{
		if (message_id == UINT_MAX)
		{
			index->ResetUnreadCount(); // OK, doesn't need optimization
		}
		else if (index->Contains(message_id))
		{
			INT32 model_flags = index->GetModelFlags();
			if ((model_flags&(1<<IndexTypes::MODEL_FLAG_HIDDEN))==0)
			{
				Index* unread = m_indexer->GetIndexById(IndexTypes::UNREAD);
				if (unread->Contains(message_id))
				{
					index->ResetUnreadCount(); // OK, is only called from manual filtering
				}
			}

			OnIndexChanged(index->GetId());
		}
	}

	OnMessageChanged(message_id);
}

// ----------------------------------------------------

void MessageEngine::OnMessagesRead(const OpINT32Vector& message_ids, BOOL read)
{
	for (UINT32 i = 0; i < m_message_listeners.GetCount(); i++)
	{
		m_message_listeners.Get(i)->OnMessagesRead(message_ids, read);
	}
}

// ----------------------------------------------------

void MessageEngine::OnMessageReplied(message_gid_t message_id)
{
	for (UINT32 i = 0; i < m_message_listeners.GetCount(); i++)
	{
		m_message_listeners.Get(i)->OnMessageReplied(message_id);
	}
}


/***********************************************************************************
**
**	Implementing the distribution of Interaction events to listeners
**
***********************************************************************************/

void MessageEngine::OnChangeNickRequired(UINT16 account_id, const OpStringC& old_nick)
{
	for (UINT32 i = 0; i < m_interaction_listeners.GetCount(); i++)
	{
		m_interaction_listeners.Get(i)->OnChangeNickRequired(account_id, old_nick);
	}
}

void MessageEngine::OnRoomPasswordRequired(UINT16 account_id, const OpStringC& room)
{
	for (UINT32 i = 0; i < m_interaction_listeners.GetCount(); i++)
	{
		m_interaction_listeners.Get(i)->OnRoomPasswordRequired(account_id, room);
	}
}

// ----------------------------------------------------

void MessageEngine::OnError(UINT16 account_id, const OpStringC& errormessage, const OpStringC& context, EngineTypes::ErrorSeverity severity)
{
	for (UINT32 i = 0; i < m_interaction_listeners.GetCount(); i++)
	{
//#pragma PRAGMAMSG("FG: Console doesn't handle anything but Critical very well yet (no prefs for opening severity 'Error' for M2)")
		m_interaction_listeners.Get(i)->OnError(account_id, errormessage, context, EngineTypes::GENERIC_ERROR);
	}
}

void MessageEngine::OnYesNoInputRequired(UINT16 account_id, EngineTypes::YesNoQuestionType type, OpString* sender, OpString* param)
{
	for (UINT32 i = 0; i < m_interaction_listeners.GetCount(); i++)
	{
		m_interaction_listeners.Get(i)->OnYesNoInputRequired(account_id, type, sender, param);
	}
}


/***********************************************************************************
**
**	Implementing the distribution of Chat events to listeners
**
***********************************************************************************/

#ifdef IRC_SUPPORT

void MessageEngine::OnChatStatusChanged(UINT16 account_id)
{
	for (UINT32 i = 0; i < m_chat_listeners.GetCount(); i++)
	{
		m_chat_listeners.Get(i)->OnChatStatusChanged(account_id);
	}
}

// ----------------------------------------------------

void MessageEngine::OnChatMessageReceived(UINT16 account_id,
	EngineTypes::ChatMessageType type, const OpStringC& message,
	const ChatInfo& chat_info, const OpStringC& chatter, BOOL is_private_chat)
{
	for (UINT32 i = 0; i < m_chat_listeners.GetCount(); i++)
	{
		m_chat_listeners.Get(i)->OnChatMessageReceived(account_id, type,
			message, chat_info, chatter, is_private_chat);
	}
}

// ----------------------------------------------------

void MessageEngine::OnChatServerInformation(UINT16 account_id, const OpStringC& server,
	const OpStringC& information)
{
	for (UINT32 i = 0; i < m_chat_listeners.GetCount(); i++)
	{
		m_chat_listeners.Get(i)->OnChatServerInformation(account_id, server, information);
	}
}

// ----------------------------------------------------

void MessageEngine::OnChatRoomJoined(UINT16 account_id, const ChatInfo& room)
{
	UINT32 i;

	for (i = 0; i < m_chat_listeners.GetCount(); i++)
	{
		m_chat_listeners.Get(i)->OnChatRoomJoining(account_id, room);
	}

	for (i = 0; i < m_chat_listeners.GetCount(); i++)
	{
		m_chat_listeners.Get(i)->OnChatRoomJoined(account_id, room);
	}
}

// ----------------------------------------------------

void MessageEngine::OnChatRoomLeft(UINT16 account_id, const ChatInfo& room,
	const OpStringC& kicker, const OpStringC& kick_reason)
{
	UINT32 i;

	for (i = 0; i < m_chat_listeners.GetCount(); i++)
	{
		m_chat_listeners.Get(i)->OnChatRoomLeaving(account_id, room, kicker, kick_reason);
	}

	for (i = 0; i < m_chat_listeners.GetCount(); i++)
	{
		m_chat_listeners.Get(i)->OnChatRoomLeft(account_id, room, kicker, kick_reason);
	}
}

// ----------------------------------------------------

void MessageEngine::OnChatterJoined(UINT16 account_id, const ChatInfo& room,
	const OpStringC& chatter, BOOL is_operator, BOOL is_voiced,
	const OpStringC& prefix, BOOL initial)
{
	UINT32 i;

	for (i = 0; i < m_chat_listeners.GetCount(); i++)
	{
		if (!m_chat_listeners.Get(i)->OnChatterJoining(account_id, room, chatter, is_operator, is_voiced, prefix, initial))
			return;
	}

	for (i = 0; i < m_chat_listeners.GetCount(); i++)
	{
		m_chat_listeners.Get(i)->OnChatterJoined(account_id, room, chatter, is_operator, is_voiced, prefix, initial);
	}
}

// ----------------------------------------------------

void MessageEngine::OnChatterLeft(UINT16 account_id, const ChatInfo& room,
	const OpStringC& chatter, const OpStringC& message, const OpStringC& kicker)
{
	UINT32 i;

	for (i = 0; i < m_chat_listeners.GetCount(); i++)
	{
		m_chat_listeners.Get(i)->OnChatterLeaving(account_id, room, chatter, message, kicker);
	}

	for (i = 0; i < m_chat_listeners.GetCount(); i++)
	{
		m_chat_listeners.Get(i)->OnChatterLeft(account_id, room, chatter, message, kicker);
	}
}

// ----------------------------------------------------

void MessageEngine::OnChatPropertyChanged(UINT16 account_id, const ChatInfo& room,
	const OpStringC& chatter, const OpStringC& changed_by,
	EngineTypes::ChatProperty property, const OpStringC& property_string,
	INT32 property_value)
{
	for (UINT32 i = 0; i < m_chat_listeners.GetCount(); i++)
	{
		m_chat_listeners.Get(i)->OnChatPropertyChanged(account_id, room, chatter, changed_by, property, property_string, property_value);
	}
}


void MessageEngine::OnWhoisReply(UINT16 account_id, const OpStringC& nick, const OpStringC& user_id, const OpStringC& host,
								 const OpStringC& real_name, const OpStringC& server, const OpStringC& server_info, const OpStringC& away_message,
								 const OpStringC& logged_in_as, BOOL is_irc_operator, int seconds_idle, int signed_on, const OpStringC& channels)
{
	for (UINT32 i = 0; i < m_chat_listeners.GetCount(); i++)
	{
		m_chat_listeners.Get(i)->OnWhoisReply(account_id, nick, user_id, host, real_name, server,
			server_info, away_message, logged_in_as, is_irc_operator, seconds_idle, signed_on, channels);
	}
}


void MessageEngine::OnInvite(UINT16 account_id, const OpStringC& nick, const ChatInfo& room)
{
	for (UINT32 i = 0; i < m_chat_listeners.GetCount(); ++i)
	{
		m_chat_listeners.Get(i)->OnInvite(account_id, nick, room);
	}
}


void MessageEngine::OnFileReceiveRequest(UINT16 account_id, const OpStringC& sender,
	const OpStringC& filename, UINT32 file_size, UINT port_number, UINT id)
{
	if (KioskManager::GetInstance()->GetNoSave() ||
		KioskManager::GetInstance()->GetNoDownload())
	{
		// We can't accept files in kiosk mode.
		return;
	}

	for (UINT32 i = 0; i < m_chat_listeners.GetCount(); ++i)
	{
		m_chat_listeners.Get(i)->OnFileReceiveRequest(account_id, sender, filename, file_size, port_number, id);
	}
}


void MessageEngine::OnUnattendedChatCountChanged(OpWindow* op_window, UINT32 unread)
{
	for (UINT32 i = 0; i < m_chat_listeners.GetCount(); ++i)
	{
		m_chat_listeners.Get(i)->OnUnattendedChatCountChanged(op_window, unread);
	}
}

void MessageEngine::OnChatReconnecting(UINT16 account_id, const ChatInfo& room)
{
	for (UINT32 i = 0; i < m_chat_listeners.GetCount(); ++i)
	{
		m_chat_listeners.Get(i)->OnChatReconnecting(account_id, room);
	}
}
#endif // IRC_SUPPORT

/***********************************************************************************
**
**	Implementing the reporting of results from interaction
**
***********************************************************************************/

OP_STATUS MessageEngine::ReportAuthenticationDialogResult(const OpStringC& user_name, const OpStringC& password, BOOL cancel, BOOL remember)
{
	// TO DO, fill in code

	return OpStatus::OK;
}

// ----------------------------------------------------

OP_STATUS MessageEngine::ReportChangeNickDialogResult(UINT16 account_id, OpString& new_nick)
{
	Account* account = GetAccountById(account_id);

	if (!account)
		return OpStatus::ERR;

	return account->ChangeNick(new_nick);
}

OP_STATUS MessageEngine::ReportRoomPasswordResult(UINT16 account_id, OpString& room, OpString& password)
{
    Account* account = GetAccountById(account_id);

	if (!account)
		return OpStatus::ERR;

	// We MUST NOT lookup the password when responding to the dialog as we don't want
	// it to find any passwords saved, if no password is entered, or something new
	return account->JoinChatRoom(room, password, TRUE);
}

// ----------------------------------------------------

OP_STATUS MessageEngine::Sent(Account* account, message_gid_t uid, OP_STATUS transmit_status)
{
	if (transmit_status == OpStatus::OK)
	{
		m_store->MessageSent(uid);
#ifdef ENABLE_USAGE_REPORT
		if (g_mail_report)
			g_mail_report->IncreaseSentMailCount();
#endif

	}
	return OpStatus::OK;
}

OP_STATUS MessageEngine::SignalStartSession(BOOL incoming)
{
#if 0
	BrowserUtils* browser_utils = GetGlueFactory()->GetBrowserUtils();
    if (incoming)
	{
		m_incoming_sessions++;
		if (m_incoming_sessions==1 && browser_utils)
			browser_utils->UpdateAllInputActionsButtons();
	}
	else
	{
		m_outgoing_sessions++;
		if (m_outgoing_sessions==1 && browser_utils)
			browser_utils->UpdateAllInputActionsButtons();
	}
#endif

    return OpStatus::OK;
}

OP_STATUS MessageEngine::SignalEndSession(BOOL incoming, int message_count, BOOL report_session)
{
#if 0
	BrowserUtils* browser_utils = GetGlueFactory()->GetBrowserUtils();
	if (incoming)
	{
		if (report_session)
            m_total_downloaded_messages += message_count;

		m_incoming_sessions--;
		if (m_incoming_sessions==0)
		{
			if (browser_utils)
				browser_utils->UpdateAllInputActionsButtons();
		}
	}
	else
	{
        if (report_session)
		    m_total_uploaded_messages += message_count;

		m_outgoing_sessions--;
		if (m_outgoing_sessions==0)
		{
			if (browser_utils)
				browser_utils->UpdateAllInputActionsButtons();
		}
	}

    m_report_sessions |= report_session;

	if (m_incoming_sessions==0 && m_outgoing_sessions==0 && m_report_sessions)
	{
		OpString status_text;
		if (m_total_downloaded_messages==0 && m_total_uploaded_messages==0)
		{
			OpStatus::Ignore(g_languageManager->GetString(Str::S_NO_NEW_MESSAGES, status_text));
		}
		else
		{
			OpString format_string;
			if (m_total_downloaded_messages>0)
			{
				OpStatus::Ignore(g_languageManager->GetString(Str::S_DOWNLOADED_MESSAGE_COUNT, format_string));
				OpString downloaded_string;
				downloaded_string.AppendFormat(format_string.CStr(), m_total_downloaded_messages);
				OpStatus::Ignore(status_text.Set(downloaded_string));
			}

			if (m_total_uploaded_messages>0)
			{
				if (status_text.HasContent())
					OpStatus::Ignore(status_text.Append(UNI_L(", ")));

				OpStatus::Ignore(g_languageManager->GetString(Str::S_MESSAGES_SENT, format_string));
				OpString uploaded_string;
				uploaded_string.AppendFormat(format_string.CStr(), m_total_uploaded_messages);
				OpStatus::Ignore(status_text.Append(uploaded_string));
			}
		}

		ProgressInfo dummy_progress;
		BOOL dummy;
		dummy_progress.Clear(dummy);
		dummy_progress.m_display_flag = ProgressInfo::DISPLAY_NEWMESSAGES;
		dummy_progress.m_protocol_count = m_total_downloaded_messages;
		dummy_progress.m_protocol_size = m_total_uploaded_messages;

		OnProgressChanged(dummy_progress, status_text);
		m_total_downloaded_messages = m_total_uploaded_messages = 0;
        m_report_sessions = FALSE;
	}
#endif

    return OpStatus::OK;
}

// ----------------------------------------------------

OP_STATUS MessageEngine::CreateMessage(Message& new_message, UINT16 account_id, message_gid_t old_message_id, MessageTypes::CreateType type) const
{
	return new_message.Init(account_id, old_message_id, type);
}

Account* MessageEngine::CreateAccount()
{
	RETURN_VALUE_IF_ERROR(InitMessageDatabaseIfNeeded(), NULL);
    return OP_NEW(Account, (*m_message_database));
}

OP_STATUS MessageEngine::SetOfflineMode(BOOL offline_mode)
{
	if (offline_mode)
		return m_account_manager->CloseAllConnections();
	else
		return m_account_manager->ToOnlineMode();
}

OP_STATUS MessageEngine::GetMessage(Message& message, message_gid_t id, BOOL full, BOOL timeout_request)
{
	RETURN_IF_ERROR(m_store->GetMessage(message, id));

	if (full && !m_multiple_message_changes)
	{
		BOOL has_body;

		// Check if this message has a body, else we fetch it
		RETURN_IF_ERROR(m_store->HasMessageDownloadedBody(id, has_body));

		if (has_body)
			RETURN_IF_ERROR(m_store->GetMessageData(message));
		else if (message.GetAccountPtr())
	 		RETURN_IF_ERROR(message.GetAccountPtr()->FetchMessage(id, !timeout_request, FALSE));
	}

    return OpStatus::OK;
}

OP_STATUS MessageEngine::GetMessage(Message& message, const OpStringC8& message_id, BOOL full)
{
    if (message_id.IsEmpty())
        return OpStatus::OK;

    message_gid_t id;

	id = m_store->GetMessageByMessageId(message_id);

    if (id == 0)
        return OpStatus::ERR; //Not found

    return GetMessage(message, id, full);
}

OP_STATUS MessageEngine::FetchCompleteMessage(message_gid_t message_id)
{
	Account* account = GetAccountById(m_store->GetMessageAccountId(message_id));
	if (account)
		return account->FetchMessage(message_id, TRUE, TRUE);

	return OpStatus::OK;
}

// ----------------------------------------------------

OP_STATUS MessageEngine::FetchMessages(BOOL enable_signalling, BOOL full_sync) const
{
    if (!m_account_manager)
        return OpStatus::ERR_NULL_POINTER;

    Account* account = m_account_manager->GetFirstAccount();
    while (account)
    {
		if (account->GetManualCheckEnabled() && account->GetIncomingProtocol() != AccountTypes::IRC)
		{
			if (full_sync)
				RETURN_IF_ERROR(account->RefreshAll());
			else
				RETURN_IF_ERROR(account->FetchMessages(enable_signalling));
		}
        account = (Account*)account->Suc();
    }
    return OpStatus::OK;
}

OP_STATUS MessageEngine::FetchMessages(UINT16 account_id, BOOL enable_signalling) const
{
    if (!m_account_manager)
        return OpStatus::ERR_NULL_POINTER;

    OP_STATUS ret;
    Account* account = NULL;
    if (((ret=m_account_manager->GetAccountById(account_id, account)) != OpStatus::OK) || !account)
        return ret;

    return account->FetchMessages(enable_signalling);
}

OP_STATUS MessageEngine::SelectFolder(UINT32 index_id)
{
	Index* index = m_indexer->GetIndexById(index_id);
	INT16 account_id;
	Account* account = NULL;

	if (!m_account_manager || !index)
		return OpStatus::ERR_NULL_POINTER;

	// Check which account is associated with this index
	account_id = (INT16)index->GetAccountId();
	RETURN_IF_ERROR(m_account_manager->GetAccountById(account_id, account));
	if (!account)
		return OpStatus::OK;

	// Select folder in account
	return account->SelectFolder(index_id);
}

OP_STATUS MessageEngine::RefreshFolder(UINT32 index_id)
{
    if (index_id >= IndexTypes::FIRST_ACCOUNT && index_id < IndexTypes::LAST_ACCOUNT)
    {
        Account* account = NULL;
        RETURN_IF_ERROR(m_account_manager->GetAccountById(index_id - IndexTypes::FIRST_ACCOUNT, account));
        if (!account)
            return OpStatus::ERR;

        return account->RefreshAll();
    }
    else
    {
        Index* index = m_indexer->GetIndexById(index_id);
        if (!index)
            return OpStatus::ERR;

		if (index->GetType() == IndexTypes::UNIONGROUP_INDEX)
		{
			OpINT32Vector children;
			RETURN_IF_ERROR(m_indexer->GetChildren(index_id, children));
			for (UINT i = 0; i < children.GetCount(); i++)
				RETURN_IF_ERROR(RefreshFolder(children.Get(i)));
			return OpStatus::OK;
		}

        UINT32 account_id = index->GetAccountId();
        if (account_id)
        {
            Account* account = NULL;
            RETURN_IF_ERROR(m_account_manager->GetAccountById(account_id, account));
            if (!account)
                return OpStatus::ERR;

            return account->RefreshFolder(index_id);
        }
        else
        {
            for (Account* account = m_account_manager->GetFirstAccount(); account; account = (Account*)account->Suc())
            {
                switch (account->GetType())
                {
                case AccountTypes::IMAP:
                case AccountTypes::POP:
                case AccountTypes::NEWS:
                case AccountTypes::RSS:
                    account->RefreshAll();
                    break;
                }
            }
            return OpStatus::OK;
        }
    }
}

OP_STATUS MessageEngine::StopFetchingMessages() const
{
    if (!m_account_manager)
        return OpStatus::ERR_NULL_POINTER;

    Account* account = m_account_manager->GetFirstAccount();
    while (account)
    {
        account->StopFetchingMessages();
        account = (Account*)account->Suc();
    }
    return OpStatus::OK;

}

OP_STATUS MessageEngine::StopSendingMessages()
{
    if (!m_account_manager)
        return OpStatus::ERR_NULL_POINTER;

    Account* account = m_account_manager->GetFirstAccount();
    while (account)
    {
        account->StopSendingMessage();
        account = (Account*)account->Suc();
    }
    return OpStatus::OK;
}

OP_STATUS MessageEngine::StopSendingMessages(UINT16 account_id)
{
    if (!m_account_manager)
        return OpStatus::ERR_NULL_POINTER;

    Account* account = m_account_manager->GetFirstAccount();
    while (account)
    {
		if(account->GetAccountId() == account_id)
		{
			account->StopSendingMessage();
			return OpStatus::OK;
		}
        account = (Account*)account->Suc();
    }
    return OpStatus::OK;
}

OP_STATUS MessageEngine::StopFetchingMessages(UINT16 account_id) const
{
    if (!m_account_manager)
        return OpStatus::ERR_NULL_POINTER;

    OP_STATUS ret;
    Account* account = NULL;
    if (((ret=m_account_manager->GetAccountById(account_id, account)) != OpStatus::OK) || !account)
        return ret;

    return account->StopFetchingMessages();
}

OP_STATUS MessageEngine::SendMessages(UINT16 only_from_account_id)
{
	Index* outbox = m_indexer->GetIndexById(IndexTypes::OUTBOX);
	Index* trash = m_indexer->GetIndexById(IndexTypes::TRASH);

	if (outbox == NULL)
	{
		return OpStatus::ERR;
	}

	for (INT32SetIterator it(outbox->GetIterator()); it; it++)
	{
		message_gid_t id = it.GetData();

		if (trash->Contains(id))
			continue;

		Message message;
		if (OpStatus::IsSuccess(GetMessage(message, id, FALSE)) &&
			(only_from_account_id==0 || only_from_account_id==message.GetAccountId()))
		{
			Account* account = NULL;

			RETURN_IF_ERROR(m_account_manager->GetAccountById(message.GetAccountId(), account));
			if (account==NULL)
				return OpStatus::ERR;

			RETURN_IF_ERROR(account->SendMessage(message, FALSE));
		}
	}

    return OpStatus::OK;
}

OP_STATUS MessageEngine::SendMessage(Message& message, BOOL anonymous)
{
	OpString error_context;
	OpStatus::Ignore(error_context.Set("Sending mail"));

	OP_STATUS ret;
	UINT16    account_id = message.GetAccountId();
	Account*  account    = NULL;

	OP_ASSERT(m_account_manager);

	// Get account
    if (account_id == 0)
        return OpStatus::ERR_OUT_OF_RANGE;

	RETURN_IF_ERROR(m_account_manager->GetAccountById(account_id, account));
	if (!account)
		return OpStatus::ERR_NULL_POINTER;

	// Set message properties
	message.SetFlag(Message::IS_OUTGOING,TRUE);
	message.SetFlag(Message::IS_READ, TRUE); // all outgoing are read

    BOOL is_resent = message.IsFlagSet(Message::IS_RESENT);

    OpString8 dummy_contenttype;
    OpString from, organization, error_message;

	if (OpStatus::IsError(account->GetFormattedEmail(from, TRUE)) ||
		OpStatus::IsError(account->GetOrganization(organization)) ||
		(from.HasContent() && OpStatus::IsError(message.SetFrom(from, TRUE))) ||
		(!is_resent && organization.HasContent() && OpStatus::IsError(message.SetHeaderValue(Header::ORGANIZATION, organization, TRUE))))
	{
		OpString format_string;

		g_languageManager->GetString(Str::S_FROM_OR_ORGANIZATION_FAILED, format_string);
		error_message.AppendFormat(format_string.CStr(), from.CStr(), organization.CStr());
		account->OnError(error_message);

		return OpStatus::ERR;
	}

    if (from.IsEmpty())
	{
		account->OnError(Str::S_ILLEGAL_FROM_ADDRESS);
        return OpStatus::ERR;
	}

    BOOL only_to_outbox = !anonymous &&
                          ( account->GetQueueOutgoing() || //User has asked for message to be put in queue
                            g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode) ); //Opera is offline. Put in queue

    //Generate ID for message if needed
	if (message.GetId())
	{
		m_store->UpdateMessage(message, TRUE);
		m_store->SetRawMessage(message);
	}
	else
	{
		message_gid_t new_id;
		if (OpStatus::IsError(ret=m_store->AddMessage(new_id, message)))
		{
			account->OnError(Str::S_ADDMESSAGE_FAILED);
			return ret;
		}
	}

	// move from drafts to outbox
	Index *outbox = m_indexer->GetIndexById(IndexTypes::OUTBOX);
	Index *drafts = m_indexer->GetIndexById(IndexTypes::DRAFTS);

	if (!drafts || !outbox)
	{
		account->OnError(Str::S_DRAFTS_OR_OUTBOX_NOT_FOUND);
		return OpStatus::ERR;
	}

	if (OpStatus::IsError(ret=outbox->NewMessage(message.GetId())))
	{
		account->OnError(Str::S_FAILED_MOVING_TO_OUTBOX);
		return ret;
	}

	if (OpStatus::IsError(ret=drafts->RemoveMessage(message.GetId())))
	{
		account->OnError(Str::S_FAILED_MOVING_FROM_DRAFTS);
		return ret;
	}

	// finally send the message if not queueing
	if (!only_to_outbox)
	{
		if (OpStatus::IsError(ret=account->SendMessage(message, anonymous)))
		{
			account->OnError(Str::S_SENDING_MESSAGE_FAILED);
			return ret;
		}
	}

	return OpStatus::OK;
}


OP_STATUS MessageEngine::DisconnectAccount(UINT16 account_id) const
{
    if (!m_account_manager)
        return OpStatus::ERR_NULL_POINTER;

    OP_STATUS ret;
    Account* account = NULL;
    if (((ret=m_account_manager->GetAccountById(account_id, account)) != OpStatus::OK) || !account)
        return ret;

	OpStatus::Ignore(account->Disconnect(TRUE));
    OpStatus::Ignore(account->Disconnect(FALSE));
	return OpStatus::OK;
}

OP_STATUS MessageEngine::DisconnectAccounts() const
{
    if (!m_account_manager)
        return OpStatus::ERR_NULL_POINTER;

    Account* account = m_account_manager->GetFirstAccount();
    while (account)
    {
		OpStatus::Ignore(account->Disconnect(TRUE));
		OpStatus::Ignore(account->Disconnect(FALSE));
        account = (Account*)account->Suc();
    }
    return OpStatus::OK;
}


OP_STATUS MessageEngine::SaveDraft(Message* m2message)
{
	Message* message = (Message*) m2message;

    if (!message)
        return OpStatus::ERR_NULL_POINTER;

	message->SetFlag(Message::IS_OUTGOING,TRUE);
	message->SetFlag(Message::IS_READ, TRUE); // all outgoing are read

	/*
	RETURN_IF_ERROR(message->MimeEncodeAttachments(FALSE, TRUE));
	*/

    //Generate ID for message if needed
    message_gid_t new_id;

	if (message->GetId())
	{
		m_store->UpdateMessage(*message, TRUE);
		m_store->SetRawMessage(*message);
	}
	else
	{
		RETURN_IF_ERROR(m_store->AddMessage(new_id, *message));
		RETURN_IF_ERROR(m_indexer->NewMessage(*message));
	}

	return OpStatus::OK;
}

OP_STATUS MessageEngine::HasMessageDownloadedBody(message_gid_t id, BOOL& has_downloaded_body)
{
	return m_store->HasMessageDownloadedBody(id, has_downloaded_body);
}

OP_STATUS MessageEngine::OnDeleted(message_gid_t message_id, BOOL permanently)
{
	if (permanently)
	{
		return m_message_database->RemoveMessage(message_id);
	}
	else
	{
		Index* trash = m_indexer->GetIndexById(IndexTypes::TRASH);
		Index* unread = m_indexer->GetIndexById(IndexTypes::UNREAD);
		if (trash && unread)
		{
			RETURN_IF_ERROR(trash->NewMessage(message_id));
			RETURN_IF_ERROR(unread->RemoveMessage(message_id));
			RETURN_IF_ERROR(m_store->SetMessageFlag(message_id, Message::IS_READ, TRUE));
			OnMessageChanged(message_id);
		}
	}

	return OpStatus::OK;
}

OP_STATUS MessageEngine::RemoveMessages(OpINT32Vector& message_ids, BOOL permanently)
{
	OpAutoINT32HashTable<OpINT32Vector> account_message_ids;
	Account* account;

	RETURN_IF_ERROR(SplitIntoAccounts(message_ids, account_message_ids));

	for (INT32HashIterator<OpINT32Vector> it(account_message_ids); it; it++)
	{
		account = GetAccountById(it.GetKey());
		RETURN_IF_ERROR(RemoveMessages(*it.GetData(), account, permanently));
	}

	return OpStatus::OK;
}

OP_STATUS MessageEngine::RemoveMessages(OpINT32Vector& message_ids, Account* account, BOOL permanently)
{
	// Don't remove messages that are being edited
	for (int i = message_ids.GetCount() - 1; i >= 0; i--)
	{
		if (m_messages_being_edited.Contains(message_ids.Get(i)))
			message_ids.Remove(i);
	}

	// Remove messages from account (remote)
	if(account)
		account->RemoveMessages(message_ids, permanently);

	// Remove messages from store and index (local)
	MultipleMessageChanger changer(message_ids.GetCount());

	for(UINT32 index = 0; index < message_ids.GetCount(); index++)
		RETURN_IF_ERROR(OnDeleted(message_ids.Get(index), permanently));

	return OpStatus::OK;
}


OP_STATUS MessageEngine::UndeleteMessages(const OpINT32Vector& message_ids)
{
	Index *trash = m_indexer->GetIndexById(IndexTypes::TRASH);
	if(!trash)
		return OpStatus::ERR_NULL_POINTER;

	OpAutoINT32HashTable<OpINT32Vector> account_message_ids;

	RETURN_IF_ERROR(SplitIntoAccounts(message_ids, account_message_ids));

	//Notify backends that message is removed from trash
	for (INT32HashIterator<OpINT32Vector> it(account_message_ids); it; it++)
	{
		Account* account = GetAccountById(it.GetKey());
		if (account)
			OpStatus::Ignore(account->MessagesMovedFromTrash(*it.GetData()));
	}

	MultipleMessageChanger changer(message_ids.GetCount());

	for(UINT32 index = 0; index < message_ids.GetCount(); index++)
	{
		RETURN_IF_ERROR(trash->RemoveMessage(message_ids.Get(index)));
		OnMessageChanged(message_ids.Get(index));
	}

	return OpStatus::OK;
}


OP_STATUS MessageEngine::EmptyTrash()
{
	Index* trash = GetIndexById(IndexTypes::TRASH);
	if (trash)
	{
		if (trash->MessageCount()==0)
		{
			//No messages to remove. Signal all active backends that we at least have had an Empty Trash request
			Account* account = m_account_manager->GetFirstAccount();
			while (account)
			{
				if (m_account_manager->IsAccountActive(account->GetAccountId()))
				{
					OpStatus::Ignore(account->EmptyTrash(FALSE));
					OpStatus::Ignore(account->EmptyTrash(TRUE));
				}
				account = static_cast<Account*>(account->Suc());
			}
		}
		else
		{
			for (INT32SetIterator it(trash->GetIterator()); it; it++)
			{
				message_gid_t message_id = it.GetData();

				// Only remove messages from active accounts
				if (message_id && m_account_manager->IsAccountActive(m_store->GetMessageAccountId(message_id)))
					m_messages_to_delete.Add(message_id);
			}

			// trash the first 50 immediately
			UINT trash_count = min(trash->MessageCount(), 50U);
			for (UINT j = 0; j < trash_count; j++)
			{
				Receive(MSG_M2_EMPTY_TRASH);
			}

			return m_loop->Post(MSG_M2_EMPTY_TRASH);
		}
	}

	return OpStatus::OK;
}


OP_STATUS MessageEngine::Receive(OpMessage message)
{
	switch (message)
	{
		case MSG_M2_EMPTY_TRASH:
			if (m_messages_to_delete.GetCount() > 0)
			{
				if (!m_deleting_trash)
				{
					SignalStartSession(TRUE); //It is actually neither incoming not outgoing
					m_deleting_trash = TRUE;

					//Signal all accounts
					Account* account = m_account_manager->GetFirstAccount();
					while (account)
					{
						if (m_account_manager->IsAccountActive(account->GetAccountId()))
							OpStatus::Ignore(account->EmptyTrash(FALSE));
						account = static_cast<Account*>(account->Suc());
					}
				}

				message_gid_t message_id = m_messages_to_delete.Remove(m_messages_to_delete.GetCount()-1);

				Index* trash = (Index*)MessageEngine::GetInstance()->GetIndexById(IndexTypes::TRASH);

				if (trash->Contains(message_id))
				{
					if (OnDeleted(message_id, TRUE) != OpStatus::OK) // not from_account, since EmptyTrash() will do that
					{
						OP_ASSERT(0);
					}
				}

				if (m_messages_to_delete.GetCount() > 0)
				{
					return m_loop->Post(MSG_M2_EMPTY_TRASH);
				}
				else
				{
					//Signal all accounts
					Account* account = m_account_manager->GetFirstAccount();
					while (account)
					{
						if (m_account_manager->IsAccountActive(account->GetAccountId()))
							OpStatus::Ignore(account->EmptyTrash(TRUE));
						account = static_cast<Account*>(account->Suc());
					}

					SignalEndSession(TRUE, 0, FALSE);
					m_deleting_trash = FALSE;
				}
			}
			break;
		case MSG_M2_INDEX_CHANGED:
			for (unsigned i = 0; i < m_engine_listeners.GetCount(); i++)
			{
				for (unsigned j = 0; j < m_index_changed.GetCount(); j++)
					m_engine_listeners.Get(i)->OnIndexChanged(m_index_changed.GetByIndex(j));
			}
			m_index_changed.Clear();
			m_posted_index_changed = FALSE;
			break;
		case MSG_M2_MAILCOMMAND:
		{
			OP_STATUS result = OpStatus::ERR;
			switch ((URLType)m_command_url.GetAttribute(URL::KType))
			{
				case URL_HTTP :
				case URL_HTTPS :
					result = MailCommandHTTP(m_command_url);
					break;
#ifdef IRC_SUPPORT
				case URL_IRC :
					result = MailCommandIRC(m_command_url);
					break;
				case URL_CHAT_TRANSFER :
					result = MailCommandChatTransfer(m_command_url);
					break;
#endif // IRC_SUPPORT
				case URL_NEWS :
				case URL_SNEWS :
					result = MailCommandNews(m_command_url);
					break;
			}
			m_command_url = URL();
			return result;
		}
	}
	return OpStatus::OK;
}


OP_STATUS MessageEngine::CancelMessage(message_gid_t message_id)
{
	Message original_message;
	if (OpStatus::IsError(GetMessage(original_message, message_id, FALSE)))
		return OpStatus::OK; //No message to cancel

	if (!original_message.IsFlagSet(Message::IS_OUTGOING) ||
		!original_message.IsFlagSet(Message::IS_NEWS_MESSAGE) ||
		original_message.IsFlagSet(Message::IS_CONTROL_MESSAGE))
	{
		return OpStatus::ERR_OUT_OF_RANGE;
	}

	Message cancel_message;

	RETURN_IF_ERROR(CreateMessage(cancel_message, original_message.GetAccountId(), original_message.GetId(), MessageTypes::CANCEL_NEWSPOST));

	return SendMessage(cancel_message, FALSE);
}

OP_STATUS MessageEngine::UpdateMessage(Message& message)
{
	return m_store->UpdateMessage(message);
}

OP_STATUS MessageEngine::IndexRead(index_gid_t index_id)
{
	Index* index = m_indexer->GetIndexById(index_id);
	Index* unread = m_indexer->GetIndexById(IndexTypes::UNREAD);
	OpINT32Vector message_ids;

	if (index && unread)
	{
		Index common;
		common.SetVisible(FALSE);
		RETURN_IF_ERROR(m_indexer->IntersectionIndexes(common,unread,index,0));

		for (INT32SetIterator it(common.GetIterator()); it; it++)
		{
			message_gid_t id = it.GetData();
			if (id && !index->MessageHidden(id))
			{
				message_ids.Add(id);
			}
		}

		return MessagesRead(message_ids, TRUE);
	}

	return OpStatus::OK;
}

OP_STATUS MessageEngine::MessageFlagged(message_gid_t message_id, BOOL flagged)
{
	RETURN_IF_ERROR(m_store->SetMessageFlag(message_id, Message::IS_FLAGGED, flagged));
	GetIndexer()->AddToPinBoard(message_id, flagged);
	Account* account = GetAccountById(m_store->GetMessageAccountId(message_id));
	if (account)
		return account->FlaggedMessage(message_id, flagged);
	return OpStatus::OK;
}

OP_STATUS MessageEngine::MessageRead(Message& message, BOOL read)
{
	OpINT32Vector message_ids;

	// Mark local message as read
	message.SetFlag(Message::IS_READ, read);

	// Mark message as read in other instances
	RETURN_IF_ERROR(message_ids.Add(message.GetId()));

	return MessagesRead(message_ids, read);
}

OP_STATUS MessageEngine::MessagesRead(OpINT32Vector& message_ids, BOOL read)
{
	MultipleMessageChanger changer(message_ids.GetCount());

	for(int index = message_ids.GetCount() - 1; index >= 0; index--)
	{
		message_gid_t message_id = message_ids.Get(index);

		// Check if message is already marked correctly
		if (!(m_store->GetMessageFlags(message_id) & (1 << Message::IS_READ)) != read)
		{
			message_ids.Remove(index);
			continue;
		}

		// sync unread index first
		if (m_indexer->MessageRead(message_id, read) != OpStatus::OK)
		{
			OP_ASSERT(0);
		}
		// then toggle flag..
		RETURN_IF_ERROR(m_store->SetMessageFlag(message_id, Message::IS_READ, read));
	}

	// Tell backends about changes if necessary
	OpAutoINT32HashTable<OpINT32Vector> account_messages;

	RETURN_IF_ERROR(SplitIntoAccounts(message_ids, account_messages));

	for (INT32HashIterator<OpINT32Vector> it(account_messages); it; it++)
	{
		Account* account = GetAccountById(it.GetKey());
		if (account)
			account->ReadMessages(*it.GetData(), read);
	}

	OnMessagesRead(message_ids, read);

	return OpStatus::OK;
}

OP_STATUS MessageEngine::MessageReplied(message_gid_t message, BOOL replied)
{
	RETURN_IF_ERROR(m_store->SetMessageFlag(message, Message::IS_REPLIED, replied));
	if (replied)
	{
		UINT16 account_id = m_store->GetMessageAccountId(message);
		Account* account = GetAccountById(account_id);

		if (account)
			RETURN_IF_ERROR(account->ReplyToMessage(message));
	}

	return OpStatus::OK;
}

OP_STATUS MessageEngine::MarkAsSpam(const OpINT32Vector& message_ids, BOOL is_spam, BOOL propagate_call, BOOL imap_move)
{
	for (UINT32 i = 0; i < message_ids.GetCount(); i++)
	{
		if (is_spam)
		{
			m_indexer->Spam(message_ids.Get(i));
		}
		else
		{
			m_indexer->NotSpam(message_ids.Get(i), !propagate_call, 0);
		}
	}

	OpAutoINT32HashTable<OpINT32Vector> account_message_ids;
	Account* account;

	RETURN_IF_ERROR(SplitIntoAccounts(message_ids, account_message_ids));

	for (INT32HashIterator<OpINT32Vector> it(account_message_ids); it; it++)
	{
		account = GetAccountById(it.GetKey());
		if (account)
			RETURN_IF_ERROR(account->MarkMessagesAsSpam(*it.GetData(), is_spam, imap_move));
	}

	return OpStatus::OK;
}


void MessageEngine::SetSpamLevel(INT32 level)
{
	OpStatus::Ignore(m_indexer->SetSpamLevel(level));
}


INT32 MessageEngine::GetSpamLevel()
{
	return m_indexer->GetSpamLevel();
}


OP_STATUS MessageEngine::AddToContacts(message_gid_t message, int parent_folder_id)
{
	return m_indexer->NotSpam(message,TRUE,parent_folder_id);
}


OP_STATUS MessageEngine::GetFromAddress(message_gid_t message_id, OpString& sender, BOOL do_quote_pair_encode)
{
	sender.Empty();

	Message message;
	RETURN_IF_ERROR(GetMessage(message, message_id, FALSE));

	return message.GetFromAddress(sender, do_quote_pair_encode);
}


OP_STATUS MessageEngine::ImportMessage(Message* message, OpString& original_folder_name, BOOL move_to_sent)
{
	if (message == 0)
		return OpStatus::ERR_NULL_POINTER;

    message_gid_t message_id;

	OpString8 x_opera_status;
	if (OpStatus::IsSuccess(message->GetHeaderValue("X-Opera-Status", x_opera_status)) &&
		x_opera_status.HasContent())
	{
		if (x_opera_status.Length() == 114)
		{
			// Get the value of the flags.
			OpString8 flagsLW_string, flagsHW_string;
			OpStatus::Ignore(flagsLW_string.Set(x_opera_status.SubString(34).CStr(), 8));
			OpStatus::Ignore(flagsHW_string.Set(x_opera_status.SubString(50).CStr(), 8));
			OpStatus::Ignore(flagsLW_string.Insert(0, "0x"));
			OpStatus::Ignore(flagsHW_string.Insert(0, "0x"));

			// Get the value of the label.
			OpString8 label_string;
			OpStatus::Ignore(label_string.Set(x_opera_status.SubString(60).CStr(), 2));
			OpStatus::Ignore(label_string.Insert(0, "0x"));

			char* end_ptr = 0;
			const UINT64 flags = static_cast<UINT64>(op_strtol(flagsHW_string.CStr(), &end_ptr, 16)) << 32 | op_strtol(flagsLW_string.CStr(), &end_ptr, 16);

			// Remember the default flags for this message.
			const UINT32 default_flags = message->GetAllFlags();

			// Update the message with the values we found.
			message->SetAllFlags(flags);

			// If the message is outgoing we can be pretty sure it should be
			// moved to sent.
			if (message->IsFlagSet(Message::IS_OUTGOING))
				move_to_sent = TRUE;

			// Most flags can't be trusted, so after examining those few we
			// actually trust, it's best to just reset them.
			message->SetAllFlags(default_flags);
		}
		else
			OP_ASSERT(0); // Wrong length of X-Opera-Status, or header changed?

		// We have the information we need, delete the old header.
		message->RemoveXOperaStatusHeader();
		message->CopyCurrentToOriginalHeaders();
	}

	message->SetFlag(Message::IS_IMPORTED, TRUE);
	message->SetFlag(Message::IS_SEEN, TRUE);

	if (move_to_sent)
	{
		message->SetFlag(Message::IS_NEWS_MESSAGE, FALSE); //just to be sure...
		message->SetFlag(Message::IS_OUTGOING, TRUE);
		message->SetFlag(Message::IS_SENT, TRUE);
	}

	RETURN_IF_ERROR(m_store->AddMessage(message_id, *message));
	RETURN_IF_ERROR(m_indexer->NewMessage(*message));

	if (move_to_sent)	// add message to Sent index
	{
		Index* sent_index = m_indexer->GetIndexById(IndexTypes::SENT);
		if (!sent_index)
			return OpStatus::ERR;

		sent_index->NewMessage(message_id);
	}

	return OpStatus::OK;
}


OP_STATUS MessageEngine::ImportContact(OpString& full_name, OpString& email_address)
{
	return OpStatus::OK;
}

OP_STATUS MessageEngine::RemoveIndex(index_gid_t id)
{
	if ((id >= IndexTypes::FIRST_SEARCH && id < IndexTypes::LAST_SEARCH) ||
		(id >= IndexTypes::FIRST_CONTACT && id < IndexTypes::LAST_CONTACT) ||
		(id >= IndexTypes::FIRST_NEWSGROUP && id < IndexTypes::LAST_NEWSGROUP) ||
		(id >= IndexTypes::FIRST_THREAD && id < IndexTypes::LAST_THREAD) ||
		(id >= IndexTypes::FIRST_IMAP && id < IndexTypes::LAST_IMAP) ||
		(id >= IndexTypes::FIRST_FOLDER && id < IndexTypes::LAST_FOLDER) ||
		(id >= IndexTypes::FIRST_NEWSFEED && id < IndexTypes::LAST_NEWSFEED) ||
		(id >= IndexTypes::FIRST_UNIONGROUP && id < IndexTypes::LAST_UNIONGROUP) ||
		(id >= IndexTypes::FIRST_ARCHIVE && id < IndexTypes::LAST_ARCHIVE))
	{
		Account* account = NULL;
		Index* index     = m_indexer->GetIndexById(id);

		if (!index)
			return OpStatus::ERR_NULL_POINTER;

		if (index->GetHideFromOther())
		{
			index->SetHideFromOther(FALSE);
		}

		// Clean out index models
		for (unsigned m = 0; m < m_index_models.GetCount(); m++)
		{
			IndexModel* model = m_index_models.Get(m);
			if (model && model->GetIndexId() == id)
				model->Empty();
		}

		// See if there is an account for this index
		OpStatus::Ignore(m_account_manager->GetAccountById(index->GetAccountId(), account));

		// If there is an account, call appropriate functions, else just remove
		if (account && (id < IndexTypes::FIRST_UNIONGROUP || id > IndexTypes::LAST_UNIONGROUP))
		{
			RETURN_IF_ERROR(account->RemoveSubscribedFolder(id));
			return account->CommitSubscribedFolders();
		}
		else
		{
			if (id >= IndexTypes::FIRST_UNIONGROUP && id < IndexTypes::LAST_UNIONGROUP)
			{
				OpINT32Vector children;
				m_indexer->GetChildren(id, children, TRUE);
				for (UINT32 i = 0; i < children.GetCount(); i++)
				{
					RETURN_IF_ERROR(RemoveIndex(children.Get(i)));
				}
			}
			index->Empty();
			return m_indexer->RemoveIndex(index);
		}
	}

	return OpStatus::OK;
}

OP_STATUS MessageEngine::IndexRemoved(Indexer *indexer, UINT32 index_id)
{
	UINT32 count = m_index_models.GetCount();

	for (UINT m = 0; m < count; m++)
	{
		IndexModel* model = m_index_models.Get(m);
		if (model && model->GetIndexId() == index_id)
		{
			model->Empty();
		}
	}
	return OpStatus::OK;
}

OP_STATUS MessageEngine::IndexKeywordChanged(Indexer *indexer, UINT32 index_id, const OpStringC8& old_keyword, const OpStringC8& new_keyword)
{
	OpINT32Vector message_ids;
	Index*        index = GetIndexById(index_id);

	if (!index)
		return OpStatus::ERR_NULL_POINTER;

	// For all messages in the index, remove the existing keyword, add the new one
	for (INT32SetIterator it(index->GetIterator()); it; it++)
	{
		message_gid_t message_id = it.GetData();

		RETURN_IF_ERROR(KeywordRemoved(indexer, message_id, old_keyword));
		RETURN_IF_ERROR(KeywordAdded(indexer, message_id, new_keyword));
	}

	return OpStatus::OK;
}

OP_STATUS MessageEngine::KeywordAdded(Indexer* indexer, message_gid_t message_id, const OpStringC8& keyword)
{
	// Set property on message, to make sure that store knows this message has keywords and can
	// remove them later (see also bug #330335)
	m_store->SetMessageFlag(message_id, Message::HAS_KEYWORDS, TRUE);

	// Find the account for this message
	Account* account = GetAccountById(m_store->GetMessageAccountId(message_id));

	if (!account)
		return OpStatus::ERR_NULL_POINTER;

	return account->KeywordAdded(message_id, keyword);
}

OP_STATUS MessageEngine::KeywordRemoved(Indexer* indexer, message_gid_t message_id, const OpStringC8& keyword)
{
	// Find the account for this message
	Account* account = GetAccountById(m_store->GetMessageAccountId(message_id));

	if (!account)
		return OpStatus::ERR_NULL_POINTER;

	return account->KeywordRemoved(message_id, keyword);
}

Index* MessageEngine::CreateIndex(index_gid_t parent_id, BOOL visible)
{
	if (
		parent_id == IndexTypes::CATEGORY_LABELS ||
		(parent_id >= IndexTypes::FIRST_FOLDER && parent_id < IndexTypes::LAST_FOLDER) ||
		(parent_id >= IndexTypes::FIRST_ACCOUNT && parent_id < IndexTypes::LAST_ACCOUNT))
	{
		OpAutoPtr<Index> index(OP_NEW(Index, ()));
		if (!index.get())
			return NULL;

		OpString index_name;
		OpStatus::Ignore(g_languageManager->GetString(Str::S_DEFAULT_INDEX_NAME, index_name));

		IndexTypes::Type index_type = IndexTypes::FOLDER_INDEX;

		if (parent_id >= IndexTypes::FIRST_ACCOUNT && parent_id < IndexTypes::LAST_ACCOUNT)
		{
			if (index->AddM2Search() == NULL)
				return NULL;

			Account* account;
			m_account_manager->GetAccountById((UINT16)(parent_id-IndexTypes::FIRST_ACCOUNT), account);

			if (account)
			{
				if (account->GetIncomingProtocol() == AccountTypes::RSS)
				{
					index_type = IndexTypes::NEWSFEED_INDEX;
					index->GetSearch()->SetSearchBody(SearchTypes::FOLDER_PATH);
					RETURN_VALUE_IF_ERROR(index->GetSearch()->SetSearchText(UNI_L("http://example")), NULL);

					index->SetAccountId(parent_id-IndexTypes::FIRST_ACCOUNT);
					RETURN_VALUE_IF_ERROR(g_languageManager->GetString(Str::S_RSS_NEWSFOLDER, index_name), NULL);
				}
				else
				{
					return NULL;
				}
			}
			else
			{
				return NULL;
			}
		}
		else
		{
			// for regular filters, don't set the filtered flag by default
			index->SetHideFromOther(FALSE);
			index->EnableModelFlag(IndexTypes::MODEL_FLAG_SENT);
		}

		RETURN_VALUE_IF_ERROR(index->SetName(index_name.CStr()), NULL);

		index->SetType(index_type);
		RETURN_VALUE_IF_ERROR(m_indexer->NewIndex(index.get()), NULL);

		index->SetParentId(parent_id);
		index->SetVisible(visible != FALSE);
		index->SetSaveToDisk(TRUE);

		m_indexer->SaveAllToFile();

		return index.release();
	}

	return NULL;
}

OP_STATUS MessageEngine::UpdateIndex(index_gid_t id)
{
	return m_indexer->UpdateIndex(id);
}

BOOL MessageEngine::IsIndexMailingList(index_gid_t id)
{
	Index* index = GetIndexById(id);
	if (index)
	{
		IndexSearch* search = index->GetM2Search();
		if (search)
		{
			if (search->GetSearchText().Find("@") == KNotFound)
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

BOOL MessageEngine::IsIndexNewsfeed(index_gid_t id)
{
	if (!m_account_manager->GetRSSAccount(FALSE))
		return FALSE;
	// if there's no mail accounts setup yet, pretend it's a feed index (to get the feed toolbar for labels for instance)
	if (m_account_manager->GetMailNewsAccountCount() == 0)
		return TRUE;
	else
		return m_indexer->GetIndexById(id) ? m_indexer->GetIndexById(id)->GetAccountId() == (UINT32)m_account_manager->GetRSSAccount(FALSE)->GetAccountId() : FALSE;
}

OP_STATUS MessageEngine::ToClipboard(const OpINT32Vector& message_ids, index_gid_t source_index_id, BOOL cut)
{
	Index* clipboard = m_indexer->GetIndexById(IndexTypes::CLIPBOARD);
	if (!clipboard)
		return OpStatus::ERR;

	// Empty existing items on clipboard
	RETURN_IF_ERROR(clipboard->Empty());

	// Mark as copy/cut
	m_clipboard_cut = cut;

	// Save source index
	m_clipboard_source = source_index_id;

	// Add messages to clipboard
	for (unsigned i = 0; i < message_ids.GetCount(); i++)
		RETURN_IF_ERROR(clipboard->NewMessage(message_ids.Get(i)));

	return OpStatus::OK;
}


OP_STATUS MessageEngine::PasteFromClipboard(index_gid_t dest_index_id)
{
	// We only paste to filters, labels, IMAP folders, spam, trash and unread
	// Pasting to filters or IMAP folders is just normal copy/pasting inside indexes
	if (dest_index_id < IndexTypes::LAST_IMPORTANT)
	{
		RETURN_IF_ERROR(PasteFromClipboardToSpecialIndex(dest_index_id));
	}
	else if (IndexTypes::FIRST_ACCOUNT <= dest_index_id && dest_index_id < IndexTypes::LAST_ACCOUNT)
	{
		Account* account = GetAccountById(dest_index_id - IndexTypes::FIRST_ACCOUNT);

		if (account && account->GetIncomingProtocol() == AccountTypes::ARCHIVE)
			RETURN_IF_ERROR(PasteFromClipboardToFolderIndex(dest_index_id));
	}
	else if ((IndexTypes::FIRST_FOLDER <= dest_index_id && dest_index_id < IndexTypes::LAST_FOLDER) ||
			 (IndexTypes::FIRST_IMAP   <= dest_index_id && dest_index_id < IndexTypes::LAST_IMAP)   ||
			 (IndexTypes::FIRST_LABEL  <= dest_index_id && dest_index_id < IndexTypes::LAST_LABEL)  ||
			 (IndexTypes::FIRST_ARCHIVE <= dest_index_id && dest_index_id < IndexTypes::LAST_ARCHIVE))
	{
		RETURN_IF_ERROR(PasteFromClipboardToFolderIndex(dest_index_id));
	}

	m_clipboard_cut = FALSE;

	return OpStatus::OK;
}


OP_STATUS MessageEngine::PasteFromClipboardToSpecialIndex(index_gid_t dest_index_id)
{
	Index* clipboard = m_indexer->GetIndexById(IndexTypes::CLIPBOARD);
	Index* destination_index = m_indexer->GetIndexById(dest_index_id);

	if (!clipboard || !destination_index)
		return OpStatus::ERR;
	
	switch (dest_index_id)
	{
		case IndexTypes::SPAM:
		{
			OpINT32Vector messages;
			RETURN_IF_ERROR(clipboard->GetAllMessages(messages));
			return MarkAsSpam(messages, TRUE);
		}
		case IndexTypes::UNREAD_UI:
		{
			OpINT32Vector messages;
			RETURN_IF_ERROR(clipboard->GetAllMessages(messages));
			return MessagesRead(messages, FALSE);
		}

		case IndexTypes::TRASH:
		{
			OpINT32Vector messages;
			RETURN_IF_ERROR(clipboard->GetAllMessages(messages));
			return RemoveMessages(messages, FALSE);
		}
	}

	return OpStatus::OK;
}


OP_STATUS MessageEngine::PasteFromClipboardToFolderIndex(index_gid_t dest_index_id)
{
	Index*   clipboard    = m_indexer->GetIndexById(IndexTypes::CLIPBOARD);
	Index*   source_index = m_indexer->GetIndexById(m_clipboard_source);
	Index*   dest_index   = m_indexer->GetIndexById(dest_index_id);

	if (!clipboard || !source_index || !dest_index)
		return OpStatus::ERR;

	// Check if we have something to do
	if (clipboard->MessageCount() == 0 || m_clipboard_source == dest_index_id)
		return OpStatus::OK;

	Account* src_account  = GetAccountById(source_index->GetAccountId());
	Account* dst_account  = GetAccountById(dest_index->GetAccountId());

	// Pass on to the account if possible
	if (dst_account)
	{

		if (dst_account != src_account && !EnsureAllBodiesDownloaded(IndexTypes::CLIPBOARD))
			return SimpleDialog::ShowDialog(WINDOW_NAME_MAILBODY_MISSING,
                                             g_application->GetActiveDesktopWindow(),
											 Str::S_MAIL_PANEL_BODY_MISSING,
											 Str::S_MESSAGE_BODY_NOT_DOWNLOADED,
											 Dialog::TYPE_OK,
											 Dialog::IMAGE_INFO);

		OpINT32Vector message_ids;

		for (INT32SetIterator it(clipboard->GetIterator()); it; it++)
		{
			RETURN_IF_ERROR(message_ids.Add(it.GetData()));

			if (dest_index->GetSpecialUseType() == AccountTypes::FOLDER_SENT)
			{
				// move to a sent IMAP folder
				INT64 flags = m_store->GetMessageFlags(it.GetData());
				flags |= (1<<StoreMessage::IS_OUTGOING);
				flags |= (1<<StoreMessage::IS_SENT);
				OpStatus::Ignore(m_store->SetMessageFlags(it.GetData(), flags));
				OpStatus::Ignore(GetIndexById(IndexTypes::SENT)->NewMessage(it.GetData()));
			}

			if (m_clipboard_cut)
			{
				switch (source_index->GetSpecialUseType())
				{
					case AccountTypes::FOLDER_SENT:
					{
						// move from an IMAP sent folder
						INT64 flags = m_store->GetMessageFlags(it.GetData());
						flags &= ~(1<<StoreMessage::IS_OUTGOING);
						flags &= ~(1<<StoreMessage::IS_SENT);
						OpStatus::Ignore(m_store->SetMessageFlags(it.GetData(), flags));
						OpStatus::Ignore(GetIndexById(IndexTypes::SENT)->RemoveMessage(it.GetData()));
						break;
					}
					case AccountTypes::FOLDER_TRASH:
					{
						GetIndexById(IndexTypes::TRASH)->RemoveMessage(it.GetData());
						break;
					}
				}
			}
		}

		if (source_index->GetSpecialUseType() == AccountTypes::FOLDER_SPAM)
		{
			RETURN_IF_ERROR(MarkAsSpam(message_ids, FALSE, FALSE, FALSE));
		}

		switch (dest_index->GetSpecialUseType())
		{			
			case AccountTypes::FOLDER_SPAM:	
			case AccountTypes::FOLDER_TRASH:
			{
				// move to an IMAP trash or spam folder
				OpINT32Vector clipboard_messages, *messages;
				OpAutoINT32HashTable<OpINT32Vector> account_message_ids;

				// only handle the messages for that account, assume the rest is by dropped there by mistake
				RETURN_IF_ERROR(clipboard->GetAllMessages(clipboard_messages));
				RETURN_IF_ERROR(SplitIntoAccounts(clipboard_messages, account_message_ids));
				RETURN_IF_ERROR(account_message_ids.GetData(dest_index->GetAccountId(), &messages));

				if (dest_index->GetSpecialUseType() == AccountTypes::FOLDER_TRASH)
				{
					return RemoveMessages(*messages, GetAccountById(dest_index->GetAccountId()), FALSE);
				}
				else
				{
					return MarkAsSpam(*messages, TRUE);
				}
			}
		}

		if (m_clipboard_cut)
			return dst_account->MoveMessages(message_ids, m_clipboard_source, dest_index_id);
		else
			return dst_account->CopyMessages(message_ids, m_clipboard_source, dest_index_id);
	}
	else
	{
		for (INT32SetIterator it(clipboard->GetIterator()); it; it++)
		{
			// Add message to destination index
			RETURN_IF_ERROR(dest_index->NewMessage(it.GetData()));

			// Remove from source index if we were cutting
			if (m_clipboard_cut)
			{
				if (src_account)
					RETURN_IF_ERROR(src_account->RemoveMessage(it.GetData(), TRUE));
				else
					RETURN_IF_ERROR(source_index->RemoveMessage(it.GetData()));
			}
		}
	}

	return OpStatus::OK;
}

index_gid_t MessageEngine::GetClipboardSource()
{
	Index* clipboard = m_indexer->GetIndexById(IndexTypes::CLIPBOARD);

	if (clipboard && clipboard->MessageCount() > 0)
		return m_clipboard_source;

	return 0;
}


/***********************************************************************************
 ** Checks if all items in an index have bodies downloaded
 **
 ** MessageEngine::HasAllBodiesDownloaded
 ** @param index_id Which index to check
 ** @return Whether all bodies are available
 ***********************************************************************************/
BOOL MessageEngine::HasAllBodiesDownloaded(index_gid_t index_id)
{
	Index* index = m_indexer->GetIndexById(index_id);
	if (!index)
		return FALSE;

	for (INT32SetIterator it(index->GetIterator()); it; it++)
	{
		BOOL has_body;
		if (OpStatus::IsError(m_store->HasMessageDownloadedBody(it.GetData(), has_body)))
			return FALSE;

		if (!has_body)
		{
			return FALSE;
		}
	}

	return TRUE;
}

/***********************************************************************************
 ** Ensure that all items in an index have bodies downloaded
 **
 ** MessageEngine::EnsureAllBodiesDownloaded
 ** @param index_id Which index to check
 ** @return Whether all bodies are available, will start body fetch otherwise
 ***********************************************************************************/
BOOL MessageEngine::EnsureAllBodiesDownloaded(index_gid_t index_id)
{
	Index* index = m_indexer->GetIndexById(index_id);
	if (!index)
		return FALSE;

	BOOL all_bodies_downloaded = TRUE;

	for (INT32SetIterator it(index->GetIterator()); it; it++)
	{
		BOOL has_body;
		if (OpStatus::IsError(m_store->HasMessageDownloadedBody(it.GetData(), has_body)))
			return FALSE;

		if (!has_body)
		{
			Message message;
			OpStatus::Ignore(GetMessage(message, it.GetData(), TRUE));
			all_bodies_downloaded = FALSE;
		}
	}

	return all_bodies_downloaded;
}


OP_STATUS MessageEngine::GetLexiconWords(uni_char **result, const uni_char *start_of_word, INT32 &count)
{
	return m_indexer->FindWords(start_of_word, result, &count);
}

UINT32 MessageEngine::GetIndexCount()
{
	return m_indexer ? m_indexer->IndexCount() : 0;
}

Index* MessageEngine::GetIndexById(index_gid_t id)
{
	return m_indexer ? (Index*)(m_indexer->GetIndexById(id)) : NULL;
}

Index* MessageEngine::GetIndexRange(UINT32 range_start, UINT32 range_end, INT32& iterator)
{
	return m_indexer ? (Index*)(m_indexer->GetRange(range_start, range_end, iterator)) : NULL;
}

INT32 MessageEngine::GetUnreadCount()
{
	if (!m_indexer)
		return 0;

	Index* index = m_indexer->GetIndexById(IndexTypes::UNREAD_UI);

	return index ? (INT32)index->UnreadCount() : 0;
}

BOOL MessageEngine::ImportInProgress() const
{
	return (m_mail_import_in_progress > 0);
}

void MessageEngine::SetImportInProgress(BOOL status)
{
	if (status)
	{
		m_mail_import_in_progress++;
	}
	else
	{
		m_mail_import_in_progress--;
	}
}

EngineTypes::ImporterId MessageEngine::GetDefaultImporterId()
{
	return ImportFactory::Instance()->GetDefaultImporterId();
}

OP_STATUS MessageEngine::ImportOPML(const OpStringC& filename)
{
	RETURN_IF_ERROR(InitMessageDatabaseIfNeeded());

	OPMLM2ImportHandler import_handler;
	RETURN_IF_ERROR(import_handler.Init());

	RETURN_IF_ERROR(OPML::Import(filename, import_handler));
	return OpStatus::OK;
}

OP_STATUS MessageEngine::ExportOPML(const OpStringC& filename)
{
	return OPML::Export(filename);
}

OP_STATUS MessageEngine::ExportToMbox(const index_gid_t index_id, const OpStringC& mbox_filename)
{
	OpTreeModel* index_model = NULL;
	INT32 start_pos = -1;

	OP_STATUS res = GetIndexModel(&index_model, index_id, start_pos);
	if (OpStatus::IsError(res))
	{
		return res;
	}

	INT32 count = index_model->GetItemCount();
	if (0 == count)
	{
		return OpStatus::OK;
	}

	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
	OpFile* export_file = glue_factory->CreateOpFile(mbox_filename);
	if (!export_file)
	{
		return OpStatus::ERR;
	}

	res = export_file->Open(OPFILE_WRITE|OPFILE_SHAREDENYREAD|OPFILE_SHAREDENYWRITE);
	if (OpStatus::IsError(res))
	{
		glue_factory->DeleteOpFile(export_file);
		return res;
	}

	BOOL first = TRUE;
	BOOL ended_in_linefeed = TRUE;

	for (INT32 pos = 0; pos < count; pos++)
	{
		OpTreeModelItem* item =	index_model->GetItemByPosition(pos);
		message_gid_t id = item->GetID();

		Message message;
		if (OpStatus::IsError(GetMessage(message, id, TRUE)))
		{
			// this is normal, the message just doesn't exist anymore
			continue;
		}

		OpString8 raw;
		message.GetRawMessage(raw, TRUE, TRUE, TRUE);

		int raw_length = raw.Length();

		if (raw_length > 0)
		{
			OpString8 from_line;
			if (first)
			{
				from_line.Set("From ");
				first = FALSE;
			}
			else
			{
				from_line.Set("\nFrom ");
				if (!ended_in_linefeed)
					from_line.Insert(0, "\n");
			}

			Header* from_header = message.GetHeader(Header::FROM);
			if (from_header)
			{
				const Header::From* from = from_header->GetFirstAddress();
				if (from)
				{
					OpString8 imaa_address;
					RETURN_IF_ERROR(from->GetIMAAAddress(imaa_address));
					RETURN_IF_ERROR(from_line.Append(imaa_address));
				}
				else
				{
					RETURN_IF_ERROR(from_line.Append("?@?"));
				}
			}
			from_line.Append(" ");

			time_t recv_time;
			message.GetDate(recv_time);
			if (recv_time)
			{
				struct tm* gmt = op_gmtime(&recv_time);
				if (gmt)
				{
					RETURN_IF_ERROR(from_line.Append(asctime(gmt),24));
				}
			}
			RETURN_IF_ERROR(from_line.Append("\r\n"));

			if (export_file->Write(from_line.CStr(), from_line.Length())!=OpStatus::OK)
			{
				export_file->Close();
				glue_factory->DeleteOpFile(export_file);
				return OpStatus::ERR;
			}

			if (export_file->Write(raw.CStr(), raw_length)!=OpStatus::OK)
			{
				export_file->Close();
				glue_factory->DeleteOpFile(export_file);
				return OpStatus::ERR;
			}

			ended_in_linefeed = (raw_length>0 && (*(raw.CStr()+raw_length-1)=='\n' || *(raw.CStr()+raw_length-1)=='\r'));
		}
	}

	export_file->Close();
	glue_factory->DeleteOpFile(export_file);

	return OpStatus::OK;
}

OP_STATUS MessageEngine::LoadRSSFeed(const OpStringC& url, URL& downloaded_url, BOOL manual_create, BOOL open_panel)
{
	if (url.IsEmpty() && downloaded_url.IsEmpty())
		return OpStatus::ERR;

	OpString real_url;
	if (url.IsEmpty())
		RETURN_IF_ERROR(downloaded_url.GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI, real_url, TRUE));

	RETURN_IF_ERROR(InitMessageDatabaseIfNeeded());

	// assuming RSS for now:
	Account* account = m_account_manager->GetFirstAccountWithType(AccountTypes::RSS);
	if (account)
	{
		RSSBackend* backend = static_cast<RSSBackend*>(account->GetIncomingBackend());
		if (!backend)
			return OpStatus::ERR;

        if (real_url.HasContent())
            RETURN_IF_ERROR(backend->UpdateFeed(real_url, manual_create));
        else
            RETURN_IF_ERROR(backend->UpdateFeed(url, manual_create));
	}

	// signal that we have an RSS account available
	g_application->SettingsChanged(SETTINGS_ACCOUNT_SELECTOR);
	g_application->SettingsChanged(SETTINGS_MENU_SETUP);
	g_application->SettingsChanged(SETTINGS_TOOLBAR_CONTENTS);

	g_hotlist_manager->ShowPanelByName(UNI_L("Mail"), TRUE, open_panel);

	return OpStatus::OK;
}

OP_STATUS MessageEngine::MailCommand(URL& url)
{
	m_command_url = url;
	return m_loop->Post(MSG_M2_MAILCOMMAND);
}

// ---------------------------------------------------------------------------

OP_STATUS MessageEngine::ConvertToBestChar8(IO OpString8& preferred_charset, BOOL force_charset, IN const OpStringC16& source, OUT OpString8& dest, int max_dst_length, int* converted)
{
    dest.Empty();

    OP_STATUS ret;
    if (preferred_charset.IsEmpty())
    {
        if ((ret=preferred_charset.Set("utf-8")) != OpStatus::OK)
            return ret;
    }

    int invalid_count=0;
	OpString dummy_invalid_chars;
    if ((ret=ConvertToChar8(preferred_charset, source, dest, invalid_count, dummy_invalid_chars, max_dst_length, converted))!=OpStatus::OK &&
         ret!=OpStatus::ERR_OUT_OF_RANGE) //ConvertToChar8 will return ERR_OUT_OF_RANGE if an invalid charset is given
        return ret;

    if (ret==OpStatus::ERR_OUT_OF_RANGE && preferred_charset.CompareI("utf-8")==0) //If we have a legal charset, the error is a real OOM
        return ret;

    if ((ret==OpStatus::ERR_OUT_OF_RANGE || invalid_count>0) && !force_charset && preferred_charset.CompareI("utf-8")!=0)
    {
        preferred_charset.Empty(); //Will be set to utf-8 when entering this function again in the recursive call
        return ConvertToBestChar8(preferred_charset, force_charset, source, dest, max_dst_length, converted);
    }

    return OpStatus::OK;
}


OP_STATUS MessageEngine::ConvertToChar8(IO OpString8& charset, IN const OpStringC16& source, OUT OpString8& dest,
                                        OUT int& invalid_count, OUT OpString16& invalid_chars, int max_dst_length, int* converted)
{
	OutputConverter* conv_temp = 0;
	if (OpStatus::IsError(OutputConverter::CreateCharConverter(charset.CStr(), &conv_temp)))
    {
		RETURN_IF_ERROR(OutputConverter::CreateCharConverter("iso-8859-1", &conv_temp));
    }
	OpAutoPtr<OutputConverter> conv (conv_temp);

	RETURN_IF_ERROR(charset.Set(conv->GetDestinationCharacterSet()));

	OP_STATUS ret = ConvertToChar8(conv.get(), source, dest, max_dst_length, converted);

	invalid_count = conv->GetNumberOfInvalid();
#ifdef ENCODINGS_CAP_SPECIFY_INVALID
	OpStatus::Ignore(invalid_chars.Set(conv->GetInvalidCharacters()));
#endif

	return ret;
}

OP_STATUS MessageEngine::ConvertToChar8(OutputConverter* converter, IN const OpStringC16& source, OUT OpString8& dest, int max_dst_length, int* converted)
{
	OP_STATUS ret;
    dest.Empty();

	if (converted)
	{
		*converted = 0;
	}

    OpString8 temp_buffer;
    if (!temp_buffer.Reserve(max(2*source.Length(), max_dst_length)+9+1))
        return OpStatus::ERR_NO_MEMORY;

	//Buffer needs to be terminated to avoid problems in OpString::Append
	if (temp_buffer.Capacity())
	{
		*(temp_buffer.CStr()+temp_buffer.Capacity()-1) = 0;
	}

    int read = 0;
    char* src = (char*)source.CStr();
    int src_len = UNICODE_SIZE(source.Length());
	int dest_len = temp_buffer.Capacity()-1;

	if (max_dst_length > -1 && max_dst_length < dest_len)
		dest_len = max_dst_length;

    while (src_len>0 && dest_len>0)
    {
        int l = converter->Convert(src, (dest_len>9 ? src_len : 2), (char*)temp_buffer.CStr(), (max_dst_length==-1 ? dest_len : (dest_len>9 ? dest_len-9 : dest_len)), &read); //9 is the worst-case longest self-contained sequence for a character
		temp_buffer.CStr()[l] = 0;
        if ((ret=dest.Append(temp_buffer.CStr(), l)) != OpStatus::OK)
            return ret;

		if (converted)
		{
			*converted += UNICODE_DOWNSIZE(read);
		}

		if (max_dst_length!=-1)
		{
			dest_len -= l;
			if (converter->LongestSelfContainedSequenceForCharacter()>dest_len) //We might not have room for the next char
				break;
		}

        if (!read) //We have data in the buffer that will not be converted. For now, we bail out with an OK
            break;

        src += read;
        src_len -= read;
    }
	int len=converter->ReturnToInitialState(NULL);
	if (len)
	{
		converter->ReturnToInitialState(temp_buffer.CStr());
		temp_buffer.CStr()[len] = 0;
        if ((ret=dest.Append(temp_buffer.CStr(), len)) != OpStatus::OK)
            return ret;
	}

    return OpStatus::OK;
}


OP_STATUS MessageEngine::MailCommandHTTP(URL& url)
{
	NewsfeedSubscribeDialog* dialog = OP_NEW(NewsfeedSubscribeDialog, ());
	if (!dialog)
		return OpStatus::ERR_NO_MEMORY;

	return dialog->Init(g_application->GetActiveDesktopWindow(), url);
}


// ***************************************************************************
//
//	IRCURLNewAccountListener; class used by the function below.
//
// ***************************************************************************
#ifdef IRC_SUPPORT
class IRCURLNewAccountListener
:	public DialogListener
{
public:
	// Construction.
	IRCURLNewAccountListener(AccountManager* account_manager, OpAutoPtr<IRCURLParser> irc_url_parser)
	:	m_account_manager(account_manager),
		m_irc_url_parser(irc_url_parser)
	{
		OP_ASSERT(m_account_manager != 0);
	}

private:
	// DialogListener.
	virtual void OnOk(Dialog* dialog, UINT32 result)
	{
		// Look up the account.
		Account* account = 0;
		m_account_manager->GetChatAccountByServer(account, m_irc_url_parser->Location());

		if (account == 0)
			return;

		// Join the channels we should join.
		const UINT channel_count = m_irc_url_parser->ChannelCount();
		if (channel_count > 0)
		{
			OpString channel;
			OpString key;

			for (UINT index = 0; index < channel_count; ++index)
			{
				OpStatus::Ignore(m_irc_url_parser->Channel(index, channel, key));

				if (channel.HasContent())
					account->JoinChatRoom(channel, key);
			}
		}
		else
			account->SetChatStatus(AccountTypes::ONLINE);

		// Engage in the private conversations we should.
		{
			OpString target;
			for (UINT index = 0, query_count = m_irc_url_parser->QueryCount();
				index < query_count; ++index)
			{
				OpStatus::Ignore(m_irc_url_parser->Query(index, target));

				if (target.HasContent())
				{
					ChatInfo chat_info(target);
					g_application->GoToChat(account->GetAccountId(), chat_info,
						FALSE, FALSE, account->GetOpenPrivateChatsInBackground());
				}
			}
		}
	}

	virtual void OnClose(Dialog* dialog) { OP_DELETE(this); }

	// Members.
	AccountManager* m_account_manager;
	OpAutoPtr<IRCURLParser> m_irc_url_parser;
};
// ***************************************************************************

OP_STATUS MessageEngine::MailCommandIRC(URL& url)
{
	OpAutoPtr<IRCURLParser> irc_url_parser(OP_NEW(IRCURLParser, ()));
	if (irc_url_parser.get() == 0)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(irc_url_parser->Init(url));

	// See if we allready have an account the the server specified by the
	// IRC URL.
	Account* account = 0;
	RETURN_IF_ERROR(m_account_manager->GetChatAccountByServer(account,
		irc_url_parser->Location()));

	if (account == 0)
	{
		// Fetch the default irc account.
		Account* irc_account = 0;

		const UINT16 default_irc_account_id = m_account_manager->GetDefaultAccountId(AccountTypes::TYPE_CATEGORY_CHAT);
		if (default_irc_account_id != 0)
		{
			OpStatus::Ignore(m_account_manager->GetAccountById(
				default_irc_account_id, irc_account));
		}

		// If the default account doesn't exist, we fetch the first
		// available IRC account.
		if (irc_account == 0)
			irc_account = m_account_manager->GetFirstAccountWithType(AccountTypes::IRC);

		// We have a default IRC account we can use some settings from.
		if (irc_account != 0 && irc_url_parser->HasLocation())
		{
			// Get the nickname to use.
			OpString nickname;

			if (irc_url_parser->HasNickname())
				RETURN_IF_ERROR(nickname.Set(irc_url_parser->Nickname()));
			else
				RETURN_IF_ERROR(nickname.Set(irc_account->GetIncomingUsername()));

			// Create a new temporary account.
			RETURN_IF_ERROR(m_account_manager->AddTemporaryAccount(account,
				AccountTypes::IRC, irc_url_parser->Location(), irc_url_parser->Port(), FALSE,
				nickname, UNI_L("")));

			// Set some properties from the default account.
			OpString realname;
			OpStatus::Ignore(irc_account->GetRealname(realname));
			OpStatus::Ignore(account->SetRealname(realname));

			OpString email;
			OpStatus::Ignore(irc_account->GetEmail(email));
			OpStatus::Ignore(account->SetEmail(email));
		}
		else if (irc_account == 0)
		{
			// Bring up the account wizard.
			NewAccountWizard* account_wizard = OP_NEW(NewAccountWizard, ());
			if (account_wizard == 0)
				return OpStatus::ERR_NO_MEMORY;

			// Set up a listener class to perform a join / connection when
			// the wizard has finished.
			OpString location;
			OpStatus::Ignore(location.Set(irc_url_parser->Location()));

			IRCURLNewAccountListener* listener = OP_NEW(IRCURLNewAccountListener, (m_account_manager, irc_url_parser));
			if (listener == 0)
				return OpStatus::ERR;

			account_wizard->SetDialogListener(listener);
			account_wizard->Init(AccountTypes::IRC,
				g_application->GetActiveDesktopWindow(), location);
		}
		else if (irc_account != 0)
		{
			// If we get here, it means that the parsed host is empty. Just
			// use the current account then.
			account = irc_account;
		}
	}

	if (account == 0)
		return OpStatus::OK;

	OP_ASSERT(irc_url_parser.get() != 0);

	// Join the desired chat rooms if some are specified.
	const UINT channel_count = irc_url_parser->ChannelCount();
	if (channel_count > 0)
	{
		for (UINT index = 0; index < channel_count; ++index)
		{
			OpString channel;
			OpString key;
			OpStatus::Ignore(irc_url_parser->Channel(index, channel, key));

			if (channel.HasContent())
			{
				RETURN_IF_ERROR(account->JoinChatRoom(channel, key));

				// See if we allready are in this room, if so - focus it.
				const OpStringC& room_name = (channel[0] == '#' ? channel.SubString(1) : channel);
				if (m_chatrooms_model->GetRoomItem(account->GetAccountId(), room_name) != 0)
				{
					ChatInfo chat_info(room_name);
					g_application->GoToChat(account->GetAccountId(), chat_info, TRUE, FALSE);
				}
			}
		}
	}
	else
	{
		// No room was specified, just go online.
		RETURN_IF_ERROR(account->SetChatStatus(AccountTypes::ONLINE));
	}

	// Engage in the private conversations we should.
	{
		OpString target;
		for (UINT index = 0, query_count = irc_url_parser->QueryCount();
			index < query_count; ++index)
		{
			OpStatus::Ignore(irc_url_parser->Query(index, target));
			if (target.HasContent())
			{
				ChatInfo chat_info(target);
				g_application->GoToChat(account->GetAccountId(), chat_info,
					FALSE, FALSE, account->GetOpenPrivateChatsInBackground());
			}
		}
	}

	return OpStatus::OK;
}

OP_STATUS MessageEngine::MailCommandChatTransfer(URL& url)
{
	// The format of a dcc url will be "chattransfer:///accountid=%u&port=%u&transferid=%u".
	OpString path;
	RETURN_IF_ERROR(url.GetAttribute(URL::KUniPathAndQuery_Escaped, path));
	if (path.IsEmpty())
		return OpStatus::ERR;

	OpString parameters;
	if (path[0] == '/')
		RETURN_IF_ERROR(parameters.Set(path.CStr() + 1));
	else
		RETURN_IF_ERROR(parameters.Set(path));

	UINT account_id = 0;
	UINT port = 0;
	UINT transfer_id = 0;

	StringTokenizer parameter_tokenizer(parameters, UNI_L("&"));
	while (parameter_tokenizer.HasMoreTokens())
	{
		OpString parameter;
		RETURN_IF_ERROR(parameter_tokenizer.NextToken(parameter));

		if (parameter.HasContent())
		{
			if (parameter.CompareI("accountid=", 10) == 0)
				account_id = uni_atoi(parameter.CStr() + 10);
			else if (parameter.CompareI("port=", 5) == 0)
				port = uni_atoi(parameter.CStr() + 5);
			else if (parameter.CompareI("transferid=", 11) == 0)
				transfer_id = uni_atoi(parameter.CStr() + 11);
		}
	}

	// Get the account associated with the parsed account id and notify it.
	Account* account = 0;
	OpStatus::Ignore(m_account_manager->GetAccountById(account_id, account));

	if (account != 0)
		OpStatus::Ignore(account->AcceptReceiveRequest(port, transfer_id));

	return OpStatus::OK;
}
#endif // IRC_SUPPORT

OP_STATUS MessageEngine::MailCommandNews(URL& url)
{
	OP_STATUS ret;

    Account* account = NULL;
    OpString param_str;
    OpString accesspoint_path;

	OpString username, servername, password;
	if ((ret=username.SetFromUTF8(url.GetAttribute(URL::KUserName).CStr()))!=OpStatus::OK ||
		(ret=servername.Set(url.GetAttribute(URL::KHostName)))!=OpStatus::OK ||
		(ret=password.Set(url.GetAttribute(URL::KPassWord)))!=OpStatus::OK) //Password is from the URL, and shouldn't need to be wiped
	{
		return ret;
	}

	BOOL bracket_messageID = username.HasContent() && *(username.CStr())=='<' && servername.HasContent() && *(servername.CStr()+servername.Length()-1)=='>';
	BOOL nonbracket_messageID = username.HasContent() && password.IsEmpty();

    if (bracket_messageID || nonbracket_messageID) // news://<messageID> or news://messageID ?
    {
		RETURN_IF_ERROR(url.GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI, param_str, TRUE));

		int protocol_length = ((URLType)url.GetAttribute(URL::KType) == URL_SNEWS) ? 8 : 7; //'snews://' or 'news://'
        if (param_str.Length() > protocol_length)
            param_str.Delete(0, protocol_length); //Skip 'snews://' or 'news://'

        if ((ret=m_account_manager->GetAccountByProperties(account, AccountTypes::NEWS, NULL, 0,
                                                           ((URLType)url.GetAttribute(URL::KType) == URL_SNEWS), NULL)) != OpStatus::OK)
        {
            return ret;
        }

        uni_char* last_char = param_str.CStr()+param_str.Length()-1;
        if (*last_char == '/')
            *last_char = 0;

        if ((ret=accesspoint_path.Set(param_str)) != OpStatus::OK)
            return ret;
    }
    else
    {
        UINT16 port = url.GetAttribute(URL::KServerPort);
        if (port==0) port = ((URLType)url.GetAttribute(URL::KType) == URL_NEWS) ? 119 : 563;
        if ((ret=m_account_manager->GetAccountByProperties(account, AccountTypes::NEWS, servername, port,
                                                           ((URLType)url.GetAttribute(URL::KType) == URL_SNEWS), username)) != OpStatus::OK)
        {
			account = NULL;
        }

        //No matching account defined. Add it as temporary account.
        if (account==NULL)
		{
			if (servername.HasContent())
        	{
				OpString password;

				RETURN_IF_ERROR(password.Set(url.GetAttribute(URL::KPassWord)));

				RETURN_IF_ERROR(InitMessageDatabaseIfNeeded());

				if ((ret=m_account_manager->AddTemporaryAccount(account, AccountTypes::NEWS, servername, port, ((URLType)url.GetAttribute(URL::KType) == URL_SNEWS),
																username, password)) != OpStatus::OK)
				{
					return ret;
				}

				if (!account)
					return OpStatus::ERR_NULL_POINTER;

                // This could have been the first account created ever, update the UI
                g_application->SettingsChanged(SETTINGS_ACCOUNT_SELECTOR);
                g_application->SettingsChanged(SETTINGS_MENU_SETUP);
                g_application->SettingsChanged(SETTINGS_TOOLBAR_CONTENTS);
                g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_PANEL, 0, UNI_L("Mail"), g_application->GetActiveDesktopWindow());
			}
			else
			{
				// Show account setup wizard
				g_application->ShowAccountNeededDialog(AccountTypes::NEWS);
				return OpStatus::OK;
			}
        }

		OpString parameter_string;
		RETURN_IF_ERROR(url.GetAttribute(URL::KUniPathAndQuery_Escaped, parameter_string));
		const uni_char* parameters = parameter_string.CStr();
		if (parameters)
		{
			while (*parameters=='/') parameters++;
			if ((ret=param_str.Set(parameters)) != OpStatus::OK)
				return ret;

			const uni_char* accesspoint_separator = parameters;
			while (*accesspoint_separator && *accesspoint_separator != '/') accesspoint_separator++;
			if ((ret=accesspoint_path.Set(parameters, accesspoint_separator ? accesspoint_separator-parameters : (int)KAll)) != OpStatus::OK)
				return ret;
		}
    }

    OpString accesspoint_name;
    BOOL is_msgid = (accesspoint_path.FindFirstOf('@') != KNotFound);
    if (is_msgid) //We don't know the access-point when only given Message-id
    {
        if ( ((ret=accesspoint_name.Set(UNI_L("news://"))) != OpStatus::OK) ||
            ((ret=accesspoint_name.Append(accesspoint_path)) != OpStatus::OK) )
        {
            return ret;
        }
    }
    else
    {
        if ((ret=accesspoint_name.Set(accesspoint_path)) != OpStatus::OK)
            return ret;
    }

    if (!account)
        return OpStatus::ERR_NULL_POINTER;

    if (accesspoint_path.IsEmpty()) //Link was probably <URL:news://news.server.domain/>
    {
        GetGlueFactory()->GetBrowserUtils()->SubscribeDialog(account->GetAccountId());
    }
    else
    {
        //Show (and Add, if needed) index
		Index* index = m_indexer->GetSubscribedFolderIndex(account, accesspoint_path, 0, accesspoint_name, TRUE, TRUE);
		if (index)
        {
            GetGlueFactory()->GetBrowserUtils()->ShowIndex(index->GetId(), FALSE);


            if (is_msgid) //If we already have the message, no need to go asking for it again
			{
				if (param_str.HasContent())
				{
					UINT32 message_number = 0;
					OpString8 message_id;

					RETURN_IF_ERROR(message_id.Set(param_str.CStr()));
					message_number = m_store->GetMessageByMessageId(message_id);

					if (message_number!=0 && index->NewMessage(message_number)==OpStatus::OK)
					{
						return OpStatus::OK; //We have added it to the index. We are done.
					}
				}
			}
			else //Make sure newsgroup is subscribed
			{
                OpStatus::Ignore(account->AddSubscribedFolder(accesspoint_path));
			}

			OpString8 param_str8;

			RETURN_IF_ERROR(param_str8.Set(param_str.CStr()));
			return ((NntpBackend*)((Account*)account)->GetIncomingBackend())->FetchMessage(param_str8);
        }
    }

	return OpStatus::OK;
}

OP_STATUS MessageEngine::SplitIntoAccounts(const OpINT32Vector& message_ids, OpINT32HashTable<OpINT32Vector>& hash_table)
{
	OpINT32Vector *temp_vector;

	// Split messages in different accounts
	for(UINT32 index = 0; index < message_ids.GetCount(); index++)
	{
		UINT16 account_id = m_store->GetMessageAccountId(message_ids.Get(index));

		if(hash_table.GetData(account_id, &temp_vector) != OpStatus::OK)
		{
			temp_vector = OP_NEW(OpINT32Vector, ());
			if (!temp_vector)
				return OpStatus::ERR_NO_MEMORY;

			RETURN_IF_ERROR(hash_table.Add(account_id, temp_vector));
		}

		RETURN_IF_ERROR(temp_vector->Add(message_ids.Get(index)));
	}

	return OpStatus::OK;
}

#ifdef M2_MAPI_SUPPORT
void MessageEngine::CheckDefaultMailClient()
{
	if (!m_check_default_mailer)
		return;
	m_check_default_mailer = FALSE;
	Account* account = g_m2_engine->GetAccountManager()->GetFirstAccountWithCategory(AccountTypes::TYPE_CATEGORY_MAIL);
	if (!account)
		return;

	if ((g_application->DetermineFirstRunType() == Application::RUNTYPE_FIRST ||
		g_application->DetermineFirstRunType() == Application::RUNTYPE_FIRSTCLEAN||
		g_pcm2->GetIntegerPref(PrefsCollectionM2::ShowDefaultMailClientDialog)) && 
		g_desktop_mail_client_utils->IsOperaAbleToBecomeDefaultMailClient())
	{
		ShowDialog(OP_NEW(DefaultMailClientDialog,()), g_global_ui_context, g_application->GetActiveMailDesktopWindow());
	}
	// Fix for DSK-362042 External mailto-links don't open compose window when Opera is already running
	else if ((g_application->DetermineFirstRunType() == Application::RUNTYPE_FIRSTCLEAN||
		g_application->DetermineFirstRunType() == Application::RUNTYPE_FIRST_NEW_BUILD_NUMBER) &&
		g_desktop_mail_client_utils->IsOperaDefaultMailClient())
	{
		g_desktop_mail_client_utils->SetOperaAsDefaultMailClient();
	}
}
#endif //M2_MAPI_SUPPORT

// ---------------------------------------------------------------------------------

MessageEngine::MultipleMessageChanger::MultipleMessageChanger(unsigned tochange)
	: m_docount(g_m2_engine && (tochange == 0 || tochange > 5))
{
	if (m_docount)
		g_m2_engine->m_multiple_message_changes++;
}


MessageEngine::MultipleMessageChanger::~MultipleMessageChanger()
{
	if (m_docount)
	{
		g_m2_engine->m_multiple_message_changes--;
		if (g_m2_engine->m_multiple_message_changes == 0)
		{
			g_m2_engine->OnMessageChanged(UINT_MAX);
			g_m2_engine->OnIndexChanged(UINT_MAX);
		}
	}
}

#endif //M2_SUPPORT
