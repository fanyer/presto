/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Luca Venturi
**
*/
#ifndef CACHE_SIMPLE_STREAM_
#define CACHE_SIMPLE_STREAM_

#if defined(CACHE_FAST_INDEX) || CACHE_CONTAINERS_ENTRIES>0
#include "modules/url/url_tags.h"
#include "modules/util/opfile/opfile.h"
#include "modules/pi/network/OpSocketAddress.h"


// Maximum size of the relative section; this is just a double check to prevent the cache entry to go over 64KB
#define RELATIVE_MAX_SIZE 32768
#define _SET_STRING(val, str) { if(val) return str.Set(val->CStr()); return OpStatus::OK; }
class OpFileDescriptor;
class DiskCacheEntry;
class SimpleFileReadWrite;

class BufferedStream
{
protected:
	/// Temporary buffer
	unsigned char *buf;
	/// TRUE if the buffer is owned by the object
	BOOL own_buffer;

	/// If the buffer supplied is NULL, it will be owned by the object, else by the caller
	// The buffer needs to be at least STREAM_BUF_SIZE bytes
	BufferedStream(unsigned char *ext_buf)
	{
		buf=ext_buf;
		own_buffer=(ext_buf==NULL);
	}
	/// Fill the buffer with the given charcter
	void FillBuffer(unsigned char c) { if(buf) { for(int i=0; i<STREAM_BUF_SIZE; i++) buf[i]=c; } }
	
#ifdef _DEBUG
	/// Fill the buffer with 0xCC, in _DEBUG
	void DebugCleanBuffer() { FillBuffer(0xCC); }
#else
	/// Fill the buffer with 0xCC, in _DEBUG
	void DebugCleanBuffer() { }
#endif
	
public:
	/// Second phase contructor
	OP_STATUS Construct() { if(!buf) { buf=OP_NEWA(unsigned char, STREAM_BUF_SIZE); } return buf?OpStatus::OK : OpStatus::ERR_NO_MEMORY; }
	virtual ~BufferedStream() { if(own_buffer && buf) { OP_DELETEA(buf); } buf=NULL; }
};

/**
	Abstract class that read data from a buffer; implementation classes need to define a way to fill the buffer with
	the data required
*/
class SimpleStreamReader: public BufferedStream
{
protected:
	/// Bytes read in the buffer
	UINT32 read;
	/// Bytes available in the buffer
	UINT32 available;
	/// Total number of bytes read
	UINT32 total_read;
	/// Bytes read from the last snapshot
	UINT32 snapshot_read;

protected:
	/// Refill the buffer from the source
	virtual OP_STATUS RefillBuffer()=0;
	/// Move the bytes available but not read at the beginning of the buffer; read will be 0;
	/// Rerturn the number of bytes kept (==available)
	int CompactBuffer();
	/// Increase the number of bytes read;
	inline void IncRead(UINT32 n) { read+=n; total_read+=n; OP_ASSERT(read<=available); }

public:
	/// If the buffer supplied is NULL, it will be owned by the object, else by the caller
	// The buffer needs to be at least STREAM_BUF_SIZE bytes
	SimpleStreamReader(unsigned char *ext_buf): BufferedStream(ext_buf), read(0), available(0), total_read(0), snapshot_read(0) { }

	/** Read 64 bits; they NEED to be available (def will be returned in case of error)
		@param def: default value
	*/
	UINT64 Read64(UINT64 def);
	/** Read 32 bits; they NEED to be available (def will be returned in case of error)
		@param def: default value
	*/
	UINT32 Read32(UINT32 def);
	/** Read 16 bits; they NEED to be available (def will be returned in case of error)
		@param def: default value
	*/
	UINT32 Read16(UINT32 def);
	/** Read 8 bits; they NEED to be available (def will be returned in case of error)
		@param def: default value
	*/
	UINT32 Read8(UINT32 def);
	/// Read an int (len will specify if it is 1, 2, 4 or 8 bytes bits) as a UINT32
	inline UINT32 ReadInt(int len, UINT32 def);
	/// Read an int (len will specify if it is 1, 2, 4 or 8 bytes bits) as a UINT64
	inline UINT64 ReadInt64(int len, UINT64 def);

	/// Read 64 bits (faster version); they NEED to be available (0 will be returned in case of error)
	UINT64 Read64();
	/// Read 32 bits (faster version); they NEED to be available (0 will be returned in case of error)
	UINT32 Read32();
	/// Read 16 bits (faster version); they NEED to be available (0 will be returned in case of error)
	UINT32 Read16();
	/// Read 8 bits (faster version); they NEED to be available (0 will be returned in case of error)
	UINT32 Read8();
	/** Read an int as a UINT32 (faster version)
		@param len will specify if it is 1, 2, 4 or 8 bytes bits
	*/
	inline UINT32 ReadInt(int len);
	/** Read an int (len will specify if it is 1, 2, 4 or 8 bytes bits) as a UINT64
	*/
	inline UINT64 ReadInt64(int len);

	/// Read a time (32 or 64 bits); they NEED to be available (0 will be returned in case of error); len specify the len
	time_t ReadTime(int len);
	/// Read a (Pascal-like) String
	OP_STATUS ReadString(OpString8 *str, UINT32 len);
	/// Read a (Pascal-like) String
	OP_STATUS ReadString(OpString *str, UINT32 len);
	/// Read len bytes and put them in a buffer
	OP_STATUS ReadBuf(void *dest, UINT32 len);
	/// Skip the requested number of bytes
	OP_STATUS Skip(UINT32 len);
	/// Return the number of bytes read
	int GetBytesRead() { return total_read; }

	/// Empty an OpString8
	static inline void EmptyString(OpString8 *str) { OP_ASSERT(str); if(str && str->CStr()) ((char *)(str->CStr()))[0]=0; }
	/// Empty an OpString
	static inline void EmptyString(OpString *str) { OP_ASSERT(str); if(str && str->CStr()) ((uni_char *)(str->CStr()))[0]=0; }
	
	/// Return the number of bytes read from the last snapshot
	UINT32 GetSnapshotBytes() { return total_read-snapshot_read; }
	/// Take a snapshot and return GetSnapshotBytes()
	UINT32 TakeSnapshot() { UINT32 t=GetSnapshotBytes(); snapshot_read=total_read; return t; }
};

/**
	Abstract class that write data to a buffer; implementation classes need to define a way to flush the buffer and close the resource (if required)
*/
class SimpleStreamWriter: public BufferedStream
{
protected:
	/// Bytes written in the buffer
	UINT32 written;
	/// Total number of bytes written
	UINT32 total_written;
	/// Bytes written from the last snapshot
	UINT32 snapshot_written;

	/// Increase the number of bytes written;
	inline void IncWrite(UINT32 n) { written+=n; total_written+=n; OP_ASSERT(written<=STREAM_BUF_SIZE); }

public:
	/// If the buffer supplied is NULL, it will be owned by the object, else by the caller
	// The buffer needs to be at least STREAM_BUF_SIZE bytes
	SimpleStreamWriter(unsigned char *ext_buf): BufferedStream(ext_buf), written(0), total_written(0), snapshot_written(0) { }

	/// Save the buffer
	/// @param finished TRUE if this is the last call (so, for example, a file has to be closed)
	virtual OP_STATUS FlushBuffer(BOOL finished)=0;

	/// Close the stream, possibly flushing the buffer
	virtual OP_STATUS Close(BOOL flush_buffer = TRUE) = 0;


	/// Return the total number of bytes written
	UINT32 GetBytesWritten() { return total_written; }

	/// Write 64 bits
	OP_STATUS Write64(UINT64 val);
	/// Write 32 bits
	OP_STATUS Write32(UINT32 val);
	/// Write 16 bits
	OP_STATUS Write16(UINT32 val);
	/// Write 8 bits
	OP_STATUS Write8(UINT32 val);
	/// Write a time
	//OP_STATUS WriteTime(time_t val, int len) { Write32((UINT32)val); }
	/// Write a string
	OP_STATUS WriteString(OpString8 *val, int length_len);
	/// Write a buffer
	OP_STATUS WriteBuf(const void *src, UINT32 buf_len);
	
