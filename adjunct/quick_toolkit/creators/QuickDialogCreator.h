/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef QUICK_DIALOG_CREATOR_H
#define QUICK_DIALOG_CREATOR_H

#include "adjunct/quick_toolkit/creators/QuickUICreator.h"

class QuickDialog;

/**
 * @brief Creates a QuickDialog object based on a ParserNode definition
 * @see QuickUICreator
 * 
 * To be used like this:
 * 
 * ParserNode node = // get it from somewhere, e.g. DialogReader
 * QuickDialogCreator creator;
 * RETURN_IF_ERROR(creator.Init(node));
 * QuickDialog * dialog;
 * RETURN_IF_ERROR(creator.CreateQuickDialog(dialog));
 */
class QuickDialogCreator : public QuickUICreator
{
public:
	QuickDialogCreator(ParserLogger& log) : QuickUICreator(log) {}

	/** @see QuickUICreator::Init */
	virtual OP_STATUS	Init(ParserNodeMapping * ui_node);

	/**
	 * Traverses the attributes of the ParserNode and builds a dialog by
	 * creating its content widgets/layouts.
	 *
	 * @param dialog the QuickDialog to be built
	 * @returns Error if OOM or ill-defined parser nodes.
	 */
	OP_STATUS	CreateQuickDialog(QuickDialog& dialog);

private:
	OpString8	m_dialog_name; //< The name of the dialog, retrieved separatly from hash map for quick access.
};


#endif // QUICK_DIALOG_CREATOR_H
