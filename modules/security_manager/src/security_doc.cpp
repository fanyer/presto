/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2007-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/security_manager/include/security_manager.h"
#include "modules/encodings/utility/charsetnames.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/docman.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/gadgets/OpGadgetClass.h"
#include "modules/gadgets/OpGadget.h"

#ifdef _LOCALHOST_SUPPORT_
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/formats/uri_escape.h"
#include "modules/url/url2.h"
#endif // _LOCALHOST_SUPPORT_

#ifdef DOM_JIL_API_SUPPORT
# include "modules/device_api/jil/JILFSMgr.h"
#endif // DOM_JIL_API_SUPPORT

OP_STATUS OpSecurityManager_DOC::CheckPreferredCharsetSecurity(const OpSecurityContext& source, const OpSecurityContext &target, BOOL &allowed)
{
	// This method answers the question if a site can be allowed to fetch (inherit) charset encoding from target.

	// We would prefer to be somewhat careful here since by changing what encoding
	// is used, it changes meaning of the text and can turn harmless text into
	// something else. This is mainly a problem with the UTF-7 charset where
	// the encoded data will look like very innocent ascii text.

	// Unfortunately we can't be too picky because it breaks sites. See CORE-18816

	// Perform the origin check if a charset is set for some document.
	allowed = !target.inline_element ? OpSecurityManager::OriginCheck(source, target) : TRUE;

	// Check context
	OP_ASSERT(target.m_charset);
	if (!target.m_charset)
		return OpStatus::OK;
	const char *target_charset = g_charsetManager->GetCanonicalCharsetName(target.m_charset);

	// UTF-7 is dangerous, never allow it to be inherited
	if (target_charset && op_strcmp(target_charset, "utf-7") == 0)
		allowed = FALSE;

	return OpStatus::OK;
}

#ifdef _LOCALHOST_SUPPORT_
/**
 * Check if the given file: URL is an allowed stylesheet import inside the
 * generated document for an XML parse error. Only allowed if it is
 * equal to the underlying pref or that of the 'standard' opera.css.
 *
 * @param the target URL being loaded inline.
 * @return OpBoolean::IS_TRUE if allowed, OpBoolean::IS_FALSE.
 *         OpStatus::ERR_NO_MEMORY on OOM.
 */
static OP_BOOLEAN
IsAllowedStyleFileImport(const URL &url)
{
	OpString file_path;
	RETURN_IF_ERROR(url.GetAttribute(URL::KUniName, file_path));

	OpString xmlerror_css;
	RETURN_IF_LEAVE(g_pcfiles->GetFileURLL(PrefsCollectionFiles::StyleErrorFile, &xmlerror_css));
	if (file_path.Compare(xmlerror_css) == 0)
		return OpBoolean::IS_TRUE;

	/* This is not complete, but we do also allow the importing
	   of the style folder's opera.css. Clearly someone could
	   provide a custom error.css that has an arbitrary collection
	   of imports. This will not work for generated XML error pages. */
	OpFile opera_css;
	RETURN_IF_ERROR(opera_css.Construct(UNI_L("opera.css"), OPFILE_STYLE_FOLDER));
	const uni_char *path = opera_css.GetFullPath();
	TempBuffer escaped_path;
	RETURN_IF_ERROR(escaped_path.Expand(uni_strlen(path) * 3 + 1));

	UriEscape::Escape(escaped_path.GetStorage(), path, UriEscape::Filename);
	OpString opera_css_file;
	RETURN_IF_LEAVE(g_url_api->ResolveUrlNameL(escaped_path.GetStorage(), opera_css_file));
	if (file_path.Compare(opera_css_file) == 0)
		return OpBoolean::IS_TRUE;

	return OpBoolean::IS_FALSE;
}
#endif // _LOCALHOST_SUPPORT_

