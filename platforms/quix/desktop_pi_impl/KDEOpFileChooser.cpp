/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "platforms/quix/desktop_pi_impl/KDEOpFileChooser.h"
#include "platforms/posix/posix_native_util.h"
#include "platforms/unix/base/x11/x11utils.h"
#include "platforms/quix/desktop_pi_impl/ArgumentCreator.h"

#include "adjunct/quick/Application.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/OpLocale.h"

#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>


/***********************************************************************************
 ** Constructor
 **
 ** KDEOpFileChooser::KDEOpFileChooser
 ***********************************************************************************/
KDEOpFileChooser::KDEOpFileChooser()
	: m_childpid(0)
	, m_listener(0)
{
	memset(m_pipe_descriptor, 0, sizeof(m_pipe_descriptor));
}


/***********************************************************************************
 ** Destructor
 **
 ** KDEOpFileChooser::~KDEOpFileChooser
 ***********************************************************************************/
KDEOpFileChooser::~KDEOpFileChooser()
{
	Cancel();
}


/***********************************************************************************
 ** Execute a file chooser dialog
 ** NB: this will fork
 **
 ** KDEOpFileChooser::Execute
 ***********************************************************************************/
OP_STATUS KDEOpFileChooser::Execute(OpWindow* parent,
									DesktopFileChooserListener* listener,
									const DesktopFileChooserRequest& request)
{
	// Don't call twice
	if (m_childpid)
		return OpStatus::ERR;

	m_listener = listener;

	// Prepare communication pipe
	if (pipe(m_pipe_descriptor) != 0)
		return OpStatus::ERR;

	// fork() and check for error
	m_childpid = fork();

	if (m_childpid == 0)
	{
		// Child
		ClosePipe(PipeRead);
		dup2(m_pipe_descriptor[PipeWrite], fileno(stdout));

		if (OpStatus::IsError(OpenDialog(request)))
			exit(1);
	}
	else if (m_childpid > 0)
	{
		// Parent
		ClosePipe(PipeWrite);

		g_main_message_handler->SetCallBack(this, MSG_FILE_CHOOSER_SHOW, reinterpret_cast<MH_PARAM_1>(this));
		g_main_message_handler->PostMessage(MSG_FILE_CHOOSER_SHOW, reinterpret_cast<MH_PARAM_1>(this), 0, 100);
	}
	else
	{
		// Error occured
		m_childpid = 0;
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Use canceled
 **
 ** KDEOpFileChooser::Cancel
 ***********************************************************************************/
void KDEOpFileChooser::Cancel()
{
	m_listener = 0;

	if (!m_childpid)
		return;

	// Kill child process
	if (kill(m_childpid, SIGTERM) != 0)
		return;

	// Wait until child process dies
	int status;
	waitpid(m_childpid, &status, 0);
	Cleanup();
}


/***********************************************************************************
 ** Setup arguments for use in command line
 **
 ** KDEOpFileChooser::PrepareCommonArguments
 ***********************************************************************************/
OP_STATUS KDEOpFileChooser::PrepareArguments(const DesktopFileChooserRequest& request, ArgumentCreator& argument_creator)
{
	// Initialize arguments
	RETURN_IF_ERROR(argument_creator.Init("kdialog"));

	// Create caption
	RETURN_IF_ERROR(argument_creator.AddArgument("--title"));
	if (request.caption.HasContent())
	{
		RETURN_IF_ERROR(argument_creator.AddArgument(request.caption));
	}
	else
	{
		OpString caption_string;
	
		if (request.action == DesktopFileChooserRequest::ACTION_FILE_SAVE ||
			request.action == DesktopFileChooserRequest::ACTION_FILE_SAVE_PROMPT_OVERWRITE)
		{
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_SAVE_AS_CAPTION, caption_string));
		}
		else
		{
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_OPEN_FILE, caption_string));
		}

		RETURN_IF_ERROR(argument_creator.AddArgument(caption_string));
	}

	// Create window caption
	RETURN_IF_ERROR(argument_creator.AddArgument("--caption"));
	RETURN_IF_ERROR(argument_creator.AddArgument("Opera"));

	// Create embed parameter
	X11Types::Window window = X11Utils::GetX11WindowFromDesktopWindow(g_application->GetActiveDesktopWindow(TRUE));
	if (window)
	{
		OpString8 win_id;
		RETURN_IF_ERROR(win_id.AppendFormat("%d", window));
		RETURN_IF_ERROR(argument_creator.AddArgument("--embed"));
		RETURN_IF_ERROR(argument_creator.AddArgument(win_id));
	}

	// We need an initial path for all commands
	const uni_char* initial_path = UNI_L(".");
	if (request.initial_path.HasContent())
		initial_path = request.initial_path.CStr();

	// Create command-specific arguments
	switch (request.action)
	{
#ifdef DESKTOP_FILECHOOSER_DIRFILE
		case DesktopFileChooserRequest::ACTION_DIR_FILE_OPEN_MULTI: /* Fall through */
#endif // DESKTOP_FILECHOOSER_DIRFILE
		case DesktopFileChooserRequest::ACTION_FILE_OPEN: /* Fall through */
		case DesktopFileChooserRequest::ACTION_FILE_OPEN_MULTI:
		{
			RETURN_IF_ERROR(argument_creator.AddArgument("--getopenfilename"));
			RETURN_IF_ERROR(argument_creator.AddArgument(initial_path));
			RETURN_IF_ERROR(AddExtensionFilter(request, argument_creator));

			// Multiple files
			if (request.action != DesktopFileChooserRequest::ACTION_FILE_OPEN)
			{
				RETURN_IF_ERROR(argument_creator.AddArgument("--multiple"));
				RETURN_IF_ERROR(argument_creator.AddArgument("--separate-output"));
			}
			break;
		}

		case DesktopFileChooserRequest::ACTION_FILE_SAVE: /* Fall through */
		case DesktopFileChooserRequest::ACTION_FILE_SAVE_PROMPT_OVERWRITE:
		{
			RETURN_IF_ERROR(argument_creator.AddArgument("--getsavefilename"));
			RETURN_IF_ERROR(argument_creator.AddArgument(initial_path));
			RETURN_IF_ERROR(AddExtensionFilter(request, argument_creator));
			break;
		}

		case DesktopFileChooserRequest::ACTION_DIRECTORY:
		{
			RETURN_IF_ERROR(argument_creator.AddArgument("--getexistingdirectory"));
			RETURN_IF_ERROR(argument_creator.AddArgument(initial_path));
			break;
		}

		default:
			OP_ASSERT(!"Unknown file action for this chooser!");
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Add an extension filter argument to the argument creator
 ** Format of KDE extension filters:
 **   "*.x *.y |Description for x, y\n*.p *.q |Description for p, q"
 **
 ** KDEOpFileChooser::AddExtensionFilter
 ***********************************************************************************/
OP_STATUS KDEOpFileChooser::AddExtensionFilter(const DesktopFileChooserRequest& request, ArgumentCreator& argument_creator)
{
	OpString filter;

	for (unsigned i = 0; i < request.extension_filters.GetCount(); i++)
	{
		if (filter.HasContent())
			filter.Append("\n");

		OpFileSelectionListener::MediaType* media_type = request.extension_filters.Get(i);

		// extensions
		for (unsigned j = 0; j < media_type->file_extensions.GetCount(); j++)
			RETURN_IF_ERROR(filter.AppendFormat(UNI_L("%s "), media_type->file_extensions.Get(j)->CStr()));
	
		// description
		RETURN_IF_ERROR(filter.AppendFormat(UNI_L("|%s"), media_type->media_type.CStr()));
	}

	return argument_creator.AddArgument(filter);
}


/***********************************************************************************
 **
 **
 ** KDEOpFileChooser::OpenDialog
 ***********************************************************************************/
OP_STATUS KDEOpFileChooser::OpenDialog(const DesktopFileChooserRequest& request)
{
	// Make argv array
	ArgumentCreator arg_creator;

	RETURN_IF_ERROR(PrepareArguments(request, arg_creator));

	// Execute the application
	if (execvp("kdialog", arg_creator.GetArgumentArray()) != 0)
	{
		// Can't start application
		return OpStatus::ERR;
	}

	// We should never get here
	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** KDEOpFileChooser::ClosePipe
 ***********************************************************************************/
void KDEOpFileChooser::ClosePipe(Pipe pipe)
{
	if (m_pipe_descriptor[pipe])
	{
		close(m_pipe_descriptor[pipe]);
		m_pipe_descriptor[pipe] = 0;
	}
}


/***********************************************************************************
 ** TODO: this should set other result members as well, not just result.files
 **
 ** KDEOpFileChooser::ReadOutput
 ***********************************************************************************/
void KDEOpFileChooser::ReadOutput(DesktopFileChooserResult& result)
{
	// Read data

	const size_t bufsize = 1024;
	OpString8    buffer;
	OpAutoPtr<OpString> current_file(0);

	if (!buffer.Reserve(bufsize))
		return;

	// Start reading
	size_t readsize = read(m_pipe_descriptor[PipeRead], buffer.CStr(), bufsize);

	while (readsize > 0)
	{
		// Terminate result
		buffer.CStr()[readsize] = '\0';

		// Read filenames, separated by '\n'
		char* buf_ptr = buffer.CStr();

		while (buf_ptr && *buf_ptr)
		{
			char* next_buf = op_strchr(buf_ptr, '\n');

			if (next_buf)
				*next_buf = '\0';

			// Convert filename
			OpString partial_filename;
			RETURN_VOID_IF_ERROR(g_oplocale->ConvertFromLocaleEncoding(&partial_filename, buf_ptr));

			if (!current_file.get())
			{
				current_file = OP_NEW(OpString, ());
				if (!current_file.get())
					return;
			}
			RETURN_VOID_IF_ERROR(current_file->Append(partial_filename));

			if (next_buf)
			{
				RETURN_VOID_IF_ERROR(result.files.Add(current_file.get()));
				current_file.release();
			}

			buf_ptr = next_buf ? next_buf + 1 : 0;
		}

		readsize = read(m_pipe_descriptor[PipeRead], buffer.CStr(), bufsize);
	}
}


/***********************************************************************************
 **
 **
 ** KDEOpFileChooser::Cleanup
 ***********************************************************************************/
void KDEOpFileChooser::Cleanup()
{
	m_childpid = 0;
	ClosePipe(PipeRead);
	ClosePipe(PipeWrite);
	g_main_message_handler->UnsetCallBack(this, MSG_FILE_CHOOSER_SHOW, reinterpret_cast<MH_PARAM_1>(this));
}


/***********************************************************************************
 **
 **
 ** KDEOpFileChooser::HandleCallback
 ***********************************************************************************/
void KDEOpFileChooser::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg != MSG_FILE_CHOOSER_SHOW)
		return;

	// Check if the dialog has been closed yet
	int status;
	if (waitpid(m_childpid, &status, WNOHANG))
	{
		// Read output from process
		DesktopFileChooserResult result;
		ReadOutput(result);

		// Process has finished, cleanup
		Cleanup();

		// Warn listener
		if (m_listener)
			m_listener->OnFileChoosingDone(this, result);
	}
	else
	{
		// Process still running, try again
		g_main_message_handler->PostMessage(MSG_FILE_CHOOSER_SHOW, reinterpret_cast<MH_PARAM_1>(this), 0, 100);
	}
}
