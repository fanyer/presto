/**
* Test N 03
*/

var Test03_TEST_DATA = [
    // the data are cached first time the page is loaded
    {
        info:    "register value check when a page is just loaded",
        server: ["A", "B", null],
        client: {
            expected: [
                "A",    // `res?name=RES_REG&ix=0' is explicitly cached
                "B",    // `res?name=RES_REG&ix=1' is implicitly cached (1st register is a 2nd part of FALLBACK)
                "B"     // `res?name=RES_REG&ix=2' is FALLBACK matched, hence:
                        //                                                  1) try to load online => failure
                        //                                                  2) then, read content of the FALLBACK
            ], // the 3rd is read from cache, via FALLBACK matching
            current: []
        }
    },
];

var Test03_Case01 = Class.create (DefCase, {

    initialize: function ($super) {

        $super (
            "check the order of events when manifest is not changed",
            "the case verifies what is going on in case when the content of the manifest is not changed",
            [
                DefCase.buildDefaultFinalStatusChecker ("IDLE"),

                DefCase.buildDefaultEventOrderChecker ("[BEGIN] => [checking] => [noupdate] => [END]"),

                DefCase.buildDefaultEventValueChecker (),

                DefCase.buildTestDataChecker (Test03_TEST_DATA[0]),
            ],

            5000
        ); // ~ $super ()
    }, // ~  initialize ()
}); // ~ class Test03_Case01

var TEST_03 = { // test `window.appicationCache.update ()'
    "def": new Def (
        "`window.applicationCache.update ()'",
        "the test checks how the system works when `window.appicationCache.update ()' function is invoked"
    ),

    inits: [
        // create a pure environment from scratch
        // first clear load page where `html@manifest' is not defined
        new DefInitializator (0, null),

        // initialize resources content
        //  it should be done before manifest is loaded
        //  in other case the changes will not be handled
        new (
        	Class.create (DefInitializator, {
	            init: function ($super) {
	                Utility.setRegs (Test03_TEST_DATA[0].server, [0, 1, 2]);
	
		            $super ();
	            }
        	}
        )) (0, 0), // MASTER: 0, MANIFEST: 0

        // second, load a page with valid manifest (that is with`html@manifst'
        //   for the 0th master
        new DefInitializator (0, 0),
    ], // ~ initS

    cases: [
        new Test03_Case01 (),
    ] // ~ caseS
}; // ~ TEST_03
