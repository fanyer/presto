/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Luca Venturi
 */
#ifndef _CACHE_MULTIMEDIA_CACHE_
#define _CACHE_MULTIMEDIA_CACHE_

#include "modules/cache/simple_stream.h"
#include "modules/cache/cache_utils.h"




#if defined(CACHE_FAST_INDEX) || CACHE_CONTAINERS_ENTRIES>0
#ifdef MULTIMEDIA_CACHE_SUPPORT

/******************************************************************************
This file is part of the Multimedia Cache, used to improve the video/audio
tag support. The main selling point is the ability to store multiple, out-of-order
parts of a file.
******************************************************************************/

// TODO:
// To test:
// - (done) Segments sefltests (lookup, creation, append...)
// - (done) Segment converage (all the combinations, also byte specific)
// - (done) Write on empty file
// - (done) Append in a segment already in place...
// - (done) Single Write
// - (done) Multiple Writes (adjacent and not, in order and out of order)
// - (done) Size respected
// - (done) Overwrites (error)
// - (done) Sequential Read before closing
// - (done) Random Read before closing
// - (done) Write error after close
// - (done) Read error after close
// - (done) Sequential Read after closing
// - (done) Random Read after closing
// - (done) crash after writing something (NEW flag)
// - (done) Delete need to close and save flags
// - (done) appending after a crash (with NEW flag)
// - (done) error appending after a close
// - (done) Check for first and last segment
// - (done) Read an empty file
// - More than the allowed number of segments
// - Write / Read More than 4GB file (more than one segment, and one over 4GB)...
// - Limit over 4GB
// - Add guards?
//
// Also: test guards and sizes; test flags
// - (NO!)Size revision? (reduced or incresed later)
// - (not real case?) crash after creating a new segment, but without writing (just NEW flag)

/// If true, the offsets are 64 bits
#define MMCACHE_HEADER_64_BITS 1
// TRUE if every segment has a "guard" byte
#define MMCACHE_HEADER_GUARDS 0x80
// Default maximum number of segments
#define MMCACHE_DEFAULT_MAX_SEGMENTS 128

/// The segment is new, and Opera is writing on it. It means that the length is unknown.
/// At most one segment can be in this position.
/// This is a temporary state. When the file is closed, the flag should be updated
/// After reading, the state should also be updated on RAM, and fixed on disk only after a write and a successfull close
#define MMCACHE_SEGMENT_NEW 1
/// The segment is dirty, so a reader cannot use its content. But the length is meaningful, so it can be skipped
#define MMCACHE_SEGMENT_DIRTY 2

/// Size of the fixed part of the header
#define MMCACHE_HEADER_SIZE 16

/// Guard used to check the content of the file
#define MMCACHE_SEGMENTS_GUARD 0x77

class MultimediaSegment;
class MultimediaCacheFileDescriptor;

/// Consume Policy used when streaming is activated; consuming bytes is important to free space for
/// new content
enum ConsumePolicy
{
	// Do not automatically consume bytes. This mean that the cache will simply fill, unless ConsumeBytes() is call
	CONSUME_NONE,
	// Automatically consume bytes when reading (the bytes read, and everything before that, will be consumed).
	// This could somewhat simplify the logic of the module that buffers the video.
	// If empty space recovery is activated, the consumed bytes can still be read, if available.
    // It has some drawbacks and some "black out" on the recovery, which means that it can use a bit more bandwidth
	// than required
	CONSUME_ON_READ,
	// Automatically consume bytes when writing (the same amount of bytes written will be tentatively consumed before writing; they will be consumed on the first segment available)
	// This is the DEFAULT mode. The module using the cache needs to control correctly the buffering, or it can end-up
	// discarding content that the user still has not seen.
	// If used properly, this provide the minimum bandwidth consumption, while still being easier than CONSUME_NONE, as
	// the bytes are consumed automatically
	CONSUME_ON_WRITE,
};

