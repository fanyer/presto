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

#include "core/pch.h"

#include "modules/mime/multipart_parser/text_mpp.h" // TextMultiPartParser


TextMultiPartParser::TextMultiPartParser (const char *bdry, unsigned int bdryLen, MultiPartParser_Listener* listener)
	: AbstractMultiPartParser(listener)
	, boundary(0)
	, boundaryLength(0)
	, firstBoundaryFound(FALSE)
	, boundaryFound(FALSE)
	, boundaryStart(0)
	, boundaryEnd(0)
	, isLastBoundary(FALSE)
	, currentPart(0)
	, currentPartOrigDataOffset(0)
{
	if (bdry && *bdry) { // bdry is non-NULL and has nonzero length. I.e. we have a given boundary.
		if (bdryLen > 0) boundaryLength = bdryLen;
		else             boundaryLength = (unsigned int)op_strlen(bdry);
		BOOL valid = validateBoundary(bdry, boundaryLength);
		if (!valid) {
			warn(WARNING_INVALID_BOUNDARY);
		}
		else {
			boundary = OP_NEWA(char, boundaryLength + 1);
			if (!boundary) {
				warn(WARNING_OUT_OF_MEMORY);
			}
			else { // Successfully allocated.
				op_memcpy(boundary, bdry, boundaryLength);
				boundary[boundaryLength] = '\0';
			}
		}
		if (!boundary) boundaryLength = 0;
	}
	else {
		warn(WARNING_NO_BOUNDARY);
	}
}

TextMultiPartParser::~TextMultiPartParser ()
{
	if (boundary) OP_DELETEA(boundary);
	boundaryLength = 0;
}

BOOL TextMultiPartParser::parse ()
{
	// Preconditions
	OP_ASSERT(parserOffset <= bufferLength);   // We cannot parse past end of buffer.
	OP_ASSERT(returnedOffset <= parserOffset); // We cannot return more than has been parsed.

	BOOL progress = FALSE; // TRUE indicates success (i.e. something was parsed correctly and parserOffset was moved).

	switch (state) {
		case STATE_BEFORE_MULTIPART: {
			progress = parsePreamble(parserOffset, bufferLength - parserOffset);
			/* If progress is TRUE, state is either STATE_FINISHED_PART (found boundary of part #1)
			 * or STATE_FINISHED_MULTIPART (found last-boundary).
			 * If progress is FALSE, then state is STATE_BEFORE_MULTIPART.
			 */
			returnedOffset = parserOffset; // Nothing worth holding on to in preamble; returnedOffset should follow parserOffset.
			break;
		}
		case STATE_FINISHED_PART: {
			progress = parsePartBeginning(parserOffset, bufferLength - parserOffset);
			/* Will try to parse headers. If success of definitive failure,
			 * progress is TRUE and state proceeds to STATE_INSIDE_PART.
			 * Else, more data is needed and state remains at STATE_FINISHED_PART.
			 */
			break;
		}
		case STATE_INSIDE_PART: {
			progress = parsePartData(parserOffset, bufferLength - parserOffset, parserOffset + bufferOffset == currentPartOrigDataOffset);
			/* Will parse data up to end of part/multipart. progress is set if parserOffset is moved.
			 * state proceeds to STATE_FINISHED_PART if start of next part is found,
			 * or STATE_FINISHED_MULTIPART if last-boundary is found.
			 * Else, state remains at STATE_INSIDE_PART.
			 */
			break;
		}
		case STATE_FINISHED_MULTIPART: {
			progress = FALSE;
			break;
		}
		case STATE_FAILURE:
		default: {
			progress = FALSE;
			break;
		}
	}

	if (finishedLoading && (parserOffset == bufferLength || !progress)) {
		// There is no more incoming data, and we have parsed as much of
		// the existing data is possible.
		if (state == STATE_INSIDE_PART) {
			// We're in the middle of a part, and there's no
			// terminating last-boundary. This is not right, but
			// handle it gracefully anyway.
			// Finish current part at end-of-multipart,
			// and progress to STATE_FINISHED_MULTIPART.
			foundCurrentPartDataEnd(bufferLength);
			state = STATE_FINISHED_MULTIPART;
			warn(WARNING_UNFINISHED_PART);
		}
		else if (state == STATE_BEFORE_MULTIPART || state == STATE_FINISHED_PART) {
			// We're before/between parts, and there's no boundary
			// in sight. Finish multipart after no/current part.
			state = STATE_FINISHED_MULTIPART;
			warn(WARNING_UNFINISHED_PART);
		}
	}

	// Postconditions
	OP_ASSERT(parserOffset <= bufferLength);   // We cannot parse past end of buffer.
	OP_ASSERT(returnedOffset <= parserOffset); // We cannot return more than has been parsed.
	return progress;
}

