/* -*- Mode: c; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 2009-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef GST_OPERASRC_H
#define GST_OPERASRC_H

#ifdef MEDIA_BACKEND_GSTREAMER

#include "platforms/media_backends/gst/gstlibs.h"

G_BEGIN_DECLS

#define GST_TYPE_OPERASRC \
  (gst_operasrc_get_type())
#define GST_OPERASRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OPERASRC,GstOperaSrc))
#define GST_OPERASRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OPERASRC,GstOperaSrcClass))
#define GST_IS_OPERASRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OPERASRC))
#define GST_IS_OPERASRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OPERASRC))

/* a flag to indicate that the buffer end is aligned with EOS */
#define GST_OPERASRC_BUFFER_FLAG_EOS GST_BUFFER_FLAG_MEDIA1

typedef struct _GstOperaSrc      GstOperaSrc;
typedef struct _GstOperaSrcClass GstOperaSrcClass;

typedef void (*GstOperaSrcCallback) (gpointer data);

struct _GstOperaSrc
{
  GstBaseSrc basesrc;
  GMutex *lock;
  GCond *cond;
  GQueue *queue;
  GstOperaSrcCallback callback;
  gpointer callback_data;
  gboolean flushing;
  gboolean eos;
  gboolean quit; /* like eos but never reset */
  guint64 need_offset;
  guint need_size;
  guint64 size;
  gboolean seekable;
};

struct _GstOperaSrcClass
{
  GstBaseSrcClass parent_class;
};

GType gst_operasrc_get_type (void);

void gst_operasrc_set_callback (GstOperaSrc *opsrc, GstOperaSrcCallback callback, gpointer data);
void gst_operasrc_set_size (GstOperaSrc *opsrc, guint64 size);
void gst_operasrc_set_seekable (GstOperaSrc *opsrc, gboolean seekable);
gboolean gst_operasrc_need_data (GstOperaSrc *opsrc, guint64 *offset, guint *size);
void gst_operasrc_push_buffer (GstOperaSrc *opsrc, GstBuffer *buffer);
void gst_operasrc_end_of_stream (GstOperaSrc *opsrc);
void gst_operasrc_quit (GstOperaSrc *opsrc);

G_END_DECLS

#endif /* MEDIA_BACKEND_GSTREAMER */

#endif /* GST_OPERASRC_H */
