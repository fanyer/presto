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
#include "platforms/media_backends/gst/gstmediaplayer.h"
#include "platforms/media_backends/gst/gstmediamanager.h"
#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpThreadTools.h"

GST_DEBUG_CATEGORY_EXTERN (gst_opera_debug);
#define GST_CAT_DEFAULT gst_opera_debug

// unique element names
#define GST_OP_SRC_NAME "opsrc"
#define GST_OP_DECODE_BIN_NAME "opdecodebin"
#define GST_OP_AUDIO_BIN_NAME "opaudiobin"
#define GST_OP_AUDIO_SINK_NAME "opaudiosink"
#define GST_OP_VOLUME_NAME "opvolume"
#define GST_OP_VIDEO_BIN_NAME "opvideobin"
#define GST_OP_VIDEO_SINK_NAME "opvideosink"

/** Type of helper function that creates a new sinkbin */
typedef GstElement * (*GstOpNewBinFunc)();

// pipeline building utilities
static gboolean gst_op_make_add_link_replace(GstBin *bin, GstElement **sink,
											 const gchar *factory, const gchar *name);
static gboolean gst_op_link_pad(GstElement *pipeline, GstPad *srcpad,
								const gchar *binname, GstOpNewBinFunc binfunc);
static void gst_op_remove_unlinked(GstElement *pipeline, const gchar *name);
static GstElement * gst_op_audio_bin_new();
static GstElement * gst_op_video_bin_new();
static gboolean gst_op_bin_add_or_unref(GstBin *bin, GstElement *element);

// post application messages
static void gst_op_post_structure(GstElement *element, GstStructure *structure);

/** Find a time/byte pair in the index. */
static gboolean gst_op_index_lookup(GstIndex *index, OpFileLength byte,
									double &time_out, OpFileLength &byte_out,
									GstIndexLookupMethod method);
/** Find the byte offset after a time offset in the index. */
static gboolean gst_op_index_lookup(GstIndex *index, double time, OpFileLength &byte);

// set volume on the appropriate element
static void gst_op_set_volume(GstElement *pipeline, gdouble volume);

GstSharedState::GstSharedState() :
	m_mutex(g_mutex_new()),
	m_has_calls(g_cond_new())
{
}

GstSharedState::~GstSharedState()
{
	g_mutex_free(m_mutex);
	g_cond_free(m_has_calls);
}

void
GstSharedState::Restore(gdouble position)
{
	GLock l(m_mutex);

	// If a position was specified, set it to the current position.
	if (op_isfinite(position))
		m_state.m_position = position;

	if (m_state.m_state != GST_STATE_VOID_PENDING)
		m_state.CallSetState(m_state.m_state);
	if (op_isfinite(m_state.m_position))
		m_state.CallSetPosition(m_state.m_position);
	if (m_state.m_volume >= 0 && m_state.m_volume <= 1)
		m_state.CallSetVolume(m_state.m_volume);
}

void
GstSharedState::WaitForCalls(GstActionState &state)
{
	g_mutex_lock(m_mutex);

	while (!m_state.HasCalls())
		g_cond_wait(m_has_calls, m_mutex);

	// Copy the state.
	state = m_state;
	m_state.ResetCalls();

	g_mutex_unlock(m_mutex);

	state.ResetReturns(); // Calls only.
}

void
GstSharedState::GetReturns(GstActionState &state)
{
	g_mutex_lock(m_mutex);
	state = m_state;
	m_state.ResetReturns();
	g_mutex_unlock(m_mutex);

	state.ResetCalls(); // Returns only.
}

bool
GstSharedState::Merge(const GstActionState &state)
{
	if (!state.HasCalls() && !state.HasReturns())
		return true;

	g_mutex_lock(m_mutex);

	gboolean had_returns = m_state.HasReturns();

	m_state.Merge(state);

	if (state.HasCalls())
		g_cond_signal(m_has_calls);

	g_mutex_unlock(m_mutex);

	return (state.HasReturns() && !had_returns);
}

GstMediaPlayer::GstMediaPlayer(OpMediaSource* src) :
	m_listener(NULL),
	m_src(src),
	m_frame(NULL),
	m_bitmap(NULL),
	m_gstsrc(NULL),
	m_gstsrc_seekable(FALSE),
	m_pipeline(NULL),
	m_index(NULL),
	m_thread(NULL),
	m_lock(NULL),
	m_native_width(0),
	m_native_height(0),
	m_native_par(0.0),
	m_preload_duration(g_stdlib_infinity),
	m_preload_bytes(FILE_LENGTH_NONE),
	m_src_pending(FALSE),
	m_stalled_state(GST_STATE_VOID_PENDING),
	m_decode_err(FALSE),
	m_state_change_mutex(NULL),
	m_state_change_cond(NULL)
{
}

/* virtual */
GstMediaPlayer::~GstMediaPlayer()
{
	gst_buffer_replace(&m_frame, NULL);
	SuspendPipeline();
	OP_DELETE(m_bitmap);
	OP_DELETE(m_src);
}

OP_STATUS
GstMediaPlayer::EnsurePipeline()
{
	if (m_pipeline)
		return OpStatus::OK;

	// never try creating the pipeline once failed
	if (m_decode_err)
		return OpStatus::ERR;

	OP_ASSERT(!m_index && !m_thread && !m_lock);

	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_MEDIA_BACKENDS_GST_STATE_CHANGED,
														reinterpret_cast<MH_PARAM_1>(this)));
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_MEDIA_BACKENDS_GST_SRC,
														reinterpret_cast<MH_PARAM_1>(this)));

	// create top-level pipeline
	m_pipeline = gst_pipeline_new("pipeline");

	// decoder chain
	GstElement *srcdec = NULL;

	m_state_change_mutex = g_mutex_new();
	m_state_change_cond = g_cond_new();

	// make and link operasrc ! decodebin
	if (gst_op_make_add_link_replace(GST_BIN_CAST(m_pipeline), &srcdec, "decodebin2", GST_OP_DECODE_BIN_NAME) ||
		gst_op_make_add_link_replace(GST_BIN_CAST(m_pipeline), &srcdec, "decodebin", GST_OP_DECODE_BIN_NAME))
	{
		// set sink-caps to bypass typefinder
		const char *type = GstMapType(m_src->GetContentType());
		OP_ASSERT(type || !"Unsupported type should have been caught by CanPlayType");
		GstCaps *caps = gst_caps_new_simple(type, NULL);
		g_object_set(srcdec, "sink-caps", caps, NULL);
		gst_caps_unref(caps);
	}
	else
	{
		GST_ERROR("unable to create decodebin");
		goto error;
	}

	// audio and video bins are added in "new-decoded-pad"
	// and removed in "no-more-pads"
	g_signal_connect(srcdec, "new-decoded-pad", G_CALLBACK(GstMediaPlayer::NewDecodedPad), this);
	g_signal_connect(srcdec, "no-more-pads", G_CALLBACK(GstMediaPlayer::NoMorePads), this);

	// index decodebin (in practice the demuxer)
	m_index = gst_index_factory_make("memindex");
	if (m_index)
	{
		gst_element_set_index(srcdec, m_index);
	}
	else
	{
		GST_WARNING("unable to create memindex");
	}

	// create and set up operasrc
	if (gst_op_make_add_link_replace(GST_BIN_CAST(m_pipeline), &srcdec, "operasrc", GST_OP_SRC_NAME))
	{
		m_gstsrc = GST_OPERASRC(gst_object_ref(srcdec));
		// callback
		gst_operasrc_set_callback(m_gstsrc, GstMediaPlayer::NeedData, this);
		// size
		OpFileLength size = m_src->GetTotalBytes();
		if (size > 0)
			gst_operasrc_set_size(m_gstsrc, size);
		// seekable
		m_gstsrc_seekable = m_src->IsSeekable();
		gst_operasrc_set_seekable(m_gstsrc, m_gstsrc_seekable);
	}
	else
	{
		GST_ERROR("unable to create operasrc");
		goto error;
	}

	// decoder chain done
	gst_object_unref(srcdec);

	m_lock = g_mutex_new();

	g_gst_thread_manager->Queue(this);

	return OpStatus::OK;

