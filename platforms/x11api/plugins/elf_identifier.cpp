/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; -*-
 *
 * Copyright (C) 2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#if defined(X11API) && defined(NS4P_COMPONENT_PLUGINS)

#include "platforms/x11api/plugins/elf_identifier.h"

#include <fcntl.h>
#include <unistd.h>

OP_STATUS ElfIdentifier::Init(const char* path)
{
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		return OpStatus::ERR;

	ssize_t bytes_read = read(fd, &m_header, sizeof(m_header));
	close(fd);

	if (bytes_read < (ssize_t)sizeof(m_header))
		return OpStatus::ERR;

	// check for ELF magic number
	if (m_header.e_ident[EI_MAG0] != ELFMAG0 ||
		m_header.e_ident[EI_MAG1] != ELFMAG1 ||
		m_header.e_ident[EI_MAG2] != ELFMAG2 ||
		m_header.e_ident[EI_MAG3] != ELFMAG3)
		return OpStatus::ERR;

	// Only read files with encoding similar to current running program
	if (m_header.e_ident[EI_DATA] != ELFDATA_NATIVE)
		return OpStatus::ERR;

	return OpStatus::OK;
}

#endif // X11API && NS4P_COMPONENT_PLUGINS
