/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMPRIVATENAMES_H
#define DOM_DOMPRIVATENAMES_H

/* The enum below is used for the ES_Runtime::GetPrivate and ES_Runtime::PutPrivate
   interface -- storing pointers in the ES_Object so that they are visible to the
   garbage collector, but unavailable to scripts.

   There is no significance to the names here -- this is just a pool of identifiers,
   all different.

   There is no significance to the numbers here, either, so the names should be kept
   alphabetical for easy modification and lookup by humans.

   And enumerators cost nothing, so keep this file clean of #ifdef-s, please. */

enum DOM_PrivateName
{
	DOM_PRIVATE_DOM = ES_PRIVATENAME_DOM,
	DOM_PRIVATE_all,
	DOM_PRIVATE_allFeeds,
	DOM_PRIVATE_anchors,
	DOM_PRIVATE_applicationCache,
	DOM_PRIVATE_applets,
	DOM_PRIVATE_areas,
	DOM_PRIVATE_attributes,
	DOM_PRIVATE_audioRecording,
	DOM_PRIVATE_blur,
	DOM_PRIVATE_cells,
	DOM_PRIVATE_childNodes,
	DOM_PRIVATE_children,
	DOM_PRIVATE_classList,
	DOM_PRIVATE_close,
	DOM_PRIVATE_collectionRoot,
	DOM_PRIVATE_cssRules,
	DOM_PRIVATE_currentObject,
	DOM_PRIVATE_currentStyle,
	DOM_PRIVATE_database,
	DOM_PRIVATE_dataset,
	DOM_PRIVATE_dbResultRows,
	DOM_PRIVATE_defaultValue,
	DOM_PRIVATE_doctype,
	DOM_PRIVATE_document,
	DOM_PRIVATE_documentNodes,
	DOM_PRIVATE_domchain,
	DOM_PRIVATE_element,
	DOM_PRIVATE_elements,
	DOM_PRIVATE_embeds,
	DOM_PRIVATE_entities,
	DOM_PRIVATE_entries,
	DOM_PRIVATE_event,
	DOM_PRIVATE_feed,
	DOM_PRIVATE_feedreaders,
	DOM_PRIVATE_feeds,
	DOM_PRIVATE_filelist,
	DOM_PRIVATE_filelist_string,
	DOM_PRIVATE_filter,
	DOM_PRIVATE_focus,
	DOM_PRIVATE_form,
	DOM_PRIVATE_forms,
	DOM_PRIVATE_frames,
	DOM_PRIVATE_geolocation,
	DOM_PRIVATE_globalObject,
	DOM_PRIVATE_history,
	DOM_PRIVATE_hwa,
	DOM_PRIVATE_images,
	DOM_PRIVATE_input,
	DOM_PRIVATE_internalValues,
	DOM_PRIVATE_io,
	DOM_PRIVATE_itemRef,
	DOM_PRIVATE_itemProp,
	DOM_PRIVATE_jilWidget,
	DOM_PRIVATE_labels_collection,
	DOM_PRIVATE_links,
	DOM_PRIVATE_localStorage,
	DOM_PRIVATE_location,
	DOM_PRIVATE_map,
	DOM_PRIVATE_media,
	DOM_PRIVATE_mimeTypes,
	DOM_PRIVATE_moveBy,
	DOM_PRIVATE_moveTo,
	DOM_PRIVATE_names,
	DOM_PRIVATE_nameInWindow,
	DOM_PRIVATE_navigator,
	DOM_PRIVATE_neutered,
	DOM_PRIVATE_nextRule,
	DOM_PRIVATE_notations,
	DOM_PRIVATE_opera,
	DOM_PRIVATE_options,
	DOM_PRIVATE_output,
	DOM_PRIVATE_ownerDocument,
	DOM_PRIVATE_parameterNames,
	DOM_PRIVATE_parentNode,
	DOM_PRIVATE_parser,
	DOM_PRIVATE_pixmap,
	DOM_PRIVATE_plugin_scriptable,
	DOM_PRIVATE_pluginObject,
	DOM_PRIVATE_pluginGlobalObjects,
	DOM_PRIVATE_plugins,
	DOM_PRIVATE_preferences,
	DOM_PRIVATE_propertiesMicrodata,
	DOM_PRIVATE_relatedTarget,
	DOM_PRIVATE_resizeBy,
	DOM_PRIVATE_resizeTo,
	DOM_PRIVATE_root,
	DOM_PRIVATE_rootNode,
	DOM_PRIVATE_rows,
	DOM_PRIVATE_screen,
	DOM_PRIVATE_scripts,
	DOM_PRIVATE_select,
	DOM_PRIVATE_selectedOptions,
	DOM_PRIVATE_selection,
	DOM_PRIVATE_sessionStorage,
	DOM_PRIVATE_sheet,
	DOM_PRIVATE_style,
	DOM_PRIVATE_styleSheets,
	DOM_PRIVATE_subcollections,
	DOM_PRIVATE_subscribedFeeds,
	DOM_PRIVATE_table,
	DOM_PRIVATE_tablerow,
	DOM_PRIVATE_target,
	DOM_PRIVATE_templateElements,
	DOM_PRIVATE_tbodies,
	DOM_PRIVATE_top,
	DOM_PRIVATE_upnp,
	DOM_PRIVATE_urlfilter,
	DOM_PRIVATE_validityState,
	DOM_PRIVATE_webserver,
	DOM_PRIVATE_widget,
	DOM_PRIVATE_widgets,
	DOM_PRIVATE_nearbydebuggers,

	DOM_PRIVATE_LASTNAME
};

#endif // DOM_DOMPRIVATENAMES_H
