/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef KDE_OP_FILE_CHOOSER_H
#define KDE_OP_FILE_CHOOSER_H

#include "adjunct/desktop_pi/desktop_file_chooser.h"

class ArgumentCreator;


/** @brief A file chooser for KDE that relies on kdialog
  */
class KDEOpFileChooser
	: public DesktopFileChooser
	, private MessageObject
{
public:
	KDEOpFileChooser();

	virtual ~KDEOpFileChooser();

	// From DesktopFileChooser
	virtual OP_STATUS Execute(OpWindow* parent, DesktopFileChooserListener* listener, const DesktopFileChooserRequest& request);

	virtual void	  Cancel();

private:
	enum Pipe
	{
		PipeRead  = 0,
		PipeWrite = 1
	};

	OP_STATUS		  PrepareArguments(const DesktopFileChooserRequest& request, ArgumentCreator& argument_creator);
	OP_STATUS		  AddExtensionFilter(const DesktopFileChooserRequest& request, ArgumentCreator& argument_creator);
	OP_STATUS		  OpenDialog(const DesktopFileChooserRequest& request);
	void			  ClosePipe(Pipe pipe);
	void			  ReadOutput(DesktopFileChooserResult& result);
	void			  Cleanup();

	// From MessageObject
	virtual void	  HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	pid_t			  m_childpid;
	int				  m_pipe_descriptor[2];
	DesktopFileChooserListener* m_listener;
};

#endif // KDE_OP_FILE_CHOOSER_H
