/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2009-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef GSTLIBS_H
#define GSTLIBS_H

#ifdef MEDIA_BACKEND_GSTREAMER

#ifdef MEDIA_BACKEND_GSTREAMER_USE_OPDLL

class OpDLL;

// Disable GStreamer debugging for Windows, as it requires building
// debug DLLs and the release DLLs don't have the needed symbols.
#if defined(_MSC_VER)
# define GST_DISABLE_GST_DEBUG 1
#endif

// Visual Studio allows setting structure alignment for a project
// (usually 4 or 8). When sharing headers with another project as
// below, the structure alignment must match what was used to compile
// that, which is accomplished with pragma pack.

#ifdef _MSC_VER
# pragma pack(push,8)
#endif

#include "platforms/media_backends/gst/include/glib.h"
#include "platforms/media_backends/gst/include/gst/gst.h"
#include "platforms/media_backends/gst/include/gst/base/gstbasesrc.h"
#include "platforms/media_backends/gst/include/gst/video/gstvideosink.h"
#include "platforms/media_backends/gst/include/gst/video/video.h"
#include "platforms/media_backends/gst/include/gst/riff/riff-media.h"
#include "platforms/media_backends/gst/include/gst/riff/riff-read.h"

#ifdef _MSC_VER
# pragma pack(pop)

// On Windows, g_module_open is a macro which converts the incoming filename
// to UTF-8 before calling the real g_module_open. Since we have our own
// (manual) linkage to glib, and use macros to build a usable API against the
// the library, we have to undefine this macro here.
//
// Doing this is completely safe, because we will in fact assign the correct
// function pointer to g_module_open shortly.
# undef g_module_open

#endif

OP_STATUS GstLibsInit();

// WARNING: Do not edit the below manually, use scripts/gstlibs.py

// BEGIN GENERATED CODE


class LibGLib
{
public:
	BOOL Load(OpDLL* dll);

#ifndef GST_DISABLE_GST_DEBUG
	typedef void (*g_error_free_t) (GError * error);
#endif
	typedef void (*g_free_t) (gpointer mem);
	typedef G_CONST_RETURN gchar * (*g_intern_static_string_t) (const gchar * string);
#ifndef GST_DISABLE_GST_DEBUG
	typedef void (*g_log_t) (const gchar * log_domain, GLogLevelFlags log_level, const gchar * format, ...);
#endif
	typedef gboolean (*g_once_init_enter_impl_t) (volatile gsize * value_location);
	typedef void (*g_once_init_leave_t) (volatile gsize * value_location, gsize initialization_value);
	typedef void (*g_queue_free_t) (GQueue * queue);
	typedef guint (*g_queue_get_length_t) (GQueue * queue);
	typedef GQueue * (*g_queue_new_t) (void);
	typedef gpointer (*g_queue_pop_head_t) (GQueue * queue);
	typedef void (*g_queue_push_tail_t) (GQueue * queue, gpointer data);
	typedef gpointer (*g_realloc_t) (gpointer mem, gsize n_bytes);
#ifndef GST_DISABLE_GST_DEBUG
	typedef void (*g_return_if_fail_warning_t) (const char * log_domain, const char * pretty_function, const char * expression);
#endif
#ifndef GST_DISABLE_GST_DEBUG
	typedef GPrintFunc (*g_set_print_handler_t) (GPrintFunc func);
#endif
#ifndef GST_DISABLE_GST_DEBUG
	typedef GPrintFunc (*g_set_printerr_handler_t) (GPrintFunc func);
#endif
	typedef GSList * (*g_slist_delete_link_t) (GSList * list, GSList * link_);
	typedef GSList * (*g_slist_prepend_t) (GSList * list, gpointer data);
#ifdef MEDIA_BACKEND_GSTREAMER_PULSESINK_WORKAROUND
	typedef GMutex * (*g_static_mutex_get_mutex_impl_t) (GMutex * * mutex);
#endif
	typedef gboolean (*g_str_has_prefix_t) (const gchar * str, const gchar * prefix);
	typedef gchar * (*g_strdup_printf_t) (const gchar * format, ...);
	typedef GThread * (*g_thread_create_full_t) (GThreadFunc func, gpointer data, gulong stack_size, gboolean joinable, gboolean bound, GThreadPriority priority, GError * * error);
	typedef GThreadFunctions *g_thread_functions_for_glib_use_t;
	typedef gpointer (*g_thread_join_t) (GThread * thread);
#ifdef MEDIA_BACKEND_GSTREAMER_PULSESINK_WORKAROUND
	typedef gboolean *g_thread_use_default_impl_t;
#endif
	typedef gboolean *g_threads_got_initialized_t;

#ifndef GST_DISABLE_GST_DEBUG
	g_error_free_t sym_g_error_free;
#endif
	g_free_t sym_g_free;
	g_intern_static_string_t sym_g_intern_static_string;
#ifndef GST_DISABLE_GST_DEBUG
	g_log_t sym_g_log;
#endif
	g_once_init_enter_impl_t sym_g_once_init_enter_impl;
	g_once_init_leave_t sym_g_once_init_leave;
	g_queue_free_t sym_g_queue_free;
	g_queue_get_length_t sym_g_queue_get_length;
	g_queue_new_t sym_g_queue_new;
	g_queue_pop_head_t sym_g_queue_pop_head;
	g_queue_push_tail_t sym_g_queue_push_tail;
	g_realloc_t sym_g_realloc;
#ifndef GST_DISABLE_GST_DEBUG
	g_return_if_fail_warning_t sym_g_return_if_fail_warning;
#endif
#ifndef GST_DISABLE_GST_DEBUG
	g_set_print_handler_t sym_g_set_print_handler;
#endif
#ifndef GST_DISABLE_GST_DEBUG
	g_set_printerr_handler_t sym_g_set_printerr_handler;
#endif
	g_slist_delete_link_t sym_g_slist_delete_link;
	g_slist_prepend_t sym_g_slist_prepend;
#ifdef MEDIA_BACKEND_GSTREAMER_PULSESINK_WORKAROUND
	g_static_mutex_get_mutex_impl_t sym_g_static_mutex_get_mutex_impl;
#endif
	g_str_has_prefix_t sym_g_str_has_prefix;
	g_strdup_printf_t sym_g_strdup_printf;
	g_thread_create_full_t sym_g_thread_create_full;
	g_thread_functions_for_glib_use_t sym_g_thread_functions_for_glib_use;
	g_thread_join_t sym_g_thread_join;
#ifdef MEDIA_BACKEND_GSTREAMER_PULSESINK_WORKAROUND
	g_thread_use_default_impl_t sym_g_thread_use_default_impl;
#endif
	g_threads_got_initialized_t sym_g_threads_got_initialized;
};

