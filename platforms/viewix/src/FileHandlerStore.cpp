/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#include "core/pch.h"

#include "modules/util/OpHashTable.h"

#include "platforms/viewix/FileHandlerManager.h"
#include "platforms/viewix/src/HandlerElement.h"
#include "platforms/viewix/src/FileHandlerStore.h"
#include "platforms/viewix/src/FileHandlerManagerUtilities.h"
#include "platforms/viewix/src/nodes/ApplicationNode.h"
#include "platforms/viewix/src/nodes/MimeTypeNode.h"
#include "platforms/viewix/src/input_files/DefaultListFile.h"
#include "platforms/viewix/src/input_files/DesktopFile.h"
#include "platforms/viewix/src/input_files/GlobsFile.h"
#include "platforms/viewix/src/input_files/MimeInfoCacheFile.h"
#include "platforms/viewix/src/input_files/ProfilercFile.h"
#include "platforms/viewix/src/input_files/GnomeVFSFile.h"

/***********************************************************************************
 ** FileHandlerStore - Constructor
 **
 **
 **
 **
 ***********************************************************************************/
FileHandlerStore::FileHandlerStore()
{
    m_mime_table.SetHashFunctions(&m_hash_function);
    m_globs_table.SetHashFunctions(&m_hash_function);
    m_default_table.SetHashFunctions(&m_hash_function);
    m_application_table.SetHashFunctions(&m_hash_function);
}


/***********************************************************************************
 ** FileHandlerStore - Destructor
 **
 **
 **
 **
 ***********************************************************************************/
FileHandlerStore::~FileHandlerStore()
{
	EmptyStore();
}

/***********************************************************************************
 ** EmptyStore
 **
 **
 **
 **
 ***********************************************************************************/
void FileHandlerStore::EmptyStore()
{
    OP_STATUS ret_val;

    // Empty out the application table:
    OpHashIterator* it = GetApplicationTable().GetIterator();
    ret_val = it->First();

    while (OpStatus::IsSuccess(ret_val))
    {
		uni_char * key = (uni_char *)it->GetKey();
		ApplicationNode * node = (ApplicationNode *)it->GetData();
		OP_DELETEA(key);
		OP_DELETE(node);
		ret_val = it->Next();
    }

    OP_DELETE(it);

    // Empty out the globs table:
    it = GetGlobsTable().GetIterator();
    ret_val = it->First();

    while (OpStatus::IsSuccess(ret_val))
    {
		uni_char * key = (uni_char *)it->GetKey();
		GlobNode * node = (GlobNode *)it->GetData();
		OP_DELETEA(key);
		OP_DELETE(node);
		ret_val = it->Next();
    }

    OP_DELETE(it);

    // Empty out the default table:
    it = GetDefaultTable().GetIterator();
    ret_val = it->First();

    while (OpStatus::IsSuccess(ret_val))
    {
		uni_char * key = (uni_char *)it->GetKey();
		OP_DELETEA(key);
		// node was deleted when emptying application table.
		ret_val = it->Next();
    }

    OP_DELETE(it);

    m_mime_table.RemoveAll();
    m_globs_table.RemoveAll();
    m_default_table.RemoveAll();
    m_application_table.RemoveAll();

    m_nodelist.DeleteAll();
	m_nodelist.Clear();

    m_complex_globs.DeleteAll();
	m_complex_globs.Clear();

	OP_ASSERT(GetNumberOfMimeTypes() == 0);
}

/***********************************************************************************
 ** GetGlobsTable
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS FileHandlerStore::InitGlobsTable(OpVector<OpString>& files)
{
    // Initialize them:
    for(UINT32 i = 0; i < files.GetCount(); i++)
    {
		GlobsFile globs_file;
		globs_file.Parse(*files.Get(i));
    }

    return OpStatus::OK;
}


/***********************************************************************************
 ** GetDefaultTable
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS FileHandlerStore::InitDefaultTable(OpVector<OpString>& profilerc_files,
											 OpVector<OpString>& files)
{
    UINT32 i = 0;

    for(i = 0; i < profilerc_files.GetCount(); i++)
    {
		ProfilercFile profilerc_file;
		profilerc_file.Parse(*profilerc_files.Get(i));
    }

    for(i = 0; i < files.GetCount(); i++)
    {
		DefaultListFile default_file;
		default_file.Parse(*files.Get(i));
    }

    return OpStatus::OK;
}


/***********************************************************************************
 ** GetMimeTable
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS FileHandlerStore::InitMimeTable(OpVector<OpString>& files,
										  OpVector<OpString>& gnome_files)
{
    UINT32 i = 0;

    for(i = 0; i < files.GetCount(); i++)
    {
		MimeInfoCacheFile mime_info_file;
		mime_info_file.Parse(*files.Get(i));
    }

    for(i = 0; i < gnome_files.GetCount(); i++)
    {
		GnomeVFSFile gnome_file;
		gnome_file.Parse(*gnome_files.Get(i));
    }

    return OpStatus::OK;
}




/***********************************************************************************
 ** GetMimeTypeNode
 **
 ** Gets a mimenode and returns a pointer to it.
 **
 ** @return pointer to mimenode (or 0 if it doesn't exist)
 **
 ** NOTE:
 ***********************************************************************************/
