/**
 * Robot Class
 * 
 * Robot automatically starts the tests and invokes initialization, validation, storage data into server etc.
 * 
 * 
 * @return
 */
var Robot = Class.create ({
    initialize: function () {

		alert (document.getElementsByTagName ("html")[0].outerHTML);
        this.innerToDo = null;

        if (TESTS.length == this.getToDo().testNo) {
            this.report ();
        } else {
            this.run ();
        }
    },

    getToDo: function () {
        if (null == this.innerToDo) {
			var toDo = null;
	
			// try to find the next ToDo that is runnable
			while (true) {
	        	toDo = new SimpleHTTP().get (RES.TODO, 0);
	        	var checkList = new SimpleHTTP().get (RES.CHECK_LIST, toDo.testNo);
	
	        	if (!checkList.isRunnable && toDo.testNo < TESTS.length) {
			        this.setToDo ({
			            testNo: toDo.testNo + 1,
			            initNo: 0,
			            caseNo: 0
			        });
			        continue;
	        	} else {
	        		break;
	        	}
	        }
	
			this.innerToDo = toDo;
		}

        return this.innerToDo;
    },

    setToDo: function (newToDo) {
        this.innerToDo = newToDo;
        new SimpleHTTP().set (RES.TODO, 0, newToDo); // put new value of ToDo in def. 0th index
    },

    getHistory: function (ix) {
        var history = new SimpleHTTP().get (RES.HISTORY, ix);
        return history;
    },

    setHistory: function (newHistory) {
        new SimpleHTTP().set (RES.HISTORY, this.getToDo ().testNo, newHistory);
    },

    loadNextTest: function () {
        this.setToDo ({
            testNo: this.getToDo().testNo + 1,
            initNo: 0,
            caseNo: 0
        });

        location.reload (true);
    },

    run: function () {
        // if the test is runnable, then go through all the std. steps
        //  1) initializations;
        //  2) running cases;
        //  3) running a next test...
        if (TESTS[this.getToDo().testNo].inits.length == this.getToDo().initNo) {
            this.runCase ();
        } else {
            this.runInit ();
        }
    },

    runCase: function () {
        if (TESTS[this.getToDo().testNo].cases.length == this.getToDo().caseNo) {
            this.loadNextTest ();
        } else {
            var caseObj = TESTS[this.getToDo().testNo].cases[this.getToDo().caseNo];
            caseObj.pre (this);
        }
    },

    verifyChecks: function (caseChecks) {
        var history = this.getHistory (this.getToDo().testNo);
        if (null == history) { // if the history is null, it means that the history is initialized the first time
        	history = [];
        }
        var checks = [];
        for (var ix = 0; ix < caseChecks.length; ix++) {
            var checkObj = caseChecks[ix];
            var checkResult = null;
            try {
                checkResult = checkObj.check (null);
            } catch (e) {
                checkResult = {
                    result: false,
                    dump: e
                };
            }

            checks.push (checkResult);
        }
        history.push (checks);
        this.setHistory (history);
        this.setToDo ({
            testNo: this.getToDo().testNo,
            initNo:	this.getToDo().initNo,
            caseNo: this.getToDo().caseNo + 1
        });
    },

    runInit: function () {
        var oldToDo = this.getToDo ();
        this.setToDo ({
            testNo: this.getToDo().testNo,
            initNo: this.getToDo().initNo + 1,
            caseNo: 0
        });
        TESTS[oldToDo.testNo].inits[oldToDo.initNo].init();
    },

    report: function () {
        var styleSelector = function (expectedNo, currentNo) {
            if (expectedNo === currentNo) {
                return "ok";
            } else {
                if (currentNo === 0) {
                    return "nok";
                }  else {
                    return "sok";
                }
            }
        }

        var writer = function (baseObj, ix, levelNo, prefix, expected, current, childrenText) {
            var obj = baseObj[ix];
            return 	  ""
            + "<p style=\"left: " + (levelNo * 5) + "mm;\">"
            +	"<h" + levelNo
            +		" class=\"" + styleSelector (expected, current) + "\""
            +	">"
            +		prefix + " No. "  + (ix + 1) + ": "
            +		obj.def.title
            +		"<p class=\"details\">" + obj.def.descr + "</p>"
            +		childrenText
            + 	"</h" + levelNo + ">"
            // +	"<br/>"
            //+	childrenText;
            + "</p>"
        ;
        }

        var checkListIxs = [];
        var histories = [];
        for (var ix =0; ix < TESTS.length; ix++) {
            // check is the test runnable at this moment
            var checkList = new SimpleHTTP().get (RES.CHECK_LIST, ix);

            if (checkList.isRunnable) {
                checkListIxs.push (ix);
                histories.push (this.getHistory (ix));
            }
        }

        var globalText = "";
        var globalResult = 0;
        // TODO: ix!!!!
        for (var plainIx = 0; plainIx < checkListIxs.length; plainIx++) {
        	var ix = checkListIxs[plainIx];
            var testObj = TESTS[ix];
            var testResult = 0;

            testChildrenText = "";
            for (var jx = 0; jx < testObj.cases.length; jx++) {
                var caseObj = testObj.cases[jx];
                var caseResult = 0;
                var caseChildrenText = "";
                for (var kx = 0; kx < caseObj.checks.length; kx++) {
                    var checkObj = caseObj.checks[kx];
                    var historyCheckObj = histories[plainIx][jx][kx];
                    var currentCheck = historyCheckObj.result ? 1 : 0;
                    caseResult += currentCheck;
                    details = ""
                    + "<p class=\"details\">"
                    + 	"<b>result</b>: " + JSON.stringify (historyCheckObj.dump) + "<br/>"
                    + "</p>"
                    + "<br/>"
                    ;
                    caseChildrenText += writer (caseObj.checks, kx, 5, "Check", 1, currentCheck, details);
                }
                testResult += caseResult == caseObj.checks.length ? 1 : 0;
                testChildrenText += writer (testObj.cases, jx, 4, "Case", caseObj.checks.length, caseResult, caseChildrenText);
            }

            globalResult += testObj.cases.length == testResult ? 1 : 0;
            globalText += writer (TESTS, ix, 3, "Test", testObj.cases.length, testResult, testChildrenText);
        }

        globalText 	= "<h1 class=\"" + styleSelector (TESTS.length, globalResult) + "\">Results</h1><br/>"
        + globalText
        + "<br/>";
        window.onload = function (e) {
            var consoleObj = document.getElementById ("test-console");
            consoleObj.innerHTML = globalText;
        };
        this.setToDo ({
            testNo: 0,
            initNo: 0,
            caseNo: 0,
        });
        for (var ix = 0; ix < TESTS.length; ix++) {
            this.innerToDo.testNo = ix;
            this.setHistory ([]);
        }
    }
});

var ROBOT = new Robot (); // create a robot