extern LibGLib g_LibGLib;


#ifndef GST_DISABLE_GST_DEBUG
#define g_error_free (*g_LibGLib.sym_g_error_free)
#endif
#define g_free (*g_LibGLib.sym_g_free)
#define g_intern_static_string (*g_LibGLib.sym_g_intern_static_string)
#ifndef GST_DISABLE_GST_DEBUG
#define g_log (*g_LibGLib.sym_g_log)
#endif
#define g_once_init_enter_impl (*g_LibGLib.sym_g_once_init_enter_impl)
#define g_once_init_leave (*g_LibGLib.sym_g_once_init_leave)
#define g_queue_free (*g_LibGLib.sym_g_queue_free)
#define g_queue_get_length (*g_LibGLib.sym_g_queue_get_length)
#define g_queue_new (*g_LibGLib.sym_g_queue_new)
#define g_queue_pop_head (*g_LibGLib.sym_g_queue_pop_head)
#define g_queue_push_tail (*g_LibGLib.sym_g_queue_push_tail)
#define g_realloc (*g_LibGLib.sym_g_realloc)
#ifndef GST_DISABLE_GST_DEBUG
#define g_return_if_fail_warning (*g_LibGLib.sym_g_return_if_fail_warning)
#endif
#ifndef GST_DISABLE_GST_DEBUG
#define g_set_print_handler (*g_LibGLib.sym_g_set_print_handler)
#endif
#ifndef GST_DISABLE_GST_DEBUG
#define g_set_printerr_handler (*g_LibGLib.sym_g_set_printerr_handler)
#endif
#define g_slist_delete_link (*g_LibGLib.sym_g_slist_delete_link)
#define g_slist_prepend (*g_LibGLib.sym_g_slist_prepend)
#ifdef MEDIA_BACKEND_GSTREAMER_PULSESINK_WORKAROUND
#define g_static_mutex_get_mutex_impl (*g_LibGLib.sym_g_static_mutex_get_mutex_impl)
#endif
#define g_str_has_prefix (*g_LibGLib.sym_g_str_has_prefix)
#define g_strdup_printf (*g_LibGLib.sym_g_strdup_printf)
#define g_thread_create_full (*g_LibGLib.sym_g_thread_create_full)
#define g_thread_functions_for_glib_use (*g_LibGLib.sym_g_thread_functions_for_glib_use)
#define g_thread_join (*g_LibGLib.sym_g_thread_join)
#ifdef MEDIA_BACKEND_GSTREAMER_PULSESINK_WORKAROUND
#define g_thread_use_default_impl (*g_LibGLib.sym_g_thread_use_default_impl)
#endif
#define g_threads_got_initialized (*g_LibGLib.sym_g_threads_got_initialized)


class LibGObject
{
public:
	BOOL Load(OpDLL* dll);

	typedef void (*g_object_class_install_property_t) (GObjectClass * oclass, guint property_id, GParamSpec * pspec);
	typedef void (*g_object_get_t) (gpointer object, const gchar * first_property_name, ...);
	typedef void (*g_object_set_t) (gpointer object, const gchar * first_property_name, ...);
	typedef gulong (*g_signal_connect_data_t) (gpointer instance, const gchar * detailed_signal, GCallback c_handler, gpointer data, GClosureNotify destroy_data, GConnectFlags connect_flags);
	typedef GTypeClass * (*g_type_check_class_cast_t) (GTypeClass * g_class, GType is_a_type);
	typedef GTypeInstance * (*g_type_check_instance_cast_t) (GTypeInstance * instance, GType iface_type);
	typedef gboolean (*g_type_check_instance_is_a_t) (GTypeInstance * instance, GType iface_type);
	typedef gpointer (*g_type_class_peek_parent_t) (gpointer g_class);
	typedef gpointer (*g_type_class_ref_t) (GType type);
	typedef G_CONST_RETURN gchar * (*g_type_name_t) (GType type);
	typedef GType (*g_type_register_static_t) (GType parent_type, const gchar * type_name, const GTypeInfo * info, GTypeFlags flags);
	typedef gdouble (*g_value_get_double_t) (const GValue * value);
	typedef gint (*g_value_get_int_t) (const GValue * value);