error:
	SuspendPipeline();
	// the error isn't in the media resource, but in any case we
	// can't decode this.
	OP_ASSERT(!m_decode_err);
	m_decode_err = TRUE;
	if (m_listener)
		m_listener->OnDecodeError(this);
	return OpStatus::ERR;
}

void
GstMediaPlayer::SuspendPipeline()
{
	// N.B. order is important, gst_operasrc_quit must be called
	// before g_thread_join (deadlock is possible otherwise)
	if (m_gstsrc)
	{
		// unblock gst_operasrc_create
		gst_operasrc_quit(m_gstsrc);
		gst_object_unref(m_gstsrc);
		m_gstsrc = NULL;
	}
	if (m_pipeline)
	{
		AsyncQuit();
		g_gst_thread_manager->Stop(this);
	}
	if (m_pipeline)
	{
		gst_object_unref(m_pipeline);
		m_pipeline = NULL;
	}
	if (m_index)
	{
		gst_object_unref(m_index);
		m_index = NULL;
	}
	if (m_lock)
	{
		g_mutex_free(m_lock);
		m_lock = NULL;
	}
	if (m_state_change_mutex)
	{
		g_mutex_free(m_state_change_mutex);
		m_state_change_mutex = NULL;
	}
	if (m_state_change_cond)
	{
		g_cond_free(m_state_change_cond);
		m_state_change_cond = NULL;
	}

	g_main_message_handler->UnsetCallBacks(this);
	g_main_message_handler->RemoveDelayedMessage(MSG_MEDIA_BACKENDS_GST_STATE_CHANGED,
												 reinterpret_cast<MH_PARAM_1>(this), 0);
	g_main_message_handler->RemoveDelayedMessage(MSG_MEDIA_BACKENDS_GST_SRC,
												 reinterpret_cast<MH_PARAM_1>(this), 0);
}

OP_STATUS
GstMediaPlayer::ResumePipeline(gdouble position)
{
	RETURN_IF_ERROR(EnsurePipeline());
	m_state.Restore(position);
	return OpStatus::OK;
}

static void
ReturnDuration(GstActionState &state, gint64 duration)
{
	// We only need to apply the preload once, when the duration becomes
	// known for the first time.
	if (op_isnan(state.GetDuration()))
		state.ReturnApplyPreload();

	state.ReturnDuration(duration > 0 ? duration * 1E-9 : op_nan(NULL));
}

