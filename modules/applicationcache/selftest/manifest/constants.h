#ifndef CONSTANTS_H
#define CONSTANTS_H

// note, the constants are defined (not declared) in order to compose string
//  and in order to solve the problem with UNI_L macro when a couple of strings are concatenated
//  like `"string 1" "string 2"'
#   define TEST_PROT "http"
#   define TEST_HOST "www.opera.com"
#   define TEST_BASE_URL "http://www.opera.com"

#   define TEST_MASTER_URL "http://www.opera.com/index.html"
#   define TEST_MANIFEST_URL "http://www.opera.com/manifes.manifest"

#endif // CONSTANTS_H