MimeTypeNode* FileHandlerStore::GetMimeTypeNode(const OpStringC & mime_type)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    // mime_type has to be set
    OP_ASSERT(mime_type.HasContent());
    if (mime_type.HasContent())
    {
		void *node = 0;
		// Look up mimetype in mime table - Reuse node if possible:
		m_mime_table.GetData((void *) mime_type.CStr(), &node);

		return (MimeTypeNode*) node;
    }
    return 0;
}


/***********************************************************************************
 ** MakeMimeTypeNode
 **
 ** Makes a mimenode (if it doesn't already exist) and returns a pointer to it.
 **
 ** @return pointer to created mimenode (or 0 if OOM)
 **
 ** NOTE:
 ***********************************************************************************/
MimeTypeNode* FileHandlerStore::MakeMimeTypeNode(const OpStringC & mime_type)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    // mime_type has to be set
    OP_ASSERT(mime_type.HasContent());
    if(mime_type.IsEmpty())
		return 0;
    //-----------------------------------------------------

    MimeTypeNode* node = GetMimeTypeNode(mime_type);

    if(!node)
    {
		node = OP_NEW(MimeTypeNode, (mime_type));
		m_nodelist.Add(node);

		if(node)
		{
			const uni_char * key = node->GetMimeTypes().Get(0)->CStr();
			m_mime_table.Add((void *) key, (void *) node);
		}
    }

    return node;
}


/***********************************************************************************
 ** LinkMimeTypeNode
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS FileHandlerStore::LinkMimeTypeNode(const OpStringC & mime_type,
											 MimeTypeNode* node,
											 BOOL subclass_type)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    // mime_type has to be set
    OP_ASSERT(mime_type.HasContent());
    if(mime_type.IsEmpty())
		return OpStatus::ERR;

    // node has to be set
    OP_ASSERT(node);
    if(!node)
		return OpStatus::ERR;
    //-----------------------------------------------------

    //Make sure the mimetype is not already linked :
    MimeTypeNode* existing_node = GetMimeTypeNode(mime_type);

    if(!existing_node)
    {

		OpString * mime_type_str = 0;

		if(subclass_type)
			mime_type_str = node->AddSubclassType(mime_type);
		else
			mime_type_str = node->AddMimeType(mime_type);

		if(!mime_type_str)
			return OpStatus::ERR_NO_MEMORY;

		m_mime_table.Add((void *) mime_type_str->CStr(), (void *) node);
    }

    return OpStatus::OK;
}


/***********************************************************************************
 ** MergeMimeNodes
 **
 ** Merges node_1 into node_2
 **
 **
 ***********************************************************************************/
