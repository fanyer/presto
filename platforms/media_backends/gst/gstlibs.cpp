/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2009-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch_system_includes.h"

#ifdef MEDIA_BACKEND_GSTREAMER

#ifdef MEDIA_BACKEND_GSTREAMER_USE_OPDLL

#include "platforms/media_backends/gst/gstlibs.h"
#include "modules/pi/OpDLL.h"
#include "modules/util/opfile/opfile.h"

#ifdef MEDIA_BACKEND_GSTREAMER_BUNDLE_LIBS
# ifdef G_OS_WIN32
// Single GStreamer DLL
#  define GSTREAMER_DLL "gstreamer.dll"
#  define g_dll_LibGLib (g_opera->media_backends_module.dll_LibGStreamer)
#  define g_dll_LibGObject (g_opera->media_backends_module.dll_LibGStreamer)
#  define g_dll_LibGModule (g_opera->media_backends_module.dll_LibGStreamer)
#  define g_dll_LibGStreamer (g_opera->media_backends_module.dll_LibGStreamer)
#  define g_dll_LibGstBase (g_opera->media_backends_module.dll_LibGStreamer)
#  define g_dll_LibGstVideo (g_opera->media_backends_module.dll_LibGStreamer)
#  define g_dll_LibGstRiff (g_opera->media_backends_module.dll_LibGStreamer)
# else
#  error "Unsupported platform"
# endif
#else // !MEDIA_BACKEND_GSTREAMER_BUNDLE_LIBS
// Use system GStreamer (UNIX)
# define g_dll_LibGLib (g_opera->media_backends_module.dll_LibGLib)
# define g_dll_LibGObject (g_opera->media_backends_module.dll_LibGObject)
# define g_dll_LibGModule (g_opera->media_backends_module.dll_LibGModule)
# define g_dll_LibGStreamer (g_opera->media_backends_module.dll_LibGStreamer)
# define g_dll_LibGstBase (g_opera->media_backends_module.dll_LibGstBase)
# define g_dll_LibGstVideo (g_opera->media_backends_module.dll_LibGstVideo)
# define g_dll_LibGstRiff (g_opera->media_backends_module.dll_LibGstRiff)
#endif // MEDIA_BACKEND_GSTREAMER_BUNDLE_LIBS

static OP_STATUS LoadDLL(const uni_char* dll_name, OpDLL*& dll)
{
	OpAutoPtr<OpDLL> dll_safe;
	{
		OpDLL* tmp;
		RETURN_IF_ERROR(OpDLL::Create(&tmp));
		dll_safe.reset(tmp);
	}
	RETURN_IF_ERROR(dll_safe->Load(dll_name));
	dll = dll_safe.release();
	return OpStatus::OK;
}

OP_STATUS GstLibsInit()
{
#ifdef MEDIA_BACKEND_GSTREAMER_BUNDLE_LIBS
	OP_ASSERT(!g_dll_LibGStreamer);
	OpFile dllfile;
	RETURN_IF_ERROR(dllfile.Construct(UNI_L(GSTREAMER_DLL), OPFILE_GSTREAMER_FOLDER));
	RETURN_IF_ERROR(LoadDLL(dllfile.GetFullPath(), g_dll_LibGStreamer));
#else // !MEDIA_BACKEND_GSTREAMER_BUNDLE_LIBS
	OP_ASSERT(!g_dll_LibGLib && !g_dll_LibGObject && !g_dll_LibGModule &&
			  !g_dll_LibGStreamer && !g_dll_LibGstBase &&
			  !g_dll_LibGstVideo && !g_dll_LibGstRiff);
	RETURN_IF_ERROR(LoadDLL(UNI_L("libglib-2.0.so.0"), g_dll_LibGLib));
	RETURN_IF_ERROR(LoadDLL(UNI_L("libgobject-2.0.so.0"), g_dll_LibGObject));
	RETURN_IF_ERROR(LoadDLL(UNI_L("libgmodule-2.0.so.0"), g_dll_LibGModule));
	RETURN_IF_ERROR(LoadDLL(UNI_L("libgstreamer-0.10.so.0"), g_dll_LibGStreamer));
	RETURN_IF_ERROR(LoadDLL(UNI_L("libgstbase-0.10.so.0"), g_dll_LibGstBase));
	RETURN_IF_ERROR(LoadDLL(UNI_L("libgstvideo-0.10.so.0"), g_dll_LibGstVideo));
	RETURN_IF_ERROR(LoadDLL(UNI_L("libgstriff-0.10.so.0"), g_dll_LibGstRiff));
#endif // MEDIA_BACKEND_GSTREAMER_BUNDLE_LIBS

	if (!g_LibGLib.Load(g_dll_LibGLib) ||
		!g_LibGObject.Load(g_dll_LibGObject) ||
		!g_LibGModule.Load(g_dll_LibGModule) ||
		!g_LibGStreamer.Load(g_dll_LibGStreamer) ||
		!g_LibGstBase.Load(g_dll_LibGstBase) ||
		!g_LibGstVideo.Load(g_dll_LibGstVideo) ||
		!g_LibGstRiff.Load(g_dll_LibGstRiff))
		return OpStatus::ERR;

	return OpStatus::OK;
}

