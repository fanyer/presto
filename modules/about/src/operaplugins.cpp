/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#if defined _PLUGIN_SUPPORT_ && !defined NO_URL_OPERA
#include "modules/about/operaplugins.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/url/url2.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/viewers/viewers.h"
#include "modules/util/adt/opvector.h"
#include "modules/util/htmlify.h"

OP_STATUS OperaPlugins::GenerateData()
{
#ifdef _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenDocument(Str::SI_PLUGIN_LIST_TEXT, PrefsCollectionFiles::StylePluginsFile));
#else
	RETURN_IF_ERROR(OpenDocument(Str::SI_PLUGIN_LIST_TEXT));
#endif
	OpString disabled_plugin_string;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_IDSTR_PLUGIN_DISABLED, disabled_plugin_string));

	BOOL global_state = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::PluginsEnabled) != 0;
	OpAutoVector<OpString> plugin_path_list;
	OpVector<PluginViewer> filtered_viewer_list;

	// Get strings
	OpString enable, disable, details, description, refresh, global_enable;
	RETURN_IF_ERROR(JSSafeLocaleString(&enable, Str::S_LITERAL_ENABLE));
	RETURN_IF_ERROR(JSSafeLocaleString(&disable, Str::S_LITERAL_DISABLE));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_PLUGIN_LIST_DETAILS, details));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_PLUGIN_LIST_PLUGIN_DESC, description));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_PLUGIN_REFRESH, refresh));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDM_PLUGINS_TOGGLE, global_enable));
#ifdef ABOUT_PLUGINS_SHOW_ARCHITECTURE
	OpString arch_label, arch_native, arch_non_native;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_PLUGIN_LIST_ARCHITECTURE, arch_label));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_PLUGIN_LIST_ARCHITECTURE_NATIVE, arch_native));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_PLUGIN_LIST_ARCHITECTURE_NON_NATIVE, arch_non_native));
