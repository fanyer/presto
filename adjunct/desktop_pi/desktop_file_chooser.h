/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DESKTOP_FILECHOOSER_H
# define DESKTOP_FILECHOOSER_H

# include "modules/util/adt/opvector.h"
# include "modules/windowcommander/OpWindowCommander.h"

class OpWindow;
class DesktopFileChooser;

class DesktopFileChooserListener;
class DesktopFileChooserRequest;
class DesktopFileChooserResult;

/** @short Mechanism for choosing one or several files.
 *
 * The files chosen will be used in some kind of operation. Which files to use
 * are signalled back to core via DesktopFileChooserListener.
 *
 * An implementation of this class provides some sort of mechanism for
 * selecting a file or several files for reading or writing. Directories may
 * also be selected.
 *
 * A file choosing session is started by calling Execute(). This method takes
 * an DesktopFileChooserRequest, where core informs the platform implementation
 * about what type of files to select (directories or not, which extensions),
 * and what will happen with the selected files (write or read), if multiple
 * files are allowed, etc.
 *
 * A typical implementation is to display a file dialog, to let the user
 * navigate through the file system and select a file or several files. At some
 * point the user clicks "OK" or "Cancel", and core is notified about what
 * happened (dialog cancelled, or which files were selected) during the file
 * choosing session via a DesktopFileChooserListener.
 */
class DesktopFileChooser
{
public:
	/** Create an DesktopFileChooser object. Creator becomes owner and is responsible for the descruction of new_object. */
	static OP_STATUS Create(DesktopFileChooser** new_object);

	/** Destructor.
	 *
	 * This will cancel the file choosing session, if active. Any UI mechanism
	 * used to enable the user to select files must be removed (i.e. the file
	 * dialog will be closed now). The listener is not to be accessed in this
	 * call.
	 */
	virtual ~DesktopFileChooser() { }

	/** Execute the file chooser - start a file choosing session.
	 *
	 * A typical implementation displays a file dialog and lets the user select
	 * files or directories.
	 *
	 * Synchronous operation by starting a nested event loop is not allowed;
	 * the method must return immediately after having started the file
	 * choosing session (e.g. shown a file dialog). The platform side may still
	 * provide file dialog modality internally, but it is not required. This
	 * means that, although most platform implementations probably will provide
	 * modal dialogs here, the caller must not assume that; input (mouse /
	 * keyboard) events may be delivered to other windows while a file dialog
	 * is shown.
	 *
	 * A file choosing session is concluded by a call to
	 * DesktopFileChooserListener::OnFileChoosingDone(). When finished, any UI
	 * mechanism used to enable user to select files must be hidden/removed.
	 *
	 * This method may be called several times per object, but a subsequent
	 * call must not be made while a session is already in progress.
	 *
	 * @param parentwin The owner window of this file chooser. May NOT be NULL.
	 * You take the risk of running into unexpected issues on other platforms by
	 * setting this to NULL!
	 * @param listener Listener on which to call OnFileChoosingDone() when file
	 * choosing is done. If Cancel() is called before choosing is done,
	 * OnFileChoosingDone() is not to be called.
	 * @param request A set of input parameters to the file choosing
	 * session. An implementation needs to check the various members, to
	 * provide a dialog window title, to know what kind of files or directories
	 * to allow the user to select, and the intended operation that will be
	 * performed on the selected files (read / write).
	 * The file chooser will assume ownership of this parameter.
	 * @return OK, ERR or ERR_NO_MEMORY
	 */
	virtual OP_STATUS Execute(OpWindow* parent, DesktopFileChooserListener* listener, const DesktopFileChooserRequest& request) = 0;

	/** Cancel the file chooser.
	 *
	 * Any UI mechanism used to enable the user to select files must be removed
	 * (i.e. the file dialog will be closed now). The listener is not to be
	 * accessed in this call.
	 *
	 * This call has no effect if file choosing is not in progress.
	 */
	virtual void Cancel() = 0;
};

/** @short Listener for DesktopFileChooser.
 *
 * Used to report the result of a file choosing session back to the owner of
 * the DesktopFileChooser object (typically core/adjunct), so that the owner may
 * perform whatever operation intended on the the files/directories chosen.
 */
class DesktopFileChooserListener
{
public:
	virtual ~DesktopFileChooserListener() {};

	/** File choosing is completed.
	 *
	 * The platform implementation must have hidden/removed any UI mechanism
	 * that was provided to enable the user to select files at this point. A
	 * call to DesktopFileChooser::Cancel() has no effect at this point, since
	 * choosing is defined as completed.
	 *
	 * @param chooser The DesktopFileChooser for which file choosing is
	 * completed.
	 * @param result Result of the file choosing session.
	 */
	virtual void OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result) = 0;
};

