/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_JILAPPLICATIONNAMES_H
#define DOM_JILAPPLICATIONNAMES_H

#include "modules/dom/src/opatom.h"

#include "modules/windowcommander/OpWindowCommander.h"

class ES_Object;
class DOM_JILGetInstalledApplicationsCallback;

/** @short Store for application JIL name application type enum mapping.
 *
 * It is not a DOM object available from EcmaScript.
 *
 * JIL defines "predefined" application names on Widget.Device.ApplicationTypes
 * object. DOM_JILApplicationNames provides mapping between these well-known
 * names and OpSystemInfo::ApplicationType enum values.
 *
 * It also provides a more dynamic mapping for custom applications, i.e. those
 * not having named OpSystemInfo::ApplicationType enum values. This additional
 * mapping can be set-up by TakeOverCustomApplications.
 */
class JILApplicationNames
{
	friend class DOM_JILGetInstalledApplicationsCallback;	// TODO: coupling between these two classes should be decreased.
public:

	/* Application parameter type */
	enum JILApplicationParameterType
	{
		APP_PARAM_NONE,
		APP_PARAM_URL,
		APP_PARAM_PATH,
		APP_PARAM_OTHER,
		APP_PARAM_UNKNOWN
	};

	struct CustomApplicationDesc
	{
		OpApplicationListener::ApplicationType type;
		uni_char* path;

		CustomApplicationDesc()	: path(NULL) { }
		~CustomApplicationDesc()	{ op_free(path); }
	};

	void ConstructL();

	/** Return application type for given JIL name.
	 *
	 * @param name name used in JIL for identifying the application. It is either
	 *             one of the predefined JIL names or custom ones that are provided
	 *             to UpdateCustomApplications
	 * @param[out] app_type set to application type value that identifies this
	 *             application. It may be either
	 * @return
	 *  - OpStatus::OK on success,
	 *  - OpStatus::ERR_NO_SUCH_RESOURCE if there is no application by that
	 *		name or other error values,
	 *  - OpStatus::ERR_NO_MEMORY.
	 *
	 * @see DOM_JILApplicationNames::TakeOverCustomApplications, OpApplicationListener::ExecuteApplication
	 */
	OP_STATUS GetApplicationByJILName(const uni_char* name, OpApplicationListener::ApplicationType* app_type);

	/** Return application parameter type for given application type.
	 *
	 * @param app_type application type value that identifies this
	 *             application.
	 * @param[out]- application parameter type
	 * @return
	 *  - OpStatus::OK on success,
	 *  - OpStatus::ERR_NO_SUCH_RESOURCE if there is no application by that
	 *		type,
	 *  - OpStatus::NULL_POINTER if param_type is NULL
	 *
	 */
	OP_STATUS GetApplicationParameterType(OpApplicationListener::ApplicationType app_type, JILApplicationParameterType *param_type);

	/** Sets up a name to application type mapping for custom applications.
	 *
	 * @param custom_application_names a name to ApplicationType mapping.
	 *        The ownership is taken by this object.
	 */
	void TakeOverCustomApplications(OpAutoStringHashTable<CustomApplicationDesc>* custom_application_names)	{ m_custom_applications.reset(custom_application_names); }

	OP_STATUS MakeJILApplicationTypesObject(ES_Object** new_app_types_obj, DOM_Runtime* runtime);
private:

	/** Provides JIL predefined name for application type.
	 *
	 * JIL predefined names are those defined in JIL Widget.Device.ApplicationTypes object.
	 *
	 * @returns name string or NULL if there is no JIL standard name for given type.
	 */
	const uni_char* TypeToJILPredefinedName(OpApplicationListener::ApplicationType type);

	/** Provides application type value for a given JIL predefined name.
	 *
	 * @param name name of the application.
	 * @param[out] app_type set to type value.
	 * @return OpStatus::OK if app_type has been set, OpStatus::ERR_NO_SUCH_RESOURCE
	 *         if name is not one of the predefined names, other values on error.
	 */
	OP_STATUS JILPredefinedNameToType(const uni_char* name, OpApplicationListener::ApplicationType* app_type);

	/** Mapping of predefined JIL application names to types.
	 */
	const uni_char* m_jil_name_to_type[OpApplicationListener::APPLICATION_LAST];

	OpAutoPtr<OpAutoStringHashTable<CustomApplicationDesc> > m_custom_applications;
};

/** A helper class for building a hash table of custom applications IDs for JILApplicationNames.
 */
class CustomApplicationsBuilder
{
public:
	/** Constructor.
	 *
	 * @params jil_application_names The JILApplicationNames to update with Save method.
	 */
	CustomApplicationsBuilder()
		: m_custom_applications(NULL)
	{
	}

	OP_STATUS AddApplication(OpApplicationListener::ApplicationType type, const uni_char* path)
	{
		RETURN_IF_ERROR(EnsureCustomApplicationsMap());
		JILApplicationNames::CustomApplicationDesc* custom_app = OP_NEW(JILApplicationNames::CustomApplicationDesc, ());
		RETURN_OOM_IF_NULL(custom_app);
		OpAutoPtr<JILApplicationNames::CustomApplicationDesc> custom_app_deleter(custom_app);
		custom_app->path = uni_strdup(path);
		RETURN_OOM_IF_NULL(custom_app->path);
		custom_app->type = type;
		RETURN_IF_ERROR(m_custom_applications->Add(custom_app->path, custom_app));
		custom_app_deleter.release();
		return OpStatus::OK;
	}

	/** Saves the collected applications in a JILApplicationNames object.
	 *
	 * The collected applications data is passed over to the jil_application_names
	 * object, not copied, therefore the object is restored to a clean state after
	 * this operation.
	 */
	void Give(JILApplicationNames* jil_application_names)
	{
		jil_application_names->TakeOverCustomApplications(m_custom_applications);
		m_custom_applications = NULL;
	}

private:
	OP_STATUS EnsureCustomApplicationsMap()
	{
		if (m_custom_applications == NULL)
		{
			m_custom_applications = OP_NEW(OpAutoStringHashTable<JILApplicationNames::CustomApplicationDesc>, ());
			return m_custom_applications ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
		}
		return OpStatus::OK;
	}

	OpAutoStringHashTable<JILApplicationNames::CustomApplicationDesc>* m_custom_applications;
};

#endif // DOM_JILAPPLICATIONNAMES_H