	g_object_class_install_property_t sym_g_object_class_install_property;
	g_object_get_t sym_g_object_get;
	g_object_set_t sym_g_object_set;
	g_signal_connect_data_t sym_g_signal_connect_data;
	g_type_check_class_cast_t sym_g_type_check_class_cast;
	g_type_check_instance_cast_t sym_g_type_check_instance_cast;
	g_type_check_instance_is_a_t sym_g_type_check_instance_is_a;
	g_type_class_peek_parent_t sym_g_type_class_peek_parent;
	g_type_class_ref_t sym_g_type_class_ref;
	g_type_name_t sym_g_type_name;
	g_type_register_static_t sym_g_type_register_static;
	g_value_get_double_t sym_g_value_get_double;
	g_value_get_int_t sym_g_value_get_int;
};

extern LibGObject g_LibGObject;


#define g_object_class_install_property (*g_LibGObject.sym_g_object_class_install_property)
#define g_object_get (*g_LibGObject.sym_g_object_get)
#define g_object_set (*g_LibGObject.sym_g_object_set)
#define g_signal_connect_data (*g_LibGObject.sym_g_signal_connect_data)
#define g_type_check_class_cast (*g_LibGObject.sym_g_type_check_class_cast)
#define g_type_check_instance_cast (*g_LibGObject.sym_g_type_check_instance_cast)
#define g_type_check_instance_is_a (*g_LibGObject.sym_g_type_check_instance_is_a)
#define g_type_class_peek_parent (*g_LibGObject.sym_g_type_class_peek_parent)
#define g_type_class_ref (*g_LibGObject.sym_g_type_class_ref)
#define g_type_name (*g_LibGObject.sym_g_type_name)
#define g_type_register_static (*g_LibGObject.sym_g_type_register_static)
#define g_value_get_double (*g_LibGObject.sym_g_value_get_double)
#define g_value_get_int (*g_LibGObject.sym_g_value_get_int)