OP_STATUS FileHandlerStore::MergeMimeNodes(MimeTypeNode* node_1, MimeTypeNode* node_2)
{
    if(!node_1 || !node_2)
		return OpStatus::OK;

    if(node_1 == node_2)
		return OpStatus::OK;

    unsigned int i = 0;

	// Remove referrences to node_1
	for(i = 0; i < node_1->GetMimeTypes().GetCount(); i++)
    {
		// Get the mimetype :
		OpString * mime_type_1 = node_1->GetMimeTypes().Get(i);

		// Remove the mapping:
		void * tmp_node = 0;
		OpStatus::Ignore(m_mime_table.Remove(mime_type_1->CStr(), &tmp_node));
		OP_ASSERT((MimeTypeNode*)tmp_node == node_1);
    }

	// Make sure node_1 is deleted even if we are not successful in the merging
	m_nodelist.RemoveByItem(node_1);
	OpAutoPtr<MimeTypeNode> anchor(node_1);

    // Merge the mimetypes and map the mime types to node_2 instead
    for(i = 0; i < node_1->GetMimeTypes().GetCount(); i++)
    {
		// Get the mimetype :
		OpString * mime_type_1 = node_1->GetMimeTypes().Get(i);

		// Add the new mapping:
		OpString * mime_type_2 = node_2->AddMimeType(mime_type_1->CStr());

		if(!mime_type_2)
			return OpStatus::ERR_NO_MEMORY;

		OP_ASSERT(m_mime_table.Contains((void *) mime_type_2->CStr()) == FALSE);

		RETURN_IF_ERROR(m_mime_table.Add((void *) mime_type_2->CStr(), (void *) node_2));
    }

    // Merge the default
    if(node_1->GetDefaultApp())
		node_2->SetDefault(node_1->GetDefaultApp());

    // Merge the applications
    OpHashIterator* it = node_1->m_applications->GetIterator();

    OP_STATUS status = it->First();

    while (OpStatus::IsSuccess(status))
    {
		ApplicationNode * app = (ApplicationNode*) it->GetData();

		status = node_2->AddApplication(app);

		if(OpStatus::IsError(status))
			break;
		else
			status = it->Next();
    }

    OP_DELETE(it);

    return status;
}


/***********************************************************************************
 ** GetGlobNode
 **
 ** Gets a globnode and returns a pointer to it.
 **
 ** @return pointer to globnode (or 0 if it doesn't exist)
 **
 ** NOTE:
 ***********************************************************************************/
GlobNode* FileHandlerStore::GetGlobNode(const OpStringC & glob)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    // glob has to be set
    OP_ASSERT(glob.HasContent());
    if (glob.HasContent())
    {
		void* node = 0;

		//Look up mimetype in mime table - Reuse node if possible:
		m_globs_table.GetData((void *) glob.CStr(), &node);

		return (GlobNode*) node;
    }
    return 0;
}


/***********************************************************************************
 ** MakeGlobNode
 **
 ** Makes a globnode (if it doesn't already exist) and returns a pointer to it.
 **
 ** @return pointer to created globnode (or 0 if OOM)
 **
 ** NOTE:
 ***********************************************************************************/
GlobNode* FileHandlerStore::MakeGlobNode(const OpStringC & glob,
										 const OpStringC & mime_type,
										 GlobType type)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    // mime_type has to be set
    OP_ASSERT(mime_type.HasContent());
    if(mime_type.IsEmpty())
		return 0;

    // glob has to be set
    OP_ASSERT(glob.HasContent());
    if(glob.IsEmpty())
		return 0;
    //-----------------------------------------------------

    GlobNode* node = GetGlobNode(glob);

    if(!node)
    {
		node = OP_NEW(GlobNode, (glob, mime_type, GLOB_TYPE_SIMPLE));

		if(node)
		{
			m_globs_table.Add((void *) node->GetKey(), (void *) node);
		}
    }

    return node;
}


/***********************************************************************************
 ** MakeApplicationNode
 **
 ** Makes a application node and returns a pointer to it.
 **
 ** @return pointer to created application node (or 0 if element is 0)
 ***********************************************************************************/
ApplicationNode* FileHandlerStore::MakeApplicationNode(HandlerElement* element)
{
    if(!element)
		return 0;

    return MakeApplicationNode(0, 0, element->handler, 0, FALSE);
}


/***********************************************************************************
 ** MakeApplicationNode
 **
 ** Makes a application node and returns a pointer to it.
 **
 ** @return pointer to created application node (or 0 if OOM or if both desktop_file_name
 **         and command are 0)
 **
 ** NOTE: desktop_file_name, path, command and icon can all be 0, but either command
 **       or desktop_file_name have to be set.
 ***********************************************************************************/
ApplicationNode* FileHandlerStore::MakeApplicationNode(const OpStringC & desktop_file_name,
													   const OpStringC & path,
													   const OpStringC & comm,
													   const OpStringC & icon,
													   BOOL do_guessing)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    //Either desktop_file_name or command has to be set
    OP_ASSERT(desktop_file_name.HasContent() || comm.HasContent());
    if(desktop_file_name.IsEmpty() && comm.IsEmpty())
		return 0;
    //-----------------------------------------------------

    ApplicationNode* node = 0;

    OpString command;
    OpString desktop_filename;
    OpString desktop_path;

    command.Set(comm);
    desktop_filename.Set(desktop_file_name);
    desktop_path.Set(path);

    command.Strip();
    desktop_filename.Strip();
    desktop_path.Strip();

    if (do_guessing && desktop_filename.IsEmpty() && command.HasContent())
		FindDesktopFile(command, desktop_filename, desktop_path);

    // Look up application in application table - Reuse node if possible:
	void *lookup_node = 0;

	if (desktop_filename.HasContent())
		m_application_table.GetData(desktop_filename.CStr(), &lookup_node);
	else if (command.HasContent())
		m_application_table.GetData(command.CStr(), &lookup_node);
	else
		return NULL; // It did not have neither desktop_filename or command so just bail

	node = (ApplicationNode*) lookup_node;

    if (!node) //If no node was present make a new one and insert it into the table
    {
		node = OP_NEW(ApplicationNode, (desktop_filename, desktop_path, command, icon));
		if(node)
			m_application_table.Add(node->GetKey(), node);
    }

    return node;
}


