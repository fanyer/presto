// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#include "core/pch.h"

#ifdef IRC_SUPPORT

#include "adjunct/m2/src/backend/irc/chat-filetransfer.h"

#include "adjunct/desktop_pi/desktop_file_chooser.h"
#include "adjunct/desktop_util/adt/hashiterator.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/url/url2.h"
#include "modules/windowcommander/OpTransferManager.h"

#include "adjunct/m2/src/backend/irc/irc-protocol.h"

//********************************************************************
//
//	ChatFileTransferManager
//
//********************************************************************

ChatFileTransferManager::ChatFileTransferManager()
	: m_is_transfer_manager_listener(FALSE)
	, m_chooser(0)
{
}


ChatFileTransferManager::~ChatFileTransferManager()
{
	if (m_is_transfer_manager_listener)
		g_transferManager->RemoveTransferManagerListener(this);

	OP_DELETE(m_chooser);
}

INT32 ChatFileTransferManager::m_next_transfer_id = 0;

OP_STATUS ChatFileTransferManager::FileSendInitializing(ChatFileTransfer& chat_transfer)
{
	// Create a new transfer item for this file send.
	return AddTransferItem(chat_transfer);
}

OP_STATUS
ChatFileTransferManager::FileSendBegin(const ChatFileTransfer& chat_transfer)
{
	OpTransferItem* item = GetTransferItem(chat_transfer);
	if (item == 0)
		return OpStatus::ERR;

	TransferItem* transfer_item = (TransferItem *)(item);
	OP_ASSERT(transfer_item != 0);

	// Update status.
	return transfer_item->GetURL()->SetAttribute(URL::KLoadStatus, URL_LOADING);
}

OP_STATUS
ChatFileTransferManager::FileSendProgress(const ChatFileTransfer& chat_transfer, UINT32 bytes_sent)
{
	OpTransferItem* item = GetTransferItem(chat_transfer);
	if (item == 0)
		return OpStatus::ERR;

	TransferItem* transfer_item = (TransferItem *)(item);
	OP_ASSERT(transfer_item != 0);

	// If the user presses stop, the URL is not loading anymore.
	if (transfer_item->GetURL()->GetAttribute(URL::KLoadStatus, TRUE) != URL_LOADING)
		return OpStatus::ERR;

	transfer_item->SetCurrentSize(OpFileLength(bytes_sent));
	return OpStatus::OK;
}

OP_STATUS
ChatFileTransferManager::FileSendCompleted(const ChatFileTransfer& chat_transfer)
{
	OpTransferItem* item = GetTransferItem(chat_transfer);
	if (item == 0)
		return OpStatus::ERR;

	TransferItem* transfer_item = (TransferItem *)(item);
	OP_ASSERT(transfer_item != 0);

	return transfer_item->GetURL()->SetAttribute(URL::KLoadStatus, URL_LOADED);
}

OP_STATUS
ChatFileTransferManager::FileSendFailed(const ChatFileTransfer& chat_transfer)
{
	OpTransferItem* item = GetTransferItem(chat_transfer);
	if (item == 0)
		return OpStatus::ERR;

	TransferItem* transfer_item = (TransferItem *)(item);
	OP_ASSERT(transfer_item != 0);

	return transfer_item->GetURL()->SetAttribute(URL::KLoadStatus, URL_LOADING_FAILURE);
}

OP_STATUS
ChatFileTransferManager::FileReceiveInitializing(ChatFileTransfer& chat_transfer,
												 FileReceiveInitListener * listener)
{
	// Create a new transfer item for this file receive.
	listener->SetChatInfo(&chat_transfer, this);
	return AddTransferItem(chat_transfer, listener);
}

OP_STATUS
ChatFileTransferManager::FileReceiveBegin(const ChatFileTransfer& chat_transfer)
{
	OpTransferItem* item = GetTransferItem(chat_transfer);
	if (item == 0)
		return OpStatus::ERR;

	TransferItem* transfer_item = (TransferItem *)(item);
	OP_ASSERT(transfer_item != 0);

	// Update the status to loading.
	return transfer_item->GetURL()->SetAttribute(URL::KLoadStatus, URL_LOADING);
}

OP_STATUS
ChatFileTransferManager::FileReceiveProgress(	const ChatFileTransfer& chat_transfer,
												unsigned char* 			received,
												UINT32					bytes_received)
{
	OpTransferItem* item = GetTransferItem(chat_transfer);
	if (item == 0)
		return OpStatus::ERR;

	TransferItem* transfer_item = (TransferItem *)(item);
	OP_ASSERT(transfer_item != 0);

	// If the user have pressed stop, the status won't be loading anymore.
	if (transfer_item->GetURL()->Status(TRUE) != URL_LOADING)
		return OpStatus::ERR;

	transfer_item->PutURLData((const char *)(received), bytes_received);
	return OpStatus::OK;
}

