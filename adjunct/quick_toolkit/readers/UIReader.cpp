/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/readers/UIReader.h"
#include "adjunct/ui_parser/ParserIterators.h"
#include "adjunct/quick_toolkit/creators/QuickDialogCreator.h"
#include "adjunct/quick_toolkit/creators/QuickWidgetCreator.h"
#include "adjunct/desktop_util/adt/typedobjectcollection.h"

////////// ~UIReader
UIReader::~UIReader()
{
	m_logger.EndLogging();
}

////////// Init
OP_STATUS
UIReader::Init(const OpStringC8& node_name, const OpStringC& file_path, OpFileFolder folder, const OpStringC& logfile)
{
	if (logfile.HasContent())
	{
		RETURN_IF_ERROR(m_logger.Init(logfile));
		m_logger.StartLogging();
	}

	m_ui_document.SetLogger(&m_logger);
	RETURN_IF_ERROR(m_ui_document.Init(file_path, folder));

	ParserLogger::AutoIndenter indenter(m_logger, "Reading %s definitions into map", node_name);

	ParserNodeMapping node;
	if (!m_ui_document.GetRootNode(node))
	{
		m_logger.OutputEntry("ERROR: could not retrieve the document's root node");
		return OpStatus::OK;
	}

	// read first hash level: retrieve the '<node_name>' node
	ParserNodeSequence relevant_node;
	if (!node.GetChildNodeByID(node.FindValueForKey(node_name), relevant_node))
	{
		m_logger.OutputEntry("ERROR: document doesn't contain a '%s' top-level node", node_name);
		return OpStatus::OK;
	}
	// traverse all nodes under 'node_name', get their name
	// store name and YAML node ID in a hash
	for (ParserSequenceIterator it(relevant_node); it; ++it)
	{
		ParserNodeMapping ui_element;
		if (!m_ui_document.GetNodeByID(it.Get(), ui_element))
		{
			m_logger.OutputEntry("WARNING: Couldn't retrieve node in '%s' sequence. It is probably not of type 'MAPPING'. Ignoring entry.", node_name);
			continue;
		}
		ParserNodeScalar name_node;
		if (!ui_element.GetChildNodeByID(ui_element.FindValueForKey("name"), name_node))
		{
			m_logger.OutputEntry("ERROR: %s node doesn't have a child node called 'name'. Ignoring...", node_name);
			continue;
		}
		OpAutoPtr<ParserNodeIDTableData> data (OP_NEW(ParserNodeIDTableData, (it.Get())));
		RETURN_OOM_IF_NULL(data.get());

		RETURN_IF_ERROR(m_logger.Evaluate(name_node.GetString(data->key), "ERROR retrieving string from name node"));
		RETURN_IF_ERROR(m_ui_element_hash.Add(data->key.CStr(), data.get()));
		m_logger.OutputEntry("Read '%s'", data->key);
		data.release();
	}
	
	indenter.Done();

	return OpStatus::OK;
}



OP_STATUS UIReader::GetNodeFromMap(const OpStringC8 &name, ParserNodeMapping* node)
{
	ParserNodeIDTableData * data;
	RETURN_IF_ERROR(m_logger.Evaluate(m_ui_element_hash.GetData(name.CStr(), &data), "ERROR: could not find widget with name '%s'", name));

	if (!m_ui_document.GetNodeByID(data->data_id, *node))
	{
		m_logger.OutputEntry("ERROR: could not retrieve widget node");
		return OpStatus::ERR;
	}
	return OpStatus::OK;;
}

////////// DialogReader
BOOL
DialogReader::HasQuickDialog(const OpStringC8 & dialog_name)
{
	ParserNodeMapping node;
	if (OpStatus::IsError(GetNodeFromMap(dialog_name, &node)))
		return FALSE;

	return TRUE;
}

OP_STATUS
DialogReader::CreateQuickDialog(const OpStringC8 & dialog_name, QuickDialog & dialog)
{
	QuickDialogCreator creator(m_logger);
	ParserNodeMapping node;
	RETURN_IF_ERROR(GetNodeFromMap(dialog_name, &node));
	RETURN_IF_ERROR(creator.Init(&node));
	return creator.CreateQuickDialog(dialog);
}

////////// WidgetReader
OP_STATUS
WidgetReader::CreateQuickWidget(const OpStringC8 & widget_name, OpAutoPtr<QuickWidget>& widget, TypedObjectCollection &collection)
{
	QuickWidgetCreator creator(collection, m_logger, false);
	ParserNodeMapping node;
	RETURN_IF_ERROR(GetNodeFromMap(widget_name, &node));
	RETURN_IF_ERROR(creator.Init(&node));
	return creator.CreateWidget(widget);
}

