/* -*- Mode: c; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 2009-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch_system_includes.h"

#ifdef MEDIA_BACKEND_GSTREAMER

#include "platforms/media_backends/gst/gstoperasrc.h"
#include "modules/pi/OpSystemInfo.h"

/* queue should be sufficiently long so that we don't have to
 * block waiting for Opera on every read. */
#define GST_OPERASRC_QUEUE_LENGTH 8

GST_DEBUG_CATEGORY_EXTERN (gst_opera_debug);
#define GST_CAT_DEFAULT gst_opera_debug

GST_BOILERPLATE (GstOperaSrc, gst_operasrc, GstBaseSrc, GST_TYPE_BASE_SRC);

static const GstElementDetails gst_operasrc_details =
GST_ELEMENT_DETAILS (
  (gchar *) "Opera source",
  (gchar *) "Source",
  (gchar *) "Source for Opera",
  (gchar *) "Opera Software ASA"
);

static GstStaticPadTemplate src_factory =
GST_STATIC_PAD_TEMPLATE (
  "src",
  GST_PAD_SRC,
  GST_PAD_ALWAYS,
  GST_STATIC_CAPS_ANY
);

static void gst_operasrc_finalize (GObject *object);
static gboolean gst_operasrc_start (GstBaseSrc *bsrc);
static gboolean gst_operasrc_stop (GstBaseSrc *bsrc);
static gboolean gst_operasrc_get_size (GstBaseSrc *bsrc, guint64 *size);
static gboolean gst_operasrc_is_seekable (GstBaseSrc *bsrc);
GstFlowReturn gst_operasrc_create (GstBaseSrc *bsrc, guint64 offset, guint size, GstBuffer **buf);
static gboolean gst_operasrc_do_seek (GstBaseSrc *bsrc, GstSegment *segment);
static gboolean gst_operasrc_check_get_range (GstBaseSrc *bsrc);
static gboolean gst_operasrc_unlock (GstBaseSrc *bsrc);
static gboolean gst_operasrc_unlock_stop (GstBaseSrc *bsrc);

/* value indicating unknown stream size */
#define NO_SIZE ((guint64)-1)

static void
gst_operasrc_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details (element_class, &gst_operasrc_details);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
}

/* initialize the plugin's class */
static void
gst_operasrc_class_init (GstOperaSrcClass *klass)
{
  GObjectClass *gobject_class;
  GstBaseSrcClass *gstbasesrc_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstbasesrc_class = GST_BASE_SRC_CLASS (klass);

  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_operasrc_finalize);

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_operasrc_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_operasrc_stop);
  gstbasesrc_class->get_size = GST_DEBUG_FUNCPTR (gst_operasrc_get_size);
  gstbasesrc_class->is_seekable = GST_DEBUG_FUNCPTR (gst_operasrc_is_seekable);
  gstbasesrc_class->create = GST_DEBUG_FUNCPTR (gst_operasrc_create);
  gstbasesrc_class->do_seek = GST_DEBUG_FUNCPTR (gst_operasrc_do_seek);
  gstbasesrc_class->check_get_range = GST_DEBUG_FUNCPTR (gst_operasrc_check_get_range);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_operasrc_unlock);
  gstbasesrc_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_operasrc_unlock_stop);

  parent_class = GST_BASE_SRC_CLASS (g_type_class_peek_parent (klass));
}

/* initialize the new element */
static void
gst_operasrc_init (GstOperaSrc *opsrc,
                   GstOperaSrcClass *g_class)
{
  opsrc->lock = g_mutex_new ();
  opsrc->cond = g_cond_new ();
  opsrc->queue = g_queue_new ();
  opsrc->callback = NULL;
  opsrc->quit = FALSE;
  opsrc->size = NO_SIZE;
  opsrc->seekable = FALSE;
}

