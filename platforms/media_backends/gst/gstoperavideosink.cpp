/* -*- Mode: c; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 2009-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch_system_includes.h"

#ifdef MEDIA_BACKEND_GSTREAMER

#include "platforms/media_backends/gst/gstoperavideosink.h"

GST_DEBUG_CATEGORY_EXTERN (gst_opera_debug);
#define GST_CAT_DEFAULT gst_opera_debug

GST_BOILERPLATE (GstOperaVideoSink, gst_operavideosink, GstVideoSink, GST_TYPE_VIDEO_SINK);

static const GstElementDetails gst_operavideosink_details =
GST_ELEMENT_DETAILS (
  (gchar *) "Opera video sink",
  (gchar *) "Sink/Video",
  (gchar *) "Video sink for Opera",
  (gchar *) "Opera Software ASA"
);

/* Sink pad template. OpBitmap uses ARGB pixel format. */
static GstStaticPadTemplate sink_factory =
GST_STATIC_PAD_TEMPLATE (
  "sink",
  GST_PAD_SINK,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS(GST_VIDEO_CAPS_xRGB_HOST_ENDIAN)
);

enum
{
  ARG_0,
  ARG_LAST_FRAME
};

static void gst_operavideosink_finalize (GObject *object);
static void gst_operavideosink_get_property (GObject *object, guint prop_id,
                                             GValue *value, GParamSpec *pspec);
static GstCaps *gst_operavideosink_get_caps (GstBaseSink *bsink);
static gboolean gst_operavideosink_set_caps (GstBaseSink *bsink, GstCaps *caps);
static GstFlowReturn gst_operavideosink_buffer_alloc(GstBaseSink *bsink,
                                                     guint64 offset, guint size,
                                                     GstCaps *caps, GstBuffer **buf);
static gboolean gst_operavideosink_start (GstBaseSink *bsink);
static gboolean gst_operavideosink_stop (GstBaseSink *bsink);
static GstFlowReturn gst_operavideosink_have_frame (GstBaseSink *bsink, GstBuffer *buf);

static GstOperaBuffer *gst_operabuffer_new (GstOperaVideoSink *sink);

static void
gst_operavideosink_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details (element_class, &gst_operavideosink_details);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}

/* initialize the plugin's class */
static void
gst_operavideosink_class_init (GstOperaVideoSinkClass *klass)
{
  GObjectClass *gobject_class;
  GstBaseSinkClass *gstbasesink_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstbasesink_class = GST_BASE_SINK_CLASS (klass);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_operavideosink_finalize);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_operavideosink_get_property);

  g_object_class_install_property (gobject_class, ARG_LAST_FRAME,
      gst_param_spec_mini_object ("last-frame", "", "", GST_TYPE_BUFFER,
                                  G_PARAM_READABLE));

  gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_operavideosink_get_caps);
  gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_operavideosink_set_caps);
  gstbasesink_class->buffer_alloc = GST_DEBUG_FUNCPTR (gst_operavideosink_buffer_alloc);
  gstbasesink_class->start = GST_DEBUG_FUNCPTR (gst_operavideosink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_operavideosink_stop);
  gstbasesink_class->preroll = GST_DEBUG_FUNCPTR (gst_operavideosink_have_frame);
  gstbasesink_class->render = GST_DEBUG_FUNCPTR (gst_operavideosink_have_frame);

  parent_class = GST_VIDEO_SINK_CLASS (g_type_class_peek_parent (klass));
}

/* initialize the new element */
static void
gst_operavideosink_init (GstOperaVideoSink *opsink,
                         GstOperaVideoSinkClass *g_class)
{
  opsink->running = FALSE;
  opsink->pool_lock = g_mutex_new();
  opsink->buffer_pool = NULL;
  opsink->last_frame = NULL;
}

