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

#ifndef __HOTLIST_PARSER_H__
#define __HOTLIST_PARSER_H__

#include "modules/util/adt/opvector.h"
#include "modules/util/opstring.h"
#include "modules/util/opfile/opsafefile.h"
#include "modules/bookmarks/bookmark_item.h"

#include "adjunct/quick/hotlist/HotlistModel.h"
#include "adjunct/quick/models/FolderStack.h"
#include "adjunct/quick/models/BookmarkModel.h"

class HotlistFileReader;
class HotlistFileWriter;
class HotlistModel;
class HotlistXBELReader;

#define SUPPORT_IMPORT_LINKBAR_TAG
#define SUPPORT_UNKNOWN_TAGS
#define SUPPORT_WRITE_NETSCAPE_BOOKMARKS

static const char KEYWORD_URLSECTION[] = "#URL";
static const char KEYWORD_NOTESECTION[] = "#NOTE";
static const char KEYWORD_SEPERATORSECTION[] = "#SEPERATOR";
#ifdef WEBSERVER_SUPPORT
static const char KEYWORD_UNITESECTION[] = "#UNITESERVICE";
#endif // WEBSERVER_SUPPORT
static const char KEYWORD_CONTACTSECTION[] = "#CONTACT";
static const char KEYWORD_FOLDERSECTION[] = "#FOLDER";
static const char KEYWORD_GROUPSECTION[] = "#GROUP";
static const char KEYWORD_ENDOFFOLDER[] = "-";
static const char KEYWORD_NAME[] = "NAME=";
static const char KEYWORD_ID[] = "ID=";
static const char KEYWORD_CREATED[] = "CREATED=";
static const char KEYWORD_VISITED[] = "VISITED=";
static const char KEYWORD_URL[] = "URL=";

// If this value is present for a bookmark or speeddial, it should be used
// in favor of KEYWORD_URL on UI. When visiting the bookmark/speeddial 
// KEYWORD_URL should be used.
static const char KEYWORD_DISPLAY_URL[] = "DISPLAY URL=";

static const char KEYWORD_DESCRIPTION[] = "DESCRIPTION=";
static const char KEYWORD_SHORTNAME[] = "SHORT NAME=";
static const char KEYWORD_PERSONALBAR_POS[] = "PERSONALBAR_POS=";
static const char KEYWORD_PANEL_POS[] = "PANEL_POS=";
static const char KEYWORD_ON_PERSONALBAR[] = "ON PERSONALBAR=";
static const char KEYWORD_IN_PANEL[] = "IN PANEL=";
static const char KEYWORD_SMALL_SCREEN[] = "SMALL SCREEN=";
static const char KEYWORD_ACTIVE[] = "ACTIVE=";
static const char KEYWORD_EXPANDED[] = "EXPANDED=";
static const char KEYWORD_TRASH_FOLDER[] = "TRASH FOLDER=";
static const char KEYWORD_PARTNER_ID[] = "PARTNERID=";

// This is the url to an icon, typically used to set favicon for preset bookmarks,
// different from the core BOOKMARK_FAVICON_FILE attribute which is the actual data
// of the icon(base64). BOOKMARK_FAVICON_FILE is not read from the bookmark file.
// We don't read this attribute into the model, favicon manager will read this and
// fetch icons for preset bookmarks.
static const char KEYWORD_FAVICON[]      = "ICONFILE=";

static const char KEYWORD_THUMBNAIL[]    = "THUMBNAIL="; // Not used

// used to store uuids of deleted default bookmarks
static const char KEYWORD_DELETED_BOOKMARK[]  = "#DELETED";

#if defined(SUPPORT_IMPORT_LINKBAR_TAG)
// Compatibility with pre 7 files
static const char KEYWORD_LINKBAR_FOLDER[] = "LINKBAR FOLDER=";
static const char KEYWORD_LINKBAR_STOP[] = "LINKBAR STOP=";
#endif

