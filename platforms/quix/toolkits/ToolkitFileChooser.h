/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef TOOLKIT_FILE_CHOOSER_H
#define TOOLKIT_FILE_CHOOSER_H

#include "platforms/utilix/x11types.h"

class ToolkitFileChooserListener;

class ToolkitFileChooser
{
public:
	enum DialogType
	{
		FILE_OPEN,					///< Open a single file
		FILE_OPEN_MULTI,			///< Open one or more files
		FILE_SAVE,					///< Save a file
		FILE_SAVE_PROMPT_OVERWRITE,	///< Save a file, but prompt if this will overwrite
		DIRECTORY					///< Select a directory for reading/writing
	};

	virtual ~ToolkitFileChooser() {}

	// Setup

	/** Initialization of dialog if necessary
	  * @param parent Parent window to use for dialog
	  */
	virtual void InitDialog() = 0;

	/** Set the type of dialog
	  */
	virtual void SetDialogType(DialogType type) = 0;

	/** Set the caption of the file chooser
	  * @param caption Caption in UTF-8 format
	  */
	virtual void SetCaption(const char* caption) = 0;

	/** Set the initial path the file chooser should use
	  * @param path Path in UTF-8 format
	  */
	virtual void SetInitialPath(const char* path) = 0;

	/** Add a filter to display in the file chooser
	  * @param id ID with which this filter can be identified later. The first filter added
	  *        will get ID 0, then 1, 2, ... etc
	  * @param media_type String that describes the filter in UTF-8 format
	  */
	virtual void AddFilter(int id, const char* media_type) = 0;

	/** Add an extension to a filter
	  * @param id ID of filter to add extension to
	  * @param extension Extension to add (eg. "*.htm")
	  */
	virtual void AddExtension(int id, const char* extension) = 0;

	/** Set the default filter to select in the chooser
	  * @param id ID of filter to set as default
	  */
	virtual void SetDefaultFilter(int id) = 0;

	/** Set whether the user can add extensions/filters to the list
	  * @param fixed If true, user cannot add extensions to list, list is fixed
	  */
	virtual void SetFixedExtensions(bool fixed) = 0;

	/** Set whether to display hidden files
	  * @param show_hidden Whether to show hidden files
	  */
	virtual void ShowHiddenFiles(bool show_hidden) = 0;

	// Act

	/** Open the dialog
	  * @param parent Parent window for the dialog
	  * @param result_listener Listener to call when dialog is finished
	  */
	virtual void OpenDialog(X11Types::Window parent, ToolkitFileChooserListener* result_listener) = 0;

	/** Cancel an open dialog
	 * NB: this function can be called while still in an OpenDialog() call!
	  */
	virtual void Cancel() = 0;

	/** Destroy this ToolkitFileChooser object and deallocate it
	  * NB: this function can be called while still in an OpenDialog() call!
	  * Destruction is allowed to be asynchronous if necessary
	  */
	virtual void Destroy() = 0;

	// Result getters

	/** @return The number of files selected by the user
	  */
	virtual int GetFileCount() = 0;

	/** Get files selected by the user
	  * @param index A number in the range [ 0 .. GetFileCount() )
	  * @return Path of a file selected by the user in UTF-8
	  */
	virtual const char* GetFileName(int index) = 0;

	/** Get the directory that was active when the user closed the dialog
	  * @return Directory path in UTF-8
	  */
	virtual const char* GetActiveDirectory() = 0;

	/** Get the filter selected by the user
	  * @return ID of filter selected by the user
	  */
	virtual int GetSelectedFilter() = 0;
};

class ToolkitFileChooserListener
{
public:
	virtual ~ToolkitFileChooserListener() {}

	/** To be called by toolkit when a file is about to be saved and
	  * the toolkit itself does not provide the functionality to
	  * prompt and warn the user if the file already exists.
	  *
	  * @return true if the file can be written otherwise false. false
	  * will be returned on error or if file count is not 1
	  */
	virtual bool OnSaveAsConfirm(ToolkitFileChooser* chooser) = 0;

	/** This callback will be called when the file dialog is closed by
	 * the user, regardless of how it happens (Ok/Open/Save, Cancel,
	 * closing the window, etc.)
	 *
	 * It will NOT be called if the file dialog is closed
	 * programmatically by opera (e.g. ToolkitFileChooser::Cancel()).
	 *
	 * (Implementations of ToolkitFileChooser may need to handle this
	 * case carefully, in case the user closes the dialog before
	 * Cancel() (or similar methods) takes effect.)
	 */
	virtual void OnChoosingDone(ToolkitFileChooser* chooser) = 0;
};

#endif // TOOLKIT_FILE_CHOOSER_H