// WARNING: Do not edit the below manually, use scripts/gstlibs.py

// BEGIN GENERATED CODE


BOOL LibGLib::Load(OpDLL* dll)
{
#ifndef GST_DISABLE_GST_DEBUG
	sym_g_error_free = (g_error_free_t)dll->GetSymbolAddress("g_error_free");
#endif
	sym_g_free = (g_free_t)dll->GetSymbolAddress("g_free");
	sym_g_intern_static_string = (g_intern_static_string_t)dll->GetSymbolAddress("g_intern_static_string");
#ifndef GST_DISABLE_GST_DEBUG
	sym_g_log = (g_log_t)dll->GetSymbolAddress("g_log");
#endif
	sym_g_once_init_enter_impl = (g_once_init_enter_impl_t)dll->GetSymbolAddress("g_once_init_enter_impl");
	sym_g_once_init_leave = (g_once_init_leave_t)dll->GetSymbolAddress("g_once_init_leave");
	sym_g_queue_free = (g_queue_free_t)dll->GetSymbolAddress("g_queue_free");
	sym_g_queue_get_length = (g_queue_get_length_t)dll->GetSymbolAddress("g_queue_get_length");
	sym_g_queue_new = (g_queue_new_t)dll->GetSymbolAddress("g_queue_new");
	sym_g_queue_pop_head = (g_queue_pop_head_t)dll->GetSymbolAddress("g_queue_pop_head");
	sym_g_queue_push_tail = (g_queue_push_tail_t)dll->GetSymbolAddress("g_queue_push_tail");
	sym_g_realloc = (g_realloc_t)dll->GetSymbolAddress("g_realloc");
#ifndef GST_DISABLE_GST_DEBUG
	sym_g_return_if_fail_warning = (g_return_if_fail_warning_t)dll->GetSymbolAddress("g_return_if_fail_warning");
#endif
#ifndef GST_DISABLE_GST_DEBUG
	sym_g_set_print_handler = (g_set_print_handler_t)dll->GetSymbolAddress("g_set_print_handler");
#endif
#ifndef GST_DISABLE_GST_DEBUG
	sym_g_set_printerr_handler = (g_set_printerr_handler_t)dll->GetSymbolAddress("g_set_printerr_handler");
#endif
	sym_g_slist_delete_link = (g_slist_delete_link_t)dll->GetSymbolAddress("g_slist_delete_link");
	sym_g_slist_prepend = (g_slist_prepend_t)dll->GetSymbolAddress("g_slist_prepend");
#ifdef MEDIA_BACKEND_GSTREAMER_PULSESINK_WORKAROUND
	sym_g_static_mutex_get_mutex_impl = (g_static_mutex_get_mutex_impl_t)dll->GetSymbolAddress("g_static_mutex_get_mutex_impl");
#endif
	sym_g_str_has_prefix = (g_str_has_prefix_t)dll->GetSymbolAddress("g_str_has_prefix");
	sym_g_strdup_printf = (g_strdup_printf_t)dll->GetSymbolAddress("g_strdup_printf");
	sym_g_thread_create_full = (g_thread_create_full_t)dll->GetSymbolAddress("g_thread_create_full");
	sym_g_thread_functions_for_glib_use = (g_thread_functions_for_glib_use_t)dll->GetSymbolAddress("g_thread_functions_for_glib_use");
	sym_g_thread_join = (g_thread_join_t)dll->GetSymbolAddress("g_thread_join");
#ifdef MEDIA_BACKEND_GSTREAMER_PULSESINK_WORKAROUND
	sym_g_thread_use_default_impl = (g_thread_use_default_impl_t)dll->GetSymbolAddress("g_thread_use_default_impl");
#endif
	sym_g_threads_got_initialized = (g_threads_got_initialized_t)dll->GetSymbolAddress("g_threads_got_initialized");

	BOOL loaded =
#ifndef GST_DISABLE_GST_DEBUG
		sym_g_error_free != NULL &&
#endif
		sym_g_free != NULL &&
		sym_g_intern_static_string != NULL &&
#ifndef GST_DISABLE_GST_DEBUG
		sym_g_log != NULL &&
#endif
		sym_g_once_init_enter_impl != NULL &&
		sym_g_once_init_leave != NULL &&
		sym_g_queue_free != NULL &&
		sym_g_queue_get_length != NULL &&
		sym_g_queue_new != NULL &&
		sym_g_queue_pop_head != NULL &&
		sym_g_queue_push_tail != NULL &&
		sym_g_realloc != NULL &&
#ifndef GST_DISABLE_GST_DEBUG
		sym_g_return_if_fail_warning != NULL &&
#endif
#ifndef GST_DISABLE_GST_DEBUG
		sym_g_set_print_handler != NULL &&
#endif
#ifndef GST_DISABLE_GST_DEBUG
		sym_g_set_printerr_handler != NULL &&
#endif
		sym_g_slist_delete_link != NULL &&
		sym_g_slist_prepend != NULL &&
#ifdef MEDIA_BACKEND_GSTREAMER_PULSESINK_WORKAROUND
		sym_g_static_mutex_get_mutex_impl != NULL &&
#endif
		sym_g_str_has_prefix != NULL &&
		sym_g_strdup_printf != NULL &&
		sym_g_thread_create_full != NULL &&
		sym_g_thread_functions_for_glib_use != NULL &&
		sym_g_thread_join != NULL &&
#ifdef MEDIA_BACKEND_GSTREAMER_PULSESINK_WORKAROUND
		sym_g_thread_use_default_impl != NULL &&
#endif
		sym_g_threads_got_initialized != NULL &&
		TRUE;

	return loaded;
}