static void
gst_operasrc_finalize (GObject *object)
{
  GstOperaSrc *opsrc = GST_OPERASRC (object);

  g_mutex_free (opsrc->lock);
  g_cond_free (opsrc->cond);
  g_queue_free (opsrc->queue);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* GstBaseSrc vmethod implementations */

static gboolean
gst_operasrc_start (GstBaseSrc *bsrc)
{
  GstOperaSrc *opsrc = GST_OPERASRC (bsrc);

  g_mutex_lock (opsrc->lock);
  opsrc->flushing = FALSE;
  opsrc->eos = FALSE;
  opsrc->need_offset = 0;
  opsrc->need_size = 0;
  g_mutex_unlock (opsrc->lock);

  return TRUE;
}

static gboolean
gst_operasrc_stop (GstBaseSrc *bsrc)
{
  GstOperaSrc *opsrc = GST_OPERASRC (bsrc);
  GstBuffer *buf;

  g_mutex_lock (opsrc->lock);
  /* flush queue */
  while ((buf = (GstBuffer *) g_queue_pop_head (opsrc->queue))) {
    gst_buffer_unref (buf);
  }
  g_mutex_unlock (opsrc->lock);

  return TRUE;
}

static gboolean
gst_operasrc_get_size (GstBaseSrc *bsrc, guint64 *size)
{
  GstOperaSrc *opsrc = GST_OPERASRC (bsrc);
  *size = opsrc->size;
  return *size != NO_SIZE;
}

/* Check for seekability. It's possible to be seekable in push mode,
 * which may be suitable for non-random access sources. */
static gboolean
gst_operasrc_is_seekable (GstBaseSrc *bsrc)
{
  GstOperaSrc *opsrc = GST_OPERASRC (bsrc);
  return opsrc->seekable;
}

/* Read data from the Opera queue and feed to GStreamer. */
GstFlowReturn
gst_operasrc_create (GstBaseSrc *bsrc, guint64 offset, guint size, GstBuffer **buf)
{
  GstOperaSrc *opsrc = GST_OPERASRC (bsrc);
  GstFlowReturn ret = GST_FLOW_OK;

  /* running in Opera's main thread will result in deadlock */
  if (g_op_system_info->IsInMainThread()) {
    OP_ASSERT(!"deadlock is certain");
    return GST_FLOW_ERROR;
  }

  GST_LOG ("requested buffer of size %u at offset %" G_GUINT64_FORMAT,
           size, offset);

  GST_LOG ("%u buffers on queue", g_queue_get_length (opsrc->queue));

  g_mutex_lock (opsrc->lock);

  while (TRUE) {
    /* check for flushing */
    if (G_UNLIKELY (opsrc->flushing)) {
      ret = GST_FLOW_WRONG_STATE;
      break;
    }

    /* check for shutdown */
    if (G_UNLIKELY (opsrc->quit)) {
      ret = GST_FLOW_UNEXPECTED;
      break;
    }

    /* pop buffer from the queue */
    while ((*buf = (GstBuffer *) g_queue_pop_head (opsrc->queue))) {
      GST_LOG ("popped buffer of size %u at offset %" G_GUINT64_FORMAT,
               GST_BUFFER_SIZE (*buf), GST_BUFFER_OFFSET (*buf));

      /* check that the buffer is at the requested offset and that the
       * size is at least as requested, or aligned with EOS. */
      if (G_LIKELY (GST_BUFFER_OFFSET (*buf) == offset &&
                    (GST_BUFFER_SIZE (*buf) >= size ||
                     GST_BUFFER_FLAG_IS_SET (buf, GST_OPERASRC_BUFFER_FLAG_EOS))))
        break;

      /* else unref and try the next buffer */
      GST_LOG ("got buffer at wrong offset or size, discarding...");
      gst_buffer_unref (*buf);
    }

    /* done if we got a buffer */
    if (*buf) {
      /* unset our custom EOS flag, just to be safe */
      GST_BUFFER_FLAG_UNSET (*buf, GST_OPERASRC_BUFFER_FLAG_EOS);
      /* queue cannot be full now, request more data */
      if (opsrc->need_size > 0)
        opsrc->callback (opsrc->callback_data);
      break;
    }

    /* check for EOS */
    if (G_UNLIKELY (opsrc->eos)) {
      ret = GST_FLOW_UNEXPECTED;
      break;
    }

    /* nag Opera for more data */
    opsrc->need_offset = offset;
    opsrc->need_size = size;
    opsrc->callback (opsrc->callback_data);

    /* wait for buffer, flush or end-of-stream */
    g_cond_wait (opsrc->cond, opsrc->lock);
  }

  g_mutex_unlock (opsrc->lock);

  return ret;
}

/* Just say yes, actual seeking mechanism is in _create */
static gboolean
gst_operasrc_do_seek (GstBaseSrc *bsrc, GstSegment *segment)
{
  return TRUE;
}

/* Check for random access seekable source. By returning TRUE we will
 * operate in pull mode. */
static gboolean
gst_operasrc_check_get_range (GstBaseSrc *bsrc)
{
  GstOperaSrc *opsrc = GST_OPERASRC (bsrc);
  return opsrc->seekable;
}

static gboolean
gst_operasrc_unlock (GstBaseSrc *bsrc)
{
  GstOperaSrc *opsrc = GST_OPERASRC (bsrc);
  g_mutex_lock (opsrc->lock);
  opsrc->flushing = TRUE;
  g_cond_signal (opsrc->cond);
  g_mutex_unlock (opsrc->lock);
  return TRUE;
}

static gboolean
gst_operasrc_unlock_stop (GstBaseSrc *bsrc)
{
  GstOperaSrc *opsrc = GST_OPERASRC (bsrc);
  g_mutex_lock (opsrc->lock);
  opsrc->flushing = FALSE;
  g_mutex_unlock (opsrc->lock);
  return TRUE;
}

/* GstOperaSrc implementations */

void
gst_operasrc_set_callback (GstOperaSrc *opsrc, GstOperaSrcCallback callback, gpointer data)
{
  opsrc->callback = callback;
  opsrc->callback_data = data;
}

void
gst_operasrc_set_size (GstOperaSrc *opsrc, guint64 size)
{
  opsrc->size = size;
}

void gst_operasrc_set_seekable (GstOperaSrc *opsrc, gboolean seekable)
{
  opsrc->seekable = seekable;
}

gboolean
gst_operasrc_need_data (GstOperaSrc *opsrc, guint64 *offset, guint *size)
{
  gboolean ret = FALSE;
  g_mutex_lock (opsrc->lock);
  if (g_queue_get_length (opsrc->queue) < GST_OPERASRC_QUEUE_LENGTH) {
    *offset = opsrc->need_offset;
    *size = opsrc->need_size;
    ret = (*size > 0);
  }
  g_mutex_unlock (opsrc->lock);
  return ret;
}

void
gst_operasrc_push_buffer (GstOperaSrc *opsrc, GstBuffer *buffer)
{
  g_mutex_lock (opsrc->lock);
  OP_ASSERT(!opsrc->eos);
  if (GST_BUFFER_OFFSET (buffer) == opsrc->need_offset &&
      GST_BUFFER_SIZE (buffer) >= 1024 &&
      GST_BUFFER_SIZE (buffer) < 64*1024 &&
      !GST_BUFFER_FLAG_IS_SET (buffer, GST_OPERASRC_BUFFER_FLAG_EOS)) {
    /* read ahead another buffer if this one was at the needed offset,
     * of a reasonable size and not at EOS. */
    opsrc->need_offset += GST_BUFFER_SIZE (buffer);
    opsrc->need_size = GST_BUFFER_SIZE (buffer);
  } else {
    /* otherwise, hold off further buffers */
    opsrc->need_offset = opsrc->need_size = 0;
  }
  g_queue_push_tail (opsrc->queue, buffer);
  g_cond_signal (opsrc->cond);
  g_mutex_unlock (opsrc->lock);
}

void
gst_operasrc_end_of_stream (GstOperaSrc *opsrc)
{
  g_mutex_lock (opsrc->lock);
  opsrc->eos = TRUE;
  opsrc->need_offset = opsrc->need_size = 0;
  g_cond_signal (opsrc->cond);
  g_mutex_unlock (opsrc->lock);
}

void
gst_operasrc_quit (GstOperaSrc *opsrc)
{
  g_mutex_lock (opsrc->lock);
  opsrc->quit = TRUE;
  g_cond_signal (opsrc->cond);
  g_mutex_unlock (opsrc->lock);
}

#endif /* MEDIA_BACKEND_GSTREAMER */
