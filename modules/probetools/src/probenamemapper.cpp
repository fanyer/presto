/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** \author { Anders Oredsson <anderso@opera.com> }
*/

#include "core/pch.h"

#ifdef SUPPORT_PROBETOOLS

#include "modules/probetools/src/probeidentifiers.h"
#include "modules/probetools/src/probenamemapper.h"
#include "modules/probetools/generated/probedefines.h"
#include "modules/hardcore/mh/messages.h"

// Put probe index_parameter name-mappings into this method.
// Only usable in special cases, returns false if no name was copied
bool 
OpProbeNameMapper::GetMappedProbeParameterName(OpProbeIdentifier& probe_id, char* targetBuffer, size_t targetBufferLen)
{
	bool result = false;
	switch(probe_id.get_location())
	{
	case OP_PROBE_HC_MSG:
		op_snprintf(targetBuffer, targetBufferLen, "%s",
					GetStringFromMessage(static_cast<OpMessage>(probe_id.get_index_parameter())));
		result = true;
		break;

#ifdef HC_MODULE_INIT_PROFILING
	case OP_PROBE_HC_INIT_MODULE:
		op_snprintf(targetBuffer, targetBufferLen,
					"OP_PROBE_HC_INIT_MODULE(%s)",
					g_opera->module_names[probe_id.get_index_parameter()]);
		result = true;
		break;

	case OP_PROBE_HC_DESTROY_MODULE:
		op_snprintf(targetBuffer, targetBufferLen,
					"OP_PROBE_HC_DESTROY_MODULE(%s)",
					g_opera->module_names[probe_id.get_index_parameter()]);
		result = true;
		break;
#endif // HC_MODULE_INIT_PROFILING

#ifdef GADGET_SUPPORT
	case OP_PROBE_GADGETS_ISTHISA:
		{
			const char* type_name = "unknown";
			switch (probe_id.get_index_parameter())
			{
#ifdef WEBSERVER_SUPPORT
			case URL_UNITESERVICE_INSTALL_CONTENT: type_name = "unite_service"; break;
#endif // WEBSERVER_SUPPORT
#ifdef EXTENSION_SUPPORT
			case URL_EXTENSION_INSTALL_CONTENT: type_name = "extension"; break;
#endif // EXTENSION_SUPPORT
			case URL_GADGET_INSTALL_CONTENT: type_name = "gadget"; break;
			default:
				OP_ASSERT(!"Unknown gadget install content type");
			}
			op_snprintf(targetBuffer, targetBufferLen,
						"OP_PROBE_GADGETS_ISTHISA(%s)", type_name);
			result = true;
			break;
		}
#endif // GADGET_SUPPORT
	}
	// return what we found
	return result;
}

#endif