OP_STATUS
ChatFileTransferManager::FileReceiveCompleted(const ChatFileTransfer& chat_transfer)
{
	OpTransferItem* item = GetTransferItem(chat_transfer);
	if (item == 0)
		return OpStatus::ERR;

	TransferItem* transfer_item = (TransferItem *)(item);
	OP_ASSERT(transfer_item != 0);

	transfer_item->FinishedURLPutData();
	 return transfer_item->GetURL()->SetAttribute(URL::KLoadStatus, URL_LOADED);
}

OP_STATUS
ChatFileTransferManager::FileReceiveFailed(const ChatFileTransfer& chat_transfer)
{
	OpTransferItem* item = GetTransferItem(chat_transfer);
	if (item == 0)
		return OpStatus::ERR;

	TransferItem* transfer_item = (TransferItem *)(item);
	OP_ASSERT(transfer_item != 0);

	return transfer_item->GetURL()->SetAttribute(URL::KLoadStatus, URL_LOADING_FAILURE);
}

BOOL
ChatFileTransferManager::OnTransferItemAdded(	OpTransferItem* transfer_item,
												BOOL			is_populating,
												double			last_speed)
{
	return FALSE;
}

void
ChatFileTransferManager::OnTransferItemRemoved(OpTransferItem* transfer_item)
{
	// Remove this item from our hash of transfer items.
	const INT32 id = GetTransferId(transfer_item);
	if (id != -1)
	{
		OpTransferItem* current_item = 0;
		m_transfer_items.Remove(id, &current_item);
	}

	// Remove ourself as listener in case we have no more transfers.
	if (m_transfer_items.GetCount() == 0)
	{
		g_transferManager->RemoveTransferManagerListener(this);
		m_is_transfer_manager_listener = FALSE;
	}
}

OP_STATUS
ChatFileTransferManager::AddTransferItem(	ChatFileTransfer&	chat_transfer)
{
	OP_ASSERT(GetTransferItem(chat_transfer) == 0);

	OpString file_name;

	RETURN_IF_ERROR(chat_transfer.GetFileName(file_name));

	// Figure out where to save the file if we are receiving the file.
	OpFileLength dummy_resume;
	return AddTransferItem(chat_transfer, SEND, file_name, dummy_resume);
}

OP_STATUS
ChatFileTransferManager::AddTransferItem(	ChatFileTransfer&	chat_transfer,
											FileReceiveInitListener * listener)
{
	OP_ASSERT(GetTransferItem(chat_transfer) == 0);

	OpString file_name;

	RETURN_IF_ERROR(chat_transfer.GetFileName(file_name));

	// Figure out where to save the file if we are receiving the file.
	DesktopFileChooserRequest& request = listener->GetRequest();
	request.action = DesktopFileChooserRequest::ACTION_FILE_SAVE;
	OpString tmp_storage;
	request.initial_path.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_OPEN_FOLDER, tmp_storage));

	request.initial_path.Append(UNI_L(PATHSEP));
	request.initial_path.Append(file_name.CStr());
    request.initial_filter = -1;
    request.fixed_filter = FALSE;
	g_languageManager->GetString(Str::S_CHAT_RECEIVE_FILE_TITLE, request.caption);
	OpWindow* parent_window = g_application->GetActiveDesktopWindow()->GetOpWindow();

	if (!m_chooser)
		RETURN_IF_ERROR(DesktopFileChooser::Create(&m_chooser));

	return m_chooser->Execute(parent_window, listener, request);
}

