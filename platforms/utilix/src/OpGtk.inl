#include "platforms/utilix/OpGtk.h"

#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

namespace OpGtk
{
	void * g_gtk_handle     = 0;
	void * g_gdk_handle     = 0;
	void * g_glib_handle    = 0;
	void * g_gobject_handle = 0;

	// glib
	DEFINE_SYMBOL(guint,		 g_idle_add,    (gboolean (*)(void*), void*));
	DEFINE_SYMBOL(gboolean,		 g_idle_remove_by_data, (void*));
	DEFINE_SYMBOL(guint,		 g_timeout_add, (guint, gboolean (*)(void*), void*));
	DEFINE_SYMBOL(GSList*,       g_slist_nth,   (GSList *, guint));
	DEFINE_SYMBOL(guint,         g_slist_length,(GSList *));
	DEFINE_SYMBOL(void,          g_slist_free,  (GSList *));
	DEFINE_SYMBOL(void,          g_free,        (gpointer));
	DEFINE_SYMBOL(void, g_io_channel_unref, (GIOChannel *channel));
	DEFINE_SYMBOL(GIOChannel*, g_io_channel_unix_new, (int fd));
	DEFINE_SYMBOL(guint, g_io_add_watch, (GIOChannel *channel, GIOCondition condition, GIOFunc func, gpointer user_data));
	DEFINE_SYMBOL(gint, g_io_channel_unix_get_fd, (GIOChannel *channel));
	DEFINE_SYMBOL(gboolean, g_source_remove, (guint tag));

	// gobject
	DEFINE_SYMBOL(gulong,	     g_signal_connect_data, (gpointer, const gchar *, GCallback,
														 gpointer,GClosureNotify, GConnectFlags));
	DEFINE_SYMBOL(void,          g_object_disconnect,   (gpointer, const gchar *, ... ));
	DEFINE_SYMBOL(void,          g_object_get,          (gpointer, const gchar *, ... ));
	DEFINE_SYMBOL(void,	         g_signal_handler_disconnect, (gpointer, gulong));

	// Gdk
	DEFINE_SYMBOL(gint,               gdk_input_add, (gint, GdkInputCondition,
	                                                  void (*)(void*, gint, GdkInputCondition), gpointer));
	DEFINE_SYMBOL(void,               gdk_input_remove,               (gint));
	DEFINE_SYMBOL(GdkDisplay*,        gdk_display_get_default,        ());
	DEFINE_SYMBOL(GType,              gdk_window_object_get_type,     ());
	DEFINE_SYMBOL(void,               gdk_window_set_transient_for,   (GdkWindow*, GdkWindow*));
	DEFINE_SYMBOL(void,               gdk_window_set_events,          (GdkWindow*, GdkEventMask));
	DEFINE_SYMBOL(void,               gdk_flush,                      ());
	DEFINE_SYMBOL(gint,               gdk_error_trap_pop,             ());
	DEFINE_SYMBOL(void,               gdk_error_trap_push,            ());
	DEFINE_SYMBOL(GdkPixbuf*,         gdk_pixbuf_new_from_xpm_data,   (const char** data));

	// Gdkx
	DEFINE_SYMBOL(GdkWindow*,         gdk_window_foreign_new,         (GdkNativeWindow));
	DEFINE_SYMBOL(GdkDisplay*,        gdk_x11_lookup_xdisplay,        (X11Types::Display*));
	DEFINE_SYMBOL(X11Types::Colormap, gdk_x11_colormap_get_xcolormap, (GdkColormap*));
	DEFINE_SYMBOL(X11Types::Display*, gdk_x11_display_get_xdisplay,   (GdkDisplay*));
	DEFINE_SYMBOL(X11Types::Display*, gdk_x11_drawable_get_xdisplay,  (GdkDrawable*));
	DEFINE_SYMBOL(XID,                gdk_x11_drawable_get_xid,       (GdkDrawable*));
	DEFINE_SYMBOL(Screen*,            gdk_x11_screen_get_xscreen,     (GdkScreen*));
	DEFINE_SYMBOL(X11Types::Visual*,  gdk_x11_visual_get_xvisual,     (GdkVisual*));

