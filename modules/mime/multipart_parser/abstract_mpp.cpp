/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Johan Herland
 */
#include "core/pch.h"

#include "modules/mime/multipart_parser/abstract_mpp.h" // AbstractMultiPartParser

#include "modules/url/protocols/pcomm.h" // ProtocolComm

#define AMP_STATUS_STRING_MAX_LENGTH 1024


AbstractMultiPartParser::AbstractMultiPartParser (MultiPartParser_Listener* l)
	: status(NULL)
	, state(STATE_BEFORE_MULTIPART)
	, currentPartNum(0)
	, buffer(0)
	, bufferAllocated(0)
	, bufferLength(0)
	, parserOffset(0)
	, returnedOffset(0)
	, bufferOffset(0)
	, parts()
	, finishedLoading(FALSE)
	, returningCurrentPart(FALSE)
	, listener(l)
{
}

AbstractMultiPartParser::~AbstractMultiPartParser ()
{
	parts.Clear();
	if (buffer) OP_DELETEA(buffer);
	buffer = 0;
	OP_DELETEA(status);
}

const char *AbstractMultiPartParser::getStatus ()
{
	if (!status) {
		status = OP_NEWA(char, AMP_STATUS_STRING_MAX_LENGTH);
		if (!status)
			return ""; // No way to return status (this is just debug code anyway)
	}
	op_snprintf(status, AMP_STATUS_STRING_MAX_LENGTH, "[%s, Part# %2u, Buffer: Alloc %5u, Len %5u, Par %5u, Ret %5u, Off %5u]", stateString(state), currentPartNum, bufferAllocated, bufferLength, parserOffset, returnedOffset, bufferOffset);
	return status;
}

const char* AbstractMultiPartParser::stateString (State state)
{
	switch (state) {
		case STATE_BEFORE_MULTIPART:   return "  BEFORE_MULTIPART";
		case STATE_FINISHED_PART:      return "     FINISHED_PART";
		case STATE_INSIDE_PART:        return "       INSIDE_PART";
		case STATE_FINISHED_MULTIPART: return "FINISHED_MULTIPART";
		case STATE_FAILURE:            return " **** FAILURE ****";
	}
	return "UNKNOWN";
}

const char* AbstractMultiPartParser::warningString (Warning warning)
{
	switch (warning) {
		case WARNING_NO_BOUNDARY:            return "NO_BOUNDARY";
		case WARNING_INVALID_BOUNDARY:       return "INVALID_BOUNDARY";
		case WARNING_INVALID_HEADER:         return "INVALID_HEADER";
		case WARNING_INCOMPLETE_PART_HEADER: return "INCOMPLETE_PART_HEADER";
		case WARNING_UNFINISHED_PART:        return "UNFINISHED_PART";
		case WARNING_PARSING_ERROR:          return "PARSING_ERROR";
		case WARNING_UNSUPPORTED_CONTENT:    return "UNSUPPORTED_CONTENT";
		case WARNING_OUT_OF_MEMORY:          return "OUT_OF_MEMORY";
		case WARNING_UNSPECIFIED:            return "UNSPECIFIED";
	}
	return "UNKNOWN";
}

void AbstractMultiPartParser::load (const char *data, unsigned int dataLength)
{
	if (!data || dataLength == 0 || finishedLoading || state == STATE_FAILURE || state == STATE_FINISHED_MULTIPART)
		return;

	prepareBuffer(dataLength);
	dataLength = MIN(dataLength, bufferAllocated - bufferLength);
	op_memcpy(buffer+bufferLength, data, dataLength);
	bufferLength += dataLength;
	while (parse()) // Parse as much of the incoming data as possible
		/* skip */ ;
}

