/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas) and Eirik Byrkjeflot Anonsen (eirik)
 */

#ifndef OP_GTK_H
#define OP_GTK_H

// Qt can define 'signals'. Get rid of it.
#ifdef signals
# if signals == protected
#  define QT_SIGNALS_PROTECTED
#  undef signals
# endif // signals == protected
#endif // signals

// Include GTK headers
#include <gtk/gtk.h>

// Put back 'signals' define
#ifdef QT_SIGNALS_PROTECTED
# define signals protected
#endif // QT_SIGNALS_PROTECTED

#include "platforms/utilix/dlmacros.h"
#include "platforms/utilix/x11_all.h"

namespace OpGtk
{
	extern void * g_gtk_handle;
	extern void * g_gdk_handle;
	extern void * g_glib_handle;
	extern void * g_gobject_handle;

	// glib
	DECLARE_SYMBOL(guint,		 g_idle_add,    (gboolean (*)(void*), void*));
	DECLARE_SYMBOL(gboolean,	 g_idle_remove_by_data, (void*));
	DECLARE_SYMBOL(guint,		 g_timeout_add, (guint, gboolean (*)(void*), void*));
	DECLARE_SYMBOL(GSList*,       g_slist_nth,   (GSList *, guint));
	DECLARE_SYMBOL(guint,         g_slist_length,(GSList *));
	DECLARE_SYMBOL(void,          g_slist_free,  (GSList *));
	DECLARE_SYMBOL(void,          g_free,        (gpointer));
	DECLARE_SYMBOL(void, g_io_channel_unref, (GIOChannel *channel));
	DECLARE_SYMBOL(GIOChannel*, g_io_channel_unix_new, (int fd));
	DECLARE_SYMBOL(guint, g_io_add_watch, (GIOChannel *channel, GIOCondition condition, GIOFunc func, gpointer user_data));
	DECLARE_SYMBOL(gint, g_io_channel_unix_get_fd, (GIOChannel *channel));
	DECLARE_SYMBOL(gboolean, g_source_remove, (guint tag));

	// gobject
	DECLARE_SYMBOL(gulong,	     g_signal_connect_data, (gpointer, const gchar *, GCallback,
														 gpointer,GClosureNotify, GConnectFlags));
	DECLARE_SYMBOL(void,          g_object_disconnect,   (gpointer, const gchar *, ... ));
	DECLARE_SYMBOL(void,          g_object_get,          (gpointer, const gchar *, ... ));
	DECLARE_SYMBOL(void,	         g_signal_handler_disconnect, (gpointer, gulong));

	// Gdk
	DECLARE_SYMBOL(gint,               gdk_input_add, (gint, GdkInputCondition,
	                                                   void (*)(void*, gint, GdkInputCondition), gpointer));
	DECLARE_SYMBOL(void,               gdk_input_remove,               (gint));
	DECLARE_SYMBOL(GdkDisplay*,        gdk_display_get_default,        ());
	DECLARE_SYMBOL(GType,              gdk_window_object_get_type,     ());
	DECLARE_SYMBOL(void,               gdk_window_set_transient_for,   (GdkWindow*, GdkWindow*));
	DECLARE_SYMBOL(void,               gdk_window_set_events,          (GdkWindow*, GdkEventMask));
	DECLARE_SYMBOL(void,               gdk_flush,                      ());
	DECLARE_SYMBOL(gint,               gdk_error_trap_pop,             ());
	DECLARE_SYMBOL(void,               gdk_error_trap_push,            ());
	DECLARE_SYMBOL(GdkPixbuf*,         gdk_pixbuf_new_from_xpm_data,   (const char** data));

	// Gdkx
	DECLARE_SYMBOL(GdkWindow*,         gdk_window_foreign_new,         (GdkNativeWindow));
	DECLARE_SYMBOL(GdkDisplay*,        gdk_x11_lookup_xdisplay,        (X11Types::Display*));
	DECLARE_SYMBOL(X11Types::Colormap, gdk_x11_colormap_get_xcolormap, (GdkColormap*));
	DECLARE_SYMBOL(X11Types::Display*, gdk_x11_display_get_xdisplay,   (GdkDisplay*));
	DECLARE_SYMBOL(X11Types::Display*, gdk_x11_drawable_get_xdisplay,  (GdkDrawable*));
	DECLARE_SYMBOL(XID,                gdk_x11_drawable_get_xid,       (GdkDrawable*));
	DECLARE_SYMBOL(Screen*,            gdk_x11_screen_get_xscreen,     (GdkScreen*));
	DECLARE_SYMBOL(X11Types::Visual*,  gdk_x11_visual_get_xvisual,     (GdkVisual*));

