/**
* Test N 01
*/
var Test01_Case01 = Class.create (DefCase, {
   initialize: function ($super) {
       $super (
            "smoke test case",
            "the test case checks that statuses are valid and that functions works as it is specified in spec.",
            [
                 DefCase.buildDefaultFinalStatusChecker ("UNCACHED"),

                 DefCase.buildDefaultEventOrderChecker ("[BEGIN] => [END]"),

                {
                    "def": new Def (
                        "invocation `windows.applicationCache.udate ()'",
                        "must be generated: `INVALID_STATE_ERR' exception"
                    ),

                    check: function (context) {
                        return handleException ( // TODO: move handleException function to Utility class
                            function (context) {
                                window.applicationCache.update ();
                            },
                            context,
                            "INVALID_STATE_ERR"
                        );
                    }
                },

                {
                    "def": new Def (
                        "invocation `windows.applicationCache.swapCache ()'",
                        "must be generated: `INVALID_STATE_ERR' exception"
                    ),

                    check: function (context) {
                        return handleException (
                            function (context) {
                                window.applicationCache.swapCache ();
                            },
                            context,
                            "INVALID_STATE_ERR"
                        );
                    }
                }, // ~ check
            ] // ~ checkS
        ); // ~$super
   } // ~ initialize
}); // ~ class Test01_Case01

var TEST_01 = { // smoke test when `html@manifest' attribute is not defined

    "def": new Def (
        "`manifest' attribute is not defined",
        "the test verifies the cases when the `html@manifest' attribute IS NOT defined"
    ),


    inits: [
        // do not use `html@manifest' attribute for 0th master
        new DefInitializator (0, null),
    ],

    cases: [
        new Test01_Case01 (), // ~ case
    ] // ~ caseS

}; // ~ TEST_01
