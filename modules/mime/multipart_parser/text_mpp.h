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

#ifndef TEXT_MPP_H
#define TEXT_MPP_H

#include "modules/mime/multipart_parser/abstract_mpp.h" // AbstractMultiPartParser

/// Boundaries longer than this # bytes are not supported. (RFC 2046 says 70.)
#define MULTIPART_MAX_BOUNDARY_LENGTH 1024

/** Text Multipart Parser. Performs low-level parsing of text multipart documents. */
class TextMultiPartParser : public AbstractMultiPartParser {
public:

/* Public interface */

	/** Constructor.
	 *
	 * The constructor accepts an optional boundary argument. If not given, the
	 * parser will try to autodetect the boundary from the multipart data.
	 *
	 * If bdry is given, but bdryLen is NOT given, the caller must make sure
	 * that bdry is null-terminated, and that the boundary itself does not
	 * contains null bytes.
	 *
	 * @param bdry Boundary as it appears in the "boundary"-parameter to the Content-Type HTTP header.
	 * @param bdryLen # bytes in above boundary.
	 * @param listener (optional) This object will notified about any events, e.g. warnings that occur during multipart parsing.
	 */
	TextMultiPartParser (const char *bdry = 0, unsigned int bdryLen = 0, MultiPartParser_Listener* listener = 0);

	/// Destructor
	virtual ~TextMultiPartParser ();

/* The rest of the public interface is inherited from AbstractMultiPartParser. */

protected:

/* Private subclasses and types */

	/// Return type used to indicate results in subparser methods.
	typedef enum {
		PARSE_RESULT_OK,         ///< Parsing successful.
		PARSE_RESULT_MORE_DATA,  ///< Need more data in buffer to parse successfully. Retry later.
		PARSE_RESULT_FAILURE     ///< Parsing failed.
	} ParseResult;

/* Private methods */

	/** Try to parse the next logical chunk of data.
	 *
	 * @return TRUE iff something was successfully parsed and parserOffset was
	 *         updated; FALSE otherwise.
	 */
	virtual BOOL parse ();

	/** Parse (part of) the preamble of the multipart document.
	 *
	 * This method must only be called when in state STATE_BEFORE_MULTIPART.
	 *
	 * This function will parse as much of the preamble as possible.
	 * 1. If the first boundary is found, offset will be moved to the start of
	 *    the first part (i.e. after the boundary) and state will be set to
	 *    STATE_FINISHED_PART.
	 * 2. If the last boundary is found, offset will be moved end of buffer and
	 *    state will be set to STATE_FINISHED_MULTIPART.
	 * 3. If no boundary is found, offset will be moved to the position from
	 *    which the next invocation of this method should resume parsing.
	 *
	 * @param offset Where to start parsing (relative to buffer).
	 * @param maxLength Max. # bytes to parse from above offset.
	 * @return TRUE if something was parsed (may be TRUE even if state was not
	 *         changed). FALSE if no progress at all was made.
	 */
	virtual BOOL parsePreamble (unsigned int& offset, unsigned int maxLength);

	/** Parse the beginning of a part.
	 *
	 * This method must only be called when in state STATE_FINISHED_PART.
	 *
	 * This function will try to parse the headers at the start of the current
	 * part. If this fails, it will set up the entire part for data-only
	 * parsing. Note that since all headers may not be available when this
	 * method is first called, the state may not change from STATE_FINISHED_PART
	 * to STATE_INSIDE_PART. In this case, this method must be called again when
	 * more data arrives in order to parse the rest of the header.
	 * If header parsing fails completely, the method will assume that no
	 * headers are present, and that the entire part should be parsed as data.
	 * In this case state will be set to STATE_INSIDE_PART, and offset will be
	 * rewound to the start of the part 
	 *
	 * @param offset Where to start parsing (relative to buffer).
	 * @param maxLength Max. # bytes to parse from above offset.
	 * @return TRUE if something was parsed (may be TRUE even if state was not
	 *         changed). FALSE if no progress at all was made.
	 */
	virtual BOOL parsePartBeginning (unsigned int& offset, unsigned int maxLength);