/** Class that manage a multimedia file, used to cache multiple parts of an audio or video file.
	The files are buffered, but in any case reading and writing are supposed to be performed in relatively large chunks.
	
	Keys requirements:
	- Support for multiple, out of order, segments (a segment is a download range). 64 or 128 seems a sensible number. The number of segments could affect performances.
	- Support for a size limit. 
	- A function to get the segments "coverage" of the requested range (so that it will be possible to show the user how many parts are locally available, using the normal video progress bar)
	- If the override is not in place, a crash should have no impact (the last segment will usually be available)
	
	Forward looking:
	- An override (update of the content but also "recycle" after the size limit is reached) could be added later
	- After an override, a crash should usually result in the loss of the last segment updated, not on the wrong content displayed.
	- We can implement a method to have a better protection against crash (so supporting out of order writes on the disk), but it will be a bit inefficient.
	
	Header (16 bytes)
	- OMCF						(UINT32, it stands for Opera Multimedia Cache Format)
	- Version: 1				(UINT8)
	- Flags						(UINT8)  [MMCACHE_HEADER_64_BITS: 64 bits offsets]
	- Max file size				(UINT64) [0 means no maximum size; header size is not part of this file size]
	- Max number of segments	(UINT16)
	*	For each segment:
		- Start offset (in the media content)	UINT32/UINT64 (based on MMCACHE_HEADER_64_BITS flag)
		- Length								UINT32/UINT64 (based on MMCACHE_HEADER_64_BITS flag)
		- segment flags:						UINT8 (MMCACHE_SEGMENT_NEW / MMCACHE_SEGMENT_DIRTY)


	**** STREAMING ****
	To satisfy the requirements of some platforms, streaming support has been added to the Multimedia Cache.
	The streaming is activated by the variable "streaming", and the intended usage is activating it before starting to write and then never disabling it.
	While in theory it is possible to change the logic to support streaming with multiple segments, things get pretty complicated
	with not so clear advantages, so it's not addressed by the current implementation (nor it is supposed to be implemented in the future).
	The streaming is also intended to be used with a size limit, which requires the user (media module) to be aware of some internal workings of this class.
	The streaming can also be performed in RAM, and it's achieved by SimpleFileReadWrite using an OpFileDescriptor that works in memory (unfortunately, OpMemFile could not be used).

	The streaming is performed in this way:
	- The concept of empty space is introduced, and it allows a segment to use more bytes than it needs to store its content. The empty space is at the end of the segment.
	- When some content is "consumed", it becomes empty space. Content is consumed from the beginning of the segment
	- At most two segments are used: the Master and the Reserve
	- While the size and of the segments change, the sum of their size is constant
	- Streaming start with one segment, the master, which occupy all the space available (max_size), to store empty space
	- As a side effect, no other segments are allowed, apart the reserve (more on this later).
	- Read is intended to be performed from the beginning of a segment, and if CONSUME_ON_READ is chosen it will "consumes" all the bytes requested.
	(in this case, a read in the middle, will simply consume also all the bytes before the intended reading point)
	- If CONSUME_ON_WRITE is selected, a "consume" operation (from the beginning of the first segment) is performed before writing
	- When a consume operation is performed, a reserve segment is created (if not already present) and it will get the consumed bytes as empty space
	- Based on the position, a write can happen both on the master or on the reserve
	- When the master has been completely consumed, all the space of the Reserve is assigned back to the Master
	- Consume should never happen in the reserve, as this means that the master has been completely read (situations that promote the Reserve to Master)
	- As the reserve segment is at the beginning of the Master segment, no disk operations are required to move bytes
		on the file, but only to store new content
	- NOTE: it is strongly recommended that these URLs are created unique, and that they are deleted after use, as the content is not kept coherent to avoid disk operations

	An example:
	* NOTE: 'e' ==> Empty byte, 'X' ==> 1 byte of content
	- A streaming file is created, that can contains up to 8 bytes. 1 Master segment is created, pre allocating all the space:
			[eeeeeeee]  Master: 8 bytes empty

	- 8 Bytes are written:
			[XXXXXXXX]  Master: 8 bytes full

	- 2 bytes are read: an empty reserve segment of 2 bytes is created. Please note that the bytes on disk are not moved,
		they have been simply assigned to the reserve.
			[ee]        Reserve: 2 bytes empty
			  [XXXXXX]  Master:  6 bytes full

	- 4 bytes read: 4 bytes are consumed and assigned to the reserve, as empty bytes
			[eeeeee]    Reserve: 6 bytes empty
			      [XX]  Master:  2 bytes full

	- 2 bytes written: as the Master is full, these bytes are added to the Reserve
			[XXeeee]    Reserve: 2 bytes of content, 4 empty bytes
			      [XX]  Master:  2 bytes full

	- 2 bytes read (from the master)
			[XXeeeeee]  Reserve: 2 bytes of content, 6 empty bytes
			        []  Master:  empty

	- as the Master is empty, an automatic reassign happen, effectively swap Master and Reserve
			[] 			Reserve: empty
			[XXeeeeee]  Master:  2 bytes of content, 6 empty bytes


	* During these operations, it is very important that the beginning (content_start) of the Reserve coincide with the end of the Master,
		 as this is what allows reuse of the Reserve instead of creating a new segment; note that physically on the disk, the Reserve is before the Master,
		 but logically the content is after it. This basically creates a circular buffer.
	* At the beginning, the Master is also preallocated to use all the size available, to avoid problems if the reserve is created when there is still space available
	* Please note that if the streaming is activated, basically the cache is no longer able to store multiple segments (even if internally it does so),
		and some bandwidth waste has to be expected when the user perform a seek operation.


	Implementation notes:
	- Segments cannot shrink, their size cannot decrease overtime
	- The number of segments cannot decrease
	- For now, all the segments are loaded and created, even if empty
*/
class MultimediaCacheFile
{
friend class MultimediaCacheFileDescriptor;
protected:
	/// Object used to access the physical file
	SimpleFileReadWrite sfrw;
	/// Flags of the header
	UINT8 header_flags;
	/// Maximum size allowed for the file
	OpFileLength max_size;
	/// Maximum number of segments present in the file
	UINT16 max_segments;
	/// Vector of segments in use
	OpAutoVector<MultimediaSegment> segments;
	/// Size of the content present in the file
	OpFileLength cached_size;
	// TRUE if streaming is on, which means that reading some content delete it from the file...
	// This can reorganize the segments.
	// Please, for more informations, read the section about the streaming, before the class definition.
	BOOL streaming;
	// TRUE if the file is in RAM
	BOOL ram;
	/// TRUE if during streaming the DeleteContent() function is called automatically when required.
	// Please read the documentation of SetAutoDeleteContentDuringStream().
	BOOL auto_delete_on_streaming;
	// TRUE to enable a slightly stronger check for streaming (some tests purposedly bypass it to test some "unsupported" situations)
	BOOL deep_streaming_check;
	/// TRUE if it possible, in streaming, to retrieve data from the empty space
	BOOL enable_empty_space_recover;
	/// Policy used to call "ConsumeBytes()" when streaming is on
	ConsumePolicy consume_policy;

private:
	/// Debug function used to check if the state of the variables make sense
	#ifdef DEBUG_ENABLE_OPASSERT
		void CheckInvariants();
	#else
		void CheckInvariants() { }
	#endif
	/// Return the number of segment really used
	UINT16 GetSegmentsInUse() const { return segments.GetCount(); }
	/// Return the segment that will host the content for write; it can create a new segment if required
	OP_STATUS GetWriteSegmentByContentPosition(MultimediaSegment *&segment, OpFileLength content_position, OpFileLength &file_pos, BOOL update_disk);
	/// Return the size of a segment header in bytes
	UINT16 GetSegmentHeaderLength() { return (header_flags & MMCACHE_HEADER_64_BITS) ? 17 : 9; }
	/// Return the position of the request segment
	UINT32 GetSegmentPos(UINT16 segment_index) { OP_ASSERT(segment_index<=max_segments); return MMCACHE_HEADER_SIZE + segment_index*GetSegmentHeaderLength(); }
	