	// Gtk
	DECLARE_SYMBOL(void,			 gtk_init, (int*, char***));
	DECLARE_SYMBOL(gboolean,      gtk_init_check, (int *, char ***));
	DECLARE_SYMBOL(void,			 gtk_main, ());
	DECLARE_SYMBOL(guint,			 gtk_main_level, ());
	DECLARE_SYMBOL(void,			 gtk_main_quit, ());
	DECLARE_SYMBOL(void,			 gtk_true, ());

	// Container
	DECLARE_SYMBOL(void,			 gtk_container_add, (GtkContainer*, GtkWidget*));

	// Widget
	DECLARE_SYMBOL(void,          gtk_widget_grab_focus,       (GtkWidget*));
	DECLARE_SYMBOL(void,          gtk_widget_realize,          (GtkWidget*));
	DECLARE_SYMBOL(GdkWindow*,    gtk_widget_get_parent_window,(GtkWidget*));
	DECLARE_SYMBOL(GtkWidget*,    gtk_widget_get_parent,       (GtkWidget*));
	DECLARE_SYMBOL(void,			 gtk_widget_set_size_request, (GtkWidget*, gint, gint));
	DECLARE_SYMBOL(void,			 gtk_widget_destroy, (GtkWidget*));
	DECLARE_SYMBOL(void,			 gtk_widget_destroyed, (GtkWidget*, GtkWidget**));
	DECLARE_SYMBOL(void,			 gtk_widget_show, (GtkWidget*));
	DECLARE_SYMBOL(void,          gtk_widget_hide, (GtkWidget*));
	DECLARE_SYMBOL(void,             gtk_widget_set_uposition, (GtkWidget*, gint, gint));
	DECLARE_SYMBOL(GdkVisual*,	 gtk_widget_get_visual,  (GtkWidget*));
	DECLARE_SYMBOL(GdkColormap*,	 gtk_widget_get_colormap,(GtkWidget*));
	DECLARE_SYMBOL(GdkScreen*,	 gtk_widget_get_screen,  (GtkWidget*));
	DECLARE_SYMBOL(void,          gtk_widget_path,        (GtkWidget*, guint*, gchar**, gchar**));

	// Socket
	DECLARE_SYMBOL(GtkWidget*,	 gtk_socket_new, ());
	DECLARE_SYMBOL(GdkNativeWindow,gtk_socket_get_id, (GtkSocket*));

	// Plug
	DECLARE_SYMBOL(GtkWidget*,	 gtk_plug_new, (GdkNativeWindow));

	// Dialog
	DECLARE_SYMBOL(gint,          gtk_dialog_run, (GtkDialog *));
	DECLARE_SYMBOL(void,          gtk_dialog_set_default_response, (GtkDialog *, gint));
	DECLARE_SYMBOL(void,          gtk_dialog_response,             (GtkDialog *, gint));
	DECLARE_SYMBOL(GtkWidget*,    gtk_message_dialog_new,  (GtkWindow *parent,
														   GtkDialogFlags flags,
														   GtkMessageType type,
														   GtkButtonsType buttons,
														   const gchar *message_format,
														   ...));

	// GetTypes
	DECLARE_SYMBOL(GType,		 gtk_container_get_type, ());
	DECLARE_SYMBOL(GTypeInstance*,g_type_check_instance_cast, (GTypeInstance*, GType));
	DECLARE_SYMBOL(gboolean,      g_type_check_instance_is_a, (GTypeInstance, GType));
	DECLARE_SYMBOL(GType, 		 g_type_fundamental,		(GType type_id));
	DECLARE_SYMBOL(GType,         gtk_file_chooser_get_type, ());
	DECLARE_SYMBOL(GType,         gtk_dialog_get_type, ());
	DECLARE_SYMBOL(GType,         gtk_file_chooser_dialog_get_type, ());
	DECLARE_SYMBOL(GType,         gtk_window_get_type, ());
	DECLARE_SYMBOL(GType,         gtk_socket_get_type, ());
	DECLARE_SYMBOL(GType,         gtk_object_get_type, ());
	DECLARE_SYMBOL(GType,         gtk_plug_get_type ,  ());
	DECLARE_SYMBOL(GType,         gtk_image_get_type, ());

