/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef QUICK_UI_CREATOR_H
#define QUICK_UI_CREATOR_H

#include "adjunct/ui_parser/ParserNode.h"
#include "modules/inputmanager/inputaction.h"

class ParserLogger;
class QuickWidget;
class TypedObjectCollection;

/*
 * @brief A helper class to create UI elements from ParserNodes.
 * Typically used through sub-classes QuickWidgetCreator or QuickDialogCreator, etc.
 */
class QuickUICreator
{
public:
	virtual ~QuickUICreator() {}

	/**
	 * Retrieves all parameters from hashtable node ui_node.
	 * Stores them in a map with key=key and value is value-ParserNodeID
	 */
	virtual OP_STATUS	Init(ParserNodeMapping* ui_node);

protected:
	/** Constructor
	  * @param log Log warnings and errors to this logging object
	  */
	explicit QuickUICreator(ParserLogger& log);

	ParserNode *		GetUINode() const { return m_ui_node; }

	// HELPERS

	/** @return Logger for this creator */
	ParserLogger& GetLog() const { return m_log; }

	/** Gets ParserNodeID from map (by key specified)
	 */
	ParserNodeID GetNodeIDFromMap(const OpStringC8& key);

	/**
	 * Gets ParserNodeID from map (by key specified), retrieves node from ID, and creates input action from node
	 * @param action NULL if no action is specified, a created action otherwise.
	 */
	OP_STATUS	CreateInputActionFromMap(OpAutoPtr<OpInputAction>& action);

	/**
	 * Gets ParserNodeID from map (by key specified), retrieves node from ID, and creates widget from node
	 * @see QuickWidgetCreator
	 */	
	OP_STATUS	CreateWidgetFromMapNode(const OpStringC8 & key,
			OpAutoPtr<QuickWidget>& widget, TypedObjectCollection& widget_collection, bool dialog_reader);

	/** Gets ParserNodeID from map (by key specified), and retrieves node from ID.
	 * If the node can't be found it is left empty. */
	OP_STATUS	GetNodeFromMap(const OpStringC8 & key, ParserNode & node);
	OP_STATUS	GetNodeFromMap(const OpStringC8 & key, ParserNodeScalar & node);
	OP_STATUS	GetNodeFromMap(const OpStringC8 & key, ParserNodeMapping & node);
	OP_STATUS	GetNodeFromMap(const OpStringC8 & key, ParserNodeSequence & node);

	/** Checks if specified key is defined in the map. */
	bool		IsScalarInMap(const OpStringC8& key) const;

	/** Gets ParserNodeID from map (by key specified), retrieves node from ID, and integer value from scalar node.
	 * Doesn't change value of the integer if the node can't be found. */
	OP_STATUS	GetScalarIntFromMap(const OpStringC8 & key, INT32 & integer);

	/** Gets ParserNodeID from map (by key specified), retrieves node from ID, and widget size value from scalar node.
	 * Doesn't change value of the size if the node can't be found. */
	OP_STATUS	GetWidgetSizeFromMap(const OpStringC8 & key, UINT32 & size);

	/** Gets ParserNodeID from map (by key specified), retrieves node from ID, and character size value from scalar node.
	 * Doesn't change value of the size if the node can't be found. */
	OP_STATUS	GetCharacterSizeFromMap(const OpStringC8 & key, UINT32 & size);

	/** Gets ParserNodeID from map (by key specified), retrieves node from ID, and boolean value from scalar node.
	 * Doesn't change value of the boolean if the node can't be found. */
	OP_STATUS	GetScalarBoolFromMap(const OpStringC8 & key, bool & boolean);

	/** Gets ParserNodeID from map (by key specified), retrieves node from ID, and translated string from node.
	 * Doesn't change value of the string if the node can't be found. */
	OP_STATUS	GetTranslatedScalarStringFromMap(const OpStringC8 & key, OpString & translated_string);
	
	/** Gets ParserNodeID from map (by key specified), retrieves node from ID, and string from node.
	 * Doesn't change value of the string if the node can't be found. */
	OP_STATUS	GetScalarStringFromMap(const OpStringC8 & key, OpString8 & string);

	/** Retrieves the string from a scalar node. Returns error if not a scalar. */
	OP_STATUS	GetScalarString(const ParserNodeID & scalar_node_id, OpString8 & string);

	/** Translates the string_id using the language manager, or sets the string_id, as translated string, if no translation found. 
	 * @param string_id The string id.
	 * @param translated_string		The output string. Either whatever retrieved from language manager, or string_id.
	 */
	OP_STATUS TranslateString(const OpStringC8 & string_id, OpString & translated_string);

private:
	template<class T> OP_STATUS GetNodeFromMapInternal(const OpStringC8 & key, T& node);

	ParserNodeMapping*		m_ui_node;
	ParserNodeIDTable		m_ui_parameters;
	ParserLogger&			m_log;
};

#endif // QUICK_UI_CREATOR_H