BOOL TextMultiPartParser::parsePreamble (unsigned int& offset, unsigned int maxLength)
{
	// Given offset points to somewhere before first boundary.
	// Find boundary marking start of first part. Set offset to start of first
	// part (i.e. after boundary) and transfer to STATE_FINISHED_PART
	// (or STATE_FINISHED_MULTIPART if last boundary found).

	// Preconditions
	OP_ASSERT(offset <= bufferLength);
	OP_ASSERT(offset + maxLength <= bufferLength);
	OP_ASSERT(state == STATE_BEFORE_MULTIPART);

	if (maxLength == 0) return FALSE; // Nothing to parse.

	const unsigned int origOffset = offset;
	BOOL progress = FALSE;
	if (findNextBoundary(offset, maxLength)) { // Found first boundary.
		OP_ASSERT(boundaryFound);
		if (isLastBoundary) { // Found end of last part. I.e. no parts in document.
			state = STATE_FINISHED_MULTIPART;
			offset = bufferLength;
		}
		else { // Found start of first part.
			state = STATE_FINISHED_PART;
			offset = boundaryEnd; // Set offset to start of first part, i.e. immediately after boundary.
		}
		progress = TRUE;
	}
	else {
		progress = offset > origOffset; // First boundary not found. Progress depends on whether findNextBoundary() moved offset.
	}

	// Postconditions
	OP_ASSERT(offset >= origOffset);
	OP_ASSERT(offset <= origOffset + maxLength);
	if (progress) OP_ASSERT(offset > origOffset);

	return progress;
}

BOOL TextMultiPartParser::parsePartBeginning (unsigned int& offset, unsigned int maxLength)
{
	// Given offset points to start of next (-> current) part.
	// Parse part headers. Add Part object to parts, and transfer to
	// STATE_INSIDE_PART moving offset to start of part data.
	// If no headers are found, start of part data is considered to be at the
	// given offset (i.e. offset is not moved, but state is changed).

	// Preconditions
	OP_ASSERT(offset <= bufferLength);
	OP_ASSERT(offset + maxLength <= bufferLength);
	OP_ASSERT(state == STATE_FINISHED_PART);

	clearBoundary();
	BOOL progress = FALSE;

	if (maxLength == 0) return FALSE; // Nothing to parse.

	// Before parsing headers for the current part, the currentPart Part object
	// must be properly prepared. However, since we may be called multiple
	// times until headers for a part are completely parsed, we cannot
	// initialize currentPart on every call. The solution is to initialize it
	// when NULL, and set it back to NULL when the headers are completely
	// parsed.
	if (!currentPart) { // Construct new part object for the current part.
		HeaderList *headers = OP_NEW(HeaderList, (KeywordIndex_HTTP_MIME)); // This will be filled with part headers when calling parsePartHeader().
		if (!headers) {
			warn(WARNING_OUT_OF_MEMORY);
			return FALSE;
		}
		// Create preliminary object for current part. Must be filled with headers using parsePartHeader() before adding to parts list.
		++currentPartNum;
		currentPart = OP_NEW(Part, (currentPartNum, headers, parserOffset + bufferOffset));
		if (!currentPart) {
			OP_DELETE(headers);
			warn(WARNING_OUT_OF_MEMORY);
			return FALSE;
		}
	}
	else { // Already constructed. This means we're in the middle of parsing headers for the current part.
		OP_ASSERT(currentPart);
		OP_ASSERT(currentPart->headers);
		OP_ASSERT(currentPart->num == currentPartNum);
		OP_ASSERT(currentPart->dataOffset);
		OP_ASSERT(!currentPart->dataLengthDetermined);
	}

	const unsigned int origOffset = offset;
	BOOL atEnd = finishedLoading && (origOffset + maxLength == bufferLength); // No more incoming data and maxLength is at end of buffer.

	// Call findNextBoundary() to limit maxLength before calling parsePartHeader()
	BOOL nextPartFound = FALSE;
	if (findNextBoundary(offset, maxLength, TRUE)) { // Found next boundary.
		OP_ASSERT(boundaryFound);
		nextPartFound = TRUE;
		maxLength = boundaryStart - origOffset; // Limit parseLength to end of part if found before end of buffer.
	}
	else {
		// We didn't find a boundary, but we still shouldn't search
		// past the start of a potential boundary. Therefore limit
		// maxLength based on how much offset was moved by
		// findNextBoundary().
		maxLength = offset - origOffset;
	}
	offset = origOffset; // Reset offset back to start of headers

	// Parse headers between offset and maxLength.
	ParseResult result = parsePartHeader(offset, maxLength, nextPartFound || atEnd); // Try to parse next part's headers.

	switch (result) {
		case PARSE_RESULT_OK: { // Headers parsed successfully. offset points to start of data.
			// Add currentPart to parts list.
			currentPart->dataOffset = offset + bufferOffset; // Set part's data offset to start of data portion.
			state = STATE_INSIDE_PART; // Move to next state to trigger parsing of data portion.
			progress = TRUE;
			break;
		}
		case PARSE_RESULT_MORE_DATA: { // Not finished parsing headers. Need more data. offset points to start of headers.
			progress = offset > origOffset; // Progress depends on whether offset was moved.
			break;
		}
		case PARSE_RESULT_FAILURE: { // Failed to parse headers. offset is undefined.
			// Treat entire part as data only.
			// Rewind parserOffset to start of part, and treat everything as data.
			warn(WARNING_INCOMPLETE_PART_HEADER);
			OP_ASSERT(currentPart->dataOffset >= bufferOffset);
			offset = currentPart->dataOffset - bufferOffset; // Reset offset to start of part.
			OP_ASSERT(offset >= returnedOffset);
			OP_ASSERT(offset <= bufferLength);
			OP_ASSERT(currentPart->headers);
			currentPart->headers->Clear(); // Remove all headers that may have been parsed up to this point.
			state = STATE_INSIDE_PART; // Move to next state to parse entire part as data-only.
			progress = TRUE;
			break;
		}
	}

	if (state == STATE_INSIDE_PART) { // We have finished parsing the header.
		// Add currentPart to list of parts.
		if (parts.Empty()) { // currentPart is the only part.
			// This means that returnedOffset does not depend on any other part which is still to be returned.
			// Update returnedOffset to start of data portion in this part.
			OP_ASSERT(currentPart->dataOffset >= bufferOffset);
			OP_ASSERT(returnedOffset + bufferOffset <= currentPart->dataOffset);
			OP_ASSERT(parserOffset + bufferOffset >= currentPart->dataOffset);
			returnedOffset = currentPart->dataOffset - bufferOffset;
		}
		currentPartOrigDataOffset = currentPart->dataOffset;
		currentPart->Into(&parts);
		currentPart = 0;

		OP_ASSERT(!parts.Empty()); // Part should have been added to parts list.
		OP_ASSERT(parts.Last()->num == currentPartNum); // Added part should have correct part number.
	}

	return progress;
}