	/// Return the number of bytes read from the last snapshot
	UINT32 GetSnapshotBytes() { return total_written-snapshot_written; }
	/// Take a snapshot and return GetSnapshotBytes()
	UINT32 TakeSnapshot() { UINT32 t=GetSnapshotBytes(); snapshot_written=total_written; return t; }
};

/// Interface that describe a stream that can also be accessed randomly
class RandomAccessStream
{
public:
	// Get the file pointer position (dangerous operation)
	virtual OP_STATUS GetFilePos(OpFileLength& pos) const = 0;
	// Set the file pinter position (dangerous operation)
	virtual OP_STATUS SetFilePos(OpFileLength pos, OpSeekMode seek_mode=SEEK_FROM_START) const = 0;
	/// Return the length of the file
	virtual OP_STATUS GetFileLengthUnsafe(OpFileLength& len) const = 0;
};

/// Streamer writer that can also be accessed randomly
class SimpleRandomStreamWriter: public SimpleStreamWriter, public RandomAccessStream
{
public:
	friend class SimpleFileReadWrite;

 	SimpleRandomStreamWriter(unsigned char *ext_buf): SimpleStreamWriter(ext_buf) { }
};

/**
	Simple Stream that read the bytes from a file
*/
class SimpleFileReader: public SimpleStreamReader
{
private:
	friend class SimpleFileReadWrite;
	
	/// File to read
	OpFileDescriptor *file;
	/// True if the class has to delete the file
	BOOL take_over;

protected:
	virtual OP_STATUS RefillBuffer();
	// Get the file pointer position (dangerous operation)
	OP_STATUS GetFilePos(OpFileLength& pos) const { if(!file) return OpStatus::ERR_NULL_POINTER; return file->GetFilePos(pos); }
	// Set the file pointer position (dangerous operation)
	OP_STATUS SetFilePos(OpFileLength pos, OpSeekMode seek_mode=SEEK_FROM_START) const { if(!file) return OpStatus::ERR_NULL_POINTER; return file->SetFilePos(pos, seek_mode); }
	/// Retrieve the logical position (bytes read from the user, not just buffered from the disk) and set the physical to it
	//OP_STATUS GetLogicalFilePositionAndAlign(OpFileLength& pos);

public:
	/// If the buffer supplied is NULL, it will be owned by the object, else by the caller
	// The buffer needs to be at least STREAM_BUF_SIZE bytes
	SimpleFileReader(unsigned char *ext_buf=NULL): SimpleStreamReader(ext_buf) { file=NULL; take_over=TRUE; }

	virtual ~SimpleFileReader() { Close(); }
	/// Second phase contructor that requires an OpFile; if manage_file is TRUE, the OpFile will be later deleted by the class; in any case, the file will be closed.
	OP_STATUS Construct(OpFileDescriptor *data_file, BOOL manage_file=TRUE);
	/// Second phase contructor
	OP_STATUS Construct(const uni_char* path, OpFileFolder folder);
	/// Next Read will fetch from the disk; return the number of bytes that could have been read from the buffer, before clearing (it is the difference between the logical and physical read)
	UINT32 ClearBuffer() { UINT32 remaining=available-read; read=available=0; DebugCleanBuffer(); return remaining; }

	/// Close the file, and delete the pointer (if required)
	OP_STATUS Close();
	/// Return the length of the file
	OP_STATUS GetFileLength(OpFileLength& len) const;
	
	/// Return TRUE if the file is finished
	BOOL IsFileFinished() { OP_ASSERT(file); return read>=available && file && file->Eof(); }
};

/**
	Simple Stream that write the bytes to a file
*/
class SimpleFileWriter: public SimpleRandomStreamWriter
{
private:
	friend class SimpleFileReadWrite;
	/// File to read
	OpFileDescriptor *file;
	/// True if the class has to delete the file
	BOOL take_over;

protected:
	// Close the file
	OP_STATUS CloseFile(BOOL flush_buffer);

public:
	/// If the buffer supplied is NULL, it will be owned by the object, else by the caller
	// The buffer needs to be at least STREAM_BUF_SIZE bytes
	SimpleFileWriter(unsigned char *ext_buf=NULL): SimpleRandomStreamWriter(ext_buf)  { file=NULL; take_over=TRUE; }
	virtual ~SimpleFileWriter();

	/// Second phase contructor that requires an OpFile; if manage_file is TRUE, the OpFile will be later deleted by the class; in any case, the file will be closed.
	OP_STATUS Construct(OpFileDescriptor *data_file, BOOL manage_file=TRUE);
	/// Second phase contructor
	OP_STATUS Construct(const uni_char* path, OpFileFolder folder, BOOL safe_file);

	/// Close the file, and delete the pointer (if required). Also flush the buffer
	virtual OP_STATUS Close(BOOL flush_buffer = TRUE);

	virtual OP_STATUS FlushBuffer(BOOL finished);

	// Get the file pointer position (dangerous operation)
	virtual OP_STATUS GetFilePos(OpFileLength& pos) const { if(!file) return OpStatus::ERR_NULL_POINTER; return file->GetFilePos(pos); }
	// Set the file pinter position (dangerous operation)
	virtual OP_STATUS SetFilePos(OpFileLength pos, OpSeekMode seek_mode=SEEK_FROM_START) const { if(!file) return OpStatus::ERR_NULL_POINTER; return file->SetFilePos(pos, seek_mode); }
	/// Return the length of the file
	virtual OP_STATUS GetFileLengthUnsafe(OpFileLength& len) const;
};

/**
	Class that enable reading and writing from the same file. Switching from reading to writing could be relatively expensive (the file pointer could have to change and the buffer could have to be flush)
	This class uses no interfaces and virtual method to avoid changing the other classes, that are performance
	critical for the index of the cache, and can impact in the startup and sutdown time of Opera
*/
class SimpleFileReadWrite
{
private:
	// Operations
	enum Operation { OperationRead, OperationWrite, OperationSeek, OperationNone};
	
	// Object that write on the file
	SimpleRandomStreamWriter *sfw;
	// Object that read from the file
	SimpleFileReader *sfr;
	/// File to read
	OpFileDescriptor *file;
	/// Length of the file
	//OpFileLength length;
	/// Last operation performed
	Operation last_operation;
	
	/// Read position
	OpFileLength read_pos;
	/// Write position
	OpFileLength write_pos;
	/// TRUE if the file is in read only mode
	BOOL read_only;
	
	/// Switch to a read operation
	OP_STATUS SwitchToRead(BOOL force_clear_buffer);
	/// Switch to a write operation
	OP_STATUS SwitchToWrite(BOOL force_clear_buffer);
	/// Internal Second phase contructor
	OP_STATUS ConstructPrivate();
	
public:
	/** Different types of files supported */
	enum FileType { FileNormal, FileSafe, FileRAM };

	SimpleFileReadWrite(BOOL open_read_only=FALSE): sfw(NULL), sfr(NULL), file(NULL), last_operation(OperationNone), read_pos(0), write_pos(0), read_only(open_read_only) { }
	virtual ~SimpleFileReadWrite() { Close(); OP_DELETE(sfw); OP_DELETE(sfr); }
	/// Construct the object, and uses a normal or a safe file
	OP_STATUS ConstructFile(const uni_char* path, OpFileFolder folder, BOOL safe);
	/// Construct the object, and uses a memory file
	OP_STATUS ConstructMemory(UINT32 size);

	/// Close the file, and delete the pointer. Also flush the buffer
	OP_STATUS Close();
	/// Set the file in read only or R/W mode
	void SetReadOnly(BOOL read_only_mode) { read_only=read_only_mode; }
	/// Return TRUE if the file is read only
	BOOL IsReadOnly() const { return read_only; }
	
