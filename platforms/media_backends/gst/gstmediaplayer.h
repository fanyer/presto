/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2009-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef GSTMEDIAPLAYER_H
#define GSTMEDIAPLAYER_H

#ifdef MEDIA_BACKEND_GSTREAMER

#include "modules/pi/OpMediaPlayer.h"
#include "modules/windowcommander/OpMediaSource.h"
#include "platforms/media_backends/gst/gstlibs.h"
#include "platforms/media_backends/gst/gstoperasrc.h"

class GstSharedState;

struct GstBufferedTimeRange
{
	double start;
	double end;
};

class GstBufferedTimeRanges :
	public OpMediaTimeRanges
{
public:
	GstBufferedTimeRanges() : ranges(NULL), allocated(0), used(0) {}
	virtual ~GstBufferedTimeRanges() { op_free(ranges); }
	virtual UINT32 Length() const
	{
		return used;
	}
	virtual double Start(UINT32 idx) const
	{
		OP_ASSERT(idx < used);
		return ranges[idx].start;
	}
	virtual double End(UINT32 idx) const
	{
		OP_ASSERT(idx < used);
		return ranges[idx].end;
	}
	GstBufferedTimeRange* ranges;
	UINT32 allocated;
	UINT32 used;
};

/** If the source is seekable, then there is a single seekable time
 *  range from 0 to duration. Otherwise, we cannot seek at all. */
class GstSeekableTimeRanges :
	public OpMediaTimeRanges
{
public:
	GstSeekableTimeRanges() : end(op_nan(NULL)) {}
	virtual UINT32 Length() const
	{
		return op_isfinite(end) ? 1 : 0;
	}
	virtual double Start(UINT32 idx) const
	{
		OP_ASSERT(idx == 0);
		return 0;
	}
	virtual double End(UINT32 idx) const
	{
		OP_ASSERT(idx == 0);
		OP_ASSERT(op_isfinite(end));
		return end;
	}
	double end;
};

/** Monolithic state for asynchronous actions.
 *
 * Each GstMediaPlayer has an instance of this class which is shared between
 * the main thread, and other threads in the media backend. The purpose of
 * GstActionState is to be able to perform certain actions in a separate
 * thread, and (if applicable) store the results of that action for further
 * processing in the main thread.
 *
 * Some actions, such as seeking, can not be executed in the main thread,
 * because they can block while waiting for data to appear in the pipeline.
 * These actions result in a deadlock when called from the main thread,
 * because it is the main thread that is responsible to provide that data
 * being waited for. This class aims to solve this issue by defining flags
 * and associated variables for all possible actions that must be
 * performed in different threads. There are two kinds of actions:
 *
 * - A "Call", which always takes place in a separate, non-main thread.
 * - A "Return", which always takes place in the main thread.
 *
 * "Calls" and "returns" are completely independent, although some "calls"
 * may automatically trigger "returns", e.g. CallGetDuration() will trigger
 * ReturnDuration() later.
 *
 * To issue an action, for instance a seek to a position in the media
 * file, you simply call GstActionState::CallSetPosition(...), and the
 * appropriate flags and variables will be set in the state. The actual
 * action will take place some time in the future, in a separate thread.
 *
 * Note that the requested actions are not executed in any particular order.
 * Whenever a "call" or "return" flag is set in the state, the relevant
 * thread will wake up as soon as possible to execute *all* actions
 * currently defined in the state. For instance, if you do CallSetRate(),
 * and then quickly do CallSetRate() again (with another value) before the
 * previous "call" was handled, only the latter CallSetRate() will be
 * performed. In other words: when you request an action to be performed,
 * you overwrite the previously requested action of the same type, if the
 * previously requested action has not yet been handled.
 *
 * To minimize lock times, you can also batch actions in a GstActionState
 * copy, and submit them to the main shared instance all at once. See
 * GstActionState::Merge() for more information.
 *
 * @see GstActionState::Merge
 */
class GstActionState
{
public:

	/** Constructor.
	 *
	 * By default, no action flags are set.
	 */
	GstActionState() :
		m_state(GST_STATE_VOID_PENDING),
		m_volume(op_nan(NULL)),
		m_position(op_nan(NULL)),
		m_rate(1.0),
		m_duration(op_nan(NULL))
	{
		m_calls.all = 0;
		m_returns.all = 0;
	}

