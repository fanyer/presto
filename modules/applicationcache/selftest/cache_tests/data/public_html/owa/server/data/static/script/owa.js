/**
 * The Test Environment on Client Side
 *
 * The file contains the complete set of tests those will be run on client's side
 * 
 */

// init tests
var TESTS = [];
var TOTAL_TESTS = 4;
for (var ix = 1; ix <= 4; ix++) {
    TESTS.push (eval ("TEST_0" + ix)); // FIXME: number formatting should be fixed
}
