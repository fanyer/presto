/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __G_LIB_H__
#define __G_LIB_H__

#define __GLIB_H_INSIDE__

#include "platforms/media_backends/gst/include/glib/galloca.h"
#include "platforms/media_backends/gst/include/glib/garray.h"
#include "platforms/media_backends/gst/include/glib/gasyncqueue.h"
#include "platforms/media_backends/gst/include/glib/gatomic.h"
#include "platforms/media_backends/gst/include/glib/gbacktrace.h"
#include "platforms/media_backends/gst/include/glib/gbase64.h"
#include "platforms/media_backends/gst/include/glib/gbookmarkfile.h"
#include "platforms/media_backends/gst/include/glib/gcache.h"
#include "platforms/media_backends/gst/include/glib/gchecksum.h"
#include "platforms/media_backends/gst/include/glib/gcompletion.h"
#include "platforms/media_backends/gst/include/glib/gconvert.h"
#include "platforms/media_backends/gst/include/glib/gdataset.h"
#include "platforms/media_backends/gst/include/glib/gdate.h"
#include "platforms/media_backends/gst/include/glib/gdir.h"
#include "platforms/media_backends/gst/include/glib/gerror.h"
#include "platforms/media_backends/gst/include/glib/gfileutils.h"
#include "platforms/media_backends/gst/include/glib/ghash.h"
#include "platforms/media_backends/gst/include/glib/ghook.h"
#include "platforms/media_backends/gst/include/glib/giochannel.h"
#include "platforms/media_backends/gst/include/glib/gkeyfile.h"
#include "platforms/media_backends/gst/include/glib/glist.h"
#include "platforms/media_backends/gst/include/glib/gmacros.h"
#include "platforms/media_backends/gst/include/glib/gmain.h"
#include "platforms/media_backends/gst/include/glib/gmappedfile.h"
#include "platforms/media_backends/gst/include/glib/gmarkup.h"
#include "platforms/media_backends/gst/include/glib/gmem.h"
#include "platforms/media_backends/gst/include/glib/gmessages.h"
#include "platforms/media_backends/gst/include/glib/gnode.h"
#include "platforms/media_backends/gst/include/glib/goption.h"
#include "platforms/media_backends/gst/include/glib/gpattern.h"
#include "platforms/media_backends/gst/include/glib/gpoll.h"
#include "platforms/media_backends/gst/include/glib/gprimes.h"
#include "platforms/media_backends/gst/include/glib/gqsort.h"
#include "platforms/media_backends/gst/include/glib/gquark.h"
#include "platforms/media_backends/gst/include/glib/gqueue.h"
#include "platforms/media_backends/gst/include/glib/grand.h"
#include "platforms/media_backends/gst/include/glib/grel.h"
#include "platforms/media_backends/gst/include/glib/gregex.h"
#include "platforms/media_backends/gst/include/glib/gscanner.h"
#include "platforms/media_backends/gst/include/glib/gsequence.h"
#include "platforms/media_backends/gst/include/glib/gshell.h"
#include "platforms/media_backends/gst/include/glib/gslice.h"
#include "platforms/media_backends/gst/include/glib/gslist.h"
#include "platforms/media_backends/gst/include/glib/gspawn.h"
#include "platforms/media_backends/gst/include/glib/gstrfuncs.h"
#include "platforms/media_backends/gst/include/glib/gstring.h"
#include "platforms/media_backends/gst/include/glib/gtestutils.h"
#include "platforms/media_backends/gst/include/glib/gthread.h"
#include "platforms/media_backends/gst/include/glib/gthreadpool.h"
#include "platforms/media_backends/gst/include/glib/gtimer.h"
#include "platforms/media_backends/gst/include/glib/gtree.h"
#include "platforms/media_backends/gst/include/glib/gtypes.h"
#include "platforms/media_backends/gst/include/glib/gunicode.h"
#include "platforms/media_backends/gst/include/glib/gurifuncs.h"
#include "platforms/media_backends/gst/include/glib/gutils.h"
#ifdef G_PLATFORM_WIN32
#include "platforms/media_backends/gst/include/glib/gwin32.h"
#endif

#undef __GLIB_H_INSIDE__

#endif /* __G_LIB_H__ */