BOOL TextMultiPartParser::parsePartData (unsigned int& offset, unsigned int maxLength, BOOL isAtPartBeginning /* = FALSE */)
{
	// Given offset points to somewhere inside current part.
	// Keep parsing part data and moving parserOffset until next boundary is found.
	// Set part's dataLength. Set parserOffset to start of next part and transfer to
	// STATE_FINISHED_PART (or STATE_FINISHED_MULTIPART if last boundary found).

	// Preconditions
	OP_ASSERT(offset <= bufferLength);
	OP_ASSERT(offset + maxLength <= bufferLength);
	OP_ASSERT(state == STATE_INSIDE_PART);

	if (maxLength == 0) return FALSE; // Nothing to parse.

	BOOL progress = FALSE;
	const unsigned int origOffset = offset;
	if (findNextBoundary(offset, maxLength, isAtPartBeginning)) { // Found next boundary.
		OP_ASSERT(boundaryFound);
		// Current part ends at bStart.
		foundCurrentPartDataEnd(boundaryStart);

		// Next part starts at bEnd.
		if (isLastBoundary) { // Found end of last part. I.e. no more parts in document.
			state = STATE_FINISHED_MULTIPART;
			offset = origOffset + maxLength;
		}
		else if (currentPartNum >= 65536) { // Too many parts. Bailing out
			state = STATE_FINISHED_MULTIPART;
			offset = origOffset + maxLength;
		}
		else { // Found start of next part.
			state = STATE_FINISHED_PART;
			offset = boundaryEnd;
		}
		progress = TRUE;
	}
	else {
		progress = offset > origOffset;
	}

	return progress;
}

void TextMultiPartParser::foundCurrentPartDataEnd (unsigned int endOffset)
{
	// endOffset is relative to buffer.
	if (!parts.Empty()) { // Current part is still in parts list.
		Part *p = parts.Last(); // Retrieve current part.
		OP_ASSERT(p->num == currentPartNum); // Ensure this is the current part.
		OP_ASSERT(bufferOffset + endOffset >= p->dataOffset); // Ensure endOffset does not precede start of data.
		OP_ASSERT(!p->dataLengthDetermined); // Data length should not alredy be determined for the current part.
		p->dataLength = (bufferOffset + endOffset) - p->dataOffset; // Set dataLength to #bytes between start of part and given endOffset.
		p->dataLengthDetermined = TRUE;
	}
	else {
		warn(WARNING_UNSPECIFIED);
	}
}

