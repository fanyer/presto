/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef __FILEHANDLER_MANAGER_H__
#define __FILEHANDLER_MANAGER_H__

#include "modules/util/adt/opvector.h"
#include "modules/util/OpTypedObject.h"
#include "modules/util/opfile/opfile.h"

#include "platforms/viewix/src/nodes/FileHandlerNode.h"
#include "platforms/viewix/src/nodes/GlobNode.h"
#include "platforms/viewix/src/FileHandlerStore.h"
#include "platforms/viewix/src/input_files/InputFileManager.h"
#include "platforms/viewix/src/HandlerElement.h"

// Switches :
#define CHECK_DESKTOP_FILE_SECURITY  1
#define CHECK_SHELL_SCRIPT_SECURITY  1
#define CHECK_EXECUTABLE_FLAG_ITSELF 1
#define LINUX_FREEDESKTOP_INTEGRATION

class ApplicationNode;
class MimeTypeNode;
class GlobNode;
class StringHash;

/**
 *
 * Pixels :
 *
 *     1) Note that max pixel size supported (both for height and width)
 *        is 128 and min pixel size is 16.
 *	   2) Note also that the normal pixel sizes supported on linux are:
 *
 *	      16x16
 *	      22x22
 *	      32x32
 *	      48x48
 *	      64x64
 *	      128x128
 *
 **/

class FileHandlerManager
{
	friend class FileHandlerNode;
	friend class FileHandlerStore;
	friend class InputFileManager;
	friend class AliasesFile;
	friend class SubclassesFile;
	friend class DefaultListFile;
	friend class GlobsFile;
	friend class MailcapFile;
	friend class MimeInfoCacheFile;
	friend class ProfilercFile;
	friend class GnomeVFSFile;

public:

	/**
	   Get method for singleton class FileHandlerManager
	   @return pointer to the instance of class FileHandlerManager
	*/
	static FileHandlerManager* GetManager();

	/**
	   Scans file system for applicatons that can handle filetypes. This
	   function can block for some time (seconds) anf should not be used
	   until necessary. Opera will call this function automatically if there
	   has been no mouse or keyboard input for some time.
	 */
	void DelayedInit();

	/**
	   Gets a file handler for the mime-type of the file (if any)

	   Current priority:

	   1. User specified handler (if no contenttype is supplied)*
	   2. System default handler
	   3. First in list of handlers for this mime-type (should be considered random
	   since implementation could change here)

	   @param filename         - name of file (eg. document.pdf)
	   @param content_type     - reference to a (possibly empty) string with the known content type
	   @param handler          - reference to string where handler (if any) will be placed
	   @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if OOM

	   * If content type is supplied the client should use the viewers to find handler
	     and GetFileHandler should only present system defaults.
	*/
	OP_STATUS GetFileHandler( const OpString& filename,
							  const OpString& content_type,
							  OpString &handler,
							  OpString &handler_name);

	/**
	   Gets a file handler for the protocol of the uri (if any)

	   Current priority:

	   1. System default handler
	   2. First in list of handlers for this protocol (should be considered random
	   since implementation could change here)

	   @param uri_string       - the uri one wants a handler for (possibly empty)
	   @param protocol         - reference to a (possibly empty) string with the known protocol
	   @param handler          - reference to string where handler (if any) will be placed
	   @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if OOM
	*/
	OP_STATUS GetProtocolHandler( const OpString &uri_string,
								  const OpString& protocol,
								  OpString &handler);

	/**
	   Gets all file handlers for the mime type of the file. The file handlers specified
	   in the opera_default_handler and default_handler are not repeated in the handlers
	   vector.

	   @param filename              - name of file (eg. document.pdf)
	   @param content_type          - reference to a string with the content type
	   @param handlers              - reference to vector where handlers (if any) will be placed
	   @param default_handler       - reference to string where default handler (if any) will be placed
	   @param opera_default_handler - reference to string where opera default handler (if any) will be placed
	   @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if OOM
	*/
	OP_STATUS GetFileHandlers( const OpString& filename,
							   const OpString &content_type,
							   OpVector<OpString>& handlers,
							   OpVector<OpString>& handler_names,
							   OpVector<OpBitmap>& handler_icons,
							   URLType type,
							   UINT32 icon_size = 16);

