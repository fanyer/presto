/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/sqlite/systemconfig.h"

#ifdef SQLITE_OPERA_PI

#include "modules/sqlite/sqlite3.h"

// Character encoding convertion
#include "modules/encodings/encoders/outputconverter.h"
#include "modules/encodings/decoders/inputconverter.h"

// Opera PI
#include "modules/util/opfile/opfile.h"
#include "modules/util/opfile/opmemfile.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/system/OpLowLevelFile.h"

// Utils
#include "modules/util/opfile/opfolder.h"
#include "modules/util/gen_math.h"

// Debug
#ifdef _DEBUG
# include "modules/debug/src/debug_internal.h"
#endif // _DEBUG

/*
 ** The default size of a disk sector
 */
#ifndef SQLITE_DEFAULT_SECTOR_SIZE
# define SQLITE_DEFAULT_SECTOR_SIZE 512
#endif

#ifndef SQLITE_API
# define SQLITE_API
#endif

#define OP_OS_MAX_CONVERT_BUFFER 2048

#define OP_STATUS_TO_SQLITE(r) OpStatus::IsSuccess(r)? SQLITE_OK : (OpStatus::IsMemoryError(r)? SQLITE_IOERR_NOMEM : SQLITE_IOERR)
#define RETURN_SQLITE_IF_OP_ERROR(r) if (OpStatus::IsError(r)) { return OP_STATUS_TO_SQLITE(r); }
#define RETURN_SQLITE_IF_SQLITE_ERR(r) if (r != SQLITE_OK) { return r; }

/**
 * @param in Input buffer.
 * @param in_n Input buffer length (number of uni_char's).
 * @param out Pointer to a character array. Newly allocated if returning OK.
 * @param out_n Pointer to a integer. Set to number of char in output.
 */
OP_STATUS uni_char_to_utf8(const uni_char *in, char **out)
{
    OpStringC16 ops(in);

    OP_STATUS r = ops.UTF8Alloc(out);

    return OP_STATUS_TO_SQLITE(r);
}

/**
 * @param in Input buffer.
 * @param in_n Input buffer length (number of uni_char's).
 * @param out Pointer to a character array. Allocated if returning OK.
 * @param out_n Pointer to a integer. Set to number of char in output.
 */
OP_STATUS utf8_to_uni_char(const char *in, uni_char **out)
{
    OpString16 ops;

    OP_STATUS r = ops.SetFromUTF8(in);
	RETURN_SQLITE_IF_OP_ERROR(r);

    int out_n = ops.Length();
    (*out) = OP_NEWA(uni_char, out_n + 1);

    if (!(*out))
        return OpStatus::ERR_NO_MEMORY;

    uni_strcpy(*out, ops.CStr());

    return OpStatus::OK;
}

/*
 ** The opLiteFile structure is subclass of sqlite3_file specific for the opLite32
 ** portability layer.
 */
typedef struct OpLiteFile OpLiteFile;
struct OpLiteFile
{
    const sqlite3_io_methods *pMethod;  /* Always the first entry */
    OpLowLevelFile *opLLFile;           /* Handle for accessing the file */
    OpMemFile *opMFile;                 /* Handle for accessing a file in the memory */
    unsigned char locktype;             /* Type of lock currently held on this file */
    int deleteOnClose;                  /* If non-zero, delete the file after closing. */
};

inline BOOL isOpLLFile(OpLiteFile *op)
{
    return op->opLLFile != NULL;
}

inline OP_STATUS opGetFileLength(OpLiteFile *op, OpFileLength *len)
{
    return isOpLLFile(op)? op->opLLFile->GetFileLength(len) : op->opMFile->GetFileLength(*len);
}

inline OP_STATUS opSetFilePos(OpLiteFile *op, OpFileLength pos, OpSeekMode seek_mode)
{
    return isOpLLFile(op)? op->opLLFile->SetFilePos(pos, seek_mode) : op->opMFile->SetFilePos(pos, seek_mode);
}

