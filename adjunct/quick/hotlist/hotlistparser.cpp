/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 * @author Co-owner: Espen Sand (espen)
 *
 */

#include "core/pch.h"

#include "adjunct/quick/hotlist/hotlistparser.h"

#include "adjunct/m2/src/util/qp.h"
#include "adjunct/quick/hotlist/hotlistfileio.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/managers/DesktopBookmarkManager.h"
#include "adjunct/quick/models/BookmarkModel.h"
#include "adjunct/quick/models/DesktopBookmark.h"
#include "adjunct/quick/models/Bookmark.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/util/htmlify.h"
#include "modules/util/opfile/unistream.h"

#ifdef PREFS_CAP_GENERATE_CLIENTGUID
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#endif // PREFS_CAP_GENERATE_CLIENTGUID
#include "adjunct/quick/Application.h"
#include "modules/sqlite/sqlite3.h"

#ifndef WIN_CE
# include <errno.h>
#endif

//#define MEASURE_LOADTIME
#if defined(MEASURE_LOADTIME)
#include <sys/timeb.h>
#endif

// Defined in modules/doc/logtree/htm_elm.cpp
int ReplaceEscapes(uni_char* txt, BOOL require_termination, BOOL remove_tabs, BOOL treat_nbsp_as_space);

static const uni_char newline_comma_replacement[3]={2,2,0};

HotlistParser::HotlistParser()
	: m_current_item_valid(TRUE)
	, m_current_node(NULL)
{
}

HotlistParser::~HotlistParser()
{
}

// If more bookmark formats are added to DataFormat enum, they MUST be added here too.
BOOL HotlistParser::IsBookmarkFormat(int format)
{
	return (format == HotlistModel::OperaBookmark ||
			format == HotlistModel::NetscapeBookmark ||
			format == HotlistModel::ExplorerBookmark ||
			format == HotlistModel::KDE1Bookmark ||
			format == HotlistModel::KonquerorBookmark);
}

/*******************************************************************************
 **
 ** ParseHeader
 **
 ** Checks if the file is syncable or not
 **
 *******************************************************************************/

OP_STATUS HotlistParser::ParseHeader(HotlistFileReader &reader, HotlistModel &model, BOOL is_import)
{
	int line_length;
    uni_char *line = reader.Readline(line_length);
	while (!line)
	{
		line = reader.Readline(line_length);
	}

	OpStringC hline(line);
	if (hline.FindI(UNI_L("Hotlist version 2.0")) == KNotFound)
		return OpStatus::ERR;

	line = reader.Readline(line_length);

	// Find next line
	while (!line)
	{
		line = reader.Readline(line_length);
	}

	// Should be Options
	OpStringC sline(line);
	if (sline.FindI(UNI_L("Options:"))!=KNotFound)
	{ 
		if (sline.FindI(UNI_L("sync=NO")) != KNotFound)
		{
			model.SetModelIsSynced(FALSE);
		}
	}
	else
	{
		return OpStatus::ERR;
	}

    return OpStatus::OK;
}


OP_STATUS HotlistParser::Parse(const OpStringC &filename, HotlistModel &model,
							   int parent_index, int format, BOOL is_import, BOOL import_into_root_folder)
{
	if (   (model.GetModelType() == HotlistModel::BookmarkRoot && !IsBookmarkFormat(format))
		|| (model.GetModelType() == HotlistModel::NoteRoot && format != HotlistModel::OperaNote)
		|| (model.GetModelType() == HotlistModel::ContactRoot && format != HotlistModel::OperaContact)
		|| (model.GetModelType() == HotlistModel::UniteServicesRoot && format != HotlistModel::OperaGadget)
		)
	{
		// Don't import contacts, notes, gadgets into bookmarks model
		return OpStatus::ERR;
	}

	if (parent_index != -1)
	{
		if (!model.IsFolderAtIndex(parent_index))
			parent_index = -1;

		HotlistModelItem* hmi = model.GetItemByIndex(parent_index);
		if (hmi && hmi->GetIsInsideTrashFolder())
			parent_index = -1;
	}
	model.SetParentIndex(parent_index);

	// Cleanup from any earlier parsed adr files
	ResetCurrentNode();

	GenericTreeModel::ModelLock lock(&model);

	if (format == HotlistModel::OperaBookmark )
	{
		return g_desktop_bookmark_manager->ImportAdrBookmark(filename.CStr());
	}
	else if( format == HotlistModel::OperaContact || format == HotlistModel::OperaNote || format == HotlistModel::OperaGadget)
	{
		HotlistFileReader reader;
		if (!reader.Init(filename, HotlistFileIO::UTF8))
			return OpStatus::OK;

		/* if (ParseHeader(reader, model, is_import)) == OpStatus::ERR) return OpStatus::ERR; */
		ParseHeader(reader, model, is_import);
		return Parse(reader, model, is_import);
	}
	else if (format == HotlistModel::NetscapeBookmark)
	{
		if (FileIsPlacesSqlite(filename))
			return ImportNetscapeSqlite(filename, !import_into_root_folder);
		return ImportNetscapeHTML(filename, !import_into_root_folder);
	}
	else if (format == HotlistModel::ExplorerBookmark)
	{
		return ImportExplorerFavorites(filename, !import_into_root_folder);
	}
	else if (format == HotlistModel::KDE1Bookmark)
	{
		return ImportKDE1Bookmarks(filename);
	}
	else if (format == HotlistModel::KonquerorBookmark)
	{
		return ImportKonquerorBookmarks(filename);
	}

	return OpStatus::ERR_NOT_SUPPORTED;
}

void HotlistParser::AddItem(HotlistModelItem *item, HotlistModel &_model, BOOL is_import)
{
	OP_ASSERT(_model.GetModelType() != HotlistModel::BookmarkRoot);

	if (_model.GetModelType() == HotlistModel::BookmarkRoot)
		return;

	HotlistGenericModel &model = static_cast<HotlistGenericModel&>(_model);

   	if (item && !item->IsInModel())
	{
		// in two cases, the hotlist model might contain invalid entries (ie referencing an invalid gadget)
		// * the gadget ID stored is wrong
		// * the path to the widget, stored in profile/widgets/widgets.dat is wrong or in an unsupported format
		// these entries are of no use, so we don't want to add those
		if (item->IsUniteService())
		{
			if (static_cast<GadgetModelItem*>(item)->GetOpGadget() == NULL)
			{
				OP_ASSERT(!"Invalid gadget entry!");
				OP_DELETE(item);
				item = NULL;
			}
		}

		if (item)
		{
			// Don't allow duplicate items on import
			HotlistModelItem* added_item = model.AddItem(item, /* allow_dups = */!is_import, /*parsed_item=*/TRUE);
			if (!added_item)
				OP_DELETE(item);

		}
	}
}


HotlistModelItem *HotlistParser::AddAndCreateNew(HotlistModelItem *item, HotlistModel &model, BOOL is_import, OpTypedObject::Type new_type, OP_STATUS &type_status)
{
	OP_ASSERT(model.GetModelType() != HotlistModel::BookmarkRoot);

	AddItem(item, static_cast<HotlistGenericModel&>(model), is_import);
	item = static_cast<HotlistGenericModel&>(model).CreateItem(new_type, type_status, TRUE);
	SetCurrentNode(item);
	return item;
}


