/**
* Test N 04
*/

var Test04_TEST_DATA = [
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

    // the data are changed before `update ()' invocation
    {
        info:    "register value check when content of registers is changed",
        server: ["X", null, "C"],
        client: {
            expected: [
                "A",    // `res?name=RES_REG&ix=0' is explicitly cached
                "B",    // `res?name=RES_REG&ix=1' is implicitly cached (1st register is a 2nd part of FALLBACK)
                "C",    // `res?name=RES_REG&ix=2' is fallback matched, hence first try to load online
            ],
            currnet: []
        }
    },

    // 1st data are changes after `swapCache ()' invocation
    {
        info:    "1st register value check when content of registers is changed, after `swapCache'",
        server: ["Y", null, "Z"],
        client: {
            expected: [
                "X",    // the content of `res?name=RES_REG&ix=0' is explicitly cached
                "C",    // `res?name=RES_REG&ix=1' is not available online, hence via FALLBACK, get "C"
                "C"     // `res?name=RES_REG&ix=2' is implicitly cached because it is FALLBACK (2nd part) URL
            ],
            current: []
        }
    },

    // 2nd data are changes after `swapCache ()' invocation
    {
        info:    "2nd register value check when content of registers is changed, after `swapCache'",
        server: [null, "E", null],
        client: {
            expected: [
                "X",    // `res?name=RES_REG&ix=0' is explicitly cached, though it is not available online
                "E",    // `res?name=RES_REG&ix=1' is FALLBACK matches namespace, hence try load - success
                "C"     // `res?name=RES_REG&ix=2' is implicitly cached because it is FALLBACK (2nd part) URL
            ],
            current: []
        }
    }
];

var Test04_Case01 = Class.create (DefCase, {
    initialize: function ($super) {

        var checks = [
             DefCase.buildDefaultFinalStatusChecker ("IDLE"),

             DefCase.buildDefaultEventOrderChecker ("[BEGIN] => [checking] => [downloading] => ([progress] => )+[cached] => [END]"),

             DefCase.buildDefaultEventValueChecker (),

           {
                "def": new Def (
                    "check event order after call `window.applicationCache.update ()'",
                    "expected event order: [BEGIN] => [checking] => [downloading] => ([progress] => )+[cached] => [END]"
                ),

                check: function (context) {
                     return DefCase.buildDefaultEventOrderChecker (
                         "[BEGIN] => [checking] => [downloading] => ([progress] => )+[cached] => [END]",
                         this.updateEventMonitor
                     );
                }
            },

            {
                "def": new Def (
                    "check event order after call `window.applicationCache.swapCache ()'",
                    "expected event order: [BEGIN] => [END]"
                ),

                check: function (context) {
                     return DefCase.buildDefaultEventOrderChecker (
                         "[BEGIN] => [END]",
                         this.swapCacheEventMonitor
                     );
                }
            },
        ];

        for (var ix = 0; ix < Test04_TEST_DATA.length; ix++) {
            checks.push (DefCase.buildTestDataChecker (Test04_TEST_DATA[ix]));
        }

        $super (
            "basic test of `window.applicationCache.swapCache ()'",
            "The test verifies the content of the registers before and after `swapCache ()' function as well as the event order",
            checks,

            10000
        );
    }, // ~ initialize ()

    pre: function ($super, robot) {

        $super (robot); // call std routine of validation

        window.addEventListener (
            "load",
            function (e) {
                try {
                    
                    // perform the first part of the process only when the application cache has been completely loaded
                    window.applicationCache.addEventListener (
                    	"cached",
                    	function (e) {
	                    	try {
	                    		alert ("cached listener!");
		                        DEFAULT_EVENT_MONITOR. stop ();
		    
		                        Test04_TEST_DATA[0].client.current = collectRegisterValues ([0, 1, 2]);
		    
		                        // update manifest content
		                        var newManifest = DEFAULT_MANIFEST;
		                        newManifest.content.CACHE = [
		                            ["res?name=RES_IMG&ix=0"],
		                            ["res?name=RES_IMG&ix=1"],
		                            ["res?name=RES_REG&ix=0"], // the 0th register is cached
		                        ];
		    
		                        newManifest.content.FALLBACK = [
		                            // fallback namespace has been changed:
		                            //  res?name=RES_REG => res?name=RES_IMG
		                            // instead of the 1st, the 2nd register is used
		                            // it is necessary to present matching 1st register
		                            ["res?name=RES_IMG", "res?name=RES_REG&ix=2"],
		                        ];
		                        new SimpleHTTP ().set (RES.MANIFEST, 0, newManifest);
		            
		                        // update register content - they will be downloaded
		                        Utility.setRegs (Test04_TEST_DATA[1].server, [0, 1, 2]);
		                        Test04_TEST_DATA[1].client.current = Utility.getRegs ([0, 1, 2]);
		    
		                        { // `update ()'
		                            this.updateEventMonitor = new DefEventMonitor().start ();
		    
		                            // handle changes in the manifest
		                            window.applicationCache.update ();
		                        }
	                        } catch (e) {
	                        	this.registerException (e);
	                        }
	                    }.bind (this), 
	                    false
                    );
    
                    // this part is performed only when application has been updated
                    window.applicationCache.addEventListener (
                    	"updateready",
                    	function (e) {
	                        try {
	                        	alert ("updateready listener!");
	                            this.updateEventMonitor.stop ();
	    
	                            this.swapCacheEventMonitor = new DefEventMonitor().start ();
	    
	                           // call `swapCache ()'
	                           window.applicationCache.swapCache ();
	    
	                           Utility.setRegs (Test04_TEST_DATA[2].server, [0, 1, 2]);
	                           Test04_TEST_DATA[2].client.current = Utility.getRegs ([0, 1, 2]);
	    
	                           Utility.setRegs (Test04_TEST_DATA[3].server, [0, 1, 2]);
	                           Test04_TEST_DATA[3].client.current = Utility.getRegs ([0, 1, 2]);
	        
	                           this.swapCacheEventMonitor.stop ();
	                        } catch (e) {
	                            this.registerException (e);
	                        }
	                    }.bind (this),
	                    false
            		); // ~ window.applicationCache.onupdateready
                } catch (e) {
                   this.registerException (e);
                } // ~ try/catch
            }.bind (this),
            false
        ); // ~ window.addEventListener (...)

    } // ~ pre ()
});// ~ class Test04_Case01