	/** Parse the data portion of a part.
	 *
	 * This method must only be called when in state STATE_INSIDE_PART.
	 *
	 * This function will parse data for the current part up to the boundary
	 * separating the current part from the next part. When a boundary is found
	 * the dataLength and dataLengthDetermined attributes will be set on the
	 * current part.
	 * 1. If the last boundary is found, offset will be moved to the end of the
	 *    buffer, and state will be set to STATE_FINISHED_MULTIPART.
	 * 2. If the next boundary is found, offset will be moved to the start of
	 *    the next part, and state will be set to STATE_FINISHED_PART.
	 * 3. If no boundary is found, offset will be moved to the position from
	 *    which the next call to this method should start, and state will not
	 *    be updated.
	 *
	 * @param offset Where to start parsing (relative to buffer).
	 * @param maxLength Max. # bytes to parse from above offset.
	 * @param isAtPartBeginning Set to TRUE if we're at the very start of
	 *         the part data, i.e. if there is a CR/LF immediately preceding
	 *         given offset, that might conceivably be interpreted as the
	 *         start of a boundary.
	 * @return TRUE if something was parsed (may be TRUE even if state was not
	 *         changed). FALSE if no progress at all was made.
	 */
	virtual BOOL parsePartData (unsigned int& offset, unsigned int maxLength, BOOL isAtPartBeginning = FALSE);

	/** Set the end (i.e. Part::dataLength) of the current part.
	 *
	 * @param endOffset Part ends at this offset, relative to buffer.
	 */
	virtual void foundCurrentPartDataEnd (unsigned int endOffset);

	/** Try to find next boundary. Return TRUE iff successful.
	 *
	 * The given offset variable will be set to an offset
	 * (>= offset && <= offset + maxLength) from which the next invocation of
	 * this method should start.
	 *
	 * If boundary is found, the given boundaryStart variable will be set to the
	 * start of the boundary (i.e. the end of the current part), and the given
	 * boundaryEnd variable will be set to the end of the boundary (i.e. the
	 * start of the next part). Also, the given isLastBoundary flag will be set
	 * to FALSE if the boundary found is a regular boundary, or TRUE if the last
	 * boundary is found.
	 *
	 * If boundary is a NULL pointer, this method will auto-detect the boundary
	 * (using the first occurence of a pattern matching the "delimiter"
	 * specification in RFC 2046). It will then set the boundary variable to the
	 * detected boundary.
	 *
	 * @param offset Start searching for boundary at this offset in buffer. The
	 *         offset appropriate for the start of the next search will be
	 *         stored into here upon return.
	 * @param maxLength Limit search to this # bytes from offset.
	 * @param isAtPartBeginning Set to TRUE if we're at the very beginning of
	 *         a part, i.e. if there is a CR/LF immediately preceding given
	 *         offset, that might conceivably be interpreted as the start of
	 *         a boundary.
	 * @return TRUE iff boundary was found (consult above output parameters for
	 *         details); FALSE if boundary not found.
	 */
	virtual BOOL findNextBoundary (unsigned int& offset, unsigned int maxLength, BOOL isAtPartBeginning = FALSE);

	/** Try to verify the boundary starting at the given offset.
	 *
	 * If verification succeeds, the given offset variable will be set to the
	 * end of the verified boundary (i.e. start of next part). Otherwise,
	 * offset is not moved.
	 *
	 * This method returns:
	 * - PARSE_RESULT_OK if boundary was verified and offset was updated.
	 * - PARSE_RESULT_MORE_DATA if more data is needed in order to verify
	 *                          boundary.
	 * - PARSE_RESULT_FAILURE if boundary verification failed.
	 *
	 * @param offset Start of boundary candidate, relative to buffer. This
	 *         offset variable will be moved to the end of the boundary if
	 *         verification succeeds, i.e. if PARSE_RESULT_OK is returned.
	 * @param maxLength Limit search to this # bytes from offset.
	 * @param atStart Flag indicating that offset points to the start of the
	 *          multipart document. We treat the preceding (CR)LF as optional
	 *          at the start of the document.
	 * @param atEnd Flag indicating that offset + maxLength points to the end of
	 *          the multipart document. We treat the trailing (CR)LF as optional
	 *          at the end of the document.
	 * @return One of the following:
	 *         - PARSE_RESULT_OK if boundary verification succeeds.
	 *         - PARSE_RESULT_MORE_DATA if more data is needed.
	 *         - PARSE_RESULT_FAILURE if boundary verification failed.
	 */
	virtual ParseResult verifyBoundary (unsigned int& offset, unsigned int maxLength, BOOL atStart, BOOL atEnd);
	
	/** Clear the boundary for the current part
	 */
	virtual void clearBoundary ();

	/** Store a found boundary.
	 * When first boundary is found, fix the boundary string if it erroneously starts with "--"
	 * @param start Start offset of boundary of the current part
	 * @param end End offset of boundary of the current part
	 * @param isLast True if the last boundary in the multipart document was found
	 * @param removeLeadingBoundaryDashes If true, leading "--" will be removed from boundary
	 */
	virtual void handleBoundary (unsigned int start, unsigned int end, BOOL isLast, BOOL removeLeadingBoundaryDashes);

