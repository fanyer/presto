/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Johan Herland
**
*/

#ifndef ABSTRACT_MPP_H
#define ABSTRACT_MPP_H

#include "modules/util/simset.h"
#include "modules/formats/hdsplit.h"

class ProtocolComm;
class MultiPartParser_Listener;

// *** Should the following two constants depend on prefsManager->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize) ???
/// Buffer block size. Buffer will be allocated (when needed) in multiples of this size [# bytes].
#define MULTIPART_BUFFER_BLOCK_SIZE 6144
/// Minimum # bytes required to be available at end of buffer in order to NOT reallocate before reading.
#define MULTIPART_BUFFER_MIN_READ_SIZE 1600


/** Abstract Multipart Parser. Base class for multipart parsers. */
class AbstractMultiPartParser {
public:

/* Public types and constants */

	/** AbstractMultiPartParser uses the following state machine internally:
	 *
	 * 1. Start in state STATE_BEFORE_MULTIPART
	 * 2. Process up to start of first part. Transfer to STATE_FINISHED_PART if ok, STATE_FAILURE otherwise.
	 * 3. Process first part's header. Transfer to STATE_INSIDE_PART if ok, STATE_FAILURE otherwise.
	 * 4. Process first part's content. Transfer to STATE_FINISHED_PART when done. STATE_FAILURE otherwise
	 * 5. Repeat step 3 and 4 for additional parts.
	 * 6. Set state to STATE_FINISHED_MULTIPART when done.
	 */
	typedef enum {
		STATE_BEFORE_MULTIPART,   ///< parserOffset points to start of multipart document.
		STATE_FINISHED_PART,      ///< parserOffset points to end of the current part (aka. start of next boundary).
		STATE_INSIDE_PART,        ///< parserOffset points to somewhere inside the content portion of the current part.
		STATE_FINISHED_MULTIPART, ///< parserOffset points to the end of the multipart document, i.e. offset is no longer valid.
		STATE_FAILURE             ///< Parsing has failed. parserOffset is undefined.
	} State;
	char* status;

	/**
	 * Warning enums used by multipart parsers.
	 */
	typedef enum {
		WARNING_NO_BOUNDARY,           ///< No boundary specified for multipart parsing
		WARNING_INVALID_BOUNDARY,      ///< Multipart boundary contains illegal characters or is too long
		WARNING_INVALID_HEADER,        ///< A multipart header was invalid
		WARNING_INCOMPLETE_PART_HEADER,///< A multipart part was terminated before the header was complete
		WARNING_UNFINISHED_PART,       ///< Multipart parsing was terminated without finding the end-boundary of a part
		WARNING_PARSING_ERROR,         ///< Error while parsing, probably invalid input
		WARNING_UNSUPPORTED_CONTENT,   ///< While parsing, content of a type that is currently unsupported was encountered
		WARNING_OUT_OF_MEMORY,         ///< Out of memory occurred during parsing
		WARNING_UNSPECIFIED            ///< An unspecified warning has occurred, this is probably a programming bug
	} Warning;
	
/* Public interface */

	/** Constructor.
	 *
	 * @param listener (optional) This object will notified about any events, e.g. warnings that occur during multipart parsing.
	 */
	AbstractMultiPartParser (MultiPartParser_Listener* listener = 0);

	/// Destructor.
	virtual ~AbstractMultiPartParser ();

	/// Return the current state of this parser object.
	virtual State getState () const { return state; }
	/// Return a string representation of the given state. Only for debugging, not for end-users
	static const char* stateString(State state);

	/// Return the current status of this parser object as a human-readable text string.
	virtual const char *getStatus ();
	/// Return a string representation of the given warning. Only for debugging, not for end-users
	static const char* warningString(Warning warning);
	/// Add raw multipart data to be parsed.
	virtual void load (const char *data, unsigned int dataLength);
	virtual void load (ProtocolComm *comm);

	/// Signal end of data. Do not call load() after calling this function.
	virtual void loadingFinished ();

/* Methods for retrieving part data from the parser. */

	/** Move processing to the next part. The next part becomes the current part.
	 *
	 * This method may only be called BEFORE any of the other retrieval
	 * functions, or AFTER finishCurrentPart() has been called.
	 *
	 * If no next part is available (yet), this method will return FALSE, and
	 * no other retrieval methods may be called. Instead, this method should be
	 * called at a later time. This should be repeated until TRUE is returned,
	 * at which point the other retrieval methods may be called.
	 *
	 * @return TRUE iff there is a next part, and it is available to be
	 *         retrieved (in which case it is from now on known as the current
	 *         part). FALSE means that there is no part currently available to
	 *         be retrieved.
	 */
	virtual BOOL beginNextPart ();

