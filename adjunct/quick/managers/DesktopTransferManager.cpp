/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** George Refseth and Patricia Aas
*/

#include "core/pch.h"
#include "adjunct/quick/managers/DesktopTransferManager.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/viewers/viewers.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/url/url_tags.h"
#include "modules/util/opfile/opfile.h"
#include "modules/formats/url_dfr.h"
#include "modules/cache/cache_common.h"
#include "modules/cache/cache_int.h"
#include "modules/windowcommander/OpTransferManager.h"

class DelayedManageAction : public OpTimerListener
{
public:
    void Start()
		{
			m_timer.SetTimerListener( this );
			m_timer.Start(100); // ms
		}

    void OnTimeOut(OpTimer* timer)
		{
			g_input_manager->InvokeAction(OpInputAction::ACTION_MANAGE,
										  g_pcui->GetIntegerPref(PrefsCollectionUI::TransWinActivateOnNewTransfer) == 2,
										  UNI_L("transfers"));
		}

private:
    OpTimer m_timer;
};

#ifdef DESKTOP_ASYNC_ICON_LOADER
/* static */
OP_STATUS OpAsyncFileBitmapLoader::Create(OpAsyncFileBitmapLoader** new_asyncfilebitmaploader)
{
	OP_ASSERT(new_asyncfilebitmaploader != NULL);
	*new_asyncfilebitmaploader = OP_NEW(DesktopOpAsyncFileBitmapLoader, ());
	if(*new_asyncfilebitmaploader == NULL)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

DesktopOpAsyncFileBitmapLoader::~DesktopOpAsyncFileBitmapLoader() 
{
	g_main_message_handler->UnsetCallBacks(this);
}
OP_STATUS DesktopOpAsyncFileBitmapLoader::Init(OpAsyncFileBitmapHandlerListener *listener)
{
	m_listener = listener;
	g_main_message_handler->SetCallBack(this, MSG_QUICK_LOAD_ICONS, 0);

	return OpStatus::OK;
}
void DesktopOpAsyncFileBitmapLoader::Start(OpVector<TransferItemContainer>& transferitems)
{
	OP_ASSERT(transferitems.GetCount());

	if(transferitems.GetCount())
	{
		for(UINT32 n = 0; n < transferitems.GetCount(); n++)
		{
			TransferItemContainer *item = transferitems.Get(n);
			if(item->NeedToLoadIcon())
			{
				OpAutoPtr<OpString> filename(OP_NEW(OpString, ()));
				if(filename.get() && item->GetAssociatedItem())
				{
					if(OpStatus::IsSuccess(filename->Set(*item->GetAssociatedItem()->GetStorageFilename())) &&
						OpStatus::IsSuccess(m_filenames.Add(filename.get())))
						filename.release();
				}
			}
		}
		g_main_message_handler->PostMessage(MSG_QUICK_LOAD_ICONS,(MH_PARAM_1)this,0);
	}
}

void DesktopOpAsyncFileBitmapLoader::LoadItemIcons()
{
	if (m_filenames.GetCount() > 0)
	{
		// find the matching transfer item. This also ensures that if the user
		// removes an item while we're getting icons, we'll just ignore it
		TransferItemContainer *item = g_desktop_transfer_manager->FindTransferItemContainerFromFilename(*m_filenames.Get(0));
		if(item && item->NeedToLoadIcon())
		{
			if(item->LoadIconBitmap())
				m_listener->OnBitmapLoaded(item);

			item->SetHasTriedToLoadIcon();
		}

		m_filenames.Delete(0, 1);
	}

	// leave the leftover work to next round if not finished
	if(!m_filenames.GetCount())
	{
		m_listener->OnBitmapLoadingDone();
	}
	else
		g_main_message_handler->PostMessage(MSG_QUICK_LOAD_ICONS,(MH_PARAM_1)this,0);
}

void DesktopOpAsyncFileBitmapLoader::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch(msg)
	{
	case MSG_QUICK_LOAD_ICONS:
		LoadItemIcons();
		break;

	default:
		OP_ASSERT(FALSE);
	}
}