inline OP_STATUS opGetFilePos(OpLiteFile *op, OpFileLength *pos)
{
    return isOpLLFile(op)? op->opLLFile->GetFilePos(pos) : op->opMFile->GetFilePos(*pos);
}

inline OP_STATUS opWrite(OpLiteFile *op, const void *buf, OpFileLength amt)
{
    return isOpLLFile(op)? op->opLLFile->Write(buf, amt) : op->opMFile->Write(buf, amt);
}

inline OP_STATUS opRead(OpLiteFile *op, void *buf, OpFileLength amt, OpFileLength *n_read)
{
    return isOpLLFile(op)? op->opLLFile->Read(buf, amt, n_read) : op->opMFile->Read(buf, amt, n_read);
}

inline BOOL opEof(OpLiteFile *op)
{
    return isOpLLFile(op)? op->opLLFile->Eof() : op->opMFile->Eof();
}

static int opLiteTruncate(sqlite3_file *id, sqlite_int64 nByte);

/*
 ** Close a file.
 */
static int opLiteClose(sqlite3_file *id)
{
    OpLiteFile *op = reinterpret_cast<OpLiteFile*>(id);
    if (!op->opLLFile && !op->opMFile)
        return SQLITE_OK;

    if (isOpLLFile(op))
    {
        OP_STATUS r = OpStatus::OK;

        // Close but ignore if it fails or not
        op->opLLFile->Close();

        if (op->deleteOnClose)
        {
            r = op->opLLFile->Delete();
        }
        OP_DELETE(op->opLLFile);

        op->opLLFile = NULL;

        return OP_STATUS_TO_SQLITE(r);
    }
    else
    {
        op->opMFile->Close();
        OP_DELETE(op->opMFile);
        op->opMFile = NULL;
        return SQLITE_OK;
    }
}

/*
 ** Read data from a file into a buffer.  Return SQLITE_OK if all
 ** bytes were read successfully, SQLITE_IOERR_SHORT_READ if to few
 ** bytes were read, and SQLITE_IOERR if anything goes
 ** wrong.
 **
 ** param id File to read from
 ** param pBuf Write content into this buffer
 ** param amt Number of bytes to read
 ** param offset Begin reading at this offset
 */
static int opLiteRead(sqlite3_file *id, void *pBuf, int amt, sqlite3_int64 offset)
{
    OpLiteFile *op = reinterpret_cast<OpLiteFile*>(id);
    OpFileLength op_offset = static_cast<OpFileLength>(offset);
    if (!op->opLLFile && !op->opMFile)
        return SQLITE_IOERR;

    OP_STATUS r;

    OpFileLength cur_len;
    r = opGetFileLength(op, &cur_len);
    RETURN_SQLITE_IF_OP_ERROR(r);

    // If file size is smaller than offset, return immediately
    // with no bytes read
    if (cur_len < op_offset)
    {
        OP_ASSERT(amt >= 0);
        op_memset(pBuf, 0, static_cast<size_t>(amt));
        return SQLITE_IOERR_SHORT_READ;
    }

    // Set the file possition to offset
    r = opSetFilePos(op, op_offset, SEEK_FROM_START);
    RETURN_SQLITE_IF_OP_ERROR(r);

    BYTE *byte_buf = static_cast<BYTE*>(pBuf);
    OpFileLength n_to_read = static_cast<OpFileLength>(amt);
    OpFileLength n_read = 0;
    OpFileLength n_read_total = 0;

    while (true)
    {
        r = opRead(op, byte_buf + n_read_total, n_to_read, &n_read);

        if (OpStatus::IsSuccess(r))
        {
            if (n_read < n_to_read && !opEof(op))
            {
                n_to_read -= n_read;
                n_read_total += n_read;
                continue;
            }
            else if (n_read == n_to_read)
            {
                return SQLITE_OK;
            }
            else
            {
                // Zero-set the rest of the array that was not read
                op_memset(static_cast<void*>(&(static_cast<BYTE*>(pBuf)[n_read_total])), 0, static_cast<size_t>(n_to_read - n_read_total));
                return SQLITE_IOERR_SHORT_READ;
            }
        }
        else
            break;
    }

    return SQLITE_IOERR_READ;
}

