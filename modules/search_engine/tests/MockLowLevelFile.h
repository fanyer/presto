/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/

#ifndef MOCKLOWLEVELFILE_H
#define MOCKLOWLEVELFILE_H

#include "modules/pi/system/OpLowLevelFile.h"

class Mock
{
public:
	Mock() : m_num_expectations(0), m_should_fail(FALSE) {}

	void Expect(const char* method, char* arguments = NULL, char* returns = NULL);
	void Expect(const char* method, char* arguments, OP_STATUS returned_status);
	BOOL VerifyExpectations();

	static char* Str(const char* fmt, ...);

protected:
	void Fail() const;
	BOOL CheckExpectation(const char* method, char* arguments = NULL, const char* returns_fmt = NULL, ...) const;
	OP_STATUS CheckExpectation_StatusReturned(const char* method, char* arguments = NULL) const;

private:
	enum { m_max_expectations = 64 };
	struct Expectation {
		const char* method;
		char* arguments;
		char* returns;
		Expectation() : method(NULL), arguments(NULL), returns(NULL) {}
		~Expectation() { cleanup(); }
		void cleanup() {
			if (arguments != NULL) OP_DELETEA(arguments);
			if (returns != NULL)   OP_DELETEA(returns);
			method = arguments = returns = NULL;
		}
	};
	mutable Expectation m_expectations[m_max_expectations];
	mutable int m_num_expectations;
	mutable BOOL m_should_fail;
};

class MockLowLevelFile: public OpLowLevelFile, public Mock
{
public:
	MockLowLevelFile()									{}
	~MockLowLevelFile();

	const uni_char*	GetFullPath() const					{ Fail(); return NULL; }
	const uni_char*	GetSerializedName() const			{ Fail(); return NULL; }
	OP_STATUS		GetLocalizedPath(OpString*) const	{ Fail(); return OpStatus::ERR; }
	OP_STATUS		MakeDirectory()						{ Fail(); return OpStatus::ERR; }
	BOOL			IsWritable() const					{ Fail(); return FALSE; }
	OP_STATUS		Open(int);
	BOOL			IsOpen() const						{ Fail(); return FALSE; }
	BOOL			Eof() const							{ Fail(); return TRUE; }
	OP_STATUS		Write(const void*, OpFileLength);
	OP_STATUS		Read(void*, OpFileLength, OpFileLength*);
	OP_STATUS		ReadLine(char**)					{ Fail(); return OpStatus::ERR; }
	OpLowLevelFile*	CreateCopy()						{ Fail(); return NULL; }
	OpLowLevelFile*	CreateTempFile(const uni_char*)		{ Fail(); return NULL; }
	OP_STATUS		CopyContents(const OpLowLevelFile*)	{ Fail(); return OpStatus::ERR; }
	OP_STATUS		SafeClose()							{ Fail(); return OpStatus::ERR; }
	OP_STATUS		SafeReplace(OpLowLevelFile*)		{ Fail(); return OpStatus::ERR; }
	OP_STATUS		Exists(BOOL*) const					{ Fail(); return OpStatus::ERR; }
	OP_STATUS		GetFilePos(OpFileLength*) const;
	OP_STATUS		GetFileLength(OpFileLength*) const;
	OP_STATUS		GetFileInfo(OpFileInfo*)			{ Fail(); return OpStatus::ERR; }
#ifdef SUPPORT_OPFILEINFO_CHANGE
	OP_STATUS		ChangeFileInfo(const OpFileInfoChange* changes)	{ Fail(); return OpStatus::ERR; }
#endif // SUPPORT_OPFILEINFO_CHANGE
	OP_STATUS		Close();
	OP_STATUS		Delete()							{ Fail(); return OpStatus::ERR; }
	OP_STATUS		Flush()								{ Fail(); return OpStatus::ERR; }
	OP_STATUS		SetFilePos(OpFileLength, OpSeekMode);
	OP_STATUS		SetFileLength(OpFileLength);
#ifdef PI_ASYNC_FILE_OP
	void			SetListener(OpLowLevelFileListener *listener)		{ Fail(); }
	OP_STATUS		ReadAsync(void*, OpFileLength, OpFileLength)		{ Fail(); return OpStatus::ERR; }
	OP_STATUS		WriteAsync(const void*, OpFileLength, OpFileLength)	{ Fail(); return OpStatus::ERR; }
	OP_STATUS		DeleteAsync()						{ Fail(); return OpStatus::ERR; }
	OP_STATUS		FlushAsync()						{ Fail(); return OpStatus::ERR; }
	OP_STATUS		Sync()								{ Fail(); return OpStatus::ERR; }
	BOOL			IsAsyncInProgress()					{ Fail(); return FALSE; }
#endif // PI_ASYNC_FILE_OP
};


#endif // MOCKLOWLEVELFILE_H
