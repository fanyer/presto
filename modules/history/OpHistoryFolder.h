/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
** psmaas - Patricia Aas
*/

#ifndef CORE_HISTORY_MODEL_FOLDER_H
#define CORE_HISTORY_MODEL_FOLDER_H

#ifdef HISTORY_SUPPORT

#include "modules/history/OpHistoryItem.h"
#include "modules/history/history_enums.h"
#include "modules/history/src/HistoryKey.h"

//___________________________________________________________________________
//                         CORE HISTORY MODEL FOLDER
//___________________________________________________________________________

class HistoryFolder
	: public HistoryItem
{
   friend class HistoryModel;

public:

	/*
	 * Used to distiguish between different types of folders
	 * Has to be implemented by subclasses
	 *
	 * @return the type of folder this is
	 **/
    virtual HistoryFolderType GetFolderType() const = 0;

	/*
	 * @return true if the folder is empty and therefore can be deleted.
	 **/
    virtual BOOL IsEmpty() const { return FALSE; }

	/*
	 * @return the type of image that should be used for this item
	 **/
	virtual const char* GetImage() const { return "Folder"; }

	/*
	 * @return the key for this folder
	 **/
	virtual const LexSplayKey * GetKey() const { return m_key; }

	/*
	 * @param address
	 *
	 * @return the address of this item
	 **/
	virtual OP_STATUS GetAddress(OpString & address) const;

	// ------------------ Implementing OpTypedObject interface ----------------------

    virtual Type GetType() { return FOLDER_TYPE; }

#ifdef HISTORY_DEBUG
	static INT m_number_of_folders;
	virtual int GetSize() { return sizeof(HistoryFolder) + GetTitleLength();}
#endif //HISTORY_DEBUG

protected:

	/*
	 * @param key that will be used if/when this item is inserted into a lex splay tree tree
	 *
	 * Protected constructor - this is an abstract class
	 **/
    HistoryFolder(HistoryKey * key);
	HistoryFolder();

	/*
	 * Protected destructor - this is an abstract class
	 **/
    virtual ~HistoryFolder();

	/*
	 * @param key
	 **/
	void SetKey(HistoryKey * key);

	HistoryKey * m_key;
};

typedef HistoryFolder CoreHistoryModelFolder;  // Temorary typedef to old class name, so old code continues to compile

#endif // HISTORY_SUPPORT
#endif // CORE_HISTORY_MODEL_FOLDER_H