OP_STATUS HotlistParser::Parse(HotlistFileReader &reader, HotlistModel &model, BOOL is_import)
{
	int line_length, offset;
	BOOL isFolder  = FALSE;
	BOOL isContact = FALSE;
	BOOL onLinkBar = FALSE; // TRUE when parent is a link folder (pre7 support)

#if defined(MEASURE_LOADTIME)
	struct timeb start;
	ftime(&start);
#endif

	m_current_item_valid = TRUE;

	HotlistModelItem* item = GetCurrentNode();

	if (item && item->GetType() == OpTypedObject::FOLDER_TYPE)
		isFolder  = TRUE;

	//	OpString current_line;
	OP_STATUS type_status;

	while (1)
	{
		uni_char *line = reader.Readline(line_length);

		if (!line)
		{
			break;
		}

		// Skip blank lines
		if (!line[0])
		{
			continue;
		}

		// If the current item does not have a supported type
		// skip the rest of the item, continue until new item reached
		if (!m_current_item_valid)
		{
			if (!IsKey(line, line_length, "#", 1))
			{
				// go to next line
				continue;
			}
			else
			{
				m_current_item_valid = TRUE;
			}
		}

		/**      SECTIONS      **/

		offset = IsKey(line, line_length, KEYWORD_URLSECTION, ARRAY_SIZE(KEYWORD_URLSECTION)-1);
		if (offset != -1)
		{
			OP_ASSERT(FALSE);
			continue;
		}
		offset = IsKey(line, line_length, KEYWORD_NOTESECTION, ARRAY_SIZE(KEYWORD_NOTESECTION)-1);
		if (offset != -1)
		{
			item = AddAndCreateNew(item, model, is_import, OpTypedObject::NOTE_TYPE, type_status);

			if (!item)
			{
				if (type_status == OpStatus::ERR_NOT_SUPPORTED)
				{
					m_current_item_valid = FALSE;
				    continue;
				}
				else
					return type_status;
			}
			if (onLinkBar)
				item->SetOnPersonalbar(TRUE);

			isContact = FALSE;
			isFolder  = FALSE;
			continue;
		}
		offset = IsKey(line, line_length, KEYWORD_SEPERATORSECTION, ARRAY_SIZE(KEYWORD_SEPERATORSECTION)-1);
		if (offset != -1)
		{
			item = AddAndCreateNew(item, model, is_import, OpTypedObject::SEPARATOR_TYPE, type_status);

			if (!item)
			{
				if (type_status == OpStatus::ERR_NOT_SUPPORTED)
				{
					m_current_item_valid = FALSE;
				    continue;
				}
				else
					return type_status;
			}

			isContact = FALSE;
			isFolder  = FALSE;
			continue;
		}
#ifdef WEBSERVER_SUPPORT
		offset = IsKey(line, line_length, KEYWORD_UNITESECTION, ARRAY_SIZE(KEYWORD_UNITESECTION)-1);
		if (offset != -1)
		{

			item = AddAndCreateNew(item, model, is_import, OpTypedObject::UNITE_SERVICE_TYPE, type_status);

			if (!item)
			{
				if (type_status == OpStatus::ERR_NOT_SUPPORTED)
				{
					m_current_item_valid = FALSE;
				    continue;
				}
				else
					return type_status;
			}
			if (onLinkBar)
				item->SetOnPersonalbar(TRUE);

			isContact = FALSE;
			isFolder  = FALSE;
			continue;
		}
#endif // WEBSERVER_SUPPORT
		offset = IsKey(line, line_length, KEYWORD_CONTACTSECTION, ARRAY_SIZE(KEYWORD_CONTACTSECTION)-1);
		if (offset != -1)
		{
			item = AddAndCreateNew(item, model, is_import, OpTypedObject::CONTACT_TYPE, type_status);

			if (!item)
			{
				if (type_status == OpStatus::ERR_NOT_SUPPORTED)
				{
					m_current_item_valid = FALSE;
				    continue;
				}
				else
					return type_status;
			}
			if (onLinkBar)
				item->SetOnPersonalbar(TRUE);

			isContact = TRUE;
			isFolder  = FALSE;
			continue;
		}
		offset = IsKey(line, line_length, KEYWORD_GROUPSECTION, ARRAY_SIZE(KEYWORD_GROUPSECTION)-1);
		if (offset != -1)
		{
			OP_STATUS rc;

			item = AddAndCreateNew(item, model, is_import, OpTypedObject::GROUP_TYPE, rc);

			if (!item)
				return OpStatus::ERR;

			if (onLinkBar)
				item->SetOnPersonalbar(TRUE);

			isContact = FALSE;
			isFolder  = FALSE;
			continue;
		}
		offset = IsKey(line, line_length, KEYWORD_FOLDERSECTION, ARRAY_SIZE(KEYWORD_FOLDERSECTION)-1);
		if (offset != -1)
		{
			OP_STATUS rc;

			item = AddAndCreateNew(item, model, is_import, OpTypedObject::FOLDER_TYPE, rc);

			if (!item)
				return  OpStatus::ERR;

			INT32 parentIndex = model.GetParentIndex();

			if (onLinkBar)
				item->SetOnPersonalbar(TRUE);

			RETURN_IF_ERROR(Parse(reader, model, is_import));
			model.SetParentIndex(parentIndex);
			item = 0;

			isContact = TRUE;
			isFolder  = TRUE;

			continue;
		}
		offset = IsKey(line, line_length, KEYWORD_ENDOFFOLDER, ARRAY_SIZE(KEYWORD_ENDOFFOLDER)-1);
		if (offset != -1)
		{
			AddItem(item, model, is_import);
			return OpStatus::OK;
		}

		/**        KEY - VALUES       **/
		offset = IsKey(line, line_length, KEYWORD_NAME, ARRAY_SIZE(KEYWORD_NAME)-1);
		if (offset != -1)
		{
			if (item)
			{
				if (item->IsNote() || item->IsFolder() && model.GetModelType() == HotlistModel::NoteRoot)
				{
					OpString tmp;
					ConvertToNewline(tmp, &line[5]);
					item->SetName(tmp.CStr());
				}
				else
				{
					item->SetName(&line[5]);
				}
			}
			continue;
		}
		offset = IsKey(line, line_length, KEYWORD_ID, ARRAY_SIZE(KEYWORD_ID)-1);
		if (offset != -1)
		{
			if (item && !is_import)
			{
				//item->SetID(uni_atoi(&line[3]));
			}
			continue;
		}
//		if (!is_import)
//		{
			offset = IsKey(line, line_length, KEYWORD_UNIQUEID, ARRAY_SIZE(KEYWORD_UNIQUEID)-1);
			if (offset != -1)
			{
				if (item)
				{
					// Make upper
					for (UINT32 i = 0; i < uni_strlen(&line[9]); i++)
					{
						line[9+i] = uni_toupper(line[9+i]);
					}
					item->SetUniqueGUID(&line[9], model.GetModelType());
				}
				continue;
			}
//		}
		offset = IsKey(line, line_length, KEYWORD_CREATED, ARRAY_SIZE(KEYWORD_CREATED)-1);
		if (offset != -1)
		{
			if (item)
				item->SetCreated(uni_atoi(&line[8]));

			continue;
		}
		offset = IsKey(line, line_length, KEYWORD_VISITED, ARRAY_SIZE(KEYWORD_VISITED)-1);
		if (offset != -1)
		{
			if (item)
				item->SetVisited(uni_atoi(&line[8]));

			continue;
		}
		offset = IsKey(line, line_length, KEYWORD_URL, ARRAY_SIZE(KEYWORD_URL)-1);
		if (offset != -1)
		{
			if (item)
				item->SetUrl(&line[4]);

			continue;
		}
		offset = IsKey(line, line_length, KEYWORD_DESCRIPTION, ARRAY_SIZE(KEYWORD_DESCRIPTION)-1);
		if (offset != -1)
		{
			if (item)
			{
				OpString tmp;
				ConvertToNewline(tmp, &line[12]);
				item->SetDescription(tmp.CStr());
			}
			continue;
		}
		offset = IsKey(line, line_length, KEYWORD_SHORTNAME, ARRAY_SIZE(KEYWORD_SHORTNAME)-1);
		if (offset != -1)
		{
			if (item)
			{
				OpStringC s(&line[11]);
				if (!is_import || !g_hotlist_manager->HasNickname(s,item))
					item->SetShortName(&line[11]);
			}
			continue;
		}
		offset = IsKey(line, line_length, KEYWORD_PERSONALBAR_POS, ARRAY_SIZE(KEYWORD_PERSONALBAR_POS)-1);
		if (offset != -1)
		{
			if (item && !is_import)
				item->SetPersonalbarPos(uni_atoi(&line[16]));

			continue;
		}
		offset = IsKey(line, line_length, KEYWORD_PANEL_POS, ARRAY_SIZE(KEYWORD_PANEL_POS)-1);
		if (offset != -1)
		{
			if (item && !is_import)
			{
				item->SetPanelPos(uni_atoi(&line[10]));
			}

			continue;
		}
		offset = IsKey(line, line_length, KEYWORD_ON_PERSONALBAR, ARRAY_SIZE(KEYWORD_ON_PERSONALBAR)-1);
		if (offset != -1)
		{
			if (item && !is_import)
				item->SetOnPersonalbar(IsYes(&line[15]));

			continue;
		}
		offset = IsKey(line, line_length, KEYWORD_IN_PANEL, ARRAY_SIZE(KEYWORD_IN_PANEL)-1);
		if (offset != -1)
		{
			if (item && !is_import)
			{
				if (item->GetPanelPos() == -1 && IsYes(&line[9]))
				{
					item->SetPanelPos(0);
					//item->SetInPanel(IsYes(&line[9]));
				}
			}
			continue;
		}
		offset = IsKey(line, line_length, KEYWORD_SMALL_SCREEN, ARRAY_SIZE(KEYWORD_SMALL_SCREEN)-1);
		if (offset != -1)
		{
			if (item)
				item->SetSmallScreen(IsYes(&line[13]));

			continue;
		}
		offset = IsKey(line, line_length, KEYWORD_ACTIVE, ARRAY_SIZE(KEYWORD_ACTIVE)-1);
		if (offset != -1)
		{
			if (item && IsYes(&line[7]) && !is_import)
			{
				model.SetActiveItemId(item->GetID(), FALSE);
			}
			continue;
		}
		offset = IsKey(line, line_length, KEYWORD_MEMBER, ARRAY_SIZE(KEYWORD_MEMBER)-1);
		if (offset != -1)
		{
			if (item)
			{
				item->SetGroupList(&line[8]);

				// TODO JOHAN: if (is_import) { correct_group_ids(); }
			}
			continue;
		}

#ifdef WEBSERVER_SUPPORT
		offset = IsKey(line, line_length, KEYWORD_ROOT, ARRAY_SIZE(KEYWORD_ROOT)-1);
		if (offset != -1)
		{
			if (item && item->IsUniteService() && IsYes(&line[offset]) && !((HotlistGenericModel&)model).GetRootService())
			{
				static_cast<UniteServiceModelItem*>(item)->SetIsRootService();
				//model.SetRootService(	static_cast<UniteServiceModelItem*>(item));
			}
			continue;
		}
		
		offset = IsKey(line, line_length, KEYWORD_NEEDSCONFIG, ARRAY_SIZE(KEYWORD_NEEDSCONFIG)-1);
		if (offset != -1)
		{
			if (item && item->IsUniteService() && IsYes(&line[offset]))
				static_cast<UniteServiceModelItem*>(item)->SetNeedsConfiguration(TRUE);
			continue;
		}

		offset = IsKey(line, line_length, KEYWORD_RUNNING, ARRAY_SIZE(KEYWORD_RUNNING)-1);
		if (offset != -1)
		{
			if (item && item->IsUniteService())
			{
				UniteServiceModelItem * unite_item = static_cast<UniteServiceModelItem*>(item);
				if (!unite_item->NeedsConfiguration() && IsYes(&line[offset]))
				{
					unite_item->SetAddToAutoStart(TRUE);
				}
			}
			continue;
		}
#endif

		//gadget properties reading
		offset = IsKey(line, line_length, KEYWORD_IDENTIFIER, ARRAY_SIZE(KEYWORD_IDENTIFIER)-1);
		if (offset != -1)
			{
				if (item && !is_import)
				{
#ifdef GADGET_SUPPORT
					item->SetGadgetIdentifier(&line[offset]);
#endif // GADGET_SUPPORT
				}
				continue;
			}

		//gadget type of installation
		offset = IsKey(line, line_length, KEYWORD_CLEAN_UNINSTALL, ARRAY_SIZE(KEYWORD_CLEAN_UNINSTALL)-1);
		if (offset != -1)
			{
				if (item && !is_import)
				{
#ifdef GADGET_SUPPORT
					item->SetGadgetCleanUninstall(IsYes(&line[offset]));
#endif // GADGET_SUPPORT
				}
				continue;
			}
		//folder property reading
		if (isFolder)
		{

			offset = IsKey(line, line_length, KEYWORD_EXPANDED, ARRAY_SIZE(KEYWORD_EXPANDED)-1);
			if (offset != -1)
			{
				if (item)
					item->SetIsExpandedFolder(IsYes(&line[9]));

				continue;
			}

			if (model.GetModelType() == HotlistModel::BookmarkRoot)
			{
				offset = IsKey(line, line_length, KEYWORD_TARGET, ARRAY_SIZE(KEYWORD_TARGET)-1);
				if (offset != -1)
				{
					if (item)
						item->SetTarget(&line[7]);

					continue;
				}
				offset = IsKey(line, line_length, KEYWORD_MOVE_IS_COPY, ARRAY_SIZE(KEYWORD_MOVE_IS_COPY)-1);
				if (offset != -1)
				{
					if (item)
						item->SetHasMoveIsCopy(IsYes(&line[13]));

					continue;
				}
				offset = IsKey(line, line_length, KEYWORD_DELETABLE, ARRAY_SIZE(KEYWORD_DELETABLE)-1);
				if (offset != -1)
				{
					if (item)
						item->SetIsDeletable(!IsNo(&line[10]));

					continue;
				}
				offset = IsKey(line, line_length, KEYWORD_SUBFOLDER_ALLOWED, ARRAY_SIZE(KEYWORD_SUBFOLDER_ALLOWED)-1);
				if (offset != -1)
				{
					if (item)
						item->SetSubfoldersAllowed(!IsNo(&line[18]));

					continue;
				}
				offset = IsKey(line, line_length, KEYWORD_SEPARATOR_ALLOWED, ARRAY_SIZE(KEYWORD_SEPARATOR_ALLOWED)-1);
				if (offset != -1)
				{
					if (item)
						item->SetSeparatorsAllowed(!IsNo(&line[18]));

					continue;
				}
				offset = IsKey(line, line_length, KEYWORD_MAX_ITEMS, ARRAY_SIZE(KEYWORD_MAX_ITEMS)-1);
				if (offset != -1)
				{
					if (item)
						item->SetMaxItems((uni_atoi(&line[10])));

					continue;
				}
			}

			offset = IsKey(line, line_length, KEYWORD_TRASH_FOLDER, ARRAY_SIZE(KEYWORD_TRASH_FOLDER)-1);
			if (offset != -1)
			{
				/* If import; add all items inside trash folder to real trash folder  */
				if (item)
				{
					if (is_import)
					{
						HotlistModelItem* trash_folder = model.GetTrashFolder();
						if (trash_folder)
						{
							OP_ASSERT(item->IsFolder());
							model.SetParentIndex(trash_folder->GetIndex());	// reset m_parent_index

							OP_ASSERT(item == model.GetCurrentNode());
							// GetTrashFolder will create trash folder if it's not already there.

							ResetCurrentNode();
							OP_DELETE(item);
							item = NULL;
						}
					}
					else
					{
						BOOL is_trash = IsYes(&line[13]);
						item->SetIsTrashFolder(is_trash);
						if (is_trash)
						{
							model.SetTrashfolderId(item->GetID());
						}
					}
				}
				continue;
			}
#if defined(SUPPORT_IMPORT_LINKBAR_TAG)
			offset = IsKey(line, line_length, KEYWORD_LINKBAR_FOLDER, ARRAY_SIZE(KEYWORD_LINKBAR_FOLDER)-1);
			if (offset != -1)
			{
				if (item && !is_import)
				{
					item->SetIsLinkBarFolder(IsYes(&line[15]));
					onLinkBar = TRUE;
				}
				continue;
			}
			offset = IsKey(line, line_length, KEYWORD_LINKBAR_STOP, ARRAY_SIZE(KEYWORD_LINKBAR_STOP)-1);
			if (offset != -1)
			{
				if (item && !is_import)
				{
					onLinkBar = FALSE;
				}
				continue;
			}
#endif
		}
		if (isContact)
		{
			offset = IsKey(line, line_length, KEYWORD_MAIL, ARRAY_SIZE(KEYWORD_MAIL)-1);
			if (offset != -1)
			{
				if (item)
				{
					// Pre 7 versions used 0x020x02 as a separator in the file
					OpString tmp;
					ConvertToComma(tmp, &line[5]);
					item->SetMail(tmp.CStr());
				}
				continue;
			}
			offset = IsKey(line, line_length, KEYWORD_PHONE, ARRAY_SIZE(KEYWORD_PHONE)-1);
			if (offset != -1)
			{
				if (item)
					item->SetPhone(&line[6]);

				continue;
			}
			offset = IsKey(line, line_length, KEYWORD_FAX, ARRAY_SIZE(KEYWORD_FAX)-1);
			if (offset != -1)
			{
				if (item)
					item->SetFax(&line[4]);

				continue;
			}
			offset = IsKey(line, line_length, KEYWORD_POSTALADDRESS, ARRAY_SIZE(KEYWORD_POSTALADDRESS)-1);
			if (offset != -1)
			{
				if (item)
				{
					OpString tmp;
					ConvertToNewline(tmp, &line[14]);
					item->SetPostalAddress(tmp.CStr());
				}
				continue;
			}
			offset = IsKey(line, line_length, KEYWORD_PICTUREURL, ARRAY_SIZE(KEYWORD_PICTUREURL)-1);
			if (offset != -1)
			{
				if (item)
					item->SetPictureUrl(&line[11]);

				continue;
			}
			offset = IsKey(line, line_length, KEYWORD_CONAX, ARRAY_SIZE(KEYWORD_CONAX)-1);
			if (offset != -1)
			{
				if (item)
					item->SetConaxNumber(&line[22]);

				continue;
			}
			offset = IsKey(line, line_length, KEYWORD_ICON, ARRAY_SIZE(KEYWORD_ICON)-1);
			if (offset != -1)
			{
				if (item)
				{
					OpString8 icon_name;
					icon_name.Set(&line[5]);
					item->SetIconName(icon_name.CStr());
				}
				continue;
			}
			if (line[0] == 'I' && line[1] == 'M')
			{
				offset = IsKey(line, line_length, KEYWORD_IMADDRESS, ARRAY_SIZE(KEYWORD_IMADDRESS)-1);
				if (offset != -1)
				{
					if (item)
						item->SetImAddress(&line[10]);

					continue;
				}
				offset = IsKey(line, line_length, KEYWORD_IMPROTOCOL, ARRAY_SIZE(KEYWORD_IMPROTOCOL)-1);
				if (offset != -1)
				{
					if (item)
						item->SetImProtocol(&line[11]);

					continue;
				}
				offset = IsKey(line, line_length, KEYWORD_IMSTATUS, ARRAY_SIZE(KEYWORD_IMSTATUS)-1);
				if (offset != -1)
				{
					if (item)
						item->SetImStatus(&line[9]);

					continue;
				}
				offset = IsKey(line, line_length, KEYWORD_IMVISIBILITY, ARRAY_SIZE(KEYWORD_IMVISIBILITY)-1);
				if (offset != -1)
				{
					if (item)
						item->SetImVisibility(&line[13]);

					continue;
				}
				offset = IsKey(line, line_length, KEYWORD_IMXPOSITION, ARRAY_SIZE(KEYWORD_IMXPOSITION)-1);
				if (offset != -1)
				{
					if (item)
						item->SetImXPosition(&line[12]);

					continue;
				}
				offset = IsKey(line, line_length, KEYWORD_IMYPOSITION, ARRAY_SIZE(KEYWORD_IMYPOSITION)-1);
				if (offset != -1)
				{
					if (item)
						item->SetImYPosition(&line[12]);

					continue;
				}

				offset = IsKey(line, line_length, KEYWORD_IMWIDTH, ARRAY_SIZE(KEYWORD_IMWIDTH)-1);
				if (offset != -1)
				{
					if (item)
						item->SetImWidth(&line[8]);

					continue;
				}
				offset = IsKey(line, line_length, KEYWORD_IMHEIGHT, ARRAY_SIZE(KEYWORD_IMHEIGHT)-1);
				if (offset != -1)
				{
					if (item)
						item->SetImHeight(&line[9]);

					continue;
				}
				offset = IsKey(line, line_length, KEYWORD_IMMAXIMIZED, ARRAY_SIZE(KEYWORD_IMMAXIMIZED)-1);
				if (offset != -1)
				{
					if (item)
						item->SetImMaximized(&line[12]);

					continue;
				}
			}
			offset = IsKey(line, line_length, KEYWORD_M2INDEX, ARRAY_SIZE(KEYWORD_M2INDEX)-1);
			if (offset != -1)
			{
				if (item && !is_import)
					item->SetM2IndexId(uni_atoi(&line[10]));

				continue;
			}
		}

#if defined(SUPPORT_UNKNOWN_TAGS)
		// If line did not contain any key, we end up here.
	if (item && uni_strlen(line) > 0)
		{
			OpString formattedTag;
			formattedTag.Set("\t");
			formattedTag.Append(line);
			formattedTag.Append("\n");
			item->SetUnknownTag(formattedTag.CStr());
		}
#endif
	} // end while

#if defined(MEASURE_LOADTIME)
	struct timeb stop;
	ftime(&stop);
	int s1 = start.time * 1000 + start.millitm;
	int s2  = stop.time * 1000 + stop.millitm;
	printf("bookmark parsing: duration=%d [ms], nodes=%d\n", s2 - s1,
			model.GetItemCount());
#endif

	AddItem(item, model, is_import);

	return OpStatus::OK;
}

#if 0
OP_STATUS HotlistParser::MergeFromFile(const OpStringC &filename, HotlistModel &existing_model)
{
	HotlistModel work_model(1, TRUE);
	HotlistParser parser;
	UINT32 dirty_count = 0;

	if (OpStatus::IsSuccess(parser.Parse(filename, work_model, -1, HotlistModel::OperaBookmark, TRUE, dirty_count)))
	{
		//we now have the merge file in tmp
		CheckLevelForDuplicates(NULL, NULL, work_model, existing_model);
	}
	existing_model.PasteItem(work_model, NULL, INSERT_INTO, 0);
	existing_model.SetDirty(TRUE);
	return OpStatus::OK;
}
#endif