	///////////////////// Read operations  ///////////////
	/// Read 64 bits (faster version); they NEED to be available (0 will be returned in case of error)
	UINT64 Read64() { SwitchToRead(FALSE); return sfr->Read64(); }
	/// Read 32 bits (faster version); they NEED to be available (0 will be returned in case of error)
	UINT32 Read32() { SwitchToRead(FALSE); return sfr->Read32(); }
	/// Read 16 bits (faster version); they NEED to be available (0 will be returned in case of error)
	UINT32 Read16() { SwitchToRead(FALSE); return sfr->Read16(); }
	/// Read 8 bits (faster version); they NEED to be available (0 will be returned in case of error)
	UINT32 Read8() { SwitchToRead(FALSE); return sfr->Read8(); }
	/// Read len bytes and put them in a buffer
	OP_STATUS ReadBuf(void *dest, UINT32 len)  { SwitchToRead(FALSE); return sfr->ReadBuf(dest, len); }
	
	///////////////////// Write operations  ///////////////
	/// Write 64 bits
	OP_STATUS Write64(UINT64 val) { RETURN_IF_ERROR(SwitchToWrite(FALSE)); return sfw->Write64(val); }
	/// Write 32 bits
	OP_STATUS Write32(UINT32 val) { RETURN_IF_ERROR(SwitchToWrite(FALSE)); return sfw->Write32(val); }
	/// Write 16 bits
	OP_STATUS Write16(UINT32 val) { RETURN_IF_ERROR(SwitchToWrite(FALSE)); return sfw->Write16(val); }
	/// Write 8 bits
	OP_STATUS Write8(UINT32 val) { RETURN_IF_ERROR(SwitchToWrite(FALSE)); return sfw->Write8(val); }
	/// Write a buffer
	OP_STATUS WriteBuf(const void *src, UINT32 buf_len) { RETURN_IF_ERROR(SwitchToWrite(FALSE)); return sfw->WriteBuf(src, buf_len); }
	
	// Get the Read file pointer position
	OP_STATUS GetReadFilePos(OpFileLength& pos) { if(!sfr) return OpStatus::ERR_NULL_POINTER; pos=read_pos+sfr->GetSnapshotBytes(); return OpStatus::OK; /*SwitchToRead(TRUE); return sfr->GetFilePos(pos);*/ }
	// Set the Read file pointer position fromt he biginning of the file. This can be a relatively expensive operation
	OP_STATUS SetReadFilePos(OpFileLength pos) { if(!sfr) return OpStatus::ERR_NULL_POINTER; if(last_operation==OperationRead && pos==read_pos+sfr->GetSnapshotBytes()) return OpStatus::OK; sfr->ClearBuffer(); read_pos=pos; last_operation=OperationSeek; sfr->TakeSnapshot(); return OpStatus::OK; }
	// Get the Write file pointer position
	OP_STATUS GetWriteFilePos(OpFileLength& pos) { if(!sfw) return OpStatus::ERR_NULL_POINTER; pos=write_pos+sfw->GetSnapshotBytes(); return OpStatus::OK; }
	// Set the Write file pinter position. This can be a relatively expensive operation
	OP_STATUS SetWriteFilePos(OpFileLength pos);
	/// Return the length of the file
	OP_STATUS GetFileLength(OpFileLength& len);
	/// Return the length of the file in an "unsafe" way, but with a const method
	OP_STATUS GetFileLengthUnsafe(OpFileLength& len) const { if(!sfr) return OpStatus::ERR_NULL_POINTER; return sfr->GetFileLength(len); }
	/// b is set to TRUE if the file exists
	OP_STATUS Exists(BOOL &b) const { if(!sfr) return OpStatus::ERR_NULL_POINTER; return sfr->file->Exists(b); }
	/// Debug fucntion that returns the physical position of the file
	OP_STATUS DebugGetPhysicalPos(OpFileLength& pos) { if(!file) return OpStatus::ERR_NULL_POINTER; return file->GetFilePos(pos); }
	/// Return TRUE if the file is finished
	BOOL IsFileFinished() { return !sfr || sfr->IsFileFinished(); }
	
	/// Completely truncate the file. Be careful... Of course this function is valid only
	/// after a sucessful call to Construct()
	/// The file will be opened adter this call
	OP_STATUS Truncate();
	
	/// Return TRUE if the file is valid (not closed)
	BOOL IsFileValid() { return file!=NULL; }
	/// Return TRUE if the file is valid (not closed)
	BOOL IsFileOpened() const { return file && file->IsOpen(); }
	
	/// Flush the buffer, so all the content is saved
	OP_STATUS FlushWriteBuffer() { return sfw?sfw->FlushBuffer(FALSE):OpStatus::ERR_NULL_POINTER; }
};

/**
	Simple Reader that read the bytes from a buffer
*/
class SimpleBufferReader: public SimpleStreamReader
{
private:
	int cur;
	int size;
	const unsigned char *data;

public:
	SimpleBufferReader(const void *src, int dim): SimpleStreamReader(NULL) { cur=0; data=(const unsigned char *)src; size=dim; }
	virtual OP_STATUS RefillBuffer();
	OP_STATUS Construct() { return SimpleStreamReader::Construct(); }
};

/**
	Simple Reader that write the bytes to a buffer
*/
class SimpleBufferWriter: public SimpleStreamWriter
{
private:
	UINT32 cur;
	UINT32 size;
	unsigned char *data;

public:
	SimpleBufferWriter(void *src, int dim): SimpleStreamWriter(NULL)  { cur=0; data=(unsigned char *)src; size=dim; }
	virtual OP_STATUS FlushBuffer(BOOL finished);
	virtual OP_STATUS Close(BOOL flush_buffer = TRUE) { if(flush_buffer) return FlushBuffer(TRUE); return OpStatus::OK; }
	OP_STATUS Construct() { return SimpleStreamWriter::Construct(); }
};

/**
  Object used to with OpConfigFileReader to read a configuration file.
  See OpConfigFileReader for more informations
*/
class OpConfigFileHeader
{
private:
	friend class OpConfigFileReader;
	friend class OpConfigFileWriter;

	/// Application Version
	UINT32 app_version;
	/// File Version
	UINT32 file_version;
	/// Length of the tag
	UINT16 tag_len;
	/// Length of the "length" field
	UINT16 length_len;

	/// Default constructor
	OpConfigFileHeader() { app_version=file_version=tag_len=length_len=0; }

public:
	/// Get the application version
	UINT32 GetAppVersion() { return app_version; }
	/// Get the file version
	UINT32 GetFileVersion() { return file_version; }
	/// Return the length of the "tag" field
	UINT16 GetTagLength() { return tag_len; }
	/// Return the length of the "length" field
	UINT16 GetLengthLength() { return length_len; }
};

/**
  Stream used to read an Opera configuration file, like the one of the cache.
  Each file is composed by one header (12 bytes) and many records.
  Each record is composed by several Values.

  For more informations: http://www.opera.com/docs/fileformats/
  Check also url_tags.h
*/
class OpConfigFileReader: protected SimpleFileReader
{
private:
	friend class DiskCacheEntry;
	friend class HTTPCacheEntry;
	friend class HTTPHeaderEntry;
	friend class RelativeEntry;

	/// Configuration file
	OpConfigFileHeader header;
	/// Length of the current record
	UINT32 record_len;
	/// Bytes (relative to the beginning of the file) that mark the beginning of the current record
	UINT32 record_start;
	/// Function used to read the tag
	UINT32 (OpConfigFileReader::*read_tag_fn)(void);
	/// Function used to read the length
	UINT32 (OpConfigFileReader::*read_len_fn)(void);
	/// MSB value, used for reading the flags
	UINT32 msb;
protected:
	/// Read the header from the file
	OP_STATUS ReadHeader();

public:
	OpConfigFileReader(): SimpleFileReader(NULL) { record_len=0; record_start=0; read_tag_fn=NULL; read_len_fn=NULL; msb=0; }
	/// Second phase constructor: open the file and the header
	OP_STATUS Construct(const uni_char* path, OpFileFolder folder);
	/// Second phase constructor: read the header
	OP_STATUS Construct(OpFile *data_file, BOOL manage_file=TRUE);
	/**
		Begin to read a new Record
		@param tag Next record type
		@param len Length of the record; less than zero means that the file is finished
	*/
	OP_STATUS ReadNextRecord(UINT32 &record_tag, int &len);
	/**
		Begin to read a new Value
		@param tag Next Value type
		@param len Length of the record; less than zero means that the record is finished
	*/
	OP_STATUS ReadNextValue(UINT32 &value_tag, int &len);