	// Gtk
	DEFINE_SYMBOL(void,			 gtk_init, (int*, char***));
	DEFINE_SYMBOL(gboolean,      gtk_init_check, (int *, char ***));
	DEFINE_SYMBOL(void,			 gtk_main, ());
	DEFINE_SYMBOL(guint,		 gtk_main_level, ());
	DEFINE_SYMBOL(void,			 gtk_main_quit, ());
	DEFINE_SYMBOL(void,			 gtk_true, ());

	// Container
	DEFINE_SYMBOL(void,			 gtk_container_add, (GtkContainer*, GtkWidget*));

	// Widget
	DEFINE_SYMBOL(void,          gtk_widget_grab_focus,       (GtkWidget*));
	DEFINE_SYMBOL(void,          gtk_widget_realize,          (GtkWidget*));
	DEFINE_SYMBOL(GdkWindow*,    gtk_widget_get_parent_window,(GtkWidget*));
	DEFINE_SYMBOL(GtkWidget*,    gtk_widget_get_parent,       (GtkWidget*));
	DEFINE_SYMBOL(void,			 gtk_widget_set_size_request, (GtkWidget*, gint, gint));
	DEFINE_SYMBOL(void,			 gtk_widget_destroy, (GtkWidget*));
	DEFINE_SYMBOL(void,			 gtk_widget_destroyed, (GtkWidget*, GtkWidget**));
	DEFINE_SYMBOL(void,			 gtk_widget_show, (GtkWidget*));
	DEFINE_SYMBOL(void,          gtk_widget_hide, (GtkWidget*));
	DEFINE_SYMBOL(void,          gtk_widget_set_uposition, (GtkWidget*, gint, gint));
	DEFINE_SYMBOL(GdkVisual*,	 gtk_widget_get_visual,  (GtkWidget*));
	DEFINE_SYMBOL(GdkColormap*,	 gtk_widget_get_colormap,(GtkWidget*));
	DEFINE_SYMBOL(GdkScreen*,	 gtk_widget_get_screen,  (GtkWidget*));
	DEFINE_SYMBOL(void,          gtk_widget_path,        (GtkWidget*, guint*, gchar**, gchar**));

	// Socket
	DEFINE_SYMBOL(GtkWidget*,	 gtk_socket_new, ());
	DEFINE_SYMBOL(GdkNativeWindow,gtk_socket_get_id, (GtkSocket*));

	// Plug
	DEFINE_SYMBOL(GtkWidget*,	 gtk_plug_new, (GdkNativeWindow));

	// Dialog
	DEFINE_SYMBOL(gint,          gtk_dialog_run, (GtkDialog *));
	DEFINE_SYMBOL(void,          gtk_dialog_set_default_response, (GtkDialog *, gint));
	DEFINE_SYMBOL(void,          gtk_dialog_response,             (GtkDialog *, gint));
	DEFINE_SYMBOL(GtkWidget*,    gtk_message_dialog_new,  (GtkWindow *parent,
														   GtkDialogFlags flags,
														   GtkMessageType type,
														   GtkButtonsType buttons,
														   const gchar *message_format,
														   ...));

	// GetTypes
	DEFINE_SYMBOL(GType,		 gtk_container_get_type, ());
	DEFINE_SYMBOL(GTypeInstance*,g_type_check_instance_cast, (GTypeInstance*, GType));
	DEFINE_SYMBOL(gboolean,      g_type_check_instance_is_a, (GTypeInstance, GType));
	DEFINE_SYMBOL(GType, 		 g_type_fundamental,		(GType type_id));
	DEFINE_SYMBOL(GType,         gtk_file_chooser_get_type, ());
	DEFINE_SYMBOL(GType,         gtk_dialog_get_type, ());
	DEFINE_SYMBOL(GType,         gtk_file_chooser_dialog_get_type, ());
	DEFINE_SYMBOL(GType,         gtk_window_get_type, ());
	DEFINE_SYMBOL(GType,         gtk_socket_get_type, ());
	DEFINE_SYMBOL(GType,         gtk_object_get_type, ());
	DEFINE_SYMBOL(GType,         gtk_plug_get_type ,  ());
	DEFINE_SYMBOL(GType,         gtk_image_get_type, ());