void AbstractMultiPartParser::load (ProtocolComm *comm)
{
	unsigned int bytesRead = 0; // # bytes read on last call to comm->ReadData()
	unsigned int totalBytesRead = 0; // Total # bytes read from comm->ReadData()

	if (!comm || finishedLoading)
		return;

	if (state == STATE_FAILURE || state == STATE_FINISHED_MULTIPART) {
		const unsigned int tempBufferLength = 1024;
		char tempBuffer[tempBufferLength]; /* ARRAY OK 2009-04-23 roarl */
		// Read as much data from comm as possible
		do {
			bytesRead = comm->ReadData(tempBuffer, tempBufferLength);
			totalBytesRead += bytesRead;
		} while (bytesRead == tempBufferLength); // Keep going as long as comm fills tempBuffer completely.
		return;
	}

	// Read as much data from comm as possible
	do {
		prepareBuffer();
		bytesRead = comm->ReadData(buffer+bufferLength, bufferAllocated - bufferLength);
		bufferLength += bytesRead;
		totalBytesRead += bytesRead;
		while (parse()) // Parse as much of the incoming data as possible
			/* skip */ ;
	} while (bufferLength == bufferAllocated // Repeat as long as comm fills buffer completely (i.e. there is probably more available data in comm)...
	     && !(state == STATE_FAILURE || state == STATE_FINISHED_MULTIPART)); // ... and parser state indicates that more data is welcome.
}

void AbstractMultiPartParser::loadingFinished ()
{
	finishedLoading = TRUE;
	while (parse())
		/* skip */ ;
	OP_ASSERT(state == STATE_FINISHED_MULTIPART || state == STATE_FAILURE);
}

BOOL AbstractMultiPartParser::beginNextPart ()
{
	// Preconditions
	if (returningCurrentPart) {
		warn(WARNING_UNSPECIFIED);
		return FALSE;
	}

	if (parts.Empty()) return FALSE; // No parts are ready to be returned.
	returningCurrentPart = TRUE;
	return TRUE;
}

int AbstractMultiPartParser::getCurrentPartNumber () const
{
	// Preconditions
	if (!returningCurrentPart || parts.Empty()) {
		warn(WARNING_UNSPECIFIED);
		return -1;
	}

	Part *p = parts.First();
	return p->num;
}

HeaderList *AbstractMultiPartParser::getCurrentPartHeaders ()
{
	// Preconditions
	if (!returningCurrentPart || parts.Empty()) {
		warn(WARNING_UNSPECIFIED);
		return 0;
	}

	Part *p = parts.First();
	HeaderList *retval = p->headers;
	p->headers = 0;
	return retval;
}

unsigned int AbstractMultiPartParser::getCurrentPartData (char *buf, unsigned int bufLength)
{
	// Preconditions
	if (!returningCurrentPart || parts.Empty()) {
		warn(WARNING_UNSPECIFIED);
		return 0;
	}

	Part *p = parts.First();                                // Retrieve current part.
	OP_ASSERT(p->dataOffset >= bufferOffset);               // Start of data must be within current buffer.
	unsigned int dOffset = p->dataOffset - bufferOffset;    // dOffset is p->dataOffset relative to current buffer.
	OP_ASSERT(returnedOffset == dOffset);                   // Must have already returned all data preceding dOffset.
	OP_ASSERT(parserOffset >= dOffset);                     // Cannot have parsed less than dOffset.
	bufLength = MIN(bufLength, availableCurrentPartData()); // Cannot return more data than is available.

	if (bufLength == 0) return 0; // Nothing to copy.
	op_memcpy(buf, buffer + dOffset, bufLength);
	p->dataOffset += bufLength;                               // Move dataOffset forwards by the amount of data returned.
	if (p->dataLengthDetermined) p->dataLength -= bufLength;  // Decrease dataLength by the amount of data returned.
	returnedOffset += bufLength;                              // Move returnedOffset forwards by the amount of data returned.
	return bufLength;
}

