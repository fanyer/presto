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

#ifdef WBMULTIPART_MIXED_SUPPORT

#include "modules/mime/multipart_parser/binary_mpp.h" // BinaryMultiPartParser

#include "modules/url/protocols/pcomm.h" // ProtocolComm
#include "modules/url/tools/arrays.h" // CONST_ARRAY...
#include "modules/util/datefun.h" // MakeDate3()

#if !defined(ENCODINGS_HAVE_MIB_SUPPORT) && !defined(WBXML_SUPPORT)
# warning BinaryMultiPartParser may not compile without #defining ENCODINGS_HAVE_MIB_SUPPORT or WBXML_SUPPORT
#endif
#include "modules/encodings/utility/charsetnames.h" // CharsetManager::GetCharsetNameFromMIBenum()

PREFIX_CONST_ARRAY(static, const char*, binary_mpp_wellknownheadername, mime)
	CONST_ENTRY("Accept")                // 0x00
	CONST_ENTRY("Accept-Charset")        // 0x01
	CONST_ENTRY("Accept-Encoding")       // 0x02
	CONST_ENTRY("Accept-Language")       // 0x03
	CONST_ENTRY("Accept-Ranges")         // 0x04
	CONST_ENTRY("Age")                   // 0x05
	CONST_ENTRY("Allow")                 // 0x06
	CONST_ENTRY("Authorization")         // 0x07
	CONST_ENTRY("Cache-Control")         // 0x08
	CONST_ENTRY("Connection")            // 0x09
	CONST_ENTRY("Content-Base")          // 0x0A
	CONST_ENTRY("Content-Encoding")      // 0x0B
	CONST_ENTRY("Content-Language")      // 0x0C
	CONST_ENTRY("Content-Length")        // 0x0D
	CONST_ENTRY("Content-Location")      // 0x0E
	CONST_ENTRY("Content-MD5")           // 0x0F
	CONST_ENTRY("Content-Range")         // 0x10
	CONST_ENTRY("Content-Type")          // 0x11
	CONST_ENTRY("Date")                  // 0x12
	CONST_ENTRY("Etag")                  // 0x13
	CONST_ENTRY("Expires")               // 0x14
	CONST_ENTRY("From")                  // 0x15
	CONST_ENTRY("Host")                  // 0x16
	CONST_ENTRY("If-Modified-Since")     // 0x17
	CONST_ENTRY("If-Match")              // 0x18
	CONST_ENTRY("If-None-Match")         // 0x19
	CONST_ENTRY("If-Range")              // 0x1A
	CONST_ENTRY("If-Unmodified-Since")   // 0x1B
	CONST_ENTRY("Location")              // 0x1C
	CONST_ENTRY("Last-Modified")         // 0x1D
	CONST_ENTRY("Max-Forwards")          // 0x1E
	CONST_ENTRY("Pragma")                // 0x1F
	CONST_ENTRY("Proxy-Authenticate")    // 0x20
	CONST_ENTRY("Proxy-Authorization")   // 0x21
	CONST_ENTRY("Public")                // 0x22
	CONST_ENTRY("Range")                 // 0x23
	CONST_ENTRY("Referer")               // 0x24
	CONST_ENTRY("Retry-After")           // 0x25
	CONST_ENTRY("Server")                // 0x26
	CONST_ENTRY("Transfer-Encoding")     // 0x27
	CONST_ENTRY("Upgrade")               // 0x28
	CONST_ENTRY("User-Agent")            // 0x29
	CONST_ENTRY("Vary")                  // 0x2A
	CONST_ENTRY("Via")                   // 0x2B
	CONST_ENTRY("Warning")               // 0x2C
	CONST_ENTRY("WWW-Authenticate")      // 0x2D
	CONST_ENTRY("Content-Disposition")   // 0x2E
	CONST_ENTRY("X-Wap-Application-Id")  // 0x2F
	CONST_ENTRY("X-Wap-Content-URI")     // 0x30
	CONST_ENTRY("X-Wap-Initiator-URI")   // 0x31
	CONST_ENTRY("Accept-Application")    // 0x32
	CONST_ENTRY("Bearer-Indication")     // 0x33
	CONST_ENTRY("Push-Flag")             // 0x34
	CONST_ENTRY("Profile")               // 0x35
	CONST_ENTRY("Profile-Diff")          // 0x36
	CONST_ENTRY("Profile-Warning")       // 0x37
	CONST_ENTRY("Expect")                // 0x38
	CONST_ENTRY("TE")                    // 0x39
	CONST_ENTRY("Trailer")               // 0x3A
	CONST_ENTRY("Accept-Charset")        // 0x3B
	CONST_ENTRY("Accept-Encoding")       // 0x3C
	CONST_ENTRY("Cache-Control")         // 0x3D
	CONST_ENTRY("Content-Range")         // 0x3E
	CONST_ENTRY("X-Wap-Tod")             // 0x3F
	CONST_ENTRY("Content-ID")            // 0x40
	CONST_ENTRY("Set-Cookie")            // 0x41
	CONST_ENTRY("Cookie")                // 0x42
	CONST_ENTRY("Encoding-Version")      // 0x43
	CONST_ENTRY("Profile-Warning")       // 0x44
	CONST_ENTRY("Content-Disposition")   // 0x45
	CONST_ENTRY("X-WAP-Security")        // 0x46
	CONST_ENTRY("Cache-Control")         // 0x47
CONST_END(binary_mpp_wellknownheadername)

PREFIX_CONST_ARRAY(static, const char*, binary_mpp_wellknowncachecontrol, mime)
	CONST_ENTRY("no-cache")          // 0x00
	CONST_ENTRY("no-store")          // 0x01
	CONST_ENTRY("max-age")           // 0x02
	CONST_ENTRY("max-stale")         // 0x03
	CONST_ENTRY("min-fresh")         // 0x04
	CONST_ENTRY("only-if-cached")    // 0x05
	CONST_ENTRY("public")            // 0x06
	CONST_ENTRY("private")           // 0x07
	CONST_ENTRY("no-transform")      // 0x08
	CONST_ENTRY("must-revalidate")   // 0x09
	CONST_ENTRY("proxy-revalidate")  // 0x0a
	CONST_ENTRY("s-maxage")          // 0x0b
CONST_END(binary_mpp_wellknowncachecontrol)