	// FileChooser
	DEFINE_SYMBOL(gchar*,        gtk_file_chooser_get_filename,    (GtkFileChooser*));
	DEFINE_SYMBOL(GSList*,       gtk_file_chooser_get_filenames,   (GtkFileChooser*));
	DEFINE_SYMBOL(gboolean,      gtk_file_chooser_set_filename,    (GtkFileChooser*, const char*));
	DEFINE_SYMBOL(void,          gtk_file_chooser_set_current_name,(GtkFileChooser*, const gchar*));
	DEFINE_SYMBOL(gchar*,        gtk_file_chooser_get_current_folder,(GtkFileChooser*));
	DEFINE_SYMBOL(gboolean,      gtk_file_chooser_set_current_folder,(GtkFileChooser*, const gchar*));

	DEFINE_SYMBOL(void,          gtk_file_chooser_set_select_multiple, (GtkFileChooser*, gboolean));
	DEFINE_SYMBOL(GtkWidget*,    gtk_file_chooser_dialog_new, (const gchar*, GtkWindow*, GtkFileChooserAction, const gchar*, ...));

	DEFINE_SYMBOL(GtkFileFilter*,gtk_file_filter_new, (void));
	DEFINE_SYMBOL(void,          gtk_file_filter_set_name, (GtkFileFilter*, const gchar*));
	DEFINE_SYMBOL(const gchar*,  gtk_file_filter_get_name, (GtkFileFilter*));
	DEFINE_SYMBOL(void,          gtk_file_filter_add_pattern, (GtkFileFilter*, const gchar*));
	DEFINE_SYMBOL(void,          gtk_file_chooser_add_filter, (GtkFileChooser*, GtkFileFilter*));
	DEFINE_SYMBOL(void,          gtk_file_chooser_set_filter, (GtkFileChooser*, GtkFileFilter*));
	DEFINE_SYMBOL(GtkFileFilter*,gtk_file_chooser_get_filter, (GtkFileChooser*));
	//DEFINE_SYMBOL(void,          gtk_file_chooser_set_local_only, (GtkFileChooser*, gboolean));
	//DEFINE_SYMBOL(gchar*,        gtk_file_chooser_get_uri,    (GtkFileChooser *chooser));

	DEFINE_SYMBOL(void,          gtk_file_chooser_set_preview_widget, (GtkFileChooser *chooser,
																		GtkWidget *preview_widget));
	DEFINE_SYMBOL(gchar*,        gtk_file_chooser_get_preview_filename, (GtkFileChooser* chooser));
	DEFINE_SYMBOL(void,          gtk_file_chooser_set_preview_widget_active, (GtkFileChooser *chooser,
																			  gboolean active));
	DEFINE_SYMBOL(void,          gtk_file_chooser_set_use_preview_label, (GtkFileChooser* chooser,  gboolean use_label));


	DEFINE_SYMBOL(GtkWidget*,    gtk_image_new,                       ());
	DEFINE_SYMBOL(void,          gtk_image_set_from_file,            (GtkImage *image,
																	  const gchar *filename));

	// Window
	DEFINE_SYMBOL(void,          gtk_window_set_transient_for, (GtkWindow*, GtkWindow*));
	DEFINE_SYMBOL(void,          gtk_window_set_destroy_with_parent, (GtkWindow*, gboolean));
	DEFINE_SYMBOL(void,          gtk_window_set_modal,      (GtkWindow*, gboolean));
	DEFINE_SYMBOL(void,          gtk_window_set_icon,                 (GtkWindow *window,
																	   GdkPixbuf *icon));

	// Filechooser functions, requires version 2.8 of Gtk
	DEFINE_SYMBOL(void,          gtk_file_chooser_set_show_hidden, (GtkFileChooser*, gboolean));
	DEFINE_SYMBOL(void,          gtk_file_chooser_set_do_overwrite_confirmation,(GtkFileChooser*, gboolean));
};

