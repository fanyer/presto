/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *
 * gst.h: Main header for GStreamer, apps should include this
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifndef __GST_H__
#define __GST_H__

#include "platforms/media_backends/gst/include/glib.h"

#include "platforms/media_backends/gst/include/gst/glib-compat.h"

#include "platforms/media_backends/gst/include/gst/gstenumtypes.h"
#include "platforms/media_backends/gst/include/gst/gstversion.h"

#include "platforms/media_backends/gst/include/gst/gstbin.h"
#include "platforms/media_backends/gst/include/gst/gstbuffer.h"
#include "platforms/media_backends/gst/include/gst/gstcaps.h"
#include "platforms/media_backends/gst/include/gst/gstchildproxy.h"
#include "platforms/media_backends/gst/include/gst/gstclock.h"
#include "platforms/media_backends/gst/include/gst/gstdebugutils.h"
#include "platforms/media_backends/gst/include/gst/gstelement.h"
#include "platforms/media_backends/gst/include/gst/gsterror.h"
#include "platforms/media_backends/gst/include/gst/gstevent.h"
#include "platforms/media_backends/gst/include/gst/gstghostpad.h"
#include "platforms/media_backends/gst/include/gst/gstindex.h"
#include "platforms/media_backends/gst/include/gst/gstindexfactory.h"
#include "platforms/media_backends/gst/include/gst/gstinfo.h"
#include "platforms/media_backends/gst/include/gst/gstinterface.h"
#include "platforms/media_backends/gst/include/gst/gstiterator.h"
#include "platforms/media_backends/gst/include/gst/gstmarshal.h"
#include "platforms/media_backends/gst/include/gst/gstmessage.h"
#include "platforms/media_backends/gst/include/gst/gstminiobject.h"
#include "platforms/media_backends/gst/include/gst/gstobject.h"
#include "platforms/media_backends/gst/include/gst/gstpad.h"
#include "platforms/media_backends/gst/include/gst/gstparamspecs.h"
#include "platforms/media_backends/gst/include/gst/gstpipeline.h"
#include "platforms/media_backends/gst/include/gst/gstplugin.h"
#include "platforms/media_backends/gst/include/gst/gstpoll.h"
#include "platforms/media_backends/gst/include/gst/gstpreset.h"
#include "platforms/media_backends/gst/include/gst/gstquery.h"
#include "platforms/media_backends/gst/include/gst/gstregistry.h"
#include "platforms/media_backends/gst/include/gst/gstsegment.h"
#include "platforms/media_backends/gst/include/gst/gststructure.h"
#include "platforms/media_backends/gst/include/gst/gstsystemclock.h"
#include "platforms/media_backends/gst/include/gst/gsttaglist.h"
#include "platforms/media_backends/gst/include/gst/gsttagsetter.h"
#include "platforms/media_backends/gst/include/gst/gsttask.h"
#include "platforms/media_backends/gst/include/gst/gsttrace.h"
#include "platforms/media_backends/gst/include/gst/gsttypefind.h"
#include "platforms/media_backends/gst/include/gst/gsttypefindfactory.h"
#include "platforms/media_backends/gst/include/gst/gsturi.h"
#include "platforms/media_backends/gst/include/gst/gstutils.h"
#include "platforms/media_backends/gst/include/gst/gstvalue.h"
#include "platforms/media_backends/gst/include/gst/gstxml.h"

#include "platforms/media_backends/gst/include/gst/gstparse.h"

/* API compatibility stuff */
#include "platforms/media_backends/gst/include/gst/gstcompat.h"

G_BEGIN_DECLS

void		gst_init			(int *argc, char **argv[]);
gboolean	gst_init_check			(int *argc, char **argv[],
						 GError ** err);
GOptionGroup *	gst_init_get_option_group	(void);
void		gst_deinit			(void);

void		gst_version			(guint *major, guint *minor,
						 guint *micro, guint *nano);
gchar *		gst_version_string		(void);

gboolean        gst_segtrap_is_enabled          (void);
void            gst_segtrap_set_enabled         (gboolean enabled);

gboolean        gst_registry_fork_is_enabled    (void);
void            gst_registry_fork_set_enabled   (gboolean enabled);

gboolean        gst_update_registry (void);

G_END_DECLS

#endif /* __GST_H__ */