BOOL TextMultiPartParser::findNextBoundary (
	unsigned int& offset,
	unsigned int  maxLength,
	BOOL          isAtPartBeginning /* = FALSE */)
{
	// Preconditions
	OP_ASSERT(offset <= bufferLength);
	OP_ASSERT(offset + maxLength <= bufferLength);

	if (boundaryFound) {
		OP_ASSERT(boundaryStart >= offset);
		OP_ASSERT(boundaryEnd <= offset+maxLength);
		return TRUE;
	}

	if (maxLength == 0) return FALSE; // Empty search space. Not found; don't touch offset.

	const unsigned int origOffset = offset; // maxLength is always relative to origOffset
	BOOL atStart = isAtPartBeginning || (origOffset == 0 && bufferOffset == 0); // origOffset is at start of multipart.
	BOOL atEnd = finishedLoading && (origOffset + maxLength == bufferLength); // No more incoming data and maxLength is at end of buffer.
	unsigned int startOffset = 0;

	// Boundary MUST start and end with CR/LF. Search for CRs and LFs
	// Exception: Accept first boundary without preceding CR/LF at start of (multi)part.
	const char *p = 0;
	if (atStart) p = (const char *) buffer + offset; // Start searching at beginning of (multi)part document.
	else p = findFirstCRorLF(buffer + offset, maxLength); // Start searching for boundary at first CR/LF

	while (p) { // Next CR/LF found at p.
		offset = (unsigned int)(p - buffer); // Set offset to leading CR/LF.
		OP_ASSERT(offset < origOffset + maxLength);
		startOffset = offset;

		// Try to verify/detect potential boundary starting at current offset.
		unsigned int o = offset;
		ParseResult result;

		if (boundary) { // Try to verify boundary.
			result = verifyBoundary(o, maxLength - (offset - origOffset), atStart && offset == origOffset, atEnd);
			if (result == PARSE_RESULT_OK) { // Boundary verified successfully.
				offset = o; // Move offset to start of next part.
				// boundaryStart, boundaryEnd and isLastBoundary is set by verifyBoundary().
				return TRUE;
			}
		}
		else { // Try to auto-detect boundary.
			result = detectBoundary(o, maxLength - (offset - origOffset), atStart && offset == origOffset, atEnd);
			if (result == PARSE_RESULT_OK) { // Boundary detected successfully.
				continue; // Rerun loop iteration from same p to verify boundary.
			}
		}
		// Boundary not verified/detected.

		if (result == PARSE_RESULT_MORE_DATA) break; // More data needed before boundary can be verified/detected.

		// If we get here, we did not find/detect potential boundary starting at p. Keep searching.
		OP_ASSERT(result == PARSE_RESULT_FAILURE);
		OP_ASSERT(buffer + origOffset <= p);
		atStart = FALSE; // We are no longer at start of (multi)part.
		if (p[0] == '\r' && p[1] == '\n') ++p; // Skip one more in case of CRLF
		++p; // Start next search immediately after current CR/LF.
		p = findFirstCRorLF(p, (unsigned int)(maxLength - (p - (buffer + origOffset)))); // Find next CR or LF
	}

	// At this point:
	// - If p is NULL, no potential start-of-boundary was found within
	//   search space, and we can move offset to end of buffer
	// - Otherwise, p points to a potential start-of-boundary that needs
	//   more data in order to be verified. (If it is verified, we have
	//   already returned before we get here). In that case offset should
	//   point to potential start-of-boundary (but only if we will get more
	//   data).
	// In either case return FALSE since we did not find a complete boundary.

	if (p && !atEnd) { // Potential start-of-boundary found
		// Next search should start at beginning of potential start-of-boundary.
		offset = startOffset;
	}
	else { // No potential start-of-boundary found
		// Move offset to end of buffer, as there is definitely no boundary (or start-of-boundary) in the given buffer.
		offset = origOffset + maxLength;
	}

	return FALSE;
}

