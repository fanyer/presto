/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef __FILEHANDLER_KDERC_FILE_H__
#define __FILEHANDLER_KDERC_FILE_H__

#include "platforms/viewix/src/input_files/InputFile.h"

class OpFile;

class KDErcFile : public InputFile
{
 public:

    KDErcFile() {}

    OpString & GetDir() { return m_dir; }

 protected:

    OP_STATUS ParseInternal(OpFile & file);

 private:

    BOOL ParsedTag(OpString & line, OpString & tag, OP_STATUS & status);

    OpString m_dir;
};

#endif //__FILEHANDLER_KDERC_FILE_H__