// this function needs to be reimplemented if needed due to the refactoring of bookmarks. --shuais
//OP_STATUS HotlistParser::CheckLevelForDuplicates(HotlistModelItem *item, HotlistModelItem *parent_item, HotlistModel &_work_model, HotlistModel &_existing_model)
//{
//	OP_ASSERT(_work_model.GetModelType() != HotlistModel::BookmarkRoot && _existing_model.GetModelType() != HotlistModel::BookmarkRoot);
//	if(_work_model.GetModelType() == HotlistModel::BookmarkRoot)
//		return OpStatus::ERR;
//
//	HotlistGenericModel& work_model		= static_cast<HotlistGenericModel&>(_work_model);
//	HotlistGenericModel& existing_model = static_cast<HotlistGenericModel&>(_existing_model);
//	
//
//	if(item && parent_item)	//this is a folder, find the "correpsonding folder" in the models and merge the contents
//	{
//		HotlistModelItem* new_child;
//		HotlistModelItem* existing_child;
//
//		OpINT32Vector newfolder_list;
//		OpINT32Vector permanentfolder_list;
//
//		INT32 idx = work_model.GetIndexByItem(item);
//
//		work_model.GetIndexList(idx, newfolder_list, FALSE, 1, OpTypedObject::BOOKMARK_TYPE);
//		existing_model.GetIndexList(existing_model.GetIndexByItem(parent_item), permanentfolder_list, FALSE, 1, OpTypedObject::BOOKMARK_TYPE);
//
//		BOOL found = FALSE;
//
//		UINT32 k;
//		for (k=0; k<newfolder_list.GetCount(); k++)
//		{
//			new_child = work_model.GetItemByIndex(newfolder_list.Get(k));
//
//			UINT32 n;
//			for (n=0; n<permanentfolder_list.GetCount(); n++)
//			{
//				existing_child = existing_model.GetItemByIndex(permanentfolder_list.Get(n));
//
//				if(new_child->GetName().Compare(existing_child->GetName()) == 0)
//				{
//					found = TRUE;
//					break;
//				}
//			}
//
//			if(!found)
//			{
//				// add the item to the existing model at the position of the parent_item and break
//				BookmarkModel paste_model(TRUE); 
//				INT32 gotidx;
//
//				HotlistModelItem* added = new_child->GetDuplicate();
//				if (added)
//				{
//					paste_model.AddLastBeforeTrash(-1, added, &gotidx);
//
//					existing_model.PasteItem(paste_model, parent_item, INSERT_INTO, 0);
//				}
//			}
//			found = FALSE;
//		}
//
//		work_model.DeleteItem(item, FALSE);
//		item=NULL;
//		return OpStatus::OK;
//	}
//
//	for (int i = 0; i<work_model.GetItemCount();)
//	{
//		HotlistModelItem *hmi = work_model.GetItemByIndex(i++);
//
//		for (int j = 0; j<existing_model.GetItemCount();)
//		{
//			HotlistModelItem *permanent_hmi = existing_model.GetItemByIndex(j++);
//			{
//				if(permanent_hmi && hmi && hmi->GetName().Compare(permanent_hmi->GetName()) == 0)	//they have the same name
//				{
//					if(hmi->IsFolder() && !permanent_hmi->GetIsTrashFolder())
//					{
//						return CheckLevelForDuplicates(hmi, permanent_hmi, work_model, existing_model);
//					}
//					work_model.DeleteItem(hmi, FALSE);
//					i=0;
//					break;
//				}
//			}
//		}
//	}
//
//	return OpStatus::OK;
//}

#if SQLITE_VERSION_NUMBER >= 3007000
#define SQLITE_VERSION_VFS 2
#define SQLITE_VERSION_IO 2
#else
#define SQLITE_VERSION_VFS 1
#define SQLITE_VERSION_IO 1
#endif

//This entire class has been created for it's fileLock method.
//Otherwise, it's bunch of pass-throughs
struct SQLite3VFS
{
	sqlite3_vfs copy;
	sqlite3_vfs* os;
	sqlite3_io_methods methods;
	char name[200];

	struct SQLite3File: sqlite3_file
	{
		sqlite3_file* orig;
	};

	SQLite3VFS()
	{
		unsigned long secs, millisecs;
		g_op_time_info->GetWallClock(secs, millisecs);

		op_snprintf(name, 200, "op_vfs_%lu_%lu", secs, millisecs);

		memset(&copy, 0, sizeof(copy));
		os = sqlite3_vfs_find(NULL);
		if (!os) return;
		memcpy(&copy, os, sizeof(copy));
		copy.iVersion = SQLITE_VERSION_VFS;
		copy.szOsFile = sizeof(SQLite3File);
		copy.zName = name;
		copy.pAppData = this;
		copy.xOpen = vfsOpen;
		copy.xDelete = vfsDelete;
		copy.xAccess = vfsAccess;
		copy.xFullPathname = vfsFullPathname;
		copy.xDlOpen = vfsDlOpen;
		copy.xDlError = vfsDlError;
		copy.xDlSym = vfsDlSym;
		copy.xDlClose = vfsDlClose;
		copy.xRandomness = vfsRandomness;
		copy.xSleep = vfsSleep;
		copy.xCurrentTime = vfsCurrentTime;
		copy.xGetLastError = vfsGetLastError;
#if SQLITE_VERSION_NUMBER >= 3007000
		copy.xCurrentTimeInt64 = vfsCurrentTimeInt64;
#endif
		sqlite3_vfs_register(&copy, 0);

		methods.iVersion = SQLITE_VERSION_IO;
		methods.xClose = fileClose;
		methods.xRead = fileRead;
		methods.xWrite = fileWrite;
		methods.xTruncate = fileTruncate;
		methods.xSync = fileSync;
		methods.xFileSize = fileFileSize;
		methods.xLock = fileLock;
		methods.xUnlock = fileUnlock;
		methods.xCheckReservedLock = fileCheckReservedLock;
		methods.xFileControl = fileFileControl;
		methods.xSectorSize = fileSectorSize;
		methods.xDeviceCharacteristics = fileDeviceCharacteristics;
#if SQLITE_VERSION_NUMBER >= 3007000
		methods.xShmMap = fileShmMap;
		methods.xShmLock = fileShmLock;
		methods.xShmBarrier = fileShmBarrier;
		methods.xShmUnmap = fileShmUnmap;
#endif
	}
	~SQLite3VFS() { sqlite3_vfs_unregister(&copy); }
	const char* Name() const { return copy.zName; }

private:
#define OS (((SQLite3VFS*)_this->pAppData)->os)
	static int vfsOpen(sqlite3_vfs* _this, const char *zName, sqlite3_file* f, int flags, int *pOutFlags)
	{
		void * ptr = op_malloc(OS->szOsFile);
		if (!ptr) return SQLITE_NOMEM;
		memset(ptr, OS->szOsFile, 0);

		SQLite3File* file = (SQLite3File*)f;
		file->pMethods = NULL;
		file->orig = (sqlite3_file*)ptr;

		int ret = OS->xOpen(OS, zName, file->orig, flags, pOutFlags);
		if (ret != SQLITE_OK || file->orig->pMethods == NULL)
		{
			op_free(ptr);
			file->orig = NULL;
			if (ret == SQLITE_OK)
				ret = SQLITE_ERROR;
			return ret;
		}
		file->pMethods = &((SQLite3VFS*)_this->pAppData)->methods;
		return ret;
	}

	static int vfsDelete(sqlite3_vfs* _this, const char *zName, int syncDir) { return OS->xDelete(OS, zName, syncDir); }
	static int vfsAccess(sqlite3_vfs* _this, const char *zName, int flags, int *pResOut) { return OS->xAccess(OS, zName, flags, pResOut); }
	static int vfsFullPathname(sqlite3_vfs* _this, const char *zName, int nOut, char *zOut) { return OS->xFullPathname(OS, zName, nOut, zOut); }
	static void *vfsDlOpen(sqlite3_vfs* _this, const char *zFilename) { return OS->xDlOpen(OS, zFilename); }
	static void vfsDlError(sqlite3_vfs* _this, int nByte, char *zErrMsg) { return OS->xDlError(OS, nByte, zErrMsg); }
	static void (*vfsDlSym(sqlite3_vfs* _this, void* ptr, const char *zSymbol))(void) { return OS->xDlSym(OS, ptr, zSymbol); }
	static void vfsDlClose(sqlite3_vfs* _this, void* ptr) { return OS->xDlClose(OS, ptr); }
	static int vfsRandomness(sqlite3_vfs* _this, int nByte, char *zOut) { return OS->xRandomness(OS, nByte, zOut); }
	static int vfsSleep(sqlite3_vfs* _this, int microseconds) { return OS->xSleep(OS, microseconds); }
	static int vfsCurrentTime(sqlite3_vfs* _this, double* out) { return OS->xCurrentTime(OS, out); }
	static int vfsGetLastError(sqlite3_vfs* _this, int i, char * ptr) { return OS->xGetLastError(OS, i, ptr); }
#if SQLITE_VERSION_NUMBER >= 3007000
	static int vfsCurrentTimeInt64(sqlite3_vfs* _this, sqlite3_int64* ptr) { return OS->xCurrentTimeInt64(OS, ptr); }
#endif

#undef OS

#define ORIG (((SQLite3File*)file)->orig)
#define METHODS (ORIG->pMethods)
	static int fileClose(sqlite3_file* file)
	{
		int ret = METHODS->xClose(ORIG);
		op_free(ORIG);
		return ret;
	}

	static int fileRead(sqlite3_file* file, void* p, int iAmt, sqlite3_int64 iOfst) { return METHODS->xRead(ORIG, p, iAmt, iOfst); }
	static int fileWrite(sqlite3_file* file, const void* p, int iAmt, sqlite3_int64 iOfst) { return METHODS->xWrite(ORIG, p, iAmt, iOfst); }
	static int fileTruncate(sqlite3_file* file, sqlite3_int64 size) { return METHODS->xTruncate(ORIG, size); }
	static int fileSync(sqlite3_file* file, int flags) { return METHODS->xSync(ORIG, flags); }
	static int fileFileSize(sqlite3_file* file, sqlite3_int64 *pSize) { return METHODS->xFileSize(ORIG, pSize); }

	static int fileLock(sqlite3_file* file, int i)
	{
		int ret = METHODS->xLock(ORIG, i);
		if (ret == SQLITE_BUSY)
			ret = SQLITE_OK;
		return ret;
	}

	static int fileUnlock(sqlite3_file* file, int i) { return METHODS->xUnlock(ORIG, i); }
	static int fileCheckReservedLock(sqlite3_file* file, int *pResOut) { return METHODS->xCheckReservedLock(ORIG, pResOut); }
	static int fileFileControl(sqlite3_file* file, int op, void *pArg) { return METHODS->xFileControl(ORIG, op, pArg); }
	static int fileSectorSize(sqlite3_file* file) { return METHODS->xSectorSize(ORIG); }
	static int fileDeviceCharacteristics(sqlite3_file* file) { return METHODS->xDeviceCharacteristics(ORIG); }
#if SQLITE_VERSION_NUMBER >= 3007000
	static int fileShmMap(sqlite3_file* file, int iPg, int pgsz, int i, void volatile** shm) { return METHODS->xShmMap(ORIG, iPg, pgsz, i, shm); }
	static int fileShmLock(sqlite3_file* file, int offset, int n, int flags) { return METHODS->xShmLock(ORIG, offset, n, flags); }
	static void fileShmBarrier(sqlite3_file* file) { return METHODS->xShmBarrier(ORIG); }
	static int fileShmUnmap(sqlite3_file* file, int deleteFlag) { return METHODS->xShmUnmap(ORIG, deleteFlag); }
#endif

#undef METHODS
#undef ORIG
};

#define SQLITE3_RETURN_IF_ERROR(expr) \
	do { \
		int RETURN_IF_ERROR_TMP = expr; \
		if (RETURN_IF_ERROR_TMP != SQLITE_OK) \
			return RETURN_IF_ERROR_TMP == SQLITE_NOMEM ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR; \
	} while (0)

class SQLite3Cursor;
class SQLite3DB
{
	friend class SQLite3Cursor;
	sqlite3* db;
public:
	SQLite3DB(): db(NULL) {}
	~SQLite3DB() { Close(); }
	void Close() { if (db) sqlite3_close(db); db = NULL; }
	OP_STATUS Open(const char* filename_u8, const char* vfsname)
	{
		OP_ASSERT(!db);
		SQLITE3_RETURN_IF_ERROR(sqlite3_open_v2(filename_u8, &db, SQLITE_OPEN_READONLY, vfsname));
		return OpStatus::OK;
	}
};

class SQLite3Cursor
{
	sqlite3_stmt* stmt;
	OP_STATUS last_step_result;
public:
	SQLite3Cursor(): stmt(NULL), last_step_result(0) {}
	void Close() { if (stmt) sqlite3_finalize(stmt); stmt = NULL; }
	OP_STATUS Open(const SQLite3DB& db, const char* sql)
	{
		OP_ASSERT(!stmt);
		OP_ASSERT(db.db);
		SQLITE3_RETURN_IF_ERROR(sqlite3_prepare_v2(db.db, sql, -1, &stmt, NULL));
		return OpStatus::OK;
	}

	BOOL Step()
	{
		OP_ASSERT(stmt);
		int ret = sqlite3_step(stmt);
		last_step_result =
			ret == SQLITE_ROW || ret == SQLITE_OK || ret == SQLITE_DONE ? OpStatus::OK :
			ret == SQLITE_NOMEM ? OpStatus::ERR_NO_MEMORY :
			OpStatus::ERR;
		return ret == SQLITE_ROW;
	}
	OP_STATUS StepStatus() const { return last_step_result; }

	int TupleSize() const { return sqlite3_column_count(stmt); }
	int Type(int column) const { return sqlite3_column_type(stmt, column); }
	int GetInt(int column) const { return sqlite3_column_int(stmt, column); }
	const uni_char* GetRawText(int column) const { return (const uni_char*)sqlite3_column_text16(stmt, column); }
	int GetRawTextLen(int column) const { return sqlite3_column_bytes16(stmt, column); }
	const char* GetRawText8(int column) const { return (const char*)sqlite3_column_text(stmt, column); }
	int GetRawTextLen8(int column) const { return sqlite3_column_bytes(stmt, column); }
	OP_STATUS GetText(int column, OpString& string) const
	{
		const uni_char* text = GetRawText(column);
		return string.Set(text, GetRawTextLen(column));
	}
};

/* SQLite seems to be overzealous in opening databases even when
 * they are opened with SQLITE_OPEN_READONLY. To check that this
 * seems to be places.sqlite, we'll do little duck typing:
 * does the database contain needed tables? (If it's not the
 * SQLite DB, then there will be no schema...)
 */
BOOL HotlistParser::FileIsPlacesSqlite(const OpStringC& filename)
{
	struct _ {
		static OP_STATUS isPlacesSqlite(const OpStringC& filename)
		{
			SQLite3VFS overwrite;
			SQLite3DB db;
			SQLite3Cursor cursor;

			const int MOZ_TABLES_COUNT = 5;
			const char* MOZ_TABLES_SQL = "select count() from sqlite_master where type='table' and ("
				"name = 'moz_bookmarks' or "
				"name = 'moz_places' or "
				"name = 'moz_annos' or "
				"name = 'moz_items_annos' or "
				"name = 'moz_bookmarks_roots')";

			OpString8 path;
			RETURN_IF_ERROR(path.SetUTF8FromUTF16(filename));
			RETURN_IF_ERROR(db.Open(path.CStr(), overwrite.Name()));
			RETURN_IF_ERROR(cursor.Open(db, MOZ_TABLES_SQL));
			RETURN_IF_ERROR(cursor.Step() ? OpStatus::OK : cursor.StepStatus());
			if (cursor.TupleSize() != 1 || cursor.Type(0) != SQLITE_INTEGER)
				return OpStatus::ERR;

			return cursor.GetInt(0) == MOZ_TABLES_COUNT ? OpStatus::OK : OpStatus::ERR;
		}
	};
	return OpStatus::IsSuccess(_::isPlacesSqlite(filename));
}