TextMultiPartParser::ParseResult TextMultiPartParser::verifyBoundary (unsigned int& offset, unsigned int maxLength, BOOL atStart, BOOL atEnd)
{
	// Preconditions
	OP_ASSERT(offset <= bufferLength);
	OP_ASSERT(offset + maxLength <= bufferLength);
	OP_ASSERT(boundary && *boundary); // Boundary must be determined.

	if (maxLength < 3) return PARSE_RESULT_MORE_DATA; // Boundary must be at least 3 chars. Need more data.

	// Initialize output parameters
	BOOL lastBoundary = FALSE;

	// The following offsets are all relative to buffer (and therefore comparable to offset)
	unsigned int startOffset     = offset; // Offset to start of initial CRLF/LF/CR.
	unsigned int dashOffset      = 0; // Offset to start of dashes following initial CRLF/LF/CR.
	unsigned int boundaryOffset  = 0; // Offset to start of boundary following dashes.
	unsigned int endDashOffset   = 0; // Offset to dashes following boundary (if any)
	unsigned int tpadOffset      = 0; // Offset to transport padding (linear whitespace) following boundary.
	unsigned int crlfOffset      = 0; // Offset to CRLF/LF/CR following transport padding.
	const unsigned int maxOffset = offset + maxLength; // maxOffset is maxLength relative to buffer.

	// Special case: We're at the beginning of the multipart document, and the very first character is a '-'.
	if (atStart && buffer[startOffset] == '-') { // Special case
		dashOffset = startOffset; // Allow dashes immediately *without* a preceding CRLF/LF/CR.
	}
	// Regular case: offset starts with CRLF/LF/CR
	else if (buffer[startOffset] == '\r' && buffer[startOffset + 1] == '\n') dashOffset = startOffset + 2; // CRLF.
	else if (buffer[startOffset] == '\n' || buffer[startOffset] == '\r') dashOffset = startOffset + 1; // CR/LF only. Dashes start immediately after.
	else return PARSE_RESULT_FAILURE; // Boundary does not start with CRLF/LF/CR.

	boundaryOffset = dashOffset + 2; // Boundary should start immediately after dashes.
	if (boundaryOffset >= maxOffset) return PARSE_RESULT_MORE_DATA; // Not enough data to check dashes.

	if (buffer[dashOffset] != '-' || buffer[dashOffset + 1] != '-') return PARSE_RESULT_FAILURE; // Dashes not found.
	
	// Dashes found. Start looking for boundary after dashes.
	unsigned int thisBoundaryLength = boundaryLength;
	unsigned int cmpLen = MIN(thisBoundaryLength, maxOffset - boundaryOffset);
	BOOL partiallyFound = FALSE;
	BOOL removedLeadingBoundaryDashes = FALSE;
	if (!cmpLen) return PARSE_RESULT_MORE_DATA; // Need more data.
	if (op_memcmp(boundary, buffer + boundaryOffset, cmpLen) == 0) partiallyFound = TRUE; // First cmpLen bytes of boundary matches.
	else if (!firstBoundaryFound) { // No match.
		// Special case before giving up: If boundary start with "--", try matching from boundary[2].
		if (thisBoundaryLength > 2 && boundary[0] == '-' && boundary[1] == '-') {
			thisBoundaryLength -= 2;
			cmpLen = MIN(thisBoundaryLength, maxOffset - boundaryOffset);
			if (!cmpLen) return PARSE_RESULT_MORE_DATA; // Need more data.
			else if (op_memcmp(boundary + 2, buffer + boundaryOffset, cmpLen) == 0) {
				partiallyFound = TRUE; // First cmpLen bytes of boundary[2:] matches.
				// Must wait until PARSE_RESULT_OK before modifying the boundary string
				removedLeadingBoundaryDashes = TRUE;
			}
		}
	}

	if (!partiallyFound) return PARSE_RESULT_FAILURE; // No partial match
	else if (cmpLen < thisBoundaryLength) return PARSE_RESULT_MORE_DATA; // We have a partial match.

	// We have a complete match.
	OP_ASSERT(cmpLen == thisBoundaryLength);
	endDashOffset = boundaryOffset + thisBoundaryLength;
	if (endDashOffset >= maxOffset) { // Cannot continue parsing
		if (atEnd) {
			// We're at the very end of the multipart and have found
			// a boundary without end-dashes, transport padding and
			// terminating CR/LF. Accept it as boundary anyway.
			offset = maxOffset;
			handleBoundary(startOffset, offset, FALSE, removedLeadingBoundaryDashes);
			return PARSE_RESULT_OK;
		}
		else {
			return PARSE_RESULT_MORE_DATA;
		}
	}

	tpadOffset = endDashOffset; // transport padding normally starts now, override if last boundary found
	if (buffer[endDashOffset] == '-') { // This may be the last boundary
		if (endDashOffset + 1 >= maxOffset) { // Cannot check the next byte
			if (atEnd) {
				// We're at the very end of the multipart and
				// have found a boundary with _one end-dash,
				// no transport padding andno terminating CR/LF.
				// Accept it as boundary anyway.
				offset = maxOffset;
				handleBoundary(startOffset, offset, TRUE, removedLeadingBoundaryDashes);
				return PARSE_RESULT_OK;
			}
			else {
				return PARSE_RESULT_MORE_DATA;
			}
		}
		if (buffer[endDashOffset + 1] == '-') { // Found last boundary
			tpadOffset = endDashOffset + 2; // Set tpadOffset to after dashes.
			lastBoundary = TRUE;
		}
	}

	crlfOffset = tpadOffset;
	while (crlfOffset < maxOffset) { // Analyze characters starting at tpadOffset, searching for CRLF/LF/CR to end boundary.
		switch (buffer[crlfOffset]) {
			case ' ':
			case '\t': { // Match linear whitespace.
				// Linear whitespace is allowed in transport-padding.
				// Keep going.
				break;
			}
			case '\r':
			case '\n': { // Found CR or LF.
				// The boundary pattern ends with one of CRLF, LF, CR.
				if (buffer[crlfOffset] == '\r') { // Need to look ahead to see if we have CRLF or just CR.
					if (crlfOffset + 1 >= maxOffset) { // Need more data to determine CR vs. CRLF.
						if (atEnd) { // But there will never be more data. CR is ok.
							offset = maxOffset;
							handleBoundary(startOffset, offset, lastBoundary, removedLeadingBoundaryDashes);
							return PARSE_RESULT_OK;
						}
						else {
							return PARSE_RESULT_MORE_DATA;
						}
					}
					if (buffer[crlfOffset + 1] == '\n') { // CRLF
						// We have now matched the complete boundary pattern. Update offset and return appropriately
						offset = crlfOffset + 2;
						handleBoundary(startOffset, offset, lastBoundary, removedLeadingBoundaryDashes);
						return PARSE_RESULT_OK;
					}
					// CR only; fall through to LF only handling
				}
				// LF only

				// We have now matched the complete boundary pattern. Update offset and return appropriately
				offset = crlfOffset + 1;
				handleBoundary(startOffset, offset, lastBoundary, removedLeadingBoundaryDashes);
				return PARSE_RESULT_OK;
			}
			default: { // Found something else, i.e. illegal.
				return PARSE_RESULT_FAILURE;
			}
		}
		++crlfOffset;
	}

	OP_ASSERT(crlfOffset == maxOffset); // We're at end of buffer.
	if (atEnd) { // Allow missing (CR)LF at end of multipart document
		offset = crlfOffset;
		handleBoundary(startOffset, offset, lastBoundary, removedLeadingBoundaryDashes);
		return PARSE_RESULT_OK;
	}

	return PARSE_RESULT_MORE_DATA; // Hit end of buffer, but not end of document. Need more data.
}

