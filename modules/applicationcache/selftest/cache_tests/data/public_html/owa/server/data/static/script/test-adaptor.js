var DEFAULT_TIMEOUT = 3000;

var TestAdaptor = Class.create ({
	initialize: function (testId) {
        
        var testName = "TEST_" + testId;
        var testObj = eval (testName);

        // framework variables
        var verifyTest = window.opener.verifyTest;

        setTimeout (
            function () {
                try {
                    for (var ix = 0; ix < testObj.cases.length; ix++) {
                        var caseObj = testObj.cases[ix];
                        for (var jx = 0; jx < caseObj.checks.length; jx++) {
                            var checkObj = caseObj.checks[jx];
                            var result = checkObj.check ();
                            verifyTest(result.result, checkObj.def.title);
                        }
                    }
                } catch (e) {
                    verifyTest(false, "Inner test error: " + e.message);
                }

                verifyTest (true); // immediately go to the next test 
            },

            DEFAULT_TIMEOUT
        );
	},
});
