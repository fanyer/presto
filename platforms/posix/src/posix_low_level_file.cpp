/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2007-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
// #define INSIDE_PI_IMPL // needed if any methods get deprecated ...
#include "core/pch.h"
#ifdef POSIX_OK_FILE
# ifndef POSIX_INTERNAL
#define POSIX_INTERNAL(X) X
# endif

#include "platforms/posix/posix_file_util.h"
#include "platforms/posix/posix_native_util.h"
#include "modules/util/str.h"

#include "platforms/posix/posix_logger.h"
#if defined(POSIX_SERIALIZE_FILENAME) && defined(POSIX_OK_SYS)
#include "platforms/posix/posix_system_info.h"
/* The serialization methods are defined in posix (and used below) even in some
 * situations in which they *aren't* defined in the porting interface.  This
 * enables platforms to extend serialization, beyond what PosixFileUtil does. */
#define g_posix_system_info static_cast<PosixSystemInfo*>(g_op_system_info)
#endif // POSIX_SERIALIZE_FILENAME && POSIX_OK_SYS

#ifdef PI_ASYNC_FILE_OP
#include "platforms/posix/src/posix_async.h"
#include "modules/hardcore/mh/messobj.h"
#include "modules/hardcore/mh/mh.h"
#endif // PI_ASYNC_FILE_OP

#include POSIX_LOWFILE_BASEINC

#ifdef SUPPORT_OPFILEINFO_CHANGE
#include <utime.h>
#endif

#undef REALITY_CHECK
/** \def REALITY_CHECK(condition)
 * \brief Assertions not wanted in selftests.
 *
 * Some pi selftests deliberately do things that shouldn't happen in reality.
 * We don't want assertion failures in selftests, but it does make sense to
 * assert relevant things don't happen in other contexts.  This macro should
 * only be used where we *do* return an error code; it should help callers know
 * why what they're doing is dumb.
 */
#ifdef SELFTEST
#define REALITY_CHECK(x)
#else
#define REALITY_CHECK(x) OP_ASSERT(x)
#endif

#undef OPFILE_LENGTH_CHECK
#ifdef OPFILE_LENGTH_IS_SIGNED
#define OPFILE_LENGTH_CHECK(len) OP_ASSERT(len >= 0 && len != FILE_LENGTH_NONE)
#else
#define OPFILE_LENGTH_CHECK(len) OP_ASSERT(len != FILE_LENGTH_NONE)
#endif // OPFILE_LENGTH_IS_SIGNED

/* TODO: implement async using aio_{read,write}, calling aio_error until ready,
 * then using aio_return to find out what to say to the call-back.
 */
/* TODO: add some logging. */

/** Implement OpLowLevelFile; see modules/pi/system/OpLowLevelFile.h */
class PosixLowLevelFile : public PosixOpLowLevelFileBase, public PosixLogger
{
#ifdef PI_ASYNC_FILE_OP
	OpLowLevelFileListener *m_ear;

	class DiskOpReport : public Link
	{
		OpLowLevelFileListener * const m_listener;
		OpLowLevelFile * const m_file;
		OP_STATUS m_result;
		bool m_ready;
	protected:
		virtual void Report(OpLowLevelFileListener* listener,
							OpLowLevelFile *file, OP_STATUS result) = 0;
	public:
		DiskOpReport(OpLowLevelFileListener* listener, OpLowLevelFile *file, Head *queue)
			: m_listener(listener), m_file(file), m_result(OpStatus::ERR), m_ready(false)
			{ IntoStart(queue); }
		bool Ready() { return m_ready; }
		void Report() { Report(m_listener, m_file, m_result); }

		/** Record outcome of asynchronous I/O operation.
		 *
		 * Note that (only) this method may be called from a non-main thread.
		 * Derived classes may embelish this method with more data than just a
		 * status; they should also embelish the Report() chain so as to deliver
		 * that data.  Derived extensions of this method should call this method
		 * @em after everything else they do: it must be the last thing that
		 * happens to this object other than on the main thread.
		 *
		 * Subject to that proviso, ReportQueue::Flush()'s iteration should be
		 * safe, as setting m_ready is the very last thing done on a non-main
		 * thread, so nothing should mind if this is deleted immediately (from
		 * any given thread's point of view) after a successful call to Ready().
		 */
		void Record(OP_STATUS result)
		{
			OP_ASSERT(!m_ready); // Method should only be called once on each object !
			m_result = result;
			PosixAsyncManager::PostSyncMessage(MSG_POSIX_ASYNC_DONE, (MH_PARAM_1)GetList());
			// *after* posting the message: otherwise, we'll need a mutex when threaded !
			m_ready = true;
		}
	};

	class IOReport : public DiskOpReport
	{
		OpFileLength m_size;
	protected:
		virtual void Report(OpLowLevelFileListener* listener,
							OpLowLevelFile *file, OP_STATUS result,
							OpFileLength size) = 0;
		virtual void Report(OpLowLevelFileListener* listener,
							OpLowLevelFile *file, OP_STATUS result)
		{
			Report(listener, file, result, m_size);
		}
	public:
		IOReport(OpLowLevelFileListener* listener, OpLowLevelFile *file, Head *queue)
			: DiskOpReport(listener, file, queue), m_size(0) {}

		void Record(OP_STATUS result, OpFileLength size)
		{
			OPFILE_LENGTH_CHECK(size);
			m_size = size;
			DiskOpReport::Record(result);
		}
	};

	/** Queue of reports of completion of asynchronous file I/O operations.
	 *
	 * Because all additions to, removals from, and iteration over this queue
	 * happens in the main thread (when threads are in use), it is not necessary
	 * to have a mutex to control list access.  The one potential issue is that
	 * Flush() runs down the list testing each item's ->Ready(), which may be
	 * changed by activity in another thread, which may be in the process of
	 * calling DiskOpReport::Report(), q.v., on an entry in the list.  See that
	 * method's doc for why this should be safe.
	 *
	 * The OpLowLevelFile documentation asserts that calls to async listener
	 * methods, signalling completion of async I/O requests, shall be made in
	 * the same order as the the requests were made.  Consequently, this class
	 * implements a FIFO queue (using Head's First() as most recently queued and
	 * Last() as first to be resolved, since Link provides an IntoStart()
	 * method, but no IntoEnd() method), resolving each entry only after all
	 * earlier entries have been resolved.
	 *
	 * Each entry in this queue shall be the DiskOpReport associated with a
	 * PendingDiskOp; the latter gets deleted by the PosixAsyncManager as soon
	 * as it's been Run(), and this may complete before some other async I/O
	 * operation (even in threadless operation, if the POSIX aio_* methods are
	 * in use) which
	 */
	class ReportQueue : public AutoDeleteHead, public MessageObject
	{
		DiskOpReport * Last() { return static_cast<DiskOpReport *>(Head::Last()); }
	public:
		/** Prerequisites for even considering async: */
		static bool Allowable()
		{
			return g_opera && g_main_message_handler &&
				g_op_system_info && g_op_system_info->IsInMainThread() &&
				g_posix_async && g_posix_async->IsActive();
		}
		/** True iff async operations shall be possible. */
		bool Init()
		{
			return (Allowable() &&
					OpStatus::IsSuccess(
						g_main_message_handler->SetCallBack(
							this, MSG_POSIX_ASYNC_DONE, (MH_PARAM_1)this)));
		}
		void Stop() { g_main_message_handler->UnsetCallBacks(this); }
		void Flush();
		virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
		{
			OP_ASSERT(par1 == (MH_PARAM_1)this && par2 == 0 &&
					  msg == MSG_POSIX_ASYNC_DONE);
			Flush();
		}
	} m_async_reports; // list of DiskOpReport for PendingDiskOp

	const bool m_can_async;
	bool ForbidAsync() const
		{ return !(m_ear && m_can_async && m_async_reports.Allowable()); }

	/** Pending Disk Operation.
	 *
	 * Base-class for asynchronous file operations; each of these needs to be
	 * able to report their outcomes via a DiskOpReport.
	 */
	class PendingDiskOp : public PosixAsyncManager::PendingFileOp
	{
		DiskOpReport *const m_report;
	protected:
		DiskOpReport *Reporter() const { return m_report; }
	public:
		PendingDiskOp(PosixLowLevelFile *boss, DiskOpReport *reporter)
			: PosixAsyncManager::PendingFileOp(boss), m_report(reporter)
			{ OP_ASSERT(boss); }

		virtual bool TryLock()
		{
#ifdef POSIX_ASYNC_CANCELLATION
			/* Don't allow a thread that runs a disk operation to be cancelled
			 * until Run() has finished */
			PosixThread::DisableCancellation();
#endif
			return true;
		} // TODO: synchronize with Sync

		virtual void RunDiskOp() = 0;
		virtual void Run()
		{
			RunDiskOp();
#ifdef POSIX_ASYNC_CANCELLATION
			// After Run() has finished we allow thread cancellation again:
			PosixThread::EnableCancellation();
#endif
		}
	};

	class PendingIO : public PendingDiskOp
	{
		OpFileLength const m_pos;

	protected:
		virtual OP_STATUS DoChunk(OpFileLength *size) = 0;
		IOReport *Reporter() const
			{ return static_cast<IOReport *>(PendingDiskOp::Reporter()); }