void TextMultiPartParser::clearBoundary ()
{
	boundaryFound = FALSE;
	boundaryStart = 0;
	boundaryEnd = 0;
	isLastBoundary = FALSE;
}

void TextMultiPartParser::handleBoundary (unsigned int start, unsigned int end, BOOL isLast, BOOL removeLeadingBoundaryDashes)
{
	boundaryFound = TRUE;
	boundaryStart = start;
	boundaryEnd = end;
	isLastBoundary = isLast;

	if (removeLeadingBoundaryDashes) {
		OP_ASSERT (!firstBoundaryFound && boundaryLength > 2 && boundary[0] == '-' && boundary[1] == '-');
		boundaryLength -= 2;
		op_memmove(boundary, boundary+2, boundaryLength+1); // Modify in place, keeping the old boundary buffer
		warn(WARNING_INVALID_BOUNDARY);
	}
	firstBoundaryFound = TRUE;
}

TextMultiPartParser::ParseResult TextMultiPartParser::detectBoundary (unsigned int offset, unsigned int maxLength, BOOL atStart, BOOL atEnd)
{
	// Preconditions
	OP_ASSERT(offset <= bufferLength);
	OP_ASSERT(offset + maxLength <= bufferLength);
	OP_ASSERT(!boundary); // Boundary must not be determined.

	if (maxLength < 3) return PARSE_RESULT_MORE_DATA; // Boundary must be at least 3 chars. Need more data.

	// The following offsets are all relative to buffer (and therefore comparable to offset)
	unsigned int startOffset     = offset; // Offset to start of initial CRLF/LF/CR.
	unsigned int dashOffset      = 0; // Offset to start of dashes following initial CRLF/LF/CR.
	unsigned int boundaryOffset  = 0; // Offset to start of boundary following dashes.
	const unsigned int maxOffset = offset + maxLength; // maxOffset is maxLength relative to buffer.

	// Special case: We're at the beginning of the multipart document, and the very first character is a '-'.
	if (atStart && buffer[startOffset] == '-') { // Special case
		dashOffset = startOffset; // Allow dashes immediately *without* a preceding CRLF/LF/CR.
	}
	// Regular case: offset starts with CRLF/LF/CR
	else if (buffer[startOffset] == '\r' && buffer[startOffset + 1] == '\n') dashOffset = startOffset + 2; // CRLF.
	else if (buffer[startOffset] == '\n' || buffer[startOffset] == '\r') dashOffset = startOffset + 1; // LF/CR only. Dashes start immediately after.
	else return PARSE_RESULT_FAILURE; // Boundary does not start with CRLF/LF/CR.

	boundaryOffset = dashOffset + 2; // Boundary should start immediately after dashes.
	if (boundaryOffset >= maxOffset) return PARSE_RESULT_MORE_DATA; // Not enough data to check dashes.

	if (buffer[dashOffset] != '-' || buffer[dashOffset + 1] != '-') return PARSE_RESULT_FAILURE; // Dashes not found.
	
	// Dashes found. Start looking for boundary after dashes.
	// Find next CRLF/LF/CR that terminates boundary. Special case: atEnd may also terminate boundary.
	// If not found within MULTIPART_MAX_BOUNDARY_LENGTH bytes. Assume no boundary.
	// If found, try to validate boundary between boundaryOffset and found CRLF/LF/CR.

	unsigned int searchLength = MIN(maxOffset - boundaryOffset, MULTIPART_MAX_BOUNDARY_LENGTH);
	const char *lf = findFirstCRorLF(buffer + boundaryOffset, searchLength); // Find next CR or LF.
	if (!lf && atEnd && boundaryOffset + searchLength == maxOffset) { // CRLF/LF/CR not found, but we're at end of multipart document.
		lf = buffer + maxOffset; // Fake LF at end of multipart document.
	}
	if (lf) { // Found CRLF/CR/LF (or end-of-multipart).
		boundaryLength = (unsigned int)(lf - (buffer + boundaryOffset));
		if (validateBoundary(buffer + boundaryOffset, boundaryLength)) { // Detected boundary!
			boundary = OP_NEWA(char, boundaryLength + 1);
			if (!boundary) {
				warn(WARNING_OUT_OF_MEMORY);
				boundaryLength = 0;
				return PARSE_RESULT_FAILURE; // Give up and try again later.
			}
			else { // Successfully allocated auto-detected boundary.
				op_memcpy(boundary, buffer + boundaryOffset, boundaryLength);
				boundary[boundaryLength] = '\0';
				return PARSE_RESULT_OK;
			}
		}
		// Invalid boundary.
		boundaryLength = 0;
		return PARSE_RESULT_FAILURE;
	}
	// CRLF/LF/CR not found.

	// If search space is < MULTIPART_MAX_BOUNDARY_LENGTH bytes and we're not at end of multipart document. We must retry when we get more data.
	if (searchLength < MULTIPART_MAX_BOUNDARY_LENGTH && !atEnd) return PARSE_RESULT_MORE_DATA;

	// LF not found within MULTIPART_MAX_BOUNDARY_LENGTH. Give up.
	return PARSE_RESULT_FAILURE;
}