	/** Merge a GstActionState into this GstActionState.
	 *
	 * This function copies call and return data from @c state into
	 * this state, overwriting anything that is already set in this
	 * state. The function will never unset any flag in this state, but
	 * it will reset actions that are present in both this state and the
	 * incoming state.
	 *
	 * This function is useful because it allows the caller to batch several
	 * actions together and merge them into the main state in one call. Ergo
	 * only one lock/unlock sequence will be required, as opposed to one
	 * lock/unlock for each flag.
	 *
	 * @param state The GstActionState to merge into this one.
	 */
	void Merge(const GstActionState &state)
	{
		m_calls.all |= state.m_calls.all;

		if (state.HasCallSetState())
			m_state = state.m_state;
		if (state.HasCallSetVolume())
			m_volume = state.m_volume;
		if (state.HasCallSetPosition())
			m_position = state.m_position;
		if (state.HasCallSetRate())
			m_rate = state.m_rate;

		m_returns.all |= state.m_returns.all;

		if (state.HasReturnDuration())
			m_duration = state.m_duration;
	}

	/** Quits the delegate thread. */
	void CallQuit() { m_calls.b.quit = true; }

	/** Set the state of the pipeline.
	 *
	 * @param state GST_STATE_PAUSED or GST_STATE_PLAYING.
	 */
	void CallSetState(GstState state)
	{
		m_calls.b.set_state = true;
		m_state = state;
	}

	/** Set the audio volume.
	 *
	 * @param volume The volume, from 0.0 (min) to 1.0 (max).
	 */
	void CallSetVolume(gdouble volume)
	{
		m_calls.b.set_volume = true;
		m_volume = volume;
	}

	/** Set the playback position (seek).
	 *
	 * Will trigger ReturnSeekComplete().
	 *
	 * @param position The position in the media file, in seconds since
	 *                 the beginning.
	 */
	void CallSetPosition(gdouble position)
	{
		m_calls.b.set_position = true;
		m_position = position;
	}

	/** Set the playback rate.
	 *
	 * @param rate The new playback rate. For instance, 1.0 is normal playback
	 *             speed, 2.0 is double speed, and 0.5 is half the normal
	 *             speed.
	 */
	void CallSetRate(gdouble rate)
	{
		m_calls.b.set_rate = true;
		m_rate = rate;
	}

	/** Get the duration of the media file.
	 *
	 * Will trigger ReturnDuration(). When HasReturnDuration() returns true,
	 * the duration can be retrieved with GetDuration().
	 */
	void CallGetDuration() { m_calls.b.get_duration = true; }

	/** Get the duration of the media file, even if duration is not known.
	 *
	 * Will trigger ReturnDuration(). When HasReturnDuration() returns true,
	 * the duration can be retrieved with GetDuration().
	 */
	void CallForceDuration() { m_calls.b.force_duration = true; }

	/** Notify the delegate thread that the stream has ended.
	 *
	 * The delegate thread may trigger ReturnDuration(), if the duration
	 * is not known at the time of EOS.
	 *
	 * Will trigger ReturnEOS().
	 */
	void CallEOS() { m_calls.b.eos = true; }

	// Functions for checking call flags:
	bool HasCallQuit() const { return m_calls.b.quit; }
	bool HasCallSetState() const { return m_calls.b.set_state; }
	bool HasCallSetVolume() const { return m_calls.b.set_volume; }
	bool HasCallSetPosition() const { return m_calls.b.set_position; }
	bool HasCallSetRate() const { return m_calls.b.set_rate; }
	bool HasCallGetDuration() const { return m_calls.b.get_duration; }
	bool HasCallForceDuration() const { return m_calls.b.force_duration; }
	bool HasCallEOS() const { return m_calls.b.eos; }

	/** Returns 'true' if there is at least one call. */
	bool HasCalls() const { return m_calls.all != 0; }
	/** Reset all call flags to 'false'. */
	void ResetCalls() { m_calls.all = 0; }