	/// Load all the segment informations from the file. If the file position is not right, set repostion to TRUE).
	OP_STATUS LoadAllSegments(BOOL repostion);
	/// Write all the segment informations from the file. If the file position is not right, set repostion to TRUE).
	OP_STATUS WriteAllSegments(BOOL repostion, BOOL update_header);
	/// Write the first part of the header: sign, version and flags
	OP_STATUS WriteInitialHeader();
	
	/** Write the specified content
		@param file_position Position in the file
		@param buf buffer
		@param size Size of the buffer (bytes to write)
		@param usable_empty_space Empty space usable (so if !streaming, it must be 0)
		@param written_bytes bytes really written (out)
		@param last_segment TRUE if ti is the last segment (so it can expand)
	*/
	OP_STATUS WriteContentDirect(OpFileLength file_position, const void *buf, UINT32 size, OpFileLength usable_empty_space, UINT32 &written_bytes, BOOL last_segment);

	/** Function internally usedby WriteContent() to try a double write for streams
		@param buf Buffer with the data to write
		@param content_position Position in the content (where the video should be in the complete file)
		@param size Size of the buffer
		@param written_bytes Bytes effectively written
	*/
	OP_STATUS WriteContentKernel(OpFileLength content_position, const void *buf, UINT32 size, UINT32 &written_bytes);
	