/*
 SELECT
	moz_bookmarks.id,
	moz_bookmarks.type,
	moz_bookmarks.parent,
	moz_bookmarks.position,
	moz_bookmarks.title,
	moz_bookmarks.dateAdded,
	moz_bookmarks.lastModified,
	moz_places.last_visit_date,
	moz_places.url,
	annotation_1.content as feed,
	annotation_2.content as site,
	annotation_3.content as description,
	ifnull(moz_places.hidden, 0) as hidden

 FROM
	moz_bookmarks

 LEFT JOIN
	moz_places
	ON moz_bookmarks.fk = moz_places.id
 LEFT JOIN
	(SELECT item_id, content FROM moz_items_annos LEFT JOIN moz_anno_attributes ON moz_items_annos.anno_attribute_id = moz_anno_attributes.id WHERE name = 'livemark/feedURI') AS annotation_1
	ON annotation_1.item_id = moz_bookmarks.id
 LEFT JOIN
	(SELECT item_id, content FROM moz_items_annos LEFT JOIN moz_anno_attributes ON moz_items_annos.anno_attribute_id = moz_anno_attributes.id WHERE name = 'livemark/siteURI') AS annotation_2
	ON annotation_2.item_id = moz_bookmarks.id
 LEFT JOIN
	(SELECT item_id, content FROM moz_items_annos LEFT JOIN moz_anno_attributes ON moz_items_annos.anno_attribute_id = moz_anno_attributes.id WHERE name = 'bookmarkProperties/description') AS annotation_3
	ON annotation_3.item_id = moz_bookmarks.id

 ORDER BY parent, position
*/

#define MOZ_PLACES_ATTRS "moz_bookmarks.id, " \
"moz_bookmarks.type, " \
"moz_bookmarks.parent, " \
"moz_bookmarks.position, " \
"moz_bookmarks.title, " \
"moz_bookmarks.dateAdded, " \
"moz_bookmarks.lastModified, " \
"moz_places.last_visit_date, " \
"moz_places.url, " \
"annotation_1.content as feed, " \
"annotation_2.content as site, " \
"annotation_3.content as description, " \
"ifnull(moz_places.hidden, 0) as hidden"

#define MOZ_PLACES_ANNOTATION_VIEW(anno) "select item_id, content from moz_items_annos left join moz_anno_attributes on moz_items_annos.anno_attribute_id = moz_anno_attributes.id where name = '" anno "'"

#define MOZ_PLACES_JOIN_PLACES "left join moz_places on moz_bookmarks.fk = moz_places.id"
#define MOZ_PLACES_JOIN_ANNOTATION(id, anno) "left join (" MOZ_PLACES_ANNOTATION_VIEW(anno) ") as annotation_" id " on annotation_" id ".item_id = moz_bookmarks.id"

#define MOZ_PLACES_SQL "select " MOZ_PLACES_ATTRS \
" from moz_bookmarks " MOZ_PLACES_JOIN_PLACES \
" " MOZ_PLACES_JOIN_ANNOTATION("1", "livemark/feedURI") \
" " MOZ_PLACES_JOIN_ANNOTATION("2", "livemark/siteURI") \
" " MOZ_PLACES_JOIN_ANNOTATION("3", "bookmarkProperties/description") \
" order by parent, position"

enum
{
	MOZ_ID,
	MOZ_TYPE,
	MOZ_PARENT,
	MOZ_POSITION,
	MOZ_TITLE,
	MOZ_CTIME,
	MOZ_MTIME,
	MOZ_ATIME,
	MOZ_URL,
	MOZ_FEED,
	MOZ_SITE,
	MOZ_DESCRIPTION,
	MOZ_HIDDEN,
	MOZ_COLUMN_COUNT
};

struct MozPlacesBookmark: ListElement<MozPlacesBookmark>
{
	INT32 m_id;
	INT32 m_type;
	INT32 m_parent;
	INT32 m_position;
	OpString m_title;
	OpString m_url;
	OpString m_feed;
	OpString m_site;
	OpString m_description;
	INT32 m_ctime;
	INT32 m_mtime;
	INT32 m_atime;
	OpVector<MozPlacesBookmark> m_children;

	enum
	{
		MOZ_PLACES_TYPE_LINK = 1,
		MOZ_PLACES_TYPE_FOLDER = 2,
		MOZ_PLACES_TYPE_SEPARATOR = 3
	};

	MozPlacesBookmark()
		: m_id(0)
		, m_type(0)
		, m_parent(0)
		, m_position(0)
		, m_ctime(0)
		, m_mtime(0)
		, m_atime(0)
	{
	}

	BOOL HasChildren() const { return m_children.GetCount() > 0; }

	OP_STATUS ReadTuple(const SQLite3Cursor& bookmarks, unsigned long &feed_count)
	{
		m_id       = bookmarks.GetInt(MOZ_ID);
		m_type     = bookmarks.GetInt(MOZ_TYPE);
		m_parent   = bookmarks.GetInt(MOZ_PARENT);
		m_position = bookmarks.GetInt(MOZ_POSITION);
		m_ctime    = bookmarks.GetInt(MOZ_CTIME);
		m_mtime    = bookmarks.GetInt(MOZ_MTIME);
		m_atime    = bookmarks.GetInt(MOZ_ATIME);

		RETURN_IF_ERROR(bookmarks.GetText(MOZ_TITLE,       m_title));
		RETURN_IF_ERROR(bookmarks.GetText(MOZ_URL,         m_url));
		RETURN_IF_ERROR(bookmarks.GetText(MOZ_FEED,        m_feed));
		RETURN_IF_ERROR(bookmarks.GetText(MOZ_SITE,        m_site));
		RETURN_IF_ERROR(bookmarks.GetText(MOZ_DESCRIPTION, m_description));

		if (m_feed.HasContent())
			feed_count++;

		return OpStatus::OK;
	}

	OP_STATUS AppendChild(MozPlacesBookmark* child) { return m_children.Add(child); }

	BookmarkItem* LookForFeeds(BookmarkItem* parent, BookmarkItem* after)
	{
		INT32 count = m_children.GetCount();
		for (INT32 i = 0; i < count; ++i)
			after = m_children.Get(i)->LookForFeed(parent, after);
		return after;
	}

	BookmarkItem* LookForFeed(BookmarkItem* parent, BookmarkItem* after)
	{
		if (m_type == MOZ_PLACES_TYPE_FOLDER)
		{
			if (m_feed.HasContent()) //Livemark folder...
				return ImportLink(parent, after, m_title, m_feed, m_description, m_ctime, m_mtime, m_atime);
			return LookForFeeds(parent, after);
		}

		return after;
	}

	BookmarkItem* ImportChildren(BookmarkItem* parent, BookmarkItem* after)
	{
		INT32 count = m_children.GetCount();
		for (INT32 i = 0; i < count; ++i)
			after = m_children.Get(i)->Import(parent, after);
		return after;
	}

	BookmarkItem* Import(BookmarkItem* parent, BookmarkItem* after)
	{
		if (m_type == MOZ_PLACES_TYPE_FOLDER)
		{
			//LivemarkImportStrategy removed on 2010-09-22

			BookmarkItem* local = NULL;
			if (m_feed.HasContent()) //Livemark folder...
			{
				BookmarkItem* child = after;
				local = parent;
				if (HasChildren())
				{
					child = NULL;
					local = ImportFolder(parent, after, m_title, m_description, m_ctime, m_mtime, m_atime);
					child = ImportChildren(local, child);
					if (child) child = ImportSeparator(local, child);
				}

				child = ImportLink(local, child, m_title, m_site, m_description, m_ctime, m_mtime, m_atime);

				if (!HasChildren())
				{
					//in case there was no sub-menu, we must properly move the current last to the most recent "child" added...
					local = child;
				}
			}
			else //normal folders
			{
				local = ImportFolder(parent, after, m_title, m_description, m_ctime, m_mtime, m_atime);
				ImportChildren(local, NULL);
			}
			return local;

		}

		if (m_type == MOZ_PLACES_TYPE_LINK)
			return ImportLink(parent, after, m_title, m_url, m_description, m_ctime, m_mtime, m_atime);

		if (m_type == MOZ_PLACES_TYPE_SEPARATOR)
			return ImportSeparator(parent, after);

		return NULL;
	}

	static BookmarkItem* ImportFolder(BookmarkItem* parent, BookmarkItem* after, const OpString& title, const OpString& description, INT32 ctime, INT32 mtime, INT32 atime)
	{
		BookmarkItem* item = OP_NEW(BookmarkItem, ());
		if (!item) return NULL;
		item->SetFolderType(FOLDER_NORMAL_FOLDER);
		BOOL succeeded = TRUE;
		if (succeeded) succeeded = OpStatus::IsSuccess(SetAttribute(item, BOOKMARK_TITLE,       title.CStr()));
		if (succeeded) succeeded = OpStatus::IsSuccess(SetAttribute(item, BOOKMARK_DESCRIPTION, description.CStr()));
		if (succeeded) succeeded = OpStatus::IsSuccess(SetAttribute(item, BOOKMARK_CREATED,     ctime));
		if (succeeded) succeeded = OpStatus::IsSuccess(SetAttribute(item, BOOKMARK_VISITED,     atime));
		if (succeeded) succeeded = OpStatus::IsSuccess(DesktopBookmarkManager::AddCoreItem(item, after, parent));
		if (!succeeded)
		{
			OP_DELETE(item);
			return NULL;
		}
		return item;
	}

	static BookmarkItem* ImportLink(BookmarkItem* parent, BookmarkItem* after, const OpString& title, const OpString& url, const OpString& description, INT32 ctime, INT32 mtime, INT32 atime)
	{
		BookmarkItem* item = OP_NEW(BookmarkItem, ());
		if (!item) return after;
		item->SetFolderType(FOLDER_NO_FOLDER);
		BOOL succeeded = TRUE;
		if (succeeded) succeeded = OpStatus::IsSuccess(SetAttribute(item, BOOKMARK_TITLE,       title.CStr()));
		if (succeeded) succeeded = OpStatus::IsSuccess(SetAttribute(item, BOOKMARK_URL,         url.CStr()));
		if (succeeded) succeeded = OpStatus::IsSuccess(SetAttribute(item, BOOKMARK_DESCRIPTION, description.CStr()));
		if (succeeded) succeeded = OpStatus::IsSuccess(SetAttribute(item, BOOKMARK_CREATED,     ctime));
		if (succeeded) succeeded = OpStatus::IsSuccess(SetAttribute(item, BOOKMARK_VISITED,     atime));
		if (succeeded) succeeded = OpStatus::IsSuccess(DesktopBookmarkManager::AddCoreItem(item, after, parent));
		if (!succeeded)
		{
			OP_DELETE(item);
			return after;
		}
		return item;
	}

	static BookmarkItem* ImportSeparator(BookmarkItem* parent, BookmarkItem* after)
	{
		BookmarkItem* item = OP_NEW(BookmarkItem, ());
		if (!item) return after;
		item->SetFolderType(FOLDER_SEPARATOR_FOLDER);
		if(!OpStatus::IsSuccess(DesktopBookmarkManager::AddCoreItem(item, after, parent)))
		{
			OP_DELETE(item);
			return after;
		}
		return item;
	}
};

OP_STATUS HotlistParser::ImportNetscapeSqlite(const OpStringC& filename, BOOL into_sub_folder)
{
	SQLite3VFS overwrite;
	SQLite3DB db;
	SQLite3Cursor roots;
	SQLite3Cursor bookmarks;

	AutoDeleteList<MozPlacesBookmark> list;
	OpString8 path;
	RETURN_IF_ERROR(path.SetUTF8FromUTF16(filename));
	RETURN_IF_ERROR(db.Open(path.CStr(), overwrite.Name()));

	struct FolderDef
	{
		const char* name;
		int options;
		int bookmarks_id;
		MozPlacesBookmark* ref;
		enum
		{
			IMPORT_NONEMPTY = 1,
			IMPORT_CONTENT = 2
		};
	}
	folders[] =
	{
		{ "toolbar" },
		{ "unfiled", FolderDef::IMPORT_NONEMPTY },
		{ "menu", FolderDef::IMPORT_CONTENT },
	};
	size_t folder_count = sizeof(folders) / sizeof(folders[0]);

	//*************************************************
	//
	//    READ ROOT FOLDER DEFINITIONS
	//
	RETURN_IF_ERROR(roots.Open(db, "SELECT root_name, folder_id FROM moz_bookmarks_roots"));
	RETURN_IF_ERROR(roots.TupleSize() != 2 ? OpStatus::ERR : OpStatus::OK);

	while (roots.Step())
	{
		int folder_id = roots.GetInt(1);
		const char* root_name = roots.GetRawText8(0);
		int root_name_len = roots.GetRawTextLen8(0);

		for (size_t i = 0; i < folder_count; ++i)
		{
			if (!strncmp(folders[i].name, root_name, root_name_len))
			{
				folders[i].bookmarks_id = folder_id;
				break;
			}
		}
	};
	roots.Close();

	//*************************************************
	//
	//    READ BOOKMARKS DEFINITIONS
	//
	RETURN_IF_ERROR(bookmarks.Open(db, MOZ_PLACES_SQL));
	RETURN_IF_ERROR(bookmarks.TupleSize() != MOZ_COLUMN_COUNT ? OpStatus::ERR : OpStatus::OK);

	unsigned long feed_count = 0;
	while (bookmarks.Step())
	{
		int hidden = bookmarks.GetInt(MOZ_HIDDEN);
		if (hidden) continue;

		MozPlacesBookmark* bm = OP_NEW(MozPlacesBookmark, ());
		RETURN_IF_ERROR(bm ? OpStatus::OK : OpStatus::ERR_NO_MEMORY);
		bm->Into(&list);
		RETURN_IF_ERROR(bm->ReadTuple(bookmarks, feed_count));
	}
	RETURN_IF_ERROR(bookmarks.StepStatus());
	bookmarks.Close();
	db.Close();

	//*************************************************
	//
	//    CROSS REFERENCE ROOT FOLDERS WITH BOOKMARKS
	//
	for (size_t i = 0; i < folder_count; ++i)
	{
		folders[i].ref = list.First();
		while (folders[i].ref && folders[i].ref->m_id != folders[i].bookmarks_id)
			folders[i].ref = folders[i].ref->Suc();
	}

	//*************************************************
	//
	//    BUILD BOOKMARKS TREE
	//
	MozPlacesBookmark* node = list.First();
	while (node)
	{
		MozPlacesBookmark* parent = list.First();
		while (parent && parent->m_id != node->m_parent)
			parent = parent->Suc();
		if (parent)
			RETURN_IF_ERROR(parent->AppendChild(node));

		node = node->Suc();
	}

	//*************************************************
	//
	//    ACTUALLY IMPORT INTO CORE
	//
	BookmarkItem* root = NULL;
	BookmarkItem* after = NULL;
	if (into_sub_folder)
	{
		OpString name;
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_IMPORTED_FIREFOX_BOOKMARKS,name));
		OpString empty;
		root = MozPlacesBookmark::ImportFolder(NULL, NULL, name, empty, 0, 0, 0);
	}

	if (feed_count > 0)
	{
		OpString name;
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_IMPORTED_FIREFOX_FEEDS,name));
		OpString empty;
		BookmarkItem* feeds = MozPlacesBookmark::ImportFolder(root, NULL, name, empty, 0, 0, 0);

		for (size_t i = 0; i < folder_count; ++i)
		{
			if (!folders[i].ref) continue;
			after = folders[i].ref->LookForFeed(feeds, after);
		}

		after = feeds;
	}

	for (size_t i = 0; i < folder_count; ++i)
	{
		if (!folders[i].ref) continue;
		if (folders[i].options & FolderDef::IMPORT_NONEMPTY && !folders[i].ref->HasChildren()) continue;
		if (folders[i].options & FolderDef::IMPORT_CONTENT)
			after = folders[i].ref->ImportChildren(root, after);
		else
			after = folders[i].ref->Import(root, after);
	}

	return OpStatus::OK;
}