class LibGStreamer
{
public:
	BOOL Load(OpDLL* dll);

#ifndef GST_DISABLE_GST_DEBUG
	typedef GstDebugLevel *__gst_debug_min_t;
#endif
#ifndef GST_DISABLE_GST_DEBUG
	typedef GstDebugCategory * (*_gst_debug_category_new_t) (const gchar * name, guint color, const gchar * description);
#endif
#ifndef GST_DISABLE_GST_DEBUG
	typedef void (*_gst_debug_register_funcptr_t) (GstDebugFuncPtr func, const gchar * ptrname);
#endif
	typedef gboolean (*gst_bin_add_t) (GstBin * bin, GstElement * element);
	typedef GstElement * (*gst_bin_get_by_name_t) (GstBin * bin, const gchar * name);
	typedef GType (*gst_bin_get_type_t) (void);
	typedef GstElement * (*gst_bin_new_t) (const gchar * name);
	typedef gboolean (*gst_bin_remove_t) (GstBin * bin, GstElement * element);
	typedef GType (*gst_buffer_get_type_t) (void);
	typedef GstBuffer * (*gst_buffer_new_and_alloc_t) (guint size);
	typedef void (*gst_buffer_set_caps_t) (GstBuffer * buffer, GstCaps * caps);
	typedef GSource * (*gst_bus_create_watch_t) (GstBus * bus);
	typedef gboolean (*gst_bus_have_pending_t) (GstBus * bus);
	typedef GstBus * (*gst_bus_new_t) (void);
	typedef gboolean (*gst_bus_post_t) (GstBus * bus, GstMessage * message);
	typedef void (*gst_bus_set_sync_handler_t) (GstBus * bus, GstBusSyncHandler func, gpointer data);
	typedef GstMessage * (*gst_bus_timed_pop_t) (GstBus * bus, GstClockTime timeout);
	typedef GstCaps * (*gst_caps_copy_t) (const GstCaps * caps);
	typedef GstCaps * (*gst_caps_from_string_t) (const gchar * string);
	typedef GstStructure * (*gst_caps_get_structure_t) (const GstCaps * caps, guint index);
	typedef GstCaps * (*gst_caps_intersect_t) (const GstCaps * caps1, const GstCaps * caps2);
	typedef gboolean (*gst_caps_is_any_t) (const GstCaps * caps);
	typedef gboolean (*gst_caps_is_empty_t) (const GstCaps * caps);
	typedef gboolean (*gst_caps_is_fixed_t) (const GstCaps * caps);
	typedef GstCaps * (*gst_caps_new_simple_t) (const char * media_type, const char * fieldname, ...);
	typedef void (*gst_caps_unref_t) (GstCaps * caps);
#ifndef GST_DISABLE_GST_DEBUG
	typedef void (*gst_debug_log_t) (GstDebugCategory * category, GstDebugLevel level, const gchar * file, const gchar * function, gint line, GObject * object, const gchar * format, ...);
#endif
	typedef gboolean (*gst_element_add_pad_t) (GstElement * element, GstPad * pad);
	typedef void (*gst_element_class_add_pad_template_t) (GstElementClass * klass, GstPadTemplate * templ);
	typedef void (*gst_element_class_set_details_t) (GstElementClass * klass, const GstElementDetails * details);
	typedef G_CONST_RETURN gchar * (*gst_element_factory_get_klass_t) (GstElementFactory * factory);
	typedef G_CONST_RETURN gchar * (*gst_element_factory_get_longname_t) (GstElementFactory * factory);
	typedef G_CONST_RETURN GList * (*gst_element_factory_get_static_pad_templates_t) (GstElementFactory * factory);
	typedef GType (*gst_element_factory_get_type_t) (void);
	typedef GstElement * (*gst_element_factory_make_t) (const gchar * factoryname, const gchar * name);
	typedef GstStateChangeReturn (*gst_element_get_state_t) (GstElement * element, GstState * state, GstState * pending, GstClockTime timeout);
	typedef GstPad * (*gst_element_get_static_pad_t) (GstElement * element, const gchar * name);
	typedef GType (*gst_element_get_type_t) (void);
	typedef gboolean (*gst_element_link_t) (GstElement * src, GstElement * dest);
	typedef gboolean (*gst_element_post_message_t) (GstElement * element, GstMessage * message);
	typedef gboolean (*gst_element_query_duration_t) (GstElement * element, GstFormat * format, gint64 * duration);
	typedef gboolean (*gst_element_query_position_t) (GstElement * element, GstFormat * format, gint64 * cur);
	typedef gboolean (*gst_element_register_t) (GstPlugin * plugin, const gchar * name, guint rank, GType type);
	typedef gboolean (*gst_element_seek_t) (GstElement * element, gdouble rate, GstFormat format, GstSeekFlags flags, GstSeekType cur_type, gint64 cur, GstSeekType stop_type, gint64 stop);
	typedef gboolean (*gst_element_seek_simple_t) (GstElement * element, GstFormat format, GstSeekFlags seek_flags, gint64 seek_pos);
	typedef void (*gst_element_set_index_t) (GstElement * element, GstIndex * index);
	typedef GstStateChangeReturn (*gst_element_set_state_t) (GstElement * element, GstState state);
	typedef G_CONST_RETURN gchar * (*gst_element_state_get_name_t) (GstState state);
	typedef GstPad * (*gst_ghost_pad_new_t) (const gchar * name, GstPad * target);
	typedef gboolean (*gst_index_entry_assoc_map_t) (GstIndexEntry * entry, GstFormat format, gint64 * value);
	typedef GstIndex * (*gst_index_factory_make_t) (const gchar * name);
	typedef GstIndexEntry * (*gst_index_get_assoc_entry_t) (GstIndex * index, gint id, GstIndexLookupMethod method, GstAssocFlags flags, GstFormat format, gint64 value);
	typedef gboolean (*gst_init_check_t) (int * argc, char * * argv [], GError * * err);
	typedef const GstStructure * (*gst_message_get_structure_t) (GstMessage * message);
	typedef GType (*gst_message_get_type_t) (void);
	typedef GstMessage * (*gst_message_new_application_t) (GstObject * src, GstStructure * structure);
	typedef GstMessage * (*gst_message_new_duration_t) (GstObject * src, GstFormat format, gint64 duration);
	typedef void (*gst_message_parse_duration_t) (GstMessage * message, GstFormat * format, gint64 * duration);
	typedef void (*gst_message_parse_error_t) (GstMessage * message, GError * * gerror, gchar * * debug);
	typedef void (*gst_message_parse_state_changed_t) (GstMessage * message, GstState * oldstate, GstState * newstate, GstState * pending);
	typedef void (*gst_message_parse_warning_t) (GstMessage * message, GError * * gerror, gchar * * debug);
	typedef const gchar * (*gst_message_type_get_name_t) (GstMessageType type);
	typedef GType (*gst_mini_object_get_type_t) (void);
	typedef GstMiniObject * (*gst_mini_object_new_t) (GType type);
	typedef GstMiniObject * (*gst_mini_object_ref_t) (GstMiniObject * mini_object);
	typedef void (*gst_mini_object_replace_t) (GstMiniObject * * olddata, GstMiniObject * newdata);
	typedef void (*gst_mini_object_unref_t) (GstMiniObject * mini_object);
	typedef GType (*gst_object_get_type_t) (void);
	typedef gpointer (*gst_object_ref_t) (gpointer object);
	typedef void (*gst_object_replace_t) (GstObject * * oldobj, GstObject * newobj);
	typedef void (*gst_object_unref_t) (gpointer object);
	typedef GstCaps * (*gst_pad_get_caps_t) (GstPad * pad);
	typedef G_CONST_RETURN GstCaps * (*gst_pad_get_pad_template_caps_t) (GstPad * pad);
	typedef gboolean (*gst_pad_is_linked_t) (GstPad * pad);
	typedef GstPadLinkReturn (*gst_pad_link_t) (GstPad * srcpad, GstPad * sinkpad);
	typedef GParamSpec * (*gst_param_spec_mini_object_t) (const char * name, const char * nick, const char * blurb, GType object_type, GParamFlags flags);
	typedef GstBus * (*gst_pipeline_get_bus_t) (GstPipeline * pipeline);
	typedef GstElement * (*gst_pipeline_new_t) (const gchar * name);
	typedef void (*gst_plugin_feature_list_free_t) (GList * list);
	typedef gboolean (*gst_plugin_register_static_t) (gint major_version, gint minor_version, const gchar * name, gchar * description, GstPluginInitFunc init_func, const gchar * version, const gchar * license, const gchar * source, const gchar * package, const gchar * origin);
	typedef GList * (*gst_registry_feature_filter_t) (GstRegistry * registry, GstPluginFeatureFilter filter, gboolean first, gpointer user_data);
	typedef GstRegistry * (*gst_registry_get_default_t) (void);
	typedef gboolean (*gst_registry_scan_path_t) (GstRegistry * registry, const gchar * path);
	typedef GstPadTemplate * (*gst_static_pad_template_get_t) (GstStaticPadTemplate * pad_template);
	typedef GstCaps * (*gst_static_pad_template_get_caps_t) (GstStaticPadTemplate * templ);
	typedef gboolean (*gst_structure_get_fraction_t) (const GstStructure * structure, const gchar * fieldname, gint * value_numerator, gint * value_denominator);
	typedef gboolean (*gst_structure_get_int_t) (const GstStructure * structure, const gchar * fieldname, gint * value);
	typedef G_CONST_RETURN gchar * (*gst_structure_get_name_t) (const GstStructure * structure);
	typedef G_CONST_RETURN GValue * (*gst_structure_get_value_t) (const GstStructure * structure, const gchar * fieldname);
	typedef gboolean (*gst_structure_has_name_t) (const GstStructure * structure, const gchar * name);
	typedef GstStructure * (*gst_structure_new_t) (const gchar * name, const gchar * firstfield, ...);
	typedef GType (*gst_type_register_static_full_t) (GType parent_type, const gchar * type_name, guint class_size, GBaseInitFunc base_init, GBaseFinalizeFunc base_finalize, GClassInitFunc class_init, GClassFinalizeFunc class_finalize, gconstpointer class_data, guint instance_size, guint16 n_preallocs, GInstanceInitFunc instance_init, const GTypeValueTable * value_table, GTypeFlags flags);
	typedef void (*gst_value_take_mini_object_t) (GValue * value, GstMiniObject * mini_object);

#ifndef GST_DISABLE_GST_DEBUG
	__gst_debug_min_t sym___gst_debug_min;
#endif
#ifndef GST_DISABLE_GST_DEBUG
	_gst_debug_category_new_t sym__gst_debug_category_new;
#endif
#ifndef GST_DISABLE_GST_DEBUG
	_gst_debug_register_funcptr_t sym__gst_debug_register_funcptr;
#endif
	gst_bin_add_t sym_gst_bin_add;
	gst_bin_get_by_name_t sym_gst_bin_get_by_name;
	gst_bin_get_type_t sym_gst_bin_get_type;
	gst_bin_new_t sym_gst_bin_new;
	gst_bin_remove_t sym_gst_bin_remove;
	gst_buffer_get_type_t sym_gst_buffer_get_type;
	gst_buffer_new_and_alloc_t sym_gst_buffer_new_and_alloc;
	gst_buffer_set_caps_t sym_gst_buffer_set_caps;
	gst_bus_create_watch_t sym_gst_bus_create_watch;
	gst_bus_have_pending_t sym_gst_bus_have_pending;
	gst_bus_new_t sym_gst_bus_new;
	gst_bus_post_t sym_gst_bus_post;
	gst_bus_set_sync_handler_t sym_gst_bus_set_sync_handler;
	gst_bus_timed_pop_t sym_gst_bus_timed_pop;
	gst_caps_copy_t sym_gst_caps_copy;
	gst_caps_from_string_t sym_gst_caps_from_string;
	gst_caps_get_structure_t sym_gst_caps_get_structure;
	gst_caps_intersect_t sym_gst_caps_intersect;
	gst_caps_is_any_t sym_gst_caps_is_any;
	gst_caps_is_empty_t sym_gst_caps_is_empty;
	gst_caps_is_fixed_t sym_gst_caps_is_fixed;
	gst_caps_new_simple_t sym_gst_caps_new_simple;
	gst_caps_unref_t sym_gst_caps_unref;
#ifndef GST_DISABLE_GST_DEBUG
	gst_debug_log_t sym_gst_debug_log;
#endif
	gst_element_add_pad_t sym_gst_element_add_pad;
	gst_element_class_add_pad_template_t sym_gst_element_class_add_pad_template;
	gst_element_class_set_details_t sym_gst_element_class_set_details;
	gst_element_factory_get_klass_t sym_gst_element_factory_get_klass;
	gst_element_factory_get_longname_t sym_gst_element_factory_get_longname;
	gst_element_factory_get_static_pad_templates_t sym_gst_element_factory_get_static_pad_templates;
	gst_element_factory_get_type_t sym_gst_element_factory_get_type;
	gst_element_factory_make_t sym_gst_element_factory_make;
	gst_element_get_state_t sym_gst_element_get_state;
	gst_element_get_static_pad_t sym_gst_element_get_static_pad;
	gst_element_get_type_t sym_gst_element_get_type;
	gst_element_link_t sym_gst_element_link;
	gst_element_post_message_t sym_gst_element_post_message;
	gst_element_query_duration_t sym_gst_element_query_duration;
	gst_element_query_position_t sym_gst_element_query_position;
	gst_element_register_t sym_gst_element_register;
	gst_element_seek_t sym_gst_element_seek;
	gst_element_seek_simple_t sym_gst_element_seek_simple;
	gst_element_set_index_t sym_gst_element_set_index;
	gst_element_set_state_t sym_gst_element_set_state;
	gst_element_state_get_name_t sym_gst_element_state_get_name;
	gst_ghost_pad_new_t sym_gst_ghost_pad_new;
	gst_index_entry_assoc_map_t sym_gst_index_entry_assoc_map;
	gst_index_factory_make_t sym_gst_index_factory_make;
	gst_index_get_assoc_entry_t sym_gst_index_get_assoc_entry;
	gst_init_check_t sym_gst_init_check;
	gst_message_get_structure_t sym_gst_message_get_structure;
	gst_message_get_type_t sym_gst_message_get_type;
	gst_message_new_application_t sym_gst_message_new_application;
	gst_message_new_duration_t sym_gst_message_new_duration;
	gst_message_parse_duration_t sym_gst_message_parse_duration;
	gst_message_parse_error_t sym_gst_message_parse_error;
	gst_message_parse_state_changed_t sym_gst_message_parse_state_changed;
	gst_message_parse_warning_t sym_gst_message_parse_warning;
	gst_message_type_get_name_t sym_gst_message_type_get_name;
	gst_mini_object_get_type_t sym_gst_mini_object_get_type;
	gst_mini_object_new_t sym_gst_mini_object_new;
	gst_mini_object_ref_t sym_gst_mini_object_ref;
	gst_mini_object_replace_t sym_gst_mini_object_replace;
	gst_mini_object_unref_t sym_gst_mini_object_unref;
	gst_object_get_type_t sym_gst_object_get_type;
	gst_object_ref_t sym_gst_object_ref;
	gst_object_replace_t sym_gst_object_replace;
	gst_object_unref_t sym_gst_object_unref;
	gst_pad_get_caps_t sym_gst_pad_get_caps;
	gst_pad_get_pad_template_caps_t sym_gst_pad_get_pad_template_caps;
	gst_pad_is_linked_t sym_gst_pad_is_linked;
	gst_pad_link_t sym_gst_pad_link;
	gst_param_spec_mini_object_t sym_gst_param_spec_mini_object;
	gst_pipeline_get_bus_t sym_gst_pipeline_get_bus;
	gst_pipeline_new_t sym_gst_pipeline_new;
	gst_plugin_feature_list_free_t sym_gst_plugin_feature_list_free;
	gst_plugin_register_static_t sym_gst_plugin_register_static;
	gst_registry_feature_filter_t sym_gst_registry_feature_filter;
	gst_registry_get_default_t sym_gst_registry_get_default;
	gst_registry_scan_path_t sym_gst_registry_scan_path;
	gst_static_pad_template_get_t sym_gst_static_pad_template_get;
	gst_static_pad_template_get_caps_t sym_gst_static_pad_template_get_caps;
	gst_structure_get_fraction_t sym_gst_structure_get_fraction;
	gst_structure_get_int_t sym_gst_structure_get_int;
	gst_structure_get_name_t sym_gst_structure_get_name;
	gst_structure_get_value_t sym_gst_structure_get_value;
	gst_structure_has_name_t sym_gst_structure_has_name;
	gst_structure_new_t sym_gst_structure_new;
	gst_type_register_static_full_t sym_gst_type_register_static_full;
	gst_value_take_mini_object_t sym_gst_value_take_mini_object;
};

