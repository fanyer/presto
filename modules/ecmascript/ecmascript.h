/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1995 - 2010
 */

#ifndef ECMASCRIPT_H
#define ECMASCRIPT_H

#if defined ECMASCRIPT_DISASSEMBLER && !defined _DEBUG
#  undef ECMASCRIPT_DISASSEMBLER			// Never let the disassembler escape into a release build
#endif // defined ECMASCRIPT_DISASSEMBLER && !defined _DEBUG

/** Engine supports ES_ArgumentsConversion and ES_NEEDS_CONVERSION
    return from host functions. */
#define ES_ARGUMENT_CONVERSION_SUPPORT
/** Engine supports dictionary type conversion specifications. */
#define ES_DICTIONARY_CONVERSION_SUPPORT

/*** PUBLIC API ***/

#include "modules/ecmascript/carakan/ecmascript_module.h"				// Global variables
#include "modules/ecmascript/carakan/src/ecmascript_types.h"			// Types and classes
#include "modules/ecmascript/carakan/src/ecmascript_object.h"			// EcmaScript_Object
#include "modules/ecmascript/carakan/src/ecmascript_manager.h"			// EcmaScript_Manager
#include "modules/ecmascript/carakan/src/ecmascript_runtime.h"			// ES_Runtime
#include "modules/ecmascript/carakan/src/ecmascript_private.h"			// ES_LAST_PRIVATESLOT=65535: the last private name available to client code

/*** END PUBLIC API ***/

#include "modules/ecmascript/carakan/src/ecmascript_runtime_inlines.h"

#endif // ECMASCRIPT_H