#undef MOZ_PLACES_JOIN_LIVEMARK
#undef MOZ_PLACES_JOIN_PLACES
#undef MOZ_PLACES_ATTRS
#undef MOZ_PLACES_SQL

OP_STATUS HotlistParser::ImportNetscapeHTML(const OpStringC& filename,  BOOL into_sub_folder)
{
	HotlistFileReader reader;

	OpString8 charset;
	BOOL charset_found = HotlistFileIO::ProbeCharset(filename, charset);

	if (charset_found)
	{
		reader.SetCharset(charset);
	}

	reader.Init(filename, HotlistFileIO::Ascii);

 	if (into_sub_folder)
	{
		BookmarkItem* item = OP_NEW(BookmarkItem, ());
		item->SetFolderType(FOLDER_NORMAL_FOLDER);

		if (item)
		{
			OpString name;

			const uni_char* match = filename.CStr() ? uni_stristr(filename.CStr(), UNI_L("firefox")) : 0;
			if (match)
			{
				g_languageManager->GetString(Str::S_IMPORTED_FIREFOX_BOOKMARKS,name);
			}
			else
			{
				g_languageManager->GetString(Str::S_IMPORTED_NETSCAPE_BOOKMARKS,name);
			}

			SetAttribute(item, BOOKMARK_TITLE, name.CStr());
			if (OpStatus::IsSuccess(DesktopBookmarkManager::AddCoreItem(item, /*previous?*/ 0, m_stack.Peek() ? m_stack.Peek() : 0)))
			{
				m_stack.Push(item);
			}
			else
			{
				OP_DELETE(item);
			}
		}
	}

	return ImportNetscapeHTML(reader, into_sub_folder);
}

OP_STATUS HotlistParser::ImportNetscapeHTML(HotlistFileReader &reader, BOOL into_sub_folder)
{
	int line_length;
	BOOL multiline_description = FALSE;

	while (1)
	{
		uni_char *line = reader.Readline(line_length);

		if (!line)
		{
			break;
		}
		if (!line[0])
		{
			continue;
		}

		if (multiline_description)
		{
			OpString tmp;
			multiline_description = GetNetscapeDescription(line,tmp);
			continue;
		}

		const uni_char* p1 = uni_stristr(line, UNI_L("<DT>"));
		if (p1)
		{
			const uni_char* p2;

			p2 = uni_stristr(p1, UNI_L("<A HREF="));
			if (p2)
			{
				const uni_char* p2_end = p2;
				if (uni_stristr(p2, UNI_L("<A HREF=\"")) == p2)
				{
					// Typically a bookmarklet. Locate end of content.
					for (p2_end=&p2[9]; *p2_end && *p2_end != '\"'; p2_end++) {};
				}

				const uni_char *p4 = uni_stristr(p2_end, UNI_L("</A>"));
				if (p4)
				{
					const uni_char* p3 = p4;
					for (; *p3 != '>' && p3 > p2_end; p3--) {};
					if (p3 > p2_end)
					{
						BookmarkItem* item = OP_NEW(BookmarkItem, ());
						item->SetFolderType(FOLDER_NO_FOLDER);

						if (item)
						{
							OpString tmp;
							tmp.Set(p3+1, p4-p3-1);
							ReplaceEscapes(tmp.CStr(), TRUE, FALSE, TRUE); // Convert '&amp;' and friends
							SetAttribute(item, BOOKMARK_TITLE, tmp.CStr());

							if (GetNetscapeTagValue(p2, UNI_L("A HREF"), tmp))
							{
								SetAttribute(item, BOOKMARK_URL, tmp.CStr());
							}
							if (GetNetscapeTagValue(p2_end, UNI_L("ADD_DATE"), tmp))
							{
								SetAttribute(item, BOOKMARK_CREATED, uni_atoi(tmp.CStr()));
							}
							if (GetNetscapeTagValue(p2_end, UNI_L("LAST_VISIT"), tmp))
							{
								SetAttribute(item, BOOKMARK_VISITED, uni_atoi(tmp.CStr()));
							}
							if (GetNetscapeTagValue(p2_end, UNI_L("SHORTCUTURL"), tmp))
							{
								SetAttribute(item, BOOKMARK_SHORTNAME, tmp.CStr());
							}

							// TODO: Previous
							if(OpStatus::IsSuccess(DesktopBookmarkManager::AddCoreItem(item, /*previous?*/0, m_stack.Peek() ? m_stack.Peek() : 0)))
							{
								if (item->GetFolderType() == FOLDER_NORMAL_FOLDER || item->GetFolderType() == FOLDER_TRASH_FOLDER)
									m_stack.Push(item);
							}
							else
							{
								OP_DELETE(item);
							}

						}
					}
				}
				continue;
			}

			p2 = uni_stristr(p1, UNI_L("<H"));
			if (p2)
			{
				const uni_char *p3 = uni_strstr(p2+1, UNI_L(">"));
				if (p3)
				{
#if defined(OMNIWEB_BOOKMARKS_SUPPORT)
					const uni_char *p5 = uni_stristr(p2+1, UNI_L("<a bookmarkid="));
					if(p5)
					{
						const uni_char *p6 = uni_strstr(p5+1, UNI_L(">"));
						if(p6)
						{
							p3 = p6;
						}
					}
#endif
					const uni_char *p4 = uni_strstr(p3+1, UNI_L("<"));
					if (p4)
					{
						BookmarkItem* item = OP_NEW(BookmarkItem, ());
						item->SetFolderType(FOLDER_NORMAL_FOLDER);

						if (item)
						{
							OpString tmp;
							tmp.Set(p3+1, p4-p3-1);
							ReplaceEscapes(tmp.CStr(), TRUE, FALSE, TRUE); // Convert '&amp;' and friends
							SetAttribute(item, BOOKMARK_TITLE, tmp.CStr());

							if (GetNetscapeTagValue(p2, UNI_L("ADD_DATE"), tmp))
							{
								SetAttribute(item, BOOKMARK_CREATED, uni_atoi(tmp.CStr()));
							}

							if(OpStatus::IsSuccess(DesktopBookmarkManager::AddCoreItem(item, /*previous?*/0, m_stack.Peek() ? m_stack.Peek() : 0)))
							{
								if (item->GetFolderType() == FOLDER_NORMAL_FOLDER || item->GetFolderType() == FOLDER_TRASH_FOLDER)
									m_stack.Push(item);
							}
							else
							{
								OP_DELETE(item);
							}
						}
						RETURN_IF_ERROR(ImportNetscapeHTML(reader, FALSE));

						m_stack.Pop();
					}
				}
				continue;
			}
		}
		p1 = uni_stristr(line, UNI_L("</DL>"));
		if (p1)
		{
			// End of folder
			return OpStatus::OK;
		}
		p1 = uni_stristr(line, UNI_L("<DD>"));
		if (p1)
		{
			// Description (multiline if line ends with a <BR>)
			OpString tmp;
			multiline_description = GetNetscapeDescription(p1+4, tmp);
			continue;
		}
		p1 = uni_stristr(line, UNI_L("<H1>"));
		if (p1)
		{
			const uni_char *p2 = uni_stristr(p1+4, UNI_L("</H1>"));
			if (p2)
			{
				BookmarkItem* item = OP_NEW(BookmarkItem, ());
				item->SetFolderType(FOLDER_NORMAL_FOLDER);

				if (item)
				{
					OpString tmp;
					tmp.Set(p1+4, p2-p1-4);

					SetAttribute(item, BOOKMARK_TITLE, tmp.CStr());

					if (OpStatus::IsSuccess(DesktopBookmarkManager::AddCoreItem(item, /*previous?*/ 0, m_stack.Peek() ? m_stack.Peek() : 0)))
					{
						if (item->GetFolderType() == FOLDER_NORMAL_FOLDER || item->GetFolderType() == FOLDER_TRASH_FOLDER)
							m_stack.Push(item);
					}
					else
					{
						OP_DELETE(item);
					}
				}

				RETURN_IF_ERROR(ImportNetscapeHTML(reader,FALSE));

				continue;
			}
		}
		p1 = uni_strstr(line, UNI_L("<HR>"));
		if (p1)
		{
			BookmarkItem* item = OP_NEW(BookmarkItem, ());
			item->SetFolderType(FOLDER_SEPARATOR_FOLDER);

			if (item)
			{
				if (OpStatus::IsSuccess(DesktopBookmarkManager::AddCoreItem(item, /*previous?*/ 0, m_stack.Peek() ? m_stack.Peek() : 0)))
				{
					if (item->GetFolderType() == FOLDER_NORMAL_FOLDER || item->GetFolderType() == FOLDER_TRASH_FOLDER)
						m_stack.Push(item);
				}
				else
				{
					OP_DELETE(item);
				}
			}

		}
	}
	return OpStatus::OK;
}


OP_STATUS HotlistParser::ImportExplorerFavorites(const OpStringC& path, BOOL into_sub_folder)
{
#ifdef _MACINTOSH_
	// Mac IE uses the same format as Netscape, but allows various text encodings too
	HotlistFileReader reader;

	OpString8 charset;
	BOOL charset_found = HotlistFileIO::ProbeCharset(path, charset);

	if (charset_found)
	{
		reader.SetCharset(charset);
	}

	reader.Init(path, HotlistFileIO::Ascii); // this is really a filename

	BookmarkItem* item = OP_NEW(BookmarkItem, ());
	item->SetFolderType(FOLDER_NORMAL_FOLDER);

	if (item)
	{
		OpString name;
		g_languageManager->GetString(Str::S_IMPORTED_EXPLORER_BOOKMARKS,name);

		BookmarkAttribute attribute;
		attribute.SetTextValue(name);
		item->SetAttribute(BOOKMARK_TITLE, &attribute);

		if (!DesktopBookmarkManager::AddCoreItem(item))
		{
			OP_DELETE(item);
			item = 0;
		}
	}

	return ImportNetscapeHTML(reader, FALSE);
#else  // _MACINTOCH_
	OpString extensions;
	extensions.Set("*");

	OpString top_folder_name;
	if (into_sub_folder)
	{
		g_languageManager->GetString(Str::S_IMPORTED_EXPLORER_BOOKMARKS, top_folder_name);
	}
	return ImportDirectory(path, extensions, top_folder_name);
#endif
}

OP_STATUS HotlistParser::ImportKDE1Bookmarks(const OpStringC& path)
{
	OpString extensions;
	//extensions.Set(".desktop,.kdelnk");
	extensions.Set("*.desktop");

	OpString name;
	g_languageManager->GetString(Str::S_IMPORTED_KDE1_BOOKMARKS,name);

	return ImportDirectory(path, extensions, name);
}

#if defined(_MACINTOSH_) && defined(SAFARI_BOOKMARKS_SUPPORT)
#include "platforms/mac/util/MachOCompatibility.h"
#define kChildrenKeyString 					"Children"
#define kVersionKeyString 					"WebBookmarkFileVersion"
#define kBookmarkTypeKeyString 				"WebBookmarkType"
#define kBookmarkFolderTitleKeyString 		"Title"			// Case sensitive
#define kBookmarkTitleKeyString				"title"			// Case sensitive
#define kBookmarkURLKeyString 				"URLString"
#define kBookmarkURIDictionaryKeyString 	"URIDictionary"

#define kBookmarkListValue 					"WebBookmarkTypeList"
#define kBookmarkLeafValue 					"WebBookmarkTypeLeaf"