/*
 ** Write data from a buffer into a file.  Return SQLITE_OK on success
 ** or some other error code on failure.
 **
 ** param id File to write into
 ** param pBuf The bytes to be written
 ** param amt Number of bytes to write
 ** param offset Offset into the file to begin writing at
 */
static int opLiteWrite(sqlite3_file *id,const void *pBuf, int amt, sqlite3_int64 offset)
{
    OpLiteFile *op = reinterpret_cast<OpLiteFile*>(id);
    OP_STATUS r;

    OpFileLength op_len = static_cast<OpFileLength>(offset);

    // Check if we need to set a new file length
    OpFileLength cur_len;
    r = opGetFileLength(op, &cur_len);
    RETURN_SQLITE_IF_OP_ERROR(r);

    if (op_len > cur_len)
    {
        int sqlite_r = opLiteTruncate(id, (sqlite_int64)op_len);
        RETURN_SQLITE_IF_SQLITE_ERR(sqlite_r);
    }

    r = opSetFilePos(op, op_len, SEEK_FROM_START);
    RETURN_SQLITE_IF_OP_ERROR(r);

    // Write data
    r = opWrite(op, pBuf, static_cast<OpFileLength>(amt));

    if (OpStatus::IsSuccess(r))
        return SQLITE_OK;
    else if (r == OpStatus::ERR_NO_DISK ||
        r == OpStatus::ERR_NO_MEMORY ||
        r == OpStatus::ERR_NO_ACCESS)
    {
        return SQLITE_IOERR_WRITE;
    }

    return SQLITE_IOERR;
}

/*
 ** Truncate an open file to a specified size
 */
static int opLiteTruncate(sqlite3_file *id, sqlite_int64 nByte)
{
    OpLiteFile *op = reinterpret_cast<OpLiteFile*>(id);
    OP_STATUS r;

    OpFileLength op_len = static_cast<OpFileLength>(nByte);

    // Check if we need to set a new file length
    OpFileLength cur_len;
    r = opGetFileLength(op, &cur_len);
    RETURN_SQLITE_IF_OP_ERROR(r);


    if (cur_len < op_len)
    {
        if (isOpLLFile(op))
        {
            OP_STATUS r = op->opLLFile->SetFileLength(op_len);

            if (r == OpStatus::OK)
                return SQLITE_OK;
            else if (OpStatus::IsMemoryError(r))
                return SQLITE_IOERR_NOMEM;
            else if (r == OpStatus::ERR_NO_DISK)
                return SQLITE_IOERR_TRUNCATE;
            else
                return SQLITE_IOERR;
        }
        else
        {
            OP_ASSERT(op_len >= cur_len);

            // OpMemFile doesn't support truncating, so what we have to do
            // is to write enough zeros so the file grows
            OpFileLength len_diff = op_len - cur_len;

            BYTE *tmp_buf = OP_NEWA(BYTE, static_cast<unsigned int>(len_diff));
            if (!tmp_buf)
                return SQLITE_IOERR_NOMEM;
            op_memset(tmp_buf, 0, static_cast<size_t>(len_diff));
            OP_STATUS r = op->opMFile->Write(static_cast<void*>(tmp_buf), len_diff);
            OP_DELETEA(tmp_buf);
            RETURN_SQLITE_IF_OP_ERROR(r);
        }
    }
    return SQLITE_OK;
}

#ifdef SQLITE_TEST
/*
 ** Count the number of fullsyncs and normal syncs.  This is used to test
 ** that syncs and fullsyncs are occuring at the right times.
 */