BOOL TextMultiPartParser::validateBoundary (const char *candidate, unsigned int& length)
{
	// Relaxed validation:
	// Allow anything that does not contain CR/LF and that is not too long.

	if (length == 0) { // Must be non-empty.
		return FALSE;
	}
	if (findFirstCRorLF(candidate, length)) { // Found a CR or LF.
		return FALSE;
	}
	while (length > 0) { // Remove trailing whitespace.
		char c = candidate[length - 1];
		if (c == ' ' || c == '\t') --length; // Decrement as long as whitespace is found.
		else break; // Stop at first non-whitespace.
	}
	if (length == 0) { // Must still be non-empty.
		return FALSE;
	}
	if (length > MULTIPART_MAX_BOUNDARY_LENGTH) { // Too long.
		return FALSE;
	}
	if (length > 70) { // Too long according to RFC 2046
		warn(WARNING_INVALID_BOUNDARY);
	}
	for (unsigned int i=0; i<length; i++)
		if (op_strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				"abcdefghijklmnopqrstuvwxyz"
				"0123456789'()+_-,./:=?@%#<>[]!&;{}\\^~|* $", candidate[i]) == NULL) {
			warn(WARNING_INVALID_BOUNDARY);
			// Warn, but accept the boundary
			break;
		}

	// candidate is still non-empty, has no LFs, and is not too long. Accept.
	return TRUE;
}