	/** Try to detect a boundary starting at the given offset.
	 *
	 * If detection succeeds, the boundary member variable will be allocated and
	 * set to the detected boudary.
	 *
	 * This method returns:
	 * - PARSE_RESULT_OK if boundary was detected, starting at given offset.
	 * - PARSE_RESULT_MORE_DATA if more data is needed in order to detect
	 *                          boundary.
	 * - PARSE_RESULT_FAILURE if boundary detection failed.
	 *
	 * @param offset Start of boundary candidate, relative to buffer.
	 * @param maxLength Limit search to this # bytes from offset.
	 * @param atStart Flag indicating that offset points to the start of the
	 *          multipart document. We treat the preceding (CR)LF as optional
	 *          at the start of the document.
	 * @param atEnd Flag indicating that offset + maxLength points to the end of
	 *          the multipart document. We treat the trailing (CR)LF as optional
	 *          at the end of the document.
	 * @return One of the following:
	 *         - PARSE_RESULT_OK if boundary detection succeeds.
	 *         - PARSE_RESULT_MORE_DATA if more data is needed.
	 *         - PARSE_RESULT_FAILURE if boundary detection failed.
	 */
	virtual ParseResult detectBoundary (unsigned int offset, unsigned int maxLength, BOOL atStart, BOOL atEnd);

	/** Try to validate the given boundary against RFC 2046.
	 *
	 * Return TRUE iff valid, and set length to the length of the boundary minus
	 * trailing linear whitespace (which is not part of the actual boundary)
	 * according to RFC 2046.
	 *
	 * Return FALSE if given boundary is not valid.
	 *
	 * @param candidate Candidate boundary string to validate.
	 * @param length Length of above string. On returning TRUE, this is set to
	 *         actual length of the boundary (minus trailing linear whitespace).
	 * @return TRUE if given candidate is a valid boundary; FALSE otherwise.
	 */
	BOOL validateBoundary (const char *candidate, unsigned int& length);

	/** Parse the header portion of the current part in the multipart document.
	 *
	 * parserOffset must point to the start of the current part.
	 *
	 * @param offset Part headers start at this offset. This parameter will be
	 *         updated to the offset at which parsing ended. When returning
	 *         PARSE_RESULT_OK, this is the start of the data portion of this
	 *         part. When returning PARSE_RESULT_MORE_DATA, this is the offset
	 *         at the end of the last header parsed (there may be more headers).
	 * @param maxLength Maximum allowed parse length.
	 * @param noMoreData Set to TRUE when maxLength indicates the definitive
	 *         end of this part's headers. If set, PARSE_RESULT_MORE_DATA
	 *         will never be returned, rather, if no blank line separating
	 *         headers from body is found, _all_ the data will instead be
	 *         parsed as headers, and the return value will be either
	 *         PARSE_RESULT_OK or PARSE_RESULT_FAILURE.
	 * @return (see ParseResult above)
	 */
	virtual ParseResult parsePartHeader (unsigned int& offset, unsigned int maxLength, BOOL noMoreData);

	/** Find first occurrence of CR or LF in a buffer
	 * 
	 * (Cannot use op_memchr(p, '\n', length) or op_strcspn(p, "\n\r"))
	 * 
	 * @param p pointer to buffer
	 * @param length size of buffer
	 * @return position of first occurrence, or NULL
	 */
	static const char* findFirstCRorLF(const char* p, unsigned int length);

	/** After a buffer relocation, all buffer offsets must be adjusted
	 *
	 * @param adjustment Number to subtract from all offsets
	 */
	virtual void adjustOffsets (unsigned int adjustment);

/* Member variables */

	char *boundary;              ///< Multipart boundary. NULL means unknown boundary (i.e. detect boundary from document).
	unsigned int boundaryLength; ///< Length of above multipart boundary.
	BOOL firstBoundaryFound;     ///< True if first boundary has been found and verified
	BOOL boundaryFound;          ///< True if the boundary of the current part has been found
	unsigned int boundaryStart;  ///< Start offset of boundary of the current part
	unsigned int boundaryEnd;    ///< End offset of boundary of the current part
	BOOL isLastBoundary;         ///< True if the last boundary in the multipart document was found

	Part *currentPart;           ///< Part object currently being prepared. See parsePartBeginning() for more info.
	unsigned int currentPartOrigDataOffset; ///< Original offset of the start of currentPart's data (relative to start of multipart)
};

#endif // TEXT_MPP_H
