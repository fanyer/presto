/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#include "core/pch.h"

#if defined(SEARCH_ENGINE) && defined(SELFTEST)

#include "modules/search_engine/tests/MockLowLevelFile.h"
#include "modules/selftest/src/testutils.h"

char* Mock::Str(const char* fmt, ...) {
	int max_size = 256, n;
	char* p = OP_NEWA(char, max_size);
	va_list ap;

	va_start(ap, fmt);
	n = op_vsnprintf(p, max_size, fmt, ap);
	va_end(ap);

	if (n < 0 || n >= max_size)
		output("\nArgument buffer of size %d was too small!\n", max_size);
	return p;
}

void Mock::Fail() const {
	output("\nUnexpected method call\n");
	m_should_fail = TRUE;
}

void Mock::Expect(const char* method, char* arguments, char* returns) {
	if (m_num_expectations >= m_max_expectations) {
		output("\nToo many expectations.\n");
		m_should_fail = TRUE;
		return;
	}
	Expectation* e = &m_expectations[m_num_expectations++];
	e->method = method;
	e->arguments = arguments;
	e->returns = returns;
}

void Mock::Expect(const char* method, char* arguments, OP_STATUS returned_status) {
	Expect(method, arguments, Str("%d", returned_status));
}

BOOL Mock::CheckExpectation(const char* method, char* arguments, const char* returns_fmt, ...) const {
	BOOL success = TRUE;
	int i;
	for (i=0; i<m_num_expectations; i++) {
		Expectation* e = &m_expectations[i];
		if (e->method != NULL &&
			op_strcmp(e->method, method) == 0 &&
			((arguments==NULL && e->arguments==NULL) ||
			 (arguments!=NULL && e->arguments!=NULL && op_strcmp(e->arguments, arguments) == 0)))
		{
			if (e->returns != NULL && returns_fmt != NULL) {
				va_list ap;
				va_start(ap, returns_fmt);
				op_vsscanf(e->returns, returns_fmt, ap);
				va_end(ap);
			} else if ((e->returns != NULL) != (returns_fmt != NULL)) {
				output("\nWhile checking %s(%s): Mismatched return assumptions.\n", method, arguments==NULL ? "" : arguments);
			}
			if (i>0 && m_expectations[i-1].method != NULL) {
				output("\n%s(%s) called before %s(%s).\n", method, arguments==NULL ? "" : arguments,
						m_expectations[i-1].method, m_expectations[i-1].arguments == NULL ? "" : m_expectations[i-1].arguments);
				success = FALSE;
				m_should_fail = TRUE;
			}
			e->cleanup();
			goto finish;
		}
	}
	output("\nUnexpected method call: %s(%s)\n", method, arguments==NULL ? "" : arguments);
	for (i=0; i<m_num_expectations; i++) {
		Expectation* e = &m_expectations[i];
		if (e->method != NULL) {
			output("Was expecting this call next: %s(%s)\n", e->method, e->arguments==NULL ? "" : e->arguments);
			break;
		}
	}
	success = FALSE;
	m_should_fail = TRUE;
finish:
	OP_DELETEA(arguments);
	return success;
}

OP_STATUS Mock::CheckExpectation_StatusReturned(const char* method, char* arguments) const {
	int status;
	if (!CheckExpectation(method, arguments, "%d", &status))
		return OpStatus::ERR;
	return (OP_STATUS)status;
}

BOOL Mock::VerifyExpectations() {
	BOOL success = TRUE;
	for (int i=0; i<m_num_expectations; i++) {
		Expectation* e = &m_expectations[i];
		if (e->method != NULL) {
			output("\nExpected method was not called: %s(%s)\n", e->method, e->arguments==NULL ? "" : e->arguments);
			e->cleanup();
			m_should_fail = TRUE;
		}
	}
	m_num_expectations = 0;
	if (m_should_fail)
		success = FALSE;
	m_should_fail = FALSE;
	return success;
}


MockLowLevelFile::~MockLowLevelFile() {
	CheckExpectation("~MockLowLevelFile");
}

OP_STATUS MockLowLevelFile::Open(int mode) {
	return CheckExpectation_StatusReturned("Open", Str("%d", mode));
}

OP_STATUS MockLowLevelFile::Close() {
	return CheckExpectation_StatusReturned("Close");
}

OP_STATUS MockLowLevelFile::Read(void* data, OpFileLength len, OpFileLength* bytes_read) {
	int status, b_read;
	char buf[256]; /* ARRAY OK 2010-09-24 roarl */
	if (!CheckExpectation("Read", Str("void*, %d, OpFileLength*", (int)len), "'%255[^']', %d, %d", buf, &b_read, &status))
		return OpStatus::ERR;
	*bytes_read = (OpFileLength)b_read;
	op_memcpy(data, buf, b_read);
	return (OP_STATUS)status;
}

OP_STATUS MockLowLevelFile::Write(const void* data, OpFileLength len) {
	int status;
	if (op_memchr(data, 0, (size_t)len) != NULL) { // Can't verify such strings exactly
		if (!CheckExpectation("Write", Str("void*, %d", (int)len), "%d", &status))
			return OpStatus::ERR;
	} else {
		char buf[256]; /* ARRAY OK 2010-09-24 roarl */
		if (len > 255) len = 255;
		op_memcpy(buf, data, (size_t)len);
		buf[len] = 0;
		if (!CheckExpectation("Write", Str("'%s', %d", buf, (int)len), "%d", &status))
			return OpStatus::ERR;
	}
	return (OP_STATUS)status;
}

OP_STATUS MockLowLevelFile::GetFilePos(OpFileLength* pos) const
{
	int status, p;
	if (!CheckExpectation("GetFilePos", Str("OpFileLength*"), "%d, %d", &p, &status))
		return OpStatus::ERR;
	*pos = (OpFileLength)p;
	return (OP_STATUS)status;
}

OP_STATUS MockLowLevelFile::GetFileLength(OpFileLength* len) const
{
	int status, l;
	if (!CheckExpectation("GetFileLength", Str("OpFileLength*"), "%d, %d", &l, &status))
		return OpStatus::ERR;
	*len = (OpFileLength)l;
	return (OP_STATUS)status;
}

OP_STATUS MockLowLevelFile::SetFilePos(OpFileLength pos, OpSeekMode mode)
{
	return CheckExpectation_StatusReturned("SetFilePos", Str("%d, %d", (int)pos, (int)mode));
}

OP_STATUS MockLowLevelFile::SetFileLength(OpFileLength len)
{
	return CheckExpectation_StatusReturned("SetFileLength", Str("%d", (int)len));
}

#endif // defined(SEARCH_ENGINE) && defined(SELFTEST)