LibGLib g_LibGLib;



BOOL LibGObject::Load(OpDLL* dll)
{
	sym_g_object_class_install_property = (g_object_class_install_property_t)dll->GetSymbolAddress("g_object_class_install_property");
	sym_g_object_get = (g_object_get_t)dll->GetSymbolAddress("g_object_get");
	sym_g_object_set = (g_object_set_t)dll->GetSymbolAddress("g_object_set");
	sym_g_signal_connect_data = (g_signal_connect_data_t)dll->GetSymbolAddress("g_signal_connect_data");
	sym_g_type_check_class_cast = (g_type_check_class_cast_t)dll->GetSymbolAddress("g_type_check_class_cast");
	sym_g_type_check_instance_cast = (g_type_check_instance_cast_t)dll->GetSymbolAddress("g_type_check_instance_cast");
	sym_g_type_check_instance_is_a = (g_type_check_instance_is_a_t)dll->GetSymbolAddress("g_type_check_instance_is_a");
	sym_g_type_class_peek_parent = (g_type_class_peek_parent_t)dll->GetSymbolAddress("g_type_class_peek_parent");
	sym_g_type_class_ref = (g_type_class_ref_t)dll->GetSymbolAddress("g_type_class_ref");
	sym_g_type_name = (g_type_name_t)dll->GetSymbolAddress("g_type_name");
	sym_g_type_register_static = (g_type_register_static_t)dll->GetSymbolAddress("g_type_register_static");
	sym_g_value_get_double = (g_value_get_double_t)dll->GetSymbolAddress("g_value_get_double");
	sym_g_value_get_int = (g_value_get_int_t)dll->GetSymbolAddress("g_value_get_int");

	BOOL loaded =
		sym_g_object_class_install_property != NULL &&
		sym_g_object_get != NULL &&
		sym_g_object_set != NULL &&
		sym_g_signal_connect_data != NULL &&
		sym_g_type_check_class_cast != NULL &&
		sym_g_type_check_instance_cast != NULL &&
		sym_g_type_check_instance_is_a != NULL &&
		sym_g_type_class_peek_parent != NULL &&
		sym_g_type_class_ref != NULL &&
		sym_g_type_name != NULL &&
		sym_g_type_register_static != NULL &&
		sym_g_value_get_double != NULL &&
		sym_g_value_get_int != NULL &&
		TRUE;

	return loaded;
}