void *OpGtk::Sym(void *handle, const char *symbol)
{
	void *ret = dlsym(handle, symbol);

	if(!ret)
		printf("\tOpera : Symbol resolution problem: %s\n", dlerror());

	return ret;
}


bool OpGtk::ResolveSymbols(bool load_extended)
{
	// Glib
	LOAD_SYMBOL(Sym, g_glib_handle, g_idle_add);
	LOAD_SYMBOL(Sym, g_glib_handle, g_idle_remove_by_data);
	LOAD_SYMBOL(Sym, g_glib_handle, g_timeout_add);
	LOAD_SYMBOL(Sym, g_glib_handle, g_slist_nth);
	LOAD_SYMBOL(Sym, g_glib_handle, g_slist_length);
	LOAD_SYMBOL(Sym, g_glib_handle, g_slist_free);
	LOAD_SYMBOL(Sym, g_glib_handle, g_free);
	LOAD_SYMBOL(Sym, g_glib_handle, g_io_channel_unref);
	LOAD_SYMBOL(Sym, g_glib_handle, g_io_channel_unix_new);
	LOAD_SYMBOL(Sym, g_glib_handle, g_io_add_watch);
	LOAD_SYMBOL(Sym, g_glib_handle, g_io_channel_unix_get_fd);
	LOAD_SYMBOL(Sym, g_glib_handle, g_source_remove);

	// Gobject
	LOAD_SYMBOL(Sym, g_gobject_handle, g_signal_connect_data);
	LOAD_SYMBOL(Sym, g_gobject_handle, g_object_disconnect);
	LOAD_SYMBOL(Sym, g_gobject_handle, g_object_get);
	LOAD_SYMBOL(Sym, g_gobject_handle, g_signal_handler_disconnect);

	// Gdk
	LOAD_SYMBOL(Sym, g_gdk_handle, gdk_input_add);
	LOAD_SYMBOL(Sym, g_gdk_handle, gdk_input_remove);
	LOAD_SYMBOL(Sym, g_gdk_handle, gdk_display_get_default);
	LOAD_SYMBOL(Sym, g_gdk_handle, gdk_window_object_get_type);
	LOAD_SYMBOL(Sym, g_gdk_handle, gdk_window_set_transient_for);
	LOAD_SYMBOL(Sym ,g_gdk_handle, gdk_window_set_events);
	LOAD_SYMBOL(Sym, g_gdk_handle, gdk_flush);
	LOAD_SYMBOL(Sym, g_gdk_handle, gdk_error_trap_pop);
	LOAD_SYMBOL(Sym, g_gdk_handle, gdk_error_trap_push);
	LOAD_SYMBOL(Sym, g_gdk_handle, gdk_pixbuf_new_from_xpm_data);

	// Gdkx
	LOAD_SYMBOL(Sym, g_gdk_handle, gdk_window_foreign_new);
	LOAD_SYMBOL(Sym, g_gdk_handle, gdk_x11_lookup_xdisplay);
	LOAD_SYMBOL(Sym, g_gdk_handle, gdk_x11_colormap_get_xcolormap);
	LOAD_SYMBOL(Sym, g_gdk_handle, gdk_x11_display_get_xdisplay);
	LOAD_SYMBOL(Sym, g_gdk_handle, gdk_x11_drawable_get_xdisplay);
	LOAD_SYMBOL(Sym, g_gdk_handle, gdk_x11_drawable_get_xid);
	LOAD_SYMBOL(Sym, g_gdk_handle, gdk_x11_screen_get_xscreen);
	LOAD_SYMBOL(Sym, g_gdk_handle, gdk_x11_visual_get_xvisual);

	// Gtk
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_init);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_init_check);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_main);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_main_level);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_main_quit);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_true);

	// Container
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_container_add);

	// Widget
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_widget_grab_focus);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_widget_realize);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_widget_get_parent_window);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_widget_get_parent);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_widget_set_size_request);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_widget_destroy);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_widget_destroyed);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_widget_show);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_widget_hide);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_widget_set_uposition);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_widget_get_visual);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_widget_get_colormap);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_widget_get_screen);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_widget_path);

	// Socket
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_socket_new);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_socket_get_id);

	// Plug
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_plug_new);

	// Get type
	LOAD_SYMBOL(Sym, g_gobject_handle, g_type_check_instance_cast);
	LOAD_SYMBOL(Sym, g_gobject_handle, g_type_check_instance_is_a);
	LOAD_SYMBOL(Sym, g_gobject_handle, g_type_fundamental);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_container_get_type);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_chooser_get_type);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_dialog_get_type);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_chooser_dialog_get_type);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_window_get_type);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_socket_get_type);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_object_get_type);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_plug_get_type);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_image_get_type);

	// Dialog
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_dialog_run);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_dialog_set_default_response);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_dialog_response);
	LOAD_SYMBOL(Sym,  g_gtk_handle, gtk_message_dialog_new);

	// FileChooser
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_chooser_get_filename);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_chooser_get_filenames);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_chooser_set_filename);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_chooser_set_current_name);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_chooser_get_current_folder);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_chooser_set_current_folder);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_chooser_set_select_multiple);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_chooser_dialog_new);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_filter_new);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_filter_set_name);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_filter_get_name);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_filter_add_pattern);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_chooser_add_filter);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_chooser_set_filter);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_chooser_get_filter);
// 		LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_chooser_set_local_only);
//		LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_chooser_get_uri);

	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_chooser_set_preview_widget);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_chooser_get_preview_filename);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_chooser_set_preview_widget_active);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_chooser_set_use_preview_label);

	// Image
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_image_new);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_image_set_from_file);

	// Window
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_window_set_transient_for);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_window_set_destroy_with_parent);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_window_set_modal);
	LOAD_SYMBOL(Sym, g_gtk_handle, gtk_window_set_icon);

	// Filechooser - extended (requires version 2.8 of Gtk)
	if (load_extended)
	{
		LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_chooser_set_show_hidden);
		LOAD_SYMBOL(Sym, g_gtk_handle, gtk_file_chooser_set_do_overwrite_confirmation);
	}


	const char* error_msg = dlerror();
	if (error_msg) {
		fprintf(stderr, "Error locating 'readfile' - %s\n", error_msg);
			//			exit(1);
	}

	bool result =
			g_idle_add &&
			g_idle_remove_by_data &&
			g_timeout_add &&
			g_signal_connect_data &&
			g_object_disconnect &&
			g_object_get &&
			g_signal_handler_disconnect &&
			g_slist_nth &&
			g_slist_length &&
			g_slist_free &&
			g_free &&

			// gdk
			gdk_input_add &&
			gdk_input_remove &&
			gdk_display_get_default &&
			gdk_window_object_get_type &&
			gdk_window_set_transient_for &&
			gdk_window_set_events &&
			gdk_flush &&
			gdk_error_trap_pop &&
			gdk_error_trap_push &&
			gdk_pixbuf_new_from_xpm_data &&

			// gdkx
			gdk_window_foreign_new &&
			gdk_x11_lookup_xdisplay &&
			gdk_x11_colormap_get_xcolormap &&
			gdk_x11_display_get_xdisplay &&
			gdk_x11_drawable_get_xdisplay &&
			gdk_x11_drawable_get_xid &&
			gdk_x11_screen_get_xscreen &&
			gdk_x11_visual_get_xvisual &&

			// gtk
			gtk_init &&
			gtk_init_check &&
			gtk_main &&
			gtk_main_quit &&
			gtk_true &&

			// container
			gtk_container_add &&

			// widget
			gtk_widget_grab_focus &&
			gtk_widget_destroy &&
			gtk_widget_destroyed &&
			gtk_widget_show &&
			gtk_widget_hide &&
			gtk_widget_set_uposition &&
			gtk_widget_set_size_request &&
			gtk_widget_get_visual &&
			gtk_widget_get_colormap &&
			gtk_widget_get_screen &&
			gtk_widget_path &&
			gtk_widget_realize &&
			gtk_widget_get_parent_window &&
			gtk_widget_get_parent &&

			// socket
			gtk_socket_new &&
			gtk_socket_get_id &&

			// plug
			gtk_plug_new &&

			// dialog
			gtk_dialog_run &&
			gtk_dialog_set_default_response &&
			gtk_dialog_response &&
			gtk_message_dialog_new &&

			// get_type
			g_type_check_instance_cast &&
			g_type_check_instance_is_a &&
			g_type_fundamental &&
			gtk_container_get_type &&
			gtk_file_chooser_get_type &&
			gtk_dialog_get_type &&
			gtk_file_chooser_dialog_get_type &&
			gtk_window_get_type &&
			gtk_socket_get_type &&
			gtk_object_get_type &&
			gtk_plug_get_type &&
			gtk_image_get_type &&

			// filechooser
			gtk_file_chooser_get_filename &&
			gtk_file_chooser_get_filenames &&
			gtk_file_chooser_set_filename &&
			gtk_file_chooser_set_current_name &&
			gtk_file_chooser_get_current_folder &&
			gtk_file_chooser_set_current_folder &&
			gtk_file_chooser_set_select_multiple &&
			gtk_file_filter_new &&
			gtk_file_filter_set_name &&
			gtk_file_filter_get_name &&
			gtk_file_filter_add_pattern &&
			gtk_file_chooser_add_filter &&
			gtk_file_chooser_set_filter &&
			gtk_file_chooser_get_filter &&
			gtk_file_chooser_dialog_new &&
			//gtk_file_chooser_set_local_only &&
			//gtk_file_chooser_get_uri;
			gtk_file_chooser_set_preview_widget &&
			gtk_file_chooser_get_preview_filename &&
			gtk_file_chooser_set_use_preview_label &&

			// image
			gtk_image_new &&
			gtk_image_set_from_file &&

			// window
			gtk_window_set_modal &&
			gtk_window_set_transient_for &&
			gtk_window_set_destroy_with_parent &&
			gtk_window_set_icon;

	if (load_extended)
	{
		result =
				result &&
				gtk_file_chooser_set_show_hidden &&
				gtk_file_chooser_set_do_overwrite_confirmation;
	}




	return result;
}