SQLITE_API int sqlite3_sync_count = 0;
SQLITE_API int sqlite3_fullsync_count = 0;
#endif

/*
 ** Make sure all writes to a particular file are committed to disk.
 **
 ** NOTE: Opera PI does not support full sync.
 */
static int opLiteSync(sqlite3_file *id, int flags)
{
    OpLiteFile *op = reinterpret_cast<OpLiteFile*>(id);

    if (isOpLLFile(op))
    {
#ifdef SQLITE_TEST
        if( flags & SQLITE_SYNC_FULL){
            sqlite3_fullsync_count++;
        }
        sqlite3_sync_count++;
#endif
        /* If we compiled with the SQLITE_NO_SYNC flag, then syncing is a
         ** no-op
         */
#ifdef SQLITE_NO_SYNC
        UNUSED_PARAMETER(pFile);
        return SQLITE_OK;
#else

        OpFileLength pos = 0;
        OP_STATUS pos_res = op->opLLFile->GetFilePos(&pos);

        OP_STATUS r = op->opLLFile->Flush();

        if (OpStatus::IsSuccess(pos_res))
            op->opLLFile->SetFilePos(pos);

        return OP_STATUS_TO_SQLITE(r);
#endif
    }
    else
        return SQLITE_OK;
}

/*
 ** Determine the current size of a file in bytes
 */
static int opLiteFileSize(sqlite3_file *id, sqlite3_int64 *pSize)
{
    OpLiteFile *op = reinterpret_cast<OpLiteFile*>(id);
    OP_STATUS r;

    OpFileLength len;

    r = opGetFileLength(op, &len);
    if (OpStatus::IsSuccess(r))
    {
        *pSize = static_cast<sqlite3_int64>(len);
        return SQLITE_OK;
    }
    else if (r == OpStatus::ERR_NO_DISK ||
            r == OpStatus::ERR_NO_ACCESS ||
            r == OpStatus::ERR_FILE_NOT_FOUND ||
            r == OpStatus::ERR_NO_MEMORY)
        return SQLITE_IOERR_FSTAT;

    return SQLITE_IOERR;
}

/*
 ** Lock the file with the lock specified by parameter locktype - one
 ** of the following:
 **
 **     (1) SHARED_LOCK
 **     (2) RESERVED_LOCK
 **     (3) PENDING_LOCK
 **     (4) EXCLUSIVE_LOCK
 **
 ** Sometimes when requesting one lock state, additional lock states
 ** are inserted in between.  The locking might fail on one of the later
 ** transitions leaving the lock state different from what it started but
 ** still short of its goal.  The following chart shows the allowed
 ** transitions and the inserted intermediate states:
 **
 **    UNLOCKED -> SHARED
 **    SHARED -> RESERVED
 **    SHARED -> (PENDING) -> EXCLUSIVE
 **    RESERVED -> (PENDING) -> EXCLUSIVE
 **    PENDING -> EXCLUSIVE
 **
 ** This routine will only increase a lock.  The unlock routine
 ** erases all locks at once and returns us immediately to locking level 0.
 ** It is not possible to lower the locking level one step at a time.  You
 ** must go straight to locking level 0.
 */
static int opLiteLock(sqlite3_file *id, int locktype)
{
    OpLiteFile *op = reinterpret_cast<OpLiteFile*>(id);

    if (op->locktype >= locktype)
        return SQLITE_OK;

    op->locktype = locktype;

    return SQLITE_OK;
}

/*
 ** This routine checks if there is a RESERVED lock held on the specified
 ** file by this or any other process. If such a lock is held, return
 ** non-zero, otherwise zero.
 */
static int opLiteCheckReservedLock(sqlite3_file *id, int *pOut)
{
    OpLiteFile *op = reinterpret_cast<OpLiteFile*>(id);
    *pOut = op->locktype == SQLITE_LOCK_RESERVED;
    return SQLITE_OK;
}