OP_STATUS HotlistParser::ImportKonquerorBookmarks(const OpStringC& filename)
{
	OpString 			name;
	g_languageManager->GetString(Str::S_IMPORTED_KONQUEROR_BOOKMARKS,name);

	CFPropertyListRef 	propertyList;
	CFStringRef       	errorString;
	CFDataRef        	resourceData;
	Boolean          	status;
	SInt32            	errorCode;
	CFURLRef			theBookmarksURL;
	CFStringRef			theBookmarksCFString;
	OP_STATUS			result = OpStatus::OK;

	theBookmarksCFString = CFStringCreateWithCharacters(kCFAllocatorDefault, (UniChar*)filename.CStr(), filename.Length());
	if(theBookmarksCFString)
	{
		theBookmarksURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, theBookmarksCFString, kCFURLPOSIXPathStyle, false);

		if(theBookmarksURL)
		{
			// Read the XML file.
			status = CFURLCreateDataAndPropertiesFromResource(	kCFAllocatorDefault,
																theBookmarksURL,
																&resourceData,            // place to put file data
																NULL,
																NULL,
																&errorCode);

			if(resourceData && (status == true))
			{
				// Reconstitute the dictionary using the XML data.
				propertyList = CFB_CALL(CFPropertyListCreateFromXMLData)(kCFAllocatorDefault,
																resourceData,
																kCFPropertyListImmutable,
																&errorString);

				if(propertyList)
				{
					// Make sure we really got a CFDictionary
					if(CFGetTypeID(propertyList) == CFDictionaryGetTypeID())
					{
						CFDictionaryRef dict = (CFDictionaryRef)propertyList;
						SInt32 value = 0;

						CFNumberRef safariBookmarksVersion;

						if(CFDictionaryGetValueIfPresent(dict, CFSTR(kVersionKeyString), (const void **)&safariBookmarksVersion))
						{
							if (CFNumberGetValue(safariBookmarksVersion, kCFNumberSInt32Type, &value))
							{
								if(value == 1)
								{
									// Version 1.0, ok
									CFStringRef cfBookmarkType;

									if(CFDictionaryGetValueIfPresent(dict, CFSTR(kBookmarkTypeKeyString), (const void **)&cfBookmarkType))
									{
										if(cfBookmarkType && CFStringCompare(cfBookmarkType, CFSTR(kBookmarkListValue), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
										{
											CFArrayRef children;

											if(CFDictionaryGetValueIfPresent(dict, CFSTR(kChildrenKeyString), (const void **)&children))
											{
												result = ImportSafariBookmarks(children, name);
											}
										}
									}
								}
							}
						}
					}

					CFRelease(propertyList);
				}

				CFRelease(resourceData);
			}

			CFRelease(theBookmarksURL);
		}

		CFRelease(theBookmarksCFString);
	}

	return result;
}

uni_char * Copy_CFStringRefToUnichar(const CFStringRef cfstr)
{
	uni_char *result = 0;

	if(cfstr)
	{
		CFIndex len = CFStringGetLength(cfstr);
		const uni_char*	cfchars = (uni_char*)CFStringGetCharactersPtr(cfstr);

		if(cfchars)
		{
			UniSetStrN(result, cfchars, len);
		}
		else
		{
			result = OP_NEWA(uni_char, len+1);
			if(result)
			{
				CFStringGetCharacters(cfstr, CFRangeMake(0, len), (UniChar*)result);
				result[len] = 0;
			}
		}
	}

	return result;
}

OP_STATUS HotlistParser::ImportSafariBookmarks(CFArrayRef array, const OpStringC& top_folder_name)
{
	OP_STATUS result = OpStatus::OK;

	if (!top_folder_name.IsEmpty())
	{
		AddNewBookmarkFolder(top_folder_name.CStr());
	}

	CFStringRef cfTemp = 0;
	uni_char *uni_temp = 0;
	CFIndex array_count, array_index;
	CFIndex dict_count, dict_index;
	CFDictionaryRef dict;

	if(!array)
		return OpStatus::OK;

	array_count = CFArrayGetCount(array);

	for (array_index = 0;array_index < array_count;array_index++)
	{
		dict = (CFDictionaryRef)CFArrayGetValueAtIndex(array, array_index);

		dict_count = CFDictionaryGetCount(dict);

		CFTypeRef *theKeys = (CFTypeRef*) malloc(sizeof(CFTypeRef*) * dict_count); // ??? op_malloc ???
		CFTypeRef *theValues = (CFTypeRef*) malloc(sizeof(CFTypeRef*) * dict_count); // ??? op_malloc ???
		/* Where do the CF... functions come from ?  Might they realloc these ?
		 * Otherwise, why don't we use op_malloc (and op_free, below) ?
		 * FIXME: please document if you know !
		 */

		if ((NULL != theKeys) && (NULL != theValues))
		{
			CFDictionaryGetKeysAndValues(dict,theKeys, theValues);
			for (dict_index = 0;dict_index < dict_count;dict_index++)
			{
				if(CFStringCompare((CFStringRef)theKeys[dict_index], CFSTR(kBookmarkTypeKeyString), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
				{
					if(CFStringCompare((CFStringRef)theValues[dict_index], CFSTR(kBookmarkListValue), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
					{
						cfTemp = (CFStringRef) CFDictionaryGetValue(dict, CFSTR(kBookmarkFolderTitleKeyString));
						uni_temp = Copy_CFStringRefToUnichar(cfTemp);

						AddNewBookmarkFolder(uni_temp);

						CFArrayRef children = (CFArrayRef) CFDictionaryGetValue(dict, CFSTR(kChildrenKeyString));
						OpStringC top_folder_name;
						result = ImportSafariBookmarks(children, top_folder_name);
						m_stack.Pop();

						OP_DELETEA(uni_temp);
					}
					else if(CFStringCompare((CFStringRef)theValues[dict_index], CFSTR(kBookmarkLeafValue), kCFCompareCaseInsensitive) == kCFCompareEqualTo)
					{
						const uni_char* uni_temp2 = NULL;

						CFDictionaryRef uriDict = (CFDictionaryRef) CFDictionaryGetValue(dict, CFSTR(kBookmarkURIDictionaryKeyString));
						cfTemp = (CFStringRef) CFDictionaryGetValue(uriDict, CFSTR(kBookmarkTitleKeyString));
						uni_temp = Copy_CFStringRefToUnichar(cfTemp);

						cfTemp = (CFStringRef) CFDictionaryGetValue(dict, CFSTR(kBookmarkURLKeyString));
						uni_temp2 = Copy_CFStringRefToUnichar(cfTemp);

						AddNewBookmark(uni_temp,uni_temp2);

						OP_DELETEA(uni_temp);
						OP_DELETEA(uni_temp2);
					}
				}
			}
		}

		free(theKeys); // ??? op_free ???
		free(theValues); // ??? op_free ???
	}
	return result;
}

#else // SAFARI_BOOKMARKS_SUPPORT && _MACINTOSH_

OP_STATUS HotlistParser::ImportKonquerorBookmarks(const OpStringC& filename)
{
	HotlistXBELReader reader;
	reader.SetFilename(filename);
	reader.Parse();

	OpString name;
	g_languageManager->GetString(Str::S_IMPORTED_KONQUEROR_BOOKMARKS,name);

	INT32 index = 0;
	INT32 folder_count = -1;
	return ImportXBEL(reader, index, folder_count, name);
}

OP_STATUS HotlistParser::ImportXBEL(HotlistXBELReader& reader, INT32& index, INT32 folder_count, const OpStringC& top_folder_name)
{
	if (!top_folder_name.IsEmpty())
	{
		AddNewBookmarkFolder(top_folder_name.CStr());
	}

	for (; index < reader.GetCount();)
	{
		if (folder_count == 0)
		{
			break;
		}

		XBELReaderModelItem* node = reader.GetItem(index);
		if (node)
		{
			if (reader.IsFolder(node))
			{
				INT32 child_count = reader.GetChildCount(index);
				index++;

				BookmarkItem* item = OP_NEW(BookmarkItem, ());
				if(item)
				{
					item->SetFolderType(FOLDER_NORMAL_FOLDER);
					SetAttribute(item, BOOKMARK_TITLE,	 reader.GetName(node).CStr());
					SetAttribute(item, BOOKMARK_CREATED, reader.GetCreated(node));
					SetAttribute(item, BOOKMARK_VISITED, reader.GetVisited(node));

					if (OpStatus::IsSuccess(DesktopBookmarkManager::AddCoreItem(item, /*previous?*/ 0, m_stack.Peek() ? m_stack.Peek() : 0)))
					{
						m_stack.Push(item);
					}
					else
					{
						OP_DELETE(item);
						item = 0;
					}

					RETURN_IF_ERROR(ImportXBEL(reader, index, child_count, OpString()));
				}
			}
			else
			{
				index++;

				BookmarkItem* item = OP_NEW(BookmarkItem, ());
				if(item)
				{
					item->SetFolderType(FOLDER_NO_FOLDER);
					SetAttribute(item, BOOKMARK_TITLE,	 reader.GetName(node).CStr());
					SetAttribute(item, BOOKMARK_URL,	 reader.GetUrl(node).CStr());
					SetAttribute(item, BOOKMARK_CREATED, reader.GetCreated(node));
					SetAttribute(item, BOOKMARK_VISITED, reader.GetVisited(node));

					if (!OpStatus::IsSuccess(DesktopBookmarkManager::AddCoreItem(item, /*previous?*/ 0, m_stack.Peek() ? m_stack.Peek() : 0)))
					{
						OP_DELETE(item);
						item = 0;
					}
				}
			}
		}

		folder_count --;
	}

	return OpStatus::OK;
}

#endif // SAFARI_BOOKMARKS_SUPPORT && _MACINTOSH_

OP_STATUS HotlistParser::ImportDirectory(const OpStringC& path, const OpStringC& extensions, const OpStringC& top_folder_name)
{
	HotlistDirectoryReader reader;
	reader.Construct(path, extensions);
	if (!top_folder_name.IsEmpty())
	{
		AddNewBookmarkFolder(top_folder_name.CStr());
	}

	HotlistDirectoryReader::Node *node = reader.GetFirstEntry();
	while (node)
	{
		if (reader.IsDirectory(node))
		{
			if(AddNewBookmarkFolder(reader.GetName(node).CStr(), reader.GetCreated(node), reader.GetVisited(node)))
			{
				OpStringC top_folder_name;
				RETURN_IF_ERROR(ImportDirectory(reader.GetPath(node), extensions, top_folder_name));
				m_stack.Pop();
			}
		}
		else if (reader.IsFile(node))
		{
			AddNewBookmark(reader.GetName(node).CStr(), reader.GetUrl(node).CStr(), reader.GetCreated(node), reader.GetVisited(node));
		}

		node = reader.GetNextEntry();
	}

	return OpStatus::OK;
}


OP_STATUS HotlistParser::Write(const OpString &filename, HotlistModel &model, BOOL all, BOOL safe)
{
	HotlistFileWriter writer;

	RETURN_IF_ERROR(writer.Open(filename, safe));
	RETURN_IF_ERROR(writer.WriteHeader(model.GetModelIsSynced()));
	RETURN_IF_ERROR(Write(writer, model, all));
	RETURN_IF_ERROR(writer.Close());
	return OpStatus::OK;
}



OP_STATUS HotlistParser::Write(HotlistFileWriter &writer, HotlistModel &model, BOOL all)
{
	for (int i=0; i<model.GetItemCount();)
	{
		RETURN_IF_ERROR(Write(writer, model, i, TRUE, all, i));
	}
	return OpStatus::OK;
}



OP_STATUS HotlistParser::Write(HotlistFileWriter& writer,
								HotlistModel& model,
								INT32 index,
								BOOL recursive,
								BOOL all,
								INT32& next_index)
{


	next_index = -1;
	HotlistModelItem* item = model.GetItemByIndex(index);
	if (item)
	{
		if (item->IsBookmark())
		{
			if (all || item->GetIsMarked())
			{
				RETURN_IF_ERROR(WriteBookmarkNode(writer, *item));
			}
		}
		else if (item->IsNote())
		{
			if (all || item->GetIsMarked())
			{
				RETURN_IF_ERROR(WriteNoteNode(writer, *item));
			}
		}
		else if (item->IsSeparator())
		{
			if (all || item->GetIsMarked())
			{
				RETURN_IF_ERROR(WriteSeparatorNode(writer, *item));
			}
		}
#ifdef WEBSERVER_SUPPORT
		else if (item->IsUniteService())
		{
			if (all || item->GetIsMarked())
			{
				RETURN_IF_ERROR(WriteUniteServiceNode(writer, *item));
			}
		}
#endif // WEBSERVER_SUPPORT
		else if (item->IsContact())
		{
			if (all || item->GetIsMarked())
			{
				RETURN_IF_ERROR(WriteContactNode(writer, *item));
			}
		}
		else if (item->IsGroup())
		{
			if (all || item->GetIsMarked())
			{
				RETURN_IF_ERROR(WriteGroupNode(writer, *item));
			}
		}
		else if (item->IsFolder())
		{
			if (all || item->GetIsMarked())
			{
				RETURN_IF_ERROR(WriteFolderNode(writer, *item, model));
			}

			if (recursive)
			{
				INT32 parent_index = index;

				index ++;
				while (index<model.GetItemCount())
				{
					int p = model.GetItemParent(index);
					if (p != parent_index)
					{
						break;
					}
					// The commented out test disables a real save-only-what-is-selected
					RETURN_IF_ERROR(Write(writer, model, index, TRUE, all || (item->GetIsMarked() /*&& !item->GetIsExpandedFolder()*/), index));
				}

				if (all || item->GetIsMarked())
				{
					RETURN_IF_ERROR(writer.WriteAscii("-\n\n"));
				}

				next_index = index;
				return OpStatus::OK;
			}
		}
	}

	next_index = index + 1;
	return OpStatus::OK;
}

OP_STATUS HotlistParser::WriteHTML(const OpString &filename, BookmarkModel &model, BOOL all, BOOL safe)
{
	HotlistFileWriter writer;
	
	RETURN_IF_ERROR(writer.Open(filename, safe));
	RETURN_IF_ERROR(WriteHTML(writer, model, all));
	RETURN_IF_ERROR(writer.Close());
	return OpStatus::OK;
}

OP_STATUS HotlistParser::WriteHTML(HotlistFileWriter& writer, BookmarkModel& model, BOOL all)
{
	OpString bookmarks;
	g_languageManager->GetString(Str::SI_IDSTR_HL_BOOKMARKS_ROOT_FOLDER_NAME, bookmarks);

#if defined(SUPPORT_WRITE_NETSCAPE_BOOKMARKS)
	RETURN_IF_ERROR(writer.WriteAscii("<!DOCTYPE NETSCAPE-Bookmark-file-1>\n<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n<!--This is an automatically generated file.\nIt will be read and overwritten.\nDo Not Edit! -->\n<TITLE>"));
	RETURN_IF_ERROR(writer.WriteUni(bookmarks.CStr()));
	RETURN_IF_ERROR(writer.WriteAscii("</TITLE>\n<H1>"));
	RETURN_IF_ERROR(writer.WriteUni(bookmarks.CStr()));
	RETURN_IF_ERROR(writer.WriteAscii("</H1>\n<DL><P>\n"));
#else
	RETURN_IF_ERROR(writer.WriteAscii("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n<html>\n<head>\n<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"><title>\n"));
	RETURN_IF_ERROR(writer.WriteUni(bookmarks.CStr()));
	RETURN_IF_ERROR(writer.WriteAscii("\n</title><style type=\"text/css\">\ndl { margin-left: 2em; margin-top: 1em}\ndt { margin-left: 0; font-weight: bold}\ndd { margin-left: 0; }\ndl.first {margin-left: 0 }\n</style>\n</head>\n\n<body>\n<dl class=first>\n"));
#endif

	BOOL first = TRUE;

	for (int i=0; i<model.GetItemCount();)
	{
		RETURN_IF_ERROR(WriteHTML(writer, model, i, first, all, i, 4));
	}

#if defined(SUPPORT_WRITE_NETSCAPE_BOOKMARKS)
	RETURN_IF_ERROR(writer.WriteAscii("</DL><P>\n"));
#else
	RETURN_IF_ERROR(writer.WriteAscii("</dl>\n\n</body>\n</html>\n"));
#endif

	return OpStatus::OK;
}

OP_STATUS HotlistParser::WriteHTML(HotlistFileWriter& writer, BookmarkModel& model, INT32 index, BOOL& first, BOOL all, INT32& next_index, INT32 indent)
{
	OP_STATUS ret = OpStatus::OK;
	next_index = -1;

	HotlistModelItem* item = model.GetItemByIndex(index);
	if (item)
	{
		uni_char* html_string;

		if (item->IsBookmark())
		{
			if (all || item->GetIsMarked())
			{

				const OpStringC &url = item->GetResolvedUrl();
				html_string = HTMLify_string(url.CStr());

#if defined(SUPPORT_WRITE_NETSCAPE_BOOKMARKS)

				ret = writer.WriteFormat("%*s<DT><A HREF=\"%s\" ADD_DATE=\"%d\" LAST_VISITED=\"%d\"", indent, UNI_L(""), html_string ? html_string : UNI_L(""), item->GetCreated(), item->GetVisited());

				OP_DELETEA(html_string);
				if (ret != OpStatus::OK)
					return ret;

				html_string = HTMLify_string(item->GetShortName().CStr());

				if (html_string && *html_string)
				{
					ret = writer.WriteFormat(" SHORTCUTURL=\"%s\"", html_string);
					if (ret != OpStatus::OK)
					{
						OP_DELETEA(html_string);
						return ret;
					}
				}
				OP_DELETEA(html_string);

				RETURN_IF_ERROR(writer.WriteAscii(">"));

				html_string = HTMLify_string(item->GetName().CStr());

				ret = writer.WriteFormat("%s</A>\n", html_string ? html_string : UNI_L(""));
				OP_DELETEA(html_string);
				RETURN_IF_ERROR(ret);

				html_string = HTMLify_string(item->GetDescription().CStr());

				if (html_string && uni_strlen(html_string) > 0)
				{
					OpString tmp;
					ConvertFromNewline(tmp, html_string, UNI_L("<BR>\n"));
					ret = writer.WriteFormat("%*s%<DD>%s</DD>\n", indent, UNI_L(""), tmp.CStr() ? tmp.CStr() : UNI_L(""));
				}
				OP_DELETEA(html_string);
				RETURN_IF_ERROR(ret);
#else // (SUPPORT_WRITE_NETSCAPE_BOOKMARKS)
				ret = writer.WriteFormat("<dd><a href=\"%s\">", html_string ? html_string : UNI_L(""));
				OP_DELETEA(html_string);
				RETURN_IF_ERROR(ret);

				html_string = HTMLify_string(item->GetName());
				ret = writer.WriteFormat("%s", html_string ? html_string : UNI_L(""));
				OP_DELETEA(html_string);
				RETURN_IF_ERROR(ret);

				RETURN_IF_ERROR(writer.WriteAscii("</a></dd>\n"));
#endif // (SUPPORT_WRITE_NETSCAPE_BOOKMARKS)
			}

			next_index = index + 1;
			return OpStatus::OK;
		}
		else if (item->IsSeparator())
		{
#if defined(SUPPORT_WRITE_NETSCAPE_BOOKMARKS)
			RETURN_IF_ERROR(writer.WriteFormat("%*s%<HR>\n", indent, UNI_L("")));
#endif
		}
		else if (item->IsFolder() && !item->GetIsTrashFolder())

		{
			BOOL was_first = first;


			if ((all && !item->GetIsTrashFolder()) || item->GetIsMarked())
			{
#if defined(SUPPORT_WRITE_NETSCAPE_BOOKMARKS)

				html_string = HTMLify_string(item->GetName().CStr());

				ret = writer.WriteFormat("%*s<DT><H3 ADD_DATE=\"%d\">%s</H3>\n", indent, UNI_L(""), item->GetCreated(), html_string ? html_string : UNI_L(""));
				OP_DELETEA(html_string);
				RETURN_IF_ERROR(ret);

				RETURN_IF_ERROR(writer.WriteFormat("%*s<DL><P>\n", indent, UNI_L("")));
#else // SUPPORT_WRITE_NETSCAPE_BOOKMARKS
				if (was_first)
				{
					RETURN_IF_ERROR(writer.WriteAscii("<dt>"));
				}
				else
				{
					RETURN_IF_ERROR(writer.WriteAscii("<dd>\n<dl>\n<dt>"));
				}
				first = FALSE;

				html_string = HTMLify_string(item->GetName());

				if (html_string)
				{
					ret = writer.WriteUni(html_string);
					OP_DELETEA(html_string);
				}
				RETURN_IF_ERROR(ret);

				RETURN_IF_ERROR(writer.WriteAscii("</dt>\n"));
#endif // SUPPORT_WRITE_NETSCAPE_BOOKAMRKS
			}

			INT32 parent_index = index;

			index ++;
			while (index<model.GetItemCount())
			{
				int p = model.GetItemParent(index);

				if (p != parent_index)
				{
					break;
				}

				// The commented out test disables a real save-only-what-is-selected
				RETURN_IF_ERROR(WriteHTML(writer, model, index, first, all || (item->GetIsMarked() /*&& !item->GetIsExpandedFolder()*/), index, indent+4));
			}

			if (all || item->GetIsMarked())
			{
#if defined(SUPPORT_WRITE_NETSCAPE_BOOKMARKS)
				RETURN_IF_ERROR(writer.WriteFormat("%*s</DL><P>\n", indent, UNI_L("")));
#else
				if (!was_first)
				{
					RETURN_IF_ERROR(writer.WriteAscii("</dl>\n</dd>\n"));
				}
#endif
			}

			first = was_first;

			next_index = index;
			return OpStatus::OK;
}
	}
	next_index = index + 1;
	return OpStatus::OK;
}

OP_STATUS HotlistParser::WriteDeletedDefaultBookmark(HotlistFileWriter& writer)
{
	// If this hits BookmarkAdrStorage wasn't closed completely when g_desktop_bookmark_manager destructs.
	OP_ASSERT(g_desktop_bookmark_manager);
	if (!g_desktop_bookmark_manager)
		return OpStatus::ERR;

	const OpAutoVector<OpString>& bookmarks = g_desktop_bookmark_manager->DeletedDefaultBookmarks();
	if (bookmarks.GetCount())
	{
		RETURN_IF_ERROR(writer.WriteAscii("#DELETED\n"));
		for (UINT32 i=0; i<bookmarks.GetCount(); i++)
		{
			RETURN_IF_ERROR(writer.WriteFormat("\tPARTNERID=%s\n", bookmarks.Get(i)->CStr()));
		}
	}
	return OpStatus::OK;
}

OP_STATUS HotlistParser::WriteBookmarkNode(HotlistFileWriter& writer, HotlistModelItem& item)
{
	RETURN_IF_ERROR(writer.WriteAscii("#URL\n"));
	RETURN_IF_ERROR(writer.WriteFormat("\tID=%d\n", item.GetID()));
	if (item.GetName().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tNAME=%s\n", item.GetName().CStr()));
	if (item.GetUrl().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tURL=%s\n", item.GetUrl().CStr()));
	if (item.GetDisplayUrl().CStr() && item.GetDisplayUrl().Compare(item.GetUrl()) != 0)
		RETURN_IF_ERROR(writer.WriteFormat("\tDISPLAY URL=%s\n", item.GetDisplayUrl().CStr()));
	if (item.GetCreated() > 0)
		RETURN_IF_ERROR(writer.WriteFormat("\tCREATED=%d\n", item.GetCreated()));
	if (item.GetVisited())
		RETURN_IF_ERROR(writer.WriteFormat("\tVISITED=%d\n", item.GetVisited()));
	if (item.GetDescription().CStr())
	{
		OpString tmp;
		ConvertFromNewline(tmp, item.GetDescription().CStr(), newline_comma_replacement);
		RETURN_IF_ERROR(writer.WriteFormat("\tDESCRIPTION=%s\n", tmp.CStr()));
	}
	if (item.GetShortName().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tSHORT NAME=%s\n", item.GetShortName().CStr()));

	if (item.GetModel()->GetActiveItemId() == item.GetID())
		RETURN_IF_ERROR(writer.WriteAscii("\tACTIVE=YES\n"));
	if (item.GetOnPersonalbar())
	{
		RETURN_IF_ERROR(writer.WriteAscii("\tON PERSONALBAR=YES\n"));
		RETURN_IF_ERROR(writer.WriteFormat("\tPERSONALBAR_POS=%d\n", item.GetPersonalbarPos()));
	}
	if (item.GetInPanel())
	{
		RETURN_IF_ERROR(writer.WriteAscii("\tIN PANEL=YES\n"));
		RETURN_IF_ERROR(writer.WriteFormat("\tPANEL_POS=%d\n", item.GetPanelPos()));
	}
	if (item.GetSmallScreen())
	{
		RETURN_IF_ERROR(writer.WriteAscii("\tSMALL SCREEN=YES\n"));
	}

	if (item.GetUniqueGUID().CStr())
	{
		RETURN_IF_ERROR(writer.WriteFormat("\tUNIQUEID=%s\n", item.GetUniqueGUID().CStr()));
	}

	if (item.GetPartnerID().CStr())
	{
		RETURN_IF_ERROR(writer.WriteFormat("\tPARTNERID=%s\n", item.GetPartnerID().CStr()));
	}

#if defined(SUPPORT_UNKNOWN_TAGS)
	if (item.GetUnknownTags().CStr())
	{
		RETURN_IF_ERROR(writer.WriteUni(item.GetUnknownTags().CStr()));
	}
#endif

	RETURN_IF_ERROR(writer.WriteAscii("\n"));

	return OpStatus::OK;
}


OP_STATUS HotlistParser::WriteNoteNode(HotlistFileWriter& writer, HotlistModelItem& item)
{
	RETURN_IF_ERROR(writer.WriteAscii("#NOTE\n"));
	RETURN_IF_ERROR(writer.WriteFormat("\tID=%d\n", item.GetID()));

#ifdef SUPPORT_SYNC_NOTES
	if (item.GetUniqueGUID().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tUNIQUEID=%s\n", item.GetUniqueGUID().CStr()));
#endif
	if (item.GetName().CStr())
	{
		OpString tmp;
		ConvertFromNewline(tmp, item.GetName().CStr(), newline_comma_replacement);
		RETURN_IF_ERROR(writer.WriteFormat("\tNAME=%s\n", tmp.CStr()));
	}
	if (item.GetUrl().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tURL=%s\n", item.GetUrl().CStr()));
	if (item.GetCreated() > 0)
		RETURN_IF_ERROR(writer.WriteFormat("\tCREATED=%d\n", item.GetCreated()));
	if (item.GetVisited())
		RETURN_IF_ERROR(writer.WriteFormat("\tVISITED=%d\n", item.GetVisited()));
	if (item.GetDescription().CStr())
	{
		OpString tmp;
		ConvertFromNewline(tmp, item.GetDescription().CStr(), newline_comma_replacement);
		RETURN_IF_ERROR(writer.WriteFormat("\tDESCRIPTION=%s\n", tmp.CStr()));
	}
	if (item.GetModel()->GetActiveItemId() == item.GetID())
		RETURN_IF_ERROR(writer.WriteAscii("\tACTIVE=YES\n"));

#if defined(SUPPORT_UNKNOWN_TAGS)
	if (item.GetUnknownTags().CStr())
	{
		RETURN_IF_ERROR(writer.WriteUni(item.GetUnknownTags().CStr()));
	}
#endif
	RETURN_IF_ERROR(writer.WriteAscii("\n"));

	return OpStatus::OK;
}

#ifdef GADGET_SUPPORT
OP_STATUS HotlistParser::WriteGadgetNode(HotlistFileWriter& writer, HotlistModelItem& item)
{
	if (item.GetName().CStr())
	{
		OpString tmp;
		ConvertFromNewline(tmp, item.GetName().CStr(), newline_comma_replacement);
		RETURN_IF_ERROR(writer.WriteFormat("\tNAME=%s\n", tmp.CStr()));
	}
	//gadget specific
	if (item.GetGadgetIdentifier().CStr())
	{
		RETURN_IF_ERROR(writer.WriteFormat("\tIDENTIFIER=%s\n", item.GetGadgetIdentifier().CStr()));
	}

	if(item.GetGadgetCleanUninstall())
		RETURN_IF_ERROR(writer.WriteAscii("\tCLEAN UNINSTALL=YES\n"));

	//generic
	if (item.GetCreated() > 0)
		RETURN_IF_ERROR(writer.WriteFormat("\tCREATED=%d\n", item.GetCreated()));
	if (item.GetVisited())
		RETURN_IF_ERROR(writer.WriteFormat("\tVISITED=%d\n", item.GetVisited()));
	if (item.GetModel()->GetActiveItemId() == item.GetID())
		RETURN_IF_ERROR(writer.WriteAscii("\tACTIVE=YES\n"));
	
#if defined(SUPPORT_UNKNOWN_TAGS)
	if (item.GetUnknownTags().CStr())
	{
		RETURN_IF_ERROR(writer.WriteUni(item.GetUnknownTags().CStr()));
	}
#endif
	RETURN_IF_ERROR(writer.WriteAscii("\n"));

	return OpStatus::OK;
}

#ifdef WEBSERVER_SUPPORT
OP_STATUS HotlistParser::WriteUniteServiceNode(HotlistFileWriter& writer, HotlistModelItem& item)
{
	if (item.IsUniteService())
	{
		RETURN_IF_ERROR(writer.WriteAscii("#UNITESERVICE\n"));
		RETURN_IF_ERROR(writer.WriteFormat("\tID=%d\n", item.GetID()));

		UniteServiceModelItem & unite_item = static_cast<UniteServiceModelItem &>(item);

		if (unite_item.IsRootService())
			RETURN_IF_ERROR(writer.WriteAscii("\tROOT=YES\n"));

		if (unite_item.NeedsConfiguration())
			RETURN_IF_ERROR(writer.WriteAscii("\tNEEDS_CONFIGURATION=YES\n"));
		if (unite_item.NeedsToAutoStart())
			RETURN_IF_ERROR(writer.WriteAscii("\tRUNNING=YES\n"));
	
		return WriteGadgetNode(writer, item);
	}
	else
		return OpStatus::ERR;
}
#endif // WEBSERVER_SUPPORT

#endif // GADGET_SUPPORT

OP_STATUS HotlistParser::WriteSeparatorNode(HotlistFileWriter& writer, HotlistModelItem& item)
{
	if (item.GetSeparatorType() != SeparatorModelItem::NORMAL_SEPARATOR)
	{
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(writer.WriteAscii("#SEPERATOR\n"));
	RETURN_IF_ERROR(writer.WriteFormat("\tID=%d\n", item.GetID()));

	if (item.GetUniqueGUID().CStr())
	{
		RETURN_IF_ERROR(writer.WriteFormat("\tUNIQUEID=%s\n", item.GetUniqueGUID().CStr()));
	}

	RETURN_IF_ERROR(writer.WriteAscii("\n"));

	return OpStatus::OK;
}

OP_STATUS HotlistParser::WriteContactNode(HotlistFileWriter& writer, HotlistModelItem& item)
{
	RETURN_IF_ERROR(writer.WriteAscii("#CONTACT\n"));
	RETURN_IF_ERROR(writer.WriteFormat("\tID=%d\n", item.GetID()));

	if (item.GetName().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tNAME=%s\n", item.GetName().CStr()));
	if (item.GetUrl().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tURL=%s\n", item.GetUrl().CStr()));
	if (item.GetCreated() > 0)
		RETURN_IF_ERROR(writer.WriteFormat("\tCREATED=%d\n", item.GetCreated()));
	if (item.GetVisited() > 0)
		RETURN_IF_ERROR(writer.WriteFormat("\tVISITED=%d\n", item.GetVisited()));
	if (item.GetDescription().CStr())
	{
		OpString tmp;
		ConvertFromNewline(tmp, item.GetDescription().CStr(), newline_comma_replacement);
		RETURN_IF_ERROR(writer.WriteFormat("\tDESCRIPTION=%s\n", tmp.CStr()));
	}
	if (item.GetShortName().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tSHORT NAME=%s\n", item.GetShortName().CStr()));
	if (item.GetModel()->GetActiveItemId() == item.GetID())
		RETURN_IF_ERROR(writer.WriteAscii("\tACTIVE=YES\n"));
	if (item.GetOnPersonalbar())
	{
		RETURN_IF_ERROR(writer.WriteAscii("\tON PERSONALBAR=YES\n"));
		RETURN_IF_ERROR(writer.WriteFormat("\tPERSONALBAR_POS=%d\n", item.GetPersonalbarPos()));
	}
	if (item.GetMail().HasContent())
	{
		// Pre 7 versions used 0x020x02 as a separator in the file
		OpString tmp;
		ConvertFromComma(tmp, item.GetMail().CStr(), newline_comma_replacement);
		RETURN_IF_ERROR(writer.WriteFormat("\tMAIL=%s\n", tmp.CStr()));
	}
	if (item.GetPhone().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tPHONE=%s\n", item.GetPhone().CStr()));
	if (item.GetFax().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tFAX=%s\n", item.GetFax().CStr()));
	if (item.GetPostalAddress().CStr())
	{
		OpString tmp;
		ConvertFromNewline(tmp, item.GetPostalAddress().CStr(), newline_comma_replacement);
		RETURN_IF_ERROR(writer.WriteFormat("\tPOSTALADDRESS=%s\n", tmp.CStr()));
		//writer.WriteFormat("\tPOSTALADDRESS=%s\n", item.GetPostalAddress().CStr());
	}
	if (item.GetPictureUrl().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tPICTUREURL=%s\n", item.GetPictureUrl().CStr()));
	if (item.IsContact() && item.GetImage())
	{
		OpString icon;
		icon.Set(item.GetImage());
		RETURN_IF_ERROR(writer.WriteFormat("\tICON=%s\n", icon.CStr()));
	}
	if (item.GetConaxNumber().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\t00: CONAX CARD NUMBER=%s\n", item.GetConaxNumber().CStr()));
	if (item.GetImAddress().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tIMADDRESS=%s\n", item.GetImAddress().CStr()));
	if (item.GetImProtocol().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tIMPROTOCOL=%s\n", item.GetImProtocol().CStr()));
	if (item.GetImStatus().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tIMSTATUS=%s\n", item.GetImStatus().CStr()));
	if (item.GetImXPosition().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tIMXPOSITION=%s\n", item.GetImXPosition().CStr()));
	if (item.GetImYPosition().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tIMYPOSITION=%s\n", item.GetImYPosition().CStr()));
	if (item.GetImWidth().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tIMWIDTH=%s\n", item.GetImWidth().CStr()));
	if (item.GetImHeight().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tIMHEIGHT=%s\n", item. GetImHeight().CStr()));
	if (item.GetImVisibility().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tIMVISIBILITY=%s\n", item.GetImVisibility().CStr()));
	if (item.GetImMaximized().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tIMMAXIMIZED=%s\n", item.GetImMaximized().CStr()));
	if (item.GetM2IndexId(FALSE) != 0)
		RETURN_IF_ERROR(writer.WriteFormat("\tM2INDEXID=%d\n", item.GetM2IndexId(FALSE)));

#if defined(SUPPORT_UNKNOWN_TAGS)
	if (item.GetUnknownTags().CStr())
	{
		RETURN_IF_ERROR(writer.WriteUni(item.GetUnknownTags().CStr()));
	}
#endif
	RETURN_IF_ERROR(writer.WriteAscii("\n"));

	return OpStatus::OK;
}

OP_STATUS HotlistParser::WriteFolderNode(HotlistFileWriter& writer, HotlistModelItem& item, HotlistModel& model)
{
	RETURN_IF_ERROR(writer.WriteAscii("#FOLDER\n"));
	RETURN_IF_ERROR(writer.WriteFormat("\tID=%d\n", item.GetID()));

	if (model.GetModelType() == HotlistModel::NoteRoot 
		&& item.GetName().CStr())
	{
		OpString tmp;
		ConvertFromNewline(tmp, item.GetName().CStr(), newline_comma_replacement);
		RETURN_IF_ERROR(writer.WriteFormat("\tNAME=%s\n", tmp.CStr()));
	}
	else if (item.GetName().CStr())
	{
		RETURN_IF_ERROR(writer.WriteFormat("\tNAME=%s\n", item.GetName().CStr()));
	}

	if (item.GetCreated() > 0)
		RETURN_IF_ERROR(writer.WriteFormat("\tCREATED=%d\n", item.GetCreated()));
	if (item.GetVisited() > 0)
		RETURN_IF_ERROR(writer.WriteFormat("\tVISITED=%d\n", item.GetVisited()));
	if (item.GetDescription().CStr())
	{
		OpString tmp;
		ConvertFromNewline(tmp, item.GetDescription().CStr(), newline_comma_replacement);
		RETURN_IF_ERROR(writer.WriteFormat("\tDESCRIPTION=%s\n", tmp.CStr()));
	}
	if (item.GetShortName().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tSHORT NAME=%s\n", item.GetShortName().CStr()));

	if (item.GetPartnerID().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tPARTNERID=%s\n", item.GetPartnerID().CStr()));

	if (item.GetModel()->GetActiveItemId() == item.GetID())
		RETURN_IF_ERROR(writer.WriteAscii("\tACTIVE=YES\n"));
	if (item.GetOnPersonalbar())
	{
		RETURN_IF_ERROR(writer.WriteAscii("\tON PERSONALBAR=YES\n"));
		RETURN_IF_ERROR(writer.WriteFormat("\tPERSONALBAR_POS=%d\n", item.GetPersonalbarPos()));
	}

	if (item.GetIsTrashFolder())
		RETURN_IF_ERROR(writer.WriteAscii("\tTRASH FOLDER=YES\n"));

	if (item.GetTarget().HasContent())
		RETURN_IF_ERROR(writer.WriteFormat("\tTARGET=%s\n", item.GetTarget().CStr()));

	if (item.GetHasMoveIsCopy())
		RETURN_IF_ERROR(writer.WriteAscii("\tMOVE_IS_COPY=YES\n"));

	if (!item.GetIsDeletable())
		RETURN_IF_ERROR(writer.WriteAscii("\tDELETABLE=NO\n"));

	if (!item.GetSubfoldersAllowed())
		RETURN_IF_ERROR(writer.WriteAscii("\tSUBFOLDER_ALLOWED=NO\n"));

	if (!item.GetSeparatorsAllowed())
		RETURN_IF_ERROR(writer.WriteAscii("\tSEPARATOR_ALLOWED=NO\n"));

	if (item.GetMaxItems() != -1)
		RETURN_IF_ERROR(writer.WriteFormat("\tMAX_ITEMS=%d\n", item.GetMaxItems()));

	if (item.GetIsExpandedFolder() && !item.GetIsTrashFolder())
		RETURN_IF_ERROR(writer.WriteAscii("\tEXPANDED=YES\n"));
#if defined(SUPPORT_IMPORT_LINKBAR_TAG)
	if (item.GetIsLinkBarFolder())
	{
		RETURN_IF_ERROR(writer.WriteAscii("\tLINKBAR FOLDER=YES\n"));
		RETURN_IF_ERROR(writer.WriteAscii("\tLINKBAR STOP=YES\n"));
	}
#endif

	if (item.GetUniqueGUID().CStr())
	{
		OpString guid;
		guid.Set(item.GetUniqueGUID());
		RETURN_IF_ERROR(writer.WriteFormat("\tUNIQUEID=%s\n", guid.CStr()));
	}

#if defined(SUPPORT_UNKNOWN_TAGS)
	if (item.GetUnknownTags().CStr())
	{
		RETURN_IF_ERROR(writer.WriteUni(item.GetUnknownTags().CStr()));
	}
#endif

	RETURN_IF_ERROR(writer.WriteAscii("\n"));

	return OpStatus::OK;
}

OP_STATUS HotlistParser::WriteTrashFolderNode(HotlistFileWriter& writer, BookmarkItem *bookmark)
{
	RETURN_IF_ERROR(writer.WriteAscii("#FOLDER\n"));
	RETURN_IF_ERROR(writer.WriteAscii("\tNAME=Trash\n")); // Name doesn't matter for trash
	RETURN_IF_ERROR(writer.WriteFormat("\tUNIQUEID=%s\n", bookmark->GetUniqueId()));
	RETURN_IF_ERROR(writer.WriteAscii("\tTRASH FOLDER=YES\n"));
	RETURN_IF_ERROR(writer.WriteAscii("\n"));
	return OpStatus::OK;
}

OP_STATUS HotlistParser::WriteGroupNode(HotlistFileWriter& writer, HotlistModelItem& item)
{
	RETURN_IF_ERROR(writer.WriteAscii("#GROUP\n"));
	RETURN_IF_ERROR(writer.WriteFormat("\tID=%d\n", item.GetID()));

	if (item.GetName().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tNAME=%s\n", item.GetName().CStr()));
	if (item.GetCreated() > 0)
		RETURN_IF_ERROR(writer.WriteFormat("\tCREATED=%d\n", item.GetCreated()));
	if (item.GetVisited() > 0)
		RETURN_IF_ERROR(writer.WriteFormat("\tVISITED=%d\n", item.GetVisited()));
	if (item.GetDescription().CStr())
	{
		OpString tmp;
		ConvertFromNewline(tmp, item.GetDescription().CStr(), newline_comma_replacement);
		RETURN_IF_ERROR(writer.WriteFormat("\tDESCRIPTION=%s\n", tmp.CStr()));
	}
	if (item.GetShortName().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tSHORT NAME=%s\n", item.GetShortName().CStr()));

	if (item.GetGroupList().CStr())
		RETURN_IF_ERROR(writer.WriteFormat("\tMEMBERS=%s\n", item.GetGroupList().CStr()));

#if defined(SUPPORT_UNKNOWN_TAGS)
	if (item.GetUnknownTags().CStr())
	{
		RETURN_IF_ERROR(writer.WriteUni(item.GetUnknownTags().CStr()));
	}
#endif
	RETURN_IF_ERROR(writer.WriteAscii("\n"));

	return OpStatus::OK;
}


int HotlistParser::IsKey(const uni_char* candidate, int candidateLen,
						 const char* key, int keyLen)
{
	if (candidateLen >= keyLen)
	{
		for (int i=0; i<keyLen; i++)
		{
			if (candidate[i] != (uni_char)key[i])
				return -1;
		}
		return keyLen;
	}

	return -1;
}


BOOL HotlistParser::IsYes(const uni_char* candidate)
{
	// faster and less complex than uni_stricmp
	return candidate && (candidate[0] & 0xdf) == 'Y' && (candidate[1] & 0xdf) == 'E' && (candidate[2] & 0xdf) == 'S';
}

BOOL HotlistParser::IsNo(const uni_char* candidate)
{
	// faster and less complex than uni_stricmp
	return candidate && (candidate[0] & 0xdf) == 'N' && (candidate[1] & 0xdf) == 'O';
}


void HotlistParser::ConvertFromNewline(OpString &dest, const uni_char* src, const uni_char* replacement)
{
	int subLen = 0;
	int subStart = 0;

	int len = src ? uni_strlen(src) : 0;

	//Fix for bug [184935] The length of a note is unlimited, notes can be very large
	//                     Necessary to reserve buffer to avoid unneccesary Grows in
	//                     OpString.
	dest.Reserve(len);

	for (int i=0; i<len; i++)
	{
		if (src[i] == '\r')
		{
			if (src[i+1] == '\n')
			{
				// "\r\n" Windows
				i++;
			}
		}
		else if (src[i] == '\n')
		{
			if (src[i+1] == '\r')
			{
				// "\n\r" Mac ?
				i++;
			}
		}
		else
		{
			if (subLen == 0)
			{
				subStart = i;
			}
			subLen ++;
			continue;
		}

		if (subLen > 0)
		{
			dest.Append(&src[subStart], subLen);
			subLen = 0;
		}

		dest.Append(replacement);
	}

	if (subLen > 0)
	{
		dest.Append(&src[subStart], subLen);
	}
}

void HotlistParser::ConvertToNewline(OpString &dest, const uni_char* src)
{
	int subLen = 0;
	int subStart = 0;

	int len = src ? uni_strlen(src) : 0;

	//Fix for bug [184935] The length of a note is unlimited, notes can be very large
	//                     Necessary to reserve buffer to avoid unneccesary Grow's in
	//                     OpString.
	dest.Reserve(len);

	for (int i=0; i<len; i++)
	{
		if (src[i] == 2)
		{
			if (src[i+1] == 2)
			{
				i++;
			}
		}
		else
		{
			if (subLen == 0)
			{
				subStart = i;
			}
			subLen ++;
			continue;
		}

		if (subLen > 0)
		{
			dest.Append(&src[subStart], subLen);
			subLen = 0;
		}

		dest.Append(NEWLINE);
	}

	if (subLen > 0)
	{
		dest.Append(&src[subStart], subLen);
		subLen = 0;
	}
}



void HotlistParser::ConvertFromComma(OpString &dest, const uni_char* src, const uni_char* replacement)
{
	int subLen = 0;
	int subStart = 0;

	int len = src ? uni_strlen(src) : 0;
	for (int i=0; i<len; i++)
	{
		if (src[i] != ',')
		{
			if (subLen == 0)
			{
				subStart = i;
			}
			subLen ++;
			continue;
		}

		if (subLen > 0)
		{
			dest.Append(&src[subStart], subLen);
			subLen = 0;
		}

		dest.Append(replacement);
	}

	if (subLen > 0)
	{
		dest.Append(&src[subStart], subLen);
	}
}


void HotlistParser::ConvertToComma(OpString &dest, const uni_char* src)
{
	int subLen = 0;
	int subStart = 0;

	int len = src ? uni_strlen(src) : 0;
	for (int i=0; i<len; i++)
	{
		if (src[i] == 2)
		{
			if (src[i+1] == 2)
			{
				i++;
			}
		}
		else
		{
			if (subLen == 0)
			{
				subStart = i;
			}
			subLen ++;
			continue;
		}

		if (subLen > 0)
		{
			dest.Append(&src[subStart], subLen);
			subLen = 0;
		}
		dest.Append(UNI_L(","));
	}

	if (subLen > 0)
	{
		dest.Append(&src[subStart], subLen);
		subLen = 0;
	}
}


BOOL HotlistParser::GetNetscapeTagValue(const uni_char* haystack, const uni_char* tag, OpString &value)
{
	if (haystack && tag)
	{
		int tag_len = uni_strlen(tag);

		const uni_char *p = uni_stristr(haystack, tag);
		if (p && p[tag_len] == '=')
		{
			int i=0;
			int num_ranges = 0;
			int range[2]={-1,-1};

			for (i=tag_len; p[i]; i++)
			{
				if (p[i] == '"')
				{
					range[num_ranges++] = i;
					if (num_ranges == 2)
					{
						break;
					}
				}
			}
			if (num_ranges == 2)
			{
				int length = range[1]-range[0]-1;
				if (length > 0)
				{
					value.Set(&p[range[0]+1], length);
				}
				return TRUE;
			}
		}
	}
	return FALSE;
}


BOOL HotlistParser::GetNetscapeDescription(const uni_char* haystack, OpString &value)
{
	if (haystack)
	{
		const uni_char* p1 = uni_stristr(haystack, UNI_L("<BR>"));
		if (p1)
		{
			int length = uni_strlen(haystack)-(p1-haystack);
			if (length > 0)
			{
				value.Set(haystack, length);
			}
			return TRUE;
		}
		else
		{
			value.Set(haystack);
			return FALSE;
		}
	}
	return FALSE;
}

BookmarkItem*	HotlistParser::AddNewBookmarkFolder(const uni_char* name,INT32 created,INT32 visited)
{
	BookmarkItem* item = OP_NEW(BookmarkItem, ());
	if(item)
	{
		item->SetFolderType(FOLDER_NORMAL_FOLDER);
		if(name)
			SetAttribute(item, BOOKMARK_TITLE,	 name);
		if(created)
			SetAttribute(item, BOOKMARK_CREATED, created);
		if(visited)
			SetAttribute(item, BOOKMARK_VISITED, visited);

		if (OpStatus::IsSuccess(DesktopBookmarkManager::AddCoreItem(item, /*previous?*/ 0, m_stack.Peek() ? m_stack.Peek() : 0)))
		{
			m_stack.Push(item);
		}
		else
		{
			OP_DELETE(item);
			item = 0;
		}
	}

	return item;
}

BookmarkItem*	HotlistParser::AddNewBookmark(const uni_char* name, const uni_char* url , INT32 created , INT32 visited )
{
	BookmarkItem* item = OP_NEW(BookmarkItem, ());
	if(item)
	{
		item->SetFolderType(FOLDER_NO_FOLDER);
		if(name)
			SetAttribute(item, BOOKMARK_TITLE,	 name);
		if(url)
			SetAttribute(item, BOOKMARK_URL,	 url);
		if(created)
			SetAttribute(item, BOOKMARK_CREATED, created);
		if(visited)
			SetAttribute(item, BOOKMARK_VISITED, visited);

		if (!OpStatus::IsSuccess(DesktopBookmarkManager::AddCoreItem(item, /*previous?*/ 0, m_stack.Peek() ? m_stack.Peek() : 0)))
		{
			OP_DELETE(item);
			item = 0;
		}
	}

	return item;
}