	public:
		PendingIO(PosixLowLevelFile *boss, OpFileLength offset, IOReport *reporter)
			: PendingDiskOp(boss, reporter), m_pos(offset) {}

		virtual void RunDiskOp()
		{
			OP_STATUS result;
			OpFileLength size;
			if (m_pos == FILE_LENGTH_NONE)
				result = DoChunk(&size);
			else
			{
				PosixLowLevelFile *posix = static_cast<PosixLowLevelFile*>(Boss());
				posix->LockIO();

				OpFileLength prior;
				result = posix->GetFilePos(&prior);
				if (OpStatus::IsSuccess(result))
				{
					result = posix->SetFilePos(m_pos);
					if (OpStatus::IsSuccess(result))
						result = DoChunk(&size);

					OP_STATUS restore = posix->SetFilePos(prior);
					if (OpStatus::IsSuccess(result))
						result = restore;
					else // else we're really in trouble and reporting it poorly !
						OP_ASSERT(OpStatus::IsSuccess(restore));
				}
				else
					size = 0;

				posix->UnlockIO();
			}
			Reporter()->Record(result, size);
		}
	}; // Read/Write base class

# ifdef THREAD_SUPPORT
	PosixMutex m_mutex;
#define HaveLLFileMutex
	friend class PendingIO;
	void   LockIO() { if (m_ear) m_mutex.Acquire(); }
	void UnlockIO() { if (m_ear) m_mutex.Release(); }
# endif // THREAD_SUPPORT
#endif // PI_ASYNC_FILE_OP
#ifndef HaveLLFileMutex
	void LockIO() {}
	void UnlockIO() {}
#endif

	static OP_STATUS ErrNoToStatus(int err, OP_STATUS other=OpStatus::ERR)
	{
		/* It may be worth doing something fancy like the except flag in
		 * lingogi/pi_impl/lingogilowlevelfile.cpp's TranslateError(). */
		RETURN_IF_MEMORY_ERROR(other);

		switch (err)
		{
		case ENOMEM: case ENOBUFS: return OpStatus::ERR_NO_MEMORY;
		case ENOSPC: return OpStatus::ERR_NO_DISK;
		case ENOENT: case ENOTDIR: return OpStatus::ERR_FILE_NOT_FOUND;

		case EEXIST: case ENOTEMPTY: // rmdir(), non-empty directory
		case EBUSY: // rmdir(), directory in use
		case EACCES: case EROFS: case EPERM: // not allowed

			// See notes on bug 166617:
		case ENXIO: case EOVERFLOW: return OpStatus::ERR /* _NOT_SUPPORTED */;
		case EFBIG: // user/process/file (not disk) size limit exceeded
			return OpStatus::ERR /* _NO_ACCESS */;
		case 0: OP_ASSERT(!"Should only be called after something failed !");
		}

		return other;
	}

#ifdef CHECK_SEEK_ON_TRANSITION
	enum { FLOATING, READING, WRITING } m_last_op;
#endif

	OpString m_uni_filename;
#ifdef POSIX_SERIALIZE_FILENAME
	uni_char * m_serial_filename;
	OP_STATUS SetSerial()
	{
# ifdef POSIX_OK_SYS
		if (g_opera && g_op_system_info)
		{
			m_serial_filename =
				g_posix_system_info->SerializeFileName(m_uni_filename.CStr());

			if (!m_serial_filename)
				return OpStatus::ERR_NO_MEMORY;
		}
		else
# endif // POSIX_OK_SYS
			RETURN_IF_ERROR(PosixFileUtil::EncodeEnvironment(m_uni_filename.CStr(),
															 &m_serial_filename));

		return OpStatus::OK;
	}
#else
	OP_STATUS SetSerial() { return OpStatus::OK; }
#endif // POSIX_SERIALIZE_FILENAME

	/** The file-name in native encoding, NULL if non-encodable.
	 *
	 * Correct behaviour (as far as util's ZipFile is concerned; see
	 * util.zipload selftest) is that we must be able to *create* a file-object
	 * with bad name - which shall then report that it doesn't exist and can't
	 * be created.  To represent this case, m_native_filename can be empty.
	 *
	 * Otherwise, it holds the native-encoded version of the uni_char file
	 * requested; this may be a relative path, if that's what was requested, for
	 * all that m_uni_filename is always absolute.
	 */
	OpString8 m_native_filename;

	FILE * m_fp;
	int m_fno;
	unsigned int m_mode;
	enum {
		POSIX_FILE_WRITE_MASK = OPFILE_WRITE | OPFILE_APPEND | OPFILE_UPDATE,
		POSIX_FILE_READ_MASK = OPFILE_READ | OPFILE_UPDATE,
		POSIX_FILE_OPEN_MASK = POSIX_FILE_WRITE_MASK | POSIX_FILE_READ_MASK
		// Note: ignore OPFILE_TEXT: it makes no difference on POSIX systems.
	};

#ifdef POSIX_APPEND_BY_HAND_HACK // TWEAK_POSIX_APPEND_BY_HAND_HACK
	bool m_file_pos_dirty;
	OP_STATUS FixupFilePos()
	{
		errno = 0;
		if (0 > fseeko(m_fp, 0, SEEK_END))
		{
			int err = errno;
			OP_ASSERT(err); // might modify errno
			return ErrNoToStatus(err);
		}

		m_file_pos_dirty = false;
		return OpStatus::OK;
	}
#endif
	bool m_commit;
	bool m_locked;

	/** Miscellaneous sanity-checks and preparations for a freshly-opened file descriptor.
	 *
	 * Packaged as separate method to simplify clean-up on failure.
	 */
	OP_STATUS FinishOpen(int mode);

	/** Initialize from a native string; used by LocalTempFile() and CreateCopy(). */
	OP_STATUS Init(const char *name, const uni_char *uni=0);
	OP_STATUS LocalTempFile(const uni_char* prefix, PosixLowLevelFile** file) const;

	/** Ensure directory exists (and log doing so). */
	OP_STATUS EnsureDir() const
		{ return PosixFileUtil::CreatePath(m_native_filename.CStr(), true); }
	/** Uses fstat when open, else stat. */
	OP_STATUS RawStat(struct stat *st) const;

#ifdef POSIX_LOWFILE_WRAPPABLE
	friend class PosixModule;
#else
	friend class OpLowLevelFile;
#endif // POSIX_LOWFILE_WRAPPABLE
	PosixLowLevelFile()
		: PosixLogger(STREAM)
#ifdef PI_ASYNC_FILE_OP
		, m_ear(0)
		, m_can_async(m_async_reports.Init())
#endif
#ifdef HaveLLFileMutex
		, m_mutex(PosixMutex::MUTEX_RECURSIVE)
#endif
#ifdef CHECK_SEEK_ON_TRANSITION
		, m_last_op(FLOATING)
#endif
#ifdef POSIX_SERIALIZE_FILENAME
		, m_serial_filename(0) // lazily evaluated if/when asked for
#endif
		, m_fp(0)
		, m_fno(-1)
		, m_mode(0)
#ifdef POSIX_APPEND_BY_HAND_HACK
		, m_file_pos_dirty(false)
#endif
		, m_commit(false)
		, m_locked(false) {}

	/** Initialize from a Unicode string as path name; used by OpLowLevelFile::Init()
	 */
	OP_STATUS Init(const uni_char *name, bool serial);

public:
	virtual ~PosixLowLevelFile();
	virtual const char* Type() const { return "PosixLowLevelFile"; }
	virtual const uni_char* GetFullPath() const { return m_uni_filename.CStr(); }
	virtual const uni_char* GetSerializedName() const;
	virtual OpLowLevelFile* CreateTempFile(const uni_char* prefix)
	{
		PosixLowLevelFile* ans = 0;
		return OpStatus::IsSuccess(LocalTempFile(prefix, &ans)) ? ans : 0;
	}
	virtual OpLowLevelFile* CreateCopy();

	virtual OP_STATUS GetFileInfo(OpFileInfo* info);
	virtual OP_STATUS GetFileLength(OpFileLength* len) const;
	virtual BOOL IsWritable() const;
	virtual OP_STATUS Exists(BOOL* exists) const;
#ifdef SUPPORT_OPFILEINFO_CHANGE
	virtual OP_STATUS ChangeFileInfo(const OpFileInfoChange* changes);
#endif

	// For when a path has a native language form:
	virtual OP_STATUS GetLocalizedPath(OpString *localized_path) const
		{ return OpStatus::ERR; }

	virtual OP_STATUS CopyContents(const OpLowLevelFile* source);
	virtual OP_STATUS SafeReplace(OpLowLevelFile* source);
	virtual OP_STATUS Delete();
	virtual OP_STATUS MakeDirectory();

	virtual BOOL IsOpen() const { return m_fp != 0; }
	virtual OP_STATUS Open(int mode);
	virtual OP_STATUS Close();
	virtual OP_STATUS SafeClose(); // close and ensure data hits physical disk

