/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef SUSPENDEDOPERATION_H
#define SUSPENDEDOPERATION_H

#ifdef WAND_SUPPORT

#include "modules/wand/wandsecuritywrapper.h"
#include "modules/wand/wandenums.h"

class WandPage;
class WandLogin;
class WandLoginCallback;
class WandInfo;
class Window;
class FramesDocument;

class SuspendedWandOperation
{
public:
	enum WandOperation
	{
		NO_WAND_OPERATION,

		WAND_OPERATION_USE,
		WAND_OPERATION_REPORT_ACTION,
		WAND_OPERATION_STORE_LOGIN,
		WAND_OPERATION_CHANGE_DB_ENCRYPTION,
		WAND_OPERATION_CHANGE_DB_ENCRYPTION_NO_WINDOW,
		WAND_OPERATION_RETRIEVE_PASSWORD,
		WAND_OPERATION_RETRIEVE_PASSWORD_NO_WINDOW,
		WAND_OPERATION_STORE_PAGE,
		WAND_OPERATION_STORE_LOGIN_NO_WINDOW,
		WAND_OPERATION_FETCH_PAGE,
		WAND_OPERATION_OPEN_DB,
		WAND_OPERATION_SYNC_DATA_AVAILABLE_NO_WINDOW,
		WAND_OPERATION_SYNC_DATA_FLUSH_HTTP_NO_WINDOW,
		WAND_OPERATION_SYNC_DATA_FLUSH_FORM_NO_WINDOW
	};

	SuspendedWandOperation()
		: m_type(NO_WAND_OPERATION),
		  m_window(NULL),
		  m_doc(NULL),
		  m_bool3(NO)
	{
	}

	virtual ~SuspendedWandOperation()
	{
	}

	void ClearReferenceToDoc(FramesDocument* doc)
	{
		if (doc == m_doc)
			{
				m_type = NO_WAND_OPERATION;
				//DeleteAndClear();
			}
	}
	void ClearReferenceToWindow(Window* window);

	WandOperation GetType() { return m_type; }
	FramesDocument* GetDocument() { return m_doc; }
	BOOL3 GetBool3() { return m_bool3; }
	Window* GetWindow() { OP_ASSERT(m_window); return m_window; }

	void Set(WandOperation type,
			 Window* window,
			 FramesDocument* doc,
			 BOOL3 bool3)
	{
		OP_ASSERT(window ||
				  type == SuspendedWandOperation::WAND_OPERATION_RETRIEVE_PASSWORD_NO_WINDOW ||
				  type == SuspendedWandOperation::WAND_OPERATION_CHANGE_DB_ENCRYPTION_NO_WINDOW ||
				  type == SuspendedWandOperation::WAND_OPERATION_STORE_LOGIN_NO_WINDOW);

		OP_ASSERT(m_type == NO_WAND_OPERATION);

		m_type = type;
		m_window = window;
		m_doc = doc;

		m_bool3 = bool3;

		if (window)
		{
			OpStatus::Ignore(m_wand_security.Enable(window));
		}
		else
		{
			OpStatus::Ignore(m_wand_security.EnableWithoutWindow());
		}
	}

private:

	WandOperation m_type;

	Window*			m_window;
	FramesDocument* m_doc;
	BOOL3			m_bool3;
	WandSecurityWrapper m_wand_security;
};

class SuspendedWandOperationWithPage : public SuspendedWandOperation
{
public:
	SuspendedWandOperationWithPage() :
		m_wand_page(NULL),
		m_owns_wand_page(FALSE)
	{
	}

	virtual ~SuspendedWandOperationWithPage();

	WandPage* TakeWandPage() { WandPage* tmp = m_wand_page; m_wand_page = NULL; return tmp; }