extern LibGStreamer g_LibGStreamer;


#ifndef GST_DISABLE_GST_DEBUG
#define __gst_debug_min (*g_LibGStreamer.sym___gst_debug_min)
#endif
#ifndef GST_DISABLE_GST_DEBUG
#define _gst_debug_category_new (*g_LibGStreamer.sym__gst_debug_category_new)
#endif
#ifndef GST_DISABLE_GST_DEBUG
#define _gst_debug_register_funcptr (*g_LibGStreamer.sym__gst_debug_register_funcptr)
#endif
#define gst_bin_add (*g_LibGStreamer.sym_gst_bin_add)
#define gst_bin_get_by_name (*g_LibGStreamer.sym_gst_bin_get_by_name)
#define gst_bin_get_type (*g_LibGStreamer.sym_gst_bin_get_type)
#define gst_bin_new (*g_LibGStreamer.sym_gst_bin_new)
#define gst_bin_remove (*g_LibGStreamer.sym_gst_bin_remove)
#define gst_buffer_get_type (*g_LibGStreamer.sym_gst_buffer_get_type)
#define gst_buffer_new_and_alloc (*g_LibGStreamer.sym_gst_buffer_new_and_alloc)
#define gst_buffer_set_caps (*g_LibGStreamer.sym_gst_buffer_set_caps)
#define gst_bus_create_watch (*g_LibGStreamer.sym_gst_bus_create_watch)
#define gst_bus_have_pending (*g_LibGStreamer.sym_gst_bus_have_pending)
#define gst_bus_new (*g_LibGStreamer.sym_gst_bus_new)
#define gst_bus_post (*g_LibGStreamer.sym_gst_bus_post)
#define gst_bus_set_sync_handler (*g_LibGStreamer.sym_gst_bus_set_sync_handler)
#define gst_bus_timed_pop (*g_LibGStreamer.sym_gst_bus_timed_pop)
#define gst_caps_copy (*g_LibGStreamer.sym_gst_caps_copy)
#define gst_caps_from_string (*g_LibGStreamer.sym_gst_caps_from_string)
#define gst_caps_get_structure (*g_LibGStreamer.sym_gst_caps_get_structure)
#define gst_caps_intersect (*g_LibGStreamer.sym_gst_caps_intersect)
#define gst_caps_is_any (*g_LibGStreamer.sym_gst_caps_is_any)
#define gst_caps_is_empty (*g_LibGStreamer.sym_gst_caps_is_empty)
#define gst_caps_is_fixed (*g_LibGStreamer.sym_gst_caps_is_fixed)
#define gst_caps_new_simple (*g_LibGStreamer.sym_gst_caps_new_simple)
#define gst_caps_unref (*g_LibGStreamer.sym_gst_caps_unref)
#ifndef GST_DISABLE_GST_DEBUG
#define gst_debug_log (*g_LibGStreamer.sym_gst_debug_log)
#endif
#define gst_element_add_pad (*g_LibGStreamer.sym_gst_element_add_pad)
#define gst_element_class_add_pad_template (*g_LibGStreamer.sym_gst_element_class_add_pad_template)
#define gst_element_class_set_details (*g_LibGStreamer.sym_gst_element_class_set_details)
#define gst_element_factory_get_klass (*g_LibGStreamer.sym_gst_element_factory_get_klass)
#define gst_element_factory_get_longname (*g_LibGStreamer.sym_gst_element_factory_get_longname)
#define gst_element_factory_get_static_pad_templates (*g_LibGStreamer.sym_gst_element_factory_get_static_pad_templates)
#define gst_element_factory_get_type (*g_LibGStreamer.sym_gst_element_factory_get_type)
#define gst_element_factory_make (*g_LibGStreamer.sym_gst_element_factory_make)
#define gst_element_get_state (*g_LibGStreamer.sym_gst_element_get_state)
#define gst_element_get_static_pad (*g_LibGStreamer.sym_gst_element_get_static_pad)
#define gst_element_get_type (*g_LibGStreamer.sym_gst_element_get_type)
#define gst_element_link (*g_LibGStreamer.sym_gst_element_link)
#define gst_element_post_message (*g_LibGStreamer.sym_gst_element_post_message)
#define gst_element_query_duration (*g_LibGStreamer.sym_gst_element_query_duration)
#define gst_element_query_position (*g_LibGStreamer.sym_gst_element_query_position)
#define gst_element_register (*g_LibGStreamer.sym_gst_element_register)
#define gst_element_seek (*g_LibGStreamer.sym_gst_element_seek)
#define gst_element_seek_simple (*g_LibGStreamer.sym_gst_element_seek_simple)
#define gst_element_set_index (*g_LibGStreamer.sym_gst_element_set_index)
#define gst_element_set_state (*g_LibGStreamer.sym_gst_element_set_state)
#define gst_element_state_get_name (*g_LibGStreamer.sym_gst_element_state_get_name)
#define gst_ghost_pad_new (*g_LibGStreamer.sym_gst_ghost_pad_new)
#define gst_index_entry_assoc_map (*g_LibGStreamer.sym_gst_index_entry_assoc_map)
#define gst_index_factory_make (*g_LibGStreamer.sym_gst_index_factory_make)
#define gst_index_get_assoc_entry (*g_LibGStreamer.sym_gst_index_get_assoc_entry)
#define gst_init_check (*g_LibGStreamer.sym_gst_init_check)
#define gst_message_get_structure (*g_LibGStreamer.sym_gst_message_get_structure)
#define gst_message_get_type (*g_LibGStreamer.sym_gst_message_get_type)
#define gst_message_new_application (*g_LibGStreamer.sym_gst_message_new_application)
#define gst_message_new_duration (*g_LibGStreamer.sym_gst_message_new_duration)
#define gst_message_parse_duration (*g_LibGStreamer.sym_gst_message_parse_duration)
#define gst_message_parse_error (*g_LibGStreamer.sym_gst_message_parse_error)
#define gst_message_parse_state_changed (*g_LibGStreamer.sym_gst_message_parse_state_changed)
#define gst_message_parse_warning (*g_LibGStreamer.sym_gst_message_parse_warning)
#define gst_message_type_get_name (*g_LibGStreamer.sym_gst_message_type_get_name)
#define gst_mini_object_get_type (*g_LibGStreamer.sym_gst_mini_object_get_type)
#define gst_mini_object_new (*g_LibGStreamer.sym_gst_mini_object_new)
#define gst_mini_object_ref (*g_LibGStreamer.sym_gst_mini_object_ref)
#define gst_mini_object_replace (*g_LibGStreamer.sym_gst_mini_object_replace)
#define gst_mini_object_unref (*g_LibGStreamer.sym_gst_mini_object_unref)
#define gst_object_get_type (*g_LibGStreamer.sym_gst_object_get_type)
#define gst_object_ref (*g_LibGStreamer.sym_gst_object_ref)
#define gst_object_replace (*g_LibGStreamer.sym_gst_object_replace)
#define gst_object_unref (*g_LibGStreamer.sym_gst_object_unref)
#define gst_pad_get_caps (*g_LibGStreamer.sym_gst_pad_get_caps)
#define gst_pad_get_pad_template_caps (*g_LibGStreamer.sym_gst_pad_get_pad_template_caps)
#define gst_pad_is_linked (*g_LibGStreamer.sym_gst_pad_is_linked)
#define gst_pad_link (*g_LibGStreamer.sym_gst_pad_link)
#define gst_param_spec_mini_object (*g_LibGStreamer.sym_gst_param_spec_mini_object)
#define gst_pipeline_get_bus (*g_LibGStreamer.sym_gst_pipeline_get_bus)
#define gst_pipeline_new (*g_LibGStreamer.sym_gst_pipeline_new)
#define gst_plugin_feature_list_free (*g_LibGStreamer.sym_gst_plugin_feature_list_free)
#define gst_plugin_register_static (*g_LibGStreamer.sym_gst_plugin_register_static)
#define gst_registry_feature_filter (*g_LibGStreamer.sym_gst_registry_feature_filter)
#define gst_registry_get_default (*g_LibGStreamer.sym_gst_registry_get_default)
#define gst_registry_scan_path (*g_LibGStreamer.sym_gst_registry_scan_path)
#define gst_static_pad_template_get (*g_LibGStreamer.sym_gst_static_pad_template_get)
#define gst_static_pad_template_get_caps (*g_LibGStreamer.sym_gst_static_pad_template_get_caps)
#define gst_structure_get_fraction (*g_LibGStreamer.sym_gst_structure_get_fraction)
#define gst_structure_get_int (*g_LibGStreamer.sym_gst_structure_get_int)
#define gst_structure_get_name (*g_LibGStreamer.sym_gst_structure_get_name)
#define gst_structure_get_value (*g_LibGStreamer.sym_gst_structure_get_value)
#define gst_structure_has_name (*g_LibGStreamer.sym_gst_structure_has_name)
#define gst_structure_new (*g_LibGStreamer.sym_gst_structure_new)
#define gst_type_register_static_full (*g_LibGStreamer.sym_gst_type_register_static_full)
#define gst_value_take_mini_object (*g_LibGStreamer.sym_gst_value_take_mini_object)