	// FileChooser
	DECLARE_SYMBOL(gchar*,        gtk_file_chooser_get_filename,    (GtkFileChooser*));
	DECLARE_SYMBOL(GSList*,       gtk_file_chooser_get_filenames,   (GtkFileChooser*));
	DECLARE_SYMBOL(gboolean,      gtk_file_chooser_set_filename,    (GtkFileChooser*, const char*));
	DECLARE_SYMBOL(void,          gtk_file_chooser_set_current_name,(GtkFileChooser*, const gchar*));
	DECLARE_SYMBOL(gchar*,        gtk_file_chooser_get_current_folder,(GtkFileChooser*));
	DECLARE_SYMBOL(gboolean,      gtk_file_chooser_set_current_folder,(GtkFileChooser*, const gchar*));

	DECLARE_SYMBOL(void,          gtk_file_chooser_set_select_multiple, (GtkFileChooser*, gboolean));
	DECLARE_SYMBOL(GtkWidget*,    gtk_file_chooser_dialog_new, (const gchar*, GtkWindow*, GtkFileChooserAction, const gchar*, ...));

	DECLARE_SYMBOL(GtkFileFilter*,gtk_file_filter_new, (void));
	DECLARE_SYMBOL(void,          gtk_file_filter_set_name, (GtkFileFilter*, const gchar*));
	DECLARE_SYMBOL(const gchar*,  gtk_file_filter_get_name, (GtkFileFilter*));
	DECLARE_SYMBOL(void,          gtk_file_filter_add_pattern, (GtkFileFilter*, const gchar*));
	DECLARE_SYMBOL(void,          gtk_file_chooser_add_filter, (GtkFileChooser*, GtkFileFilter*));
	DECLARE_SYMBOL(void,          gtk_file_chooser_set_filter, (GtkFileChooser*, GtkFileFilter*));
	DECLARE_SYMBOL(GtkFileFilter*,gtk_file_chooser_get_filter, (GtkFileChooser*));
	//DECLARE_SYMBOL(void,          gtk_file_chooser_set_local_only, (GtkFileChooser*, gboolean));
	//DECLARE_SYMBOL(gchar*,        gtk_file_chooser_get_uri,    (GtkFileChooser *chooser));

	DECLARE_SYMBOL(void,          gtk_file_chooser_set_preview_widget, (GtkFileChooser *chooser,
																		GtkWidget *preview_widget));
	DECLARE_SYMBOL(gchar*,        gtk_file_chooser_get_preview_filename, (GtkFileChooser* chooser));
	DECLARE_SYMBOL(void,          gtk_file_chooser_set_preview_widget_active, (GtkFileChooser *chooser,
																			  gboolean active));
	DECLARE_SYMBOL(void,          gtk_file_chooser_set_use_preview_label, (GtkFileChooser* chooser,  gboolean use_label));


	DECLARE_SYMBOL(GtkWidget*,    gtk_image_new,                       ());
	DECLARE_SYMBOL(void,          gtk_image_set_from_file,            (GtkImage *image,
																	  const gchar *filename));

	// Window
	DECLARE_SYMBOL(void,          gtk_window_set_transient_for, (GtkWindow*, GtkWindow*));
	DECLARE_SYMBOL(void,          gtk_window_set_destroy_with_parent, (GtkWindow*, gboolean));
	DECLARE_SYMBOL(void,          gtk_window_set_modal,      (GtkWindow*, gboolean));
	DECLARE_SYMBOL(void,          gtk_window_set_icon,                 (GtkWindow *window,
																	   GdkPixbuf *icon));

	// Filechooser functions, requires version 2.8 of Gtk
	DECLARE_SYMBOL(void,          gtk_file_chooser_set_show_hidden, (GtkFileChooser*, gboolean));
	DECLARE_SYMBOL(void,          gtk_file_chooser_set_do_overwrite_confirmation,(GtkFileChooser*, gboolean));



	void *Sym(void *handle, const char *symbol);

	bool ResolveSymbols(bool load_extended);

	/**
	 * Attempts to open the library and will print an error if this fails
	 */
	void * OpenLibrary(const char * library_name);

	/**
	 * Attempts to close the library if it is not null
	 */
	void CloseLibrary(void * lib_handle);

	/**
	 * Could try to load the Gtk symbols from the pluginwrapper, this will only
	 * work if the pluginwrapper was compiled with GTK linked in, which it currently
	 * is not. This code is therefore currently commented out.
	 */
	bool LoadGtk(bool load_extended = false);

	void UnloadGtk();
}

// ------------------
// Macros from gtk-2.0/gdk/gdkx.h
// ------------------

#undef GDK_WINDOW_XDISPLAY
#undef GDK_WINDOW_XID
#undef GDK_COLORMAP_XCOLORMAP
#undef GDK_SCREEN_XSCREEN
#undef GDK_VISUAL_XVISUAL

