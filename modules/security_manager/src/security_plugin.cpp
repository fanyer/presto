/* Copyright 2005 Opera Software ASA.  All rights Reserved.

   Cross-module security manager: Plugin model
   Lars T Hansen.

   See documentation/plugin-security.txt in the documentation directory.  */

#include "core/pch.h"

#ifdef _PLUGIN_SUPPORT_

#include "modules/url/url_man.h"
#include "modules/regexp/include/regexp_api.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/security_manager/src/security_plugin.h"

OpSecurityManager_Plugin::OpSecurityManager_Plugin()
	: re(NULL)
{
}

OpSecurityManager_Plugin::~OpSecurityManager_Plugin()
{
	OP_DELETE(re);
}

OP_STATUS
OpSecurityManager_Plugin::Init()
{
	if (re != NULL)
		return OpStatus::OK;

	// The regexp is documented in plugin-security.txt, be sure to keep the documentation
	// in sync with reality.

	OpREFlags flags;
	flags.multi_line = FALSE;
	flags.case_insensitive = FALSE;
	flags.ignore_whitespace = FALSE;
	OP_STATUS res =
		OpRegExp::CreateRegExp(&re,
							   UNI_L("^javascript:\\s*(?:(?:window|top|document)\\.)?location(?:\\.href)?(?:\\s*\\+\\s*\\\"__flashplugin_unique__\\\")?\\s*;?\\s*$"),
							   &flags);

	return res;
}

OP_STATUS
OpSecurityManager_Plugin::CheckPluginSecurityRunscript(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed)
{
	// The default is to deny
	allowed = FALSE;

	// Setup regular expressions, etc
	RETURN_IF_ERROR(Init());

	// PLUGIN_RUNSCRIPT requires source code to be present

	const uni_char* script_src = target.GetText();
	if (script_src == NULL)
	{
		OP_ASSERT(!"CheckPluginSecurity requires the script source code");
		goto failure;
	}

	// Do different things depending on whether the source is known
	if (!source.GetURL().IsEmpty())
	{
		// If a standard origin check passes then accept
		if (OpSecurityManager::OriginCheck(source, target))
			goto success;
	}

	// Otherwise, if the script looks harmless then accept
#if 1
	// See bug 213716: we always allow the script to be executed,
	// since there are many compatibility issues with not doing so.
	// But we would like the TFHB to be able to turn on stricter
	// checking here later, so we keep the policy code here even
	// if it is not enabled.
	goto success;
#else
	OpREMatchLoc ml;

	// If the matcher fails or fails to match then reject
	RETURN_IF_ERROR(re->Match(script_src, &ml));
	if (ml.matchloc == REGEXP_NO_MATCH)
		goto failure;

	goto success;
#endif

failure:
	allowed = FALSE;
	return OpStatus::OK;

success:
	allowed = TRUE;
	return OpStatus::OK;
}

#endif // _PLUGIN_SUPPORT_