	/** Get the sequence number of the current part, starting at 1.
	 *
	 * This method may ONLY be called AFTER beginNextPart() has returned TRUE,
	 * and BEFORE finishCurrentPart() is called.
	 *
	 * @return The sequence number of the current part, or -1 if called in the
	 *         wrong context.
	 */
	virtual int getCurrentPartNumber () const;

	/** Get the headers associated with the current part.
	 *
	 * The caller assumes responsibility/ownership for the returned HeaderList,
	 * and must delete it when appropriate.
	 *
	 * This method may ONLY be called AFTER beginNextPart() has returned TRUE,
	 * and BEFORE finishCurrentPart() is called.
	 *
	 * This method may only be called ONCE per part. It will return NULL on
	 * subsequent calls in the same part.
	 *
	 * @return The headers associated with the current part, or NULL if called
	 *         in the wrong context.
	 */
	virtual HeaderList *getCurrentPartHeaders ();

	/** Retrieve some data from the current part.
	 *
	 * This method may ONLY be called AFTER beginNextPart() has returned TRUE,
	 * and BEFORE finishCurrentPart() is called.
	 *
	 * @param buf Data will be copied into this buffer.
	 * @param bufLength The maximum # bytes to copy into buf.
	 * @return The actual # bytes copied into buf.
	 */
	virtual unsigned int getCurrentPartData (char *buf, unsigned int bufLength);

	/** Retrieve a short-lived pointer to data in the current part.
	 *
	 * IMPORTANT: THE RETURNED POINTER TO DATA IS ONLY VALID UNTIL THE NEXT
	 *            (NON-CONST) METHOD CALL ON THIS OBJECT.
	 *
	 * This method sets the given pointer to point to the next available chunk
	 * of data from the current part. The number of bytes available to be read
	 * starting at that pointer is returned from this method. Using the pointer
	 * to access any data outside the range [buf, buf + retval) yields undefined
	 * behaviour.
	 *
	 * Note that calling this method will not change the parser's position in
	 * the current part data. Therefore, after calling this method,
	 * consumeCurrentPartData() MUST be called in order to tell this object how
	 * much data has been read. Subsequent calls to this method without calling
	 * consumeCurrentPartData() in between will result in retrieving pointers to
	 * the exact same data, which is probably not desirable.
	 *
	 * This method may ONLY be called AFTER beginNextPart() has returned TRUE,
	 * and BEFORE finishCurrentPart() is called.
	 *
	 * @param buf This pointer will be set to point to the next available chunk
	 *         of data in the current part.
	 * @param noMoreCurrentPartData This pointer (if not NULL) will be set to
	 *         TRUE iff there will be no more current part data AFTER calling
	 *         consumeCurrentPartData() with the number of bytes returned from
	 *         this method.
	 * @return The # bytes available to be read starting at the above pointer.
	 */
	virtual unsigned int getCurrentPartDataPointer (char *& buf, BOOL *noMoreCurrentPartData=NULL) const;

	/** Instruct the parser to discard the given # bytes from the current part.
	 *
	 * This method should be called after getCurrentPartDataPointer() to inform
	 * the parser of how much data that were consumed on that call. Calling this
	 * method will cause the parser to advance across the given # bytes of data
	 * in the current part.
	 *
	 * This method may also be called independently from
	 * getCurrentPartDataPointer() in order to skip/discard the given # bytes
	 * of data from the current part. Note, however, that if one wishes to skip
	 * the rest of the data in the current part, this can be done more
	 * effectively by calling finishCurrentPart().
	 *
	 * This method may ONLY be called AFTER beginNextPart() has returned TRUE,
	 * and BEFORE finishCurrentPart() is called.
	 *
	 * @param bufLength The # bytes to skip/discard from the current part.
	 */
	virtual void consumeCurrentPartData (unsigned int bufLength);

	/** Return # bytes of data currently available to be read from current part.
	 *
	 * The actual # bytes available to be read from getCurrentPartData().
	 *
	 * This method may ONLY be called AFTER beginNextPart() has returned TRUE,
	 * and BEFORE finishCurrentPart() is called.
	 *
	 * @return Number of data bytes available to be read from the current part.
	 */
	virtual unsigned int availableCurrentPartData () const;

	/** Return TRUE iff all data for the current part has been returned.
	 *
	 * TRUE is returned if and only if all part data has been consumed, and
	 * there will never be more data available for the current part.
	 *
	 * A precondition for returning TRUE is availableCurrentPartData() returning
	 * 0, although this is not a sufficient condition.
	 *
	 * This method may ONLY be called AFTER beginNextPart() has returned TRUE,
	 * and BEFORE finishCurrentPart() is called.
	 *
	 * @return TRUE if there is no more data in current part, FALSE otherwise.
	 */
	virtual BOOL noMoreCurrentPartData () const;

