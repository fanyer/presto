/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_JILUTILS_H
#define DOM_JILUTILS_H

#ifdef DOM_JIL_API_SUPPORT
#include "modules/device_api/jil/JILFeatures.h"
#include "modules/util/OpHashTable.h"
#ifdef PI_POWER_STATUS
#include "modules/pi/device_api/OpPowerStatus.h"
#endif // PI_POWER_STATUS
#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/dom/src/domsuspendcallback.h"
#include "modules/device_api/jil/utils.h"
#include "modules/device_api/SystemResourceSetter.h"

class ES_Object;
class JILApplicationNames;
class OpGadget;
class DOM_Runtime;


struct DOM_JIL_Array_Element
{
	ES_Value m_val;
	int m_index;
};

class JIL_PropertyMappings;

class JILUtils
{
public:
	JILUtils();
	virtual ~JILUtils();
	void ConstructL();

	/**
	 * See JILApplicationNames.
	 */
	JILApplicationNames* GetApplicationNames() { return m_jil_application_names;}

	/**
	 * Extracts the gadget object from origining_runtime.
	 *
	 * @return gadget object associated with the origining_runtime or NULL
	 */
	OpGadget* GetGadgetFromRuntime(DOM_Runtime* origining_runtime);

	/**
	 * Helper determining whether runtime should have access to filesystem.
	 * should only be called by JIL objects in widgets
	 */
	// TODO: this functionality should be moved to security_manager
	BOOL RuntimeHasFilesystemAccess(DOM_Runtime* origining_runtime);

	static BOOL ValidatePhoneNumber(const uni_char* number);

	/**
	 * Check whether given api_name is present in configuration file of a
	 * gadget for this runtime.
	 */
	BOOL IsJILApiRequested(JILFeatures::JIL_Api api_name, DOM_Runtime* runtime);

	/** Helper class for implementing suspending callbacks
	*/
	template<class CallbackInterface>
	class ModificationSuspendingCallbackImpl : public DOM_SuspendCallback<CallbackInterface>
	{
	public:
		virtual void OnSuccess(const uni_char* id)
		{
			OP_STATUS error = m_id.Set(id);
			if (OpStatus::IsError(error))
				DOM_SuspendCallbackBase::OnFailed(error);
			else
				DOM_SuspendCallbackBase::OnSuccess();
		}

		virtual void OnFailed(const uni_char* id, OP_STATUS error)
		{
			m_id.Set(id);
			DOM_SuspendCallbackBase::OnFailed(error);
		}

		const uni_char* GetElementId() const { return m_id.CStr(); }
	private:
		OpString m_id;
	};

	class SetSystemResourceCallbackSuspendingImpl : public DOM_SuspendCallback<SystemResourceSetter::SetSystemResourceCallback>
	{
	public:
		virtual void OnFinished(OP_SYSTEM_RESOURCE_SETTER_STATUS status);
	};
private:
	JILApplicationNames* m_jil_application_names;
};

#endif// DOM_JIL_API_SUPPORT

#endif // DOM_JILUTILS_H
