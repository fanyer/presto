/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef __FILEHANDLER_INPUT_FILE_H__
#define __FILEHANDLER_INPUT_FILE_H__

class OpFile;

class InputFile
{
 public:

    virtual ~InputFile(){}

    virtual OP_STATUS Parse(const OpStringC & file_name);

 protected:

    virtual OP_STATUS ParseInternal(OpFile & file) = 0;

 private:

    OP_STATUS OpenFile(OpFile & file, const OpStringC & file_name);
};

#endif //__FILEHANDLER_INPUT_FILE_H__