	/** Return the duration to the main thread.
	 *
	 * @param duration The new duration of the media file, in seconds.
	 */
	void ReturnDuration(gdouble duration)
	{
		m_returns.b.duration = true;
		m_duration = duration;
	}

	/** Notify the main thread that the stream has ended. */
	void ReturnEOS() { m_returns.b.eos = true; }
	/** Notify the main thread that an error has occured. */
	void ReturnError() { m_returns.b.error = true; }
	/** Notify the main thread that a new video frame is available. */
	void ReturnHaveFrame() { m_returns.b.have_frame = true; }
	/** Notify the main thread that a seek operation has finished. */
	void ReturnSeekComplete() { m_returns.b.seek_complete = true; }
	/** Notify the main thread of the video size. */
	void ReturnVideoResize() { m_returns.b.video_resize = true; }
	/** Have the main thread apply the preload. */
	void ReturnApplyPreload() { m_returns.b.apply_preload = true; }

	// Functions for checking return flags:
	bool HasReturnDuration() const { return m_returns.b.duration; }
	bool HasReturnEOS() const { return m_returns.b.eos; }
	bool HasReturnError() const { return m_returns.b.error; }
	bool HasReturnHaveFrame() const { return m_returns.b.have_frame; }
	bool HasReturnSeekComplete() const { return m_returns.b.seek_complete; }
	bool HasReturnVideoResize() const { return m_returns.b.video_resize; }
	bool HasApplyPreload() const { return m_returns.b.apply_preload; }

	/** Returns 'true' if there is at least one return. */
	bool HasReturns() const { return m_returns.all != 0; }
	/** Resets all return flags to 'false'. */
	void ResetReturns() { m_returns.all = 0; }

	/** @return The GstState previously set with CallSetState(). */
	GstState GetState() const { OP_ASSERT(HasCallSetState()); return m_state; }
	/** @return The volume previously set with CallSetVolume(). */
	gdouble GetVolume() const { OP_ASSERT(HasCallSetVolume()); return m_volume; }
	/** @return The rate previously set with CallSet[Rate, Position]. */
	gdouble GetRate() const { return m_rate; }
	/** @return The position previously set with CallSetPosition(). */
	gdouble GetPosition() const { OP_ASSERT(HasCallSetPosition()); return m_position; }
	/** @return The duration previously set with ReturnDuration(). */
	gdouble GetDuration() const { return m_duration; }

private:

	union
	{
		struct
		{
			bool quit:1;
			bool set_state:1;
			bool set_volume:1;
			bool set_position:1;
			bool set_rate:1;
			bool get_duration:1;
			bool force_duration:1;
			bool eos:1;
		} b;
		gint8 all;
	} m_calls;

	union
	{
		struct
		{
			bool duration:1;
			bool eos:1;
			bool error:1;
			bool have_frame:1;
			bool seek_complete:1;
			bool video_resize:1;
			bool apply_preload:1;
		} b;
		gint8 all;
	} m_returns;

	GstState m_state;
	gdouble m_volume;
	gdouble m_position;
	gdouble m_rate;
	gdouble m_duration;

	friend class GstSharedState;
};

/** Class for auto-locking/unlocking GMutexes. */
class GLock
{
public:
	GLock(GMutex *mutex) : m_mutex(mutex) { g_mutex_lock(mutex); }
	~GLock() { g_mutex_unlock(m_mutex); }
private:
	GMutex *m_mutex;
};

/** A wrapper for shared GstActionStates.
 *
 * This class makes a GstActionState safely available from different threads
 * by locking a mutex whenever a variable from the shared state is accessed.
 *
 * @see GstActionState
 */
class GstSharedState
{
public:
	GstSharedState();
	~GstSharedState();

	// Getters for state variables.
	GstState GetState() const { GLock l(m_mutex); return m_state.m_state; }
	gdouble GetVolume() const { GLock l(m_mutex); return m_state.m_volume; }
	gdouble GetRate() const { GLock l(m_mutex); return m_state.m_rate; }
	gdouble GetPosition() const { GLock l(m_mutex); return m_state.m_position; }
	gdouble GetDuration() const { GLock l(m_mutex); return m_state.m_duration; }

	/** @return A copy of the GstActionState in this object.*/
	GstActionState GetActionState() const { GLock l(m_mutex); return m_state; }