/*
 ** Lower the locking level on file descriptor id to locktype.  locktype
 ** must be either NO_LOCK or SHARED_LOCK.
 **
 ** If the locking level of the file descriptor is already at or below
 ** the requested locking level, this routine is a no-op.
 */
static int opLiteUnlock(sqlite3_file *id, int locktype)
{
    OP_ASSERT(locktype == SQLITE_LOCK_NONE || locktype == SQLITE_LOCK_SHARED);

    OpLiteFile *op = reinterpret_cast<OpLiteFile*>(id);

    if (op->locktype <= locktype)
            return SQLITE_OK;

    op->locktype = locktype;
    return SQLITE_OK;
}

/*
 ** Control and query of the open file handle.
 */
static int opLiteFileControl(sqlite3_file *id, int op, void *pArg)
{
    switch( op )
    {
        case SQLITE_FCNTL_LOCKSTATE:
            *(int*)pArg = (reinterpret_cast<OpLiteFile*>(id))->locktype;
            return SQLITE_OK;
        case SQLITE_LAST_ERRNO:
            {
                *(static_cast<int*>(pArg)) = 0;
                return SQLITE_OK;
            }
        default:
            return SQLITE_ERROR;
    }
}

/*
 ** Return the sector size in bytes of the underlying block device for
 ** the specified file. This is almost always 512 bytes, but may be
 ** larger for some devices.
 **
 ** SQLite code assumes this function cannot fail. It also assumes that
 ** if two files are created in the same file-system directory (i.e.
 ** a database and its journal file) that the sector size will be the
 ** same for both.
 */
static int opLiteSectorSize(sqlite3_file *id)
{
    return SQLITE_DEFAULT_SECTOR_SIZE;
}

/*
 ** Return a vector of device characteristics.
 */
static int opLiteDeviceCharacteristics(sqlite3_file *id)
{
    return 0;
}

/*
 ** This vector defines all the methods that can operate on an
 ** sqlite3_file using Operas portability interfaces.
 */
static const sqlite3_io_methods opLiteIoMethod = {
        1,                        /* iVersion */
        opLiteClose,
        opLiteRead,
        opLiteWrite,
        opLiteTruncate,
        opLiteSync,
        opLiteFileSize,
        opLiteLock,
        opLiteUnlock,
        opLiteCheckReservedLock,
        opLiteFileControl,
        opLiteSectorSize,
        opLiteDeviceCharacteristics
};

/*
 ** Turn a relative pathname into a full pathname.  Write the full
 ** pathname into zFull[].  zFull[] will be at least pVfs->mxPathname
 ** bytes in size.
 **
 ** param pVfs Pointer to vfs object
 ** param zRelative Possibly relative input path
 ** param nFull Size of output buffer in bytes
 ** param zFull Output buffer
 */
static int opLiteFullPathname(sqlite3_vfs *pVfs, const char *zRelative, int nFull, char *zFull)
{
    OpLowLevelFile *f;
    OP_STATUS r;

    uni_char *uni_rel;

    r = utf8_to_uni_char(zRelative,  &uni_rel);
    RETURN_SQLITE_IF_OP_ERROR(r);

    r = OpLowLevelFile::Create(&f, uni_rel);
    OP_DELETEA(uni_rel);

    if (OpStatus::IsSuccess(r))
    {
        const uni_char *fp = f->GetFullPath();

        char *fpcs;

        r = uni_char_to_utf8(fp, &fpcs);
        if (OpStatus::IsError(r))
        {
            OP_DELETE(f);
            OP_DELETEA(fpcs);
            return SQLITE_CANTOPEN;
        }
        OP_DELETE(f);

        OP_ASSERT(nFull >= 0);

        if (op_strlen(fpcs) <= static_cast<size_t>(nFull))
        {
            sqlite3_snprintf(nFull, zFull, "%s", fpcs);
            OP_DELETEA(fpcs);
            return SQLITE_OK;
        }
        else
        {
            OP_DELETEA(fpcs);
            return SQLITE_CANTOPEN;
        }
    }
    else
        return SQLITE_IOERR;
}


