/* -*- Mode: c; indent-tabs-mode: nil; c-file-style: "gnu" -*-
 *
 * Copyright (C) 2009-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef GST_OPERAVIDEOSINK_H
#define GST_OPERAVIDEOSINK_H

#ifdef MEDIA_BACKEND_GSTREAMER

#include "platforms/media_backends/gst/gstlibs.h"

G_BEGIN_DECLS

#define GST_TYPE_OPERAVIDEOSINK \
  (gst_operavideosink_get_type())
#define GST_OPERAVIDEOSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OPERAVIDEOSINK,GstOperaVideoSink))
#define GST_OPERAVIDEOSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OPERAVIDEOSINK,GstOperaVideoSinkClass))
#define GST_IS_OPERAVIDEOSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OPERAVIDEOSINK))
#define GST_IS_OPERAVIDEOSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OPERAVIDEOSINK))

typedef struct _GstOperaVideoSink      GstOperaVideoSink;
typedef struct _GstOperaVideoSinkClass GstOperaVideoSinkClass;

struct _GstOperaVideoSink
{
  GstVideoSink videosink;
  gboolean running;
  GMutex *pool_lock;
  GSList *buffer_pool;
  GstBuffer *last_frame;
};

struct _GstOperaVideoSinkClass
{
  GstVideoSinkClass parent_class;
};

GType gst_operavideosink_get_type (void);

/* GstOperaBuffer is used to provide a buffer pool */

#define GST_TYPE_OPERABUFFER \
  (gst_operabuffer_get_type())
#define GST_OPERABUFFER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OPERABUFFER,GstOperaBuffer))
#define GST_IS_OPERABUFFER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OPERABUFFER))

typedef struct _GstOperaBuffer GstOperaBuffer;

struct _GstOperaBuffer
{
  GstBuffer buffer;
  GstOperaVideoSink *sink;
};

GType gst_operabuffer_get_type (void);

G_END_DECLS

#endif /* MEDIA_BACKEND_GSTREAMER */

#endif /* GST_OPERAVIDEOSINK_H */