#define GDK_WINDOW_XDISPLAY(win)      (OpGtk::gdk_x11_drawable_get_xdisplay (((GdkWindowObject *)win)->impl))
#define GDK_WINDOW_XID(win)           (OpGtk::gdk_x11_drawable_get_xid (win))
#define GDK_COLORMAP_XCOLORMAP(cmap)  (OpGtk::gdk_x11_colormap_get_xcolormap (cmap))
#define GDK_SCREEN_XSCREEN(screen)    (OpGtk::gdk_x11_screen_get_xscreen (screen))
#define GDK_VISUAL_XVISUAL(visual)    (OpGtk::gdk_x11_visual_get_xvisual (visual))

// ------------------
// Macros from glib-2.0/gobject/gtype.h
// ------------------

#undef _G_TYPE_CIC
#undef G_TYPE_CHECK_INSTANCE_CAST
#undef GTK_CHECK_CAST

#define _G_TYPE_CIC(ip, gt, ct)     ((ct*) OpGtk::g_type_check_instance_cast ((GTypeInstance*) ip, gt))
#define G_TYPE_CHECK_INSTANCE_CAST(instance, g_type, c_type)    (_G_TYPE_CIC ((instance), (g_type), c_type))
#define	GTK_CHECK_CAST		G_TYPE_CHECK_INSTANCE_CAST

// ------------------
// Macros from gtk-2.0/gtk/gtkcontainer.h
// ------------------

// Undefine the symbols :

#undef GDK_TYPE_WINDOW

#undef GTK_TYPE_CONTAINER
#undef GTK_TYPE_DIALOG
#undef GTK_TYPE_FILE_CHOOSER
#undef GTK_TYPE_OBJECT
#undef GTK_TYPE_PLUG
#undef GTK_TYPE_SOCKET
#undef GTK_TYPE_WIDGET
#undef GTK_TYPE_WINDOW
#undef GTK_TYPE_IMAGE

#undef GTK_OBJECT
#undef G_TYPE_FUNDAMENTAL

#undef GTK_CONTAINER
#undef GTK_DIALOG
#undef GTK_FILE_CHOOSER
#undef GTK_PLUG
#undef GTK_SOCKET
#undef GTK_WIDGET
#undef GTK_WINDOW
#undef GTK_IMAGE

#undef g_signal_connect
#undef g_slist_next

// Redefine the symbols :

#define GDK_TYPE_WINDOW             (OpGtk::gdk_window_object_get_type ())

#define GTK_TYPE_CONTAINER          (OpGtk::gtk_container_get_type ())
#define GTK_TYPE_DIALOG             (OpGtk::gtk_dialog_get_type ())
#define GTK_TYPE_FILE_CHOOSER       (OpGtk::gtk_file_chooser_get_type ())
#define	GTK_TYPE_OBJECT			    (OpGtk::gtk_object_get_type ())
#define GTK_TYPE_PLUG               (OpGtk::gtk_plug_get_type ())
#define GTK_TYPE_SOCKET             (OpGtk::gtk_socket_get_type ())
#define GTK_TYPE_WIDGET             (OpGtk::gtk_window_get_type ())
#define GTK_TYPE_WINDOW             (OpGtk::gtk_window_get_type ())
#define GTK_TYPE_IMAGE              (OpGtk::gtk_image_get_type ())

#define GTK_OBJECT(object)		    (GTK_CHECK_CAST ((object), GTK_TYPE_OBJECT, GtkObject))
#define G_TYPE_FUNDAMENTAL(type)	(OpGtk::g_type_fundamental (type))
#define G_TYPE_IS_OBJECT(type)      (G_TYPE_FUNDAMENTAL (type) == G_TYPE_OBJECT)
#define G_OBJECT(object)            (G_TYPE_CHECK_INSTANCE_CAST ((object), G_TYPE_OBJECT, GObject))

#define GTK_CONTAINER(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CONTAINER, GtkContainer))
#define GTK_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_DIALOG, GtkDialog))
#define GTK_FILE_CHOOSER(obj)       (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_FILE_CHOOSER, GtkFileChooser))
#define GTK_PLUG(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PLUG, GtkPlug))
#define GTK_SOCKET(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SOCKET, GtkSocket))
#define GTK_WIDGET(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_WIDGET, GtkWidget))
#define GTK_WINDOW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_WINDOW, GtkWindow))
#define GTK_IMAGE(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_IMAGE, GtkImage))

#define g_signal_connect(instance, detailed_signal, c_handler, data) \
    g_signal_connect_data ((instance), (detailed_signal), (c_handler), (data), NULL, (GConnectFlags) 0)

#define  g_slist_next(slist)	         ((slist) ? (((GSList *)(slist))->next) : NULL)

#endif // OP_GTK_H