class LibGstBase
{
public:
	BOOL Load(OpDLL* dll);

	typedef GType (*gst_base_sink_get_type_t) (void);
	typedef GType (*gst_base_src_get_type_t) (void);

	gst_base_sink_get_type_t sym_gst_base_sink_get_type;
	gst_base_src_get_type_t sym_gst_base_src_get_type;
};

extern LibGstBase g_LibGstBase;


#define gst_base_sink_get_type (*g_LibGstBase.sym_gst_base_sink_get_type)
#define gst_base_src_get_type (*g_LibGstBase.sym_gst_base_src_get_type)


class LibGstVideo
{
public:
	BOOL Load(OpDLL* dll);

	typedef GType (*gst_video_sink_get_type_t) (void);

	gst_video_sink_get_type_t sym_gst_video_sink_get_type;
};

extern LibGstVideo g_LibGstVideo;


#define gst_video_sink_get_type (*g_LibGstVideo.sym_gst_video_sink_get_type)


class LibGstRiff
{
public:
	BOOL Load(OpDLL* dll);

	typedef GstCaps * (*gst_riff_create_audio_caps_t) (guint16 codec_id, gst_riff_strh * strh, gst_riff_strf_auds * strf, GstBuffer * strf_data, GstBuffer * strd_data, char * * codec_name);
	typedef void (*gst_riff_init_t) (void);