	/**
	  Second phase constructor
	  @param path Path of the file
	  @param fodler Folder
	  @param suggested_flags Suggested initial flags. If the file already exists, then these flags will be dropped (disk wins)
	  @param suggested_segments Suggested maximum number of segments to store in the file. If the file already exists, then this value will be dropped  (disk wins)
	*/
	OP_STATUS ConstructPrivate(UINT8 suggested_flags, OpFileLength suggested_max_file_size=0, UINT16 suggested_max_segments=MMCACHE_DEFAULT_MAX_SEGMENTS);
	
	/// Returns the last segment
	MultimediaSegment *GetLastSegment() { return segments.Get(segments.GetCount()-1); }

	/// Consumes the bytes, meaning that they have been streamed. This can trigger a reorganization of the segment
	OP_STATUS ConsumeBytes(int seg_index, OpFileLength content_position, UINT32 bytes_to_consume);
	/// Automatically tries consume from the first segment (==the one with lower content_start) the bytes requested
    /// The space can be consumed from multiple segments, and it requires streaming to be on
	OP_STATUS AutoConsume(UINT32 bytes_to_consume, UINT32 &bytes_consumed);
	/// Find the first segment that can be used by AutoConsume()
	// @return the index of the segment; -1 means no segments suitable
	int FindSegmentForAutoConsume();

	/** Returns TRUE if the empty space of some segments contains the requested starting bytes;
		@param start Starting byte of the content (if TRUE, this byte is available by definition)
		@param bytes_available if the method returns TRUE, bytes_available will be set to the number of bytes that can be provided
		@param file_pos if the method returns TRUE, file_pos will be the physical address of the starting byte
	*/
	OP_STATUS RetrieveFromEmptySpace(OpFileLength start, OpFileLength &bytes_available, OpFileLength &file_pos);
	
public:
#ifdef SELFTEST
	/// For debug purposes, return the number of segment
	UINT16 DebugGetSegmentsCount() { return GetSegmentsInUse(); }
	/// Disables some check, as required by a couple of selftests
	void DebugDisableDeepStreamCheck() { deep_streaming_check=FALSE; } 
#endif

	/** Default constructor */
	MultimediaCacheFile(): header_flags(0), max_size(0), max_segments(0), cached_size(0), streaming(FALSE), ram(FALSE), auto_delete_on_streaming(TRUE), deep_streaming_check(TRUE), enable_empty_space_recover(TRUE), consume_policy(CONSUME_NONE) {}
	/** Close the file */
	~MultimediaCacheFile() { CloseAll(); }
	
	/**
	  Second phase constructor, using a file
	  @param path Path of the file
	  @param fodler Folder
	  @param suggested_flags Suggested initial flags. If the file already exists, then these flags will be dropped (disk wins)
	  @param suggested_segments Suggested maximum number of segments to store in the file. If the file already exists, then this value will be dropped  (disk wins)
	*/
	OP_STATUS ConstructFile(const uni_char* path, OpFileFolder folder, OpFileLength suggested_max_file_size=0, UINT16 suggested_max_segments=MMCACHE_DEFAULT_MAX_SEGMENTS);

	/**
	  Second phase constructor, using a memory file
	  @param path Path of the file
	  @param fodler Folder
	  @param suggested_flags Suggested initial flags. If the file already exists, then these flags will be dropped (disk wins)
	  @param suggested_segments Suggested maximum number of segments to store in the file. If the file already exists, then this value will be dropped  (disk wins)
	*/
	OP_STATUS ConstructMemory(OpFileLength suggested_max_file_size=0, UINT16 suggested_max_segments=MMCACHE_DEFAULT_MAX_SEGMENTS);