#endif // DESKTOP_ASYNC_ICON_LOADER

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
DesktopTransferManager::DesktopTransferManager()
    : m_timer(NULL),
      m_delayed_manage_action(0),
      m_new_transfers_done(FALSE),
      m_swap_column(0),
	  m_icon_loader(NULL),
	  m_bitmap_loading_in_progress(false)
{
    m_addtransferitemsontop = g_pcui->GetIntegerPref(PrefsCollectionUI::TransferItemsAddedOnTop);
	g_main_message_handler->SetCallBack(this, MSG_QUICK_LOAD_TRANSFERS_RESCUE_FILE, 0);
}


OP_STATUS DesktopTransferManager::Init()
{
	RETURN_IF_ERROR(g_transferManager->AddTransferManagerListener(this));

	RETURN_IF_ERROR(OpAsyncFileBitmapLoader::Create(&m_icon_loader));
	RETURN_IF_ERROR(m_icon_loader->Init(this));

	// delay loading the rescue file until after startup. This particular operation is slow if the user has installed
	// shell extensions, so let's not let that delay showing the main window - pettern 24.08.2010
	g_main_message_handler->PostDelayedMessage(MSG_QUICK_LOAD_TRANSFERS_RESCUE_FILE, (MH_PARAM_1)this, 0, 100);
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
DesktopTransferManager::~DesktopTransferManager()
{
	g_transferManager->RemoveTransferManagerListener(this);

	UINT32 i;

	// make sure we're not listener on any items.  Fixes bug DSK-240287
	for(i = 0; i < m_transferitems.GetCount(); i++)
	{
		TransferItemContainer* tic = m_transferitems.Get(i);
		if(tic)
		{
			OpTransferItem* item = tic->GetAssociatedItem();
			if(item)
			{
				item->SetTransferListener(NULL);
			}
		}
	}
	if(m_timer)
    {
		OP_DELETE(m_timer);
    }
    OP_DELETE(m_delayed_manage_action);
	g_main_message_handler->UnsetCallBacks(this);

	OP_DELETE(m_icon_loader);
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
TransferItemContainer* DesktopTransferManager::FindTransferItemContainerFromFilename(OpStringC filename)
{
	for(INT32 n = 0; n < GetItemCount(); n++)
	{
		TransferItemContainer *item = static_cast<TransferItemContainer *>(GetItemByPosition(n));
		if(item->GetAssociatedItem()->GetStorageFilename()->Compare(filename) == 0)
		{
			return item;
		}
	}
	return NULL;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void DesktopTransferManager::SetNewTransfersDone(BOOL new_transfers_done)
{
    if (new_transfers_done == m_new_transfers_done)
		return;

    m_new_transfers_done = new_transfers_done;

    for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
    {
		m_listeners.Get(i)->OnNewTransfersDone();
    }
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void DesktopTransferManager::OnTimeOut(OpTimer* timer)
{
    switch(m_swap_column)
    {
		case 0:
		{
			m_swap_column = 1;
			m_timer->Start(TOGGLE_COLUMN_SHOW_SPEED_MS);
		}
		break;
		case 1:
		{
			m_swap_column = 2;
			m_timer->Start(TOGGLE_COLUMN_SHOW_TIMELEFT_MS);
		}
		break;
		case 2:
		{
			m_swap_column = 0;
			m_timer->Start(TOGGLE_COLUMN_SHOW_TRANSFERRED_SIZE_MS);
		}
		break;
		default:
		{
			// No op?
		}
    }

    BroadcastItemChanged(-1);

    if(!HasActiveTransfer())	//if no more transfers, kill the timers
    {
		m_swap_column = 0;
		OP_DELETE(m_timer);
		m_timer = NULL;
		//start new timer when DesktopTransferManager::OnProgress status changes to TRANSFER_PROGRESS
    }
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void DesktopTransferManager::OnProgress(OpTransferItem* transferItem,
									TransferStatus status)
{
    INT32 model_pos = GetModelIndex(transferItem);
    TransferItemContainer* tic = m_transferitems.Get(model_pos);

    if(tic)
    {
		tic->SetStatus(status);
    }
    BroadcastItemChanged(model_pos);
    if(g_input_manager)
		g_input_manager->UpdateAllInputStates();

    for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
    {
		m_listeners.Get(i)->OnTransferProgress(tic, status);
    }

    if(status == OpTransferListener::TRANSFER_PROGRESS && !m_timer)
    {
		if (!(m_timer = OP_NEW(OpTimer, ())))
			return;

		m_timer->SetTimerListener(this);
		m_timer->Start(TOGGLE_COLUMN_SHOW_TRANSFERRED_SIZE_MS);
    }
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void DesktopTransferManager::OnReset(OpTransferItem* transferItem)
{
    INT32 model_pos = GetModelIndex(transferItem);
    TransferItemContainer* tic = m_transferitems.Get(model_pos);
    tic->ResetStatus();
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void DesktopTransferManager::OnRedirect(OpTransferItem* transferItem,
									URL* redirect_from,
									URL* redirect_to)
{
    if(redirect_to != NULL)
    {
		OpStatus::Ignore(redirect_to->LoadToFile(((TransferItem *)transferItem)->GetStorageFilename()->CStr()));
    }
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void DesktopTransferManager::AddTransferListener(TransferListener* listener)
{
    m_listeners.Add(listener);
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void DesktopTransferManager::RemoveTransferListener(TransferListener* listener)
{
    INT32 pos = m_listeners.Find(listener);

    if (pos != -1)
    {
		m_listeners.Remove(pos);
    }
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
INT32 DesktopTransferManager::GetModelIndex(OpTransferItem* transferItem)
{
    for(int i = 0; i < GetItemCount(); i++)
    {
		if(m_transferitems.Get(i)->GetAssociatedItem() == (TransferItem*)transferItem)
		{
			return i;
		}
    }
    return -1;
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
UINT32 DesktopTransferManager::GetMaxTimeRemaining(UINT32* remaining_transfers)
{
    UINT32 maxtime = 0;
    UINT32 remaining_count = 0;

    for(int i = 0; i < GetItemCount(); i++)
    {
		if(m_transferitems.Get(i)->GetStatus() == TRANSFER_PROGRESS)
		{
			remaining_count++;
			TransferItem* assoc = m_transferitems.Get(i)->GetAssociatedItem();
			UINT32 timeestimate = assoc ? assoc->GetTimeEstimate() : 0;
			if(timeestimate > maxtime && timeestimate < INT32_MAX)	// timeestimate above INT32_MAX is unknown so skip it
			{
				maxtime = timeestimate;
			}
		}
    }

    if (remaining_transfers)
		*remaining_transfers = remaining_count;

    return maxtime;
}


/***********************************************************************************
**
** Get information about the total and download bytes for the active downloads
**
***********************************************************************************/
void DesktopTransferManager::GetFilesSizeInformation(OpFileLength& downloaded_size, OpFileLength& total_sizes, UINT32* remaining_transfers)
{
	UINT32 remaining_count = 0;

	downloaded_size = total_sizes = 0;

	for(int i = 0; i < GetItemCount(); i++)
	{
		if(m_transferitems.Get(i)->GetStatus() == TRANSFER_PROGRESS)
		{
			remaining_count++;

			TransferItem *item = m_transferitems.Get(i)->GetAssociatedItem();
			
			downloaded_size += item->GetHaveSize();
			total_sizes += item->GetSize();
		}
	}
	if (remaining_transfers)
		*remaining_transfers = remaining_count;
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
BOOL DesktopTransferManager::OpenItemByID(INT32 id,
									  BOOL open_folder)
{
    for(int i = 0; i < GetItemCount(); i++)
    {
		if(m_transferitems.Get(i)->GetID() == id)
		{
			m_transferitems.Get(i)->Open(open_folder);
			return TRUE;
		}
    }
    return FALSE;
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
BOOL DesktopTransferManager::OnTransferItemAdded(OpTransferItem* transferItem,
											 BOOL is_populating,
											 double last_speed)
{
	if (!transferItem->GetShowTransfer())
	{
		return FALSE;
	}
    transferItem->SetTransferListener(this);

    TransferItemContainer* tic = OP_NEW(TransferItemContainer, (last_speed));
    if (!tic)
		return FALSE;

    tic->SetAssociatedItem((TransferItem*)transferItem);

    if(((TransferItem*)transferItem)->GetType() == TransferItem::TRANSFERTYPE_PEER2PEER_DOWNLOAD)
    {
		tic->SetResumableState(Probably_Resumable);
    }
    else
    {
		tic->SetResumableState((URL_Resumable_Status) transferItem->GetURL()->GetAttribute(URL::KResumeSupported));
    }
    tic->SetParentContainer(this);

	OP_STATUS status = tic->SetHandlerApplication();

//	OP_ASSERT(OpStatus::IsSuccess(status) || is_populating);
	OpStatus::Ignore(status);

    int insertposition = 0;

    if(m_addtransferitemsontop && !is_populating)		//if is_populating then add on bottom always
    {
		m_transferitems.Insert(insertposition, (TransferItemContainer*)tic);
    }
    else
    {
		insertposition = GetItemCount();
		m_transferitems.Add((TransferItemContainer*)tic);
    }

    BroadcastItemAdded(insertposition);

    if(!is_populating)	// the model is being built
    {
		for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
		{
			m_listeners.Get(i)->OnTransferAdded(tic);
		}

		if(g_pcui->GetIntegerPref(PrefsCollectionUI::TransWinActivateOnNewTransfer))
		{
			// Delay activation of OpInputAction::ACTION_MANAGE for now. There are a
			// number of lost-keyboard-focus (one is cross platform) problems when we open
			// the transfer window right after closing the file dialog and/or download
			// dialog. The system will after the dialog closes return focus to the window
			// that had it (when activation events arrive) and problems arise when there is
			// suddenly a new window that should have focus (or not, depeding if it is
			// opened in the background). [espen 2006-11-08]

			if( !m_delayed_manage_action )
				m_delayed_manage_action = OP_NEW(DelayedManageAction, ());
			if( m_delayed_manage_action )
				m_delayed_manage_action->Start();
		}
    }
    return TRUE;
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void DesktopTransferManager::OnTransferItemRemoved(OpTransferItem* transferItem)
{
    INT32 model_pos = GetModelIndex(transferItem);
    if(model_pos != -1)
    {
		BroadcastItemRemoving(model_pos);
		TransferItemContainer* tic = m_transferitems.Remove(model_pos);
		for (UINT32 i = 0; i < m_listeners.GetCount(); i++)
		{
			m_listeners.Get(i)->OnTransferRemoved(tic);
		}
		OP_DELETE(tic);
		BroadcastItemRemoved(model_pos);
    }
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS DesktopTransferManager::GetColumnData(ColumnData* column_data)
{
    Str::LocaleString str(Str::NOT_A_STRING);

    switch(column_data->column)
    {
		case 0:
		{
			// progressindicator
			str = Str::S_STATUS;
			break;
		}
		case 1:
		{
			// name
			str = Str::SI_IDSTR_TRANSWIN_NAME_COL;
			break;
		}
		case 2:
		{
			// size
			str = Str::SI_IDSTR_TRANSWIN_SIZE_COL;
			break;
		}
		case 3:
		{
			// progress
			str = Str::SI_IDSTR_TRANSWIN_PROGRESS_COL;
			break;
		}
		case 4:
		{
			// time
			str = Str::SI_IDSTR_TRANSWIN_TIME_COL;
			break;
		}
		case 5:
		{
			// speed
			str = Str::SI_IDSTR_TRANSWIN_SPEED_COL;
			break;
		}
		case 6:
		{
			// toggled
			str = Str::S_TRANSWIN_INFO_COL;
			break;
		}
		default:
		{
			return OpStatus::OK;
		}
    }

    return g_languageManager->GetString(str, column_data->text);
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
OP_STATUS DesktopTransferManager::GetTypeString(OpString& type_string)
{
    return g_languageManager->GetString(Str::M_VIEW_HOTLIST_MENU_TRANSFERS, type_string);
}
#endif

OpTreeModelItem* DesktopTransferManager::GetItemByPosition(INT32 position)
{
    return m_transferitems.Get(position);
}

void DesktopTransferManager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
    switch(msg)
    {
	case MSG_QUICK_LOAD_TRANSFERS_RESCUE_FILE:
		OpStatus::Ignore(g_transferManager->SetRescueFileName(UNI_L("download.dat")));
		break;

    default:
        OP_ASSERT(FALSE);
    }
}

void DesktopTransferManager::StartLoadIcons()
{
	if(!m_bitmap_loading_in_progress)
	{
		m_bitmap_loading_in_progress = true;
		m_icon_loader->Start(m_transferitems);
	}
}

void DesktopTransferManager::OnBitmapLoadingDone()
{
	m_bitmap_loading_in_progress = false;
}

void DesktopTransferManager::OnBitmapLoaded(TransferItemContainer *item)
{
	INT32 model_pos = GetModelIndex(item->GetAssociatedItem());
	if(model_pos != -1)
	{
		BroadcastItemChanged(model_pos, FALSE);
	}
}