	// When open:
	virtual BOOL Eof() const { return m_fp && feof(m_fp); }
	virtual OP_STATUS Read(void* data, OpFileLength len, OpFileLength* bytes_read);
	virtual OP_STATUS ReadLine(char** data);
	virtual OP_STATUS GetFilePos(OpFileLength* pos) const;
	virtual OP_STATUS SetFilePos(OpFileLength pos, OpSeekMode seek_mode=SEEK_FROM_START);
	virtual OP_STATUS SetFileLength(OpFileLength len);
	virtual OP_STATUS Write(const void* data, OpFileLength len);
	virtual OP_STATUS Flush();

#ifdef PI_ASYNC_FILE_OP
	virtual void SetListener (OpLowLevelFileListener *listener)
		{ OP_ASSERT(!IsAsyncInProgress()); m_ear = listener; }
	virtual OP_STATUS ReadAsync(void* data, OpFileLength len,
								OpFileLength abs_pos = FILE_LENGTH_NONE);
	virtual OP_STATUS WriteAsync(const void* data, OpFileLength len,
								 OpFileLength abs_pos = FILE_LENGTH_NONE);
	virtual OP_STATUS DeleteAsync();
	virtual OP_STATUS FlushAsync();
	virtual BOOL IsAsyncInProgress();
	virtual OP_STATUS Sync();
#endif // PI_ASYNC_FILE_OP

#ifdef POSIX_EXTENDED_FILE // DesktopOpLowLevelFile's extensions:
    OP_STATUS Move(const PosixOpLowLevelFileBase *new_file);
    OP_STATUS Stat(struct stat *buffer);
	OP_STATUS ReadLine(char *string, int max_length);
#endif
};

// static
OP_STATUS
#ifdef POSIX_LOWFILE_WRAPPABLE
PosixModule::CreateRawFile
#else
OpLowLevelFile::Create
#endif
					  (OpLowLevelFile** new_file,
					   const uni_char* path, BOOL serialized)
{
	PosixLowLevelFile *mine = OP_NEW(PosixLowLevelFile, ());
	OP_STATUS res = mine ? mine->Init(path, serialized) : OpStatus::ERR_NO_MEMORY;
	if (OpStatus::IsSuccess(res))
		*new_file = mine;
	else
	{
		*new_file = 0;
		OP_DELETE(mine);
	}
	return res;
}

/** Package umask munging.
 *
 * When we delegate to POSIX API functions that'll be creating files, but which
 * don't take a file mode parameter, we have to set umask if we're being strict
 * about file permissions (we like to use 0077 - i.e. only owner can do anything
 * to what we create, making it private - when we can).  However, when we do
 * that, we should restore umask afterwards.  Instantiating a PosixUMaskGuard
 * for the duration of the relevant file-creation activity takes care of
 * restoring the original umask (if we over-rode it) when finished.
 */
class PosixUMaskGuard
{
	const bool m_needed;
	const mode_t m_mask;
public:
	PosixUMaskGuard(bool force=false)
		: m_needed(force || PosixFileUtil::UseStrictFilePermission())
		, m_mask(m_needed ? umask(0077) : 0) {}

	~PosixUMaskGuard()
	{
		if (m_needed)
			umask(m_mask);
	}
};

PosixLowLevelFile::~PosixLowLevelFile()
{
#ifdef PI_ASYNC_FILE_OP
	// without calling associated listener methods,
	if (m_can_async && m_async_reports.Allowable())
	{
		m_async_reports.Stop();
		// complete any asynchronous file operatons
		Sync();
		// (leaving m_async_reports's destructor to delete all the report objects)
	}
	// and *subsequently*
#endif
	// close the file
	Close();
#ifdef POSIX_SERIALIZE_FILENAME
	OP_DELETEA(m_serial_filename);
#endif
#ifdef CHECK_SEEK_ON_TRANSITION
	OP_ASSERT(FLOATING == m_last_op);
#endif
}

OP_STATUS PosixLowLevelFile::Init(const char *name, const uni_char *uni /* = 0 */)
{
	OP_ASSERT(name || uni);
	if (!name && !uni)
		return OpStatus::ERR_NULL_POINTER;

#ifdef CHECK_SEEK_ON_TRANSITION
	OP_ASSERT(FLOATING == m_last_op);
#endif

	if (name)
	{
		// Skip trailing '/' except when name is "/".
		size_t end = op_strlen(name);
		while (end > 1 && name[end-1] == PATHSEPCHAR)
			end--;

		RETURN_IF_ERROR(m_native_filename.Set(name, end));
	}

	if (uni)
	{
		// Skip trailing '/' except when name is "/".
		size_t end = uni_strlen(uni);
		while (end > 1 && uni[end-1] == PATHSEPCHAR)
			end--;
		RETURN_IF_ERROR(m_uni_filename.Set(uni));
	}
	else if (m_native_filename.HasContent())
	{
		char full_path[_MAX_PATH+1];	// ARRAY OK 2010-08-12 markuso
		RETURN_IF_ERROR(PosixFileUtil::FullPath(m_native_filename.CStr(), full_path));
		RETURN_IF_ERROR(PosixNativeUtil::FromNative(full_path, &m_uni_filename));
	}

	return SetSerial();
}

OP_STATUS PosixLowLevelFile::Init(const uni_char *name, bool
#ifdef POSIX_SERIALIZE_FILENAME
								  serial
#endif
	)
{
	OP_ASSERT(name);
	if (!name)
		return OpStatus::ERR_NULL_POINTER;

#ifdef CHECK_SEEK_ON_TRANSITION
	OP_ASSERT(FLOATING == m_last_op);
#endif

	// Strip away "file://localhost"; part of fix for bug #210692.
	if (uni_strncmp(name, UNI_L("file://localhost"), 16) == 0)
	{
		OP_ASSERT(!"Something upstack should have stripped that prefix !");
		name += 16;
	}

#ifdef POSIX_SERIALIZE_FILENAME
	OpString expanded;
	if (serial)
	{
		RETURN_IF_ERROR(
# ifdef POSIX_OK_SYS
			(g_opera && g_op_system_info)
			? g_posix_system_info->ExpandSystemVariablesInString(name, &expanded) :
# endif
			PosixFileUtil::DecodeEnvironment(name, &expanded));
		name = expanded.CStr();
	}
#endif // POSIX_SERIALIZE_FILENAME

	// Skip trailing '/' except when name is "/".
	size_t end = uni_strlen(name);
	while (end > 1 && name[end-1] == PATHSEPCHAR)
		end--;

	OP_STATUS err = PosixNativeUtil::ToNative(name, &m_native_filename, end);
	if (err == OpStatus::ERR_PARSING_FAILED)
		// Filename is not representable in native encoding; file cannot exist.
		OP_ASSERT(m_native_filename.IsEmpty()); // i.e. it wasn't modified
	else
		RETURN_IF_ERROR(err);

	// Need absolute path for m_uni_filename:
	if (name[0] == PATHSEPCHAR ||
		/* bodge round CORE-25660: util/opfile/opfile.ot's test("Exists") is bogus. */
		name[0] == '\0')
		RETURN_IF_ERROR(m_uni_filename.Set(name, end));
	else
	{
		char path[_MAX_PATH + 1];	// ARRAY OK 2010-08-12 markuso
		RETURN_IF_ERROR(PosixFileUtil::FullPath(name, path));
		path[_MAX_PATH] = '\0'; // just to be sure
		RETURN_IF_ERROR(PosixNativeUtil::FromNative(path, &m_uni_filename));
	}

	return SetSerial(); // must be called *after* m_uni_filename is set.
}

const uni_char* PosixLowLevelFile::GetSerializedName() const
{
#ifdef POSIX_SERIALIZE_FILENAME
	return m_serial_filename;
#else
	return m_uni_filename.CStr();
#endif
}

OP_STATUS PosixLowLevelFile::RawStat(struct stat *st) const // private
{
	int ret;
	errno = 0;
	if (m_fp && m_fno >= 0)
	{
		/* Need to fflush before we call fstat(), when in a write-mode, or local
		 * changes aren't saved: */
		ret = m_mode & POSIX_FILE_WRITE_MASK ? fflush(m_fp) : 0;
		if (ret == 0)
			ret = fstat(m_fno, st);
	}
	else if (m_native_filename.HasContent())
	{
		ret = stat(m_native_filename.CStr(), st);
		if (ret) // might be a broken symlink:
			ret = lstat(m_native_filename.CStr(), st);
	}
	else
		return OpStatus::ERR;

	return ret == 0 ? OpStatus::OK : ErrNoToStatus(errno);
}

OP_STATUS PosixLowLevelFile::GetFileLength(OpFileLength* len) const
{
	struct stat st;
	RETURN_IF_ERROR(RawStat(&st));
	if (st.st_size < 0)
		return OpStatus::ERR; // _OUT_OF_RANGE;
	*len = st.st_size;
	off_t siz = *len;
	if (siz != st.st_size)
		return OpStatus::ERR; // _OUT_OF_RANGE;

	return OpStatus::OK;
}

