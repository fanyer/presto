/**
* Test N 02
*/
var Test02_Case01 = Class.create (DefCase, {
    initialize: function ($super) {

        var checks = [
            DefCase.buildDefaultFinalStatusChecker ("IDLE"),

            DefCase.buildDefaultEventOrderChecker ("[BEGIN] => [checking] => [downloading] => ([progress] => )+[cached] => [END]"),

            DefCase.buildDefaultEventValueChecker (),
        ];

        $super (
            "smoke test of `window.applicationCache' when `html@manifest' attribute is defined",
            "the test case checks that states are valid and that functions works as it is specified in spec.",
            checks,
            10000
        );
    },
}); // ~ class Test02_Case01

var TEST_02 = { // smoke test when `html@manifest' is defined
    "def": new Def (
        "`manifest' attribute is defined",
        "the test verifies the cases when the `manifest' attribute IS defined"
    ),

    inits: [
            // create pure environment from scratch
        // first clear load page where `html@manifest' is not defined
        new DefInitializator (0, null),
        
        // use the 0th manifest (as value of `html@manifest' attribute)
        //   for the 0th master
        new DefInitializator (0, 0),
    ],

    cases: [
        new Test02_Case01 (),
    ] // ~ caseS
}; // ~ TEST_02
