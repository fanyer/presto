/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 **
 */

#ifndef MANIFEST_PARSER
#define MANIFEST_PARSER

#ifdef APPLICATION_CACHE_SUPPORT

#include "modules/applicationcache/manifest.h"

class URL;
class Manifest;

/**
 * Manifest Parser
 *
 * The class implement statefull machine for parsing manifest data.
 * The statefull machine means that the parser may be invoked even for not completely downloaded data
 * but just for a piece of loaded data.
 *
 *
 */
class ManifestParser {

    public:

    /**
     * Parse Manifest File
     *
     * The function is used for parsing the content of the manifest. It may be invoked
     * a couple of time in order to parse available chunks of the manifest.
     *
     * @param[in] uni_str a pointer to UNICODE (UTF-16) characters those have to be parsed
     * @param[in] str_size the size of the chunk of data that has to be parsed
     * @param[in] is_last is the passed data are last or not
     *
     * @param[out] parsed_chars a number of characters those have been parsed
     * @note As a matter of fact, the parser may not take into consideration all data
     * but just a piece of them. In order to know how many character have been processed,
     * should be used this variable.
     *
     * @return OP_STATUS the status of parsing
     * @note The status is usually is OK. In case when a certain system error happened (like out of memory,
     * out of boundary etc.) then the status is ERROR.
     */
    virtual
    OP_STATUS Parse (const uni_char*const uni_str, unsigned str_size, BOOL is_last, unsigned& parsed_chars) = 0;

    /**
     * Returns Manifest Object
     *
     * The function returns a normalized version of the manifest.
     * The normalized manifest it is the manifest in which:
     *  # there is no repeated entries (neither in one, nor in a couple of sections);
     *  # all entries those haven't the same origin as a manifest itself, are removed;
     *  # all entries those are present in a couple of sections are excluded from the section
     *      that has a lover priority;
     *  # all fractions are excluded.
     *
     *  Memory Management Note: it is invoker's responsibility to handle memory management (delete object)
     *  Each time when the function is invoked, a new instance of the object is created. This approach is used
     *  in order to maintain a couple of instances of the same manifest, but in different modules, in different
     *  object with different time of leaving.
     *
     *
     *  @return a normalized manifest or NULL if manifest content has not been completely parser or
     *      during parsing a certain error occurred
     *
     */
    virtual
    OP_STATUS BuildManifest (Manifest*& new_manifest) const = 0;

    virtual
    ~ManifestParser ()  = 0;

    /**
     * Build Manifest Parser
     *
     * This is the only way to create a manifest parser.
     * When the parser is created, a context where it will work must be specified.
     * The context of the manifest parser defined by a manifest URL. All relative URL in a manifest file
     * are resolved by the manifest URL.
     *
     * @param[in] manifest_url - manifest URL
     * @param[in|out] new_manifest_parser - reference to the pointer that has to be initialized
     * @note NULL may be assigned in case when input arguments are wrong or when during memory allocation
     * an unexpected error occurred
     *
     * @return OP Status value: either OpStatus::OK - if everything is fine, or OpStatus::ERR_NO_MEMORY
     *
     * @note Memory management consideration - it is invoker's responsibility to handle
     *      memory management (delete object)
     */
    static
    OP_STATUS BuildManifestParser (
            const URL& manifest_url,
            ManifestParser*& new_manifest_parser
    );

};

#endif // APPLICATION_CACHE_SUPPORT
#endif // MANIFEST_PARSER