	/// Set if the content can be retrieved from the empty space. Usually there are no real reasons to disable it
	void SetEnableEmptySpaceRecover(BOOL b) { enable_empty_space_recover=b; }
	
	/// Close the file and free the segments
	OP_STATUS CloseAll();

	/// Set if, during the streaming, DeleteContent() must be automatically called; this will happen after a seek
	// during a write.
	// The problem is that a "write seek" (intended as a tentative to write in a position different than the end of the stream) would normally create a
	// new segment, which is impossible while streaming (as all the space is pre-allocated).
	// So if this situation is detected, and auto_delete_on_streaming is TRUE, all the segments are discarded to make space for the new content
	void SetAutoDeleteContentDuringStream(BOOL b) { auto_delete_on_streaming=b; }

	/**
		This function will create an empty Multimedia cache file, dropping the current content and all the segments.
		It is intended to be used while streaming, when the user execute a seek (for writing).
		Of course it must be used with care...
	*/
	OP_STATUS DeleteContent();

	// Activate or disable the streaming.
	//
	// When streaming is activated, depending on consume_policy, the behaviour will be different.
	// Eventually bytes need to be "consumed" to free space for new bytes.
	// Check ConsumePolicy for more information,
	//
	// Consuming bytes can reorganize the segments, so things are a bit complicated and performances can suffer.
	// Please note that for performances, the segments are marked as dirty, so they will be discarded after a crash.
	// Also note that enabling the streaming, split a "streamed segment" in two parts: Master and Reserve.
	// The real size of the segments will not be updated during runtime, but the aggregated size will remain the
	// same. As the segments will be marked as dirty, they will be discarded in any case, so it should work even after a crash
	// In any case, the streaming should be activated only on unique URLs, as the content of the file is not kept coherent
	// (to reduce the disk activity).
	// For more information, please read the "streaming" section before the class definition
	void ActivateStreaming(ConsumePolicy policy) { CheckInvariants(); streaming=TRUE; consume_policy=policy; CheckInvariants(); }

	/// Disable Streaming
	void DisableStreaming() { CheckInvariants(); streaming=FALSE; consume_policy=CONSUME_NONE; CheckInvariants(); }
	
	
	/// Returns TRUE if the streaming is activated
	BOOL IsStreaming() { return streaming; }
	/// Returns TRUE if the content is in RAM
	BOOL IsInRAM() { return ram; }

#ifdef SELFTEST
	/// Simulate a crash (the object should not be used after this call)
	OP_STATUS DebugSimulateCrash() { return sfrw.Close(); }
	/// Return the number of segments that are marked as new
	int DebugGetNewSegments();
	/// Return the number of dirty segments
	int DebugGetDirtySegments();
#endif
	
	/** Writing is a relatively expensive operation, as the segment could be updated. Also a lookup for the segment could be required.
		These files are tought for multimedia, so writing is expected to be done in relatively big blocks (let's say at least 512 bytes, but the more the better).
		@param buf Buffer with the data to write
		@param content_position Position in the content (where the video should be in the complete file)
		@param size Size of the buffer
		@param written_bytes Bytes effectively written
	*/
	OP_STATUS WriteContent(OpFileLength content_position, const void *buf, UINT32 size, UINT32 &written_bytes);
	
	/** Reading is a relatively expensive operation, as the file pointer could be moved. Also a lookup for the segment is required.
		These files are tought for multimedia, so reading is expected to be done in relatively big blocks (let's say at least 512 bytes, but the more the better).
		WARNING: ERR_NO_SUCH_RESOURCE means that the requested content is not cached
		@param buf Buffer where the data will be placed
		@param content_position Position in the content (where the video should be in the complete file)
		@param size Size of the buffer
		@param read_bytes Bytes effectively read
	*/
	OP_STATUS ReadContent(OpFileLength content_position, void *buf, UINT32 size, UINT32 &read_bytes);
	