	/// Close the file (could delete the OpFile object)
	OP_STATUS Close() { return SimpleFileReader::Close(); }

	/// Read a (pascal-like) string
	OP_STATUS ReadString(OpString8 *str, UINT32 len) { return SimpleFileReader::ReadString(str, len); }
	/// Read a (pascal-like) string
	OP_STATUS ReadString(OpString *str, UINT32 len) { return SimpleFileReader::ReadString(str, len); }

	// Return the byte position in the current record
	UINT32 GetRecordPosition() { return total_read-record_start; }

	// Return the configuration file
	OpConfigFileHeader *GetConfigFile() { return &header; }
};

/// Class that represent a dynamic attribute. This is also a valid implementation for a flag.
class StreamDynamicAttribute
{
protected:
	/// Tag
	UINT32 tag;
	/// Module ID
	UINT16 mod_id;
	/// Tag ID
	UINT16 tag_id;
	
public:
	/// Constructor
	StreamDynamicAttribute(UINT32 att_tag, UINT16 att_mod_id, UINT16 att_tag_id): tag(att_tag), mod_id(att_mod_id), tag_id(att_tag_id) { }
	/// Destructor
	virtual ~StreamDynamicAttribute() {}
	
	/// Get the tag (attribute type)
	UINT32 GetTag() { return tag; }
	/// Get the module id
	UINT16 GetModuleID() { return mod_id; }
	/// Get the tag id
	UINT16 GetTagID() { return tag_id; }
	/// Get the pointer
	virtual void *GetPointer() { return NULL; }
	/// Get the length
	virtual UINT32 GetLength() { return 0; }
	
	// Return TRUE if the tag is "long"
	BOOL IsLong() { return (tag==TAG_CACHE_DYNATTR_FLAG_ITEM_Long || tag==TAG_CACHE_DYNATTR_INT_ITEM_Long || tag==TAG_CACHE_DYNATTR_STRING_ITEM_Long || tag==TAG_CACHE_DYNATTR_URL_ITEM_Long); }
	// Return the length of the record (without external tag and length)
	UINT32 ComputeRecordLength() { return (IsLong()?4:2)+GetLength(); }
};

/// Dynamic attribute representing an Integer
class StreamDynamicAttributeInt: public StreamDynamicAttribute
{
private:
	/// INT stored
	UINT32 value;
public:
	/// Constructor
	StreamDynamicAttributeInt(UINT32 att_tag, UINT16 att_mod_id, UINT16 att_tag_id, UINT32 att_value): StreamDynamicAttribute(att_tag, att_mod_id, att_tag_id), value(att_value)
	{ OP_ASSERT(tag==TAG_CACHE_DYNATTR_INT_ITEM || tag==TAG_CACHE_DYNATTR_INT_ITEM_Long); }
	/// Destructor
	virtual ~StreamDynamicAttributeInt() {}
	
	/// Get the pointer
	virtual void *GetPointer() { return &value; }
	/// Get the length
	virtual UINT32 GetLength() { return 4; }
	/// Get directly the value
	virtual UINT32 GetValue() { return value; }
};

/// Dynamic attribute representing a String (or an URL)
class StreamDynamicAttributeString: public StreamDynamicAttribute
{
private:
	/// String stored
	OpString8 value;
public:
	/// Constructor
	StreamDynamicAttributeString(UINT32 att_tag, UINT16 att_mod_id, UINT16 att_tag_id): StreamDynamicAttribute(att_tag, att_mod_id, att_tag_id)
	{ OP_ASSERT(tag==TAG_CACHE_DYNATTR_STRING_ITEM || tag==TAG_CACHE_DYNATTR_STRING_ITEM_Long || tag==TAG_CACHE_DYNATTR_URL_ITEM || tag==TAG_CACHE_DYNATTR_URL_ITEM_Long); }
	/// Destructor
	virtual ~StreamDynamicAttributeString() {}
	
	/// Second phase constructor
	OP_STATUS Construct(const char *att_value) { return value.Set(att_value); }
	
	/// Get the pointer
	virtual void *GetPointer() { return value.CStr(); }
	/// Get the length
	virtual UINT32 GetLength() { return value.Length(); }
	/// Get directly the value
	virtual char *GetValue() { return value.CStr(); }
	/// Get the OpString8 object
	OpString8 *GetOpString8() { return &value; }
};


/**
  Stream used to write an Opera configuration file, like the one of the cache.
  Each file is composed by one header (12 bytes) and many records.
  Each record is composed by several Values.

  For more informations: http://www.opera.com/docs/fileformats/
  Check also url_tags.h
*/
class OpConfigFileWriter: protected SimpleFileWriter
{
private:
	friend class DiskCacheEntry;
	friend class HTTPCacheEntry;
	friend class HTTPHeaderEntry;
	friend class RelativeEntry;

	/// Configuration file
	OpConfigFileHeader header;
	/// Function used to write the tag
	OP_STATUS (OpConfigFileWriter::*write_tag_fn)(UINT32 val);
	/// Function used to write the length
	OP_STATUS (OpConfigFileWriter::*write_len_fn)(UINT32 val);
	/// MSB value, used for reading the flags
	UINT32 msb;

	/// Initialize the header
	OP_STATUS InitHeader(int tag_len, int length_len);

	/* Implemented to avoid warning about the Close() functions below hiding
	   this inherited virtual function instead of overriding it. */
	virtual OP_STATUS Close(BOOL flush_buffer = TRUE) { return SimpleFileWriter::Close(flush_buffer); }

public:
	OpConfigFileWriter(): SimpleFileWriter(NULL), msb(0) {}
	/// Write the header to the file
	OP_STATUS WriteHeader();
	/// Second phase constructor: open the file and the header
	OP_STATUS Construct(const uni_char* path, OpFileFolder folder, int tag_len, int length_len, BOOL safe_file);
	/// Second phase constructor: read the header
	OP_STATUS Construct(OpFile *data_file, int tag_len, int length_len, BOOL manage_file=TRUE);

	/// Write a tag
	// @param tag Tag ID
	// @param record_len Length of the whole record
	OP_STATUS WriteRecord(UINT32 record_tag, UINT32 record_len);

	/// Write the last record and close the file (could delete the OpFile object)
	/// @param next_file_name Next file name used for the disk cache
	OP_STATUS Close(OpString *next_file_name) { return Close((char *)((next_file_name)?next_file_name->CStr():NULL)); }
	/// Write the last record and close the file (could delete the OpFile object)
	/// @param next_file_name Next file name used for the disk cache
	OP_STATUS Close(char *next_file_name);