	gst_riff_create_audio_caps_t sym_gst_riff_create_audio_caps;
	gst_riff_init_t sym_gst_riff_init;
};

extern LibGstRiff g_LibGstRiff;


#define gst_riff_create_audio_caps (*g_LibGstRiff.sym_gst_riff_create_audio_caps)
#define gst_riff_init (*g_LibGstRiff.sym_gst_riff_init)


class LibGModule
{
public:
	BOOL Load(OpDLL* dll);

	typedef GModule * (*g_module_open_t) (const gchar * file_name, GModuleFlags flags);
	typedef gboolean (*g_module_close_t) (GModule * module);
	typedef gboolean (*g_module_symbol_t) (GModule * module, const gchar * symbol_name, gpointer * symbol);
	typedef void (*g_module_make_resident_t) (GModule * module);

	g_module_open_t sym_g_module_open;
	g_module_close_t sym_g_module_close;
	g_module_symbol_t sym_g_module_symbol;
	g_module_make_resident_t sym_g_module_make_resident;
};

extern LibGModule g_LibGModule;


#define g_module_open (*g_LibGModule.sym_g_module_open)
#define g_module_close (*g_LibGModule.sym_g_module_close)
#define g_module_symbol (*g_LibGModule.sym_g_module_symbol)
#define g_module_make_resident (*g_LibGModule.sym_g_module_make_resident)

