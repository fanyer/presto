// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// $Id$
//
// Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef TREE_H
#define TREE_H

// ----------------------------------------------------

/**
 * hiearchy
 *
 *   MESSENGER
 *     SESSION
 *       IDENTITY
 *       ACCOUNT
 *         TRANSPORTER
 *         PROVIDER
 *           FOLDER
 *             FOLDER
 *             MESSAGE
 *               MESSAGEPART
 *                 MESSAGEPART
 *     
 */

// ----------------------------------------------------

class ITreeNodeObserver 
{
public:
	
	virtual OP_STATUS NodeOpened() = 0;
	
	virtual OP_STATUS NodeClosed() = 0;
	
	// make this into ChildAdded, ChildRemoved etc later
	
	virtual OP_STATUS Changed() = 0;
	
};

// ----------------------------------------------------

class ITreeNode 
{
public:
	
    virtual const char* GetName() const = 0;
	
	virtual const ITreeNode* GetChild(UINT32 idx) const = 0;
	
	virtual bool IsLeaf() const = 0;
	
};

// ----------------------------------------------------

#endif // TREE_H