static void
gst_operavideosink_finalize (GObject *object)
{
  GstOperaVideoSink *opsink = GST_OPERAVIDEOSINK (object);

  g_mutex_free (opsink->pool_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_operavideosink_get_property (GObject *object, guint prop_id,
                                 GValue *value, GParamSpec *pspec)
{
  GstOperaVideoSink *opsink = GST_OPERAVIDEOSINK (object);

  if (prop_id == ARG_LAST_FRAME) {
    GstBuffer *last_frame = NULL;
    gst_buffer_replace (&last_frame, opsink->last_frame);
    gst_value_take_buffer (value, last_frame);
  }
}

/* GstBaseSink vmethod implementations */

/* Called in the beginning of caps negotiation to check which caps
 * this sink can accept. Reply with the Opera pixel format.
 */
static GstCaps *
gst_operavideosink_get_caps (GstBaseSink *bsink)
{
  GstOperaVideoSink *opsink = GST_OPERAVIDEOSINK (bsink);
  return gst_caps_copy (gst_pad_get_pad_template_caps (GST_VIDEO_SINK_PAD (opsink)));
}

/* Called when the peer element has decided on a caps to configure the
 * sink element to the agreed format. We don't need to do anything.
 */
static gboolean
gst_operavideosink_set_caps (GstBaseSink *bsink, GstCaps *caps)
{
  GST_LOG_OBJECT (bsink, "setting caps %" GST_PTR_FORMAT, caps);
  return TRUE;
}

/* Allocate a buffer for upstream element from buffer pool.
 */
static GstFlowReturn
gst_operavideosink_buffer_alloc(GstBaseSink *bsink,
                                guint64 offset, guint size,
                                GstCaps *caps, GstBuffer **buf)
{
  GstOperaVideoSink *opsink = GST_OPERAVIDEOSINK (bsink);
  GstOperaBuffer *opbuf = NULL;

  if (G_UNLIKELY (!gst_caps_is_fixed(caps)))
    return GST_FLOW_ERROR;

  GST_LOG_OBJECT (opsink, "requested buffer with caps %" GST_PTR_FORMAT, caps);

  g_mutex_lock (opsink->pool_lock);
  if (G_LIKELY (opsink->buffer_pool)) {
    GST_LOG_OBJECT (opsink, "reusing buffer from pool");
    opbuf = GST_OPERABUFFER (opsink->buffer_pool->data);
    opsink->buffer_pool = g_slist_delete_link (opsink->buffer_pool, opsink->buffer_pool);
  }
  g_mutex_unlock (opsink->pool_lock);

  if (G_UNLIKELY (opbuf == NULL)) {
    GST_LOG_OBJECT (opsink, "creating new buffer");
    opbuf = gst_operabuffer_new (opsink);
  }
  if (G_UNLIKELY (GST_BUFFER_SIZE (opbuf) != size)) {
    GST_LOG_OBJECT (opsink, "(re)allocating buffer data");
    GST_BUFFER_DATA (opbuf) = (guint8 *) g_realloc (GST_BUFFER_DATA (opbuf), size);
    GST_BUFFER_SIZE (opbuf) = size;
  }

  gst_buffer_set_caps (GST_BUFFER (opbuf), caps);

  GST_LOG_OBJECT (opsink, "returning buffer with caps %" GST_PTR_FORMAT,
                  GST_BUFFER_CAPS (opbuf));

  *buf = GST_BUFFER (opbuf);

  return GST_FLOW_OK;
}

/* Allocate resources needed at playback. */
static gboolean
gst_operavideosink_start (GstBaseSink *bsink)
{
  GstOperaVideoSink *opsink = GST_OPERAVIDEOSINK (bsink);
  GST_LOG_OBJECT(opsink, "starting");

  GST_OBJECT_LOCK (opsink);
  opsink->running = TRUE;
  GST_OBJECT_UNLOCK (opsink);

  return TRUE;
}

/* Free resources needed at playback. */
static gboolean
gst_operavideosink_stop (GstBaseSink *bsink)
{
  GstOperaVideoSink *opsink = GST_OPERAVIDEOSINK (bsink);
  GST_LOG_OBJECT(opsink, "stopping");

  GST_OBJECT_LOCK (opsink);
  opsink->running = FALSE;
  GST_OBJECT_UNLOCK (opsink);

  /* empty the buffer pool */
  g_mutex_lock (opsink->pool_lock);
  while (opsink->buffer_pool) {
    GstOperaBuffer *buf = GST_OPERABUFFER (opsink->buffer_pool->data);
    opsink->buffer_pool = g_slist_delete_link (opsink->buffer_pool, opsink->buffer_pool);
    gst_buffer_unref (GST_BUFFER (buf));
  }
  g_mutex_unlock (opsink->pool_lock);

  /* unref the last frame */
  gst_buffer_replace(&opsink->last_frame, NULL);

  return TRUE;
}

/* Send the frame over the message bus for Opera to
 * handle in the main thread.
 */
static GstFlowReturn
gst_operavideosink_have_frame (GstBaseSink *bsink, GstBuffer *buf)
{
  GstOperaVideoSink *opsink = GST_OPERAVIDEOSINK (bsink);
  GstMessage *msg;

  GST_LOG_OBJECT (opsink, "have video frame with caps %" GST_PTR_FORMAT,
                  GST_BUFFER_CAPS (buf));

  gst_buffer_replace (&opsink->last_frame, buf);

  /* post message on bus to notify Opera of new frame */
  msg = gst_message_new_application (GST_OBJECT (opsink),
                                     gst_structure_new ("have-frame", NULL));
  gst_element_post_message (GST_ELEMENT (opsink), msg);

  return GST_FLOW_OK;
}

/* GstOperaBuffer is used to provide a buffer pool */

static GstOperaBuffer *
gst_operabuffer_new (GstOperaVideoSink *sink)
{
  GstOperaBuffer *buf = (GstOperaBuffer *) gst_mini_object_new(GST_TYPE_OPERABUFFER);
  buf->sink = (GstOperaVideoSink *) gst_object_ref (sink);
  return buf;
}

static void
gst_operabuffer_finalize (GstMiniObject *obj)
{
  GstOperaBuffer *buf = GST_OPERABUFFER (obj);
  GstOperaVideoSink *sink = buf->sink;
  gboolean running;

  GST_OBJECT_LOCK (sink);
  running = sink->running;
  GST_OBJECT_UNLOCK (sink);

  if (G_LIKELY (running)) {
    g_mutex_lock (sink->pool_lock);
    GST_LOG_OBJECT (sink, "returning buffer to pool");
    gst_buffer_ref (GST_BUFFER (buf));
    sink->buffer_pool = g_slist_prepend (sink->buffer_pool, buf);
    g_mutex_unlock (sink->pool_lock);
  } else {
    GST_LOG_OBJECT (sink, "freeing buffer");
    gst_object_unref (sink);
    g_free (GST_BUFFER_DATA (buf));
  }
}

static void
gst_operabuffer_class_init (gpointer g_class,
                            gpointer class_data)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (g_class);
  mini_object_class->finalize = GST_DEBUG_FUNCPTR (gst_operabuffer_finalize);
}

GType
gst_operabuffer_get_type (void)
{
  static GType gst_operabuffer_type = 0;

  if (G_UNLIKELY (gst_operabuffer_type == 0)) {
    static const GTypeInfo gst_operabuffer_info = {
      sizeof(GstBufferClass),
      NULL,
      NULL,
      gst_operabuffer_class_init,
      NULL,
      NULL,
      sizeof(GstOperaBuffer),
      0,
      NULL,
      NULL
    };
    gst_operabuffer_type = \
      g_type_register_static(GST_TYPE_BUFFER, "GstOperaBuffer", &gst_operabuffer_info, (GTypeFlags) 0);
  }

  return gst_operabuffer_type;
}

#endif /* MEDIA_BACKEND_GSTREAMER */
