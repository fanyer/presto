/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef UI_DEFINITIONS_MANAGER_H
#define UI_DEFINITIONS_MANAGER_H

#include "adjunct/ui_parser/ParserDocument.h"
#include "adjunct/ui_parser/ParserLogger.h"


class QuickDialog;
class QuickWidget;
class TypedObjectCollection;

/**
 * @brief A reader for UI yaml files
 *
 */
class UIReader
{
public:
	virtual ~UIReader();

	/**
	 * Reads UI definitions from a .yml file and stores the definition name and its YAML root node in a hash
	 * maps.
	 *
	 * @param file_path the path to a custom .yml file
	 * @param folder Folder in which the file_path is stored
	 * @param logfile if given, log details to this path
	 */
	OP_STATUS			Init(const OpStringC8& node_name, const OpStringC& file_path, OpFileFolder folder, const OpStringC& logfile = 0);


	/** Reads YAML file and stores dialog and widget nodes in hash maps (key == dialog/widget name) */
	OP_STATUS			ReadUIDefinitions(const OpStringC& file_path, OpFileFolder folder);

protected:

	OP_STATUS			GetNodeFromMap(const OpStringC8 &name, ParserNodeMapping* node);
	
	OP_STATUS			ReadDefinitionsFromKey(const OpStringC& file_path, OpFileFolder folder, const OpStringC8 node_name);

	ParserLogger		m_logger;

private:
	ParserDocument		m_ui_document;
	ParserNodeIDTable	m_ui_element_hash;

};

class DialogReader : public UIReader
{
public:

	OP_STATUS			Init(const OpStringC& file_path, OpFileFolder folder, const OpStringC& logfile = 0) { return UIReader::Init("dialogs", file_path, folder, logfile); }
	/**
	 * Retrieves dialog node from stored map and builds a QuickDialog from that.
	 * @see QuickDialogCreator
	 * @param dialog the dialog to be built
	 * @return	Error if dialog not found, OOM or ill-defined ParserNodes.
	 */
	OP_STATUS			CreateQuickDialog(const OpStringC8 & dialog_name, QuickDialog & dialog);

	/**
	 * Checks if a dialog by this name is known (i.e. was specified in the .yml
	 * @param dialog_name	The name of the dialog.
	 * @return				TRUE if dialog is known.
	 */ 
	BOOL				HasQuickDialog(const OpStringC8 & dialog_name);
};

class WidgetReader : public UIReader
{
public:
	
	OP_STATUS			Init(const OpStringC& file_path, OpFileFolder folder, const OpStringC& logfile = 0) { return UIReader::Init("widgets", file_path, folder, logfile); }

	OP_STATUS			CreateQuickWidget(const OpStringC8 & widget_name, OpAutoPtr<QuickWidget>& widget, TypedObjectCollection &collection);
};

#endif // UI_DEFINITIONS_MANAGER_H