	/// Write a tag of 64 bits
	OP_STATUS Write64Tag(UINT32 tag, UINT64 val) { RETURN_IF_ERROR((this->*write_tag_fn)(tag)); RETURN_IF_ERROR((this->*write_len_fn)(8)); return Write64(val); }
	/// Write a tag of 32 bits
	OP_STATUS Write32Tag(UINT32 tag, UINT32 val) { RETURN_IF_ERROR((this->*write_tag_fn)(tag)); RETURN_IF_ERROR((this->*write_len_fn)(4)); return Write32(val); }
	/// Write a tag of 16 bits
	OP_STATUS Write16Tag(UINT32 tag, UINT32 val) { RETURN_IF_ERROR((this->*write_tag_fn)(tag)); RETURN_IF_ERROR((this->*write_len_fn)(2)); return Write16(val); }
	/// Write a tag of 8 bits
	OP_STATUS Write8Tag(UINT32 tag, UINT32 val) { RETURN_IF_ERROR((this->*write_tag_fn)(tag)); RETURN_IF_ERROR((this->*write_len_fn)(1)); return Write8(val); }
	/// Write a tag of a time (32 bits)
	OP_STATUS WriteTime32Tag(UINT32 tag, time_t val) { return Write32Tag(tag, (UINT32)val); }
	/// Write a tag of a time (64 bits)
	OP_STATUS WriteTime64Tag(UINT32 tag, time_t val) { return Write64Tag(tag, val); }
	/// Write a tag of a BOOL
	OP_STATUS WriteBoolTag(UINT32 tag, BOOL b) { if(b) return Write8(tag | msb); return OpStatus::OK; }
	/// Write a tag of an OpString8
	OP_STATUS WriteStringTag(UINT32 tag, OpString8 *val, BOOL write_also_empty)
	{
		if(!write_also_empty && (!val || val->IsEmpty()))
			return OpStatus::OK;

		RETURN_IF_ERROR((this->*write_tag_fn)(tag));

		return WriteString(val, header.length_len);
	}
	/// Write a tag of an OpString
	OP_STATUS WriteStringTag(UINT32 tag, OpString *val, BOOL write_also_empty)
	{
		if(!write_also_empty && (!val || val->IsEmpty()))
			return OpStatus::OK;

		OpString8 temp;
		ANCHOR(OpString8, temp);

		if(val)
			RETURN_IF_ERROR(temp.SetUTF8FromUTF16(val->CStr()));

		return WriteStringTag(tag, &temp, write_also_empty);
	}
	/// Write a dynamic attribute tag
	OP_STATUS WriteDynamicAttributeTag(UINT32 tag, UINT16 mod_id, UINT16 tag_id, void *ptr, UINT32 len);
	/// Write a dynamic attribute tag
	OP_STATUS WriteDynamicAttributeTag(StreamDynamicAttribute *att);
	/// Write a buffer
	OP_STATUS WriteBufTag(UINT32 tag, const void *buf, UINT32 len) { RETURN_IF_ERROR((this->*write_tag_fn)(tag)); RETURN_IF_ERROR((this->*write_len_fn)(len)); return WriteBuf(buf, len); }
};


/// Interface of a cache Entry
class CacheEntry
{
protected:
	/// Get the length of num BOOLEANs fields
	static int GetBoolLen(OpConfigFileHeader *header, BOOL b) { return (b)?1:0; }
	/// Get the length of num 64 bits fields
	static int GetUINT64Len(OpConfigFileHeader *header, int num) { return num*(header->GetTagLength()+header->GetLengthLength()+8); }
	/// Get the length of num 32 bits fields
	static int GetUINT32Len(OpConfigFileHeader *header, int num) { return num*(header->GetTagLength()+header->GetLengthLength()+4); }
	/// Get the length of num 16 bits fields
	static int GetUINT16Len(OpConfigFileHeader *header, int num) { return num*(header->GetTagLength()+header->GetLengthLength()+2); }
	/// Get the length of num 8 bits fields
	static int GetUINT8Len(OpConfigFileHeader *header, int num) { return num*(header->GetTagLength()+header->GetLengthLength()+1); }
	/// Get the length of num Time (32 bits) fields
	static int GetTime32Len(OpConfigFileHeader *header, int num) { return GetUINT32Len(header, num); }
	/// Get the length of num Time (64 bits) fields
	static int GetTime64Len(OpConfigFileHeader *header, int num) { return GetUINT64Len(header, num); }
	/// Get the length of an OpString8
	static int GetStringLen(OpConfigFileHeader *header, OpString8 *str, BOOL count_also_empty)
	{
		if(!count_also_empty && (!str || str->IsEmpty()))
			return 0;

		return header->GetTagLength()+header->GetLengthLength()+str->Length();
	}
	/// Get the length of an OpString
	/*static int GetStringLen(OpConfigFileHeader *header, OpString *str, BOOL count_also_empty)
	{
		if(!count_also_empty && (!str || str->IsEmpty()))
			return 0;

		return header->GetTagLength()+header->GetLengthLength()+str->Length();
	}*/
	/// Get the length of a buffer
	static int GetBufferLen(OpConfigFileHeader *header, void *buf, UINT32 len)
	{
		return header->GetTagLength()+header->GetLengthLength()+len;
	}



public:
	/// Get a (known) int value by its tag; return FALSE when the tag is not available
	virtual BOOL GetIntValueByTag(UINT32 tag_id, UINT32 &value)=0;
	/// Get a (known) string value by its tag; return FALSE when the tag is not available; the content of the string will be valid
	/// till the next ReadValues() call, and will be freed with the destructor. So the user should copy it
	virtual BOOL GetStringPointerByTag(UINT32 tag_id, OpString8 **str)=0;
	/// Set a (known) int value by its tag; if the tag is unknow, an ASSERT will
	/// be fired, but nothing will be done in release
	virtual void SetIntValueByTag(UINT32 tag_id, UINT32 value)=0;
	/// Set a (known) String value by its tag; if the tag is unknow, an error will be reported
	virtual OP_STATUS SetStringValueByTag(UINT32 tag_id, const OpStringC8 *str)=0;

	virtual ~CacheEntry() { }
};

/// Entry of a relative link
class RelativeEntry: public CacheEntry
{
private:
	/// Link name;
	OpString8 name;
	/// Last visited time;
	time_t visited;

protected:
	virtual BOOL GetIntValueByTag(UINT32 tag_id, UINT32 &value) { OP_ASSERT(FALSE); return FALSE; };
	virtual BOOL GetStringPointerByTag(UINT32 tag_id, OpString8 **str) { OP_ASSERT(FALSE); return FALSE; };
	virtual void SetIntValueByTag(UINT32 tag_id, UINT32 value) { OP_ASSERT(FALSE); }
	/// Set a (known) String value by its tag; if the tag is unknow, an error will be reported
	virtual OP_STATUS SetStringValueByTag(UINT32 tag_id, const OpStringC8 *str) { OP_ASSERT(FALSE); return OpStatus::ERR; }

public:
	/// Set the name and the time
	OP_STATUS SetValues(OpStringC8 *rel_name, time_t rel_visited);
	/// Return the name of the header
	OpString8 *GetName() { return &name; }
	/// Return the last visited time
	time_t GetLastVisited() { return visited; }
	/// Reset the values
	void Reset()
	{
		SimpleStreamReader::EmptyString(&name);
		visited=0;
	}

	/// Read all the values from an OpConfigFileReader
	OP_STATUS ReadValues(OpConfigFileReader *cache, UINT32 record_len);

	/// Compute the length (on the disk) of the whole record
	////////////////////////////////////////////////////////////////////////////
	/////// ComputeRecordLength() and WriteValues MUST BE SYNCHRONIZED
	////////////////////////////////////////////////////////////////////////////
	OP_STATUS ComputeRecordLength(OpConfigFileHeader *header, int &len)
	{
		len=GetStringLen(header, &name, TRUE) + GetTime32Len(header, 1);

		return OpStatus::OK;
	}
	/// Write all the record values to an OpConfigFileReader
	OP_STATUS WriteValues(OpConfigFileWriter *cache);
};


/// Entry of an HTTP Header
class HTTPHeaderEntry: public CacheEntry
{
private:
	/// Header name;
	OpString8 name;
	/// Header value;
	OpString8 value;
	/// True if there are HTTP data
	BOOL data_available;

public:
	/// Return the name of the header
	OpString8 *GetName() { return &name; }
	/// Return the value of the header
	OpString8 *GetValue() { return &value; }

	/// Set the name
	OP_STATUS SetName(const OpStringC8 *val) { _SET_STRING(val, name); }
	/// Set the value
	OP_STATUS SetValue(const OpStringC8 *val) { _SET_STRING(val, value); }