PREFIX_CONST_ARRAY(static, const char*, binary_mpp_wellknowncontenttype, mime)
	CONST_ENTRY("*/*")                                        // 0X00
	CONST_ENTRY("text/*")                                     // 0x01
	CONST_ENTRY("text/html")                                  // 0x02
	CONST_ENTRY("text/plain")                                 // 0x03
	CONST_ENTRY("text/x-hdml")                                // 0x04
	CONST_ENTRY("text/x-ttml")                                // 0x05
	CONST_ENTRY("text/x-vcalendar")                           // 0x06
	CONST_ENTRY("text/x-vcard")                               // 0x07
	CONST_ENTRY("text/vnd.wap.wml")                           // 0x08
	CONST_ENTRY("text/vnd.wap.wmlscript")                     // 0x09
	CONST_ENTRY("text/vnd.wap.wta-event")                     // 0x0a
	CONST_ENTRY("multipart/*")                                // 0x0b
	CONST_ENTRY("multipart/mixed")                            // 0x0c
	CONST_ENTRY("multipart/form-data")                        // 0x0d
	CONST_ENTRY("multipart/byterantes")                       // 0x0e
	CONST_ENTRY("multipart/alternative")                      // 0x0f
	CONST_ENTRY("application/*")                              // 0x10
	CONST_ENTRY("application/java-vm")                        // 0x11
	CONST_ENTRY("application/x-www-form-urlencoded")          // 0x12
	CONST_ENTRY("application/x-hdmlc")                        // 0x13
	CONST_ENTRY("application/vnd.wap.wmlc")                   // 0x14
	CONST_ENTRY("application/vnd.wap.wmlscriptc")             // 0x15
	CONST_ENTRY("application/vnd.wap.wta-eventc")             // 0x16
	CONST_ENTRY("application/vnd.wap.uaprof")                 // 0x17
	CONST_ENTRY("application/vnd.wap.wtls-ca-certificate")    // 0x18
	CONST_ENTRY("application/vnd.wap.wtls-user-certificate")  // 0x19
	CONST_ENTRY("application/x-x509-ca-cert")                 // 0x1a
	CONST_ENTRY("application/x-x509-user-cert")               // 0x1b
	CONST_ENTRY("image/*")                                    // 0x1c
	CONST_ENTRY("image/gif")                                  // 0x1d
	CONST_ENTRY("image/jpeg")                                 // 0x1e
	CONST_ENTRY("image/tiff")                                 // 0x1f
	CONST_ENTRY("image/png")                                  // 0x20
	CONST_ENTRY("image/vnd.wap.wbmp")                         // 0x21
	CONST_ENTRY("application/vnd.wap.multipart.*")            // 0x22
	CONST_ENTRY("application/vnd.wap.multipart.mixed")        // 0x23
	CONST_ENTRY("application/vnd.wap.multipart.form-data")    // 0x24
	CONST_ENTRY("application/vnd.wap.multipart.byteranges")   // 0x25
	CONST_ENTRY("application/vnd.wap.multipart.alternative")  // 0x26
	CONST_ENTRY("application/xml")                            // 0x27
	CONST_ENTRY("text/xml")                                   // 0x28
	CONST_ENTRY("application/vnd.wap.wbxml")                  // 0x29
	CONST_ENTRY("application/x-x968-cross-cert")              // 0x2a
	CONST_ENTRY("application/x-x968-ca-cert")                 // 0x2b
	CONST_ENTRY("application/x-x968-user-cert")               // 0x2c
	CONST_ENTRY("text/vnd.wap.si")                            // 0x2d
	CONST_ENTRY("application/vnd.wap.sic")                    // 0x2e
	CONST_ENTRY("text/vnd.wap.sl")                            // 0x2f
	CONST_ENTRY("application/vnd.wap.slc")                    // 0x30
	CONST_ENTRY("text/vnd.wap.co")                            // 0x31
	CONST_ENTRY("application/vnd.wap.coc")                    // 0x32
	CONST_ENTRY("application/vnd.wap.multipart.related")      // 0x33
	CONST_ENTRY("application/vnd.wap.sia")                    // 0x34
	CONST_ENTRY("text/vnd.wap.connectivity-xml")              // 0x35
	CONST_ENTRY("application/vnd.wap.connectivity-wbxml")     // 0x36
	CONST_ENTRY("application/pkcs7-mime")                     // 0x37
	CONST_ENTRY("application/vnd.wap.hashed-certificate")     // 0x38
	CONST_ENTRY("application/vnd.wap.signed-certificate")     // 0x39
	CONST_ENTRY("application/vnd.wap.cert-response")          // 0x3a
	CONST_ENTRY("application/xhtml+xml")                      // 0x3b
	CONST_ENTRY("application/wml+xml")                        // 0x3c
	CONST_ENTRY("text/css")                                   // 0x3d
	CONST_ENTRY("application/vnd.wap.mms-message")            // 0x3e
	CONST_ENTRY("application/vnd.wap.rollover-certificate")   // 0x3f
	CONST_ENTRY("application/vnd.wap.locc+wbxml")             // 0x40
	CONST_ENTRY("application/vnd.wap.loc+xml")                // 0x41
	CONST_ENTRY("application/vnd.syncml.dm+wbxml")            // 0x42
	CONST_ENTRY("application/vnd.syncml.dm+xml")              // 0x43
	CONST_ENTRY("application/vnd.syncml.notification")        // 0x44
	CONST_ENTRY("application/vnd.wap.xhtml+xml")              // 0x45
	CONST_ENTRY("application/vnd.wv.csp.cir")                 // 0x46
	CONST_ENTRY("application/vnd.oma.dd+xml")                 // 0x47
	CONST_ENTRY("application/vnd.oma.drm.message")            // 0x48
	CONST_ENTRY("application/vnd.oma.drm.content")            // 0x49
	CONST_ENTRY("application/vnd.oma.drm.rights+xml")         // 0x4a
	CONST_ENTRY("application/vnd.oma.drm.rights+wbxml")       // 0x4b
CONST_END(binary_mpp_wellknowncontenttype)

PREFIX_CONST_ARRAY(static, const char*, binary_mpp_wellknownparameter, mime)
	CONST_ENTRY("q")                  // 0x00
	CONST_ENTRY("charset")            // 0x01
	CONST_ENTRY("level")              // 0x02
	CONST_ENTRY("type")               // 0x03
	CONST_ENTRY(0)                    // 0x04
	CONST_ENTRY("name")               // 0x05
	CONST_ENTRY("filename")           // 0x06
	CONST_ENTRY("differences")        // 0x07
	CONST_ENTRY("padding")            // 0x08
	CONST_ENTRY("type")               // 0x09
	CONST_ENTRY("start")              // 0x0a
	CONST_ENTRY("start-info")         // 0x0b
	CONST_ENTRY("comment")            // 0x0c
	CONST_ENTRY("domain")             // 0x0d
	CONST_ENTRY("max-age")            // 0x0e
	CONST_ENTRY("path")               // 0x0f
	CONST_ENTRY("secure")             // 0x10
	CONST_ENTRY("SEC")                // 0x11
	CONST_ENTRY("MAC")                // 0x12
	CONST_ENTRY("creation-date")      // 0x13
	CONST_ENTRY("modification-date")  // 0x14
	CONST_ENTRY("read-date")          // 0x15
	CONST_ENTRY("size")               // 0x16
	CONST_ENTRY("name")               // 0x17
	CONST_ENTRY("filename")           // 0x18
	CONST_ENTRY("start")              // 0x19
	CONST_ENTRY("start-info")         // 0x1a
	CONST_ENTRY("comment")            // 0x1b
	CONST_ENTRY("domain")             // 0x1c
	CONST_ENTRY("path")               // 0x1d
CONST_END(binary_mpp_wellknownparameter)


BinaryMultiPartParser::BinaryMultiPartParser (MultiPartParser_Listener* listener)
	: AbstractMultiPartParser(listener)
	, contentLength(0)
{
	WellknownHeaderName       = g_binary_mpp_wellknownheadername;
	WellknownCacheControl     = g_binary_mpp_wellknowncachecontrol;
	WellknownContentType      = g_binary_mpp_wellknowncontenttype;
	WellknownParameter        = g_binary_mpp_wellknownparameter;
	MaxWellknownHeaderName    = (unsigned int)CONST_ARRAY_SIZE(mime, binary_mpp_wellknownheadername);
	MaxWellknownCacheControl  = (unsigned int)CONST_ARRAY_SIZE(mime, binary_mpp_wellknowncachecontrol);
	MaxWellknownContentType   = (unsigned int)CONST_ARRAY_SIZE(mime, binary_mpp_wellknowncontenttype);
	MaxWellknownParameter     = (unsigned int)CONST_ARRAY_SIZE(mime, binary_mpp_wellknownparameter);
}