var Test04_Case02 = Class.create (DefCase, {
    initialize: function ($super) {

        //
        this.regs = {
            previous: [],
            current: []
        }

        $super (
            "`swapCache ()' and `obsolete' event",
            "expected: when application cache is marked as obsolete then all resources should be taken online",
            [
                {
                    "def": new Def (
                        "check register content",
                        "expected: registers shoul be read explicitly fron online"
                    ),

                    check: function (context) {
                        return {
                            result: false,
                            dump: "TEST IS NOT implemented yet!"
                        };
                    }
                },

                {
                    "def": new Def (
                        "check that register content is read online",
                        "expected: 404-register - not available, 200-register handle up to date content"
                    ),

                    check: function (context) {

                        return {
                            result: false,
                            dump: "TEST IS NOT implemented yet!"
                        };
                    }
                }
            ], // ~ checks
            
            10000 // timeouts 10 secs
        );
    },

    pre: function ($super, robot) {
        window.onload = function (e) {
            try {
                var NEW_REG_PREFFIX = "NEW-REGISTER";
                for (var ix = 0; ix < 2; ix++) {
                    // first of all we need to store previous value of the register
                    // stored now in the application cache
                    this.regs.previous.push (new SimpleHTTP().get (RES.RES_REG, 0));

                    // previous and current register values need to be distinguishable
                    // and a certain string prefix makes them different
                    this.regs.current.push (NEW_REG_PREFFIX + this.regs.previous[ix]);

                    var newReg = DEFAULT_RES_REG;
                    newReg.text = this.regs.current[ix];
                    // in order to handle the changes, the content of the register
                    // has to be changed on the server side
                    new SimpleHTTP ().set (RES.RES_REG, ix, newReg);
                }

                // make manifest unavailable
                // that should to lead to obsolete event when application cache is
                //  updated
                var newManifest = DEFAULT_MANIFEST;
                newManifest.http.status.code = 404;
                newManifest.http.status.text = "Manifest content is not available artificially";
                new SimpleHTTP ().set (RES.MANIFEST, 0, newManifest);

                // when swapCache is ivoked only after obsolete event
                window.applicationCache.onobsolete = function (e) {
                    try {
                        window.applicationCache.swapCache ();
                    } catch (e) {
                        this.registerException (e);
                    } finally {
                        this.registerException (e);
                        robot.verifyChecks (this.checks);
                    }
                }.bind (this);

                window.applicationCache.update ();
            } catch (e) {
                this.registerException (e);

                robot.verifyChecks (this.checks);
                this.post ();// finish the case
            }
        }.bind (this); // ~ window.onload
    } // ~ pre ()

});

var TEST_04 =     { // test `window.aplicationCache.swapCache ()'
    "def": new Def (
        "`window.applicationCache.swapCache ()'",
        "the test checks that the basic functionality of the `swapCache ()' does work"
    ),

    inits: [
        // create pure environment from scratch
        // first clear load page where `html@manifest' is not defined
        new DefInitializator (0, null),

        // second, load a page with valid manifest (that is with `html@manifst'
        //   for the 0th master
        new DefInitializator (0, 0),
    ],

    cases: [
        new Test04_Case01 (),
        // new Test04_Case02 (), // TODO: complete this
    ]

}; // ~ TEST_04
