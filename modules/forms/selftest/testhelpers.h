/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

#ifndef SELFTEST
# error "You should only use this file in self test builds"
#endif // !SELFTEST

#ifndef FORMS_TESTHELPERS_H
#define FORMS_TESTHELPERS_H

/**
 * Defined here since getcwd isn't a function available everywhere.
 */
uni_char* formtest_getcwd(uni_char* buffer, size_t maxlen);

#endif // !FORMS_TESTHELPERS_H