BOOL PosixLowLevelFile::IsWritable() const
{
	if (m_native_filename.IsEmpty())
		return FALSE;

	errno = 0;
	int ret = access(m_native_filename.CStr(), W_OK);

	if (ret != 0 && ENOENT == errno)
		/* If we get an ENOENT, the file we asked for does not exist.  We need
		 * to check the directories it is in to see if we can create it. */
		if (m_native_filename.Length() < _MAX_PATH)
		{
			char parent[_MAX_PATH]; // ARRAY OK 2010-08-12 markuso
			op_strcpy( parent, m_native_filename.CStr() );

			while (char* lastslash = op_strrchr(parent, PATHSEPCHAR))
			{
				lastslash[0] = 0;

				errno = 0;
				ret = access(parent, W_OK);
				if (ret == 0 || errno != ENOENT)
					break;
			}
		}

	return 0 == ret;
}

#ifdef SUPPORT_OPFILEINFO_CHANGE
OP_STATUS PosixLowLevelFile::ChangeFileInfo(const OpFileInfoChange* changes)
{
	if (m_native_filename.IsEmpty())
		return OpStatus::ERR;

	OP_ASSERT(changes->flags); // else you're wasting your time !
	if (m_fp) return OpStatus::ERR; // not allowed on open file.
	const unsigned int supported =
		OpFileInfoChange::LAST_MODIFIED | OpFileInfoChange::WRITABLE;
	/* TODO: research how to support CREATION_TIME (e.g. see source of GNU
	 * tar).  When supported, this should be done last in the function: each
	 * of the other kinds of change sets st_ctime to the present. */

	if (changes->flags & ~supported)
		return OpStatus::ERR_NOT_SUPPORTED;

	struct stat prior;
	RETURN_IF_ERROR(RawStat(&prior));

	if (changes->flags & OpFileInfoChange::LAST_MODIFIED)
	{
		struct /* { time_t actime, modtime; } */ utimbuf times;
		times.actime = prior.st_mtime;
		times.modtime = changes->last_modified;
		errno = 0;
		if (utime(m_native_filename.CStr(), &times))
			return ErrNoToStatus(errno);
	}

	if (changes->flags & OpFileInfoChange::WRITABLE)
	{
		const mode_t my_write = S_IWUSR, any_write = (S_IWUSR|S_IWGRP|S_IWOTH);
		const mode_t want = changes->writable
			? (prior.st_mode | my_write) : (prior.st_mode & ~any_write);
		errno = 0;
		if (want != prior.st_mode && // else: no-op
			chmod(m_native_filename.CStr(), want))
			return ErrNoToStatus(errno);

		if (!changes->writable && IsWritable())
			/* Apparently we're root !  What was asked for is impossible.
			 * See discussion on CORE-38336. */
			return getuid() ? OpStatus::ERR : OpStatus::ERR_NOT_SUPPORTED;
	}

	return OpStatus::OK;
}
#endif

OP_STATUS PosixLowLevelFile::GetFileInfo(OpFileInfo* info)
{
	const unsigned int supported =
		OpFileInfo::LENGTH | OpFileInfo::MODE |
		OpFileInfo::CREATION_TIME | OpFileInfo::LAST_MODIFIED |
		OpFileInfo::FULL_PATH | OpFileInfo::SERIALIZED_NAME |
		OpFileInfo::HIDDEN |
		OpFileInfo::OPEN | OpFileInfo::POS | OpFileInfo::WRITABLE;

	if (info->flags & ~supported)
		return OpStatus::ERR_NOT_SUPPORTED;

	if (info->flags &
		(OpFileInfo::LENGTH | OpFileInfo::MODE |
		 OpFileInfo::CREATION_TIME | OpFileInfo::LAST_MODIFIED))
	{
		struct stat st;
		RETURN_IF_ERROR(RawStat(&st));

		if (info->flags & OpFileInfo::LENGTH)
			info->length = st.st_size;

		// Note, on Linux: st_[amc]tim.{tv_sec,tv_nsec} are the actual data.
		if (info->flags & OpFileInfo::LAST_MODIFIED)
			info->last_modified = st.st_mtime;

		if (info->flags & OpFileInfo::CREATION_TIME)
			info->creation_time = st.st_ctime;

		if (info->flags & OpFileInfo::MODE)
		{
			if (S_ISDIR(st.st_mode))
				info->mode = OpFileInfo::DIRECTORY;
			else if (S_ISREG(st.st_mode))
				info->mode = OpFileInfo::FILE;
			else if (S_ISLNK(st.st_mode))
				info->mode = OpFileInfo::SYMBOLIC_LINK;
			else
				info->mode = OpFileInfo::NOT_REGULAR;
		}
	}

	if (info->flags & OpFileInfo::FULL_PATH)
		info->full_path = m_uni_filename.CStr();

	if (info->flags & OpFileInfo::SERIALIZED_NAME)
		info->serialized_name = GetSerializedName();

	if (info->flags & OpFileInfo::WRITABLE)
		info->writable = IsWritable();

	if (info->flags & OpFileInfo::OPEN)
		info->open = m_fp != 0;

	if (info->flags & OpFileInfo::POS)
		RETURN_IF_ERROR(GetFilePos(&info->pos));

	if (info->flags & OpFileInfo::HIDDEN)
	{
		if (m_native_filename.HasContent())
		{
			char *base = op_strrchr(m_native_filename.CStr(), PATHSEPCHAR);
			if (base) base++; // first character after last /
			else base = m_native_filename.CStr(); // start of full path
			info->hidden = base[0] == '.';
		}
		else
			return OpStatus::ERR;
	}

	return OpStatus::OK;
}

// private
OP_STATUS PosixLowLevelFile::FinishOpen(int mode)
{
	// Check it's not a directory (c.f. bug #209748, but generally prudent):
	struct stat st;
	int res = fstat(m_fno, &st);
	if (res == 0 && S_ISDIR(st.st_mode))
		return OpStatus::ERR_NO_ACCESS;

	int err = errno = 0;
	// Set close on exec, e.g. bug #230354:
	int flags = fcntl(m_fno, F_GETFD, 0);
	if (flags + 1 && fcntl(m_fno, F_SETFD, flags | FD_CLOEXEC) + 1)
	{
		// Partial support for (optional) file locking:
#ifdef POSIX_FILE_REFLEX_LOCK // see TWEAK_POSIX_FILE_REFLEX_LOCK
		/* See DSK-247381; make SHAREDENY flags fatuous where sensible: */
		enum {
			LOCK_EXCLUSIVE = OPFILE_SHAREDENYREAD|OPFILE_WRITE|OPFILE_APPEND|OPFILE_UPDATE,
			LOCK_INCLUSIVE = OPFILE_SHAREDENYWRITE|OPFILE_READ
		};
		OP_ASSERT(!(OPFILE_READ & LOCK_EXCLUSIVE)); // that would be problematic ...
#else // Only request locks when explicitly told to do so:
		enum {
			LOCK_EXCLUSIVE = OPFILE_SHAREDENYREAD,
			LOCK_INCLUSIVE = OPFILE_SHAREDENYWRITE
		};
#endif // POSIX_FILE_REFLEX_LOCK
		bool exclusive = (mode & LOCK_EXCLUSIVE) != 0;
		if (exclusive || (mode & LOCK_INCLUSIVE))
		{
			/* Testing this would need a second process, since the locks are
			 * only meant to lock other processes out from doing various things.
			 */
#ifdef F_SETLK64
			struct flock64 lock;
			const int cmd = F_SETLK64;
#else
			struct flock lock;
			const int cmd = F_SETLK;
#endif
			lock.l_type = exclusive ? F_WRLCK : F_RDLCK;
			lock.l_whence = SEEK_SET;
			lock.l_start = 0;
#if 0
			/* We shouldn't need this; but it illustrates how to do it if we hit
			 * an imperfect implementation.  See #else clause for why we
			 * shouldn't need to do this.
			 *
			 * Construct max value of signed type (c.f. INT_LIMIT macro used in
			 * glibc headers): */
			lock.l_len = 1; // saved to variable so <<= acts on the right-sized type:
			lock.l_len <<= sizeof(lock.l_len) * CHAR_BIT - 1;
			lock.l_len = ~lock.l_len;
			lock.l_pid = getpid();
#else // Trust the POSIX spec:
			lock.l_len = 0; // magic token for max off_t
			lock.l_pid = 0; // spec doesn't require us to set it
#endif
			errno = 0;
			int ret = fcntl(m_fno, cmd, &lock);
			if (ret + 1)
				m_locked = true;
			else // Failed to lock
			{
				err = errno;
				OP_ASSERT(err);
			}
		}
	}
	else // Failed FD_CLOEXEC
	{
		err = errno;
		OP_ASSERT(err);
	}

	if (err)
		switch (err)
		{
		case EDEADLK: // blocking to wait for lock would deadlock
		case EACCES: // forbidden
		case EAGAIN: // conflicting lock
#ifdef POSIX_LENIENT_READ_LOCK // TWEAK_POSIX_LENIENT_READ_LOCK
			if (mode == OPFILE_READ)
			{
				OpString home_folder;
				OP_STATUS res = g_folder_manager->GetFolderPath(OPFILE_HOME_FOLDER, home_folder);
				if (OpStatus::IsSuccess(res) && home_folder.HasContent())
				{
					int folder_length = home_folder.Length();
					bool within_folder = !m_uni_filename.Compare(home_folder.CStr(), folder_length);
					if (within_folder)
					{
						// Test for trailing PATHSEPCHAR. File may not be within folder after all
						if (home_folder[folder_length-1] != PATHSEPCHAR)
						{
							int file_length = m_uni_filename.Length();
							if (file_length >= folder_length && m_uni_filename[folder_length] != PATHSEPCHAR)
								within_folder = false;
						}
					}
					if (!within_folder)
						break;
				}
			}
#endif // POSIX_LENIENT_READ_LOCK

			Log(CHATTY, "Locking prevented access to: %s\n", m_native_filename.CStr());
			return OpStatus::ERR_NO_ACCESS;

			/* Not unambiguously clear what we should do for other errors; but
			 * setting locks is optional, so treat as ignored failure. */
		case EINVAL:
		default:
			Log(CHATTY, "Unexpected error (%d) from fcntl", err);
			// Deliberate fall-through (cases listed to avoid logging):
		case ENOMEM: case ENOBUFS: // OOM
		case ENOLCK: // too many locks set already
		case EBADF: // lock type incompatible with open mode
			Log(CHATTY, "Failed to lock (%s): %s\n",
				strerror(err), m_native_filename.CStr());
			break;
		}

#ifdef CHECK_SEEK_ON_TRANSITION
	OP_ASSERT(FLOATING == m_last_op);
#endif
#ifdef POSIX_APPEND_BY_HAND_HACK
	if (0 != (m_mode & OPFILE_APPEND))
		RETURN_IF_ERROR(FixupFilePos());
#endif // uclibc doesn't position at end when opening in append mode :-(
	return OpStatus::OK;
}