LibGObject g_LibGObject;



BOOL LibGStreamer::Load(OpDLL* dll)
{
#ifndef GST_DISABLE_GST_DEBUG
	sym___gst_debug_min = (__gst_debug_min_t)dll->GetSymbolAddress("__gst_debug_min");
#endif
#ifndef GST_DISABLE_GST_DEBUG
	sym__gst_debug_category_new = (_gst_debug_category_new_t)dll->GetSymbolAddress("_gst_debug_category_new");
#endif
#ifndef GST_DISABLE_GST_DEBUG
	sym__gst_debug_register_funcptr = (_gst_debug_register_funcptr_t)dll->GetSymbolAddress("_gst_debug_register_funcptr");
#endif
	sym_gst_bin_add = (gst_bin_add_t)dll->GetSymbolAddress("gst_bin_add");
	sym_gst_bin_get_by_name = (gst_bin_get_by_name_t)dll->GetSymbolAddress("gst_bin_get_by_name");
	sym_gst_bin_get_type = (gst_bin_get_type_t)dll->GetSymbolAddress("gst_bin_get_type");
	sym_gst_bin_new = (gst_bin_new_t)dll->GetSymbolAddress("gst_bin_new");
	sym_gst_bin_remove = (gst_bin_remove_t)dll->GetSymbolAddress("gst_bin_remove");
	sym_gst_buffer_get_type = (gst_buffer_get_type_t)dll->GetSymbolAddress("gst_buffer_get_type");
	sym_gst_buffer_new_and_alloc = (gst_buffer_new_and_alloc_t)dll->GetSymbolAddress("gst_buffer_new_and_alloc");
	sym_gst_buffer_set_caps = (gst_buffer_set_caps_t)dll->GetSymbolAddress("gst_buffer_set_caps");
	sym_gst_bus_create_watch = (gst_bus_create_watch_t)dll->GetSymbolAddress("gst_bus_create_watch");
	sym_gst_bus_have_pending = (gst_bus_have_pending_t)dll->GetSymbolAddress("gst_bus_have_pending");
	sym_gst_bus_new = (gst_bus_new_t)dll->GetSymbolAddress("gst_bus_new");
	sym_gst_bus_post = (gst_bus_post_t)dll->GetSymbolAddress("gst_bus_post");
	sym_gst_bus_set_sync_handler = (gst_bus_set_sync_handler_t)dll->GetSymbolAddress("gst_bus_set_sync_handler");
	sym_gst_bus_timed_pop = (gst_bus_timed_pop_t)dll->GetSymbolAddress("gst_bus_timed_pop");
	sym_gst_caps_copy = (gst_caps_copy_t)dll->GetSymbolAddress("gst_caps_copy");
	sym_gst_caps_from_string = (gst_caps_from_string_t)dll->GetSymbolAddress("gst_caps_from_string");
	sym_gst_caps_get_structure = (gst_caps_get_structure_t)dll->GetSymbolAddress("gst_caps_get_structure");
	sym_gst_caps_intersect = (gst_caps_intersect_t)dll->GetSymbolAddress("gst_caps_intersect");
	sym_gst_caps_is_any = (gst_caps_is_any_t)dll->GetSymbolAddress("gst_caps_is_any");
	sym_gst_caps_is_empty = (gst_caps_is_empty_t)dll->GetSymbolAddress("gst_caps_is_empty");
	sym_gst_caps_is_fixed = (gst_caps_is_fixed_t)dll->GetSymbolAddress("gst_caps_is_fixed");
	sym_gst_caps_new_simple = (gst_caps_new_simple_t)dll->GetSymbolAddress("gst_caps_new_simple");
	sym_gst_caps_unref = (gst_caps_unref_t)dll->GetSymbolAddress("gst_caps_unref");
#ifndef GST_DISABLE_GST_DEBUG
	sym_gst_debug_log = (gst_debug_log_t)dll->GetSymbolAddress("gst_debug_log");
#endif
	sym_gst_element_add_pad = (gst_element_add_pad_t)dll->GetSymbolAddress("gst_element_add_pad");
	sym_gst_element_class_add_pad_template = (gst_element_class_add_pad_template_t)dll->GetSymbolAddress("gst_element_class_add_pad_template");
	sym_gst_element_class_set_details = (gst_element_class_set_details_t)dll->GetSymbolAddress("gst_element_class_set_details");
	sym_gst_element_factory_get_klass = (gst_element_factory_get_klass_t)dll->GetSymbolAddress("gst_element_factory_get_klass");
	sym_gst_element_factory_get_longname = (gst_element_factory_get_longname_t)dll->GetSymbolAddress("gst_element_factory_get_longname");
	sym_gst_element_factory_get_static_pad_templates = (gst_element_factory_get_static_pad_templates_t)dll->GetSymbolAddress("gst_element_factory_get_static_pad_templates");
	sym_gst_element_factory_get_type = (gst_element_factory_get_type_t)dll->GetSymbolAddress("gst_element_factory_get_type");
	sym_gst_element_factory_make = (gst_element_factory_make_t)dll->GetSymbolAddress("gst_element_factory_make");
	sym_gst_element_get_state = (gst_element_get_state_t)dll->GetSymbolAddress("gst_element_get_state");
	sym_gst_element_get_static_pad = (gst_element_get_static_pad_t)dll->GetSymbolAddress("gst_element_get_static_pad");
	sym_gst_element_get_type = (gst_element_get_type_t)dll->GetSymbolAddress("gst_element_get_type");
	sym_gst_element_link = (gst_element_link_t)dll->GetSymbolAddress("gst_element_link");
	sym_gst_element_post_message = (gst_element_post_message_t)dll->GetSymbolAddress("gst_element_post_message");
	sym_gst_element_query_duration = (gst_element_query_duration_t)dll->GetSymbolAddress("gst_element_query_duration");
	sym_gst_element_query_position = (gst_element_query_position_t)dll->GetSymbolAddress("gst_element_query_position");
	sym_gst_element_register = (gst_element_register_t)dll->GetSymbolAddress("gst_element_register");
	sym_gst_element_seek = (gst_element_seek_t)dll->GetSymbolAddress("gst_element_seek");
	sym_gst_element_seek_simple = (gst_element_seek_simple_t)dll->GetSymbolAddress("gst_element_seek_simple");
	sym_gst_element_set_index = (gst_element_set_index_t)dll->GetSymbolAddress("gst_element_set_index");
	sym_gst_element_set_state = (gst_element_set_state_t)dll->GetSymbolAddress("gst_element_set_state");
	sym_gst_element_state_get_name = (gst_element_state_get_name_t)dll->GetSymbolAddress("gst_element_state_get_name");
	sym_gst_ghost_pad_new = (gst_ghost_pad_new_t)dll->GetSymbolAddress("gst_ghost_pad_new");
	sym_gst_index_entry_assoc_map = (gst_index_entry_assoc_map_t)dll->GetSymbolAddress("gst_index_entry_assoc_map");
	sym_gst_index_factory_make = (gst_index_factory_make_t)dll->GetSymbolAddress("gst_index_factory_make");
	sym_gst_index_get_assoc_entry = (gst_index_get_assoc_entry_t)dll->GetSymbolAddress("gst_index_get_assoc_entry");
	sym_gst_init_check = (gst_init_check_t)dll->GetSymbolAddress("gst_init_check");
	sym_gst_message_get_structure = (gst_message_get_structure_t)dll->GetSymbolAddress("gst_message_get_structure");
	sym_gst_message_get_type = (gst_message_get_type_t)dll->GetSymbolAddress("gst_message_get_type");
	sym_gst_message_new_application = (gst_message_new_application_t)dll->GetSymbolAddress("gst_message_new_application");
	sym_gst_message_new_duration = (gst_message_new_duration_t)dll->GetSymbolAddress("gst_message_new_duration");
	sym_gst_message_parse_duration = (gst_message_parse_duration_t)dll->GetSymbolAddress("gst_message_parse_duration");
	sym_gst_message_parse_error = (gst_message_parse_error_t)dll->GetSymbolAddress("gst_message_parse_error");
	sym_gst_message_parse_state_changed = (gst_message_parse_state_changed_t)dll->GetSymbolAddress("gst_message_parse_state_changed");
	sym_gst_message_parse_warning = (gst_message_parse_warning_t)dll->GetSymbolAddress("gst_message_parse_warning");
	sym_gst_message_type_get_name = (gst_message_type_get_name_t)dll->GetSymbolAddress("gst_message_type_get_name");
	sym_gst_mini_object_get_type = (gst_mini_object_get_type_t)dll->GetSymbolAddress("gst_mini_object_get_type");
	sym_gst_mini_object_new = (gst_mini_object_new_t)dll->GetSymbolAddress("gst_mini_object_new");
	sym_gst_mini_object_ref = (gst_mini_object_ref_t)dll->GetSymbolAddress("gst_mini_object_ref");
	sym_gst_mini_object_replace = (gst_mini_object_replace_t)dll->GetSymbolAddress("gst_mini_object_replace");
	sym_gst_mini_object_unref = (gst_mini_object_unref_t)dll->GetSymbolAddress("gst_mini_object_unref");
	sym_gst_object_get_type = (gst_object_get_type_t)dll->GetSymbolAddress("gst_object_get_type");
	sym_gst_object_ref = (gst_object_ref_t)dll->GetSymbolAddress("gst_object_ref");
	sym_gst_object_replace = (gst_object_replace_t)dll->GetSymbolAddress("gst_object_replace");
	sym_gst_object_unref = (gst_object_unref_t)dll->GetSymbolAddress("gst_object_unref");
	sym_gst_pad_get_caps = (gst_pad_get_caps_t)dll->GetSymbolAddress("gst_pad_get_caps");
	sym_gst_pad_get_pad_template_caps = (gst_pad_get_pad_template_caps_t)dll->GetSymbolAddress("gst_pad_get_pad_template_caps");
	sym_gst_pad_is_linked = (gst_pad_is_linked_t)dll->GetSymbolAddress("gst_pad_is_linked");
	sym_gst_pad_link = (gst_pad_link_t)dll->GetSymbolAddress("gst_pad_link");
	sym_gst_param_spec_mini_object = (gst_param_spec_mini_object_t)dll->GetSymbolAddress("gst_param_spec_mini_object");
	sym_gst_pipeline_get_bus = (gst_pipeline_get_bus_t)dll->GetSymbolAddress("gst_pipeline_get_bus");
	sym_gst_pipeline_new = (gst_pipeline_new_t)dll->GetSymbolAddress("gst_pipeline_new");
	sym_gst_plugin_feature_list_free = (gst_plugin_feature_list_free_t)dll->GetSymbolAddress("gst_plugin_feature_list_free");
	sym_gst_plugin_register_static = (gst_plugin_register_static_t)dll->GetSymbolAddress("gst_plugin_register_static");
	sym_gst_registry_feature_filter = (gst_registry_feature_filter_t)dll->GetSymbolAddress("gst_registry_feature_filter");
	sym_gst_registry_get_default = (gst_registry_get_default_t)dll->GetSymbolAddress("gst_registry_get_default");
	sym_gst_registry_scan_path = (gst_registry_scan_path_t)dll->GetSymbolAddress("gst_registry_scan_path");
	sym_gst_static_pad_template_get = (gst_static_pad_template_get_t)dll->GetSymbolAddress("gst_static_pad_template_get");
	sym_gst_static_pad_template_get_caps = (gst_static_pad_template_get_caps_t)dll->GetSymbolAddress("gst_static_pad_template_get_caps");
	sym_gst_structure_get_fraction = (gst_structure_get_fraction_t)dll->GetSymbolAddress("gst_structure_get_fraction");
	sym_gst_structure_get_int = (gst_structure_get_int_t)dll->GetSymbolAddress("gst_structure_get_int");
	sym_gst_structure_get_name = (gst_structure_get_name_t)dll->GetSymbolAddress("gst_structure_get_name");
	sym_gst_structure_get_value = (gst_structure_get_value_t)dll->GetSymbolAddress("gst_structure_get_value");
	sym_gst_structure_has_name = (gst_structure_has_name_t)dll->GetSymbolAddress("gst_structure_has_name");
	sym_gst_structure_new = (gst_structure_new_t)dll->GetSymbolAddress("gst_structure_new");
	sym_gst_type_register_static_full = (gst_type_register_static_full_t)dll->GetSymbolAddress("gst_type_register_static_full");
	sym_gst_value_take_mini_object = (gst_value_take_mini_object_t)dll->GetSymbolAddress("gst_value_take_mini_object");

	BOOL loaded =
#ifndef GST_DISABLE_GST_DEBUG
		sym___gst_debug_min != NULL &&
#endif
#ifndef GST_DISABLE_GST_DEBUG
		sym__gst_debug_category_new != NULL &&
#endif
#ifndef GST_DISABLE_GST_DEBUG
		sym__gst_debug_register_funcptr != NULL &&
#endif
		sym_gst_bin_add != NULL &&
		sym_gst_bin_get_by_name != NULL &&
		sym_gst_bin_get_type != NULL &&
		sym_gst_bin_new != NULL &&
		sym_gst_bin_remove != NULL &&
		sym_gst_buffer_get_type != NULL &&
		sym_gst_buffer_new_and_alloc != NULL &&
		sym_gst_buffer_set_caps != NULL &&
		sym_gst_bus_create_watch != NULL &&
		sym_gst_bus_have_pending != NULL &&
		sym_gst_bus_new != NULL &&
		sym_gst_bus_post != NULL &&
		sym_gst_bus_set_sync_handler != NULL &&
		sym_gst_bus_timed_pop != NULL &&
		sym_gst_caps_copy != NULL &&
		sym_gst_caps_from_string != NULL &&
		sym_gst_caps_get_structure != NULL &&
		sym_gst_caps_intersect != NULL &&
		sym_gst_caps_is_any != NULL &&
		sym_gst_caps_is_empty != NULL &&
		sym_gst_caps_is_fixed != NULL &&
		sym_gst_caps_new_simple != NULL &&
		sym_gst_caps_unref != NULL &&
#ifndef GST_DISABLE_GST_DEBUG
		sym_gst_debug_log != NULL &&
#endif
		sym_gst_element_add_pad != NULL &&
		sym_gst_element_class_add_pad_template != NULL &&
		sym_gst_element_class_set_details != NULL &&
		sym_gst_element_factory_get_klass != NULL &&
		sym_gst_element_factory_get_longname != NULL &&
		sym_gst_element_factory_get_static_pad_templates != NULL &&
		sym_gst_element_factory_get_type != NULL &&
		sym_gst_element_factory_make != NULL &&
		sym_gst_element_get_state != NULL &&
		sym_gst_element_get_static_pad != NULL &&
		sym_gst_element_get_type != NULL &&
		sym_gst_element_link != NULL &&
		sym_gst_element_post_message != NULL &&
		sym_gst_element_query_duration != NULL &&
		sym_gst_element_query_position != NULL &&
		sym_gst_element_register != NULL &&
		sym_gst_element_seek != NULL &&
		sym_gst_element_seek_simple != NULL &&
		sym_gst_element_set_index != NULL &&
		sym_gst_element_set_state != NULL &&
		sym_gst_element_state_get_name != NULL &&
		sym_gst_ghost_pad_new != NULL &&
		sym_gst_index_entry_assoc_map != NULL &&
		sym_gst_index_factory_make != NULL &&
		sym_gst_index_get_assoc_entry != NULL &&
		sym_gst_init_check != NULL &&
		sym_gst_message_get_structure != NULL &&
		sym_gst_message_get_type != NULL &&
		sym_gst_message_new_application != NULL &&
		sym_gst_message_new_duration != NULL &&
		sym_gst_message_parse_duration != NULL &&
		sym_gst_message_parse_error != NULL &&
		sym_gst_message_parse_state_changed != NULL &&
		sym_gst_message_parse_warning != NULL &&
		sym_gst_message_type_get_name != NULL &&
		sym_gst_mini_object_get_type != NULL &&
		sym_gst_mini_object_new != NULL &&
		sym_gst_mini_object_ref != NULL &&
		sym_gst_mini_object_replace != NULL &&
		sym_gst_mini_object_unref != NULL &&
		sym_gst_object_get_type != NULL &&
		sym_gst_object_ref != NULL &&
		sym_gst_object_replace != NULL &&
		sym_gst_object_unref != NULL &&
		sym_gst_pad_get_caps != NULL &&
		sym_gst_pad_get_pad_template_caps != NULL &&
		sym_gst_pad_is_linked != NULL &&
		sym_gst_pad_link != NULL &&
		sym_gst_param_spec_mini_object != NULL &&
		sym_gst_pipeline_get_bus != NULL &&
		sym_gst_pipeline_new != NULL &&
		sym_gst_plugin_feature_list_free != NULL &&
		sym_gst_plugin_register_static != NULL &&
		sym_gst_registry_feature_filter != NULL &&
		sym_gst_registry_get_default != NULL &&
		sym_gst_registry_scan_path != NULL &&
		sym_gst_static_pad_template_get != NULL &&
		sym_gst_static_pad_template_get_caps != NULL &&
		sym_gst_structure_get_fraction != NULL &&
		sym_gst_structure_get_int != NULL &&
		sym_gst_structure_get_name != NULL &&
		sym_gst_structure_get_value != NULL &&
		sym_gst_structure_has_name != NULL &&
		sym_gst_structure_new != NULL &&
		sym_gst_type_register_static_full != NULL &&
		sym_gst_value_take_mini_object != NULL &&
		TRUE;

	return loaded;
}