/*
 ** Open a file.
 **
 */
static int opLiteOpen(sqlite3_vfs *pVfs, const char *zName, sqlite3_file *id, int flags, int *pOutFlags)
{
    int is_exclusive  = (flags & SQLITE_OPEN_EXCLUSIVE);
    int is_delete     = (flags & SQLITE_OPEN_DELETEONCLOSE);
    int is_create     = (flags & SQLITE_OPEN_CREATE);
    int is_readonly   = (flags & SQLITE_OPEN_READONLY);
    int is_readwrite  = (flags & SQLITE_OPEN_READWRITE);

    OpLiteFile *op = reinterpret_cast<OpLiteFile*>(id);
    op_memset(op, 0, sizeof(OpLiteFile));

    OpLowLevelFile *f = NULL;
    OpMemFile *mf = NULL;
    OP_STATUS r;

    if (!zName)
    {
        // Open temporary directory

        OpString tmp_dir;
        r = g_folder_manager->GetFolderPath(OPFILE_TEMP_FOLDER, tmp_dir);
        RETURN_SQLITE_IF_OP_ERROR(r);

        // Create a OpLowLevelFile that is the directory
        OpLowLevelFile *tmp_dir_f;
        r = OpLowLevelFile::Create(&tmp_dir_f, tmp_dir);
        RETURN_SQLITE_IF_OP_ERROR(r);

        f = tmp_dir_f->CreateTempFile((const uni_char*)NULL);
    }
    else
    {
        if (op_strcmp(zName, ":memory:") == 0)
        {
            mf = OpMemFile::Create(NULL, 0);
            if (!mf)
            {
                return SQLITE_IOERR;
            }
        }
        else
        {
            uni_char *uni_name;
            r = utf8_to_uni_char(zName, &uni_name);
            RETURN_SQLITE_IF_OP_ERROR(r);

            r = OpLowLevelFile::Create(&f, uni_name);
            OP_DELETEA(uni_name);
            RETURN_SQLITE_IF_OP_ERROR(r);
        }
    }

    if (f)
    {
        // Low level file
        BOOL exists;
        r = f->Exists(&exists);
        if (OpStatus::IsError(r))
        {
            OP_DELETE(f);
            return OP_STATUS_TO_SQLITE(r);
        }

        if ((exists && is_exclusive && zName) || (!exists && !is_create))
        {
            OP_DELETE(f);
            return SQLITE_IOERR;
        }

        // Mode
        int mode = OPFILE_COMMIT;
        if (is_readonly)
            mode = OPFILE_READ;
        else if (is_readwrite && exists)
            mode |= OPFILE_UPDATE;
        else if (is_readwrite && !exists)
            mode |= OPFILE_OVERWRITE;
        else
            mode |= OPFILE_WRITE;

        // Delete on close?
        op->deleteOnClose = is_delete;

        r = f->Open(mode);
        if (OpStatus::IsError(r))
        {
            // try open it read only
            r = f->Open(OPFILE_READ);
            if (OpStatus::IsError(r))
            {
                OP_DELETE(f);
                return OP_STATUS_TO_SQLITE(r);
            }

            if (pOutFlags)
                *pOutFlags = SQLITE_READONLY;
        }
        else
        {
            if (pOutFlags)
                *pOutFlags = flags;
        }
        op->opLLFile = f;
        op->pMethod = &opLiteIoMethod;
    }
    else if (mf)
    {
        // Memory file

        // mode doesn't seem to be used in implementation
        // and does always return OpStatus::OK, however lets
        // check anyway since we don't know if future versions
        // might return errors
        r = mf->Open(0);
        if (OpStatus::IsError(r))
        {
            OP_DELETE(mf);
            return OP_STATUS_TO_SQLITE(r);
        }

        if (pOutFlags)
            *pOutFlags = SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE;
        op->opMFile = mf;
        op->pMethod = &opLiteIoMethod;
    }
    return SQLITE_OK;
}