OP_STATUS PosixLowLevelFile::Open(int mode)
{
	if (m_fp)
	{
		OP_ASSERT(!"I'm already open !");
		return OpStatus::ERR;
	}
	// Note, however, that m_fno may be set, iff this object is a TempFile().

	if (m_native_filename.IsEmpty())
		return OpStatus::ERR;

	const char *modestr;
	switch (m_mode = mode & POSIX_FILE_OPEN_MASK)
	{
	case OPFILE_READ: // simple read; cannot be combined with deny-read.
		if (mode & OPFILE_SHAREDENYREAD)
		{
			m_mode = 0;
			REALITY_CHECK(!"Bad mode specified: deny-read w/o write");
			return OpStatus::ERR;
		}
		modestr = "r";
		break;

		// Truncate to zero length and write:
	case OPFILE_WRITE:					modestr = "w";	break;
		// Write at end (regardless of any fseeko):
	case OPFILE_APPEND:					modestr = "a";	break;
		// Update; (random access) read and write:
	case OPFILE_UPDATE:					modestr = "r+";	break;
		// Truncate and write, but be able to read back:
	case OPFILE_OVERWRITE:				modestr = "w+";	break;
		// Write at end and be able (via seekto) to read anywhere:
	case OPFILE_READ | OPFILE_APPEND:	modestr = "a+";	break;
		// All other combinations are (deprecated and) too complicated !
	default:
		m_mode = 0;
		REALITY_CHECK(!"Unrecognised open mode");
		return OpStatus::ERR;
	}
	m_commit = (mode & OPFILE_COMMIT) != 0;

	/* Try to open file first. If that fails and the file is opened for writing,
	 * then attempt to create directory and open again.  I do not want to create
	 * the path before the first fopen() because we have many distinct files in
	 * existing directories [espen 2005-06-23] */

	do { // Non-looping loop; break on success (simplifies conditional mess).
		const PosixUMaskGuard ignoreme; // ensure umask is suitable.

		if (m_fno >= 0) // LocalTempFile created this
			/* Fix up open-mode or (at the expense of a race condition) close
			 * ready to be re-opened.  LocalTempFile left m_fno effectively open
			 * in OVERWRITE mode; and it's an initially empty file, so this is
			 * effectively the same as UPDATE.  This mode subsumes WRITE mode,
			 * so we can get away with leaving it as it is, although it's
			 * sub-optimal that we won't fail any attempted read operations;
			 * sadly, fcntl() doesn't provide a way to turn off read mode for
			 * us.  Fortunately, fcntl() \em does provide a way to turn on
			 * append mode, albeit POSIX documentation leaves it unspecified; so
			 * we have to check fcntl succeeds.  It is also possible that our
			 * subsequent fdopen() shall fix up the read permissions of m_fno.
			 */
			switch (m_mode)
			{
			case OPFILE_WRITE: // Endurable ... but a bit naughty.
			case OPFILE_OVERWRITE: // How LocalTempFile() opened it.
			case OPFILE_UPDATE: // Functionally equivalent, as it was empty.
				// use  fdopen on m_fno instead of re-opening the file
				break;

			case OPFILE_APPEND: // Endurable, as for WRITE.
			case OPFILE_APPEND | OPFILE_READ:
			{
				int flags = fcntl(m_fno, F_GETFL);
				OP_ASSERT(!(flags + 1 && flags & O_APPEND));
				// Let's see if we can fix that:
				if (flags + 1 && 1 + fcntl(m_fno, F_SETFL, flags | O_APPEND))
				{
					flags = fcntl(m_fno, F_GETFL);
					if (flags + 1 && flags & O_APPEND)
						// we can use fdopen :-)
						break;
				}
			} // Deliberate fall-through to default action.
			default:
				/* Close and re-open with revised permissions; this creates a
				 * possible race condition.  Callers should be encouraged to use
				 * OPFILE_OVERWRITE or OPFILE_UPDATE on files obtained with
				 * CreateTempFile().
				 */
				OP_ASSERT(!"Possible race condition");
				Close();
				Delete();
			}

		errno = 0;
		m_fp = m_fno < 0
			? fopen(m_native_filename.CStr(), modestr)
			: fdopen(m_fno, modestr);
		if (m_fp) break;

		int err = errno; // save before possible failure in EnsureDir()
		// If we're allowed to create the file, be willing to create its directory:
		if ((mode & POSIX_FILE_WRITE_MASK) != 0 &&
			// ... but don't bother if we're OOM, it won't help !
			!(err == ENOMEM || err == ENOBUFS) &&
			OpStatus::IsSuccess(EnsureDir()))
		{
			errno = 0;
			m_fp = fopen(m_native_filename.CStr(), modestr);
			if (m_fp) break;
			err = errno;
		}

		switch (err)
		{
		case EMFILE: case ENFILE:
			PError("fopen", err, Str::S_ERR_FILE_NO_HANDLE,
				   "Failed to open file.  Check ulimit value.");
			break;
			// Errors not handled by ErrNoToStatus:
		case ETXTBSY: // can't write shared text file
		case EISDIR: // opening a directory file !
		case ENXIO: // special file unavailable
			Log(TERSE, "Failed to open (denied access): %s\n", m_native_filename.CStr());
			return OpStatus::ERR_NO_ACCESS;
		default:
			Log(NORMAL, "Failed to open (%s): %s\n",
				strerror(err), m_native_filename.CStr());
		}
		return ErrNoToStatus(err);
	} while (0); // Non-local jump container.
	m_fno = fileno(m_fp);

	OP_STATUS res = FinishOpen(mode);
	if (OpStatus::IsError(res))
		Close();

	return res;
}

OP_STATUS PosixLowLevelFile::Close()
{
	int ret = errno = 0;
	if (m_fp)
	{
		ret = fclose(m_fp);
		m_fp = 0;
	}
	else if (m_fno >= 0)
		ret = close(m_fno);

	m_mode = 0;
	m_fno = -1;
#ifdef POSIX_APPEND_BY_HAND_HACK
	m_file_pos_dirty = false;
#endif
#ifdef CHECK_SEEK_ON_TRANSITION
	m_last_op = FLOATING;
#endif
	/* <quote src="man 3posix close"> All outstanding record locks owned by the
	 * process on the file associated with the file descriptor shall be removed
	 * (that is, unlocked).  </quote><quote src="man 3posix fclose"> The
	 * fclose() function shall perform the equivalent of a close() on the file
	 * descriptor that is associated with the stream </quote>
	 */
	m_locked = false;

	if (ret) // EOF from fclose, -1 from close
		return ErrNoToStatus(errno);

	return OpStatus::OK;
}

OP_STATUS PosixLowLevelFile::Exists(BOOL* exists) const
{
	errno = 0;
	struct stat st;
	*exists = m_native_filename.HasContent() && 0 == lstat(m_native_filename.CStr(), &st);
	if (*exists)
		return OpStatus::OK;

	switch (errno)
	{
	case EOVERFLOW: // The file does exist, lstat merely can't describe it !
		*exists = TRUE;
		// Deliberate fall-through.
	case ENOENT:  // The file does not exist
	case ENOTDIR: // The path contains one or more folders that do no exist
	case 0: // !HasContent(), i.e. unrepresentable file-name; can't exist.
		return OpStatus::OK; // That's what we were here to determine ;-)
	}
	return ErrNoToStatus(errno);
}

OP_STATUS PosixLowLevelFile::GetFilePos(OpFileLength* pos) const
{
	OP_ASSERT(pos);
	*pos = 0;
	if (!m_fp)
	{
		REALITY_CHECK(!"Asking for position of non-open file !");
		return OpStatus::ERR;
	}

	errno = 0;
	off_t ret = ftello(m_fp);
	if (ret < 0)
		return ErrNoToStatus(errno);
	*pos = ret;
	off_t ter = *pos;
	if (ter != ret)
		return OpStatus::ERR; // _OUT_OF_RANGE

	return OpStatus::OK;
}