BinaryMultiPartParser::~BinaryMultiPartParser ()
{
}

BOOL BinaryMultiPartParser::parse () {
	// Preconditions
	OP_ASSERT(parserOffset <= bufferLength);   // We cannot parse past end of buffer.
	OP_ASSERT(returnedOffset <= parserOffset); // We cannot return more than has been parsed.

	BOOL progress = TRUE; // TRUE indicates success (i.e. something was parsed correctly and parserOffset was moved).
	ParseResult result = PARSE_RESULT_OK;

	switch (state) {
		case STATE_BEFORE_MULTIPART: {
			result = parseMultipartHeader();
			break;
		}
		case STATE_FINISHED_PART: {
			result = parsePartHeader(); // Try to parse next part's headers
			if (state == STATE_INSIDE_PART) { // We have parsed the header
				OP_ASSERT(!parts.Empty()); // Part should have been added to parts list.
				OP_ASSERT(parts.Last()->num == currentPartNum); // Added part should have correct part number.
			}
			break;
		}
		case STATE_INSIDE_PART: {
			result = parsePartContent();

			if (!stopParsing(result)) {	// Progress was made.
				OP_ASSERT(result == PARSE_RESULT_OK);
			}
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

	if (progress && stopParsing(result)) progress = FALSE; // Set proper return value if parsing failed.

	if (finishedLoading) { // No more incoming data
		if (parserOffset == bufferLength) { // All data has been parsed.
			if (state == STATE_INSIDE_PART) { // All data has been parsed, but we're in the middle of a part. This is not right.
				state = STATE_FAILURE;
				warn(WARNING_UNFINISHED_PART);

				// Also alter dataLength of current part, so that it does not exceed multipart document.
				if (!parts.Empty()) { // Current part is still in parts list.
					Part *p = parts.Last(); // Retrieve current part.
					OP_ASSERT(p->num == currentPartNum); // Ensure this is the current part.
					OP_ASSERT(p->dataLengthDetermined);  // This should always be set for binary multipart documents.
					OP_ASSERT(p->dataOffset >= bufferOffset + returnedOffset); // Ensure part's data starts within valid buffer.
					OP_ASSERT(p->dataOffset <= bufferOffset + parserOffset); // Ensure part's data starts within valid buffer.
					OP_ASSERT(bufferOffset + parserOffset <= p->dataOffset + p->dataLength); // We're parsing somewhere within current part.
					unsigned int diff = (p->dataOffset + p->dataLength) - (bufferOffset + parserOffset); // Diff between end of last part, and end of multipart.
					OP_ASSERT(p->dataLength >= diff); // Ensure that p->dataLength don't end up negative after subtracting the difference.
					p->dataLength -= diff; // Subtract difference.
					OP_ASSERT(bufferOffset + parserOffset == p->dataOffset + p->dataLength); // Current part now ends at end of multipart document.
				}
			}
			else if (state == STATE_BEFORE_MULTIPART || state == STATE_FINISHED_PART) { // No more data, and we're before/between parts. Time to finish.
				state = STATE_FINISHED_MULTIPART;
			}
		}
		else if (!progress) { // There is more data left, but there was no progress. FAILURE!
			state = STATE_FAILURE;
			warn(WARNING_PARSING_ERROR);
		}
	}

	// Postconditions
	OP_ASSERT(parserOffset <= bufferLength);   // We cannot parse past end of buffer.
	OP_ASSERT(returnedOffset <= parserOffset); // We cannot return more than has been parsed.
	return progress;
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parseMultipartHeader()
{
	// Preconditions
	OP_ASSERT(state == STATE_BEFORE_MULTIPART);
	OP_ASSERT(parserOffset == 0 || parserOffset == 2);
	OP_ASSERT(returnedOffset == 0);

	if (bufferLength < 2 && !finishedLoading) return PARSE_RESULT_MORE_DATA;
	if (parserOffset == 0 && bufferLength >= 2 && buffer[0] == (char)0x01 && buffer[1] == (char)0xa3) {
		parserOffset = 2; // Ignore magic cookie file prefix (or whatever this is)
	}

	// Multipart document header consists of nEntries of type uintvar.
	unsigned int numParts; // This value is deprecated in WSP v1.3, but it still must be parsed.
	ParseResult result = parseUintvar(parserOffset, numParts);
	if      (result == PARSE_RESULT_OK)      state = STATE_FINISHED_PART; // Think of this as "Finished part #0"
	else if (result == PARSE_RESULT_FAILURE) state = STATE_FAILURE;

	return result;
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parsePartHeader()
{
	// Preconditions
	OP_ASSERT(state == STATE_FINISHED_PART); // Must have finished previous part
	OP_ASSERT(parserOffset <= bufferLength);

	unsigned int oldOffset = parserOffset;
	unsigned int headersLen = 0, dataLen = 0;
	HeaderList *headers = OP_NEW(HeaderList, (KeywordIndex_HTTP_MIME)); // This will be filled with the headers we parse.
	if (!headers) {
		warn(WARNING_OUT_OF_MEMORY);
		return PARSE_RESULT_FAILURE;
	}
	ParseResult                    result = parseUintvar(parserOffset, headersLen);
	if (result == PARSE_RESULT_OK) result = parseUintvar(parserOffset, dataLen);
	if (result == PARSE_RESULT_OK && parserOffset + headersLen > bufferLength) result = PARSE_RESULT_MORE_DATA; // No need to continue if length of headers exceeds available data.
	unsigned int headerOffset = parserOffset;
	if (result == PARSE_RESULT_OK && headersLen > 0) result = parseContentType(parserOffset, *headers, headersLen);
	unsigned int remainingLength = headersLen - (parserOffset - headerOffset); // headersLen - length of Content-Type = length of remaining headers.
	if (result == PARSE_RESULT_OK && remainingLength > 0) result = parseHeaders(parserOffset, *headers, remainingLength);

	if (!stopParsing(result) && parserOffset != headerOffset + headersLen) {
		warn(WARNING_PARSING_ERROR);
		result = PARSE_RESULT_FAILURE;
	}

	if (!stopParsing(result)) {
		// Successfully parsed part headers. Create new Part object
		state = STATE_INSIDE_PART;
		contentLength = dataLen;
		++currentPartNum;
		Part *p = OP_NEW(Part, (currentPartNum, headers, parserOffset + bufferOffset, TRUE, contentLength));
		if (!p) {
			warn(WARNING_OUT_OF_MEMORY);
			OP_DELETE(headers);
			state = STATE_FAILURE;
			return PARSE_RESULT_FAILURE;
		}
		if (parts.Empty()) {
			// Also update returnedOffset to start of data portion in this part.
			OP_ASSERT(p->dataOffset >= bufferOffset);
			OP_ASSERT(returnedOffset <= p->dataOffset - bufferOffset);
			OP_ASSERT(parserOffset >= p->dataOffset - bufferOffset);
			returnedOffset = p->dataOffset - bufferOffset;
		}
		p->Into(&parts);
		return PARSE_RESULT_OK;
	}
	else if (result == PARSE_RESULT_MORE_DATA) {
		// Abort and retry when we get more data.
		OP_DELETE(headers);
		parserOffset = oldOffset;
		return PARSE_RESULT_MORE_DATA;
	}
	else {
		// Unrecoverable failure.
		OP_ASSERT(result == PARSE_RESULT_FAILURE);
		OP_DELETE(headers);
		state = STATE_FAILURE;
		return PARSE_RESULT_FAILURE;
	}
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parsePartContent()
{
	// Preconditions
	OP_ASSERT(state == STATE_INSIDE_PART);
	OP_ASSERT(parserOffset <= bufferLength);

	// Incremented parserOffset until all the content of the current part has been swallowed.
	// When we reach the start of the next part, switch state to STATE_FINISHED_PART.
	unsigned int incParse = MIN(contentLength, bufferLength - parserOffset);
	parserOffset += incParse;
	contentLength -= incParse;

	if (contentLength == 0) state = STATE_FINISHED_PART; // No more content in this part, Trigger switch to next part.
	return contentLength == 0 ? PARSE_RESULT_OK : PARSE_RESULT_MORE_DATA;
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parseContentType (unsigned int& offset, HeaderList& headers, unsigned int maxLength)
{
	// Preconditions
	OP_ASSERT(offset <= bufferLength);
	OP_ASSERT(offset + maxLength <= bufferLength);

	if (offset >= bufferLength) return PARSE_RESULT_MORE_DATA;
	unsigned int o = offset;

	StringBuffer value;
	ParseResult result = parseWellknownHeaderValue(o, 0x11, maxLength, value);
	if (stopParsing(result)) return result;
	if (value.empty()) { // No supported Content-Type found
		warn(WARNING_OUT_OF_MEMORY);
		return PARSE_RESULT_FAILURE;
	}

	HeaderEntry *h = OP_NEW(HeaderEntry, ());
	if (!h) {
		warn(WARNING_OUT_OF_MEMORY);
		return PARSE_RESULT_FAILURE;
	}
	OP_STATUS err = OpStatus::OK;
	TRAP(err, h->ConstructFromNameAndValueL(headers, "Content-Type", value.string(), value.length()));
	if (!OpStatus::IsSuccess(err)) {
		warn(WARNING_INVALID_HEADER);
		OP_DELETE(h);
		return PARSE_RESULT_FAILURE;
	}

	offset = o;
	return PARSE_RESULT_OK;
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parseHeaders (unsigned int& offset, HeaderList& headers, unsigned int length)
{
	// Preconditions
	OP_ASSERT(offset <= bufferLength);
	OP_ASSERT(offset + length <= bufferLength);

	unsigned int endOffset = offset + length;
	if (endOffset > bufferLength) return PARSE_RESULT_MORE_DATA;

	unsigned int o = offset;
	while (o < endOffset) {
		ParseResult result = parseHeader(o, endOffset - o, headers);
		if (stopParsing(result)) return result;
	}

	offset = o;
	return PARSE_RESULT_OK;
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parseHeader (unsigned int& offset, unsigned int maxLength, HeaderList& headers)
{
	// Preconditions
	OP_ASSERT(offset <= bufferLength);
	OP_ASSERT(offset + maxLength <= bufferLength);
	OP_ASSERT(maxLength > 0);

	if (offset >= bufferLength) return PARSE_RESULT_MORE_DATA;
	unsigned int o = offset;
	unsigned int endOffset = offset + maxLength;
	unsigned int token = buffer[o]; // Take a peek at token to filter away code page shifts.
	if (token <= 31) { // Short-cut-shift-delimiter
		warn(WARNING_UNSUPPORTED_CONTENT);
		offset = o + 1;
		return PARSE_RESULT_OK;
	}
	else if (token == 127) { // Shift-delimiter
		if (o >= bufferLength - 1) return PARSE_RESULT_MORE_DATA;
		warn(WARNING_UNSUPPORTED_CONTENT);
		offset = o + 2;
		return PARSE_RESULT_OK;
	}

	StringBuffer name, value;

	ParseResult result = parseToken(o, token);
	OP_ASSERT(result != PARSE_RESULT_IS_LENGTH);
	if (result == PARSE_RESULT_IS_STRING) { // Header is of type Application-Header
		result = parseString(o, endOffset - o, name);
		if (stopParsing(result)) return result;
		// Header value must be of type Application-specific-value, i.e. text string.
		result = parseToken(o, token);
		if (result == PARSE_RESULT_IS_WKNOWN) {
			warn(WARNING_PARSING_ERROR);
			return PARSE_RESULT_FAILURE;
		}
		else if (result == PARSE_RESULT_IS_LENGTH) {
			if (token > endOffset - o) {
				warn(WARNING_PARSING_ERROR);
				return PARSE_RESULT_FAILURE;
			}
			result = parseMultipleOctets(o, token, value);
			if (result != PARSE_RESULT_OK) return result;
		}
		else if (result == PARSE_RESULT_IS_STRING) {
			result = parseString(o, endOffset - o, value);
			if (result != PARSE_RESULT_OK) return result;
		}
		else return result;
	}
	else if (result == PARSE_RESULT_IS_WKNOWN) { // Header is of type Well-known-header
		if (token > MaxWellknownHeaderName) {
			warn(WARNING_UNSUPPORTED_CONTENT);
			token = 0xff;
		}
		else {
			OP_ASSERT(name.empty());
			name = StringBuffer(WellknownHeaderName[token]);
		}

		result = parseWellknownHeaderValue(o, token, endOffset - o, value);
		if (result != PARSE_RESULT_OK) return result;
		if (name.empty() || value.empty()) {
			warn(WARNING_UNSUPPORTED_CONTENT);
			offset = o;
			return PARSE_RESULT_OK;
		}
	}
	else if (result == PARSE_RESULT_IS_LENGTH) { // Header CANNOT start with length value.
		warn(WARNING_PARSING_ERROR);
		return PARSE_RESULT_FAILURE;
	}
	else {
		OP_ASSERT(stopParsing(result));
		return result;
	}
	// name and value should now be set to the appropriate header name and header value, respectively.
	// o should be the offset of the next byte (following this header) in buffer.

	if (o > endOffset) {
		warn(WARNING_PARSING_ERROR);
		return PARSE_RESULT_FAILURE;
	}

	// Ensure that header name is null-terminated.
	if (!name.nullTerminate()) {
		warn(WARNING_OUT_OF_MEMORY);
		return PARSE_RESULT_FAILURE;
	}

	// Add header name/value to list of headers.
	HeaderEntry *h = OP_NEW(HeaderEntry, ());
	if (!h) {
		warn(WARNING_OUT_OF_MEMORY);
		return PARSE_RESULT_FAILURE;
	}
	OP_STATUS err = OpStatus::OK;
	TRAP(err, h->ConstructFromNameAndValueL(headers, name.string(), value.string(), value.length()));
	if (!OpStatus::IsSuccess(err)) {
		warn(WARNING_INVALID_HEADER);
		OP_DELETE(h);
		return PARSE_RESULT_FAILURE;
	}

	offset = o;
	return PARSE_RESULT_OK;
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parseWellknownHeaderValue (unsigned int& offset, unsigned int nameToken, unsigned int maxLength, StringBuffer& value)
{
	// Preconditions
	OP_ASSERT(offset <= bufferLength);
	OP_ASSERT(offset + maxLength <= bufferLength);
	OP_ASSERT(maxLength > 0);
	OP_ASSERT(value.empty());

	unsigned int o = offset;
	unsigned int token;
	ParseResult result = parseToken(o, token);
	if (result == PARSE_RESULT_IS_STRING) { // This is easy, parse the string and return.
		result = parseString(o, maxLength, value);
		if (stopParsing(result)) return result;
	}
	else if (result == PARSE_RESULT_IS_WKNOWN) { // There is only one simple value with no parameters.
		value = StringBuffer(getSingleWellknownHeaderValue(nameToken, token));
		if (value.empty()) {
			warn(WARNING_INVALID_HEADER);
		}
	}
	else if (result == PARSE_RESULT_IS_LENGTH) { // This is more complicated. There may be more complex data types and parameters involved.
		if (o + token > bufferLength) return PARSE_RESULT_MORE_DATA;
		if (token == 0) {
			warn(WARNING_INVALID_HEADER);
			return PARSE_RESULT_FAILURE; // Token indicates length, which MUST be > 0
		}
		result = parseWellknownCompositeHeaderValue(o, nameToken, token, value);
		if (stopParsing(result)) return result;
	}
	else { // Parsing error
		OP_ASSERT(stopParsing(result));
		return result;
	}

	offset = o;
	return PARSE_RESULT_OK;
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parseWellknownCompositeHeaderValue (unsigned int& offset, unsigned int nameToken, unsigned int length, StringBuffer& value)
{
	// Preconditions
	OP_ASSERT(offset <= bufferLength);
	OP_ASSERT(offset + length <= bufferLength);
	OP_ASSERT(length > 0);
	OP_ASSERT(value.empty());

	unsigned int o = offset;

	if (nameToken > MaxWellknownHeaderName) {
		warn(WARNING_UNSUPPORTED_CONTENT);
		StringBuffer dummy;
		return parseMultipleOctets(offset, length, dummy);
	}

	ParseResult result = PARSE_RESULT_OK;
	switch (nameToken) {
		case 0x08:       // Cache-Control
		case 0x3D:       // Cache-Control
		case 0x47: {     // Cache-Control
			// Cache-directive starts at offset
			result = parseCacheDirective(o, length, value);
			break;
		}
		case 0x11: {     // Content-Type
			// Media-type starts at offset
			result = parseMediaType(o, length, value);
			break;
		}
		case 0x1B:       // If-Unmodified-Since
		case 0x1D:       // Last Modified
		case 0x12:       // Date
		case 0x14:       // Expires
		case 0x17:       // If-Modified-Since
		case 0x3F: {     // X-Wap-Tod
			// Date-value starts at offset
			result = parseDateValue(o, length, value);
			break;
		}
	}
	if (stopParsing(result)) {
		// Ignore errors, since we know how to skip. Just ensure that the skipping code below will trigger.
		OP_ASSERT(value.empty());
	}

	if (value.empty()) {
		warn(WARNING_UNSUPPORTED_CONTENT);
		StringBuffer dummy;
		return parseMultipleOctets(offset, length, dummy);
	}

	offset = o;
	return PARSE_RESULT_OK;
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parseMediaType (unsigned int& offset, unsigned int length, StringBuffer& value)
{
	// Preconditions
	OP_ASSERT(offset <= bufferLength);
	OP_ASSERT(offset + length <= bufferLength);
	OP_ASSERT(length > 0);
	OP_ASSERT(value.empty());

	unsigned int o = offset;
	unsigned int endOffset = offset + length;
	unsigned int token;
	ParseResult result = parseToken(o, token);

	if (result == PARSE_RESULT_IS_STRING) { // Extension-Media
		result = parseString(o, endOffset - o, value);
		if (stopParsing(result)) return result;
	}
	else if (result == PARSE_RESULT_IS_WKNOWN) { // Well-known-media -> Integer-value -> Short-integer
		// Short-integer interpreted as well-known Content-Type value.
		value = StringBuffer(getSingleWellknownHeaderValue(0x11, token));
	}
	else if (result == PARSE_RESULT_IS_LENGTH) { // Well-known-media -> Integer-value -> Long-integer, length of multi-octet integer is stored in token
		if (token > endOffset - o) {
			warn(WARNING_PARSING_ERROR);
			return PARSE_RESULT_FAILURE;
		}
		unsigned long v; // Hold value temporarily
		result = parseMultiOctetInteger(o, token, v);
		if (stopParsing(result)) return result;

		value = StringBuffer(getSingleWellknownHeaderValue(0x11, v));
	}
	else { // Parsing error
		OP_ASSERT(stopParsing(result));
		return result;
	}

	if (value.empty()) {
		warn(WARNING_PARSING_ERROR);
		return PARSE_RESULT_FAILURE;
	}

	// First part of Media-type successfully decoded. Now parse the 0 or more following Parameters.
	StringBuffer paramString;
	while (o < endOffset) {
		StringBuffer param;
		result = parseParameter(o, endOffset - o, param);
		if (stopParsing(result)) return result;
		if (!paramString.append("; ") ||
			!paramString.append(param))
		{
			warn(WARNING_OUT_OF_MEMORY);
			return PARSE_RESULT_FAILURE;
		}
	}
	if (!value.append(paramString)) {
		warn(WARNING_OUT_OF_MEMORY);
		return PARSE_RESULT_FAILURE;
	}

	OP_ASSERT(o == endOffset);
	OP_ASSERT(!value.empty());
	offset = o;
	return PARSE_RESULT_OK;
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parseParameter (unsigned int& offset, unsigned int maxLength, StringBuffer& value)
{
	// Preconditions
	OP_ASSERT(offset <= bufferLength);
	OP_ASSERT(offset + maxLength <= bufferLength);
	OP_ASSERT(maxLength > 0);
	OP_ASSERT(value.empty());

	unsigned int o = offset;
	unsigned int endOffset = offset + maxLength;
	unsigned int token;
	StringBuffer paramValue;
	ParseResult result = parseToken(o, token);

	if (result == PARSE_RESULT_IS_STRING) { // Parameter -> Untyped-parameter -> Token-text
		result = parseString(o, endOffset - o, value);
		if (stopParsing(result)) return result;
		// Untyped-parameter is followed by Untyped-value.
		result = parseUntypedValue(o, endOffset - o, paramValue);
		if (stopParsing(result)) return result;
	}
	else if (result == PARSE_RESULT_IS_WKNOWN) { // Parameter -> Typed-parameter -> Well-known-parameter-token -> Integer-value -> Short-integer
		if (token > MaxWellknownParameter) {
			warn(WARNING_PARSING_ERROR);
			return PARSE_RESULT_FAILURE;
		}
		value = StringBuffer(WellknownParameter[token]);
		if (value.empty()) {
			warn(WARNING_PARSING_ERROR);
			return PARSE_RESULT_FAILURE;
		}
		// Well-known-parameter-token is followed by Typed-value.
		result = parseTypedValue(o, endOffset - o, token, paramValue);
		if (stopParsing(result)) return result;
	}
	else if (result == PARSE_RESULT_IS_LENGTH) { // Parameter -> Typed-parameter -> Well-known-parameter-token -> Integer-value -> Long-integer
		if (token > endOffset - o) {
			warn(WARNING_PARSING_ERROR);
			return PARSE_RESULT_FAILURE;
		}
		unsigned long v;
		result = parseMultiOctetInteger(o, endOffset - o, v);
		if (stopParsing(result)) return result;
		if (v > MaxWellknownParameter) {
			warn(WARNING_PARSING_ERROR);
			return PARSE_RESULT_FAILURE;
		}
		value = StringBuffer(WellknownParameter[v]);
		if (value.empty()) {
			warn(WARNING_PARSING_ERROR);
			return PARSE_RESULT_FAILURE;
		}
		// Well-known-parameter-token is followed by Typed-value.
		result = parseTypedValue(o, endOffset - o, v, paramValue);
		if (stopParsing(result)) return result;
	}
	else { // Parsing error
		OP_ASSERT(stopParsing(result));
		return result;
	}

	OP_ASSERT(!value.empty());
	if (!value.takeOwnership(value.length() + 1 + paramValue.length() + 1) ||
		!value.append("=") ||
		!value.append(paramValue))
	{
		warn(WARNING_OUT_OF_MEMORY);
		return PARSE_RESULT_FAILURE;
	}

	offset = o;
	return PARSE_RESULT_OK;
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parseCacheDirective (unsigned int& offset, unsigned int length, StringBuffer& value)
{
	// Preconditions
	OP_ASSERT(offset <= bufferLength);
	OP_ASSERT(offset + length <= bufferLength);
	OP_ASSERT(length > 0);
	OP_ASSERT(value.empty());

	unsigned int o = offset;
	unsigned int endOffset = offset + length;
	unsigned int token;
	ParseResult result = parseToken(o, token);

	if (result == PARSE_RESULT_IS_STRING) { // Cache-extension followed by a parameter (Untyped-value)
		result = parseString(o, endOffset - o, value);
		if (stopParsing(result)) return result;
		StringBuffer param;
		result = parseUntypedValue(o, endOffset - o, param);
		if (stopParsing(result)) return result;
		if (param.empty()) {
			warn(WARNING_PARSING_ERROR);
			return PARSE_RESULT_FAILURE;
		}

		// Concatenate value and param into new value string.
		if (!value.takeOwnership(value.length() + 1 + param.length() + 1) ||
			!value.append("=") ||
		    !value.append(param))
		{
			warn(WARNING_OUT_OF_MEMORY);
			return PARSE_RESULT_FAILURE;
		}
	}
	else if (result == PARSE_RESULT_IS_WKNOWN) { // A well-known cache control value is followed by a parameter
		value = StringBuffer(getSingleWellknownHeaderValue(0x47, token));
		if (value.empty()) {
			warn(WARNING_PARSING_ERROR);
			return PARSE_RESULT_FAILURE;
		}

		// Append the following parameter interpreted in the context of the parsed well-known cache control value.
		switch (token) {
			case 0x00:   // no-cache
			case 0x07: { // private
				// Following parameter is one or more Field-names.
				StringBuffer param;
				BOOL firsttime = TRUE;
				while (o < endOffset) { // There are more field names to parse.
					StringBuffer fieldname;
					result = parseFieldname(o, endOffset - o, fieldname);
					if (stopParsing(result)) return result;
					if ((firsttime && !param.append("=\"")) ||
						(!firsttime && !param.append(", ")) ||
						!param.append(fieldname))
					{
						warn(WARNING_OUT_OF_MEMORY);
						return PARSE_RESULT_FAILURE;
					}
					firsttime = FALSE;
				}
				if ((!firsttime && !param.append("\"")) ||
					!value.append(param))
				{
					warn(WARNING_OUT_OF_MEMORY);
					return PARSE_RESULT_FAILURE;
				}
				break;
			}
			case 0x02:   // max-age
			case 0x03:   // max-stale
			case 0x04:   // min-fresh
			case 0x0b: { // s-maxage
				// Following parameter is a Delta-second-value.
				StringBuffer delta;
				result = parseUntypedValue(o, endOffset - o, delta); // Delta-second-value is same as Untyped-value, except for the lack of Text-value.
				if (stopParsing(result)) return result;
				if (!value.takeOwnership(value.length() + 1 + delta.length() + 1) ||
					!value.append("=") ||
					!value.append(delta))
				{
					warn(WARNING_OUT_OF_MEMORY);
					return PARSE_RESULT_FAILURE;
				}
				break;
			}
			default: {   // Invalid parameter construct
				warn(WARNING_PARSING_ERROR);
				return PARSE_RESULT_FAILURE;
			}
		}
	}
	else if (result == PARSE_RESULT_IS_LENGTH) { // INVALID
		warn(WARNING_PARSING_ERROR);
		return PARSE_RESULT_FAILURE;
	}
	else { // Parsing error
		OP_ASSERT(stopParsing(result));
		return result;
	}

	OP_ASSERT(!value.empty());
	offset = o;
	return PARSE_RESULT_OK;
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parseUntypedValue (unsigned int& offset, unsigned int length, StringBuffer& value)
{
	// Preconditions
	OP_ASSERT(offset <= bufferLength);
	OP_ASSERT(offset + length <= bufferLength);
	OP_ASSERT(length > 0);
	OP_ASSERT(value.empty());

	unsigned int o = offset;
	unsigned int endOffset = offset + length;
	unsigned int token;
	ParseResult result = parseToken(o, token);

	if (result == PARSE_RESULT_IS_STRING) { // Text-value
		result = parseString(o, endOffset - o, value);
		if (stopParsing(result)) return result;
	}
	else if (result == PARSE_RESULT_IS_WKNOWN) { // Short-integer
		const unsigned int MAX_DIGITS = (sizeof(unsigned int) * 8 * 11) / 30 + 1; // Rough approximation of # decimal digits needed to represent an unsigned int.
		char *tmpValue = OP_NEWA(char, MAX_DIGITS + 1);
		if (!tmpValue) {
			warn(WARNING_OUT_OF_MEMORY);
			return PARSE_RESULT_FAILURE;
		}
		unsigned int valueLength = op_snprintf(tmpValue, MAX_DIGITS + 1, "%u", token);
		OP_ASSERT(valueLength <= MAX_DIGITS);
		value.attach(tmpValue, valueLength, MAX_DIGITS + 1);
	}
	else if (result == PARSE_RESULT_IS_LENGTH) { // Long-integer, length of multi-octet integer is stored in token
		if (token > endOffset - o) {
			warn(WARNING_PARSING_ERROR);
			return PARSE_RESULT_FAILURE;
		}
		unsigned long v; // Hold value temporarily, before formatting as string
		result = parseMultiOctetInteger(o, endOffset - o, v);
		if (stopParsing(result)) return result;

		const unsigned int MAX_DIGITS = (sizeof(unsigned long) * 8 * 11) / 30 + 1; // Rough approximation of # decimal digits needed to represent an unsigned long.
		char *tmpValue = OP_NEWA(char, MAX_DIGITS + 1);
		if (!tmpValue) {
			warn(WARNING_OUT_OF_MEMORY);
			return PARSE_RESULT_FAILURE;
		}
		unsigned int valueLength = op_snprintf(tmpValue, MAX_DIGITS + 1, "%lu", v);
		OP_ASSERT(valueLength <= MAX_DIGITS);
		value.attach(tmpValue, valueLength, MAX_DIGITS + 1);
	}
	else { // Parsing error
		OP_ASSERT(stopParsing(result));
		return result;
	}

	OP_ASSERT(!value.empty());
	offset = o;
	return PARSE_RESULT_OK;
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parseTypedValue (unsigned int& offset, unsigned int length, unsigned int paramToken, StringBuffer& value)
{
	// Preconditions
	OP_ASSERT(offset <= bufferLength);
	OP_ASSERT(offset + length <= bufferLength);
	OP_ASSERT(length > 0);
	OP_ASSERT(value.empty());

	unsigned int o = offset;
	unsigned int endOffset = offset + length;
	unsigned int token;
	ParseResult result = parseToken(o, token);

	if (result == PARSE_RESULT_IS_STRING) { // Typed-value -> Text-value
		result = parseString(o, endOffset - o, value);
		if (stopParsing(result)) return result;
	}
	else if (result == PARSE_RESULT_IS_WKNOWN || result == PARSE_RESULT_IS_LENGTH) { // Typed-value -> Compact-value
		o = offset; // Reset o back to start of typed value.
		result = parseWellknownParam(paramToken, o, endOffset - o, value);
		if (stopParsing(result)) return result;
	}
	else { // Parsing error
		OP_ASSERT(stopParsing(result));
		return result;
	}

	OP_ASSERT(!value.empty());
	offset = o;
	return PARSE_RESULT_OK;
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parseFieldname (unsigned int& offset, unsigned int maxLength, StringBuffer& value)
{
	// Preconditions
	OP_ASSERT(offset <= bufferLength);
	OP_ASSERT(offset + maxLength <= bufferLength);
	OP_ASSERT(maxLength > 0);
	OP_ASSERT(value.empty());

	unsigned int o = offset;
	unsigned int token;
	ParseResult result = parseToken(o, token);

	if (result == PARSE_RESULT_IS_STRING) { // This is easy, parse the string and return.
		result = parseString(o, maxLength, value);
		if (stopParsing(result)) return result;
	}
	else if (result == PARSE_RESULT_IS_WKNOWN) { // Well-known header name token
		if (token > MaxWellknownHeaderName) {
			warn(WARNING_PARSING_ERROR);
			return PARSE_RESULT_FAILURE;
		}
		value = StringBuffer(WellknownHeaderName[token]);
	}
	else if (result == PARSE_RESULT_IS_LENGTH) {
		warn(WARNING_PARSING_ERROR);
		return PARSE_RESULT_FAILURE;
	}
	else {
		OP_ASSERT(stopParsing(result));
		return result;
	}

	OP_ASSERT(!value.empty());
	offset = o;
	return PARSE_RESULT_OK;
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parseWellknownCharset (unsigned int& offset, unsigned int length, StringBuffer& value)
{
	// Preconditions
	OP_ASSERT(offset <= bufferLength);
	OP_ASSERT(offset + length <= bufferLength);
	OP_ASSERT(length > 0);
	OP_ASSERT(value.empty());

	unsigned int o = offset;
	unsigned int endOffset = offset + length;
	unsigned int token;
	ParseResult result = parseToken(o, token);
	unsigned long charsetToken = 0;

	if (result == PARSE_RESULT_IS_STRING) { // Hmm. String not really allowed here, but we'll let it pass.
		warn(WARNING_UNSUPPORTED_CONTENT);
		result = parseString(o, endOffset - o, value);
		if (stopParsing(result)) return result;
		OP_ASSERT(!value.empty());
		offset = o;
		return PARSE_RESULT_OK;
	}
	else if (result == PARSE_RESULT_IS_WKNOWN) { // Well-known-charset -> Integer-value -> Short-integer
		charsetToken = token;
	}
	else if (result == PARSE_RESULT_IS_LENGTH) { // Well-known-charset -> Integer-value -> Long-integer
		ParseResult result = parseMultiOctetInteger(o, length, charsetToken);
		if (stopParsing(result)) return result;
	}
	else {
		OP_ASSERT(stopParsing(result));
		return result;
	}

	// Convert charsetToken to string representation, utilising the fact that WSP well-known character set tokens follow the IANA MIBEnum values.
	const char *charset = g_charsetManager->GetCharsetNameFromMIBenum(charsetToken);
	if (!charset) {
		warn(WARNING_PARSING_ERROR);
		return PARSE_RESULT_FAILURE;
	}
	value = StringBuffer(charset);

	OP_ASSERT(!value.empty());
	offset = o;
	return PARSE_RESULT_OK;
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parseDateValue (unsigned int& offset, unsigned int length, StringBuffer& value)
{
	// Preconditions
	OP_ASSERT(offset <= bufferLength);
	OP_ASSERT(offset + length <= bufferLength);
	OP_ASSERT(length > 0);
	OP_ASSERT(value.empty());

	unsigned int o = offset;
#ifdef DEBUG_ENABLE_OPASSERT
	unsigned int endOffset = offset + length;
#endif
	unsigned long v;
	ParseResult result = parseMultiOctetInteger(o, length, v);
	if (stopParsing(result)) return result;
	OP_ASSERT(o == endOffset);
	// v now holds date value as #seconds since 1970-01-01, 00:00:00 GMT.
	// Convert to appropriate string representation.
	uni_char date_str[30]; /* ARRAY OK 2009-04-23 roarl */
	MakeDate3((time_t *) &v, date_str, 30);
	OpString date_utf16;
	date_utf16.Set(date_str, (int)uni_strlen(date_str));
	char *date_string;
	date_utf16.UTF8Alloc(&date_string);

	value.attach(date_string, (unsigned int)op_strlen(date_string), (unsigned int)op_strlen(date_string) + 1);
	offset = o;
	return PARSE_RESULT_OK;
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parseString (unsigned int& offset, unsigned int maxLength, StringBuffer& value)
{
	// Preconditions
	OP_ASSERT(offset <= bufferLength);
	OP_ASSERT(value.empty());

	unsigned int limitMaxLength = MIN(maxLength, bufferLength - offset); // Don't search past end of buffer
	// Find end of string. We could use memchr() here, but it's not available on all platforms (e.g. Synergy).
	BOOL found = FALSE;
	unsigned int o = offset;
	while (o < offset + limitMaxLength) {
		if (buffer[o] == '\0') {
			found = TRUE;
			break;
		}
		++o;
	}
	if (!found) {
		if (limitMaxLength < maxLength) return PARSE_RESULT_MORE_DATA; // We don't have enough data in buffer.
		else { // There's no null-terminator before maxLength.
			warn(WARNING_PARSING_ERROR);
			return PARSE_RESULT_FAILURE;
		}
	}
	// Null-terminator found
	ParseResult result = parseMultipleOctets(offset, o - offset, value);
	if (stopParsing(result)) return result;
	OP_ASSERT(buffer[offset] == '\0'); // offset should now point to null-terminator.
	++offset; // offset now points to value following null-terminator.
	return PARSE_RESULT_OK;
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parseMultipleOctets (unsigned int& offset, unsigned int length, StringBuffer& value)
{
	// Preconditions
	OP_ASSERT(offset <= bufferLength);
	OP_ASSERT(value.empty());

	if (offset + length > bufferLength) return PARSE_RESULT_MORE_DATA;
	value = StringBuffer(buffer + offset, length, bufferLength - offset);
	offset += length;
	return PARSE_RESULT_OK;
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parseWellknownParam(unsigned int paramToken, unsigned int& offset, unsigned int length, StringBuffer& value)
{
	switch (paramToken)
	{
		case 0x01: // Charset
			return parseWellknownCharset(offset, length, value); 
	
		case 0x03: // Type // Integer-value is a subset of Untyped-value.
		case 0x08: // Padding // Short-integer is a subset of Untyped-value.
		case 0x0E: // Max-Age // Delta-second-value is a subset of Untyped-value.
		case 0x11: // SEC // Short-integer is a subset of Untyped-value.
		case 0x16: // Size // Integer-value is a subset of Untyped-value.
			return parseUntypedValue(offset, length, value); 
	
		case 0x05: // Name
		case 0x06: // Filename
		case 0x0A: // Start
		case 0x0B: // Start-info
		case 0x0C: // Comment
		case 0x0D: // Domain
		case 0x0F: // Path
		case 0x10: // Secure
		case 0x12: // MAC
		case 0x17: // Name
		case 0x18: // Filename
		case 0x19: // Start
		case 0x1A: // Start-info
		case 0x1B: // Comment
		case 0x1C: // Domain
		case 0x1D: // Path
			return parseString(offset, length, value); 
	
		case 0x07: // Differences
			return parseFieldname(offset, length, value); 
	
		case 0x13: // Creation-date
		case 0x14: // Modification-date
		case 0x15: // Read-date
			return parseDateValue(offset, length, value); 
	
		case 0x00: // Q
			// return parseQValue(offset, length, value); 
		case 0x02: // Level
			// return parseVersionValue(offset, length, value);
		case 0x09: // Type
			// return parseConstrainedEncoding(offset, length, value); 
		case 0x04: // (Unused)
		default:
			warn(WARNING_PARSING_ERROR);
			return PARSE_RESULT_FAILURE;
	}
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parseToken (unsigned int& offset, unsigned int& value)
{
	// Preconditions
	OP_ASSERT(offset <= bufferLength);

	if (offset >= bufferLength) return PARSE_RESULT_MORE_DATA;
	unsigned int o = offset;
	UINT8 token = (UINT8) buffer[o++];
	ParseResult retval = PARSE_RESULT_OK;

	if (token < 31) { // Token is a length value
		value = token;
		retval = PARSE_RESULT_IS_LENGTH;
	}
	else if (token == 31) { // Token is followed by an uintvar length value
		ParseResult result = parseUintvar(o, value);
		if (stopParsing(result)) return result;
		retval = PARSE_RESULT_IS_LENGTH;
	}
	else if (token > 127) { // Token is a Well-known 7-bit encoded value.
		value = token & 0x7f; // strip 8th bit
		retval = PARSE_RESULT_IS_WKNOWN;
	}
	else { // Token is first char in a null-terminated text string.
		--o; // Step back one char
		retval = PARSE_RESULT_IS_STRING;
	}
	// value and retval are now set to the appropriate parsed value and return value respectively.

	offset = o;
	return retval;
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parseMultiOctetInteger (unsigned int& offset, unsigned int length, unsigned long& value)
{
	// Preconditions
	OP_ASSERT(offset <= bufferLength);

	if (offset + length > bufferLength) return PARSE_RESULT_MORE_DATA;
#ifdef DEBUG_ENABLE_OPASSERT
	unsigned int endOffset = offset + length;
#endif
	value = 0;
	while (length-- > 0) {
		value <<= 8;
		value += (UINT8) buffer[offset++];
	}
	OP_ASSERT(offset == endOffset);
	return PARSE_RESULT_OK;
}

BinaryMultiPartParser::ParseResult BinaryMultiPartParser::parseUintvar (unsigned int& offset, UINT32& value)
{
	// Preconditions
	OP_ASSERT(offset <= bufferLength);

	unsigned int o = offset;
	UINT32 result = 0;
	do {
		if (o >= bufferLength) return PARSE_RESULT_MORE_DATA;
		result <<= 7;
		result |= (UINT32)(buffer[o] & 0x7f);
	} while (buffer[o++] & 0x80);
	offset = o;
	value = result;
	return PARSE_RESULT_OK;
}

const char *BinaryMultiPartParser::getSingleWellknownHeaderValue(unsigned int nameToken, unsigned int valueToken)
{
	switch (nameToken) { // Which type of header
		case 0x06: {     // Allow
			switch (valueToken) {
				case 0x40: return "GET";
				case 0x41: return "OPTIONS";
				case 0x42: return "HEAD";
				case 0x43: return "DELETE";
				case 0x44: return "TRACE";
				case 0x60: return "POST";
				case 0x61: return "PUT";
			}
			break;
		}
		case 0x08:       // Cache-Control
		case 0x3D:       // Cache-Control
		case 0x47: {     // Cache-Control
			if (valueToken <= MaxWellknownCacheControl) return WellknownCacheControl[valueToken];
			break;
		}
		case 0x11: {     // Content-Type
			if (valueToken <= MaxWellknownContentType) return WellknownContentType[valueToken];
			break;
		}
	}
	return 0;
}

/* Implementation of StringBuffer */

void StringBuffer::attach (char *string, unsigned int length, unsigned int memLength)
{
	OP_ASSERT(empty());
	OP_ASSERT(mem == 0);
	mem = string;
	s = string;
	s_len = length;
	mem_len = MAX(length, memLength);
}

char *StringBuffer::release ()
{
	OP_ASSERT(mem != 0);
	char *retval = mem;
	mem = 0;
	return retval;
}

BOOL StringBuffer::append (const char *string, unsigned int length, unsigned int memLength)
{
	if (string == s + s_len) {
		// Given string is already located directly after current string.
		s_len += length;
		mem_len = MAX(mem_len, s_len);
		return TRUE;
	}
	// Reallocate
	if (!takeOwnership(s_len + length + 1)) return FALSE; // Small optimization: Automatically add null-terminator when reallocating.
	op_memcpy(mem + s_len, string, length);
	s_len += length;
	mem[s_len] = '\0'; // Small optimization: Automatically add null-terminator when reallocating.
	return TRUE;
}

BOOL StringBuffer::nullTerminate ()
{
	if (mem_len > s_len && s[s_len] == '\0') return TRUE; // Already satisfied
	for (unsigned int i = s_len; i > 0; --i) if (s[i - 1] == '\0') return TRUE; // There is a null terminator somewhere within s already. Already satisfied. Note: Should use memrchr() or memchr() instead, but those are not available on all platforms (e.g. Synergy)

	// Need to allocate null-terminated copy.
	if (!takeOwnership(s_len + 1)) return FALSE;
	mem[s_len] = '\0';
	return TRUE;
}

BOOL StringBuffer::takeOwnership (unsigned int size)
{
	if (owned() && mem_len >= size) return TRUE; // Already satisfied.
	unsigned int new_mem_len = MAX(size, s_len + 1); // Small optimization: Automatically add null-terminator when reallocating.
	char *new_mem = OP_NEWA(char, new_mem_len);
	if (!new_mem) return FALSE;
	op_memcpy(new_mem, s, s_len);
	new_mem[s_len] = '\0'; // Small optimization: Automatically add null-terminator when reallocating.
	if (owned()) OP_DELETEA(mem);
	mem = new_mem;
	mem_len = new_mem_len;
	s = mem;
	return TRUE;
}

#endif // WBMULTIPART_MIXED_SUPPORT