static const char KEYWORD_MAIL[] = "MAIL=";
static const char KEYWORD_PHONE[] = "PHONE=";
static const char KEYWORD_FAX[] = "FAX=";
static const char KEYWORD_POSTALADDRESS[] = "POSTALADDRESS=";
static const char KEYWORD_PICTUREURL[] = "PICTUREURL=";
static const char KEYWORD_CONAX[] = "00: CONAX CARD NUMBER=";
static const char KEYWORD_ICON[] = "ICON=";

static const char KEYWORD_IMADDRESS[] = "IMADDRESS=";
static const char KEYWORD_IMPROTOCOL[] = "IMPROTOCOL=";
static const char KEYWORD_IMSTATUS[] = "IMSTATUS=";
static const char KEYWORD_IMVISIBILITY[] = "IMVISIBILITY=";
static const char KEYWORD_IMXPOSITION[] = "IMXPOSITION=";
static const char KEYWORD_IMYPOSITION[] = "IMYPOSITION=";
static const char KEYWORD_IMWIDTH[] = "IMWIDTH=";
static const char KEYWORD_IMHEIGHT[] = "IMHEIGHT=";
static const char KEYWORD_IMMAXIMIZED[] = "IMMAXIMIZED=";
static const char KEYWORD_M2INDEX[] = "M2INDEXID=";

//gadget keywords
static const char KEYWORD_IDENTIFIER[] = "IDENTIFIER=";	//position of the gadget window
static const char KEYWORD_CLEAN_UNINSTALL[] = "CLEAN UNINSTALL=";			//remove everything on uninstall

static const char KEYWORD_MEMBER[] = "MEMBERS=";

static const char KEYWORD_UNIQUEID[] = "UNIQUEID=";
static const char KEYWORD_TARGET[] = "TARGET=";
static const char KEYWORD_MOVE_IS_COPY[] = "MOVE_IS_COPY=";
static const char KEYWORD_DELETABLE[] = "DELETABLE=";
static const char KEYWORD_SUBFOLDER_ALLOWED[] = "SUBFOLDER_ALLOWED=";
static const char KEYWORD_SEPARATOR_ALLOWED[] = "SEPARATOR_ALLOWED=";
static const char KEYWORD_MAX_ITEMS[] = "MAX_ITEMS=";
static const char KEYWORD_ROOT[] = "ROOT=";
static const char KEYWORD_NEEDSCONFIG[] = "NEEDS_CONFIGURATION=";
static const char KEYWORD_RUNNING[] = "RUNNING=";

class HotlistParser
{
public:
	HotlistParser();
	~HotlistParser();

	OP_STATUS Parse(const OpStringC &filename, HotlistModel &model, int parent_index, int format, BOOL is_import, BOOL import_into_root_folder = FALSE);
	OP_STATUS Write(const OpString &filename, HotlistModel &model, BOOL all, BOOL safe);
	OP_STATUS WriteItems(const OpStringC &filename, HotlistModel &model, OpVector<INT32> &index_list);	
	OP_STATUS WriteHTML(const OpString &filename, BookmarkModel &model, BOOL all, BOOL safe);

	// bookmark only
	static OP_STATUS WriteDeletedDefaultBookmark(HotlistFileWriter& writer);
	static OP_STATUS WriteBookmarkNode(HotlistFileWriter& writer, HotlistModelItem& item);
	static OP_STATUS WriteTrashFolderNode(HotlistFileWriter& writer, BookmarkItem *bookmark);

	// bookmark and notes
	static OP_STATUS WriteSeparatorNode(HotlistFileWriter& writer, HotlistModelItem& item);
	static OP_STATUS WriteFolderNode(HotlistFileWriter& writer, HotlistModelItem& item, HotlistModel& model);

	static void ConvertFromNewline(OpString &dest, const uni_char* src, const uni_char* replacement);
	static void ConvertToNewline(OpString &dest, const uni_char* src);

	static BOOL FileIsPlacesSqlite(const OpStringC& filename);

private:
	BOOL IsBookmarkFormat(int format);
	
	// Input functions
	OP_STATUS ParseHeader(HotlistFileReader &reader, HotlistModel &model, BOOL is_import);