unsigned int AbstractMultiPartParser::getCurrentPartDataPointer (char *& buf, BOOL *noMoreCurrentPartData) const
{
	if (noMoreCurrentPartData != NULL) {
		*noMoreCurrentPartData = TRUE; // In case of premature return
	}

	// Preconditions
	if (!returningCurrentPart || parts.Empty()) {
		warn(WARNING_UNSPECIFIED);
		buf = 0;
		return 0;
	}

	Part *p = parts.First();                             // Retrieve current part.
	OP_ASSERT(p->dataOffset >= bufferOffset);            // Start of data must be within current buffer.
	unsigned int dOffset = p->dataOffset - bufferOffset; // dOffset is p->dataOffset relative to current buffer.
	OP_ASSERT(returnedOffset == dOffset);                // Must have already returned all data preceding dOffset.
	OP_ASSERT(parserOffset >= dOffset);                  // Cannot have parsed less than dOffset.
	unsigned int bufLength = availableCurrentPartData(); // Cannot return more data than is available.

	buf = buffer + dOffset; // Set buf pointer to point to start of current part data.
	
	if (noMoreCurrentPartData != NULL) {
		*noMoreCurrentPartData = p->dataLengthDetermined && p->dataLength == bufLength;
	}

	return bufLength;       // Return #bytes available starting at buf.
}

void AbstractMultiPartParser::consumeCurrentPartData (unsigned int bufLength)
{

	// Preconditions
	if (!returningCurrentPart || parts.Empty()) {
		warn(WARNING_UNSPECIFIED);
		return;
	}

	if (bufLength == 0) return;

	Part *p = parts.First();                             // Retrieve current part.
	OP_ASSERT(p->dataOffset >= bufferOffset);            // Start of data must be within current buffer.
	OP_ASSERT(returnedOffset == p->dataOffset - bufferOffset); // Must have already returned all data preceding p->dataOffset-bufferOffset.
	OP_ASSERT(parserOffset >= p->dataOffset - bufferOffset); // Cannot have parsed less than p->dataOffset-bufferOffset.
	if (bufLength > availableCurrentPartData()) {        // Oops. We're trying to consume more data than is available.
		warn(WARNING_UNSPECIFIED);
		bufLength = availableCurrentPartData();          // Limit bufLength to available data.
	}
	p->dataOffset += bufLength;  // Move dataOffset forwards by the amount of data consumed.
	if (p->dataLengthDetermined) p->dataLength -= bufLength;  // Decrease dataLength by the amount of data consumed.
	returnedOffset += bufLength; // Move returnedOffset forwards by the amount of data consumed.
}

unsigned int AbstractMultiPartParser::availableCurrentPartData () const
{
	// Preconditions
	if (!returningCurrentPart || parts.Empty()) {
		warn(WARNING_UNSPECIFIED);
		return 0;
	}

	Part *p = parts.First();
	unsigned int availableData = parserOffset - (p->dataOffset - bufferOffset); // Default to number of parsed bytes.
	if (p->dataLengthDetermined) availableData = MIN(p->dataLength, availableData); // Limit to part's data length, if known.
	return availableData;
}

BOOL AbstractMultiPartParser::noMoreCurrentPartData () const
{
	// Preconditions
	if (!returningCurrentPart || parts.Empty()) {
		warn(WARNING_UNSPECIFIED);
		return TRUE;
	}

	Part *p = parts.First();
	return p->dataLengthDetermined && p->dataLength == 0; // Return TRUE only if dataLength is known and zero.
}