OP_STATUS OpSecurityManager_DOC::CheckInlineSecurity(const OpSecurityContext& source, const OpSecurityContext& target, BOOL& allowed, OpSecurityState& state)
{
	allowed = FALSE;
	state.suspended = FALSE;
	state.host_resolving_comm = NULL;

#ifdef GADGET_SUPPORT
	if (source.IsGadget())
		return g_secman_instance->CheckGadgetUrlLoadSecurity(source, target, allowed, state);
#endif // GADGET_SUPPORT

	FramesDocument* doc = source.GetDoc();
	BOOL from_user_css = target.IsFromUserCss();
	URL inline_url = target.GetURL();

	URLType inline_url_type = inline_url.Type(), doc_url_type = doc->GetSecurityContext().Type();

	/* Disallow javascript: URLs for all inline types. */
	if (inline_url.Type() == URL_JAVASCRIPT)
		return OpStatus::OK;

	if (doc->GetSuppress(inline_url_type))
		return OpStatus::OK;

	if (inline_url_type == URL_FILE && doc_url_type != URL_FILE && doc_url_type != URL_EMAIL)
	{
		BOOL reject_file = TRUE;

		if (from_user_css)
			/* We're allowed to load file url resources from user stylesheets. */
			reject_file = FALSE;

#ifdef _LOCALHOST_SUPPORT_
		else if (doc->GetLogicalDocument() && doc->GetLogicalDocument()->IsXmlParsingFailed() && target.inline_type == CSS_INLINE)
		{
			/* The document is a generated XML parsing error document that links in
			   a stylesheet from a local file. Allow it (and its assumed import) only. */
			OP_BOOLEAN result = IsAllowedStyleFileImport(target.GetURL());
			RETURN_IF_ERROR(result);
			if (result == OpBoolean::IS_TRUE)
				reject_file = FALSE;
		}
#endif // _LOCALHOST_SUPPORT_

		else if (doc->IsGeneratedByOpera())
			reject_file = FALSE;

		else if (inline_url.GetAttribute(URL::KIsGeneratedByOpera))
			reject_file = FALSE;

#ifdef WEBFEEDS_DISPLAY_SUPPORT
		if (doc->IsInlineFeed())
			/* The document is a generated HTML document that might link in
			   local stylesheets */
			reject_file = FALSE;
#endif // WEBFEEDS_DISPLAY_SUPPORT

#ifdef GADGET_SUPPORT
#ifdef DOM_JIL_API_SUPPORT
		if (doc->GetWindow()->GetGadget() &&
			doc->GetWindow()->GetGadget()->GetClass()->HasJILFilesystemAccess())
			/* This is JIL widget which has access to the filesystem */
			reject_file = FALSE;
#endif // DOM_JIL_API_SUPPORT
#endif // GADGET_SUPPORT

		if (reject_file)
			return OpStatus::OK;
	}

	if (inline_url_type == URL_OPERA && doc_url_type != URL_OPERA)
	{
		BOOL reject_opera = TRUE;

		if (doc->GetURL().IsImage() && inline_url.GetAttribute(URL::KPath).Compare("style/image.css") == 0)
			/* The document is a generated HTML document that links in a
			   stylesheet from 'opera:style/image.css'. */
			reject_opera = FALSE;

#ifdef MEDIA_HTML_SUPPORT
		if (g_media_module.IsMediaPlayableUrl(doc->GetURL()) &&
			inline_url.GetAttribute(URL::KPath).Compare("style/media.css") == 0)
			reject_opera = FALSE;
#endif // MEDIA_HTML_SUPPORT

		if (IsAboutBlankURL(inline_url))
			reject_opera = FALSE;

		if (reject_opera)
			return OpStatus::OK;
	}

#ifdef GADGET_SUPPORT
	Window* window = doc->GetWindow();
	if (inline_url_type == URL_WIDGET)
	{
		RETURN_IF_ERROR(g_secman_instance->CheckHasGadgetManagerAccess(source, allowed));
		if (allowed)
			return OpStatus::OK;
	}

	if (window->GetGadget())
	{
		RETURN_IF_ERROR(OpSecurityManager::CheckSecurity(OpSecurityManager::DOC_INLINE_LOAD, OpSecurityContext(window->GetGadget(), window), OpSecurityContext(inline_url, target.inline_type, from_user_css), allowed, state));
		if (!allowed)
			return OpStatus::OK;
	}
	// Don't allow widgets to be loaded as inlines, except from other widgets.  See bug 305609
	else if (inline_url_type == URL_WIDGET)
		return OpStatus::OK;
#endif // GADGET_SUPPORT

	allowed = FALSE;

#ifdef _PLUGIN_SUPPORT_
	if (target.GetInlineType() == EMBED_INLINE)
	{
		if (!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::PluginsEnabled, doc->GetHostName()) ||
		    (doc->GetWindow()->IsSuppressWindow() && doc->GetURL().GetAttribute(URL::KSuppressScriptAndEmbeds, TRUE) != MIME_Local_ScriptEmbed_Restrictions) ||
			doc->GetWindow()->GetForcePluginsDisabled())
		{
			return OpStatus::OK;
		}
	}
#endif // _PLUGIN_SUPPORT_

#ifdef WEBSERVER_SUPPORT
	if (inline_url.GetAttribute(URL::KIsUniteServiceAdminURL))
	{
		OpSecurityContext source_context(doc->GetSecurityContext()), target_context(inline_url);
		RAISE_AND_RETURN_IF_ERROR(OpSecurityManager::CheckSecurity(OpSecurityManager::UNITE_STANDARD, source_context, target_context, allowed));
		return OpStatus::OK;
	}
#endif // WEBSERVER_SUPPORT

	ServerName *source_sn = source.GetURL().GetServerName();
	ServerName *inline_sn = inline_url.GetServerName();
	if (inline_sn != source_sn &&
		source_sn != NULL &&
		inline_sn != NULL &&
		inline_url.Type() != URL_FILE &&
		source.GetURL().Type() != URL_FILE &&
		source_sn->GetNetType() != NETTYPE_UNDETERMINED &&
		inline_sn->GetNetType() != NETTYPE_UNDETERMINED &&
		inline_sn->GetNetType() < source_sn->GetNetType() &&
		!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AllowCrossNetworkNavigation, source_sn))
		// allowed = FALSE
		return OpStatus::OK;

	if (inline_sn != NULL &&
		inline_sn->GetCrossNetwork() &&
		!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AllowCrossNetworkNavigation, source_sn))
		// allowed = FALSE
		return OpStatus::OK;

	allowed = TRUE;
	return OpStatus::OK;
}