/** Input parameters to a file choosing session.
 *
 * Passed to DesktopFileChooser::Execute(). Members are initially FALSE/empty/NULL
 * unless explicitly stated.
 */
class DesktopFileChooserRequest
{
public:
	DesktopFileChooserRequest()
		:
		action(ACTION_FILE_OPEN),
		initial_filter(0),
		get_selected_filter(FALSE),
		fixed_filter(FALSE)
		{ }

	/** Possible actions for a file choosing session. */
	enum Action
	{
		/** Select one file for reading. The selected file must exist. */
		ACTION_FILE_OPEN,

		/** Select one or multiple files for reading. The selected files must exist. */
		ACTION_FILE_OPEN_MULTI,

#ifdef DESKTOP_FILECHOOSER_DIRFILE
		/** Select one or multiple files or directories for reading. The selected files must exist.
		 *  One does not have to support the total mix in one selection session, the user may have to repeat
		 *  the underlaying action but the caller shall handle the result */
		ACTION_DIR_FILE_OPEN_MULTI,
#endif // DESKTOP_FILECHOOSER_DIRFILE
		
		/** Select one file for writing. */
		ACTION_FILE_SAVE,

		/** Select one file for writing, but warn the user if the selected file already exists. */
		ACTION_FILE_SAVE_PROMPT_OVERWRITE,

		/** Select one directory for read/write. */
		ACTION_DIRECTORY

#ifdef _MACINTOSH_
		/** Special Application treatment for Mac */
		,ACTION_CHOOSE_APPLICATION
#endif
	};

	/** What to do in the file choosing session. Initial value is ACTION_OPEN. */
	Action action;

	/** Short and descriptive text that explains the purpose of the file
	 * chooser (i.e. what are we going to do with the files that the user
	 * selects). Typically used as the window/dialog title created for the file
	 * choosing session. Should not be empty. */
	OpString caption;

	/** Initial path. May be either a directory or a file. The initial
	 * directory presented to the user should be this path. If it is a file,
	 * this will be the initially selected file. 
	 * The path should not be quoted, even if it contains spaces.
	 */
	OpString initial_path;

	/** List of extension filters.
	 *
	 * May be left empty if there are no extension filters to suggest for the
	 * file choosing session.
	 *
	 * Note that when adding an DesktopFileChooserExtensionFilter to this vector,
	 * the vector will take ownership, and delete it when it goes out of scope.
	 */
	OpAutoVector<OpFileSelectionListener::MediaType> extension_filters;
	/** Index of the extension filter initially selected, where 0 is the first
	 * entry in extension_filters, 1 is the second one, and so on.
	 * If -1 is provided, the file chooser is free to select the
	 * initial filter it sees fit.
	 */
	int initial_filter;

	/** If TRUE, DesktopFileChooserListener::OnFileChoosingDone() must provide the
	 * index of the file extension filter selected during the file choosing
	 * session. Also, extension_filters may not be empty then, or behavior is
	 * undefined.
	 */
	BOOL get_selected_filter;

	/** If TRUE, the file extension filter(s) shall not be editable, nor
	 * extendable. The extension_filters shall not be empty.
	 * If FALSE and action is a save action, the file chooser should make sure
	 * that the extension of the file provided as inital path (if provided) is
	 * present in the filter.
	 *
	 * Note: If TRUE, the desktop file chooser may use the index in the
	 * extension_filters to decide which action to choose (archive in case of 
	 * html save) media type to save file as.
	 */
	BOOL fixed_filter;
};

/** Output parameters from a file choosing session.
 *
 * This is the result data of a file choosing session, and an object of this
 * type is passed to DesktopFileChooserListener::OnFileChoosingDone().
 */
class DesktopFileChooserResult
{
public:
	DesktopFileChooserResult() : selected_filter(-1) { }

	/** Vector of selected files.
	 *
	 * An empty vector typically means that file choosing was cancelled.
	 */
	OpAutoVector<OpString> files;

	/** Active directory in the file choosing session when it ended.
	 *
	 * May be empty if this information is unavailable.
	 */
	OpString active_directory;

	/** Index of the file extension filter. Initially set to -1.
	 *
	 * As specified in extension_filters in DesktopFileChooserRequest chosen during
	 * the file choosing session. This value must be ignored if
	 * get_selected_filter in DesktopFileChooserRequest was FALSE; otherwise it must
	 * be set to the correct index. A value of -1 indicates that no filter was
	 * chosen, or that a filter not supplied at the start of the session was
	 * chosen (for example, some implementations may let the user add filters
	 * while in a file dialog).
	 */
	int selected_filter;
};

#endif // DESKTOP_FILECHOOSER_H