OP_STATUS PosixLowLevelFile::SetFilePos(OpFileLength pos,
										OpSeekMode mode /* = SEEK_FROM_START */)
{
	if (!m_fp)
		return OpStatus::ERR;

	OP_ASSERT(sizeof(OpFileLength) == sizeof(off_t));
	/* TODO: deal with sizeof(OpFileLength) != sizeof(off_t).
	 *
	 * In particular, if OpFileLength is unsigned, we need to deal with
	 * interpreting its values as negative when using SEEK_END and some of its
	 * values as negative when using SEEK_CUR.
	 *
	 * If pos is too big for off_t, we could do an initial fseeko() from
	 * where we were asked to, stepping max/min off_t; then from SEEK_CUR
	 * repeatedly in chunks of size min/max off_t until we've attained what we
	 * were asked for.
	 */

	int whence;
	switch (mode)
	{
	case SEEK_FROM_START:	whence = SEEK_SET; OPFILE_LENGTH_CHECK(pos); break;
	case SEEK_FROM_END:		whence = SEEK_END; break;
	case SEEK_FROM_CURRENT:	whence = SEEK_CUR; break;
	default: return OpStatus::ERR;
	}
	errno = 0;
	if (!(1 + fseeko(m_fp, pos, whence)))
		return ErrNoToStatus(errno);

#ifdef POSIX_APPEND_BY_HAND_HACK
	m_file_pos_dirty = (m_mode & OPFILE_APPEND) && !(mode == SEEK_FROM_END && pos == 0);
#endif
#ifdef CHECK_SEEK_ON_TRANSITION
	m_last_op = FLOATING;
#endif
	return OpStatus::OK;
}

OP_STATUS PosixLowLevelFile::Write(const void* data, OpFileLength len)
{
	OPFILE_LENGTH_CHECK(len);
	if (len && !data) return OpStatus::ERR_NULL_POINTER;
	if (!m_fp) return OpStatus::ERR;
	OP_ASSERT(m_mode & POSIX_FILE_WRITE_MASK);
#ifdef CHECK_SEEK_ON_TRANSITION
	OP_ASSERT(m_last_op != READING || (m_mode & OPFILE_APPEND) != 0);
	m_last_op = WRITING;
#endif
	if (len <= 0) return OpStatus::OK; // no-op :-)

	LockIO();

#ifdef POSIX_APPEND_BY_HAND_HACK
	if (m_file_pos_dirty)
		/* We're in append mode and either this is our first Write() or we've
		 * come via a SetFilePos() that didn't leave us at end-of-file.  So
		 * we need to ensure we're at end-of-file, before we start writing.
		 */
		RETURN_IF_ERROR(FixupFilePos());
#endif

	OpFileLength sent = 0;
	errno = 0;
	if (sizeof(OpFileLength) > sizeof(size_t))
	{
		OpFileLength todo = len;
		OpFileLength const block = ~(size_t)0; // size_t is unsigned, so this is its max.
		char *p = (char*)data;
		while (errno == 0 && todo > 0)
		{
			const size_t chunk = todo > block ? block : todo;
			todo -= chunk;
			size_t w = fwrite((void*)p, 1, chunk, m_fp);
			sent += w;
			if (w != chunk) break;
			p += chunk;
		}
	}
	else
		sent = fwrite(data, 1, len, m_fp);

	int err = errno;
	UnlockIO();

	return sent == len ? OpStatus::OK : ErrNoToStatus(err);
}

OP_STATUS PosixLowLevelFile::Read(void* data, OpFileLength len, OpFileLength* bytes_read)
{
	if (!bytes_read)
		return OpStatus::ERR_NULL_POINTER;
	*bytes_read = 0;

	OPFILE_LENGTH_CHECK(len);
	if (len && !data) return OpStatus::ERR_NULL_POINTER;
	if (!m_fp) return OpStatus::ERR;
	if (len <= 0) return OpStatus::OK; // no-op :-)

	OP_ASSERT(m_mode & POSIX_FILE_READ_MASK);
#ifdef CHECK_SEEK_ON_TRANSITION
	OP_ASSERT(m_last_op != WRITING);
	m_last_op = READING;
#endif

	LockIO();

	errno = 0;
	OpFileLength read = 0;
	if (sizeof(OpFileLength) > sizeof(size_t))
	{
		OpFileLength todo = len;
		OpFileLength const block = ~(size_t)0;
		char * p = (char*) data;
		while (errno == 0 && todo > 0)
		{
			const size_t chunk = todo > block ? block : todo;
			todo -= chunk;
			size_t r = fread((void*)p, 1, chunk, m_fp);
			read += r;
			if (r != chunk) break; // not necessarily an error
			p += chunk;
		}
	}
	else
		read = fread(data, 1, len, m_fp);

	*bytes_read = read;

	int err = errno;
	if (ferror(m_fp))
	{
		clearerr(m_fp);
		UnlockIO();

		return ErrNoToStatus(err);
	}
	UnlockIO();
	return OpStatus::OK;
}

OP_STATUS PosixLowLevelFile::ReadLine(char** data)
{
	if (!data) return OpStatus::ERR_NULL_POINTER;
	if (!m_fp) return OpStatus::ERR;

	OP_ASSERT(m_mode & POSIX_FILE_READ_MASK);
#ifdef CHECK_SEEK_ON_TRANSITION
	OP_ASSERT(m_last_op != WRITING);
	m_last_op = READING;
#endif

	char buf[4096];					// ARRAY OK 2010-08-12 markuso
	OpString8 temp;
	LockIO();

	// fgets() converts DOS-style CRLF newlines to LF.
	errno = 0;
	while (fgets(buf, 4096, m_fp) == buf)
	{
		RETURN_IF_ERROR(temp.Append(buf));

		int len = temp.Length();
		if (len > 0 && temp[len - 1] == '\n')
			// PI spec: "The newline character should not be stored in the result string."
			temp.Delete(len - 1);
		else if (!feof(m_fp))
		{
			errno = 0;
			continue;
		}

		*data = SetNewStr(temp.CStr());
		UnlockIO();
		return *data ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
	}
	int err = errno;
	if (!feof(m_fp) || ferror(m_fp))
	{
		*data = 0;
		clearerr(m_fp);
		UnlockIO();

		return ErrNoToStatus(err);
	}
	UnlockIO();

	// end of file needs empty string.
	*data = OP_NEWA(char, 1);
	if (!*data)
		return OpStatus::ERR_NO_MEMORY;

	**data = 0;
	return OpStatus::OK;
}

OpLowLevelFile* PosixLowLevelFile::CreateCopy()
{
#ifdef CHECK_SEEK_ON_TRANSITION
	OP_ASSERT(m_last_op == FLOATING);
#endif
	PosixLowLevelFile *copy = OP_NEW(PosixLowLevelFile, ());
	if (copy &&
		OpStatus::IsError(copy->Init(m_native_filename.CStr(), m_uni_filename.CStr())))
	{
		OP_DELETE(copy);
		return 0;
	}

	return copy;
}

OP_STATUS PosixLowLevelFile::MakeDirectory()
{
#ifdef CHECK_SEEK_ON_TRANSITION
	OP_ASSERT(m_last_op == FLOATING);
#endif
	OP_ASSERT(!m_fp);
	if (m_fp)
		return OpStatus::ERR;

	// Handle LocalTempFile's progeny:
	if (m_fno >= 0)
	{
		Close();
		Delete(); // We opened it as a file, but now know we want a directory instead.
	}

	return PosixFileUtil::CreatePath(m_native_filename.CStr(), false);
}

OP_STATUS PosixLowLevelFile::LocalTempFile(const uni_char* prefix,
										   PosixLowLevelFile** file) const
{
	OP_ASSERT(prefix && file);
	OpString8 path;
	/* First build up the template mkstemp needs: */
	int end = m_native_filename.FindLastOf(PATHSEPCHAR);
	if (end != KNotFound)
	{
		RETURN_IF_ERROR(EnsureDir()); // Unable to create directory.
		RETURN_IF_ERROR(path.Set(m_native_filename.CStr(), end + 1));
	}
	{
		// Append requested prefix:
		PosixNativeUtil::NativeString native(prefix);
		RETURN_IF_ERROR(native.Ready());
		RETURN_IF_ERROR(path.Append(native.get()));
	}
	// Last six characters of template *must* be six instances of 'X'
	RETURN_IF_ERROR(path.Append("XXXXXX"));

	/* Next, create a new low level file object and initialize it using mkstemp: */
	PosixLowLevelFile *mine = OP_NEW(PosixLowLevelFile, ());
	if (mine == 0)
		return OpStatus::ERR_NO_MEMORY;

	/* Force strict permissions for tempfile.  In glibc after version 2.0.7,
	 * this is what mkstemp() does anyway; the aim of that is to ensure no other
	 * process can mess with the file we're about to use.  This sounds like a
	 * prudent way to handle it, so I enforce the same policy in any case (this
	 * should incidentally calm Coverity Prevent if it ever sees this code).
	 */
	const PosixUMaskGuard ignoreme(true); // ensure umask is suitable.
	mine->m_fno = mkstemp(path.CStr()); // modifies path.CStr() on success
	mine->m_locked = true;
	const OP_STATUS err = mine->m_fno < 0
		? OpStatus::ERR_FILE_NOT_FOUND
		: mine->Init(path.CStr());
	if (OpStatus::IsSuccess(err))
		*file = mine;
	else
		OP_DELETE(mine);

	return err;
}