OP_STATUS
ChatFileTransferManager::AddTransferItem(	ChatFileTransfer&	chat_transfer,
											TransferMethod		transfer_method,
											const OpString&		file_name,
											OpFileLength&		resume_position)
{
	OP_ASSERT(GetTransferItem(chat_transfer) == 0);

	OpString file_path_and_name;
	RETURN_IF_ERROR(file_path_and_name.Set(file_name));

	// Fetch some information about the transfer.
	OpString other_party;
	OpString protocol_name;
	RETURN_IF_ERROR(chat_transfer.GetOtherParty(other_party));
	RETURN_IF_ERROR(chat_transfer.GetProtocolName(protocol_name));

	// Create a somewhat unique url name.
	OpString unique_url;
	unique_url.AppendFormat(UNI_L("%s://%s:%s"),
		protocol_name.CStr(), other_party.CStr(), file_path_and_name.CStr());

	URL url = g_url_api->GetURL(unique_url.CStr(), NULL, TRUE);

	// Create a transfer item and initialize it.
	OpTransferItem* op_transfer_item = 0;

	OpString url_with_rel;
	RETURN_IF_ERROR(url.GetAttribute(URL::KUniName_With_Fragment_Escaped, url_with_rel));

	BOOL already_created = FALSE;
	RETURN_IF_ERROR(g_transferManager->GetTransferItem(&op_transfer_item,
		url_with_rel.CStr(), &already_created));

	if (!already_created)
	{
		TransferManager* transfer_manager = (TransferManager *)(g_transferManager);
		RETURN_IF_ERROR(transfer_manager->AddTransferItem(url, file_path_and_name.CStr(), OpTransferItem::ACTION_UNKNOWN,
			FALSE, 0, transfer_method == SEND ? TransferItem::TRANSFERTYPE_CHAT_UPLOAD :
			TransferItem::TRANSFERTYPE_CHAT_DOWNLOAD));

		if (file_path_and_name.HasContent())
			RETURN_IF_ERROR(op_transfer_item->SetFileName(file_path_and_name.CStr()));
	}

	// Add item to internal list of transfer items if needed.
	INT32 id = GetTransferId(op_transfer_item);
	if (id == -1)
	{
		id = NewTransferId();
		RETURN_IF_ERROR(m_transfer_items.Add(id, op_transfer_item));
	}

	chat_transfer.SetTransferId(id);

	// Initialize the tranfer item.
	if (transfer_method == SEND)
	{
		TransferItem* transfer_item = (TransferItem *)(op_transfer_item);
		OP_ASSERT(transfer_item != 0);

		transfer_item->SetType(TransferItem::TRANSFERTYPE_CHAT_UPLOAD);
		transfer_item->Clear();
		transfer_item->SetCompleteSize(OpFileLength(chat_transfer.GetFileSize()));

		RETURN_IF_ERROR(transfer_item->GetURL()->SetAttribute(URL::KLoadStatus, URL_LOADING_WAITING));
	}
	else if (transfer_method == RECEIVE)
	{
		TransferItem* transfer_item = (TransferItem *)(op_transfer_item);
		OP_ASSERT(transfer_item != 0);

		transfer_item->SetType(TransferItem::TRANSFERTYPE_CHAT_DOWNLOAD);
		transfer_item->Clear();

		// If the content loaded is equal or higher to the file size, we want
		// to clear the storage and retransfer.
		transfer_item->GetURL()->Unload();
		RETURN_IF_ERROR(transfer_item->GetURL()->LoadToFile(file_path_and_name.CStr()));

		OpFileLength content_loaded = transfer_item->GetURL()->GetContentLoaded();
		const OpFileLength file_size = chat_transfer.GetFileSize();

		if (content_loaded == 0 || content_loaded >= file_size)
		{
			transfer_item->GetURL()->Unload();
		}
		else
		{
			resume_position = content_loaded;
		}

		RETURN_IF_ERROR(transfer_item->GetURL()->SetAttribute(URL::KContentSize, &file_size));
		RETURN_IF_ERROR(transfer_item->GetURL()->SetAttribute(URL::KLoadStatus, URL_LOADING_WAITING));
		RETURN_IF_ERROR(transfer_item->GetURL()->LoadToFile(file_path_and_name.CStr()));
	}

	// Start to listen on the transfer manager if needed.
	if (!m_is_transfer_manager_listener)
	{
		RETURN_IF_ERROR(g_transferManager->AddTransferManagerListener(this));
		m_is_transfer_manager_listener = TRUE;
	}

	return OpStatus::OK;
}


OpTransferItem* ChatFileTransferManager::GetTransferItem(const ChatFileTransfer& chat_transfer)
{
	OpTransferItem* item = 0;
	m_transfer_items.GetData(chat_transfer.TransferId(), &item);

	return item;
}


INT32 ChatFileTransferManager::GetTransferId(OpTransferItem* item) const
{
	ChatFileTransferManager* non_const_this = const_cast<ChatFileTransferManager *>(this);

	// Could probably think about optimizing this linear search in case
	// speed ever becoms an issue here.

	for (INT32HashIterator<OpTransferItem> it (non_const_this->m_transfer_items); it; it++)
	{
		OpTransferItem* current_item = it.GetData();
		OP_ASSERT(current_item != 0);

		if (item == current_item)
		{
			const INT32 key = it.GetKey();
			return key;
		}
	}

	return -1;
}

OP_STATUS ChatFileTransferManager::FileReceiveInitListener::FileReceiveInitDone(OpFileLength resume_position, BOOL initialization_failed)
{
	OP_STATUS sts = protocol->DCCInitReceiveCheckResume(receiver, resume_position, initialization_failed);
	OP_DELETE(this);
	return sts;
}

void ChatFileTransferManager::FileReceiveInitListener::OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
{
	BOOL initialization_failed = TRUE;
	OpFileLength resume_position = 0;
	if (result.files.GetCount() == 1)
	{
		if (OpStatus::OK == manager->AddTransferItem(*chat_transfer, RECEIVE, *(result.files.Get(0)), resume_position))
		initialization_failed = FALSE;
		if (result.active_directory.HasContent())
		{
			TRAPD(err, g_pcfiles->WriteDirectoryL(OPFILE_SAVE_FOLDER, result.active_directory.CStr()));
		}
	}

	FileReceiveInitDone(resume_position, initialization_failed); // Ignore return value.
}

#endif // IRC_SUPPORT
