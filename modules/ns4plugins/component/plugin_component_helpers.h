/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef NS4P_PLUGIN_COMPONENT_HELPERS_H
#define NS4P_PLUGIN_COMPONENT_HELPERS_H

#ifdef NS4P_COMPONENT_PLUGINS

#include "modules/ns4plugins/src/plug-inc/npruntime.h"
#include "modules/ns4plugins/component/plugin_component.h"

class PluginComponentHelpers
{
public:
	typedef OpNs4pluginsMessages_MessageSet::PluginObject PluginObject;
	typedef OpNs4pluginsMessages_MessageSet::PluginVariant PluginVariant;
	typedef OpNs4pluginsMessages_MessageSet::PluginIdentifier PluginIdentifier;
	typedef OpNs4pluginsMessages_MessageSet::PluginRect PluginRect;
	typedef OpNs4pluginsMessages_MessageSet::PluginPoint PluginPoint;

	/**
	 * Convert UniString to NPString.
	 *
	 * @param[out] out_string NPString structure allocated by caller.
	 * @param string Input UniString.
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	static OP_STATUS SetNPString(NPString* out_string, const UniString& string);

	/**
	 * Convert PluginVariant to NPVariant.
	 *
	 * @param[out] out_variant NPVariant structure allocated by caller.
	 * @param variant Input PluginVariant.
	 * @param pc PluginComponent to bind the value to. Applicable only if type is Object.
	 *
	 * @return See OpStatus.
	 * @retval OpStatus::OK on successful conversion.
	 * @retval OpStatus::ERR_NO_MEMORY when running out of memory during conversion.
	 * @retval OpStatus::ERR_NO_SUCH_RESOURCE when the PluginComponent cannot be found.
	 */
	static OP_STATUS SetNPVariant(NPVariant* out_variant, const PluginVariant& variant, PluginComponent* pc = PluginComponent::FindPluginComponent());

	/**
	 * Release NPVariant data allocated by SetVariantValue.
	 *
	 * @param variant Variant whose data is to be deallocated. The structure itself
	 *        remains is not deallocated.
	 */
	static void ReleaseNPVariant(NPVariant* variant);

	/**
	 * Convert NPVariant to PluginVariant.
	 *
	 * @param[out] out_variant PluginVariant structure allocated by caller.
	 * @param variant Input NPVariant.
	 * @param pc Necessary in order to retrieve the value of a variant of type Object.
	 *
	 * @return See OpStatus.
	 * @retval OpStatus::OK on successful conversion.
	 * @retval OpStatus::ERR_NO_MEMORY when running out of memory during conversion.
	 * @retval OpStatus::ERR_NO_SUCH_RESOURCE when the PluginComponent cannot be found.
	 * @retval OpStatus::ERR_OUT_OF_RANGE when the value cannot be retrieved.
	 */
	static OP_STATUS SetPluginVariant(PluginVariant& out_variant, const NPVariant& variant, PluginComponent* pc = PluginComponent::FindPluginComponent());

	/**
	 * Convert a browser side NPObject to a PluginObject.
	 *
	 * @param np_object Browser side object pointer.
	 *
	 * @return PluginObject or NULL on OOM. The caller assumes ownership of the
	 *         returned object.
	 */
	static PluginObject* ToPluginObject(UINT64 np_object);

	/**
	 * Embed browser side object data in a PluginObject.
	 *
	 * @param[out] out_object PluginObject in which to embed the data.
	 * @param np_object Browser side object pointer.
	 *
	 * @return PluginObject or NULL on OOM. The caller assumes ownership of the
	 *         returned object.
	 */
	static void SetPluginObject(PluginObject& out_object, UINT64 np_object);

	/**
	 * Convert a local identifier to a PluginIdentifier.
	 *
	 * @param[out] out_identifier PluginIdentifier allocated by caller.
	 * @param identifier Local identifier.
	 * @param ic Pointer to global identifier cache.
	 */
	static void SetPluginIdentifier(PluginIdentifier& out_identifier, NPIdentifier identifier, IdentifierCache* ic);

	/**
	 * Build PluginResult message.
	 *
	 * @param success Required boolean field value.
	 * @param result Optional PluginVariant field value. Will not be set if 'success' is false.
	 *
	 * @return Pointer to OpPluginResultMessage or NULL on OOM. Returned value must be free'd
	 *         by caller.
	 */
	static OpPluginResultMessage* BuildPluginResultMessage(bool success, const NPVariant* result);

	/**
	 * Convert a PluginRect to a OpRect.
	 *
	 * @param[out] out_rect Rectangle structure allocated by caller.
	 * @param rect PluginRect.
	 */
	static void SetOpRect(OpRect* out_rect, const PluginRect& rect);

	/**
	 * Convert an NPRect to a OpRect.
	 *
	 * @param[out] out_rect Rectangle structure allocated by caller.
	 * @param rect NPRect.
	 */
	static void SetOpRect(OpRect* out_rect, const NPRect& rect);

	/**
	 * Convert an NPRect to a PluginRect.
	 *
	 * @param[out] out_rect Rectangle structure allocated by caller.
	 * @param rect NPRect.
	 */
	static void SetPluginRect(PluginRect& out_rect, const NPRect& rect);

	/**
	 * Convert an OpRect to a protobuf PluginRect.
	 *
	 * @param[out] out_rect, Rectangle structure allocated by caller.
	 * @param rect OpRect.
	 */
	static void SetPluginRect(PluginRect& out_rect, const OpRect& rect);

	/**
	 * Convert an OpPoint to a protobuf PluginPoint.
	 *
	 * @param[out] out_point, PluginPoint structure allocated by caller.
	 * @param point OpPoint.
	 */
	static void SetPluginPoint(PluginPoint* out_point, const OpPoint& point);

	/**
	 * Convert an protobuf PluginPoint to an OpPoint.
	 */
	static OpPoint OpPointFromPluginPoint(PluginPoint& point);

	/**
	 * Classify boolean plug-in variables.
	 *
	 * @return True if the variable is associated with a boolean value.
	 */
	static bool IsBooleanValue(NPPVariable variable);

	/**
	 * Classify integral plug-in variables.
	 *
	 * @return True if the variable is associated with an integer value.
	 */
	static bool IsIntegerValue(NPPVariable variable);

	/**
	 * Classify EcmaScript object plug-in variables.
	 *
	 * @return True if the variable is associated with an object value.
	 */
	static bool IsObjectValue(NPPVariable variable);
};

#endif // NS4P_COMPONENT_PLUGINS

#endif // NS4P_PLUGIN_COMPONENT_HELPERS_H