	/// True if there are data available
	BOOL IsDataAvailable() { return data_available; }
	/// Reset the values
	void Reset()
	{
		data_available=FALSE;
		SimpleStreamReader::EmptyString(&name);
		SimpleStreamReader::EmptyString(&value);
	}

	/// Read all the values from an OpConfigFileReader
	OP_STATUS ReadValues(OpConfigFileReader *cache);
	/// Get a (known) string value by its tag; return FALSE when the tag is not available; the content of the string will be valid
	/// till the next ReadValues() call, and will be freed with the destructor. So the user should copy it
	BOOL GetStringPointerByTag(UINT32 tag_id, OpString8 **str);
	/// Get a (known) int value by its tag; return FALSE when the tag is not available
	BOOL GetIntValueByTag(UINT32 tag_id, UINT32 &value) { OP_ASSERT(FALSE); value=0; return FALSE; }
	/// Set a (known) int value by its tag; if the tag is unknow, an ASSERT will
	/// be fired, but nothing will be done in release
	void SetIntValueByTag(UINT32 tag_id, UINT32 value) { OP_ASSERT(FALSE); }
	/// Set a (known) String value by its tag; if the tag is unknow, an error will be reported
	virtual OP_STATUS SetStringValueByTag(UINT32 tag_id, const OpStringC8 *str) { OP_ASSERT(FALSE); return OpStatus::ERR; }

	/// Compute the length (on the disk) of the whole record
	////////////////////////////////////////////////////////////////////////////
	/////// ComputeRecordLength() and WriteValues MUST BE SYNCHRONIZED
	////////////////////////////////////////////////////////////////////////////
	OP_STATUS ComputeRecordLength(OpConfigFileHeader *header, int &len)
	{
		len=GetStringLen(header, &name, TRUE) + GetStringLen(header, &value, TRUE);

		return OpStatus::OK;
	}

	/// Write all the record values to an OpConfigFileReader
	OP_STATUS WriteValues(OpConfigFileWriter *cache);
};

/// "HTTP Entry" of the disk cache index files; it is a record inside a normal cache record
class HTTPCacheEntry: public CacheEntry
{
private:
	friend class DiskCacheEntry;

	/// HTTP Response code
	UINT32 response_code;
	/// HTTP age
	UINT32 age;
	/// User Agent ID;
	UINT32 agent_id;
	/// User Agnet Version ID
	UINT32 agent_ver_id;
	/// Refresh Interval
	UINT32 refresh_int;

	/// Load Date
	OpString8 keep_load_date;
	/// Last modified
	OpString8 keep_last_modified;
	/// Encoding
	OpString8 keep_encoding;
	/// Entity Tag
	OpString8 keep_entity;
	/// Moved tu URL
	OpString8 moved_to;
	/// HTTP Response Text
	OpString8 response_text;
	/// Refresh URL
	OpString8 refresh_url;
	/// Content Location
	OpString8 location;
	/// Content Language
	OpString8 language;
	/// Suggested name
	OpString8 suggested;

	/// Link Relation
	OpAutoVector<OpString8> ar_link_rel;

	#define HEADER_DEF_SIZE 16
	/// HTTP "default" Headers: used to speed-up things, because you don't need to allocate
	HTTPHeaderEntry ar_headers_def[HEADER_DEF_SIZE];
	/// Number of header available
	UINT32 headers_num;
	/// HTTP Headers: fallback for when there are more headers than expected...
	OpAutoVector<HTTPHeaderEntry> ar_headers_vec;

	/// True if there are HTTP data
	BOOL data_available;

public:
	~HTTPCacheEntry() { Reset(); }
	/// Reset the values
	void Reset()
	{
		data_available=FALSE;
		response_code=0; age=0; agent_id=0; agent_ver_id=0;
		SimpleStreamReader::EmptyString(&keep_load_date);	SimpleStreamReader::EmptyString(&keep_last_modified);	SimpleStreamReader::EmptyString(&keep_encoding);
		SimpleStreamReader::EmptyString(&keep_entity); SimpleStreamReader::EmptyString(&moved_to);	SimpleStreamReader::EmptyString(&response_text);
		SimpleStreamReader::EmptyString(&refresh_url); SimpleStreamReader::EmptyString(&location);	SimpleStreamReader::EmptyString(&language);
		SimpleStreamReader::EmptyString(&suggested);

		ar_link_rel.DeleteAll();
		ar_link_rel.Clear();
		ar_headers_vec.DeleteAll();
		ar_headers_vec.Clear();
		headers_num=0;
	}
	/// Read all the values from an OpConfigFileReader
	OP_STATUS ReadValues(OpConfigFileReader *cache, DiskCacheEntry *entry, UINT32 record_len);
	/// Write all the record values to an OpConfigFileReader
	OP_STATUS WriteValues(OpConfigFileWriter *cache, int expected_len, int headers_len);
	/// Get a (known) int value by its tag; return FALSE when the tag is not available
	BOOL GetIntValueByTag(UINT32 tag_id, UINT32 &value);
	/// Get a (known) string value by its tag; return FALSE when the tag is not available; the content of the string will be valid
	/// till the next ReadValues() call, and will be freed with the destructor. So the user should copy it
	BOOL GetStringPointerByTag(UINT32 tag_id, OpString8 **str);
	/// Set a (known) int value by its tag; if the tag is unknow, an ASSERT will
	/// be fired, but nothing will be done in release
	void SetIntValueByTag(UINT32 tag_id, UINT32 value);
	/// Set a (known) String value by its tag; if the tag is unknow, an error will be reported
	virtual OP_STATUS SetStringValueByTag(UINT32 tag_id, const OpStringC8 *str);
	/// True if there are data available
	BOOL IsDataAvailable() { return data_available; }
	/// Compute the length (on the disk) of the whole record
	OP_STATUS ComputeRecordLength(OpConfigFileHeader *header, int &len, int &headers_len);


	/// Return the HTTP Response code
	UINT32 GetResponseCode() { return response_code; }
	/// Return the HTTP age
	UINT32 GetAge() { return age; }
	/// Return the User Agent ID;
	UINT32 GetAgentID() { return agent_id; }
	/// Return the User Agnet Version ID
	UINT32 GetAgentVersionID() { return agent_ver_id; };
	/// Return the Refresh Interval
	UINT32 GetRefreshInterval() { return refresh_int; }

	/// Return the HTTP Response code
	void SetResponseCode(UINT32 val) { response_code=val; }
	/// Return the HTTP age
	void SetAge(UINT32 val) { age=val; }
	/// Return the User Agent ID;
	void SetAgentID(UINT32 val) { agent_id=val; }
	/// Return the User Agnet Version ID
	void SetAgentVersionID(UINT32 val) { agent_ver_id=val; };
	/// Return the Refresh Interval
	void SetRefreshInterval(UINT32 val) { refresh_int=val; }

	/// Return the Load Date
	OpString8 *GetKeepLoadDate() { return &keep_load_date; }
	/// Return the Last modified
	OpString8 *GetKeepLastModified() { return &keep_last_modified; }
	/// Return the Encoding
	OpString8 *GetKeepEncoding() { return &keep_encoding; }
	/// Return the Entity Tag
	OpString8 *GetKeepEntity() { return &keep_entity; }
	/// Return the Moved tu URL
	OpString8 *GetMovedTo() { return &moved_to; }
	/// Return the HTTP Response Text
	OpString8 *GetResponseText() { return &response_text; }
	/// Return the Refresh URL
	OpString8 *GetRefreshString() { return &refresh_url; }
	/// Return the Content Location
	OpString8 *GetLocation() { return &location; }
	/// Return the Content Language
	OpString8 *GetLanguage() { return &language; }
	/// Return the suggested name
	OpString8* GetSuggestedName() { return &suggested; }