	/// Fill the vector with the coverage of the requested part of the file; the segments are unordered and they can be deleted, as they are
	/// objects not used by MultimediaCacheFile
	OP_STATUS GetUnsortedCoverage(OpAutoVector<MultimediaSegment> &out_segments, OpFileLength start=0, OpFileLength len=INT_MAX);
	/// Fill the vector with the coverage of the requested part of the file; the segments are sorted and they can be deleted, as they are
	/// objects not used by MultimediaCacheFile;
	/// segments can be "merged" (0->100 = 100->300  ==>  0==>300)
	OP_STATUS GetSortedCoverage(OpAutoVector<MultimediaSegment> &segments, OpFileLength start=0, OpFileLength len=INT_MAX, BOOL merge=TRUE);
	
	/**
		If the requested byte is in the segment, this method says how many bytes are available, else it returns
		the number of bytes NOT available (and that needs to be downloaded before reaching a segment).
		This is bad behaviour has been chosen to avoid traversing the segments chain two times.
		
		So:
		- available==TRUE  ==> length is the number of bytes available on the current segment
		- available==FALSE ==> length is the number of bytes to skip or download before something is available.
		                       if no other segments are available, length will be 0 (this is because the Multimedia
		                       cache does not know the length of the file)
		                       
		@param position Start of the range requested
		@param available TRUE if the bytes are available, FALSE if they need to be skipped
		@param length bytes available / to skip
		@param multiple_segments TRUE if it is ok to merge different segments to get a bigger coverage (this operation is relatively slow)

	*/
	void GetPartialCoverage(OpFileLength position, BOOL &available, OpFileLength &length, BOOL multiple_segments);
	/**
		Fill a vector with the list of segments that needs to be downloaded to fully cover the stream in the range requested
		@param start First byte of the range to cover
		@param len Length of the range (-1 means till the end of the segments cached)
		@param missing Vector that will be populated with the ranges requested.
	*/
	OP_STATUS GetMissingCoverage(OpAutoVector<StorageSegment> &missing, OpFileLength start=0, OpFileLength len=FILE_LENGTH_NONE);

	/// Return the maximum size (0 if without limit)
	OpFileLength GetMaxSize() { return max_size; }
	/// Increase the maximum size of the file; negative numbers are not allowed! This because the file will not be truncated
	void IncreaseMaxSize(OpFileLength increase) { OP_ASSERT(max_size>0 && increase>0);  if(max_size>0 && increase>0) max_size+=increase; }
	
	/// Return the full range (usually only partially covered) available
	/// If no segments are available, start and end will be 0
	void GetOptimisticFullRange(OpFileLength &start, OpFileLength &end) const;

	/// Return how much space is available, included the empty space.
	/// If there are no limits, approximately 2GB minus the current size is returned
	OpFileLength GetAvailableSpace();
	
#ifdef SELFTEST
	/// Add a "custom made" segment, for debugging purposes. It will probably unrelated to the disk
	OP_STATUS DebugAddSegment(MultimediaSegment *seg);
	/// Return the length of the segment
	OpFileLength DebugGetSegmentLength(int index);
	/// Return the empty space of the segment
	OpFileLength DebugGetSegmentEmptySpace(int index);
	/// Return the content start of the segment
	OpFileLength DebugGetSegmentContentStart(int index);
#endif

	/// Flush the buffer, so the file is fully saved
	OP_STATUS FlushBuffer() { return sfrw.FlushWriteBuffer(); }
	/// Return an File Descriptor
	MultimediaCacheFileDescriptor *CreateFileDescriptor(int mode);
	/// Return the size of all the header
	UINT32 GetFullHeaderLength() { return GetSegmentPos(max_segments); }
};

/// This class represent a segment in a Multimedia Cache File. A segment is just a part of the file
class MultimediaSegment
{
private:
	friend class MultimediaCacheFile;
	
	/// Starting offset of the segment in the file
	OpFileLength file_offset;
	/// Starting byte of the segment (int the content, not in the file)
	OpFileLength content_start;
	/// Length of the segment (of course content and plysical length are the same)
	OpFileLength content_len;
	/// Number of empty bytes at the end of the segment
	OpFileLength empty_space;
	/// "Reserve Segment" that hosts the bytes consumed
	MultimediaSegment *reserve_segment;
	/// Flags related to the segment
	UINT8 flags;
	/// TRUE if the segment has to be discarded
	BOOL to_be_discarded;
	
	/// Debug function used to check if the state of the variables make sense
	#ifdef DEBUG_ENABLE_OPASSERT
		void CheckInvariants();
	#else
		void CheckInvariants() { }
	#endif
	