/* static */ gpointer
GstMediaPlayer::ThreadFunc(gpointer data)
{
	GstMediaPlayer *player = (GstMediaPlayer *)data;
	OP_ASSERT(player);
	GstElement *pipeline = player->m_pipeline;
	OP_ASSERT(pipeline);

	GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE_CAST(player->m_pipeline));

	gst_bus_set_sync_handler(bus, GstMediaPlayer::BusSyncHandler, player);

	while (TRUE)
	{
		GstActionState calls;
		player->WaitForActionStateCalls(calls);

		if (calls.HasCallQuit())
			break;

		GstActionState returns;

		if (calls.HasCallSetState())
		{
			GstState requested = calls.GetState();
			GstState current, pending;
			GstStateChangeReturn ret;
			OP_ASSERT(requested != GST_STATE_VOID_PENDING);
			ret = gst_element_get_state(pipeline, &current, &pending, 0);
			if (ret != GST_STATE_CHANGE_FAILURE && requested != pending)
			{
				ret = gst_element_set_state(pipeline, requested);

				if (ret == GST_STATE_CHANGE_ASYNC)
					player->WaitForStateChange(requested);
			}
			if (ret == GST_STATE_CHANGE_FAILURE)
			{
				GST_ERROR("unable to set state to %d", requested);
			}
		}

		if (calls.HasCallSetVolume())
			gst_op_set_volume(pipeline, calls.GetVolume());

		if (calls.HasCallGetDuration())
		{
			GstFormat format = GST_FORMAT_TIME;
			gint64 duration;
			if (gst_element_query_duration (pipeline, &format, &duration)
				&& format == GST_FORMAT_TIME && duration > 0)
			{
				ReturnDuration(returns, duration);
			}
		}

		if (calls.HasCallForceDuration())
		{
			GstFormat format = GST_FORMAT_TIME;
			gint64 duration;
			if (gst_element_query_duration (pipeline, &format, &duration)
				&& format == GST_FORMAT_TIME)
			{
				ReturnDuration(returns, duration);
			}
		}

		if (calls.HasCallEOS())
		{
			if (!op_isfinite(calls.GetDuration()))
			{
				// If we've found no duration until now, use the current position.
				GstFormat format = GST_FORMAT_TIME;
				gint64 position;
				if (gst_element_query_position (pipeline, &format, &position)
					&& format == GST_FORMAT_TIME && position > 0)
				{
					ReturnDuration(returns, position);
				}
			}

			returns.ReturnEOS();
		}

		if (calls.HasCallSetPosition() || calls.HasCallSetRate())
		{
			GstState current, pending;
			GstStateChangeReturn ret;
			ret = gst_element_get_state(pipeline, &current, &pending, 0);

			if (ret != GST_STATE_CHANGE_FAILURE && current >= GST_STATE_PAUSED)
			{
				GstFormat format = GST_FORMAT_TIME;
				gint64 position = 0;
				gdouble rate = calls.GetRate();
				gint flags = GST_SEEK_FLAG_FLUSH;

				if (calls.HasCallSetPosition())
				{
					position = static_cast<gint64>(calls.GetPosition() * GST_SECOND);
					flags |= GST_SEEK_FLAG_ACCURATE;
				}
				else
				{
					if (!gst_element_query_position(pipeline, &format, &position))
					{
						GST_ERROR("Could not query position");
					}
				}

				if (gst_element_seek(pipeline, rate, GST_FORMAT_TIME,
												static_cast<GstSeekFlags>(flags),
												GST_SEEK_TYPE_SET, position,
												GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
				{
					if (player->WaitForStateChange(current))
					{
						GST_LOG("finished seek to position %f", position);
					}
					else
					{
						GST_ERROR("failed while waiting for seek to position %f to finish", position);
					}
				}

				if (calls.HasCallSetPosition())
				{
					gst_op_post_structure(pipeline, gst_structure_new("seek-complete", NULL));
				}
			}
		}

		player->MergeActionState(returns);
	}

	gst_object_unref(bus);

	// save state and destroy the pipeline
	// no locking needed as the main thread is already blocking

	// save position
	{
		GstFormat format = GST_FORMAT_TIME;
		gint64 position;
		if (gst_element_query_position (pipeline, &format, &position)
			&& format == GST_FORMAT_TIME && position > 0)
			player->SetAsyncPosition(position*1E-9);
	}

	gst_element_set_state(player->m_pipeline, GST_STATE_NULL);
	// Note: don't call gst_element_get_state here in an attempt to
	// wait for the state change to complete. According to GStreamer
	// developer wtay get_state never blocks for NULL, the pipeline
	// and its associated threads will simply be shut down in the time
	// it takes -- we have no control over it.

	return NULL;
}


/* static */ GstBusSyncReply
GstMediaPlayer::BusSyncHandler(GstBus *bus, GstMessage *msg, gpointer data)
{
	GstMediaPlayer *player = static_cast<GstMediaPlayer*>(data);
	OP_ASSERT(player);

	GstElement *pipeline = player->m_pipeline;
	OP_ASSERT(pipeline);

	GstActionState action_state;

	switch (GST_MESSAGE_TYPE(msg))
	{
	case GST_MESSAGE_APPLICATION:
		if (gst_structure_has_name(gst_message_get_structure(msg), "have-frame"))
			action_state.ReturnHaveFrame();
		else if (gst_structure_has_name(gst_message_get_structure(msg), "seek-complete"))
			action_state.ReturnSeekComplete();
		else if (gst_structure_has_name(gst_message_get_structure(msg), "video-resize"))
			action_state.ReturnVideoResize();
		else
			OP_ASSERT(!"GstMediaPlayer: Unknown application message");
		break;
	case GST_MESSAGE_DURATION:
		if (GST_MESSAGE_SRC(msg) == GST_OBJECT_CAST(pipeline))
			action_state.CallGetDuration();
		break;
	case GST_MESSAGE_STATE_CHANGED:
		if (GST_MESSAGE_SRC(msg) == GST_OBJECT_CAST(pipeline))
		{
			GstState oldstate, newstate, pending;
			gst_message_parse_state_changed (msg, &oldstate, &newstate, &pending);

			GST_LOG("state change: old=%s; new=%s; pending=%s",
					gst_element_state_get_name(oldstate),
					gst_element_state_get_name(newstate),
					gst_element_state_get_name(pending));

			// Post duration update even if we don't know it, core relies
			// on it to determine that metadata has been loaded (which it
			// has by now, but there's no other good message to catch).
			if (oldstate == GST_STATE_READY && newstate == GST_STATE_PAUSED)
				action_state.CallForceDuration();

			player->SignalStateChanged(newstate);
		}
		break;
	case GST_MESSAGE_EOS:
		action_state.CallEOS();
		break;
	case GST_MESSAGE_ERROR:
		action_state.ReturnError();
		player->UnblockStateChanged();
#ifndef GST_DISABLE_GST_DEBUG
		{
			GError *gerror = NULL;
			gst_message_parse_error (msg, &gerror, NULL);
			GST_ERROR ("%s", gerror->message);
			g_error_free (gerror);
		}
#endif // GST_DISABLE_GST_DEBUG
		break;
	case GST_MESSAGE_WARNING:
#ifndef GST_DISABLE_GST_DEBUG
		{
			GError *gerror = NULL;
			gst_message_parse_warning (msg, &gerror, NULL);
			GST_WARNING ("%s", gerror->message);
			g_error_free (gerror);
		}
#endif // GST_DISABLE_GST_DEBUG
		break;
	default:
		GST_INFO("unhandled message type: %s", GST_MESSAGE_TYPE_NAME(msg));
	};

	player->MergeActionState(action_state);

	return GST_BUS_DROP;
}

/* virtual */ OP_STATUS
GstMediaPlayer::Play()
{
	RETURN_IF_ERROR(EnsurePipeline());
	SetAsyncState(GST_STATE_PLAYING);
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
GstMediaPlayer::Pause()
{
	RETURN_IF_ERROR(EnsurePipeline());
	SetAsyncState(GST_STATE_PAUSED);
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
GstMediaPlayer::GetBufferedTimeRanges(const OpMediaTimeRanges** ranges)
{
	if (!ranges)
		return OpStatus::ERR_NULL_POINTER;

	if (!m_pipeline)
		return OpStatus::ERR_NOT_SUPPORTED;

	const OpMediaByteRanges* byte_ranges;
	RETURN_IF_ERROR(m_src->GetBufferedBytes(&byte_ranges));

	// reallocate the ranges array if necessary
	UINT32 length = byte_ranges->Length();
	if (m_buffered.allocated < length)
	{
		m_buffered.ranges = reinterpret_cast<GstBufferedTimeRange*>(op_realloc(m_buffered.ranges, sizeof(GstBufferedTimeRange)*length));
		if (!m_buffered.ranges)
			return OpStatus::ERR_NO_MEMORY;
		m_buffered.allocated = length;
	}

	// convert the byte ranges to time ranges. it's possible for a
	// byte range to map to an empty time range, so the final number
	// of time ranges may be less than the number of byte ranges.
	m_buffered.used = 0;
	for (UINT32 i = 0; i < length; i++)
	{
		OpFileLength byte_start = byte_ranges->Start(i);
		OpFileLength byte_end = byte_ranges->End(i);
		double time_start, time_end;

		if (ConvertRange(byte_start, byte_end, time_start, time_end))
		{
			OP_ASSERT(time_start >= 0 && time_start < time_end);
			m_buffered.ranges[m_buffered.used].start = time_start;
			m_buffered.ranges[m_buffered.used].end = time_end;
			m_buffered.used++;
		}
	}

#ifdef _DEBUG
	// fill unused slots with NaN
	for (UINT32 i = m_buffered.used; i < m_buffered.allocated; i++)
		m_buffered.ranges[i].start = m_buffered.ranges[i].end = op_nan(NULL);
#endif

	*ranges = &m_buffered;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
GstMediaPlayer::GetSeekableTimeRanges(const OpMediaTimeRanges** ranges)
{
	gdouble duration = m_state.GetDuration();

	if (!ranges)
		return OpStatus::ERR_NULL_POINTER;
	if (m_src->IsSeekable() && op_isfinite(duration))
		m_seekable.end = duration;
	*ranges = &m_seekable;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
GstMediaPlayer::GetPosition(double* position)
{
	if (!position)
		return OpStatus::ERR_NULL_POINTER;

	// Note: Querying position from the main thread will deadlock if
	// getting position requires reading more data from the main
	// thread. Let's hope this never happens. See gst_operasrc_create.
	if (m_pipeline)
	{
		GstFormat format = GST_FORMAT_TIME;
		gint64 gst_pos;
		if (gst_element_query_position (m_pipeline, &format, &gst_pos)
			&& format == GST_FORMAT_TIME)
		{
			*position = gst_pos*1E-9;
			return OpStatus::OK;
		}
	}

	*position = 0.0;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
GstMediaPlayer::SetPosition(double position)
{
	OP_ASSERT(position >= 0 && op_isfinite(position));
	if (!m_gstsrc_seekable)
	{
		if (m_src->IsSeekable())
		{
			// If the source has become seekable since it was created (e.g.
			// because it has been fully downloaded, suspend and resume
			// to restart in pull mode to seek.
			m_gstsrc_seekable = TRUE;
			SuspendPipeline();
			return ResumePipeline(position);
		}
		// can't seek at this time
		return OpStatus::ERR_OUT_OF_RANGE;
	}
	RETURN_IF_ERROR(EnsurePipeline());
	SetAsyncPosition(position);
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
GstMediaPlayer::GetDuration(double* duration)
{
	if (!duration)
		return OpStatus::ERR_NULL_POINTER;

	gdouble current_duration = m_state.GetDuration();

	// Note: Duration will be updated whenever it has changed, we
	// don't need to query the pipeline here.
	if (m_gstsrc && !m_gstsrc_seekable && op_isnan(current_duration))
		*duration = g_stdlib_infinity;
	else
		*duration = current_duration;

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
GstMediaPlayer::SetPlaybackRate(double rate)
{
	if (rate <= 0 || !op_isfinite(rate))
		return OpStatus::ERR_NOT_SUPPORTED;

	RETURN_IF_ERROR(EnsurePipeline());
	SetAsyncRate(rate);
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
GstMediaPlayer::SetVolume(double volume)
{
	if (!(volume >= 0 && volume <= 1)) // catches NaN too
	{
		OP_ASSERT(FALSE);
		return OpStatus::ERR_OUT_OF_RANGE;
	}
	RETURN_IF_ERROR(EnsurePipeline());
	SetAsyncVolume(volume);
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
GstMediaPlayer::SetPreload(double preload_duration)
{
	m_preload_duration = preload_duration;
	ApplyPreload();
	return OpStatus::OK;
}

void
GstMediaPlayer::ApplyPreload()
{
	m_preload_bytes = FILE_LENGTH_NONE;

	// Note: We calculate a preload measured from the beginning of the
	// resource, not from the current playback position in bytes
	// (which we do not exactly know). Thus, we do not handle a
	// limited preload once playback has begun or after seeking.

	GstState state = m_state.GetState();
	gdouble position = m_state.GetPosition();

	if (state == GST_STATE_PLAYING || position > 0)
		return;

	if (op_isfinite(m_preload_duration))
	{
		if (m_index && gst_op_index_lookup(m_index, m_preload_duration, m_preload_bytes))
		{
			OP_ASSERT(m_preload_bytes > 0);
		}
		else
		{
			gdouble current_duration = m_state.GetDuration();

			// fall back to simply guessing based on the average rate
			OpFileLength byte_size;
			if (HaveSize(byte_size) && m_preload_duration < current_duration)
				m_preload_bytes = (OpFileLength)(byte_size * m_preload_duration / current_duration);
		}
	}
	m_src->Preload(0, m_preload_bytes);
}

/* Warning: It is tempting to attempt to eliminate copy operations by
 * passing OpBitmap::GetPointer directly to GStreamer, but this is not
 * possible as GStreamer holds references to the data even after
 * returning it for display. As a result, OpBitmap::ReleasePointer
 * can't be called, and so the OpBitmap cannot be used at
 * all. Optimization should focus on bypassing OpBitmap altogether by
 * decoding directly to the platform pixel format. */
/* virtual */ OP_STATUS
GstMediaPlayer::GetFrame(OpBitmap** bitmap)
{
	if (!bitmap)
		return OpStatus::ERR_NULL_POINTER;

	BOOL have_new_frame = FALSE;
	if (m_pipeline)
	{
		GstElement *sink = gst_bin_get_by_name(GST_BIN(m_pipeline), GST_OP_VIDEO_SINK_NAME);
		if (sink)
		{
			GstBuffer *buf;
			g_object_get(G_OBJECT(sink), "last-frame", &buf, NULL);
			if (buf)
			{
				if (m_frame != buf)
				{
					GST_LOG("new video frame with caps %" GST_PTR_FORMAT,
							GST_BUFFER_CAPS (buf));
					gst_buffer_replace(&m_frame, buf);
					have_new_frame = TRUE;
				}
				gst_buffer_unref(buf);
			}
			gst_object_unref(sink);
		}
	}

	if (!have_new_frame)
	{
		// no frame or frame unchanged, use last one (could be NULL)
		*bitmap = m_bitmap;
		return OpStatus::OK;
	}

	// Copy the buffer data into a (possibly new) OpBitmap.
	GstStructure *info = gst_caps_get_structure(GST_BUFFER_CAPS(m_frame), 0);
	gint width, height, bpp;
	if (gst_structure_get_int(info, "width", &width) &&
		gst_structure_get_int(info, "height", &height) &&
		gst_structure_get_int(info, "bpp", &bpp))
	{
		OP_ASSERT(width > 0);
		OP_ASSERT(height > 0);
		if (m_bitmap && ((UINT32)width != m_bitmap->Width() ||
						 (UINT32)height != m_bitmap->Height()))
		{
			OP_DELETE(m_bitmap);
			m_bitmap = NULL;
		}
		if (!m_bitmap)
		{
			// create new bitmap without transparent/alpha support
			RETURN_IF_ERROR(OpBitmap::Create(&m_bitmap, width, height, FALSE, FALSE));
		}
		OP_ASSERT(bpp == m_bitmap->GetBpp());
		if (m_bitmap->Supports(OpBitmap::SUPPORTS_POINTER))
		{
			void* data = m_bitmap->GetPointer(OpBitmap::ACCESS_WRITEONLY);
			if (!data)
				return OpStatus::ERR_NO_MEMORY;
			OP_ASSERT(GST_BUFFER_DATA(m_frame));
			OP_ASSERT(GST_BUFFER_SIZE(m_frame) == m_bitmap->Width()*m_bitmap->Height()*m_bitmap->GetBpp()/8);
			op_memcpy(data, GST_BUFFER_DATA(m_frame), GST_BUFFER_SIZE(m_frame));
			m_bitmap->ReleasePointer();
		}
		else
		{
			int line_size = width*bpp/8;
			for (int i=0; i<height; i++)
				RETURN_IF_ERROR(m_bitmap->AddLine(GST_BUFFER_DATA(m_frame)+i*line_size, i));
		}
	}

	*bitmap = m_bitmap;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
GstMediaPlayer::GetNativeSize(UINT32* width, UINT32* height, double* pixel_aspect)
{
	if (!width || !height || !pixel_aspect)
		return OpStatus::ERR_NULL_POINTER;

	g_mutex_lock(m_lock);
	*width = m_native_width;
	*height = m_native_height;
	*pixel_aspect = m_native_par;
	g_mutex_unlock(m_lock);

	return OpStatus::OK;
}

void
GstMediaPlayer::MergeActionState(const GstActionState &state)
{
	if (m_state.Merge(state))
		g_thread_tools->PostMessageToMainThread(MSG_MEDIA_BACKENDS_GST_STATE_CHANGED, reinterpret_cast<MH_PARAM_1>(this), 0);
}

void
GstMediaPlayer::AsyncQuit()
{
	UnblockStateChanged();

	GstActionState calls;
	calls.CallQuit();
	MergeActionState(calls);
}

void
GstMediaPlayer::SetAsyncState(GstState state)
{
	GstActionState calls;
	calls.CallSetState(state);
	MergeActionState(calls);
}

void
GstMediaPlayer::SetAsyncPosition(gdouble position)
{
	GstActionState calls;
	calls.CallSetPosition(position);
	MergeActionState(calls);
}

void
GstMediaPlayer::SetAsyncRate(gdouble rate)
{
	GstActionState calls;
	calls.CallSetRate(rate);
	MergeActionState(calls);
}

void
GstMediaPlayer::SetAsyncVolume(gdouble volume)
{
	GstActionState calls;
	calls.CallSetVolume(volume);
	MergeActionState(calls);
}

/* virtual */ void
GstMediaPlayer::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(g_op_system_info->IsInMainThread());
	switch (msg)
	{
	case MSG_MEDIA_BACKENDS_GST_STATE_CHANGED:
		HandleGstStateChanged();
		break;
	case MSG_MEDIA_BACKENDS_GST_SRC:
		HandleGstSrc();
		break;
	default:
		OP_ASSERT(!"unhandled message");
	}
}

void
GstMediaPlayer::HandleGstStateChanged()
{
	GstActionState returns;
	GetActionStateReturns(returns);

	if (!m_listener)
		return;

	if (returns.HasReturnHaveFrame())
		m_listener->OnFrameUpdate(this);
	if (returns.HasReturnSeekComplete())
		m_listener->OnSeekComplete(this);
	if (returns.HasReturnVideoResize())
		m_listener->OnVideoResize(this);
	if (returns.HasReturnDuration())
		m_listener->OnDurationChange(this);
	if (returns.HasApplyPreload() && m_preload_bytes == FILE_LENGTH_NONE)
		ApplyPreload();
	// Order matters here. We must provide the duration of the resource before
	// we signal the end of the resource.
	if (returns.HasReturnEOS())
		m_listener->OnPlaybackEnd(this);

	if (returns.HasReturnError())
	{
		// there may be several error messages, but we consider
		// them to be fatal and only forward the first.
		if (!m_decode_err)
		{
			m_decode_err = TRUE;
			m_listener->OnDecodeError(this);
		}
	}
}

void
GstMediaPlayer::HandleGstSrcStalled()
{
	OP_ASSERT(m_pipeline);
	OP_ASSERT(m_gstsrc);

	GstState state = m_state.GetState();

	if (state == GST_STATE_PLAYING)
	{
		// Pause the pipeline while we wait for data to arrive.
		SetAsyncState(GST_STATE_PAUSED);
	}

	// This function may be called multiple times during a stall, so we
	// must make sure to only store the state the first time we stalled.
	// Otherwise subsequent calls to this function will overwrite
	// 'm_stalled_state' with GST_STATE_PAUSED.
	if (m_stalled_state == GST_STATE_VOID_PENDING)
		m_stalled_state = state;

	// FIXME: this could be a lot more clever or OpMediaSource
	// could have a listener to notify of data as it arrives.
	g_main_message_handler->PostMessage(MSG_MEDIA_BACKENDS_GST_SRC, reinterpret_cast<MH_PARAM_1>(this), 0, 100);
}

void
GstMediaPlayer::HandleGstSrc()
{
	OP_ASSERT(m_pipeline);
	OP_ASSERT(m_gstsrc);
	if (!m_pipeline || !m_gstsrc)
		return;

	m_src_pending = FALSE;

	// Set to TRUE if we stalled again during function body.
	BOOL stalled = FALSE;

	guint64 offset;
	guint size;
	gdouble duration = m_state.GetDuration();

	while (gst_operasrc_need_data(m_gstsrc, &offset, &size))
	{
		// If the requested read extends past the initial preload
		// range, fall back to preloading everything from the
		// requested read offset to the end. As a prerequisite, check
		// that we've decoded "metadata" (m_duration), as before that
		// we can be seeking all over the place in a way that isn't
		// helpful in determining where to preload from.
		if (!op_isnan(duration) &&
			(m_preload_bytes == FILE_LENGTH_NONE ||
			 m_preload_bytes < (OpFileLength)(offset + size)))
		{
			m_preload_bytes = FILE_LENGTH_NONE;
			m_src->Preload(offset, FILE_LENGTH_NONE);
		}

		guint64 total = (guint64)m_src->GetTotalBytes();
		if (total)
		{
			if (offset >= total)
			{
				gst_operasrc_end_of_stream(m_gstsrc);
				break;
			}

			// clamp read to EOS
			if (size > total - offset)
				size = (guint)(total - offset);
		}
		OP_ASSERT(size > 0);

		GstBuffer *buf = gst_buffer_new_and_alloc(size);

		if (OpStatus::IsSuccess(m_src->Read(GST_BUFFER_DATA(buf), offset, size)))
		{
			// successful read

			GST_BUFFER_SIZE(buf) = size;
			GST_BUFFER_OFFSET(buf) = offset;
			GST_BUFFER_OFFSET_END(buf) = offset + size;
			GST_BUFFER_TIMESTAMP(buf) = GST_CLOCK_TIME_NONE;
			GST_BUFFER_DURATION(buf) = GST_CLOCK_TIME_NONE;

			// set the EOS flag if appropriate
			OP_ASSERT(!GST_BUFFER_FLAG_IS_SET(buf, GST_OPERASRC_BUFFER_FLAG_EOS));
			if (GST_BUFFER_OFFSET_END(buf) == total)
				GST_BUFFER_FLAG_SET(buf, GST_OPERASRC_BUFFER_FLAG_EOS);

			gst_operasrc_push_buffer(m_gstsrc, buf);

			GST_LOG("pushed buffer of size %u at offset %u",
					(unsigned)size, (unsigned)offset);
		}
		else
		{
			gst_buffer_unref(buf);
			// data not loaded yet, try again later
			HandleGstSrcStalled();
			stalled = TRUE;
			break;
		}
	}

	if ((m_stalled_state != GST_STATE_VOID_PENDING) && !stalled)
	{
		SetAsyncState(m_stalled_state);
		m_stalled_state = GST_STATE_VOID_PENDING;
	}
}

// GStreamer callbacks below, will run in GStreamer's thread context

#ifdef MEDIA_BACKEND_GSTREAMER_PULSESINK_WORKAROUND
static GStaticMutex gst_op_audio_bin_mutex = G_STATIC_MUTEX_INIT;
#endif // MEDIA_BACKEND_GSTREAMER_PULSESINK_WORKAROUND

/* static */ void
GstMediaPlayer::NewDecodedPad(GstElement *decodebin, GstPad *pad,
							  gboolean last, gpointer data)
{
	GstElement *pipeline;
	GstCaps *caps;
	GstStructure *str;
	gboolean linked = FALSE;

	pipeline = GST_ELEMENT_PARENT(decodebin);
	if (pipeline == NULL)
		return;

	caps = gst_pad_get_caps(pad);
	str = gst_caps_get_structure(caps, 0);
	if (g_str_has_prefix(gst_structure_get_name(str), "audio"))
	{
		GST_INFO("new audio stream: %" GST_PTR_FORMAT, caps);
#ifdef MEDIA_BACKEND_GSTREAMER_PULSESINK_WORKAROUND
		g_static_mutex_lock(&gst_op_audio_bin_mutex);
#endif // MEDIA_BACKEND_GSTREAMER_PULSESINK_WORKAROUND
		linked = gst_op_link_pad(pipeline, pad, GST_OP_AUDIO_BIN_NAME, &gst_op_audio_bin_new);
#ifdef MEDIA_BACKEND_GSTREAMER_PULSESINK_WORKAROUND
		g_static_mutex_unlock(&gst_op_audio_bin_mutex);
#endif // MEDIA_BACKEND_GSTREAMER_PULSESINK_WORKAROUND
		// set volume on the newly created volume or sink element
		GstMediaPlayer *player = (GstMediaPlayer *)data;
		gst_op_set_volume(pipeline, player->m_state.GetVolume());
	}
	else if (g_str_has_prefix(gst_structure_get_name(str), "video"))
	{
		GST_INFO("new video stream: %" GST_PTR_FORMAT, caps);
		linked = gst_op_link_pad(pipeline, pad, GST_OP_VIDEO_BIN_NAME, &gst_op_video_bin_new);
		if (linked)
		{
			// notify us of video size changes!
			NotifyVideoCaps(pad, NULL, data); // trigger first change manually
			g_signal_connect(pad, "notify::caps", G_CALLBACK(GstMediaPlayer::NotifyVideoCaps), data);
		}
	}
	if (!linked)
	{
		GST_INFO("unhandled stream: %" GST_PTR_FORMAT, caps);
	}
	gst_caps_unref(caps);
}

/* static */ void
GstMediaPlayer::NoMorePads(GstElement *decodebin, gpointer data)
{
	GstElement *pipeline = GST_ELEMENT_PARENT(decodebin);
	if (pipeline)
	{
		gst_op_remove_unlinked(pipeline, GST_OP_AUDIO_BIN_NAME);
		gst_op_remove_unlinked(pipeline, GST_OP_VIDEO_BIN_NAME);
	}
}

/* static */ void
GstMediaPlayer::NotifyVideoCaps(GstPad *pad, GParamSpec *param, gpointer data)
{
	GstCaps *caps;

	g_object_get(G_OBJECT(pad), "caps", &caps, NULL);
	if (caps)
	{
		GstStructure *s;
		gint width, height;

		s = gst_caps_get_structure (caps, 0);

		if (gst_structure_get_int (s, "width", &width) &&
			gst_structure_get_int (s, "height", &height) &&
			width > 0 && height > 0)
		{
			gint par_n, par_d;
			GstMediaPlayer *player = (GstMediaPlayer *)data;
			OP_ASSERT(player);
			g_mutex_lock(player->m_lock);
			player->m_native_width = width;
			player->m_native_height = height;
			if (gst_structure_get_fraction (s, "pixel-aspect-ratio", &par_n, &par_d) &&
				par_n > 0 && par_d > 0)
			{
				player->m_native_par = (gdouble)par_n/(gdouble)par_d;
			}
			else
			{
				player->m_native_par = 1.0;
			}
			GST_LOG("new video size: %dx%d, ar=%d/%d", width, height, par_n, par_d);
			g_mutex_unlock(player->m_lock);
			gst_op_post_structure(player->m_pipeline, gst_structure_new("video-resize", NULL));
		}

		gst_caps_unref(caps);
	}
}

/* static */ void
GstMediaPlayer::NeedData(gpointer data)
{
	GstMediaPlayer *player = (GstMediaPlayer *)data;
	if (!player->m_src_pending)
	{
		player->m_src_pending = TRUE;
		g_thread_tools->PostMessageToMainThread(MSG_MEDIA_BACKENDS_GST_SRC, reinterpret_cast<MH_PARAM_1>(player), 0);
	}
}

BOOL
GstMediaPlayer::ConvertRange(OpFileLength byte_start, OpFileLength byte_end,
							 double &time_start, double &time_end) const
{
	// Note: gst_element_query_convert from GST_FORMAT_BYTES to
	// GST_FORMAT_TIME on the pipeline doesn't do what you might
	// expect. The audio sink will interpret the byte offset as the
	// size of raw audio data and return the time that corresponds to,
	// which is utterly unhelpful.

	OP_ASSERT(byte_start < byte_end);

	gdouble current_duration = m_state.GetDuration();

	OpFileLength size;
	double duration;
	if (HaveSize(size))
		duration = current_duration;
	else
		size = 0, duration = 0;

	if (ConvertOffset(byte_start, time_start, size, duration) &&
		ConvertOffset(byte_end, time_end, size, duration))
	{
		// clamp for numerical errors
		if (time_start < 0)
			time_start = 0;
		if (time_end > current_duration)
			time_end = current_duration;
		return time_start < time_end;
	}
	return FALSE;
}

BOOL
GstMediaPlayer::ConvertOffset(OpFileLength byte, double &time,
							  OpFileLength size, double duration) const
{
	// Interpolate between the previous known mapping (b0,t0) and the
	// next known mapping (b1,t1), at worst using the average bitrate
	// and at best using the closest points in the keyframe index.
	// Note: It's possible to get the same time offset for different
	// byte offsets if the index entry for time 0 is at a non-zero
	// byte offset; any point before that will map to time 0.

	OpFileLength b0 = 0;
	double t0 = 0;
	OpFileLength b1 = size;
	double t1 = duration;
	if (m_index)
	{
		if (gst_op_index_lookup(m_index, byte, t0, b0, GST_INDEX_LOOKUP_BEFORE)
			&& b0 == byte)
		{
			// Exact match, look no further.
			time = t0;
			return TRUE;
		}
		gst_op_index_lookup(m_index, byte, t1, b1, GST_INDEX_LOOKUP_AFTER);
	}

	if (t0 <= t1 && b0 < b1)
	{
		OP_ASSERT(byte >= b0 && byte <= b1);
		time = t0 + (t1 - t0) * (byte - b0) / (double)(b1 - b0);
		OP_ASSERT(time >= t0 && time <= t1);
		return TRUE;
	}
	return FALSE;
}

void
GstMediaPlayer::SignalStateChanged(GstState state)
{
	g_mutex_lock(m_state_change_mutex);
	m_changed_state.state = state;
	g_cond_signal(m_state_change_cond);
	g_mutex_unlock(m_state_change_mutex);
}

void
GstMediaPlayer::UnblockStateChanged()
{
	g_mutex_lock(m_state_change_mutex);
	m_changed_state.unblock = true;
	g_cond_signal(m_state_change_cond);
	g_mutex_unlock(m_state_change_mutex);
}

bool
GstMediaPlayer::WaitForStateChange(GstState state)
{
	bool ok;

	g_mutex_lock(m_state_change_mutex);

	while (!(m_changed_state.state == state || m_changed_state.unblock))
		g_cond_wait(m_state_change_cond, m_state_change_mutex);

	ok = (m_changed_state.state == state);

	g_mutex_unlock(m_state_change_mutex);

	return ok;
}

// GStreamer helper functions below (do not need GstMediaPlayer instance)

/** link existing or create and link a new sinkbin. */
static gboolean gst_op_link_pad(GstElement *pipeline, GstPad *srcpad,
								const gchar *binname, GstOpNewBinFunc binfunc)
{
	GstElement *sinkbin;
	gboolean linked = FALSE;

	if ((sinkbin = gst_bin_get_by_name(GST_BIN(pipeline), binname)))
	{
		// use an existing sinkbin
		GstPad *sinkpad = gst_element_get_static_pad(sinkbin, "sink");
		if (sinkpad)
		{
			if (!gst_pad_is_linked(sinkpad))
				linked = GST_PAD_LINK_SUCCESSFUL(gst_pad_link(srcpad, sinkpad));
			gst_object_unref(sinkpad);
		}
		gst_object_unref(sinkbin);
	}
	else if ((sinkbin = binfunc()))
	{
		// use a newly created sinkbin
		if (gst_op_bin_add_or_unref(GST_BIN_CAST(pipeline), sinkbin))
		{
			if (gst_element_set_state(sinkbin, GST_STATE_PAUSED) != GST_STATE_CHANGE_FAILURE)
			{
				GstPad *sinkpad = gst_element_get_static_pad(sinkbin, "sink");
				if (sinkpad)
				{
					linked = GST_PAD_LINK_SUCCESSFUL(gst_pad_link(srcpad, sinkpad));
					gst_object_unref(sinkpad);
				}
			}
		}
	}

	return linked;
}

/** remove sinkbin by name from pipeline if it is unlinked. */
static void gst_op_remove_unlinked(GstElement *pipeline, const gchar *name)
{
	GstElement *sinkbin = gst_bin_get_by_name(GST_BIN(pipeline), name);
	if (sinkbin)
	{
		GstPad *sinkpad = gst_element_get_static_pad(sinkbin, "sink");
		if (sinkpad)
		{
			if (!gst_pad_is_linked(sinkpad))
			{
				gst_element_set_state(sinkbin, GST_STATE_NULL);
				if (!gst_bin_remove(GST_BIN(pipeline), sinkbin))
				{
					GST_WARNING("unable to remove unlinked sinkbin: %s",
								GST_ELEMENT_NAME(sinkbin));
				}
			}
			gst_object_unref(sinkpad);
	}
	else
	{
		GST_WARNING("unable to find the sink pad");
		}
		gst_object_unref(sinkbin);
	}
}

/** equivalent to gst_bin_add except it unrefs the element if add fails. */
static gboolean
gst_op_bin_add_or_unref(GstBin *bin, GstElement *element)
{
	OP_ASSERT(element);
	gboolean ret = gst_bin_add(bin, element);
	if (!ret)
		gst_object_unref(element);
	return ret;
}

/** create a new element from factory; add it to bin; link it with
 *  *sink if sink it is not NULL; if all successful, unref sink and
 *  replace it with the newly created element, which has an outstanding
 *  reference when returned.  in case of failure, return NULL. */
static gboolean
gst_op_make_add_link_replace(GstBin *bin, GstElement **sink,
							 const gchar *factory, const gchar *name)
{
	GstElement *src;
	if ((src = gst_element_factory_make(factory, name)))
	{
		if (gst_op_bin_add_or_unref(bin, src))
		{
			if (*sink == NULL || gst_element_link(src, *sink))
			{
				gst_object_replace((GstObject **) sink, (GstObject *) src);
				return TRUE;
			}
			else
			{
				GST_WARNING("unable to link element");
				gst_bin_remove(bin, src);
			}
		}
		else
		{
			GST_WARNING("unable to add element to bin, duplicate name?");
		}
	}

	return FALSE;
}

/* add the sink pad as a ghost pad on bin */
static gboolean gst_op_add_ghost_sink_pad(GstElement *bin, GstElement *sink)
{
	GstPad *pad;
	gboolean ret = FALSE;
	if ((pad = gst_element_get_static_pad(sink, "sink")))
	{
		ret = gst_element_add_pad(bin, gst_ghost_pad_new("sink", pad));
		gst_object_unref(GST_OBJECT(pad));
	}
	return ret;
}

/** Create a bin to be linked to the audio decoder, containing
 *  format/sample rate conversion, volume and the audio sink. */
static GstElement * gst_op_audio_bin_new()
{
	GstElement *bin;
	GstElement *sink = NULL;

	bin = gst_bin_new(GST_OP_AUDIO_BIN_NAME);

	/* queue ! audioconvert ! audioresample ! volume ! audiosink */

	/* first make sink, without it the rest is useless */
	if ((gst_op_make_add_link_replace(GST_BIN_CAST(bin), &sink, MEDIA_BACKEND_GSTREAMER_AUDIOSINK, GST_OP_AUDIO_SINK_NAME)))
	{
		GST_INFO("using " MEDIA_BACKEND_GSTREAMER_AUDIOSINK);
	}
	else
	{
		GST_ERROR("unable to create an audio sink element");
		goto error;
	}

	/* audioconvert ! audioresample ! volume are optional */
	gst_op_make_add_link_replace(GST_BIN_CAST(bin), &sink, "volume", GST_OP_VOLUME_NAME);
	gst_op_make_add_link_replace(GST_BIN_CAST(bin), &sink, "audioresample", NULL);
	gst_op_make_add_link_replace(GST_BIN_CAST(bin), &sink, "audioconvert", NULL);
	if (!gst_op_make_add_link_replace(GST_BIN_CAST(bin), &sink, "queue", NULL))
	{
		GST_ERROR("unable to create queue element");
		goto error;
	}

	if (!gst_op_add_ghost_sink_pad(bin, sink))
	{
		GST_ERROR("unable to add ghost pad");
		goto error;
	}

	/* success */

	gst_object_unref(sink);
	return bin;

error:
	if (sink)
		gst_object_unref(sink);
	gst_object_unref(bin);
	return NULL;
}

/** Create a bin to be linked to the video decoder, containing
 *  format/sample rate conversion, volume and the audio sink. */
static GstElement * gst_op_video_bin_new()
{
	GstElement *bin;
	GstElement *sink = NULL;

	bin = gst_bin_new(GST_OP_VIDEO_BIN_NAME);

	/* queue ! ffmpegcolorspace ! operavideosink */

	/* first make sink, without it the rest is useless */
	if (!gst_op_make_add_link_replace(GST_BIN_CAST(bin), &sink, "operavideosink", GST_OP_VIDEO_SINK_NAME))
	{
		GST_ERROR("unable to create operavideosink");
		goto error;
	}

	/* ffmpegcolorspace is optional, we can try to play without it */
	gst_op_make_add_link_replace(GST_BIN_CAST(bin), &sink, "ffmpegcolorspace", NULL);
	if (!gst_op_make_add_link_replace(GST_BIN_CAST(bin), &sink, "queue", NULL))
	{
		GST_ERROR("unable to create queue element");
		goto error;
	}

	if (!gst_op_add_ghost_sink_pad(bin, sink))
	{
		GST_ERROR("unable to add ghost pad");
		goto error;
	}

	/* success */

	gst_object_unref(sink);
	return bin;

error:
	if (sink)
		gst_object_unref(sink);
	gst_object_unref(bin);
	return NULL;
}

static void gst_op_post_structure(GstElement *element, GstStructure *structure)
{
	GstMessage *msg = gst_message_new_application(GST_OBJECT_CAST(element), structure);
	gst_element_post_message(element, msg);
}

// Note: We should ideally implement a GstIndexResolver and not assume
// id=1, but this works for matroskademux, which is all that matters.
static const gint GST_INDEX_WRITER_ID = 1;

static gboolean gst_op_index_lookup(GstIndex *index, OpFileLength byte,
									double &time_out, OpFileLength &byte_out,
									GstIndexLookupMethod method)
{
	OP_ASSERT(byte >= 0);

	GstIndexEntry *entry = gst_index_get_assoc_entry(index, GST_INDEX_WRITER_ID, method,
													 GST_ASSOCIATION_FLAG_NONE,
													 GST_FORMAT_BYTES, byte);
	if (entry)
	{
		gint64 time_val, byte_val;
		if (gst_index_entry_assoc_map(entry, GST_FORMAT_TIME, &time_val) &&
			gst_index_entry_assoc_map(entry, GST_FORMAT_BYTES, &byte_val))
		{
			time_out = time_val*1E-9;
			byte_out = byte_val;
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean gst_op_index_lookup(GstIndex *index, double time, OpFileLength &byte)
{
	OP_ASSERT(time >= 0 && op_isfinite(time));

	GstIndexEntry *entry = gst_index_get_assoc_entry(index, GST_INDEX_WRITER_ID,
													 GST_INDEX_LOOKUP_AFTER,
													 GST_ASSOCIATION_FLAG_NONE,
													 GST_FORMAT_TIME, (gint64)(time*1E9));
	if (entry)
	{
		gint64 byte_val;
		if (gst_index_entry_assoc_map(entry, GST_FORMAT_BYTES, &byte_val))
		{
			byte = byte_val;
			return TRUE;
		}
	}
	return FALSE;
}

static void gst_op_set_volume(GstElement *pipeline, gdouble volume)
{
	OP_ASSERT(volume >= 0 && volume <= 1);

	// First try the volume element and if that fails try setting the
	// volume directly on the audio sink (for directsoundsink).
	GstElement *volelem = gst_bin_get_by_name(GST_BIN_CAST(pipeline), GST_OP_VOLUME_NAME);
	if (!volelem)
		volelem = gst_bin_get_by_name(GST_BIN_CAST(pipeline), GST_OP_AUDIO_SINK_NAME);
	if (volelem)
	{
		g_object_set(G_OBJECT(volelem), "volume", volume, NULL);
		gst_object_unref(volelem);
	}
}

#endif // MEDIA_BACKEND_GSTREAMER