void * OpGtk::OpenLibrary(const char * library_name)
{
	void * handle = dlopen(library_name, RTLD_LAZY | RTLD_GLOBAL);

	if(!handle)
		printf("\tOpera : Failed to load library %s\n", library_name);

	return handle;
}


void OpGtk::CloseLibrary(void * lib_handle)
{
	if(lib_handle)
		dlclose(lib_handle);
}


bool OpGtk::LoadGtk(bool load_extended)
{
/*
	g_gtk_handle = RTLD_DEFAULT;

		// ------------------
		// See if we have the symbols already :
		// ------------------

	if (ResolveSymbols())
	{
	g_gtk_handle = 0;
	return true;
}
*/

	// ------------------
	// dlopen the library :
	// ------------------

	g_gtk_handle	 = OpenLibrary("libgtk-x11-2.0.so.0");
	g_gdk_handle	 = OpenLibrary("libgdk-x11-2.0.so.0");
	g_glib_handle	 = OpenLibrary("libglib-2.0.so.0");
	g_gobject_handle = OpenLibrary("libgobject-2.0.so.0");

	return g_gtk_handle &&
			g_gdk_handle &&
			g_glib_handle &&
			g_gobject_handle &&
			ResolveSymbols(load_extended);

}


void OpGtk::UnloadGtk()
{
	// ------------------
	// dlclose the library :
	// ------------------

	CloseLibrary(g_gtk_handle);
	CloseLibrary(g_gdk_handle);
	CloseLibrary(g_glib_handle);
	CloseLibrary(g_gobject_handle);

	g_gtk_handle     = 0;
	g_gdk_handle     = 0;
	g_glib_handle    = 0;
	g_gobject_handle = 0;

}