	/** Apply the currently stored state to the pipeline.
	 *
	 * This is equivalent of calling setting each state variable (volume,
	 * position, and so forth) individually, but if this function is used,
	 * the mutex will only be locked once.
	 *
	 * @param position An optional position to use instead of the current
	 *                 position.
	 */
	void Restore(gdouble position = op_nan(NULL));

	/** Wait for at least one call flag in the shared action state.
	 *
	 * This function will wait (by GCond) until at least one call is set
	 * in 'm_action_state'. When calls do appear, the calls (and only the
	 * calls) will be moved to the specified GstActionState reference.
	 *
	 * @param state When calls appear, copy them to this variable.
	 */
	void WaitForCalls(GstActionState &state);

	/** Get the return flags from the shared action state.
	 *
	 * This function immediately moves any return flags from the shared action
	 * state to the specified GstActionState reference.
	 *
	 * This function does not block.
	 *
	 * Nothing happens if the shared action state does not have any returns set.
	 *
	 * @param state GstActionState to move return flags to (if any).
	 */
	void GetReturns(GstActionState &state);

	/** Merge the specified GstActionState into the shared state.
	 *
	 * @see GstActionState::Merge
	 *
	 * @param state The GstActionState to merge into the shared state.
	 * @return true if there are return flags for the main thread to handle.
	 */
	bool Merge(const GstActionState &state);

private:
	// Protects 'm_state'.
	GMutex *m_mutex;

	// Signaled when one or more call flags are set in 'm_state'.
	GCond *m_has_calls;

	// The shared state. Protected by 'm_mutex'.
	GstActionState m_state;
};