	/**
	   Attempts to find the desktop file name of handler for file.

	   @param filename         - name of file (eg. document.pdf)
	   @param content_type     - contenttype of file
	   @param handler          - handler (eg. acroread)
	   @return name of desktop file if found, else 0
	*/
	const uni_char * GetDesktopFilename( const OpStringC & filename,
										 const OpStringC & content_type,
										 const OpStringC & handler);

	/**
	   Attempts to guess the mimetype based on the filename

	   @param filename     - filename for which to guess mimetype
	   @param content_type - string inwhich to place mimetype

	   @return will return an ERROR if setting the contenttype resulted in an error
	 */
	OP_STATUS GuessMimeType( const OpStringC & filename,
							 OpString & content_type);


	/**
	   Attempts to aquire the full path of the mime-type icon for
	   the mimetype of the file.

	   @param filename         - name of file (eg. document.pdf)
	   @param icon_size        - preferred height/width in pixels
	   @return full path to icon if found, else 0
	*/
	const uni_char * GetFileTypeIconPath( const OpStringC & filename,
										  const OpStringC & content_type,
										  UINT32 icon_size  = 16);

	/**
	   @param filename         - name of file (eg. document.pdf)
	   @param content_type     - contenttype of file
	   @param icon_size        - preferred height/width in pixels
	   @return pointer to OpBitmap containing the icon if an icon was found, else 0
	*/
	OpBitmap* GetFileTypeIcon( const OpStringC& filename,
							   const OpStringC& content_type,
							   UINT32 icon_size  = 16);

	/**
	   @param filename         - name of file (eg. document.pdf)
	   @param content_type     - contenttype of file
	   @return textual description of mimetype of file (eg. "PDF document")
	*/
	const uni_char * GetFileTypeName( const OpStringC& filename,
									  const OpStringC& content_type);

	/**
	 * Get info about the content type of a file - name of contenttype and icon for it.
	 *
	 * @param filename                 - name of file (eg. document.pdf)
	 * @param content_type             - contenttype of file
	 * @param content_type_name        - an OpString where the textual description of mimetype of file should be placed (eg. "PDF document")
	 * @param content_type_bitmap      - reference to an OpBitmap pointer where the icon should be placed
	 * @param content_type_bitmap_size - the required size of the icon
	 * @return OpStatus::OK if filetype info was found
	 */
	OP_STATUS GetFileTypeInfo( const OpStringC& filename,
							   const OpStringC& content_type,
							   OpString & content_type_name,
							   OpBitmap *& content_type_bitmap,
							   UINT32 content_type_bitmap_size  = 16);

	/**
	   @param filename         - name of file (eg. document.pdf)
	   @return textual description of handler for the file (eg. "Acrobat Reader")
	*/
	const uni_char * GetFileHandlerName( const OpStringC& handler,
										 const OpStringC& filename,
										 const OpStringC& content_type);


	/**
	   Attempts to aquire the full path of the file handler icon for
	   the file handler corresponding to the desktop file.

	   @param handler          - name of desktop file (eg. acroread.desktop)
	   @param icon_size        - preferred height/width in pixels
	   @return full path to icon if found, else 0
	*/
	const uni_char * GetApplicationIcon( const OpStringC & handler,
										 UINT32 icon_size  = 16);

	/**
	   @param handler          - handler command
	   @param filename         - name of file (eg. document.pdf)
	   @param content_type     - contenttype of file
	   @param icon_size        - preferred height/width in pixels
	   @return pointer to OpBitmap containing the icon if an icon was found, else 0
	*/
	OpBitmap* GetApplicationIcon( const OpStringC& handler,
								  const OpStringC& filename,
								  const OpStringC& content_type,
								  UINT32 icon_size  = 16);

