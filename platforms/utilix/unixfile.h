/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#ifndef UNIX_FILE_H
#define UNIX_FILE_H __FILE__
#include <sys/stat.h>

namespace UnixFile
{
	FILE *fopen(const uni_char* filename, const uni_char* mode);
	int access(const uni_char* filename, int mode);
	int chmod(const uni_char* filename, mode_t mode);
	int mkdir(const uni_char* directory, int mode);
	int rmdir(const uni_char* directory);
	int rename(const uni_char* from, const uni_char* to);
	int stat(const uni_char* path, struct stat* buf);
	int lstat(const uni_char* path, struct stat* buf);
	int readlink(const uni_char* path, uni_char* buf, size_t bufsize);
	uni_char* getcwd(uni_char* buf, size_t size);
	int mkstemp(uni_char* candidate); // NB: candidate *MUST NOT* be a UNI_L literal
	/** @deprecated Use mkstemp instead of tempnam.
	 */
	DEPRECATED(uni_char* tempnam(const uni_char* directory, const uni_char* prefix));
}
#endif // UNIX_FILE_H
