/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef CHAT_FILETRANSFER_H
# define CHAT_FILETRANSFER_H

# include "modules/windowcommander/OpTransferManager.h"
# include "modules/util/OpHashTable.h"
# include "adjunct/desktop_pi/desktop_file_chooser.h"

class ChatFileTransferManager;
class TransferItem;

//****************************************************************************
//
//	ChatFileTransfer
//
//****************************************************************************

class ChatFileTransfer
{
public:
	// Destructor.
	virtual ~ChatFileTransfer() { }

	// Methods.
	INT32 TransferId() const { return m_transfer_id; }

	// Virtual methods.
	virtual OP_STATUS GetOtherParty(OpString& other_party) const = 0;
	virtual OP_STATUS GetFileName(OpString& name) const = 0;
	virtual OpFileLength GetFileSize() const = 0;
	virtual OP_STATUS GetProtocolName(OpString& name) const = 0;

protected:
	// Constructor.
	ChatFileTransfer() : m_transfer_id(-1) { }

private:
	friend class ChatFileTransferManager;

	// Methods.
	void SetTransferId(INT32 transfer_id) { m_transfer_id = transfer_id; }

	// Members.
	INT32 m_transfer_id;
};


//****************************************************************************
//
//	ChatFileTransferManager
//
//****************************************************************************
class DCCFileReceiver;
class IRCProtocol;

class ChatFileTransferManager
:	public OpTransferManagerListener
{
public:

	class FileReceiveInitListener : public DesktopFileChooserListener
	{
		private:
			DCCFileReceiver* 			receiver;
			IRCProtocol*	 			protocol;
			ChatFileTransferManager * 	manager;
			ChatFileTransfer*			chat_transfer;
		public:
			FileReceiveInitListener(DCCFileReceiver* receiver, IRCProtocol* protocol)
				: receiver(receiver), protocol(protocol), manager(NULL){}
			void	SetChatInfo(ChatFileTransfer* chat_transfer, ChatFileTransferManager * manager)
               { this->chat_transfer = chat_transfer; this->manager = manager;}

			OP_STATUS FileReceiveInitDone(OpFileLength resume_position, BOOL initialization_failed);
			void OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result);
			DesktopFileChooserRequest		request;
			DesktopFileChooserRequest&		GetRequest() {  return request; }
	};

    friend class FileReceiveInitListener;

	// Construction / destruction.
	ChatFileTransferManager();
	~ChatFileTransferManager();

	// Methods.
	OP_STATUS FileSendInitializing(ChatFileTransfer& chat_transfer);
	OP_STATUS FileSendBegin(const ChatFileTransfer& chat_transfer);
	OP_STATUS FileSendProgress(const ChatFileTransfer& chat_transfer, UINT32 bytes_sent);
	OP_STATUS FileSendCompleted(const ChatFileTransfer& chat_transfer);
	OP_STATUS FileSendFailed(const ChatFileTransfer& chat_transfer);

	OP_STATUS FileReceiveInitializing(ChatFileTransfer& chat_transfer, FileReceiveInitListener * listener);
	OP_STATUS FileReceiveBegin(const ChatFileTransfer& chat_transfer);
	OP_STATUS FileReceiveProgress(const ChatFileTransfer& chat_transfer, unsigned char* received, UINT32 bytes_received);
	OP_STATUS FileReceiveCompleted(const ChatFileTransfer& chat_transfer);
	OP_STATUS FileReceiveFailed(const ChatFileTransfer& chat_transfer);

private:
	// Enum.
	enum TransferMethod { SEND, RECEIVE };

	// OpTransferManagerListener.
	virtual BOOL OnTransferItemAdded(OpTransferItem* transfer_item, BOOL is_populating = FALSE, double last_speed = 0);
	virtual void OnTransferItemRemoved(OpTransferItem* transfer_item);
	void OnTransferItemStopped(OpTransferItem* transferItem) {}
	void OnTransferItemResumed(OpTransferItem* transferItem) {}

#ifdef WIC_CAP_TRANSFERMANAGER_METHODS_DELETEFILES
	BOOL OnTransferItemCanDeleteFiles(OpTransferItem* transferItem) { return FALSE; }
	void OnTransferItemDeleteFiles(OpTransferItem* transferItem) {}
#endif // WIC_CAP_TRANSFERMANAGER_METHODS_DELETEFILES

	// Methods.
	OP_STATUS AddTransferItem(ChatFileTransfer&	chat_transfer); // FOR SEND
	OP_STATUS AddTransferItem(ChatFileTransfer&	chat_transfer, FileReceiveInitListener * listener); // FOR RECEIVE
	OP_STATUS AddTransferItem(ChatFileTransfer& chat_transfer, TransferMethod transfer_method,
							  const OpString& file_name, OpFileLength&	resume_position);

	OpTransferItem* GetTransferItem(const ChatFileTransfer& chat_transfer);
	INT32 GetTransferId(OpTransferItem* item) const;

	INT32 NewTransferId() { return m_next_transfer_id++; }

	// No copy or assignment.
	ChatFileTransferManager(const ChatFileTransferManager& other);
	ChatFileTransferManager& operator=(const ChatFileTransferManager& rhs);

	// Static members.
	static INT32 m_next_transfer_id;

	// Members.
	OpINT32HashTable<OpTransferItem> m_transfer_items;
	BOOL m_is_transfer_manager_listener;
	DesktopFileChooser* m_chooser;
};

#endif