class GstMediaPlayer :
	public OpMediaPlayer,
	public CountedListElement<GstMediaPlayer>,
	private MessageObject
{
public:
	friend class GstMediaManager;

	GstMediaPlayer(OpMediaSource* src);
	virtual ~GstMediaPlayer();

	virtual OP_STATUS Play();
	virtual OP_STATUS Pause();

	virtual OP_STATUS GetBufferedTimeRanges(const OpMediaTimeRanges** ranges);
	virtual OP_STATUS GetSeekableTimeRanges(const OpMediaTimeRanges** ranges);

	virtual OP_STATUS GetPosition(double* position);
	virtual OP_STATUS SetPosition(double position);
	virtual OP_STATUS GetDuration(double* duration);
	virtual OP_STATUS SetPlaybackRate(double rate);
	virtual OP_STATUS SetVolume(double volume);
	virtual OP_STATUS SetPreload(double duration);

	virtual void SetListener(OpMediaPlayerListener* listener) { m_listener = listener; }

	virtual OP_STATUS GetFrame(OpBitmap** bitmap);

	virtual OP_STATUS GetNativeSize(UINT32* width, UINT32* height, double* pixel_aspect);

	/** Wait for at least one call flag in the shared action state. */
	void WaitForActionStateCalls(GstActionState &state) { m_state.WaitForCalls(state); }
	/** Get the return flags from the shared action state. */
	void GetActionStateReturns(GstActionState &state) { m_state.GetReturns(state); }
	/** Merge the specified GstActionState into the shared state. */
	void MergeActionState(const GstActionState &state);

	/** Quit the thread asynchronously. */
	void AsyncQuit();
	/** Set the GstState asynchronously. */
	void SetAsyncState(GstState state);
	/** Set the position asynchronously. */
	void SetAsyncPosition(gdouble position);
	/** Set the playback rate asynchronously. */
	void SetAsyncRate(gdouble rate);
	/** Set the volume asynchronously. */
	void SetAsyncVolume(gdouble volume);

private:
	/** Create the pipeline and the thread in which it runs. */
	OP_STATUS EnsurePipeline();

	/** Stop the pipeline, its threads and save state. */
	void SuspendPipeline();

	/** Restore pipeline from saved state. */
	OP_STATUS ResumePipeline(gdouble position = op_nan(NULL));

	/** Thread running the glib main loop */
	static gpointer ThreadFunc(gpointer data);

	/** GStreamer callbacks during pipeline plugging */
	static void NewDecodedPad(GstElement *decodebin, GstPad *pad,
							  gboolean last, gpointer data);
	static void NoMorePads(GstElement *decodebin, gpointer data);
	static void NotifyVideoCaps(GstPad *pad, GParamSpec *param, gpointer data);

	static GstBusSyncReply BusSyncHandler(GstBus *bus, GstMessage *msg, gpointer data);

	// MessageObject
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/** Called when one or more return flags has been set in 'm_action_state'. */
	void HandleGstStateChanged();

	/** feed the operasrc element if necessary */
	void HandleGstSrc();

	/** handle data stalls */
	void HandleGstSrcStalled();

	/** operasrc callback */
	static void NeedData(gpointer data);

	/** Convert a byte range to a time range.
	 * @return TRUE if conversion was possible. */
	BOOL ConvertRange(OpFileLength byte_start, OpFileLength byte_end,
					  double &time_start, double &time_end) const;

	/** Convert a byte offset to a time offset.
	 *
	 * size/duration are used when no better information is available.
	 *
	 * @return TRUE if conversion was possible. */
	BOOL ConvertOffset(OpFileLength byte, double &time,
					   OpFileLength size, double duration) const;

	/** Get the size of the resource in bytes and return TRUE if both
	 *  the size in bytes and duration in time (m_duration) is known,
	 *  such that average rate (bytes/second) could be calculated. */
	BOOL HaveSize(OpFileLength &length) const
	{
		length = m_src->GetTotalBytes();
		gdouble duration = m_state.GetDuration();
		return length > 0 && length != FILE_LENGTH_NONE &&
			duration > 0 && op_isfinite(duration);
	}

	/** Apply the preload */
	void ApplyPreload();

	OpMediaPlayerListener *m_listener;

	OpMediaSource *m_src;

	GstBuffer *m_frame;
	OpBitmap *m_bitmap;

	// GstOperaSrc reference (needed for thread-safe suspend)
	GstOperaSrc *m_gstsrc;
	BOOL m_gstsrc_seekable;

	// top-level pipeline
	GstElement *m_pipeline;

	// index to help mapping byte offsets to times
	GstIndex *m_index;

	// thread in which pipeline runs, set by GstThreadManager
	GThread *m_thread;

	// lock for member variables accessed from multiple threads (below)
	GMutex *m_lock;

	gint m_native_width;
	gint m_native_height;
	gdouble m_native_par;

	double m_preload_duration;
	OpFileLength m_preload_bytes;

	BOOL m_src_pending;
	GstState m_stalled_state;

	BOOL m_decode_err;

	GstBufferedTimeRanges m_buffered;
	GstSeekableTimeRanges m_seekable;

	GstSharedState m_state;

	// Mutex for 'm_changed_state'.
	GMutex *m_state_change_mutex;
	// Signaled when 'm_changed_state' changes.
	GCond *m_state_change_cond;
	// The previously changed-to state of the pipeline.
	struct ChangedState
	{
		ChangedState() : unblock(false), state(GST_STATE_VOID_PENDING) {}
		bool unblock; // Set to 'true' to force-unblock WaitForStateChange.
		GstState state; // The state we just changed to.
	} m_changed_state;

	/** Signal that the state of the pipeline has changed.
	 *
	 * This will unblock a call to WaitForStateChange, if the signaled
	 * state is the waited-for state.
	 *
	 * @state The state we just changed to.
	 */
	void SignalStateChanged(GstState state);

	/** Force threads waiting for WaitForStateChange to continue.
	 *
	 * This will unblock the call, even if the waited-for state change
	 * has not yet been observed. This may be used to prevent deadlocks
	 * when errors occur, and when shutting down the media player.
	 */
	void UnblockStateChanged();

	/** Blocks until we change to the specified state.
	 *
	 * This function is a work-around for a bug in GStreamer where we can
	 * deadlock if an error occurs while waiting for a state change. See
	 * CORE-45219 for more information.
	 *
	 * @param state The state to wait for.
	 * @return True if we changed to the specified state, false if we did
	 *         not (due to a call to UnblockStateChanged()).
	 */
	bool WaitForStateChange(GstState state);

	friend class GstThreadManager;
};

#endif // MEDIA_BACKEND_GSTREAMER

#endif // GSTMEDIAPLAYER_H
