/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef __FILEHANDLER_STORE_H__
#define __FILEHANDLER_STORE_H__

#include "platforms/viewix/src/nodes/FileHandlerNode.h"
#include "platforms/viewix/src/nodes/GlobNode.h"
#include "platforms/viewix/src/StringHash.h"

class ApplicationNode;
class MimeTypeNode;
class GlobNode;
class HandlerElement;

class FileHandlerStore
{
 public:

	// Initialize

	FileHandlerStore();

	~FileHandlerStore();

    OP_STATUS InitGlobsTable(OpVector<OpString>& files);

    OP_STATUS InitDefaultTable(OpVector<OpString>& profilerc_files,
							   OpVector<OpString>& files);

    OP_STATUS InitMimeTable(OpVector<OpString>& files,
							OpVector<OpString>& gnome_files);

	// Append to Nodes

    OP_STATUS LinkMimeTypeNode(const OpStringC & mime_type,
							   MimeTypeNode* node,
							   BOOL subclass_type = FALSE);

	// Will delete node_1 since node_1 will be merged into node_2 (if both are non null and not equal)
	OP_STATUS MergeMimeNodes(MimeTypeNode* node_1, MimeTypeNode* node_2);

    ApplicationNode * InsertIntoApplicationList(MimeTypeNode* node,
												const OpStringC & desktop_file,
												const OpStringC & path,
												const OpStringC & command,
												BOOL  default_app = FALSE);

	ApplicationNode * InsertIntoApplicationList(MimeTypeNode* node,
												ApplicationNode * app,
												BOOL  default_app);


	// Nodes

    GlobNode * GetGlobNode(const OpStringC & glob);

	GlobNode * GetGlobNode(const OpStringC & filename, BOOL strip_path);

    GlobNode * MakeGlobNode(const OpStringC & glob,
							const OpStringC & mime_type,
							GlobType type);

    ApplicationNode * MakeApplicationNode(const OpStringC & desktop_file_name,
										  const OpStringC & path,
										  const OpStringC & command,
										  const OpStringC & icon,
										  BOOL do_guessing = TRUE);

	ApplicationNode * MakeApplicationNode(HandlerElement* element);

    MimeTypeNode* GetMimeTypeNode(const OpStringC & mime_type);

    MimeTypeNode* MakeMimeTypeNode(const OpStringC & mime_type);


	// Print

    void PrintDefaultTable();

    void PrintMimeTable();

    void PrintGlobsTable();


	// Get datastructures

	OpHashTable & GetMimeTable() { return m_mime_table; }

	OpHashTable & GetGlobsTable() { return m_globs_table; }

	OpHashTable & GetDefaultTable() { return m_default_table; }

	OpHashTable & GetApplicationTable() { return m_application_table; }

	OpVector<GlobNode> & GetComplexGlobs() { return m_complex_globs; }

	int GetNumberOfMimeTypes() { return m_nodelist.GetCount(); }

	void EmptyStore();

 private:

	OP_STATUS FindDesktopFile(const OpStringC & command,
							  OpString & desktop_filename,
							  OpString & desktop_path);

    StringHash m_hash_function;

    OpHashTable m_mime_table;
    OpHashTable m_globs_table;
    OpHashTable m_default_table;
    OpHashTable m_application_table;

    OpAutoVector<MimeTypeNode> m_nodelist;

    OpAutoVector<GlobNode> m_complex_globs;
};

#endif //__FILEHANDLER_STORE_H__