	/**
	   @param filename
	   @param handler
	   @return
	*/
	BOOL ValidateHandler( const OpStringC & filename,
						  const OpStringC & handler );

	/**
	   Open a file browser to the folder that the file is in and select the file, 
	   however if the file_path is to a folder then whether to open the parent folder
	   or the folder itself will depend on treat_folders_as_files. If TRUE this indicates
	   that the parent folder should be opened and the subfolder selected, otherwise the
	   folder itself should be opened and nothing selected.

	   TODO: Selecting the file or folder is not implemeted

	   @param file_path - full path to the file to be opened in a filebrowser
	   @param treat_folders_as_files - if TRUE and file_path is a path to a directory,
	 					the parent directory is opened, with the highlighting the folder.
	 					if FALSE, the path to the directory specified is opened.
	   @return OpStatus::OK if successful
	 */
	OP_STATUS OpenFileFolder(const OpStringC & file_path, BOOL treat_folders_as_files = TRUE);

	/**
	   @param filename
	   @param handler
	   @param validate
	   @return
	*/
	BOOL OpenFileInApplication( const OpStringC & filename,
								const OpStringC & handler,
								BOOL validate,
								BOOL convert_to_locale_encoding = TRUE );

	/**
	   @param filename
	   @param parameters
	   @param handler
	   @param validate
	   @return
	*/
	BOOL OpenFileInApplication( const OpStringC & filename,
								const OpStringC & parameters,
								const OpStringC & handler,
								BOOL validate );

	/**
	   @param protocol
	   @param application
	   @param parameters
	   @param run_in_terminal
	   @return
	*/
	BOOL ExecuteApplication( const OpStringC & protocol,
							 const OpStringC & application,
							 const OpStringC & parameters,
							 BOOL run_in_terminal = FALSE);

	/**
	   @param filename
	   @return
	*/
	BOOL CheckSecurityProblem( const OpStringC & filename );

	/**
	   @param file_handler
	   @param directory_handler
	   @param list
	*/
	void WriteHandlersL( HandlerElement& file_handler,
						 HandlerElement& directory_handler,
						 OpVector<HandlerElement>& list);

//--------------------------------------------------------------
// Inline public methods:
//--------------------------------------------------------------

	/**
	   Sets m_validation_enabled to the value of the parameter
	   @param enable
	*/
	void EnableValidation( BOOL enable ) { m_validation_enabled = enable; }

	/**
	   Get the default file handler for all files
	   @return  reference to default file handler element
	*/
	HandlerElement& GetDefaultFileHandler() { return m_default_file_handler; }

	/**
	   Get the default directory handler for all files
	   @return reference to default directory handler element
	*/
	HandlerElement& GetDefaultDirectoryHandler() { return m_default_directory_handler; }

	/**
	   @return
	*/
	const OpVector<HandlerElement>& GetEntries() const { return m_list; }

//--------------------------------------------------------------
// Selftest public methods:
//--------------------------------------------------------------

	int GetNumberOfMimeTypes() { return m_store.GetNumberOfMimeTypes(); }
	void EmptyStore() { m_store.EmptyStore(); }
	static void DeleteManager();

/* _________________________ MIME ___________________________ */
private:

	/**
	   Gets the user specified file handler for the mime-type of the file (if any)

	   @param filename         - name of file (eg. document.pdf)
	   @param content_type     - reference to a string with the content type
	   @param handler          - reference to string where opera default handler (if any) will be placed
	   @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if OOM
	*/
	OP_STATUS GetOperaDefaultFileHandler( const OpString& filename,
										  const OpString &content_type,
										  OpString &handler,
										  OpString &handler_name);