void AbstractMultiPartParser::finishCurrentPart ()
{
	// Preconditions
	if (!returningCurrentPart || parts.Empty()) {
		warn(WARNING_UNSPECIFIED);
		return;
	}

	Part *p = parts.First();
	if (!p->dataLengthDetermined || p->dataLength > 0)
		warn(WARNING_UNSPECIFIED);

	OP_ASSERT(p->dataOffset >= bufferOffset);              // Start of data must be within current buffer.
	unsigned int dOffset = p->dataOffset - bufferOffset;   // dOffset is p->dataOffset relative to current buffer.
	OP_ASSERT(returnedOffset == dOffset);                  // Must have already returned all data preceding dOffset.
	OP_ASSERT(parserOffset >= dOffset);                    // Cannot have parsed less than dOffset.
	returnedOffset = parserOffset;                         // Move returnedOffset to parserOffset by default.
	if (p->dataLengthDetermined) {                         // But limit to end of part, if known.
		returnedOffset = MIN(dOffset + p->dataLength, parserOffset);
	}

	// Remove p from parts
	p->Out();
	OP_DELETE(p);

	if (!parts.Empty()) { // Next part is already parsed and ready to return.
		// Update returnedOffset to start of data portion in next part.
		p = parts.First();
		OP_ASSERT(p->dataOffset >= bufferOffset);
		OP_ASSERT(returnedOffset <= p->dataOffset - bufferOffset);
		OP_ASSERT(parserOffset >= p->dataOffset - bufferOffset);
		returnedOffset = p->dataOffset - bufferOffset;
	}

	returningCurrentPart = FALSE; // May now call beginNextPart() again.
}

BOOL AbstractMultiPartParser::noMoreParts () const
{
	// Preconditions
	if (returningCurrentPart)
		warn(WARNING_UNSPECIFIED);

	// Return TRUE iff parser will generate no more parts and there are no more parts waiting to be returned.
	return (state == STATE_FINISHED_MULTIPART || state == STATE_FAILURE) && parts.Empty();
}

void AbstractMultiPartParser::prepareBuffer (unsigned int minAvailable)
{
	unsigned int endSpace   = bufferAllocated - bufferLength; // Available buffer space at end of buffer.
	unsigned int totalSpace = returnedOffset + endSpace;      // Total available buffer space.

	/* Figure out how to prepare the buffer for new data:
	 * 1. If there are less than minAvailable space available in the buffer (in total, both at start and end), reallocate buffer.
	 * 2. Else if there are less than minAvailable space at the end, shift buffer so that all available space is at the end.
	 * 3. Else, there is sufficient space available at the end; do nothing.
	 */
	if (totalSpace < minAvailable) { // 1. There are less than minAvailable space available in the buffer. Reallocate buffer.
		unsigned int new_bufferAllocated = MAX(bufferAllocated + MULTIPART_BUFFER_BLOCK_SIZE, (bufferAllocated - totalSpace) + minAvailable);
		char *new_buffer = OP_NEWA(char, new_bufferAllocated);
		if (!new_buffer) {
			warn(WARNING_OUT_OF_MEMORY);
			return;
		}
		op_memmove(new_buffer, buffer + returnedOffset, bufferLength - returnedOffset);
		bufferAllocated = new_bufferAllocated;
		parserOffset -= returnedOffset; // Keep current parser posisiton unchanged relative to data in buffer.
		bufferLength -= returnedOffset; // Buffer length no longer includes returnedOffset bytes at the beginning.
		bufferOffset += returnedOffset; // Buffer now starts returnedOffset bytes further into multipart document.
		adjustOffsets(returnedOffset);
		returnedOffset = 0; // No more available space at start of buffer.
		OP_DELETEA(buffer);
		buffer = new_buffer;
	}
	else if (endSpace < minAvailable) { // 2. There are less than minAvailable space at the end. Shift buffer so that all available space is at the end.
		op_memmove(buffer, buffer + returnedOffset, bufferLength - returnedOffset);
		parserOffset -= returnedOffset; // Keep current parser posisiton unchanged relative to data in buffer.
		bufferLength -= returnedOffset; // Buffer length no longer includes returnedOffset bytes at the beginning.
		bufferOffset += returnedOffset; // Buffer now starts returnedOffset bytes further into multipart document.
		adjustOffsets(returnedOffset);
		returnedOffset = 0; // No more available space at start of buffer.
	}
	else { // 3. There is sufficient space available at the end. Do nothing.
	}
}

void AbstractMultiPartParser::warn (Warning reason, unsigned int offset, unsigned int part) const
{
	if (listener)
		listener->OnMultiPartParserWarning(reason, offset ? offset : bufferOffset+parserOffset, part ? part : currentPartNum);
}