	void Set(WandOperation type,
			 Window* window,
			 FramesDocument* doc,
			 BOOL3 bool3,
			 WandPage* wand_page,
			 BOOL owns_wand_page)
	{
		SuspendedWandOperation::Set(type, window, doc, bool3);

		OP_ASSERT(m_wand_page == NULL);

		m_wand_page = wand_page;
		m_owns_wand_page = owns_wand_page;
	}

private:
	WandPage*		m_wand_page;
	BOOL			m_owns_wand_page;
};

class SuspendedWandOperationWithLogin : public SuspendedWandOperation
{
public:
	SuspendedWandOperationWithLogin() :
		m_wand_login(NULL),
		m_callback(NULL)
	{
	}

	virtual ~SuspendedWandOperationWithLogin();

	WandLogin* TakeWandLogin() { WandLogin* tmp = m_wand_login; m_wand_login = NULL; return tmp; }
	WandLoginCallback* TakeCallback() { WandLoginCallback* tmp = m_callback; m_callback = NULL; return tmp; }

	void Set(WandOperation type,
			 Window* window,
			 FramesDocument* doc,
			 BOOL3 bool3,
			 WandLogin* wand_login,
			 WandLoginCallback* callback)
	{
		SuspendedWandOperation::Set(type, window, doc, bool3);

		OP_ASSERT(m_wand_login == NULL);

		m_wand_login = wand_login;
		m_callback = callback;
	}

private:
	WandLogin*		m_wand_login;
	WandLoginCallback* m_callback;
};

class SuspendedWandOperationWithInfo : public SuspendedWandOperation
{
public:
	SuspendedWandOperationWithInfo() :
		m_wand_info(NULL)
	{
	}

	~SuspendedWandOperationWithInfo();

	WandInfo* TakeWandInfo() { WandInfo* tmp = m_wand_info; m_wand_info = NULL; return tmp; }
	WAND_ACTION GetGenericAction() { return m_generic_action; }
#ifdef WAND_ECOMMERCE_SUPPORT
	WAND_ECOMMERCE_ACTION GetECommerceAction() { return m_eCommerce_action; }
#endif // WAND_ECOMMERCE_SUPPORT

	void Set(WandOperation type,
			 Window* window,
			 FramesDocument* doc,
			 BOOL3 bool3,
			 WandInfo* wand_info,
			 WAND_ACTION generic_action,
			 int eCommerce_action)
	{
		SuspendedWandOperation::Set(type, window, doc, bool3);

		OP_ASSERT(m_wand_info == NULL);

		m_wand_info = wand_info;
		m_generic_action = generic_action;
#ifdef WAND_ECOMMERCE_SUPPORT
		m_eCommerce_action = static_cast<WAND_ECOMMERCE_ACTION>(eCommerce_action);
#endif // WAND_ECOMMERCE_SUPPORT
	}

private:
	WandInfo* m_wand_info;
	WAND_ACTION m_generic_action;
#ifdef WAND_ECOMMERCE_SUPPORT
	WAND_ECOMMERCE_ACTION m_eCommerce_action;
#endif // WAND_ECOMMERCE_SUPPORT
};

#ifdef SYNC_HAVE_PASSWORD_MANAGER
class SuspendedWandOperationWithSync : public SuspendedWandOperation
{
public:
	SuspendedWandOperationWithSync()
	: m_flush_dirty_sync(FALSE)
	, m_flush_first_sync(FALSE)
{
}
	BOOL GetDirtySync(){ return m_flush_dirty_sync; }
	BOOL GetFirstSync(){ return m_flush_first_sync; }

	void Set(WandOperation type, BOOL flush_first_sync, BOOL flush_dirty_sync)
	{
		SuspendedWandOperation::Set(type, NULL, NULL, NO);
		m_flush_first_sync = flush_first_sync;
		m_flush_dirty_sync = flush_dirty_sync;
	}

private:
	BOOL m_flush_dirty_sync;
	BOOL m_flush_first_sync;
};
#endif // SYNC_HAVE_PASSWORD_MANAGER

#endif // WAND_SUPPORT

#endif // SUSPENDEDOPERATION_H
