/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#include "core/pch.h"

#ifdef FEATURE_UI_TEST

#include "adjunct/ui_test_framework/OpUiTree.h"
#include "adjunct/ui_test_framework/OpUiNode.h"
#include "modules/pi/OpAccessibilityExtension.h"
#include "modules/accessibility/OpAccessibilityExtensionListener.h"

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpUiTree::OpUiTree() :
	m_root(NULL)
{

}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OpUiTree::~OpUiTree()
{
	delete m_root;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpUiTree::BuildTree(OpAccessibilityExtension* root)
{
	if(m_root)
	{
		delete m_root;
		m_root = 0;
	}

	OpUiNode* ui_root = 0;

	OP_STATUS status = CreateNode(root, ui_root);

    if(OpStatus::IsError(status))
		delete ui_root;

	m_root = ui_root;

	return status;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpUiTree::ClickAll()
{
	if(!m_root)
		return OpStatus::ERR;

	return m_root->ClickAll();
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpUiTree::CreateNode(OpAccessibilityExtension* node, OpUiNode*& ui_node)
{
	if(!node)
		return OpStatus::OK;

	OpAccessibilityExtensionListener* element = node->GetListener();

	if(!element)
		return OpStatus::OK;

	// Create the node
	RETURN_IF_ERROR(OpUiNode::Create(static_cast<OpExtensionContainer*>(node), ui_node));

	// Process children
	for (int i = 0; i < element->GetAccessibleChildrenCount(); i++)
	{
		OpUiNode* child = 0;
		RETURN_IF_ERROR(CreateNode(element->GetAccessibleChild(i), child));
		ui_node->AddChild(child);
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 **
 **
 **
 ***********************************************************************************/
OP_STATUS OpUiTree::Export(XMLFragment& fragment)
{
	return m_root ? m_root->Export(fragment) : OpStatus::ERR;
}

#endif // FEATURE_UI_TEST