LibGStreamer g_LibGStreamer;



BOOL LibGstBase::Load(OpDLL* dll)
{
	sym_gst_base_sink_get_type = (gst_base_sink_get_type_t)dll->GetSymbolAddress("gst_base_sink_get_type");
	sym_gst_base_src_get_type = (gst_base_src_get_type_t)dll->GetSymbolAddress("gst_base_src_get_type");

	BOOL loaded =
		sym_gst_base_sink_get_type != NULL &&
		sym_gst_base_src_get_type != NULL &&
		TRUE;

	return loaded;
}

LibGstBase g_LibGstBase;



BOOL LibGstVideo::Load(OpDLL* dll)
{
	sym_gst_video_sink_get_type = (gst_video_sink_get_type_t)dll->GetSymbolAddress("gst_video_sink_get_type");

	BOOL loaded =
		sym_gst_video_sink_get_type != NULL &&
		TRUE;

	return loaded;
}

LibGstVideo g_LibGstVideo;



BOOL LibGstRiff::Load(OpDLL* dll)
{
	sym_gst_riff_create_audio_caps = (gst_riff_create_audio_caps_t)dll->GetSymbolAddress("gst_riff_create_audio_caps");
	sym_gst_riff_init = (gst_riff_init_t)dll->GetSymbolAddress("gst_riff_init");

	BOOL loaded =
		sym_gst_riff_create_audio_caps != NULL &&
		sym_gst_riff_init != NULL &&
		TRUE;

	return loaded;
}

LibGstRiff g_LibGstRiff;



BOOL LibGModule::Load(OpDLL* dll)
{
	sym_g_module_open = (g_module_open_t)dll->GetSymbolAddress("g_module_open");
	sym_g_module_close = (g_module_close_t)dll->GetSymbolAddress("g_module_close");
	sym_g_module_symbol = (g_module_symbol_t)dll->GetSymbolAddress("g_module_symbol");
	sym_g_module_make_resident = (g_module_make_resident_t)dll->GetSymbolAddress("g_module_make_resident");

	BOOL loaded =
		sym_g_module_open != NULL &&
		sym_g_module_close != NULL &&
		sym_g_module_symbol != NULL &&
		sym_g_module_make_resident != NULL &&
		TRUE;

	return loaded;
}

LibGModule g_LibGModule;


// END GENERATED CODE

#endif // MEDIA_BACKEND_GSTREAMER_USE_OPDLL

#endif // MEDIA_BACKEND_GSTREAMER
