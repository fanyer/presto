/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef LABEL_PROPERTIES_CONTROLLER_H
#define LABEL_PROPERTIES_CONTROLLER_H

#include "adjunct/desktop_util/adt/opproperty.h"
#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/m2/src/include/defs.h"
#include "adjunct/quick_toolkit/contexts/CloseDialogContext.h"
#include "adjunct/quick_toolkit/bindings/DefaultQuickBinder.h"
#include "adjunct/desktop_pi/desktop_file_chooser.h"

class AccessModel;
class AccessTreeViewListener;
class Index;
class IndexSearch;
class QuickDynamicGrid;

class LabelPropertiesController : public CloseDialogContext, public DesktopFileChooserListener
{
public:
	LabelPropertiesController(index_gid_t id) : m_current_id(id), m_widgets(0), m_label_model(0), m_treeview_listener(0), m_file_chooser(0) {}
	~LabelPropertiesController();
	void InitL();

	// From DialogContext
	virtual BOOL CanHandleAction(OpInputAction* action);
	virtual BOOL DisablesAction(OpInputAction* action);
	virtual OP_STATUS HandleAction(OpInputAction* action);

	// From DesktopFileChooserListener
	virtual void OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result);

private:
	struct RuleUi
	{
		DefaultQuickBinder m_binder;
		TypedObjectCollection m_widgets;
	};

	OP_STATUS InitOptions();
	OP_STATUS InitTreeView();
	void OnChangeLabel(INT32 new_label);
	OP_STATUS RefreshLabelProperties();
	void DisconnectIndex(Index* index);
	Index* GetSearchIndex(Index* index);
	void SaveCurrentRules();
	OP_STATUS ConnectRule(QuickDynamicGrid* grid, IndexSearch* ruledata);
	OP_STATUS UpdateIcon(Index* index);
	OP_STATUS SelectSearchInIndex(index_gid_t index_id);
	OP_STATUS SetSeachInIndexTitle(Index* index);
	OP_STATUS AddNewLabel();
	OP_STATUS RemoveLabel();
	OP_STATUS AddNewRule();
	OP_STATUS RemoveRule(RuleUi* rule_ui);
	OP_STATUS SetIcon(const char* icon);
	OP_STATUS SelectCustomIcon();

	index_gid_t m_current_id;
	const TypedObjectCollection* m_widgets;
	AccessModel* m_label_model;
	AccessTreeViewListener* m_treeview_listener;
	OpVector<RuleUi> m_rules;

	OpProperty<INT32> m_selected_label;
	OpProperty<INT32> m_spam_level;

	OpProperty<bool> m_hide_from_other;
	OpProperty<bool> m_mark_as_read;
	OpProperty<bool> m_learn_from_messages;
	OpProperty<bool> m_new_messages_only;
	OpProperty<OpString> m_imap_keyword;
	DesktopFileChooser* m_file_chooser;
	DesktopFileChooserRequest m_request;
};

#endif // LABEL_PROPERTIES_CONTROLLER_H