/***********************************************************************************
 ** FindDesktopFile
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS FileHandlerStore::FindDesktopFile(const OpStringC & command,
											OpString & desktop_filename,
											OpString & desktop_path)
{
    OpAutoVector<OpString> words;
    FileHandlerManagerUtilities::SplitString(words, command, ' ');

    if(words.GetCount())
    {
		OpString command_name;
		command_name.AppendFormat(UNI_L("%s.desktop"), words.Get(0)->CStr());

		OpAutoVector<OpString> files;
		FileHandlerManager::GetManager()->FindDesktopFile(command_name, files);

		if(files.GetCount())
		{
			OpString * fullpath = files.Get(0);

			OpString path;
			FileHandlerManagerUtilities::GetPath(*fullpath, path);

			OpString filename;
			FileHandlerManagerUtilities::StripPath(filename, *fullpath);

			desktop_filename.Set(filename.CStr());
			desktop_path.Set(path.CStr());
		}
    }

    return OpStatus::OK;
}


/***********************************************************************************
 ** InsertIntoApplicationList
 **
 ** Attempts to reuse an application node (or will attempt to create one) for the
 ** handler. If one is found/created, it will be added to the nodes applications
 ** list.
 **
 ** @param node         - head of a MimeTypeNode list
 ** @param desktop_file - name of desktop file for this application
 ** @param path         - path to desktop file for this application
 **
 **  NOTE: desktop_file, path and command can all be 0, but either command
 **        or desktop_file have to be set for MakeApplicationNode to succeed.
 ***********************************************************************************/
ApplicationNode * FileHandlerStore::InsertIntoApplicationList(MimeTypeNode* node,
															  const OpStringC & desktop_file,
															  const OpStringC & path,
															  const OpStringC & command,
															  BOOL  default_app)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    //Mime-type node cannot be null
    OP_ASSERT(node);
    if(!node)
		return 0;
    //-----------------------------------------------------

    ApplicationNode * app = 0;
    app = MakeApplicationNode(desktop_file, path, command, 0); //OOM

    return InsertIntoApplicationList(node, app, default_app);
}


/***********************************************************************************
 ** InsertIntoApplicationList
 **
 ** Inserts the application into the mime nodes application list or makes it default
 ***********************************************************************************/
ApplicationNode * FileHandlerStore::InsertIntoApplicationList(MimeTypeNode* node,
															  ApplicationNode * app,
															  BOOL  default_app)
{
    //-----------------------------------------------------
    // Parameter checking:
    //-----------------------------------------------------
    //Mime-type node cannot be null
    OP_ASSERT(node);
    if(!node)
		return 0;

    OP_ASSERT(app);
    if(!app)
		return 0; //Application could not be created (OOM or error)
    //-----------------------------------------------------

    if(default_app)
		node->SetDefault(app);
    else
		node->AddApplication(app);

    return app;
}


/***********************************************************************************
 ** PrintDefaultTable
 **
 ** Prints the default table supplied using printf.
 **
 ** @param the default table to be printed
 **
 **
 ***********************************************************************************/
void FileHandlerStore::PrintDefaultTable()
{
    OpHashIterator*	it = m_default_table.GetIterator();
    printf("_____________________________________________________________________ \n");
    printf("___________________________ DEFAULT TABLE ___________________________ \n");
    printf("_____________________________________________________________________ \n");
    INT32 i = 0;

    OP_STATUS ret_val = it->First();

    while (OpStatus::IsSuccess(ret_val))
    {
		i++;
		uni_char * key = (uni_char *) it->GetKey();
		OpString8 tmp;
		tmp.Set(key);
		printf("%d.\tKey   : %s\n", i, tmp.CStr());
		MimeTypeNode* node = (MimeTypeNode* ) it->GetData();

		if(node)
		{
			ApplicationNode * app = node->GetDefaultApp();

			if(app)
			{
				OpString8 tmp2;
				tmp.Set(app->GetDesktopFileName().CStr());
				tmp2.Set(app->GetPath().CStr()); //OOM
				printf("\tValue : %s \t %s \n", tmp.CStr(), tmp2.CStr());
			}
		}

		ret_val = it->Next();
    }
    printf("_____________________________________________________________________ \n");
}