OP_STATUS PosixLowLevelFile::CopyContents(const OpLowLevelFile* source)
{
	if (m_native_filename.IsEmpty())
		return OpStatus::ERR;

	// NB: we must not assume *source is a PosixLowLevelFile; see CORE-22197 ...
	if (m_fp || source->IsOpen())
	{
		REALITY_CHECK(!"Attempted to copy an open file - not allowed.");
		return OpStatus::ERR;
	}
	// ... but that means we have to use non-const methods of it !
	OpLowLevelFile* src = const_cast<OpLowLevelFile*>(source);

	OP_STATUS result = src->Open(OPFILE_READ); // See DSK-318433 for why we don't OPFILE_SHAREDENYWRITE.
	if (OpStatus::IsSuccess(result))
	{
		const PosixUMaskGuard ignoreme; // ensure umask is suitable.
		char mode[] = "w";
		errno = 0;
		FILE *dest = m_fno < 0
			? fopen(m_native_filename.CStr(), mode)
			: fdopen(m_fno, mode);
		if (dest == 0 && !(errno == ENOMEM || errno == ENOBUFS) &&
			OpStatus::IsSuccess(EnsureDir()))
		{
			errno = 0; // Try again:
			dest = fopen(m_native_filename.CStr(), mode);
		}
		if (dest)
		{
			const int bufsize = 0x8000; // 32 kB
			char buf[bufsize];			// ARRAY OK 2010-08-12 markuso
			OpFileLength read;

			while (OpStatus::IsSuccess(result = src->Read(buf, bufsize, &read)) &&
				   (read > 0 || !source->Eof()))
				if (read > 0 && (OpFileLength)fwrite(buf, 1, read, dest) != read)
				{
					result = ErrNoToStatus(errno);
					break;
				}

			if (OpStatus::IsSuccess(result) && ferror(dest))
				result = OpStatus::ERR;

			errno = 0;
			if (fclose(dest) && OpStatus::IsSuccess(result))
				result = ErrNoToStatus(errno);
			m_fno = -1;
		}
		else
			result = ErrNoToStatus(errno);

		OP_STATUS err = src->Close();
		if (OpStatus::IsError(err) && OpStatus::IsSuccess(result))
			result = err;
	}

	if (OpStatus::IsError(result))
		unlink(m_native_filename.CStr());
	else
	{
		/* Propagate source's permissions (ignoring failure, for now), in so far
		 * as we can - unfortunately, not possible if source is a pseudo-file
		 * inside a virtual mount-point, such as a OpZipFile: */
		mode_t mode = 0777;
		struct stat s_buf;

		PosixLowLevelFile probe;
		if (OpStatus::IsSuccess(probe.Init(source->GetFullPath(), false)) &&
			OpStatus::IsSuccess(probe.RawStat(&s_buf))) // fails if file doesn't exist
			// source probably really *is* a PosixLowLevelFile, after all.
			mode &= s_buf.st_mode;

		else if (stat(m_native_filename.CStr(), &s_buf) == 0)
		{
			// All we can hope for is to preserve write bits:
			mode &= s_buf.st_mode;
			OpFileInfo info;
			info.flags = OpFileInfo::WRITABLE;
			if (OpStatus::IsSuccess(src->GetFileInfo(&info)))
			{
				if (info.writable) mode |= 0200;
				else mode &= ~0222;
			}
		}

		chmod(m_native_filename.CStr(), mode);
	}
	return result;
}

// TODO: review possible interaction with async, on either this or source
OP_STATUS PosixLowLevelFile::SafeClose()
{
	if (m_fp == 0 && m_fno < 0)
		return OpStatus::OK; // Fatuous operation - nominally succeeds.

	errno = 0;
	int res = m_fp ? fflush(m_fp) : 0;
	if (m_commit && res == 0 && m_fno >= 0)
		res = PosixFDataSync(m_fno);

	if (!res)
		return Close();

	int err = errno; // Save before calling Close()
	return ErrNoToStatus(err, Close());
}

// TODO: review possible interaction with async, on either this or source
OP_STATUS PosixLowLevelFile::SafeReplace(OpLowLevelFile* source)
{
	if (m_fp || m_native_filename.IsEmpty())
		return OpStatus::ERR;

	struct stat s_buf;
	int stat_err = stat(m_native_filename.CStr(), &s_buf);
	if (stat_err == 0 && (s_buf.st_mode & 0222) == 0)
		/* Destination file (this) exists and is write-protected; don't try to
		 * over-write, even though we might be capable of it (see pi spec). */
		return OpStatus::ERR_NO_ACCESS;

	if (op_strcmp(source->Type(), "PosixLowLevelFile") == 0)
	{
		PosixLowLevelFile *other = static_cast<PosixLowLevelFile *>(source);
		/* If files are on the same device, we can simply rename them.  Rather than
		 * trying to work out whether they are any other way, simply see whether
		 * rename() succeeds: */
		if (rename(other->m_native_filename.CStr(), m_native_filename.CStr()) == 0)
			return OpStatus::OK;
	}

	/* Otherwise, we have to do it the slow way: first copy the source file to a
	 * temporary file in the same directory as the destination file, to ensure
	 * that they are located in the same file system.  Then call rename(). */

	PosixLowLevelFile *tmpfile;
	RETURN_IF_ERROR(LocalTempFile(UNI_L("opsrp"), &tmpfile));
	const PosixUMaskGuard ignoreme; // ensure umask is suitable.

	OP_STATUS res = tmpfile->CopyContents(source);
	if (OpStatus::IsSuccess(res))
	{
		errno = 0;
		if (rename(tmpfile->m_native_filename.CStr(), m_native_filename.CStr()) != 0)
		{
			res = ErrNoToStatus(errno);
			tmpfile->Delete();
		}
		else if (stat_err == 0)
			// Restore prior file permissions of this (ignoring failure):
			chmod(m_native_filename.CStr(), s_buf.st_mode & 0777);
	}

	OP_DELETE(tmpfile);
	if (OpStatus::IsSuccess(res))
		source->Delete();

	return res;
}

// TODO: may need to do some lock/unlock around Delete for async+thread
OP_STATUS PosixLowLevelFile::Delete()
{
	if (m_fp || m_native_filename.IsEmpty())
		return OpStatus::ERR;

	struct stat st;
	errno = 0;
	if (0 == lstat(m_native_filename.CStr(), &st) && // existed and
		// We could simply call remove(), which handles the ISDIR ? rmdir : unlink for us.
		0 == (S_ISDIR(st.st_mode) // is a directory
			  ? rmdir(m_native_filename.CStr()) // so we need rmdir
			  : unlink(m_native_filename.CStr()))) // else unlink should suffice
		return OpStatus::OK;

	return ErrNoToStatus(errno);
}

OP_STATUS PosixLowLevelFile::Flush()
{
	if (!m_fp)
		return OpStatus::ERR;

#ifdef CHECK_SEEK_ON_TRANSITION
	if (m_last_op == READING)
		m_last_op = FLOATING;
#endif

	LockIO();
	errno = 0;
	OP_STATUS st = m_fp == 0 ? OpStatus::ERR
		: fflush(m_fp) ? ErrNoToStatus(errno) : OpStatus::OK;

	if (m_commit && m_fp)
	{
		errno = 0;
		if (PosixFDataSync(m_fno))
			st = ErrNoToStatus(errno, OpStatus::IsSuccess(st) ? OpStatus::ERR : st);
	}

	UnlockIO();
	return st;
}

// TODO: should Flush/SetFileLength count as write for "seek between read and write" rules ?

OP_STATUS PosixLowLevelFile::SetFileLength(OpFileLength len)
{
	OPFILE_LENGTH_CHECK(len);
	if (m_fp == 0 || !IsWritable())
		return OpStatus::ERR;

#if 0 //def CHECK_SEEK_ON_TRANSITION
	OP_ASSERT(m_last_op != READING);
	m_last_op = WRITING;
#endif
	LockIO();
	int err = errno = 0;
	int res = fflush(m_fp);
	if (res)
		err = errno;
	else
	{
		// TODO: broken if len > max off_t
		res = ftruncate(m_fno, len);
		if (res)
		{
			err = errno; // remember original error before we try fall-back.
			// ftruncate isn't guaranteed to grow a file.
			struct stat st;
			if (OpStatus::IsSuccess(RawStat(&st)) && OpFileLength(st.st_size) < len &&
				1 + (res = lseek(m_fno, len-1, SEEK_SET)))
				/* Calling lseek doesn't, itself, change file size: but a
				 * subsequent write (of one '\0' byte, provided by our "",
				 * starting at position len-1) should:
				 */
				res = write(m_fno, "", 1);

			if (res)
				switch (err)
				{
				case ENOMEM: case ENOBUFS: break;
				default: err = errno; break;
				}
		}
	}
	UnlockIO();

	if (res == 0)
	{
		clearerr(m_fp);
		return OpStatus::OK;
	}

	return ErrNoToStatus(err);
}

