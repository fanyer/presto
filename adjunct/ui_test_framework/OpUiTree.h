/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef UI_TREE_H
#define UI_TREE_H

#ifdef FEATURE_UI_TEST

class OpAccessibilityExtension;
class OpUiNode;
class XMLFragment;

/**
 * @brief A tree representing the UI during testing
 * @author Patricia Aas
 */

class OpUiTree
{
public:
    OpUiTree();
    ~OpUiTree();

    OP_STATUS BuildTree(OpAccessibilityExtension* root);
    OP_STATUS Export(XMLFragment& fragment);
    OP_STATUS ClickAll();

private:
    OP_STATUS CreateNode(OpAccessibilityExtension* node, OpUiNode*& ui_node);

    OpUiNode* m_root;
};

#endif // FEATURE_UI_TEST

#endif // UI_TREE_H
