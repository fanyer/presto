/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2009-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch_system_includes.h"

#ifdef MEDIA_BACKEND_GSTREAMER
#include "platforms/media_backends/media_backends_module.h"
#include "platforms/media_backends/gst/gstmediamanager.h"
#include "platforms/media_backends/gst/gstoperasrc.h"
#include "platforms/media_backends/gst/gstoperavideosink.h"
#include "modules/util/opfile/opfile.h"

#include "platforms/media_backends/gst/gstlibvpx.h"

#define PACKAGE "gstmediaplayer"
#define PLUGIN_NAME "opera"
#define PLUGIN_DESCRIPTION "Opera GStreamer elements"
#define PLUGIN_VERSION "0.0.1"
#define PLUGIN_LICENSE "Proprietary"
#define PLUGIN_SOURCE PACKAGE
#define PLUGIN_PACKAGE "Opera"
#define PLUGIN_ORIGIN "http://www.opera.com/"

// Introduced in 0.10.18, but we need it for earlier releases
#ifndef GST_CHECK_VERSION
#define GST_CHECK_VERSION(major,minor,micro)							\
	(GST_VERSION_MAJOR > (major) ||										\
	 (GST_VERSION_MAJOR == (major) && GST_VERSION_MINOR > (minor)) ||	\
	 GST_VERSION_MAJOR == (major) && GST_VERSION_MINOR == (minor) &&	\
	 GST_VERSION_MICRO >= (micro)))
#endif

GST_DEBUG_CATEGORY (gst_opera_debug);
#define GST_CAT_DEFAULT gst_opera_debug

#if defined(G_OS_WIN32) && !defined(GST_DISABLE_GST_DEBUG)
static void WinDebugPrintFunc(const gchar *s)
{
	dbg_printf(s);
}
#endif // defined(G_OS_WIN32) && !defined(GST_DISABLE_GST_DEBUG)

static gboolean
gst_opera_plugin_init (GstPlugin *plugin)
{
	if (!gst_element_register(plugin, "operasrc",
							  GST_RANK_NONE, GST_TYPE_OPERASRC))
		return FALSE;

	if (!gst_element_register(plugin, "operavideosink",
							  GST_RANK_NONE, GST_TYPE_OPERAVIDEOSINK))
		return FALSE;

  return TRUE;
}

#if !GST_CHECK_VERSION(0,10,16)
GST_PLUGIN_DEFINE_STATIC(
	GST_VERSION_MAJOR,
	GST_VERSION_MINOR,
	PLUGIN_NAME,
	PLUGIN_DESCRIPTION,
	gst_opera_plugin_init,
	PLUGIN_VERSION,
	PLUGIN_LICENSE,
	PLUGIN_PACKAGE,
	PLUGIN_ORIGIN)
#endif

#ifdef MEDIA_BACKEND_GSTREAMER_BUNDLE_PLUGINS

// Type for libvpx init functions.
typedef void (*libvpx_init_fptr)(void *p);

/** Initialize a libvpx symbol in a GStreamer plugin.
 *
 * @param module The plugin to initialize the symbol on.
 * @param symname The name of the symbol.
 * @param sym The symbol (function pointer) to initialize with.
 * @return TRUE on success, FALSE if symbol was not found.
 */
static gboolean
InitVPXSymbol(GModule *module, const char *symname, void *sym)
{
	gpointer symbol;
	gboolean ok = g_module_symbol(module, symname, &symbol);
	OP_ASSERT(ok || !"No libvpx init function for that symbol.");
	if (ok)
		((libvpx_init_fptr)symbol)(sym);
	return ok;
}

/** Link a GStreamer plugin with libvpx.
 *
 * The VPX plugin bundled with Opera is not linked with libvpx, but expose
 * functions for initializing the libvpx-calls it needs. This function
 * makes this initialization.
 *
 * @param module The plugin to link with libvpx.
 * @return OpStatus::OK on success, or OpStatus::ERR if the plugin did not
 *         expose all of the expected symbols.
 */