TextMultiPartParser::ParseResult TextMultiPartParser::parsePartHeader (unsigned int& offset, unsigned int maxLength, BOOL noMoreData)
{
	// Preconditions
	OP_ASSERT(currentPart->headers);   // HeaderList object must be present.
	OP_ASSERT(offset <= bufferLength); // We cannot parse past end of buffer.
	OP_ASSERT(offset + maxLength <= bufferLength); // We cannot parse past end of buffer.
	OP_ASSERT(offset + bufferOffset >= currentPart->dataOffset); // Given buffer must be inside current part.
	OP_ASSERT(!currentPart->dataLengthDetermined); // We cannot have parsed the end of the current part.
	OP_ASSERT(state == STATE_FINISHED_PART); // Must be in expected state.

	if (maxLength == 0) return noMoreData ? PARSE_RESULT_FAILURE : PARSE_RESULT_MORE_DATA;
	unsigned int maxOffset = offset + maxLength;

	// Search for first blank line (CR/LF immediately followed by CR/LF).
	// If found, set foundBlankLine to TRUE, and o to the next line (start
	// of body). If no blank line is found, set o to end of data.

	BOOL foundBlankLine = FALSE;
	unsigned int o = offset; // o starts at offset and moves towards maxOffset
	unsigned int oCRLF;

	do {
		// Find next CR/LF
		const char *p = findFirstCRorLF(buffer + o, maxOffset - o); // Start searching for CR or LF at o.
		if (!p) { // No CR/LF found
			// Did not find blank line
			o = oCRLF = maxOffset;
			break;
		}
		BOOL foundAtStart = (p == buffer + o);
		oCRLF = (unsigned int)(p - buffer);

		// p points to CR/LF
		if (p < buffer + maxOffset && *p == '\r') p++; // p pointed to CR, now points to LF in CRLF, or to after single CR
		if (p < buffer + maxOffset && *p == '\n') p++; // p pointed to LF (possibly as part of CRLF), now points to after LF
		// p points to after CR/LF
		if (foundAtStart) { // Allow empty header
			foundBlankLine = TRUE;
		}
		else if (p < buffer + maxOffset && (*p == '\r' || *p == '\n')) { // p points to another CR/LF
			foundBlankLine = TRUE;
			p++; // Move past CR/LF
			if (p < buffer + maxOffset && *(p - 1) == '\r' && *p == '\n') p++; // Move past LF in CRLF
		}
		o = (unsigned int)(p - buffer); // Resume search from current position of p
	} while (o < maxOffset && !foundBlankLine);

	// o now points to after blank line, or end of buffer.
	// Try to parse headers if we either
	// - found a blank line OR
	// - didn't find a blank line, but we'll never get more data, so try anyway.

	if (!foundBlankLine && noMoreData) {
		warn(WARNING_INCOMPLETE_PART_HEADER);
	}
	if (foundBlankLine || noMoreData) { // We need to try parsing headers.
		unsigned int headersLength = oCRLF - offset;
		if (headersLength != 0) {
			char *headersCopy = OP_NEWA(char, headersLength + 1); // This will be taken over by currentPart's HeaderList and deleted by it.
			if (!headersCopy) {
				warn(WARNING_OUT_OF_MEMORY);
				return PARSE_RESULT_FAILURE;
			}
			op_memcpy(headersCopy, buffer + offset, headersLength);
			headersCopy[headersLength] = '\0';
	
			// Initiate header parsing
			OP_STATUS result = currentPart->headers->SetValue(headersCopy, NVS_TAKE_CONTENT); // currentPart->headers now owns headersCopy
			if (!OpStatus::IsSuccess(result)) {
				warn(WARNING_INVALID_HEADER);
				return PARSE_RESULT_FAILURE;
			}
		}

		// Successfully parsed headers
		offset = o; // Move offset to start of body
		return PARSE_RESULT_OK;
	}

	// Did not find blank line terminating headers. Wait for more data
	return PARSE_RESULT_MORE_DATA;
}

const char* TextMultiPartParser::findFirstCRorLF(const char* p, unsigned int length)
{
    while (length > 0 && *p != '\n' && *p != '\r') {
       ++p;
       --length;
    }
    if (length == 0)
        return NULL;
    return p;
}

void TextMultiPartParser::adjustOffsets (unsigned int adjustment)
{
	if (boundaryFound) {
		boundaryStart -= adjustment;
		boundaryEnd -= adjustment;
	}
	currentPartOrigDataOffset -= adjustment;
}