	void AddItem(HotlistModelItem *item, HotlistModel &model, BOOL is_import);
	HotlistModelItem *AddAndCreateNew(HotlistModelItem *item, HotlistModel &model, BOOL is_import,
									  OpTypedObject::Type new_type, OP_STATUS &type_status);

	OP_STATUS Parse(HotlistFileReader &reader, HotlistModel &model,BOOL is_import);
	OP_STATUS ImportNetscapeSqlite(const OpStringC& filename,BOOL into_sub_folder = TRUE);
	OP_STATUS ImportNetscapeHTML(const OpStringC& filename,  BOOL into_sub_folder = TRUE);
	OP_STATUS ImportNetscapeHTML(HotlistFileReader &reader,  BOOL into_sub_folder = TRUE);
	OP_STATUS ImportExplorerFavorites(const OpStringC &path, BOOL into_sub_folder = TRUE);
	OP_STATUS ImportKDE1Bookmarks(const OpStringC& path);
	OP_STATUS ImportKonquerorBookmarks(const OpStringC& filename);
#if defined(_MACINTOSH_) && defined(SAFARI_BOOKMARKS_SUPPORT)
	OP_STATUS ImportSafariBookmarks(CFArrayRef children, const OpStringC& top_folder_name);
#else
	OP_STATUS ImportXBEL(HotlistXBELReader& reader, INT32& index, INT32 folder_count, const OpStringC& top_folder_name);
#endif
	OP_STATUS ImportDirectory(const OpStringC& path, const OpStringC& extensions, const OpStringC& top_folder_name);

	//OP_STATUS CheckLevelForDuplicates(HotlistModelItem *item, HotlistModelItem *parent_item, HotlistModel &work_model, HotlistModel &existing_model);

	// Output functions
	OP_STATUS Write(HotlistFileWriter &writer, HotlistModel &model, BOOL all);
	OP_STATUS Write(HotlistFileWriter &writer, HotlistModel &model, INT32 index, BOOL recursive, BOOL all, INT32& next_index);

	OP_STATUS WriteHTML(HotlistFileWriter& writer, BookmarkModel& model, BOOL all);
	OP_STATUS WriteHTML(HotlistFileWriter& writer, BookmarkModel& model, INT32 index, BOOL& first, BOOL all, INT32& next_index, INT32 indent);

	OP_STATUS WriteNoteNode(HotlistFileWriter& writer, HotlistModelItem& item);
	OP_STATUS WriteContactNode(HotlistFileWriter& writer, HotlistModelItem& item);
	OP_STATUS WriteGroupNode(HotlistFileWriter& writer, HotlistModelItem& item);
	OP_STATUS WriteGadgetNode(HotlistFileWriter& writer, HotlistModelItem& item);
#ifdef WEBSERVER_SUPPORT
	OP_STATUS WriteUniteServiceNode(HotlistFileWriter& writer, HotlistModelItem& item);
#endif // WEBSERVER_SUPPORT

	// Helper functions
	int IsKey(const uni_char* candidate, int candidateLen,
			   const char* key, int keyLen);
	BOOL IsYes(const uni_char* candidate);
	BOOL IsNo(const uni_char* candidate);
	static void ConvertFromComma(OpString &dest, const uni_char* src, const uni_char* replacement);
	static void ConvertToComma(OpString &dest, const uni_char* src);
	BOOL GetNetscapeTagValue(const uni_char* haystack, const uni_char* tag, 
							  OpString &value);
	BOOL GetNetscapeDescription(const uni_char* haystack, OpString &value);

private:
	BOOL m_current_item_valid;

	BookmarkItem*	AddNewBookmark		(const uni_char* name, const uni_char* url = NULL, INT32 created = 0, INT32 visited = 0);
	BookmarkItem*	AddNewBookmarkFolder(const uni_char* name, INT32 created = 0, INT32 visited = 0);

    void SetCurrentNode(HotlistModelItem* item) { m_current_node = item; }
    void ResetCurrentNode() { m_current_node = 0; }
    HotlistModelItem* GetCurrentNode() { return m_current_node; }
    HotlistModelItem* m_current_node;
	OpString8 m_last_parent;

	FolderStack<BookmarkItem> m_stack;
};


#endif
