/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMASYNCCALLBACK_H
#define DOM_DOMASYNCCALLBACK_H

class ES_Value;
class ES_Object;
class DOM_Runtime;

/** Util class working 'like' auto_ptr but for garbage collected objects.
 *
 * Its intended usage is providing simple mechanism for safely using garbage collected items in situations where
 * implementing full EcmaScript_Object and using GCMark would be overkill.
 * As protecting of an object can fail 2 stage init is required( Constructor which never fails + Protect to initiate protectiong of an item).
 */
class DOM_AutoProtectPtr
{
public:
	/** Constructor
	 *
	 * Does not call Protect on obj, just sets the members of DOM_AutoProtectPtr. The actual protecting is performed in Protect.
	 *
	 * @param obj - object which will be protected. If this is NULL then no actions will be performed by Protect.
	 * @param runtime - runtime on which the object will be protected. MUST NOT be NULL.
	 */
	DOM_AutoProtectPtr(ES_Object* obj, ES_Runtime* runtime);

	/** Destructor
	 *
	 * If the object contained in it was successfully protected, then it will be unprotected here.
	 */
	~DOM_AutoProtectPtr();

	/** Second stage of initialization.
	 *
	 * If it succeeded then it MUST NOT be called again unless the object was Reset'ed.
	 *
	 * @return
	 *	-OK
	 *	-ERR_NO_MEMORY
	 */
	OP_STATUS Protect();

	/** Resets the object held by this DOM_AutoProtectPtr.
	 *
	 * If the object which was previously held was successfully protected then it will be unprotected.
	 * This will NOT protect newly assigned object(Protect must be called separately). This will NOT affect
	 * ES_Runtime which must be set in the constructor.
	 */
	void Reset(ES_Object* obj);

	/** Return the object held by this DOM_AutoProtectPtr.
	 */
	ES_Object* Get() { return m_obj; }
private:
	ES_Object*  m_obj;
	ES_Runtime* m_runtime;
	BOOL        m_was_protected;
};

class DOM_AsyncCallback : public ListElement<DOM_AsyncCallback>
{
public:
	DOM_AsyncCallback(DOM_Runtime* runtime, ES_Object* callback, ES_Object* this_object);
	~DOM_AsyncCallback();
	virtual OP_STATUS Construct();

	OP_STATUS CallCallback(ES_Value* argv, unsigned int argc);

	/** Called by DOM environment when it is about to be destroyed */
	virtual void OnBeforeDestroy();

	void Abort();
	BOOL IsAborted() { return m_aborted; }
	DOM_Runtime* GetRuntime() { return m_runtime; }
private:
	DOM_Runtime* m_runtime;
	DOM_AutoProtectPtr m_callback_protector;
	DOM_AutoProtectPtr m_this_object_protector;
	BOOL m_aborted;
};

#endif // DOM_DOMASYNCCALLBACK_H
