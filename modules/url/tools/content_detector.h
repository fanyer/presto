/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef CONTENT_DETECTOR_H_
#define CONTENT_DETECTOR_H_

#define PATTERN_MAX_LENGTH 16

enum ContentDetectorSimpleType
{
	UNDETERMINED,
	UNKNOWN,
	TEXT,
	BINARY,
	IMAGE,
	VIDEO,
	FEED,
	XML,
	HTML
};

enum ContentDetectorTypeSecurity
{
	SCRIPTABLE,
	SAFE,
	NA
};

struct ContentDetectorPatternData
{
	unsigned int length;
	ContentDetectorTypeSecurity scriptable;
	URLContentType content_type;
	ContentDetectorSimpleType simple_type;
};

class ContentDetector
{
public:
	enum { DETERMINISTIC_HEADER_LENGTH = 512 }; ///< at most 512 bytes are used for fully deterministic content type detection.
	enum CompliancyType
	{
		IETF,
		WEB
	};

	ContentDetector(URL_Rep *url, BOOL dont_leave_undetermined, BOOL extension_use_allowed, CompliancyType compliancy_type = WEB);
	virtual ~ContentDetector();

	OP_STATUS DetectContentType();
	BOOL WasTheCheckDeterministic() { return m_deterministic; }

private:
	OP_STATUS IsTextOrBinary(URLContentType &url_type, OpStringC8 &mime_type);
	OP_STATUS IsUnknownType(URLContentType &url_type, OpStringC8 &mime_type);
	OP_STATUS IsImage(URLContentType &url_type, OpStringC8 &mime_type);
	OP_STATUS IsVideo(URLContentType &url_type, OpStringC8 &mime_type);
	OP_STATUS TryToFindByExtension(URLContentType &url_type, OpStringC8 &mime_type, BOOL &found);
	static BOOL HasBinaryOctet(const char *octets, unsigned long octets_length);
	static BOOL HasMP4Signature(const char *octets, unsigned long octets_length);
	static BOOL IsDisplayable(URLContentType url_type);
#ifdef NEED_URL_EXTERNAL_GET_MIME_FROM_SAMPLE
	BOOL CheckExternally();
#endif

	static ContentDetectorPatternData GetPatternData(int index);
	static void GetPattern(int index, unsigned char (&pattern)[PATTERN_MAX_LENGTH]);
	static void GetMask(int index, unsigned char (&mask)[PATTERN_MAX_LENGTH]);
	static const char *GetMimeType(int index);

	static OP_STATUS LookUpInSniffTable(const char *octets, unsigned long length, BOOL must_be_non_scriptable, BOOL check_with_mask,
		BOOL specific_type_only, ContentDetectorSimpleType specific_type, int &found_at_index);

	const char *m_octets;
	int m_length;
	BOOL m_deterministic;
	BOOL m_dont_leave_undetermined;
	BOOL m_extension_use_allowed;
	OpString m_official_mime_type;
	URLContentType m_official_content_type;
	CompliancyType m_compliancy_type;
	URL_Rep *m_url;
};


#endif /* CONTENT_DETECTOR_H_ */
