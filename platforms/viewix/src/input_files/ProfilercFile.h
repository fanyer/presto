/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef __FILEHANDLER_PROFILERC_FILE_H__
#define __FILEHANDLER_PROFILERC_FILE_H__

#include "platforms/viewix/src/input_files/InputFile.h"

class OpFile;

class ProfilercFile : public InputFile
{
 public:

    ProfilercFile() {}

 protected:

    OP_STATUS ParseInternal(OpFile & file);

 private:

    BOOL ParsedMime(OpString & line,
					OpString & mime_type,
					unsigned int & priority,
					OP_STATUS & status);
};

#endif //__FILEHANDLER_PROFILERC_FILE_H__