// END GENERATED CODE

// Note: inline functions defined in the gst headers need
// to be redefined so that our redirected functions are used.

G_INLINE_FUNC gboolean
__g_once_init_enter (volatile gsize *value_location)
{
  if G_LIKELY ((gpointer) g_atomic_pointer_get (value_location) != NULL)
    return FALSE;
  else
    return g_once_init_enter_impl (value_location);
}

static inline GstBuffer *
__gst_buffer_ref (GstBuffer * buf)
{
  return (GstBuffer *) gst_mini_object_ref (GST_MINI_OBJECT_CAST (buf));
}

static inline void
__gst_buffer_unref (GstBuffer * buf)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (buf));
}

static inline GstMessage *
__gst_message_ref (GstMessage * msg)
{
  return (GstMessage *) gst_mini_object_ref (GST_MINI_OBJECT (msg));
}

static inline void
__gst_message_unref (GstMessage * msg)
{
  gst_mini_object_unref (GST_MINI_OBJECT_CAST (msg));
}

#define g_once_init_enter __g_once_init_enter
#define gst_buffer_ref __gst_buffer_ref
#define gst_buffer_unref __gst_buffer_unref
#define gst_message_ref __gst_message_ref
#define gst_message_unref __gst_message_unref

#else // !MEDIA_BACKEND_GSTREAMER_USE_OPDLL

#include <glib.h>
#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include <gst/video/gstvideosink.h>
#include <gst/video/video.h>
#include <gst/riff/riff-media.h>
#include <gst/riff/riff-read.h>

#endif // MEDIA_BACKEND_GSTREAMER_USE_OPDLL

#endif // MEDIA_BACKEND_GSTREAMER

#endif // GSTLIBS_H