	/** Finish the retrieval of this part.
	 *
	 * This method may ONLY be called AFTER beginNextPart() has returned TRUE.
	 */
	virtual void finishCurrentPart ();

	/** Return TRUE iff parser is finished AND all parts have been consumed.
	 *
	 * In order to get the expected result, this method should be called AFTER
	 * finishCurrentPart() has been called, and BEFORE beginNextPart() has
	 * returned TRUE.
	 */
	virtual BOOL noMoreParts () const;

protected:

/* Private subclasses and types */

	/// Encapsulation of a successfully parsed part from a multipart document.
	class Part : public ListElement<Part> {
	public:
		Part (unsigned int n, HeaderList *h, unsigned int dOffset, BOOL dLengthDetermined = FALSE, unsigned int dLength = 0)
			: num(n), headers(h), dataOffset(dOffset), dataLengthDetermined(dLengthDetermined), dataLength(dLength) {}
		~Part () { if (headers) OP_DELETE(headers); }

		unsigned int num;          ///< This part's sequence number in the originating multipart document.
		HeaderList *headers;       ///< The HTTP-style headers associated with this part. If non-NULL at destruction time, this object will be deleted.
		unsigned int dataOffset;   ///< Byte offset relative to multipart document (NOT relative to buffer) where the unconsumed data in this part starts.
		BOOL dataLengthDetermined; ///< TRUE iff dataLength is known, FALSE when dataLength is unknown but >= (parserOffset + bufferOffset) - dataOffset.
		unsigned int dataLength;   ///< # bytes in the data portion of this part which has not yet been consumed.
	};

/* Private methods */

	/** Prepare buffer so that it is ready to receive more data.
	 *
	 * @param minAvailable Minimum # bytes that must be available in buffer when
	 *         this method returns.
	 */
	virtual void prepareBuffer (unsigned int minAvailable = MULTIPART_BUFFER_MIN_READ_SIZE);

	/** After a buffer relocation, all buffer offsets must be adjusted
	 *
	 * @param adjustment Number to subtract from all offsets
	 */
	virtual void adjustOffsets (unsigned int adjustment) {}

	/** Try to parse the next logical chunk of data.
	 *
	 * @return TRUE iff something was successfully parsed and parserOffset was
	 *         updated; FALSE otherwise.
	 */
	virtual BOOL parse () = 0;

	/** Call listener->OnMultiPartParserWarning() if a listener is present.
	 * 
	 * @param reason The reason for the warning
	 * @param offset Byte offset where the warning occurred, or 0 for bufferOffset+parserOffset
	 * @param part Part number where the warning occurred, or 0 for currentPartNum
	 */
	void warn(Warning reason, unsigned int offset=0, unsigned int part=0) const;

/* Member variables */

	State state;                  ///< Current state of this parser object. See AbstractMultiPartParser::State for more info.
	unsigned int currentPartNum;  ///< 0 - before first part, 1 - first part, 2 - second part, etc.

	char *buffer;                 ///< holds raw multipart data. New data is added to the end of buffer.
	unsigned int bufferAllocated; ///< # bytes currently allocated to buffer.
	unsigned int bufferLength;    ///< # bytes currently loaded into buffer.

	unsigned int parserOffset;    ///< # bytes into buffer at which parser is currently located.
	unsigned int returnedOffset;  ///< # bytes into buffer that have already been returned to the caller, and may be deallocated

	unsigned int bufferOffset;    ///< # bytes into multipart document at which buffer starts. This will only be increased when we shift the buffer starting point.


	List<Part> parts;             ///< List of Part objects holding the parts of this documents that have been parsed, but not yet returned.

	BOOL finishedLoading;         ///< Set to TRUE when no more data will be loaded into this object.
	BOOL returningCurrentPart;    ///< Set to TRUE when beginNextPart() returns TRUE. Set to FALSE when finishCurrentPart() is called.

	mutable MultiPartParser_Listener* listener; ///< Callback listener
};

/** Multipart Parser Listener.
 *
 * An object of this class is notified about any events that
 * occur during multipart parsing.
 */
class MultiPartParser_Listener
{
public:
	virtual ~MultiPartParser_Listener() {}

	/** A warning has been detected during parsing.
	 *
	 * @param reason The reason for the warning
	 * @param offset Byte offset (within the contents fed to the parser) where the warning occurred (may be inaccurate)
	 * @param part Part number (starting at 1) where the warning occurred, or 0 if before first part
	 */
	virtual void OnMultiPartParserWarning(AbstractMultiPartParser::Warning reason, unsigned int offset, unsigned int part) = 0;
};

#endif // _MIME_SUPPORT_
