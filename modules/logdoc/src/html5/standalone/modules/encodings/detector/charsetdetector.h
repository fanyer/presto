// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef CHARSETDETECTOR_H_
#define CHARSETDETECTOR_H_

class CharsetDetector
{
public:
	inline static bool StartsWithUTF16BOM(const void *buffer)
	{
		return 0xFEFF == *reinterpret_cast<const UINT16 *>(buffer) ||
		       0xFFFE == *reinterpret_cast<const UINT16 *>(buffer);
	};
};

#endif /* CHARSETDETECTOR_H_ */