#ifdef PI_ASYNC_FILE_OP
void PosixLowLevelFile::ReportQueue::Flush()
{
	while (DiskOpReport * tail = Last())
		if (tail->Ready())
		{
			tail->Out();
			tail->Report();
			OP_DELETE(tail);
		}
		else
			break;
}

BOOL PosixLowLevelFile::IsAsyncInProgress()
{
	if (m_async_reports.Empty())
	{
		OP_ASSERT(g_posix_async->Find(this, FALSE) == 0);
		return FALSE;
	}
	OP_ASSERT(g_posix_async->Find(this, FALSE) != 0);
	return TRUE;
}

OP_STATUS PosixLowLevelFile::Sync()
{
	if (g_posix_async && g_posix_async->IsActive())
		while (PosixAsyncManager::PendingFileOp *item = g_posix_async->Find(this, TRUE))
		{
			item->Run();
			OP_DELETE(item);
		}
	m_async_reports.Flush();
	OP_ASSERT(m_async_reports.Empty());
	return OpStatus::OK;
}

OP_STATUS PosixLowLevelFile::ReadAsync(void* data, OpFileLength len,
									   OpFileLength abs_pos /* = FILE_LENGTH_NONE */)
{
	OPFILE_LENGTH_CHECK(len);
	// TODO: use pread() to avoid the need to seek ? (via PendingIO, DoChunk)
	if (ForbidAsync())
		return OpStatus::ERR;

#ifdef CHECK_SEEK_ON_TRANSITION
	OP_ASSERT(m_last_op != WRITING);
	m_last_op = READING;
#endif

	class ReadReport : public IOReport
	{
	protected:
		virtual void Report(OpLowLevelFileListener* listener,
							OpLowLevelFile *file, OP_STATUS result, OpFileLength size)
			{ listener->OnDataRead(file, result, size); }
	public:
		ReadReport(OpLowLevelFileListener* listener, OpLowLevelFile *file, Head *head)
			: IOReport(listener, file, head) {}
	} * answer = OP_NEW(ReadReport, (m_ear, this, &m_async_reports));

	class PendingRead : public PendingIO
	{
		void* const m_data;
		OpFileLength const m_len;
	public:
		PendingRead(PosixLowLevelFile *file,
					void* data, OpFileLength len,
					OpFileLength abs_pos, ReadReport *reporter)
			: PendingIO(file, abs_pos, reporter), m_data(data), m_len(len) { Enqueue(); }

		virtual OP_STATUS DoChunk(OpFileLength *size)
			{ return Boss()->Read(m_data, m_len, size); }
	};

	if (0 == answer || 0 == OP_NEW(PendingRead, (this, data, len, abs_pos, answer)))
	{
		OP_DELETE(static_cast<IOReport *>(answer));
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

/* TODO: amend so that, absent THREAD_SUPPORT, we can take some upper bound on
 * chunk size and write/read in a succession of chunks, going via the message
 * loop between chunks, so that writing or reading a gigantic slab of file never
 * causes Opera to hog the process; c.f. bug 324004.
 */

OP_STATUS PosixLowLevelFile::WriteAsync(const void* data, OpFileLength len,
										OpFileLength abs_pos /* = FILE_LENGTH_NONE */)
{
	OPFILE_LENGTH_CHECK(len);
	// TODO: use pwrite() to avoid the need to seek ? (via PendingIO, DoChunk)
	if (ForbidAsync())
		return OpStatus::ERR;

#ifdef CHECK_SEEK_ON_TRANSITION
	OP_ASSERT(m_last_op != READING);
	m_last_op = WRITING;
#endif

	class WriteReport : public IOReport
	{
	protected:
		virtual void Report(OpLowLevelFileListener* listener, OpLowLevelFile *file,
							OP_STATUS result, OpFileLength size)
		{ listener->OnDataWritten(file, result, size); }
	public:
		WriteReport(OpLowLevelFileListener* listener, OpLowLevelFile *file, Head *head)
			: IOReport(listener, file, head) {}
	} * answer = OP_NEW(WriteReport, (m_ear, this, &m_async_reports));

	class PendingWrite : public PendingIO
	{
		const void* const m_data;
		OpFileLength const m_len;
	public:
		PendingWrite(PosixLowLevelFile *file,
					 const void* data, OpFileLength len, OpFileLength abs_pos,
					 WriteReport *reporter)
			: PendingIO(file, abs_pos, reporter), m_data(data), m_len(len) { Enqueue(); }

		virtual OP_STATUS DoChunk(OpFileLength *size)
			{ *size = m_len; return Boss()->Write(m_data, m_len); }
	};

	if (0 == answer || 0 == OP_NEW(PendingWrite, (this, data, len, abs_pos, answer)))
	{
		OP_DELETE(static_cast<IOReport *>(answer));
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OP_STATUS PosixLowLevelFile::DeleteAsync()
{
	if (ForbidAsync())
		return OpStatus::ERR;

#ifdef CHECK_SEEK_ON_TRANSITION
	OP_ASSERT(m_last_op == FLOATING);
#endif

	class DeleteReport : public DiskOpReport
	{
	protected:
		virtual void Report(OpLowLevelFileListener* listener,
							OpLowLevelFile *file, OP_STATUS result)
			{ listener->OnDeleted(file, result); }
	public:
		DeleteReport(OpLowLevelFileListener* listener, OpLowLevelFile *file, Head *queue)
			: DiskOpReport(listener, file, queue) {}
	} * answer = OP_NEW(DeleteReport, (m_ear, this, &m_async_reports));

	class PendingDelete : public PendingDiskOp
	{
	public:
		PendingDelete(PosixLowLevelFile *file, DeleteReport *reporter)
			: PendingDiskOp(file, reporter) { Enqueue(); }
		virtual void RunDiskOp() { Reporter()->Record(Boss()->Delete()); }
	};

	if (0 == answer || 0 == OP_NEW(PendingDelete, (this, answer)))
	{
		OP_DELETE(static_cast<DiskOpReport *>(answer));
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OP_STATUS PosixLowLevelFile::FlushAsync()
{
	if (ForbidAsync())
		return OpStatus::ERR;

#ifdef CHECK_SEEK_ON_TRANSITION
	OP_ASSERT(m_last_op != READING);
	m_last_op = WRITING;
#endif

	class FlushReport : public DiskOpReport
	{
	protected:
		virtual void Report(OpLowLevelFileListener* listener,
							OpLowLevelFile *file, OP_STATUS result)
			{ listener->OnFlushed(file, result); }
	public:
		FlushReport(OpLowLevelFileListener* listener, OpLowLevelFile *file, Head *queue)
			: DiskOpReport(listener, file, queue) {}
	} * answer = OP_NEW(FlushReport, (m_ear, this, &m_async_reports));

	class PendingFlush : public PendingDiskOp
	{
	public:
		PendingFlush(PosixLowLevelFile *file, FlushReport *reporter)
			: PendingDiskOp(file, reporter) { Enqueue(); }
		virtual void RunDiskOp() { Reporter()->Record(Boss()->Flush()); }
	};

	if (0 == answer || 0 == OP_NEW(PendingFlush, (this, answer)))
	{
		OP_DELETE(static_cast<DiskOpReport *>(answer));
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}
#endif // PI_ASYNC_FILE_OP

#ifdef POSIX_EXTENDED_FILE
OP_STATUS PosixLowLevelFile::Move(const PosixOpLowLevelFileBase *new_file)
{
	/* In principle, we have no guarantee that *new_file is a PosixLowLevelFile
	 * - see CORE-22197; however, desktop's Move() makes no sense otherwise, so
	 * assume it unless they complain ... */
	const PosixLowLevelFile* const posix
		= reinterpret_cast<const PosixLowLevelFile*>(new_file);
	if (m_native_filename.IsEmpty() || !posix || posix->m_native_filename.IsEmpty())
		return OpStatus::ERR_NULL_POINTER;

	errno = 0;
	if (rename(m_native_filename.CStr(), posix->m_native_filename.CStr()) == 0)
		return OpStatus::OK;

	return ErrNoToStatus(errno);
}

OP_STATUS PosixLowLevelFile::Stat(struct stat *buffer)
{
	if (!buffer)
		return OpStatus::ERR_NULL_POINTER;

	return RawStat(buffer);
}

OP_STATUS PosixLowLevelFile::ReadLine(char *buffer, int max_length)
{
	if (!buffer || !m_fp || !buffer)
		return OpStatus::ERR_NULL_POINTER;

	OP_ASSERT(m_mode & POSIX_FILE_READ_MASK);
#ifdef CHECK_SEEK_ON_TRANSITION
	OP_ASSERT(m_last_op != WRITING);
	m_last_op = READING;
#endif

	if (fgets(buffer, max_length, m_fp) == NULL && (!feof(m_fp) || ferror(m_fp)))
	{
		clearerr(m_fp);
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}
#endif // POSIX_EXTENDED_FILE
#endif // POSIX_OK_FILE
