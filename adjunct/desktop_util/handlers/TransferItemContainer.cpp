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
#include "adjunct/desktop_util/handlers/TransferItemContainer.h"
#include "adjunct/desktop_util/sound/SoundUtils.h"
#include "adjunct/quick/managers/DesktopTransferManager.h"
#include "adjunct/quick/managers/NotificationManager.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "modules/pi/OpBitmap.h"
#include "modules/locale/oplanguagemanager.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/pi/OpLocale.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/quick/Application.h"
#include "modules/util/filefun.h"
#include "adjunct/desktop_pi/desktop_pi_util.h"
#include "modules/locale/locale-enum.h"

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
TransferItemContainer::TransferItemContainer(double known_speed)
	: m_id(GetUniqueID()),
	  m_transferitem(NULL),
	  m_status(OpTransferListener::TRANSFER_INITIAL),
	  m_is_resumable(Not_Resumable),
	  m_parent_container(NULL),
	  m_last_known_speed(known_speed),
	  m_last_known_speed_upload(0),
	  m_need_to_load_icon(true)
{
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
TransferItemContainer::~TransferItemContainer()
{
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
BOOL TransferItemContainer::LoadIconBitmap()
{
	if(m_transferitem && NeedToLoadIcon())
	{
		OpBitmap* iconbitmap = NULL;
		DesktopPiUtil::CreateIconBitmapForFile(&iconbitmap, m_transferitem->GetStorageFilename()->CStr());

		if(iconbitmap)
			SetIconBitmap(imgManager->GetImage(iconbitmap));

		SetHasTriedToLoadIcon();
	}

	return !m_iconbitmap.IsEmpty();
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TransferItemContainer::MakeFullPath(OpString & filename)
{
    if(m_transferitem->GetType() == TransferItem::TRANSFERTYPE_PEER2PEER_DOWNLOAD)
    {
		m_transferitem->GetDownloadDirectory(filename);

		// Append a path seperator to the end if there isn't one there already
		if(filename.HasContent() && filename[filename.Length()-1] != PATHSEPCHAR)
		{
			RETURN_IF_ERROR(filename.Append(PATHSEP));
		}

		return filename.Append(*(m_transferitem->GetStorageFilename()));
    }
    else if(m_transferitem->GetType() == TransferItem::TRANSFERTYPE_CHAT_UPLOAD)
    {
		return m_transferitem->GetURL()->GetAttribute(URL::KName_Username_Password_Hidden_Escaped, filename);
    }
    else
		return filename.Set(m_transferitem->GetStorageFilename()->CStr());
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TransferItemContainer::GetItemData(ItemData* item_data)
{
    if (item_data->query_type == COLUMN_QUERY)
    {
		INT32 col = item_data->column_query_data.column;

		if (col == 6)
		{
			RETURN_IF_ERROR(TranslateColumn(item_data, col));

			if(col < 0)
			{
				item_data->column_query_data.column_text->Empty();
				return OpStatus::OK;
			}
		}

		switch(col)
		{
			case 0:
				// Status column
				return SetUpStatusColumn(item_data);

			case 1:
				// Filename column
				return SetUpFilenameColumn(item_data);

			case 2:
				// Size column
				return SetUpSizeColumn(item_data);

			case 3:
				// Progress column
				return SetUpProgressColumn(item_data);

			case 4:
				// Time column
				return SetUpTimeColumn(item_data);

			case 5:
				// Speed column
				return SetUpSpeedColumn(item_data);
		}
    }
    else if (item_data->query_type == INFO_QUERY)
    {
		return ProcessInfoQuery(item_data);
    }

    return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TransferItemContainer::SetUpSpeedColumn(ItemData* item_data)
{
	if(m_status == OpTransferListener::TRANSFER_PROGRESS ||
	   m_status == OpTransferListener::TRANSFER_SHARING_FILES)
	{
		BOOL has_speed = FALSE;

		double current_down = m_transferitem->GetKbps();
		if(current_down != 0.0)
		{
			m_last_known_speed = current_down;
			has_speed = TRUE;
		}

		double current_up = m_transferitem->GetKbpsUpload();
		if(current_up != 0.0)
		{
			m_last_known_speed_upload = current_up;
			has_speed = TRUE;
		}

		if (!has_speed)
		{
			if(m_transferitem->GetType() == TransferItem::TRANSFERTYPE_PEER2PEER_DOWNLOAD)
			{
				return item_data->column_query_data.column_text->Set("? / ?");
			}
			else
			{
				return item_data->column_query_data.column_text->Set("?");
			}
		}
	}

	item_data->column_query_data.column_text->Empty();

	if(m_status != OpTransferListener::TRANSFER_CHECKING_FILES &&
	   (m_last_known_speed != 0.0 || m_last_known_speed_upload != 0.0))
	{
		OpString speed_unit;
		g_languageManager->GetString(Str::D_TRANSFER_SPEED_KILOBYTES_PER_SECOND, speed_unit);
		item_data->column_query_data.column_sort_order = (int)(m_last_known_speed*100);
		if(m_transferitem->GetType() == TransferItem::TRANSFERTYPE_PEER2PEER_DOWNLOAD)
		{
			uni_char speedString[254];
			uni_char upspeedString[254];
			g_oplocale->NumberFormat(speedString,
									 24,
									 (float)m_last_known_speed,
									 (item_data->column_query_data.column == 6) ? 0 : 1,
									 FALSE);

			g_oplocale->NumberFormat(upspeedString,
									 24,
									 (float)m_last_known_speed_upload,
									 (item_data->column_query_data.column == 6) ? 0 : 1,
									 FALSE);

			return item_data->column_query_data.column_text->AppendFormat(UNI_L("%s %s / %s %s"),
														speedString,
														speed_unit.CStr(),
														upspeedString,
														speed_unit.CStr());
		}
		else
		{
			uni_char speedString[254];
			g_oplocale->NumberFormat(speedString,
									 24,
									 (float)m_last_known_speed,
									 (item_data->column_query_data.column == 6) ? 0 : 1,
									 FALSE);

			return item_data->column_query_data.column_text->AppendFormat( UNI_L("%s %s"),
														 speedString,
														 speed_unit.CStr());
		}
	}
	else
	{
		item_data->column_query_data.column_sort_order = 0;
		return OpStatus::OK;
	}
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TransferItemContainer::SetUpTimeColumn(ItemData* item_data)
{
	item_data->column_query_data.column_text->Empty();
	if(m_status == OpTransferListener::TRANSFER_PROGRESS)
	{
		if(m_transferitem->GetSize() < m_transferitem->GetHaveSize())
			return item_data->column_query_data.column_text->Set("?");

		UINT32 time_left = m_transferitem->GetTimeEstimate();
		item_data->column_query_data.column_sort_order = time_left;

		UINT32 days = time_left/(60*60*24);
		time_left -= days*60*60*24;

		UINT32 hours = time_left/(60*60);
		time_left -= hours*60*60;

		UINT32 minutes = time_left/60;
		time_left -= minutes*60;

		// TODO: Localize
		if(days)
			return item_data->column_query_data.column_text->AppendFormat(UNI_L("%d %d:%02d:%02d"),
														days,
														hours,
														minutes,
														time_left);
		if(hours)
			return item_data->column_query_data.column_text->AppendFormat(UNI_L("%d:%02d:%02d"),
														hours,
														minutes,
														time_left);

		return item_data->column_query_data.column_text->AppendFormat(UNI_L("%d:%02d"),
													minutes,
													time_left);
	}

	item_data->column_query_data.column_sort_order = 0;
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TransferItemContainer::SetUpProgressColumn(ItemData* item_data)
{
	if (m_status == OpTransferListener::TRANSFER_PROGRESS ||
		m_status == OpTransferListener::TRANSFER_CHECKING_FILES)
	{
		double downloadedsize = (double)(INT64)m_transferitem->GetHaveSize();
		double totalsize = (double)(INT64)m_transferitem->GetSize();

		if(totalsize < downloadedsize)
			return item_data->column_query_data.column_text->Set("?");

		// FIXME: comparing double != 0.0 is not robust; chose an error bar !
		OP_MEMORY_VAR double percent =
			(downloadedsize != 0.0 && totalsize != 0.0) ?
			(downloadedsize / totalsize) * 100 : 0;

		INT32 percent_int = static_cast<INT32>(percent);

		if (percent_int == 0)
		{
			percent_int = 1;
		}

		item_data->column_query_data.column_progress = percent_int;
		item_data->column_query_data.column_sort_order = percent_int;

		OpString progress_string;

		if (m_status == OpTransferListener::TRANSFER_CHECKING_FILES)
		{
			OpString languagestring;

			g_languageManager->GetString(Str::S_TRANSFERS_PANEL_CHECKING_FILES, languagestring);
			progress_string.AppendFormat(UNI_L("%s: "), languagestring.CStr());
		}
		OpString percent_string;

		// We want to never show 100.0% on a file unless it has been completely download.
		// Due to rounding issues, we will compare the string instead of the percentage
		percent_string.AppendFormat(UNI_L("%.1f%%"), percent);
		if(!percent_string.Compare(UNI_L("100.0%")) || !percent_string.Compare(UNI_L("100,0%")))
		{
			progress_string.AppendFormat(UNI_L("%.1f%%"), 99.9);
		}
		else
		{
			progress_string.AppendFormat(UNI_L("%.1f%%"), percent != 100.0 ? percent : 99.9);
		}
		return item_data->column_query_data.column_text->Set(progress_string);
	}
	else
	{
		Str::LocaleString str(Str::NOT_A_STRING);

		switch (m_status)
		{
			case OpTransferListener::TRANSFER_INITIAL:
				str = Str::S_STARTING;
				item_data->column_query_data.column_sort_order = 0;
				break;
			case OpTransferListener::TRANSFER_ABORTED:
				str = Str::SI_IDSTR_TRANSWIN_STATUS_CANCELLED;
				item_data->column_query_data.column_sort_order = 101;
				break;
			case OpTransferListener::TRANSFER_FAILED:
				str = Str::SI_IDSTR_TRANSWIN_STATUS_ERROR;
				item_data->column_query_data.column_sort_order = 102;
				break;
			case OpTransferListener::TRANSFER_SHARING_FILES:
				str = Str::S_TRANSFERS_PANEL_SHARING_FILES;
				item_data->column_query_data.column_sort_order = 103;
				break;
			default:
				str = Str::SI_IDSTR_TRANSWIN_STATUS_DONE;
				item_data->column_query_data.column_sort_order = 104;

		}
		return g_languageManager->GetString(str, *item_data->column_query_data.column_text);
	}
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TransferItemContainer::SetUpSizeColumn(ItemData* item_data)
{
	OpFileLength item_size = m_transferitem->GetSize();

	item_data->column_query_data.column_sort_order = (INT32)item_size;
	OpString sizestring;

	if(item_size < m_transferitem->GetHaveSize())
		sizestring.Set("?");
	else
	{
		if (sizestring.Reserve(256))
			StrFormatByteSize(sizestring, item_size, SFBS_ABBRIVATEBYTES);
	}

	return item_data->column_query_data.column_text->Set(sizestring);
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TransferItemContainer::SetUpFilenameColumn(ItemData* item_data)
{
	OpString filename;
	GetFilename(m_transferitem, filename);

	//  Notice: icon might not be loaded here. DesktopTransferManager will handle this.
	if(!m_iconbitmap.IsEmpty())
	{
		item_data->column_bitmap = m_iconbitmap;
	}
	else if(NeedToLoadIcon())
	{
		m_parent_container->StartLoadIcons();
	}


	switch(m_transferitem->GetType())
	{
		case TransferItem::TRANSFERTYPE_CHAT_UPLOAD:
		{
			item_data->column_query_data.column_image_2 = "Transfer Upload"; break;
		}
	}

	if(m_status == OpTransferListener::TRANSFER_PROGRESS)
	{
		item_data->column_query_data.column_text_color = OP_RGB(0x00, 0x88, 0x00);	//light green
	}
	else if(m_status == OpTransferListener::TRANSFER_SHARING_FILES)
	{
		item_data->column_query_data.column_text_color = OP_RGB(0x00, 0x00, 0xAA);	//light blue
	}
	else
	{
		item_data->column_query_data.column_text_color = OP_RGB(0x00, 0x00, 0x00);
	}
	return item_data->column_query_data.column_text->Set(filename);
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TransferItemContainer::SetUpStatusColumn(ItemData* item_data)
{
	const char *col_img;

	switch (m_status)
	{
		case OpTransferListener::TRANSFER_PROGRESS:
		case OpTransferListener::TRANSFER_CHECKING_FILES:
			col_img = "Transfer Loading";
			break;
		case OpTransferListener::TRANSFER_SHARING_FILES:
			col_img = "Transfer Upload";
			break;
		case OpTransferListener::TRANSFER_ABORTED:
			col_img = "Transfer Stopped";
			break;
		case OpTransferListener::TRANSFER_FAILED:
			col_img = "Transfer Failure";
			break;
		default:
			col_img = "Transfer Success";
			break;
	}
	item_data->column_query_data.column_image = col_img;
	item_data->column_query_data.column_sort_order = m_status;

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TransferItemContainer::TranslateColumn(ItemData* item_data, INT32 & col)
{
	// show progress bar with text alternating between
	// downloaded size/time left/speed while downloading,
	// (or checking/sharing progress information for torrents)
	// otherwise show size

	if(m_status == OpTransferListener::TRANSFER_CHECKING_FILES ||
	   m_status == OpTransferListener::TRANSFER_SHARING_FILES)
	{
		// progress
		col = 3;
	}
	else if(m_status == OpTransferListener::TRANSFER_PROGRESS)
	{
		double downloadedsize = (double)(INT64)m_transferitem->GetHaveSize();
		double totalsize = (double)(INT64)m_transferitem->GetSize();

		if(totalsize < downloadedsize)
			return item_data->column_query_data.column_text->Set("?");

		double percent = (downloadedsize != 0.0 && totalsize != 0.0) ? (downloadedsize / totalsize) * 100 : 0;

		INT32 percent_int = static_cast<INT32>(percent);

		if (percent_int == 0)
		{
			percent_int = 1;
		}

		item_data->column_query_data.column_progress = percent_int;

		switch(m_parent_container->GetSwapColumn())
		{
			case 0:
			{
				// size
				col = 2;
			}
			break;
			case 1:
			{
				//speed
				col = 5;
			}
			break;
			case 2:
			{
				//time left
				col = 4;
			}
			break;
			default:
			{
				col = -1;
			}
		}
	}
	else
	{
		// size
		col = 2;
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TransferItemContainer::ProcessInfoQuery(ItemData * item_data)
{
    OpString string;

    g_languageManager->GetString(Str::DI_IDL_SOURCE, string);

    if (m_transferitem->GetType() == TransferItem::TRANSFERTYPE_CHAT_UPLOAD ||
		m_transferitem->GetType() == TransferItem::TRANSFERTYPE_PEER2PEER_DOWNLOAD)
    {
		item_data->info_query_data.info_text->AddTooltipText(string.CStr(),
											 m_transferitem->GetStorageFilename()->CStr());

		item_data->info_query_data.info_text->SetStatusText(m_transferitem->GetStorageFilename()->CStr());
    }
    else
    {
		OpString16 url;
		RETURN_IF_ERROR(m_transferitem->GetURL()->GetAttribute(URL::KName_Username_Password_Hidden_Escaped, url));
		item_data->info_query_data.info_text->AddTooltipText(string.CStr(),
											 url.CStr());

		item_data->info_query_data.info_text->SetStatusText(url.CStr());
    }

    g_languageManager->GetString(Str::DI_IDL_DESTINATION, string);

    // Make the "To :" tooltip text :
    OpString filename;
    MakeFullPath(filename);
    item_data->info_query_data.info_text->AddTooltipText(string.CStr(), filename.CStr());

    g_languageManager->GetString(Str::DI_IDL_SIZE, string);

    OpString sizestring;
    OpString appendedsizestring;

    if (sizestring.Reserve(256))
    {
		StrFormatByteSize(sizestring,
						  m_transferitem->GetSize(),
						  SFBS_ABBRIVATEBYTES|SFBS_DETAILS);

		item_data->info_query_data.info_text->AddTooltipText(string.CStr(), sizestring.CStr());

		if(m_transferitem->GetType() == TransferItem::TRANSFERTYPE_PEER2PEER_DOWNLOAD)
		{
			StrFormatByteSize(sizestring,
							  m_transferitem->GetHaveSize(),
							  SFBS_ABBRIVATEBYTES|SFBS_DETAILS);

			appendedsizestring.Set(sizestring);

			appendedsizestring.Append(" / ");

			StrFormatByteSize(sizestring,
							  m_transferitem->GetUploaded(),
							  SFBS_ABBRIVATEBYTES|SFBS_DETAILS);

			appendedsizestring.Append(sizestring);
		}
		else
		{
			StrFormatByteSize(sizestring,
							  m_transferitem->GetHaveSize(),
							  SFBS_ABBRIVATEBYTES|SFBS_DETAILS);

			appendedsizestring.Set(sizestring);
		}

		g_languageManager->GetString(Str::DI_IDL_TRANSFERED, string);

		item_data->info_query_data.info_text->AddTooltipText(string.CStr(), appendedsizestring.CStr());
    }

    if(m_transferitem->GetType() == TransferItem::TRANSFERTYPE_PEER2PEER_DOWNLOAD)
    {
		OpString loc_conns;
		g_languageManager->GetString(Str::S_TRANSFERS_PANEL_CONNECTION, loc_conns);
		string.Set(loc_conns);
		sizestring.Empty();
		sizestring.AppendFormat(UNI_L("%d/%d [%d/%d]"),
								m_transferitem->GetConnectionsWithCompleteFile(),
								m_transferitem->GetConnectionsWithIncompleteFile(),
								m_transferitem->GetTotalWithCompleteFile(),
								m_transferitem->GetTotalDownloaders());

		item_data->info_query_data.info_text->AddTooltipText(string.CStr(), sizestring.CStr());

		OpString loc_trans;
		g_languageManager->GetString(Str::S_TRANSFERS_PANEL_ACTIVE_TRANSFERS, loc_trans);
		string.Set(loc_trans);
		sizestring.Empty();
		sizestring.AppendFormat(UNI_L("%d / %d"),
								m_transferitem->GetDownloadConnections(),
								m_transferitem->GetUploadConnections());

		item_data->info_query_data.info_text->AddTooltipText(string.CStr(), sizestring.CStr());
    }
    return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void TransferItemContainer::Open(BOOL open_folder)
{
    // Make the proper full path :
    OpString filename;
    MakeFullPath(filename);

    if(!filename.IsEmpty())
    {
#ifdef WIDGET_RUNTIME_SUPPORT
			if (!open_folder
				&& m_transferitem->GetViewer().GetContentType() == URL_GADGET_INSTALL_CONTENT
				&& !g_pcui->GetIntegerPref(PrefsCollectionUI::DisableWidgetRuntime)
				&& g_desktop_gadget_manager->OpenIfGadgetFolder(filename))
			{
				m_transferitem->SetTransferListener(NULL);
				return;
			}
#endif // WIDGET_RUNTIME_SUPPORT
		if (open_folder)
		{
			static_cast<DesktopOpSystemInfo*>(g_op_system_info)->OpenFileFolder(filename);
		}
		else
		{
			OP_STATUS status = SetHandlerApplication();

			OP_ASSERT(OpStatus::IsSuccess(status));
			OpStatus::Ignore(status);

			// Open the file with the handler :
#ifdef MSWIN
			if (m_handlerapplication.IsEmpty())
				Execute(filename.CStr(), NULL);
			else
#endif //MSWIN
				static_cast<DesktopOpSystemInfo*>(g_op_system_info)->OpenFileInApplication(
						m_handlerapplication.CStr(), filename.CStr());
		}
    }
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TransferItemContainer::SetHandlerApplication()
{
	RETURN_IF_ERROR(SetViewer());

	OpString handlerapplication;

	RETURN_IF_ERROR(handlerapplication.Set(m_transferitem->GetViewer().GetApplicationToOpenWith()));

	OpString fileExtension;
	OpString fileName;

	RETURN_IF_ERROR(GetFilenameAndExtension(fileName, fileExtension));

	if(handlerapplication.IsEmpty())
	{
		OpString content_type;

		URL* url = m_transferitem->GetURL();

		if(url)
		{
			RETURN_IF_ERROR(content_type.Set(url->GetAttribute(URL::KMIME_Type)));
		}

		RETURN_IF_ERROR(g_op_system_info->GetFileHandler(&fileName, content_type, handlerapplication));
	}

#ifdef MSWIN
	OpString quoted_filename;
	quoted_filename.AppendFormat(UNI_L("\"%s\""), fileName.CStr());
    BOOL isProgram = quoted_filename.Compare(handlerapplication) == 0;
    BOOL isHandledbyShell = handlerapplication.SubString(handlerapplication.Length()-4).CompareI(UNI_L(".dll")) == 0;
    if(!isProgram && !isHandledbyShell)	//no handler needed if file is "dll-handled"
#endif //MSWIN
    {
		m_handlerapplication.Set(handlerapplication.CStr());
    }

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TransferItemContainer::SetViewer()
{
	if(m_transferitem->GetViewer().GetAction() != VIEWER_NOT_DEFINED)
		return OpStatus::OK;

    OpString fileExtension;
    OpString fileName;

	GetFilenameAndExtension(fileName, fileExtension);

	Viewer* viewer = g_viewers->FindViewerByExtension(fileExtension);

	if(!viewer)
		return OpStatus::ERR;

	m_transferitem->SetViewer(*viewer);

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OP_STATUS TransferItemContainer::GetFilenameAndExtension(OpString & fileName,
														 OpString & fileExtension)
{
    RETURN_IF_ERROR(fileName.Set(*((TransferItem*)m_transferitem)->GetStorageFilename()));

    if(!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::TrustServerTypes))
    {
		RETURN_IF_ERROR(fileExtension.Set( fileName.SubString(fileName.FindLastOf('.')+1) ));
    }

    if (fileExtension.IsEmpty())
    {
		TRAPD(op_err, m_transferitem->GetURL()->GetAttributeL(URL::KSuggestedFileNameExtension_L, fileExtension));
		RETURN_IF_ERROR(op_err);
    }

    if (fileExtension.IsEmpty())
		RETURN_IF_ERROR(fileName.Set("default"));
    else
    {
		RETURN_IF_ERROR(fileName.Set("default."));
		RETURN_IF_ERROR(fileName.Append(fileExtension));
    }

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void TransferItemContainer::SetStatus(OpTransferListener::TransferStatus s)
{
    if(s == OpTransferListener::TRANSFER_DONE && m_status != s)
    {
		SetStatusDone(s);
    }
    else if(m_status == OpTransferListener::TRANSFER_CHECKING_FILES &&
			s        == OpTransferListener::TRANSFER_SHARING_FILES)
    {
		SetStatusSharing(s);
    }

    m_status = s;
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void TransferItemContainer::SetStatusDone(OpTransferListener::TransferStatus s)
{
#ifdef VIRUSSCAN_SUPPORT
    if(m_status  != OpTransferListener::TRANSFER_INITIAL ||
       (s        != OpTransferListener::TRANSFER_DONE    &&
		m_status != OpTransferListener::TRANSFER_INITIAL))
    {
		URLContentType type = m_transferitem->GetURL()->ContentType();

		if(type == URL_UNKNOWN_CONTENT || type == URL_UNDETERMINED_CONTENT)
		{
			VirusCheckDialog* dialog = OP_NEW(VirusCheckDialog, ());

			if (dialog == NULL)
			{
				return;
			}

			dialog->SetFile(*m_transferitem->GetStorageFilename());
			dialog->Init(g_application->GetActiveDesktopWindow());
		}
    }
#endif // VIRUSSCAN_SUPPORT

    // Open the item if it should be opened :

	OP_STATUS status = SetHandlerApplication();

//	OP_ASSERT(OpStatus::IsSuccess(status));
	OpStatus::Ignore(status);

#ifdef WIC_CAP_TRANSFERS_USE_VIEWERS
    if(m_transferitem->GetViewer().Action() == VIEWER_APPLICATION)
#else
    if(m_transferitem->GetAction() & OpTransferItem::ACTION_RUN_WHEN_FINISHED)
#endif // WIC_CAP_TRANSFERS_USE_VIEWERS
    {
		Open(FALSE);
    }

    OpString soundfile;
    if (m_status != OpTransferListener::TRANSFER_INITIAL
		&& OpStatus::IsSuccess(soundfile.Set(g_pcui->GetStringPref(PrefsCollectionUI::TransferDoneSound)))
		)
    {
		OpStatus::Ignore(SoundUtils::SoundIt(soundfile, FALSE));

#ifdef WIC_CAP_TRANSFERS_USE_VIEWERS
		if(m_transferitem->GetViewer().Action() != VIEWER_APPLICATION)
#else
		if(!(m_transferitem->GetAction() & OpTransferItem::ACTION_RUN_WHEN_FINISHED))
#endif // WIC_CAP_TRANSFERS_USE_VIEWERS
		{
			// Mark the transfer as done :
			g_desktop_transfer_manager->SetNewTransfersDone(TRUE);

			// Give notification that the download is done :
			OpString filename;
			GetFilename(m_transferitem, filename);

			if (g_pcui->GetIntegerPref(PrefsCollectionUI::ShowNotificationForFinishedTransfers))
			{
				OpString languagestring;
				OpInputAction* transfer_action;

#ifdef TRANSFER_CLICK_TO_OPEN
				g_languageManager->GetString(Str::S_TRANSFER_COMPLETED_NO_MENU, languagestring);
				OpString mimeType, handler;
				RETURN_VOID_IF_ERROR(g_op_system_info->GetFileHandler(&filename, mimeType, handler));
				OpString fullpath;
				RETURN_VOID_IF_ERROR(MakeFullPath(fullpath));
				if (!handler.IsEmpty()) {
					transfer_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_OPEN_FILE_IN));
					if (transfer_action) {
						transfer_action->SetActionDataString(handler.CStr());
						transfer_action->SetActionDataStringParameter(fullpath.CStr());
					}
				} else {
					transfer_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_OPEN_FOLDER));
					if (transfer_action)
						transfer_action->SetActionDataString(fullpath.CStr());
				}
#else
				g_languageManager->GetString(Str::S_TRANSFER_COMPLETED, languagestring);
				transfer_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_POPUP_MENU));
				if (transfer_action)
				{
					transfer_action->SetActionData(m_id);
					transfer_action->SetActionDataString(UNI_L("Transfers Notify Popup Menu"));
				}
#endif // TRANSFER_CLICK_TO_OPEN

				if (languagestring.HasContent())
				{
					OpString formatted;
					formatted.AppendFormat(languagestring.CStr(), filename.CStr());
					if (!transfer_action) return;
					g_notification_manager->ShowNotification(DesktopNotifier::TRANSFER_COMPLETE_NOTIFICATION,
															 formatted,
															 "Panel Transfers",
															 transfer_action);
				}
			}
		}
    }
}


/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void TransferItemContainer::SetStatusSharing(OpTransferListener::TransferStatus s)
{
    // we've gone from checking to sharing, so at this point we'll show a notification
    // that the download is done, but add some extra information to the popup

    OpString soundfile;
    if(OpStatus::IsSuccess(soundfile.Set(g_pcui->GetStringPref(PrefsCollectionUI::TransferDoneSound))))
		OpStatus::Ignore(SoundUtils::SoundIt(soundfile, FALSE));

#ifdef WIC_CAP_TRANSFERS_USE_VIEWERS
	if(m_transferitem->GetViewer().Action() != VIEWER_APPLICATION)
#else
	if(!(m_transferitem->GetAction() & OpTransferItem::ACTION_RUN_WHEN_FINISHED))
#endif // WIC_CAP_TRANSFERS_USE_VIEWERS
    {
		// Mark the transfer as done :
		g_desktop_transfer_manager->SetNewTransfersDone(TRUE);

		// Give notification that the download is done :
		OpString filename;
		GetFilename(m_transferitem, filename);

		OpString languagestring;
		g_languageManager->GetString(Str::S_BITTORRENT_TRANSFER_COMPLETED, languagestring);

		if (languagestring.HasContent() &&
			g_pcui->GetIntegerPref(PrefsCollectionUI::ShowNotificationForFinishedTransfers))
		{
			OpString formatted;
			formatted.AppendFormat(languagestring.CStr(), filename.CStr());
			OpInputAction* notification_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_POPUP_MENU));
			if (notification_action)
			{
				notification_action->SetActionData(m_id);
				notification_action->SetActionDataString(UNI_L("Transfers Notify Popup Menu"));
			}
			g_notification_manager->ShowNotification(DesktopNotifier::TRANSFER_COMPLETE_NOTIFICATION,
														formatted,
														"Panel Transfers",
														notification_action, 
														TRUE);
		}
    }
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
void TransferItemContainer::GetFilename(TransferItem * transferitem, OpString & filename)
{
    filename.Set(*transferitem->GetStorageFilename());
    int pos = filename.FindLastOf(PATHSEPCHAR);
    if(pos != KNotFound)
		filename.Set(filename.SubString(pos+1));
}