#endif // ABOUT_PLUGINS_SHOW_ARCHITECTURE

	OpString js_code;
	PluginViewers *plugin_viewer_list = g_plugin_viewers;	// Cache global
	const bool detecting = plugin_viewer_list->IsDetecting();
	// TODO CORE-40424: should either consolidate non-NS4P_COMPONENT_PLUGINS and
	// NS4P_COMPONENT_PLUGINS code so that are handled similarly or make a clear
	// separation between them using ifdefery. See http://critic-dev.oslo.osa/showcomment?chain=64850.
	if (detecting)
	{
		RETURN_IF_ERROR(js_code.Set(" <script type=\"text/javascript\">\n"
		                            "   window.onload = function()\n"
		                            "   {\n"
		                            "     setTimeout(\"window.location.reload()\", 3000);\n"
		                            "   };\n"
		                            " </script>\n"));
	}
	else
	{
		// Force sync detection if async did not finish already. Only relevant
		// for the non-NS4P_COMPONENT_PLUGINS code which never handles the
		// if-clause above.
		// TODO CORE-40424: This can be handled in a same way as the "new" code
		// given proper implementation of IsDetecting().
		plugin_viewer_list->MakeSurePluginsAreDetected();

		UINT numplugins = plugin_viewer_list->GetPluginViewerCount();

		RETURN_IF_ERROR(js_code.Set(" <script type=\"text/javascript\">\n  var plugin_list = ["));

		// First loop over plug-ins to create filtered list and javascript array for use on page
		for (UINT i = 0; i < numplugins; ++ i)
		{
			// Get the numbered plug-in
			PluginViewer *candidate_viewer = plugin_viewer_list->GetPluginViewer(i);
			const uni_char *plugin_path = candidate_viewer ? candidate_viewer->GetPath() : NULL;
			if (!plugin_path)
				continue;

			// Check for duplicate registrations
			BOOL is_duplicate = FALSE;
			UINT pathlistlen = plugin_path_list.GetCount();
			if (pathlistlen)
			{
				for (UINT j = 0; !is_duplicate && j < pathlistlen; ++j)
				{
					is_duplicate = !plugin_path_list.Get(j)->Compare(plugin_path);
				}
			}
			if (is_duplicate)
				continue;

			RETURN_IF_ERROR(filtered_viewer_list.Add(candidate_viewer));
			OpString escaped_path;
			RETURN_IF_ERROR(escaped_path.Set(candidate_viewer->GetPath()));
			RETURN_IF_ERROR(escaped_path.ReplaceAll(UNI_L("\\"), UNI_L("\\\\")));
			RETURN_IF_ERROR(escaped_path.ReplaceAll(UNI_L("'"), UNI_L("\\'")));
			RETURN_IF_ERROR(js_code.AppendFormat(UNI_L("\n  {enabled:%i, path:'%s'},"),
			                candidate_viewer->IsEnabled(),
			                escaped_path.CStr()));

			// Remember this entry
			OpString *s = OP_NEW(OpString, ());
			RETURN_OOM_IF_NULL(s);
			OP_STATUS st = s->Set(plugin_path);
			if (OpStatus::IsError(st))
			{
				OP_DELETE(s);
				return st;
			}
			plugin_path_list.Add(s);
		}

		RETURN_IF_ERROR(js_code.Append(
			"  ];\n\n"
			"  function togglePlugin(id, link)\n"
			"  {\n"
			"   var plugin = plugin_list[id], ret;\n"
			"   if (plugin && (ret = opera.togglePlugin(plugin.path, !plugin.enabled)) != -1)\n"
			"   {\n"
			"    plugin.enabled = ret;\n"
			"    document.getElementById('plug_'+id).className = ret ? '' : 'disabled';\n"));
		RETURN_IF_ERROR(js_code.AppendFormat(
			UNI_L("    link.textContent = ret ? \"%s\" : \"%s\";\n"), disable.CStr(), enable.CStr()));
		RETURN_IF_ERROR(js_code.Append(
			"   }\n"
			"   return false;\n"
			"  }\n\n"
			"  function toggleGlobal(enabled)\n"
			"  {\n"
			"   opera.setPreference('Extensions', 'Plugins', enabled ? '1' : '0');\n"
			"   document.body.id = enabled ? 'plugins-enabled' : 'plugins-disabled';\n"
			"  }\n\n"
			"  function toggleAllDetails(expand)\n"
			"  {\n"
			"   var names = document.getElementsByTagName('legend');\n"
			"   for (var i=0; i<names.length; i++)\n"
			"   {\n"
			"    toggleDetails(names[i], expand);\n"
			"   }\n"
			"  }\n\n"
			"  function toggleDetails(name, expand)\n"
			"  {\n"
			"   name.className = name.expanded=expand ? 'expanded' : '';\n"
			"   name.mime.style.height = name.mime.scrollHeight+'px';\n"
			"   name.mime.offsetHeight;\n"
			"   if (!expand)\n"
			"    name.mime.style.height = '0';\n"
			"  }\n\n"
			"  window.addEventListener('load', function()\n"
			"  {\n"
			"   // it's guaranteed to have same number of these elements\n"
			"   var names = document.getElementsByTagName('legend');\n"
			"   var mimes = document.getElementsByClassName('mime');\n\n"
			"   for (var i=0; i<names.length; i++)\n"
			"   {\n"
			"    /* after expanding, set height to auto so that\n"
			"       layout is kept fluid on resizing */\n"
			"    mimes[i].addEventListener('transitionEnd', function(e)\n"
			"    {\n"
			"     if (e.propertyName != 'height') return;\n"
			"     if (parseInt(this.style.height) != '0')\n"
			"      this.style.height = 'auto';\n"
			"    }, false);\n\n"
			"    // save reference to mime block in clicked element\n"
			"    names[i].mime = mimes[i];\n"
			"    names[i].addEventListener('click', function()\n"
			"    {\n"
			"     toggleDetails(this, this.expanded=!this.expanded);\n"
			"    }, false);\n\n"
			"    names[i].firstChild.plugin_id = i;\n"
			"    names[i].firstChild.addEventListener('click', function(e)\n"
			"    {\n"
			"     togglePlugin(this.plugin_id, this);\n"
			"     e.preventDefault();\n"
			"     e.cancelBubble = true;\n"
			"    }, false);\n"
			"   }\n"
			"  }, false);\n"
			" </script>\n"));
	}

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, js_code.CStr()));
	RETURN_IF_ERROR(OpenBody(Str::SI_PLUGIN_LIST_TEXT, global_state ? UNI_L("plugins-enabled") : UNI_L("plugins-disabled")));

	UINT32 filtered_count = filtered_viewer_list.GetCount();

	// Link for enabling/disabling the plugins globally
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal,
		UNI_L("<label><input type=\"checkbox\" onchange=\"toggleGlobal(this.checked)\"")));
	if (detecting)
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L(" disabled=\"disabled\"")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal,
		global_state ? UNI_L(" checked=\"checked\">") : UNI_L(">")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, global_enable.CStr()));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</label>\n")));

	RETURN_IF_ERROR(m_url.WriteDocumentDataUniSprintf(UNI_L("<p id=\"details\">")));

	// Link for toggling mime type details (details are initially hidden)
	if (filtered_count)
	{
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal,
			UNI_L("<label><input type=\"checkbox\" onchange=\"toggleAllDetails(this.checked)\">")));
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, details.CStr()));
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</label>\n")));
	}

	// Link for refreshing plugins directory to see if plugins were added/removed
	if (!detecting)
	{
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal,
			UNI_L("<a href=\"#\" onclick=\"navigator.plugins.refresh(true);return false;\">")));
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, refresh.CStr()));
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</a>")));
	}
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</p>\n")));

	// Output filtered plug-ins on page
	for (UINT i = 0; i < filtered_count; ++ i)
	{
		PluginViewer *candidate_viewer = filtered_viewer_list.Get(i);

		// Get name of plug-in and write it as header
		const uni_char *plugin_path = candidate_viewer->GetPath();
		const uni_char *plugin_name = candidate_viewer->GetProductName();
		const uni_char *plugin_description = candidate_viewer->GetDescription();
		const uni_char *plugin_version = candidate_viewer->GetVersion();
		BOOL disabled = !candidate_viewer->IsEnabled();

		// Start plug-in block
		RETURN_IF_ERROR(m_url.WriteDocumentDataUniSprintf(UNI_L("<fieldset id=\"plug_%i\"%s>\n<legend>"),
			i,
			disabled ? UNI_L(" class=\"disabled\"") : UNI_L("")));

		// Link for disabling/enabling plug-in
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<a href=\"#\">")));
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, disabled ? enable.CStr() : disable.CStr()));
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</a>\n")));

		// Name
		if (plugin_name)
			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, plugin_name));

		uni_char *tmp;

		// Version - shown if non-empty
		if (plugin_version && *plugin_version)
		{
			RETURN_OOM_IF_NULL(tmp = HTMLify_string(plugin_version));
			ANCHOR_ARRAY(uni_char, tmp);
			RETURN_IF_ERROR(m_url.WriteDocumentDataUniSprintf(UNI_L("<span> - %s</span>"), tmp));
			ANCHOR_ARRAY_DELETE(tmp);
		}
		else
		{
			plugin_version = UNI_L("");
		}

		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</legend>\n<div>\n")));

		/* Description - shown if non-empty and different from name and version.
		   It's not htmlified because it contains links. Instead function for
		   stripping html tags is used that leaves A tags only. */
		if (plugin_description
			&& uni_strlen(plugin_description)
			&& !uni_stri_eq(plugin_name, plugin_description)
			&& !uni_stri_eq(plugin_description, plugin_version))
		{
			OpString desc;
			RETURN_IF_ERROR(desc.Set(plugin_description));
			StripHTMLTags(desc.CStr(), desc.Length());
			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<div>")));
			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, description.CStr()));
			RETURN_IF_ERROR(m_url.WriteDocumentDataUniSprintf(UNI_L(": %s</div>\n"), desc.CStr()));
		}