	/// Set the Load Date
	OP_STATUS SetKeepLoadDate(const OpStringC8 *val) { _SET_STRING(val, keep_load_date); }
	/// Set the Last modified
	OP_STATUS SetKeepLastModified(const OpStringC8 *val) { _SET_STRING(val, keep_last_modified); }
	/// Set the Encoding
	OP_STATUS SetKeepEncoding(const OpStringC8 *val) { _SET_STRING(val, keep_encoding); }
	/// Set the Entity Tag
	OP_STATUS SetKeepEntity(const OpStringC8 *val) { _SET_STRING(val, keep_entity); }
	/// Set the Moved tu URL
	OP_STATUS SetMovedTo(const OpStringC8 *val) { _SET_STRING(val, moved_to); }
	/// Set the HTTP Response Text
	OP_STATUS SetResponseText(const OpStringC8 *val) { _SET_STRING(val, response_text); }
	/// Set the Refresh URL
	OP_STATUS SetRefreshString(const OpStringC8 *val) { _SET_STRING(val, refresh_url); }
	/// Set the Content Location
	OP_STATUS SetLocation(const OpStringC8 *val) { _SET_STRING(val, location); }
	/// Set the Content Language
	OP_STATUS SetLanguage(const OpStringC8 *val) { _SET_STRING(val, language); }
	/// Set the suggested name
	OP_STATUS SetSuggestedName(const OpStringC8 *val) { _SET_STRING(val, suggested); }


	/// Return the number of link relations available
	UINT32 GetLinkrelationsCount() { return ar_link_rel.GetCount(); }
	/// Return the requested link relation
	OpString8* GetLinkrelation(UINT32 index) { return ar_link_rel.Get(index); }
	/// Add a link relation
	OP_STATUS AddLinkRelation(OpStringC8* val);
	/// Return the number of HTTP headers available
	UINT32 GetHeadersCount() { return headers_num+ar_headers_vec.GetCount(); }
	/// Return the requested HTTP header
	HTTPHeaderEntry* GetHeader(UINT32 index);
	// Add an header
	OP_STATUS AddHeader(const OpStringC8 *name, const OpStringC8 *value);
};

/** Single "normal" entry of the disk cache index files */
class DiskCacheEntry: public CacheEntry
{
private:
	/// TAG of the current record (usually 1)
	int record_tag;
	/// HTTP Data
	HTTPCacheEntry http;

	/// URL
	OpString8 url;
	/// Name of the cache file
	OpString8 file_name;
	/// Modified date time
	OpString8 ftp_mdtm_date;
	/// MIME Content Type
	OpString8 content_type;
	/// Complete MIME Content Type as returned by server
	OpString8 server_content_type;
	/// Charset
	OpString8 charset;
	/// Content of the file
	unsigned char *embedded_content;
	/// Size of the content of the file
	UINT32 embedded_content_size;
	/// Size reserved for the content of the file
	UINT32 embedded_content_size_reserved;

	/// TRUE if it is the result fo a form query
	BOOL form_query;
	BOOL never_flush;
	/// True if the file is not in cache but has been downloaded by the user
	BOOL download;
	/// Always check if modified
	BOOL always_verify;
	/// TRUE if the file is stored in a "Multimedia Cache" file
	BOOL multimedia;
	/// TRUE if the MIME type was NULL (used by XHR to bypass the MIME sniffing)
	BOOL content_type_was_null;
	BOOL must_validate;

	/// Size of the content
	UINT64 content_size;

	/// ID of the container (0 means no container at all)
	UINT8 container_id;

	// Associated files
	UINT32 assoc_files;
	// Loading status
	UINT32 status;
	// Type of network that given resource has been loaded from
	UINT32 loaded_from_net_type;

	/// Last visited time
	time_t last_visited;
	/// Local time loaded
	time_t local_time;
	/// Expiry time
	time_t expiry;

	/// Relative Links
	OpAutoVector<RelativeEntry>ar_relative_vec;
	
	/// Dynamic attributes
	OpAutoVector<StreamDynamicAttribute>ar_dynamic_att;

	// Resume status
	URL_Resumable_Status resumable;

private:
	/// Compute the length (on the disk) of the whole record
	OP_STATUS ComputeRecordLength(OpConfigFileHeader *header, int &len, int &http_len, int &headers_len, UINT32 tag, BOOL embedded, UINT32 &len_rel);
	/// Read a dynamic attribute and add it to ar_dynamic_att
	OP_STATUS ReadDynamicAttribute(UINT32 tag, int len, OpConfigFileReader *cache);

public:
	DiskCacheEntry() { embedded_content=NULL; embedded_content_size_reserved=0; Reset(); }
	virtual ~DiskCacheEntry() { Reset(); if(embedded_content) OP_DELETEA(embedded_content); embedded_content=NULL; }
	/// Reset the values
	void Reset()
	{
		SimpleStreamReader::EmptyString(&url); SimpleStreamReader::EmptyString(&file_name); SimpleStreamReader::EmptyString(&ftp_mdtm_date); SimpleStreamReader::EmptyString(&content_type); SimpleStreamReader::EmptyString(&charset); embedded_content_size=0;
		SimpleStreamReader::EmptyString(&server_content_type);
		form_query=FALSE; never_flush=FALSE; download=FALSE; always_verify=FALSE; must_validate=FALSE; multimedia=FALSE; content_type_was_null=FALSE;
		content_size=0; assoc_files=0; status=0;
		last_visited=0; local_time=0; expiry=0;
		resumable=Resumable_Unknown;
		container_id=0;
		loaded_from_net_type=NETTYPE_UNDETERMINED;

		http.Reset();
		ar_relative_vec.DeleteAll();
		ar_relative_vec.Clear();
		ar_dynamic_att.DeleteAll();
		ar_dynamic_att.Clear();
	}
	/// Return the current tag
	int GetTag() { return record_tag; }
	/// Set the current tag
	void SetTag(UINT32 tag) { record_tag=tag; }
	/// Read all the values from an OpConfigFileReader
	OP_STATUS ReadValues(OpConfigFileReader *cache, UINT32 tag);
	/// Write all the record values to an OpConfigFileWriter
	/// @param cache Cache File object
	/// @param tag cache tag
	/// @param embedded TRUE if the file is embedded (stored in the index itself)
	/// @param url_refused TRUE if the URL has been refused (not added to the index), for example because it is too big.
	///	       The caller must avoid that the content of the URL persists on disk while not being indexed
	OP_STATUS WriteValues(OpConfigFileWriter *cache, UINT32 tag, BOOL embedded, BOOL &url_refused);
	/// Get a (known) int value by its tag; return FALSE when the tag is not available
	BOOL GetIntValueByTag(UINT32 tag_id, UINT32 &value);
	/// Get a (known) string value by its tag; return FALSE when the tag is not available; the content of the string will be valid
	/// till the next ReadValues() call, and will be freed with the destructor. So the user should copy it
	BOOL GetStringPointerByTag(UINT32 tag_id, OpString8 **str);
	/// Set a (known) int value by its tag; if the tag is unknow, an ASSERT will
	/// be fired, but nothing will be done in release
	void SetIntValueByTag(UINT32 tag_id, UINT32 value);
	/// Set a (known) String value by its tag; if the tag is unknow, an error will be reported
	virtual OP_STATUS SetStringValueByTag(UINT32 tag_id, const OpStringC8 *str);
	/// Return the HTTP Data
	HTTPCacheEntry *GetHTTP() { return &http; }

	/// Return the URL
	OpString8 *GetURL() { return &url; }
	/// Return the name of the cache file that contains the content
	OpString8 *GetFileName() { return &file_name; }
	/// Return the FTP Modified Date Time
	OpString8 *GetFTPModifiedDateTime() { return &ftp_mdtm_date; }
	/// Return the MIME Content Type
	OpString8 *GetContentType() { return &content_type; }
	/// Return the complete MIME Content Type
	OpString8* GetServerContentType() { return &server_content_type; }
	/// Return the charset
	OpString8 *GetCharset() { return &charset; }
	/// Return the content of the file embedded in the index
	unsigned char *GetEmbeddedContent() { return embedded_content; }
	/// Return the actual size of the file embedded in the index
	UINT32 GetEmbeddedContentSize() { return embedded_content_size; }
	/// Return the size reserved for the content to embedded in the index
	UINT32 GetEmbeddedContentSizeReserved() { return embedded_content_size_reserved; }
	/// Return the container ID
	UINT8 GetContainerID() { return container_id; }