	/**
	   Gets the system specified file handler for the mime-type of the file (if any)

	   @param filename         - name of file (eg. document.pdf)
	   @param content_type     - reference to a string with the content type
	   @param handler          - reference to string where default handler (if any) will be placed
	   @return OpStatus::OK or OpStatus::ERR_NO_MEMORY if OOM
	*/
	OP_STATUS GetDefaultFileHandler( const OpString& filename,
									 const OpString &content_type,
									 OpString &handler,
									 OpString &handler_name,
									 OpBitmap ** handler_bitmap);

	const uni_char * GetMimeType( const OpStringC & filename,
								  const OpStringC & content_type,
								  BOOL strip_path = FALSE);


//BACKEND:
	ApplicationNode * GetDefaultFileHandler( const OpStringC& content_type );

	OP_STATUS GetAllHandlers( const OpStringC & content_type,
							  OpVector<ApplicationNode>& handlers);

	OP_STATUS GetDefaultApplication(const OpStringC& filename,
									OpString& application);

//Interface :

	// InputManager

    void LoadNode( FileHandlerNode * node )
		{ m_input_manager.LoadNode(node); }

	OP_STATUS LoadIcon( FileHandlerNode* node,
					    UINT32 icon_size)
		{ return m_input_manager.LoadIcon(node, icon_size); }

    OP_STATUS MakeIconPath( FileHandlerNode * node,
							OpString & icon_path,
							UINT32 icon_size  = 16)
		{ return m_input_manager.MakeIconPath(node, icon_path, icon_size); }

	OP_STATUS FindDesktopFile( const OpStringC & filename,
							   OpVector<OpString>& files)
		{ return m_input_manager.FindDesktopFile(filename, files); }

	// Store :

	MimeTypeNode* GetMimeTypeNode( const OpStringC & mime_type ) { return m_store.GetMimeTypeNode(mime_type); }

	MimeTypeNode* MakeMimeTypeNode( const OpStringC & mime_type ) { return m_store.MakeMimeTypeNode(mime_type); }

    OP_STATUS LinkMimeTypeNode( const OpStringC & mime_type,
								MimeTypeNode* node,
								BOOL subclass_type = FALSE)
		{ return m_store.LinkMimeTypeNode(mime_type, node, subclass_type); }

	// Will delete node_1 since node_1 will be merged into node_2 (if both are non null and not equal)
	OP_STATUS MergeMimeNodes( MimeTypeNode* node_1,
							  MimeTypeNode* node_2)
		{ return m_store.MergeMimeNodes(node_1, node_2); }

    ApplicationNode * InsertIntoApplicationList( MimeTypeNode* node,
												 const OpStringC & desktop_file,
												 const OpStringC & path,
												 const OpStringC & command,
												 BOOL  default_app = FALSE)
		{ return m_store.InsertIntoApplicationList(node, desktop_file, path, command, default_app); }

	ApplicationNode * InsertIntoApplicationList( MimeTypeNode* node,
												 ApplicationNode * app,
												 BOOL  default_app)
		{ return m_store.InsertIntoApplicationList(node, app, default_app); }

    ApplicationNode * MakeApplicationNode( const OpStringC & desktop_file_name,
										   const OpStringC & path,
										   const OpStringC & command,
										   const OpStringC & icon)
		{ return m_store.MakeApplicationNode(desktop_file_name, path, command, icon); }

    GlobNode * MakeGlobNode( const OpStringC & glob,
							 const OpStringC & mime_type,
							 GlobType type)
		{ return m_store.MakeGlobNode(glob, mime_type, type); }

//MEMBER VARS:

	BOOL m_initialized;

private:
	FileHandlerManager();
	virtual ~FileHandlerManager();
	void LoadL();

private:
	OpAutoVector<HandlerElement> m_list;
	HandlerElement m_default_file_handler;
	HandlerElement m_default_directory_handler;
	BOOL m_validation_enabled;

	friend class OpAutoPtr<FileHandlerManager>;
	static OpAutoPtr<FileHandlerManager> m_manager;

	FileHandlerStore m_store;
	InputFileManager m_input_manager;
};

#endif