#ifdef ABOUT_PLUGINS_SHOW_ARCHITECTURE
		// Plugin architecture. Different from COMPONENT_PLUGIN means non-native.
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<div>")));
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, arch_label.CStr()));
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L(": ")));

		if (candidate_viewer->GetComponentType() == COMPONENT_PLUGIN)
			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, arch_native.CStr()));
		else
			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, arch_non_native.CStr()));

		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</div>\n")));
#endif // ABOUT_PLUGINS_SHOW_ARCHITECTURE

		// Path to the plug-in
		RETURN_OOM_IF_NULL(tmp = HTMLify_string(plugin_path));
		ANCHOR_ARRAY(uni_char, tmp);
		RETURN_IF_ERROR(m_url.WriteDocumentDataUniSprintf(UNI_L("<address>%s</address>\n"), tmp));
		ANCHOR_ARRAY_DELETE(tmp);

		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<div class=\"mime\"><table>\n")));

		// Find MIME types associated with this plug-in
		Viewer* viewer;
		ChainedHashIterator* viewer_iter;
		g_viewers->CreateIterator(viewer_iter);
		if (viewer_iter != NULL)
		{
			OpAutoPtr<OpHashIterator> viewer_iter_ap(viewer_iter);
			while (viewer_iter && (viewer=g_viewers->GetNextViewer(viewer_iter)) != NULL)
			{
				// Iterate over all plug-ins associated with this MIME type
				unsigned int numpluginviewers = viewer->GetPluginViewerCount();
				for (unsigned int k = 0; k < numpluginviewers; ++ k)
				{
					const PluginViewer *p = viewer->GetPluginViewer(k);
					if (p && p->GetPath() && uni_strcmp(p->GetPath(), plugin_path) == 0)
					{
						// This is us.

						// MIME type
						const uni_char *mime_type = viewer->GetContentTypeString();
						if (mime_type)
						{
							int mimestart = 0;

							RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L(" <tr>\n  <td>")));

							// Split string at "|"s
							for(int i = mimestart = 0; mime_type[i]; ++ i)
							{
								if (mime_type[i] == '|')
								{
									RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, mime_type + mimestart, i - mimestart));
									RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<br>\n  ")));

									mimestart = i + 1;
								}
							}

							// Print the remainder of the string
							RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, &mime_type[mimestart]));
							RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</td>\n")));
						}

						// Description
						OpString description;
						RETURN_IF_ERROR(p->GetTypeDescription(mime_type, description));
						RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("  <td>")));
						RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, description));
						RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</td>\n")));

						// Extensions
						RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("  <td>")));

						const uni_char *extensions = viewer->GetExtensions();
						if (extensions)
						{
							int extstart = 0;

							// Split string at "|"s
							for (int i = 0; extensions[i]; ++ i)
							{
								if(extensions[i] == '|')
								{
									RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, extensions + extstart, extstart - i));
									RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<br>\n  ")));

									extstart = i + 1;
								}
							}

							// Print the remainder of the string
							RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, &extensions[extstart]));
						}
						RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</td>\n </tr>\n")));
					}
				}
			}
		}

		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</table></div>\n")));

		// Finish plug-in block
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</div>\n</fieldset>\n")));
	}

	if (detecting)
	{
		OpString detecting_str;
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_PLUGIN_DETECTING, detecting_str));
		RETURN_IF_ERROR(m_url.WriteDocumentDataUniSprintf(UNI_L("<p>%s</p>\n"), detecting_str.CStr()));
	}
	else if (!filtered_viewer_list.GetCount())
	{
		OpString none_found;
		RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_IDSTR_NONE_FOUND, none_found));
		RETURN_IF_ERROR(m_url.WriteDocumentDataUniSprintf(UNI_L("<p>%s</p>\n"), none_found.CStr()));
	}

	// Finish off
	return CloseDocument();
}
#endif