/***********************************************************************************
 ** PrintMimeTable
 **
 ** Prints the mime table supplied using printf.
 **
 ** @param the mime table to be printed
 **
 **
 ***********************************************************************************/
void FileHandlerStore::PrintMimeTable()
{
    OpHashIterator*	it = m_mime_table.GetIterator();
    printf("_____________________________________________________________________ \n");
    printf("___________________________ MIME TABLE ______________________________ \n");
    printf("_____________________________________________________________________ \n");
    INT32 i = 0;

    OP_STATUS ret_val = it->First();
    while (OpStatus::IsSuccess(ret_val))
    {
		i++;
		uni_char * key = (uni_char *) it->GetKey();
		OpString8 tmp;
		tmp.Set(key);
		printf("%d.\tKey   : %s\n", i, tmp.CStr());
		MimeTypeNode* node = (MimeTypeNode* ) it->GetData();

		if(node)
		{
			ApplicationNode * app = 0;

			OpHashIterator*	it2 = node->m_applications->GetIterator();
			OP_STATUS ret_val2 = it2->First();

			while (OpStatus::IsSuccess(ret_val2))
			{
				app = (ApplicationNode*) it2->GetData();

				if(app)
				{
					OpString8 tmp2;
					tmp.Set(app->GetDesktopFileName().CStr());
					tmp2.Set(app->GetCommand().CStr()); //OOM
					printf("\tValue : %s \t %s \n", tmp.CStr(), tmp2.CStr());
				}

				ret_val2 = it2->Next();
			}
		}
		ret_val = it->Next();
    }
    printf("_____________________________________________________________________ \n");
}


/***********************************************************************************
 ** PrintGlobsTable
 **
 ** Prints the globs table supplied using printf.
 **
 ** @param the globs table to be printed
 **
 **
 ***********************************************************************************/
void FileHandlerStore::PrintGlobsTable()
{
    OpHashIterator*	it = m_globs_table.GetIterator();

    printf("_____________________________________________________________________ \n");
    printf("___________________________ GLOBS TABLE _____________________________ \n");
    printf("_____________________________________________________________________ \n");
    INT32 i = 0;

    OP_STATUS ret_val = it->First();
    while (OpStatus::IsSuccess(ret_val))
    {
		i++;
		OpString8 tmp;
		OpString8 tmp2;

		uni_char * key = (uni_char *) it->GetKey();

		tmp.Set(key);
		printf("%d.\tKey   : %s\n", i, tmp.CStr());
		GlobNode* node = (GlobNode* ) it->GetData();

		if(node)
		{
			tmp.Set(node->GetGlob().CStr());
			tmp2.Set(node->GetMimeType().CStr());
			printf("\tValue : %s \t %s \n", tmp.CStr(), tmp2.CStr());
		}

		ret_val = it->Next();
    }
    printf("_____________________________________________________________________ \n");
}



/***********************************************************************************
 ** GetGlobNode
 **
 **
 **
 **
 ***********************************************************************************/
GlobNode * FileHandlerStore::GetGlobNode(const OpStringC & filename,
										 BOOL strip_path)
{
	OP_ASSERT(filename.HasContent());

    // _GUESS_ based on extenstion :

    //Remove the path if strip_path is set :
    OpString file_name;

    if(strip_path)
    {
		FileHandlerManagerUtilities::StripPath(file_name, filename);
    }
    else
    {
		file_name.Set(filename.CStr());
    }

    // Literal:
    GlobNode * node = GetGlobNode(file_name.CStr());

    // Simple:
    if (!node)
    {
		OpAutoVector<OpString> globs;
		FileHandlerManagerUtilities::MakeGlobs(file_name, globs);

		for(UINT32 i = 0; i < globs.GetCount(); i++)
		{
			node = GetGlobNode(globs.Get(i)->CStr());

			if(node) //Found
				break;
		}
    }

    // Complex:
    if(!node)
    {
		//TODO: Test for complex in vector
    }

    return node;
}
