/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */

#ifndef __FILE_HANDLER_DIALOG_H__
#define __FILE_HANDLER_DIALOG_H__

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"


/**
 * FileHandlerEditListener
 *
 *
 */

class FileHandlerEditListener
{
public:
	virtual ~FileHandlerEditListener() {} // gcc warn: virtual methods => virtual destructor.
	virtual BOOL OnItemChanged( INT32 index, const OpString& extension, const OpString& handler, BOOL ask ) = 0;
};

/**
 * FileHandlerDialog
 *
 *
 */

class FileHandlerDialog : public Dialog, public FileHandlerEditListener
{
public:
	FileHandlerDialog() : m_handler_model(3) {}
	~FileHandlerDialog();

	static void Create(DesktopWindow* parent_window);

	virtual void				OnInit();
	void 						OnSetFocus();

	BOOL						OnInputAction(OpInputAction* action);
	BOOL                        OnItemChanged( INT32 index, const OpString& extension, const OpString& handler, BOOL ask );

	virtual BOOL				GetModality()			{return TRUE;}
	virtual BOOL				GetIsBlocking() 		{return TRUE;}
	virtual Type				GetType()				{return DIALOG_TYPE_FILE_HANDLER;}
	virtual const char*			GetWindowName()			{return "File Handler Dialog";}

	INT32						GetButtonCount() { return 2; };

private:
	static FileHandlerDialog* m_dialog;
	SimpleTreeModel m_handler_model;
};


/**
 * FileHandlerEditDialog
 *
 *
 */


class FileHandlerEditDialog : public Dialog
{
public:
	FileHandlerEditDialog( FileHandlerEditListener* listener, INT32 index, BOOL edit_filetype,
						   const OpString& extension, const OpString& handler, BOOL ask )
		: m_listener(listener), m_index(index), m_edit_filetype(edit_filetype), m_ask(ask)
	{
		m_extension.Set(extension);
		m_handler.Set(handler);
	}

	virtual void				OnInit();

	virtual BOOL				GetModality()			{return TRUE;}
	virtual BOOL				GetIsBlocking() 		{return TRUE;}
	virtual Type				GetType()				{return DIALOG_TYPE_EDIT_FILE_HANDLER;}
	virtual const char*			GetWindowName()			{return "File Handler Edit Dialog";}
	INT32						GetButtonCount()		{ return 2; }

	BOOL						OnInputAction(OpInputAction* action);

	BOOL      GetAsk() const { return m_ask; }
	const OpString& GetExtension() const { return m_extension; }
	const OpString& GetHandler() const { return m_handler; }

private:
	FileHandlerEditListener* m_listener;
	INT32 m_index;
	BOOL m_edit_filetype;
	BOOL m_ask;
	OpString m_extension;
	OpString m_handler;
};

#endif
