/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMCALLLSTATE_H
#define DOM_DOMCALLLSTATE_H

#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/dom/src/domobj.h"

class DOM_CallState : public DOM_Object
{
public:
	enum CallPhase
	{
		PHASE_NONE = 0,
		PHASE_SECURITY_CHECK = 1,
		PHASE_EXECUTION_0 = 2
	};

	static OP_STATUS Make(DOM_CallState*& new_obj, DOM_Runtime* origining_runtime, ES_Value* argv, int argc);
	DOM_CallState();
	virtual ~DOM_CallState();

	virtual void GCTrace();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_CALLSTATE || DOM_Object::IsA(type); }

	static DOM_CallState* FromReturnValue(ES_Value* return_value);
	static DOM_CallState* FromES_Object(ES_Object* es_object);
	static OP_STATUS RestoreArgumentsFromRestartValue(ES_Value* restart_value, ES_Value*& argv, int& argc);
	static CallPhase GetPhaseFromESValue(ES_Value* restart_value);

	/** Prepares call state for actual suspending

	 * Performs actions necessary for suspending like making copies of strings in argv, which we don't
	 * want to perform if we don't really have to(when we are using it and call doesnt need to suspend).
	 */
	OP_STATUS PrepareForSuspend();
	void RestoreArguments(ES_Value*& argv, int& argc) const { argv = m_argv; argc = m_argc; }

	void SetPhase(CallPhase phase) { m_phase = phase; }
	CallPhase GetPhase() const { return m_phase; }

	void SetUserData(void* user_data) { m_user_data = user_data; }
	void* GetUserData() const { return m_user_data; }
private:
	OP_STATUS SetArguments(ES_Value* argv, int argc);
	ES_Value*	m_argv;
	int			m_argc;
	void*		m_user_data;
	CallPhase	m_phase;
	BOOL        m_ready_for_suspend;
};

#ifdef _DEBUG
# define DOM_CHECK_OR_RESTORE_PERFORMED int ___missing_DOM_CHECK_OR_RESTORE_ARGUMENTS
# define DOM_CHECK_OR_RESTORE_GUARD ___missing_DOM_CHECK_OR_RESTORE_ARGUMENTS = 0, (void)___missing_DOM_CHECK_OR_RESTORE_ARGUMENTS
#else
# define DOM_CHECK_OR_RESTORE_PERFORMED
# define DOM_CHECK_OR_RESTORE_GUARD
#endif // _DEBUG

/** Check arguments and restore them from callstate if it's a restarted call.
 *
 * When the function is entered the first time, it behaves as DOM_CHECK_ARGUMENTS.
 * If the function is suspended and entered for the second (or more) time the
 * macro restores original arguments from the callstate.
 *
 * It also sets a DOM_CHECK_OR_RESTORE_PERFORMED "marker" in its scope so that
 * other macros that need rely on the arguments being restored may make sure
 * that the arguments have been restored by embedding DOM_CHECK_OR_RESTORE_GUARD.
 */
#define DOM_CHECK_OR_RESTORE_ARGUMENTS(expected_arguments)						\
DOM_CHECK_OR_RESTORE_PERFORMED;													\
DOM_CHECK_OR_RESTORE_GUARD;														\
{																				\
	if (argc < 0)																\
		CALL_FAILED_IF_ERROR(DOM_CallState::RestoreArgumentsFromRestartValue(return_value, argv, argc));\
	DOM_CHECK_ARGUMENTS(expected_arguments);									\
}

#ifdef DOM_JIL_API_SUPPORT
/** JIL version of DOM_CHECK_OR_RESTORE_ARGUMENTS.
 *
 * Throws JIL exception if the arguments are incorrect. Otherwise it behaves
 * exactly the same as DOM_CHECK_OR_RESTORE_ARGUMENTS.
 */
#define DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL(expected_arguments)						\
DOM_CHECK_OR_RESTORE_PERFORMED;														\
DOM_CHECK_OR_RESTORE_GUARD;															\
{																					\
	if (argc < 0)																	\
		CALL_FAILED_IF_ERROR(DOM_CallState::RestoreArgumentsFromRestartValue(return_value, argv, argc));\
	DOM_CHECK_ARGUMENTS_JIL(expected_arguments);									\
}
#endif

#endif // DOM_DOMCALLLSTATE_H
