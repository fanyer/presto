# -*- Mode: Makefile; tab-width: 4 -*-
# List compilation units in this directory:

SERVERSRC := certfiles.cpp evoidfiles.cpp generator.cpp database.cpp repository.cpp sign.cpp untrustedcert.cpp filenames.cpp

EXPIRECHECKSRC := expiration_check.cpp database.cpp

# Assumption: the rest of rootstore/ shall not contribute .o files.
# All other source code (if any) shall come from the evbot module itself.