/*
 ** Delete the named file.
 */
static int opLiteDelete(sqlite3_vfs *pVfs, const char *zFilename, int syncDir)
{
    OP_ASSERT(zFilename);

    OpLowLevelFile *f;
    OP_STATUS r;

    uni_char *uni_name;

    r = utf8_to_uni_char(zFilename, &uni_name);
    RETURN_SQLITE_IF_OP_ERROR(r);

    r = OpLowLevelFile::Create(&f, uni_name);
    OP_DELETEA(uni_name);
    RETURN_SQLITE_IF_OP_ERROR(r);

    r = f->Delete();
    OP_DELETE(f);
    if (OpStatus::IsError(r))
        return SQLITE_IOERR_DELETE;

    /*
    ** Todo: Implement sync dir (fsync the directory after deleting)
    */

    return SQLITE_OK;
}

/*
 ** Check the existance and status of a file.
 **
 ** param pVfs pointer to virtual file system
 ** param zFilename The filename
 ** param flags Access flags (SQLITE_ACCESS_EXISTS, SQLITE_ACCESS_READWRITE and SQLITE_ACCESS_READ)
 ** param pOut Return 1 if given access rights
 */

static int opLiteAccess(sqlite3_vfs *pVfs, const char *zFilename, int flags, int *pOut)
{
    OP_ASSERT(zFilename);

    OpLowLevelFile *f;
    OP_STATUS r;

    int is_exists = flags == SQLITE_ACCESS_EXISTS;
    int is_readwrite = flags == SQLITE_ACCESS_READWRITE;
    int is_read = flags == SQLITE_ACCESS_READ;

    uni_char *uni_name;

    r = utf8_to_uni_char(zFilename, &uni_name);
    RETURN_SQLITE_IF_OP_ERROR(r);

    r = OpLowLevelFile::Create(&f, uni_name);
    OP_DELETEA(uni_name);
    RETURN_SQLITE_IF_OP_ERROR(r);

    int rc = -1;

    /*  - Check if exists access
     *  - Since OpLowLevelFile does not provide any way of
     *    telling if the file is readable or not, so
     *    check if readable just checks if it exists
     */
    if (is_exists || is_read)
    {
        BOOL fExists;
        r = f->Exists(&fExists);
        rc = OpStatus::IsError(r) ? -1 : fExists;
    }
    // check if readwrite
    else if (is_readwrite)
    {
        rc = f->IsWritable() == TRUE;
    }

    OP_DELETE(f);

    if (rc == -1)
        return SQLITE_IOERR;
    else
    {
        *pOut = rc;
        return SQLITE_OK;
    }
}


#ifndef SQLITE_OMIT_LOAD_EXTENSION
#error "Not implemented for Opera PI"
static void *opLiteDlOpen(sqlite3_vfs *pVfs, const char *zFilename) { }

static void opLiteDlError(sqlite3_vfs *pVfs, int nBuf, char *zBufOut) { }

static void (*opLiteDlSym(sqlite3_vfs *pVfs, void *pHandle, const char *zSymbol))(void) { }

static void opLiteDlClose(sqlite3_vfs *pVfs, void *pHandle) { }

#else /* if SQLITE_OMIT_LOAD_EXTENSION is defined: */
#define opLiteDlOpen 0
#define opLiteDlError 0
#define opLiteDlSym 0
#define opLiteDlClose 0
#endif


/*
 ** Write up to nBuf bytes of randomness into zBuf.
 */
