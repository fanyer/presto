/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWSOPLOWLEVELFILE_H
#define WINDOWSOPLOWLEVELFILE_H

#include "adjunct/desktop_pi/file/DesktopOpLowLevelFile.h"

/*
 *  Class WindowsOpLowLevelFile
 *
 *  Behaviour change (DSK-324395) 15.03.2011:
 *  The implementation of OpLowLevelFile should not need core. In some cases it will
 *  use core (serialization, async file writing/reading),but asserts are set up to trigger
 *  if anyone does the mistake of using those features before core is initialized.
 *
 *  OpFile on the other hand, will always require core to be initialized.
 */

class WindowsOpLowLevelFile: public DesktopOpLowLevelFile
{
public:
	WindowsOpLowLevelFile();
	virtual ~WindowsOpLowLevelFile();

	OP_STATUS Construct(const uni_char* path);

	OP_STATUS GetFileInfo(OpFileInfo* info);

	const uni_char* GetFullPath() const;
	const uni_char* GetSerializedName() const;

	BOOL IsWritable() const;
	BOOL IsHidden() const;

	OP_STATUS Open(int mode);
	BOOL IsOpen() const { return (m_fp != NULL); }
	OP_STATUS Close();

	OP_STATUS MakeDirectory();

	BOOL Eof() const;
	OP_STATUS Exists(BOOL* exists) const;

	OP_STATUS GetFilePos(OpFileLength* pos) const;
	OP_STATUS SetFilePos(OpFileLength pos, OpSeekMode seek_mode);
	OP_STATUS GetFileLength(OpFileLength* len) const;
	OP_STATUS SetFileLength(OpFileLength len);

	OP_STATUS Write(const void* data, OpFileLength len);
	OP_STATUS Read(void* data, OpFileLength len, OpFileLength* bytes_read=NULL);
	OP_STATUS ReadLine(char** data);

#ifdef PI_CAP_FILE_PRINTF
//	void Print(const char *fmt,va_list marker);
#endif

	OpLowLevelFile* CreateCopy();
	OpLowLevelFile* CreateTempFile(const uni_char* prefix);
	OP_STATUS CopyContents(const OpLowLevelFile* copy);

	OP_STATUS SafeClose();
	OP_STATUS SafeReplace(OpLowLevelFile* new_file);
	OP_STATUS Delete();

	OP_STATUS GetLocalizedPath(OpString *localized_path) const { return OpStatus::ERR; };

	OP_STATUS Flush();

#ifdef SUPPORT_OPFILEINFO_CHANGE
	virtual OP_STATUS ChangeFileInfo(const OpFileInfoChange* changes);
#endif // SUPPORT_OPFILEINFO_CHANGE

	// DesktopOpLowLevelFile
	OP_STATUS Move(const class DesktopOpLowLevelFile *new_file);
	OP_STATUS Stat(struct _stat *buffer);
	OP_STATUS ReadLine(char *string, int max_length);

#ifdef PI_ASYNC_FILE_OP
	OP_STATUS ReadAsync(OpLowLevelFileListener* listener, void* data, OpFileLength len, OpFileLength abs_pos = FILE_LENGTH_NONE);
	OP_STATUS WriteAsync(OpLowLevelFileListener* listener, const void* data, OpFileLength len, OpFileLength pos = FILE_LENGTH_NONE);
	OP_STATUS DeleteAsync(OpLowLevelFileListener* listener);
	OP_STATUS FlushAsync(OpLowLevelFileListener* listener);
	BOOL IsAsyncInProgress();
	OP_STATUS Sync();

public:

	class FileThreadData
	{
	public:
		enum Operation
		{
			THREAD_OP_NONE,
			THREAD_OP_READ,
			THREAD_OP_WRITE,
			THREAD_OP_FLUSH,
			THREAD_OP_SYNC,
			THREAD_OP_DELETE
		};
		FileThreadData()
		{
			m_file = NULL;
			m_buffer = NULL;
			m_data_len = 0;
			m_seek_pos = FILE_LENGTH_NONE;
			m_listener = NULL;
			m_operation = THREAD_OP_NONE;
		}
		WindowsOpLowLevelFile *m_file;
		const void *m_buffer;
		OpFileLength m_data_len;
		OpFileLength m_seek_pos;
		OpLowLevelFileListener* m_listener;
		Operation m_operation;
	};
	class FileNotification
	{
	public:
		FileNotification(FileThreadData::Operation operation, WindowsOpLowLevelFile *file, OpLowLevelFileListener *listener, OP_STATUS status, OpFileLength data_len)
		{
			m_operation = operation;
			m_file = file;
			m_data_len = data_len;
			m_listener = listener;
			m_status = status;
		}
		WindowsOpLowLevelFile *m_file;
		OpFileLength m_data_len;
		OpLowLevelFileListener* m_listener;
		OP_STATUS m_status;
		FileThreadData::Operation m_operation;
	};
	OpVector<FileThreadData> m_thread_data;	// all the data waiting to be handled
	CRITICAL_SECTION	m_thread_cs;	// critical section to protect the access to shared data

#endif //PI_ASYNC_FILE_OP

	HANDLE m_event_quit;	// event set when the thread should shut down
	HANDLE m_event_quit_done;	// event set when the thread should shut down
	HANDLE m_event_file;	// event set when data is ready to be read/written/deleted
	HANDLE m_event_sync_done;	// event set when data has been written synchronously

private:

	OP_STATUS MakeSerializedName() const;

	OP_STATUS GetWindowsLastError() const;
	static OP_STATUS ErrNoToStatus(int err);

#ifdef PI_ASYNC_FILE_OP
	OP_STATUS InitThread();
	static unsigned __stdcall FileThreadFunc( void* pArguments );
#endif //PI_ASYNC_FILE_OP

	void ShutdownThread();

#ifdef WIN_USE_STDLIB_64BIT
	FILE *m_fp;
#else
	HANDLE m_fp;
#endif // WIN_USE_STDLIB_64BIT
	OpString m_path;
	mutable OpString m_serialized_path;
	int m_file_open_mode;
	BOOL m_commit_mode;
	BOOL m_text_mode;
	BOOL m_filepos_dirty; ///< Used to flag that the file position is dirty when the file is in append mode, but the file position has been moved by a call to SetFilePos(). Must be handled by write operations to ensure that appends really are appended.
	HANDLE m_thread_handle;	// handle to the thread
};

#endif // !WINDOWSOPLOWLEVELFILE_H