static OP_STATUS
InitVPXDecoder(GModule *module)
{
	#define INIT_LIBVPX_SYMBOL(SYM) \
		do { \
			if (!InitVPXSymbol(module, "libvpx_init_" #SYM, ft.SYM)) \
				return OpStatus::ERR; \
		} while (0)

	gstlibvpx_ft ft = gstlibvpx_function_table();

	INIT_LIBVPX_SYMBOL(vpx_codec_destroy);
	INIT_LIBVPX_SYMBOL(vpx_img_free);
	INIT_LIBVPX_SYMBOL(vpx_codec_get_frame);
	INIT_LIBVPX_SYMBOL(vpx_codec_decode);
	INIT_LIBVPX_SYMBOL(vpx_codec_dec_init_ver);
	INIT_LIBVPX_SYMBOL(vpx_codec_get_caps);
	INIT_LIBVPX_SYMBOL(vpx_codec_peek_stream_info);
	INIT_LIBVPX_SYMBOL(vpx_codec_control_);
	INIT_LIBVPX_SYMBOL(vpx_codec_vp8_dx);

	#undef INIT_LIBVPX_SYMBOL

	return OpStatus::OK;
}

/** Load a bundled GStreamer plugin.
 *
 * Attempts to load and register the GStreamer plugin at the specified path.
 * If the plugin is a bundled VPX decoder, the plugin will be manually bound
 * to the libvpx linked with Opera.
 *
 * @param path The full path the GStreamer plugin.
 * @return OpStatus::OK if the plugin loaded and registered successfully;
 *         OpStatus::ERR if the plugin could not be loaded or registered;
 *         OpStatus::ERR_NO_MEMORY if we run out of memory.
 */
static OP_STATUS
LoadBundled(const uni_char *path)
{
	OpString8 utf8_path;
	RETURN_IF_ERROR(utf8_path.SetUTF8FromUTF16(path));

	GModule *module = g_module_open(utf8_path.CStr(), G_MODULE_BIND_LOCAL);

	if (!module)
		return OpStatus::ERR; // Not a dynamic library.

	gpointer symbol;

	if (!g_module_symbol(module, "gst_plugin_desc", &symbol))
	{
		g_module_close(module);
		return OpStatus::ERR; // Not a GStreamer plugin.
	}

	// This makes the dynamic module exist long enough to *not* crash on shutdown.
	g_module_make_resident(module);

	GstPluginDesc *desc = (GstPluginDesc*)symbol;

	// Bind with libvpx if we are loading a VPX-decoding plugin.
	if (op_strcmp(desc->name, "webmdec") == 0 || op_strcmp(desc->name, "opera_vp8") == 0)
		RETURN_IF_ERROR(InitVPXDecoder(module));

	gboolean ok = gst_plugin_register_static(
		desc->major_version,
		desc->minor_version,
		desc->name,
		desc->description,
		desc->plugin_init,
		desc->version,
		desc->license,
		desc->source,
		desc->package,
		desc->origin);

	return ok ? OpStatus::OK : OpStatus::ERR;
}

/** Load all bundled GStreamer plugins.
 *
 * This function gathers all plugins in the GStreamer plugin folder, and
 * attempts to load and register each one with GStreamer.
 *
 * If one (or more) of the plugins couldn't be loaded successfully, for
 * instance due to not exposing the required symbols, the function will
 * still attempt to load remaining plugins.
 *
 * @return OpStatus::OK on success (though some plugins may have failed);
 *         or OpStatus::ERR_NO_MEMORY if we ran out of memory.
 */
static OP_STATUS
LoadAllBundled()
{
	OpFile plugindir;
	RETURN_IF_ERROR(plugindir.Construct(UNI_L("plugins"), OPFILE_GSTREAMER_FOLDER));

	OpAutoPtr<OpFolderLister> lister(OpFile::GetFolderLister(OPFILE_ABSOLUTE_FOLDER, UNI_L("*gst*"), plugindir.GetFullPath()));
	RETURN_OOM_IF_NULL(lister.get());

	while (lister->Next())
		RETURN_IF_MEMORY_ERROR(LoadBundled(lister->GetFullPath()));

	return OpStatus::OK;
}

#endif // MEDIA_BACKEND_GSTREAMER_BUNDLE_PLUGINS

static OP_STATUS GstInitOnce()
{
#ifdef MEDIA_BACKEND_GSTREAMER_USE_OPDLL
	RETURN_IF_ERROR(GstLibsInit());
	// the GStreamer libraries have been loaded and are safe to use.
#endif // MEDIA_BACKEND_GSTREAMER_USE_OPDLL

#if defined(G_OS_WIN32) && !defined(GST_DISABLE_GST_DEBUG)
	// redirect GLib "stdout" and "stderr" for Windows debugging
	g_set_print_handler(WinDebugPrintFunc);
	g_set_printerr_handler(WinDebugPrintFunc);
#endif // defined(G_OS_WIN32) && !defined(GST_DISABLE_GST_DEBUG)

	GError *err;
	if (!gst_init_check(0, NULL, &err))
		return OpStatus::ERR;

#ifdef MEDIA_BACKEND_GSTREAMER_BUNDLE_PLUGINS
	RETURN_IF_ERROR(LoadAllBundled());
#endif // MEDIA_BACKEND_GSTREAMER_BUNDLE_PLUGINS

#if GST_CHECK_VERSION(0,10,16)
	// register operasrc and operavideosink
	if (!gst_plugin_register_static(
			GST_VERSION_MAJOR,
			GST_VERSION_MINOR,
			(gchar *) PLUGIN_NAME,
			(gchar *) PLUGIN_DESCRIPTION,
			gst_opera_plugin_init,
			(gchar *) PLUGIN_VERSION,
			(gchar *) PLUGIN_LICENSE,
			(gchar *) PLUGIN_SOURCE,
			(gchar *) PLUGIN_PACKAGE,
			(gchar *) PLUGIN_ORIGIN))
	{
		return OpStatus::ERR;
	}
#endif

	// register GstOperaBuffer now, later it may not be thread-safe
	g_type_class_ref(GST_TYPE_OPERABUFFER);

	GST_DEBUG_CATEGORY_INIT(gst_opera_debug, "opera", 0, "Opera");

	// initialize riff library
	gst_riff_init();

	return OpStatus::OK;
}

static OP_STATUS GstInit()
{
	static BOOL called = FALSE;
	static OP_STATUS status = OpStatus::OK;

	if (!called)
	{
		called = TRUE;
		status = GstInitOnce();
	}

	return status;
}

static gboolean
demuxer_decoder_filter(GstPluginFeature *feature, gpointer use_data)
{
	if (GST_IS_ELEMENT_FACTORY(feature))
	{
		GstElementFactory *factory = GST_ELEMENT_FACTORY_CAST(feature);
		const gchar *klass = gst_element_factory_get_klass(factory);
		if (op_strstr(klass, "Demuxer") == NULL &&
			op_strstr(klass, "Decoder") == NULL)
		{
			GST_LOG("rejecting '%s' for demuxer/decoder",
					gst_element_factory_get_longname(factory));

			return FALSE;
		}
		GST_LOG("accepting '%s' for demuxer/decoder",
				gst_element_factory_get_longname(factory));
		return TRUE;
	}
	return FALSE;
}

#define CODECS_RIFF_WAVE 0x01 // base 16 integer
#define CODECS_VORBIS    0x02 // "vorbis"
#define CODECS_THEORA    0x04 // "theora"
#define CODECS_VP8       0x08 // "vp8" or "vp8.0"

static BOOL
gst_op_map_type(const char *type, const char *&caps_type, UINT32 &codec_flags)
{
	const char *subtype;
	BOOL is_audio = FALSE;
	BOOL is_video = FALSE;

	if (op_strncmp(type, "application/", 12) == 0)
	{
		subtype = type + 12;
	}
	else if (op_strncmp(type, "audio/", 6) == 0)
	{
		is_audio = TRUE;
		subtype = type + 6;
	}
	else if (op_strncmp(type, "video/", 6) == 0)
	{
		is_video = TRUE;
		subtype = type + 6;
	}
	else
		return FALSE;

	if (op_strcmp(subtype, "ogg") == 0)
	{
		// Ogg
		caps_type = "application/ogg";
		codec_flags = CODECS_VORBIS | CODECS_THEORA;
	}
	else if ((is_audio || is_video) && op_strcmp(subtype, "webm") == 0)
	{
		// WebM
		caps_type = "video/webm";
		codec_flags = CODECS_VORBIS | CODECS_VP8;
	}
	else if (is_audio && (op_strcmp(subtype, "wav") == 0 ||
						  op_strcmp(subtype, "wave") == 0 ||
						  op_strcmp(subtype, "x-wav") == 0))
	{
		// RIFF WAVE
		caps_type = "audio/x-wav";
		codec_flags = CODECS_RIFF_WAVE;
	}
	else
		return FALSE;

	return TRUE;
}

/* YES: the codec is definetely supported (e.g. PCM audio)
 * MAYBE: support depends on the returned caps, if any
 * NO: the codec isn't supported (given the flags)
 */
static BOOL3
gst_op_can_play_codec(GstCaps *&caps, const char *codec, UINT32 flags)
{
	caps = NULL;

	if (flags & CODECS_RIFF_WAVE)
	{
		// wav_fmt must be guint16
		char *endptr;
		long wav_fmt = op_strtol(codec, &endptr, 16);
		if (*endptr != '\0' /* string not consumed */ ||
			wav_fmt < 0 || wav_fmt > 0xffff /* not guint16 */)
		{
			// invalid codec string
			return NO;
		}
		switch(wav_fmt)
		{
		case GST_RIFF_WAVE_FORMAT_UNKNOWN:
			// impossible to play this
			return NO;
		case GST_RIFF_WAVE_FORMAT_PCM:
		case GST_RIFF_WAVE_FORMAT_IEEE_FLOAT:
			// supported if audio/x-wav itself is
			return YES;
		default:
			{
				GstCaps *wav_caps = gst_riff_create_audio_caps((guint16)wav_fmt,
															   NULL, NULL, NULL, NULL, NULL);
				// Note: wav_caps may be NULL here, which means NO
				caps = wav_caps;
			}
		}
	}
	else if (flags & CODECS_VORBIS && op_strcmp(codec, "vorbis") == 0)
		caps = gst_caps_new_simple("audio/x-vorbis", NULL);
	else if (flags & CODECS_THEORA && op_strcmp(codec, "theora") == 0)
		caps = gst_caps_new_simple("video/x-theora", NULL);
	else if (flags & CODECS_VP8 && (op_strcmp(codec, "vp8") == 0 ||
									op_strcmp(codec, "vp8.0") == 0))
		caps = gst_caps_new_simple("video/x-vp8", NULL);

	return caps ? MAYBE : NO;
}

const char*
GstMapType(const char* type)
{
	const char* caps_type;
	UINT32 codec_flags; // ignored
	if (gst_op_map_type(type, caps_type, codec_flags))
		return caps_type;
	return NULL;
}

/* virtual */ BOOL3
GstMediaManager::CanPlayType(const OpStringC8& type, const OpVector<OpStringC8>& codecs)
{
	// Checking for support is simple enough: first, we convert the main
	// type and codecs into GstCaps, then we walk through the list of
	// GstElementFactory and see if we can find a factory to sink each
	// GstCaps. Typically, this yields a YES/NO answer. However, if
	// the supported codecs are unknown or none are given, return MAYBE.

	// this is the list of GstCaps we need
	GSList *caps_list = NULL;

	// caps type suitable for gst_caps_new_simple
	const char *caps_type;

	// bitwise OR of the codecs to check for this format.
	UINT32 codec_flags;

	if (gst_op_map_type(type.CStr(), caps_type, codec_flags))
	{
		OP_ASSERT(caps_type && codec_flags);
		caps_list = g_slist_prepend(caps_list, gst_caps_new_simple(caps_type, NULL));
	}
	else
	{
		// For unknown types, return NO, even though technically, it's
		// possible that we could play it on UNIX. The problem with
		// defaulting to MAYBE is that basically anything works, so we
		// contribute to very lax handling of media types.
		return NO;
	}

	// At this point we have a recognized media type and need to make
	// GstCaps from the codecs. MAYBE means that the end result
	// depends on checking the caps list against the registry.
	BOOL3 can_play = MAYBE;

	for (UINT32 i = 0; i < codecs.GetCount(); i++)
	{
		const char *codec = codecs.Get(i)->CStr();
		GstCaps *codec_caps;
		BOOL3 can_play_codec =
			gst_op_can_play_codec(codec_caps, codec, codec_flags);
		if (can_play_codec == MAYBE)
		{
			if (codec_caps)
				caps_list = g_slist_prepend(caps_list, codec_caps);
		}
		else
		{
			OP_ASSERT(!codec_caps);
			// A single NO is all it takes.
			if (can_play_codec == NO)
			{
				can_play = NO;
				break;
			}
		}
	}

	// can_play is now NO if there were definitely unsupported
	// codecs. Otherwise, it is MAYBE, and we need to check the
	// registry for the final answer.
	OP_ASSERT (can_play == NO || can_play == MAYBE);

	if (can_play == MAYBE)
	{
		GList *feature_list = gst_registry_feature_filter(gst_registry_get_default(),
														  demuxer_decoder_filter, FALSE, NULL);
		for (GList *feature_iter = feature_list;
			feature_iter && caps_list;
			feature_iter = feature_iter->next)
		{
			GstElementFactory *factory = GST_ELEMENT_FACTORY(feature_iter->data);
			for (const GList *templ_iter = gst_element_factory_get_static_pad_templates(factory);
				 templ_iter && caps_list;
				 templ_iter = templ_iter->next)
			{
				GstStaticPadTemplate *templ = (GstStaticPadTemplate *)templ_iter->data;
				// only consider sink pad templates
				if (templ->direction != GST_PAD_SINK)
					continue;

				GstCaps *sink_caps = gst_static_pad_template_get_caps(templ);
				// ignore "any" caps (e.g. decodebin has this)
				if (!gst_caps_is_any(sink_caps))
				{
					GSList *caps_iter = caps_list;
					while (caps_iter)
					{
						GSList *caps_next = caps_iter->next; // so that caps_iter can be deleted below
						GstCaps *src_caps = GST_CAPS(caps_iter->data);

						GST_LOG("testing if %s sink caps %" GST_PTR_FORMAT
								"is compatible with src caps %" GST_PTR_FORMAT,
								gst_element_factory_get_longname(factory), sink_caps, src_caps);

						// using subset doesn't work for some caps created by
						// the riff library, use intersection instead.
						GstCaps *caps = gst_caps_intersect(src_caps, sink_caps);
						gboolean compat = !gst_caps_is_empty(caps);
						gst_caps_unref(caps);

						if (compat)
						{
							GST_LOG("caps are compatible, marking type/codec as available");
							gst_caps_unref(src_caps);
							caps_list = g_slist_delete_link(caps_list, caps_iter);
						}

						caps_iter = caps_next;
					}
				}
				gst_caps_unref(sink_caps);
			}
		}
		gst_plugin_feature_list_free(feature_list);
	}

	if (caps_list || can_play == NO)
	{
		// some codec was not supported
		while (caps_list)
		{
			GstCaps *unsupported = GST_CAPS(caps_list->data);
			GST_LOG("caps %" GST_PTR_FORMAT " not supported", unsupported);
			gst_caps_unref(unsupported);
			caps_list = g_slist_delete_link(caps_list, caps_list);
		}
		return NO;
	}

	OP_ASSERT(can_play == MAYBE);

	// We need codecs to say YES.
	if (codecs.GetCount() == 0)
		return MAYBE;

	return YES;
}

/* virtual */ BOOL
GstMediaManager::CanPlayURL(const uni_char* url)
{
	// support for e.g. rtsp:// or other schemes for which
	// GStreamer has its own source elements can be
	// added here.
	return FALSE;
}

/* virtual */ OP_STATUS
GstMediaManager::CreatePlayer(OpMediaPlayer** player, OpMediaHandle handle)
{
	if (!player)
		return OpStatus::ERR_NULL_POINTER;

	OpMediaSource* src;
	RETURN_IF_ERROR(OpMediaSource::Create(&src, handle));
	*player = OP_NEW(GstMediaPlayer, (src));
	if (!*player)
	{
		OP_DELETE(src);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
GstMediaManager::CreatePlayer(OpMediaPlayer** player, OpMediaHandle handle, const uni_char* url)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

/* virtual */ void
GstMediaManager::DestroyPlayer(OpMediaPlayer* player)
{
	OP_DELETE(static_cast<GstMediaPlayer*>(player));
}

/* static */ OP_STATUS
OpMediaManager::Create(OpMediaManager** manager)
{
	if (!manager)
		return OpStatus::ERR_NULL_POINTER;

	// create the thread manager up front so that it is available
	// when the GstMediaPlayer instances need it.
	RETURN_IF_ERROR(GstThreadManager::Init());

	RETURN_IF_ERROR(GstInit());

	if (!(*manager = OP_NEW(GstMediaManager, ())))
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

/* static */ OP_STATUS
GstThreadManager::Init()
{
	if (!g_gst_thread_manager)
	{
		OpAutoPtr<GstThreadManager> manager(OP_NEW(GstThreadManager, ()));
		if (!manager.get())
			return OpStatus::ERR_NO_MEMORY;
		RETURN_IF_ERROR(g_main_message_handler->SetCallBack(manager.get(), MSG_MEDIA_BACKENDS_GST_THREAD_MANAGER, 0, 0));
		g_gst_thread_manager = manager.release();
	}
	return OpStatus::OK;
}

/* virtual */ GstThreadManager::~GstThreadManager()
{
	OP_ASSERT(m_running.Cardinal() == 0);
	OP_ASSERT(m_pending.Cardinal() == 0);
	g_main_message_handler->UnsetCallBacks(this);
}

void
GstThreadManager::Queue(GstMediaPlayer* player)
{
	player->Into(&m_pending);
	StartPending();
}

void
GstThreadManager::StartPending()
{
	for (GstMediaPlayer* player = m_pending.First();
		 player && m_running.Cardinal() < MEDIA_BACKEND_GSTREAMER_MAX_THREADS;
		 player = player->Suc())
	{
		OP_ASSERT(player->m_thread == NULL);
		player->Out();
		player->m_thread = g_thread_create(GstMediaPlayer::ThreadFunc, player, TRUE, NULL);
		if (player->m_thread)
		{
			player->Into(&m_running);
		}
		else
		{
			OP_ASSERT(!"failed to start thread");
			player->Into(&m_pending);
			break;
		}
	}

	GST_INFO("GstThreadManager: %d max, %d running, %d pending",
			 MEDIA_BACKEND_GSTREAMER_MAX_THREADS,
			 m_running.Cardinal(), m_pending.Cardinal());

	g_main_message_handler->RemoveDelayedMessage(MSG_MEDIA_BACKENDS_GST_THREAD_MANAGER, 0, 0);
}

void
GstThreadManager::Stop(GstMediaPlayer* player)
{
	// not in list if EnsurePipeline failed
	if (player->m_thread)
	{
		OP_ASSERT(m_running.HasLink(player));
		g_thread_join(player->m_thread);
		player->m_thread = NULL;
		player->Out();
		if (m_pending.Cardinal() > 0 &&
			m_running.Cardinal() < MEDIA_BACKEND_GSTREAMER_MAX_THREADS)
		{
			g_main_message_handler->PostMessage(MSG_MEDIA_BACKENDS_GST_THREAD_MANAGER, 0, 0);
		}
	}
	else if (player->InList())
	{
		OP_ASSERT(m_pending.HasLink(player));
		player->Out();
	}

	GST_INFO("GstThreadManager: %d max, %d running, %d pending",
			 MEDIA_BACKEND_GSTREAMER_MAX_THREADS,
			 m_running.Cardinal(), m_pending.Cardinal());
}

/* virtual */ void
GstThreadManager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(msg == MSG_MEDIA_BACKENDS_GST_THREAD_MANAGER);
	StartPending();
}

#endif // MEDIA_BACKEND_GSTREAMER