	/// Write a segment header
	static OP_STATUS DirectWriteHeader(SimpleFileReadWrite &sfrw, UINT8 header_flags, OpFileLength content_start, OpFileLength content_len, UINT8 segment_flags);
	/// Update the segment header on the disk
	OP_STATUS UpdateDisk(UINT8 header_flags, SimpleFileReadWrite &sfrw, UINT32 header_pos);
	/// Consume the specified bytes (for streaming); this method assume that the reserve segment has already been created
	OP_STATUS ConsumeBytes(OpFileLength content_position, UINT32 bytes_to_consume, OpFileLength &dumped_bytes);
	/// Increment the size of the segment, computing empty space
	void AddContent(OpFileLength size, OpFileLength &extension_bytes);
	/// Returns TRUE if the empty space of the reserve segment contains the requested byte
	BOOL EmptySpaceContains(OpFileLength content_position);
	
public:
	/// Default constructor
	MultimediaSegment(): file_offset(MMCACHE_HEADER_SIZE), content_start(0), content_len(0), empty_space(0), reserve_segment(NULL), flags(0), to_be_discarded(FALSE)  { }
	MultimediaSegment(OpFileLength file_pos, OpFileLength cnt_start, OpFileLength cnt_len, UINT8 flag=0): file_offset(file_pos), content_start(cnt_start), content_len(cnt_len), empty_space(0), reserve_segment(NULL), flags(flag), to_be_discarded(FALSE) {}
	
	/**
		Set the data of the segment.
		@param file_start Starting byte in the file (physical position)
		@param segment_start Starting byte in the content (content/logical position)
		@param segment_len Length (valid for both physical and logical)
		@
		param segment_flags Flags
	*/
	OP_STATUS SetSegment(OpFileLength file_start, OpFileLength segment_start, OpFileLength segment_len, UINT8 segment_flags);
	/// Set the new length of the segment. Be careful...
	void SetLength(OpFileLength len) { content_len=len; }
	
	/// Get the physical offset in the file
	OpFileLength GetFileOffset() { return file_offset; }
	/// Get the start of the content (it means where this segment is located in the content); byte included
	OpFileLength GetContentStart() const { return content_start; }
	/// Get the length of the content hosted in the segment
	OpFileLength GetContentLength() const { return content_len; }
	/// Get the end of the content; byte excluded
	OpFileLength GetContentEnd() const { return content_start+content_len; }
	/** Return TRUE if the segment contains the requested starting bytes;
		@param start Starting byte of the content (if TRUE, this byte is available by definition)
		@param bytes_available if the method returns TRUE, bytes_available will be set to the number of bytes that can be provided
		@param file_pos if the method returns TRUE, file_pos will be the physical address of the starting byte
	*/
	BOOL ContainsContentBeginning(OpFileLength start, OpFileLength &bytes_available, OpFileLength &file_pos);
	/** Return TRUE if the segment contains a part of the requested bytes (not necessarily from the beginning)
		@param requested_start Starting byte of the requested content
		@param requested_len Length of the requested content
		@param available_start if the method returns TRUE, available_start will be set to the first byte that can be provided
		@param available_len if the method returns TRUE, available_len will be set to the number of bytes that can be provided (starting from available_start)
		@param file_pos if the method returns TRUE, file_pos will be the physical address of the starting byte (available_start)
	*/
	BOOL ContainsPartialContent(OpFileLength requested_start, OpFileLength requested_len, OpFileLength &available_start, OpFileLength &available_len, OpFileLength &file_pos);
	/// Return TRUE if the content can be appended to the segment; in this case, file_pos will be the physical
	// address where to append.
	// WARNING: It MUST be the last segment!!!
	BOOL CanAppendContent(OpFileLength start, OpFileLength &file_pos);
	/// Return the current flags
	UINT8 GetFlags() { return flags; }
	/// Return TRUE if the segment is dirty
	BOOL IsDirty() { return (flags & MMCACHE_SEGMENT_DIRTY) ? TRUE:FALSE; }
	/// TRUE if the segment has to be discarded
	BOOL HasToBeDiscarded() { return to_be_discarded; }
	/// Set if the segment is dirty. Attention that this is not automatically reflected to the storage
	void SetDirty(BOOL b) { if(b) flags|=MMCACHE_SEGMENT_DIRTY; else flags=flags & (~MMCACHE_SEGMENT_DIRTY); }
	/// Return TRUE if the segment is new
	BOOL IsNew() { return (flags & MMCACHE_SEGMENT_NEW) ? TRUE:FALSE; }
	/// Set if the segment is new. Attention that this is not automatically reflected to the storage
	void SetNew(BOOL b) { if(b) flags|=MMCACHE_SEGMENT_NEW; else flags=flags & (~MMCACHE_SEGMENT_NEW); }

};