	/// Set the URL
	OP_STATUS SetURL(const OpStringC8 *val) { _SET_STRING(val, url); }
	/// Set the name of the cache file that contains the content
	OP_STATUS SetFileName(const OpStringC8 *val) { _SET_STRING(val, file_name); }
	/// Set the FTP Modified Date Time
	OP_STATUS SetFTPModifiedDateTime(const OpStringC8 *val) { _SET_STRING(val, ftp_mdtm_date); }
	/// Set the MIME Content Type
	OP_STATUS SetContentType(const OpStringC8 *val) { _SET_STRING(val, content_type); }
	/// Return the complete MIME Content Type
	OP_STATUS SetServerContentType(const OpStringC8 *val) { _SET_STRING(val, server_content_type); }
	/// Set the charset
	OP_STATUS SetCharset(const OpStringC8 *val) { _SET_STRING(val, charset); }
	/// Set the size of the content to embed in the index
	void SetEmbeddedContentSize(UINT32 size) { embedded_content_size=size; }
	/// Set the container ID
	void SetContainerID(UINT8 id) { container_id=id; }
	/// Reserve the space required
	OP_STATUS ReserveSpaceForEmbeddedContent(UINT32 size);

	/// Return the Content Size
	UINT64 GetContentSize() { return content_size; }
	/// Return the associated files
	UINT32 GetAssociatedFiles() { return assoc_files; }
	/// Return the loading status
	UINT32 GetStatus() { return status; }

	/// Set the Content Size
	void SetContentSize(UINT64 val) { content_size=val; }
	/// Set the associated files
	void SetAssociatedFiles(UINT32 val) { assoc_files=val; }
	/// Set the loading status
	void SetStatus(UINT32 val) { status=val; }
	/// Add a relative entry
	OP_STATUS AddRelativeLink(OpStringC8 *name, time_t visited);

	/// Returns TRUE if the URL is the result of a form query
	BOOL GetFormQuery() { return form_query; }
	/// Returns the TAG_CACHE_NEVER_FLUSH cache value
	BOOL GetNeverFlush() { return never_flush; }
	/// True if the file is not in cache but has been downloaded by the user
	BOOL GetDownload() { return download; }
	/// Returns TRUE if the content must always be check to see if it has been modified
	BOOL GetAlwaysVerify() { return always_verify; }
	/// Returns TRUE if the content is stored in a "Multimedia Cache" file
	BOOL GetMultimedia() { return multimedia; }
	/// Returns TRUE if the MIME type was nUll, for XHR
	BOOL GetContentTypeWasNULL() { return content_type_was_null; }
	/// Returns the TAG_CACHE_MUST_VALIDATE cache value
	BOOL GetMustValidate() { return must_validate; }

	/// Set if the URL is the result of a form query
	void SetFormQuery(BOOL val) { form_query=val; }
	/// Set if the TAG_CACHE_NEVER_FLUSH cache value
	void SetNeverFlush(BOOL val) { never_flush=val; }
	/// Set if the file is not in cache but has been downloaded by the user
	void SetDownload(BOOL val) { download=val; }
	/// Set if the content must always be check to see if it has been modified
	void SetAlwaysVerify(BOOL val) { always_verify=val; }
	/// Set if the content is stored in a "Multimedia Cache" file
	void SetMultimedia(BOOL val) { multimedia=val; }
	/// Set if the MIME type was nUll, for XHR
	void SetContentTypeWasNULL(BOOL val) { content_type_was_null=val; }
	/// Return the TAG_CACHE_MUST_VALIDATE cache value
	void SetMustValidate(BOOL val) { must_validate=val; }

	/// Return the time of the last visite
	time_t GetLastVisited() { return last_visited; }
	/// Return the local time of the loading
	time_t GetLocalTime() { return local_time; }
	/// Return the expiry date
	time_t GetExpiry() { return expiry; }

	/// Set the time of the last visite
	void SetLastVisited(time_t val) { last_visited=val; }
	/// Set the local time of the loading
	void SetLocalTime(time_t val) { local_time=val; }
	/// Set the expiry date
	void SetExpiry(time_t val) { expiry=val; }
	/// Set available data status
	void SetDataAvailable(BOOL is_data_available){ http.data_available = is_data_available; }
	
	/// Return the resumable status
	URL_Resumable_Status GetResumable() { return resumable; }
	/// Set the resumable status
	void SetResumable(URL_Resumable_Status value) { resumable=value; }

	/// Return the number of relative links
	UINT32 GetRelativeLinksCount() { return ar_relative_vec.GetCount(); }
	/// Return the requested relative link
	RelativeEntry* GetRelativeLink(UINT32 index) { return ar_relative_vec.Get(index); }
	
	/// Return the number of dynamic attributes
	UINT32 GetDynamicAttributesCount() { return ar_dynamic_att.GetCount(); }
	/// Return the requested dynamic attribute
	StreamDynamicAttribute* GetDynamicAttribute(UINT32 index) { return ar_dynamic_att.Get(index); }
	/// Add a dynamic attribute
	OP_STATUS AddDynamicAttribute(StreamDynamicAttribute *att) { OP_ASSERT(att); return (att)?ar_dynamic_att.Add(att):OpStatus::ERR_NULL_POINTER; }
	/// Add a dynamic attriubute; this is called in URL_DataStorage
	OP_STATUS AddDynamicAttributeFromDataStorage(UINT32 mod_id, UINT32 tag_id, UINT32 short_tag_version, void *ptr, UINT32 len);
};


inline UINT32 SimpleStreamReader::ReadInt(int len, UINT32 def)
{
	OP_ASSERT(len==8 || len==4 || len==2 || len==1);

	if(len==1)
		return Read8(def);
	if(len==2)
		return Read16(def);
	if(len==4)
		return Read32(def);
	if(len==8)
		return (UINT32)Read64(def);

	return def;
}

inline UINT32 SimpleStreamReader::ReadInt(int len)
{
	OP_ASSERT(len==8 || len==4 || len==2 || len==1);

	if(len==1)
		return Read8();		// It should be the most common operation: its the tag byte
	if(len==2)
		return Read16();	// It should be the second most common operation: the "length" is ususally 2 bytes
	if(len==4)
		return Read32();
	if(len==8)
		return (UINT32)Read64();

	return 0;
}

inline UINT64 SimpleStreamReader::ReadInt64(int len, UINT64 def)
{
	OP_ASSERT(len==8 || len==4 || len==2 || len==1);

	if(len==8)
		return Read64(def);
	if(len==4)
		return Read32((UINT32)def);
	if(len==2)
		return Read16((UINT32)def);
	if(len==1)
		return Read8((UINT32)def);

	return def;
}

inline UINT64 SimpleStreamReader::ReadInt64(int len)
{
	OP_ASSERT(len==8 || len==4 || len==2 || len==1);

	if(len==8)
		return Read64();
	if(len==4)
		return Read32();
	if(len==2)
		return Read16();
	if(len==1)
		return Read8();

	return 0;
}

inline HTTPHeaderEntry* HTTPCacheEntry::GetHeader(UINT32 index)
{
	OP_ASSERT(index<headers_num+ar_headers_vec.GetCount());
	OP_ASSERT(headers_num==HEADER_DEF_SIZE || ar_headers_vec.GetCount()==0);

	if(index<headers_num)
	{
		OP_ASSERT(headers_num>=index);

		return &ar_headers_def[index];
	}
	OP_ASSERT(headers_num==HEADER_DEF_SIZE);

	return ar_headers_vec.Get(index-headers_num);
}
#endif // CACHE_FAST_INDEX
#endif // CACHE_SIMPLE_STREAM_