static int opLiteRandomness(sqlite3_vfs *pVfs, int nBuf, char *zBuf )
{
    int n = 0;
#if defined(SQLITE_TEST)
    n = nBuf;
    op_memset(zBuf, 0, nBuf);
#endif

    /* Write nBuf bytes of randomness into zBuf */
    // Better random?
    for (int i=0; i<nBuf; i++)
    {
        zBuf[i] = (char)(op_rand() % 0xff);
    }
    return n;
}

/*
 ** Sleep for a little while.  Return the amount of time slept.
 ** The argument is the number of microseconds we want to sleep.
 ** The return value is the number of microseconds of sleep actually
 ** requested from the underlying operating system, a number which
 ** might be greater than or equal to the argument, but not less
 ** than the argument.
 */

static int opLiteSleep( sqlite3_vfs *pVfs, int microsec )
{
    return microsec;
}

/*
 ** The following variable, if set to a non-zero value, becomes the result
 ** returned from sqlite3OsCurrentTime().  This is used for testing.
 */
#ifdef SQLITE_TEST
SQLITE_API int sqlite3_current_time = 0;
#endif

/*
 ** Find the current time (in Universal Coordinated Time).  Write the
 ** current time and date as a Julian Day number into *prNow and
 ** return SQLITE_OK.  Return SQLITE_ERROR if the time and date cannot be found.
 */
int opLiteCurrentTime( sqlite3_vfs *pVfs, double *prNow )
{
    OpSystemInfo *opSysInfo;
    OP_STATUS r;

    r = OpSystemInfo::Create(&opSysInfo);
    RETURN_SQLITE_IF_OP_ERROR(r);

    double t = opSysInfo->GetTimeUTC();
    OP_DELETE(opSysInfo);

    int a = OpRound(t);

    // milliseconds
    int ms = a % 1000;
    a = (a - ms) / 1000;

    // seconds
    int s = a % 60;
    a = (a - s) / 60;

    // minutes
    int m = a % 60;
    a = (a - m) / 60;

    // hours
    int h = a % 24;
    a = (a - h) / 24;

    /*
     * todays fraction = number of milli seconds passed the current day / amount of milliseconds during one day
     */
    double todaysFraction = (static_cast<double>(ms) + (static_cast<double>(s) * 1000) 
        + (static_cast<double>(m) * (60 * 1000)) + (static_cast<double>(h) * (60 * 60 * 1000))) / 86400000;

    // Set output parameter
    *prNow = 2440587.5 + static_cast<double>(a) + todaysFraction;

    return SQLITE_OK;
}

static int opLiteGetLastError(sqlite3_vfs *pVfs, int nBuf, char *zBuf)
{
    zBuf[0] = '\0';
    return SQLITE_OK;
}

/*
 ** Initialize and deinitialize the operating system interface.
 */
extern "C" SQLITE_API int sqlite3_os_init(void)
{
    static sqlite3_vfs opLiteVfs = {
            1,                 /* iVersion */
            sizeof(OpLiteFile),   /* szOsFile */
            _MAX_PATH,        /* mxPathname */
            0,                 /* pNext */
            "opLite",             /* zName */
            0,                 /* pAppData */

            opLiteOpen,           /* xOpen */
            opLiteDelete,         /* xDelete */
            opLiteAccess,         /* xAccess */
            opLiteFullPathname,   /* xFullPathname */
            opLiteDlOpen,         /* xDlOpen */
            opLiteDlError,        /* xDlError */
            opLiteDlSym,          /* xDlSym */
            opLiteDlClose,        /* xDlClose */
            opLiteRandomness,     /* xRandomness */
            opLiteSleep,          /* xSleep */
            opLiteCurrentTime,    /* xCurrentTime */
            opLiteGetLastError    /* xGetLastError */
    };
    sqlite3_vfs_register(&opLiteVfs, 1);
    return SQLITE_OK;
}
extern "C" SQLITE_API int sqlite3_os_end(void)
{
    return SQLITE_OK;
}

#endif // SQLITE_OPERA_PI