/**
	Class that exposes the content of the multimedia file (so no headers & Co.).
	It can basically be used as an OpFile just on the content. Of course there are some problem because the file
	can have just part of the content.
	This class is specifically thought for the integration with the cache and URL, so it depends on the expected behaviour
	of these two modules
*/
class MultimediaCacheFileDescriptor: public OpFileDescriptor
{
private:
	// Read position
	OpFileLength read_pos;
	// Write position
	OpFileLength write_pos;
	// Multimedia file
	MultimediaCacheFile *mcf;
	// TRUE if it is in read only mode
	BOOL read_only;
	
	
public:
	/// Constructor
	MultimediaCacheFileDescriptor(MultimediaCacheFile *mm_cache_file): read_pos(0), write_pos(0), mcf(mm_cache_file), read_only(FALSE) { OP_ASSERT(mcf); } 
	
	/// Set the read position; this operation alone is fast, but later the file pointer could be moved
	void SetReadPosition(OpFileLength pos);
	/// Set the write position; this operation alone is fast, but later the file pointer could be moved
	void SetWritePosition(OpFileLength pos);
	/// Return the read position
	OpFileLength GetReadPosition() const { return read_pos; }
	/// Return the write position
	OpFileLength GetWritePosition() const { return write_pos; }
	
	virtual OpFileType Type() const { return OPFILE; } 

	virtual BOOL IsWritable() const { return !read_only; }
	virtual OP_STATUS Open(int mode) { if(!mcf) return OpStatus::ERR_NULL_POINTER; OP_ASSERT(mode==(OPFILE_APPEND| OPFILE_READ) || mode==(OPFILE_READ)); read_only=TRUE; return OpStatus::OK; }
	virtual BOOL IsOpen() const { return mcf && mcf->sfrw.IsFileOpened(); }
	// TODO: verify if doing nothing is really fine
	virtual OP_STATUS Close() { return OpStatus::OK; }
	virtual BOOL Eof() const;
	virtual OP_STATUS Exists(BOOL& exists) const { if(!mcf) return OpStatus::ERR_NULL_POINTER; return mcf->sfrw.Exists(exists); }
	// Get the file length, after removing the size of the header
	virtual OP_STATUS GetFileLength(OpFileLength& len) const;
	virtual OP_STATUS Write(const void* data, OpFileLength len);
	virtual OP_STATUS Read(void* data, OpFileLength len, OpFileLength* bytes_read=NULL);
	
	// The assumption here is that the position is changed for READ operations (write are in append or explicitely set). Also only SEEK_FROM_START is currently supported
	virtual OP_STATUS GetFilePos(OpFileLength& pos)  const { pos=GetReadPosition(); return OpStatus::OK; }
	virtual OP_STATUS SetFilePos(OpFileLength pos, OpSeekMode seek_mode=SEEK_FROM_START)  { OP_ASSERT(seek_mode==SEEK_FROM_START); if(seek_mode!=SEEK_FROM_START) return OpStatus::ERR_NOT_SUPPORTED; SetReadPosition(pos); return OpStatus::OK; }
	
	/// Operations not implemented because not really required
	virtual OP_STATUS ReadLine(OpString8& str) { OP_ASSERT(FALSE && "Operation not supported!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OpFileDescriptor* CreateCopy() const { OP_ASSERT(FALSE && "Operation not supported!"); return NULL; }
	
	/// Set the file in read only or R/W mode
	void SetReadOnly(BOOL read_only_mode) { read_only=read_only_mode; }
};
#endif // MULTIMEDIA_CACHE_SUPPORT
#endif // defined(CACHE_FAST_INDEX) || CACHE_CONTAINERS_ENTRIES>0
#endif // _CACHE_MULTIMEDIA_CACHE_
