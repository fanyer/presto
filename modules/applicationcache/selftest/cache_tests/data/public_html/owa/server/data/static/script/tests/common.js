/**
 * Common Classes for All Tests
 * 
*/
// A set of objects used as prototypes.
// These objects are used in order to minimize get/set requests from/to the server
//  during initialisation phase
//
// By using these objects and changing particular properties the initialisation
//  process is reduced to the following steps:
//  1. get proper DEFAULT object;
//  2. change its certain values
//  3. send changed object to the server

// Default Master
//  master is a HTML file that has HTML element with or without `manifest'
//  attribute
var DEFAULT_MASTER = new Master (0);

// Default manifest
//  manifest file content description
var DEFAULT_MANIFEST = new Manifest (0);

// Default register
//  register is used for storing text and validation the content
var DEFAULT_RES_REG = new Register (0, "X")

var Utility = {
    /**
     * Set register values
     * 
     * @param regVals register values
     * @param regIxs register indexes
     */
  setRegs: function (regVals, regIxs) {
      for (var ix = 0; ix < regIxs.length; ix++) {
        var newReg = DEFAULT_RES_REG;
        newReg.text = regVals[ix];
        new SimpleHTTP ().set (RES.RES_REG, regIxs[ix], regVals[ix]);
      }
  }, // ~ setRegs (...)

  /**
   * Get register values
   * 
   * @param ixs indexes of the registers
   * 
   * @return values of the registers
   */
  getRegs: function  (regIxs) {
      var result = [];
      for (var ix = 0; ix < regIxs.length; ix++) {
          result.push (new SimpleHTTP ().get (RES.RES_REG, regIxs[ix]));
      }
      return results;
  } // ~ getRegs (...)
};

var Def = Class.create ({
    initialize: function (title, descr) {
        this.title = title;
        this.descr = descr;
    }
});

/**
 * Default Event Monitor
 * 
 * The class registers event listeners and stores all events fired by ApplicaitonCache
 */
var DefEventMonitor = Class.create ({
    eventHistory: {
        oldListener: [],
        newListener: []
    }, // ~ eventHistory (...)

    registerEvent: function (listener, isNewEventListener, event) {
        var prefix = isNewEventListener ? "new" : "old";
        this.eventHistory[prefix + "Listener"].push ({
            "listener": listener,
            "event": event,
            "status": window.applicationCache.status
        });
    }, // ~ registerEvent (...)

    initialize: function () {
        var EventDef = Class.create ({
            /**
             * Constructor 
             * 
             * @param type event type
             * @param status status of the ApplicatinCache when the event is fired
             */
            initialize: function (type, status) {
                this.type = type;
                this.status = status;
            }
        }); // ~ class EventDef

        var DefEventListener = Class.create({
            getListener: function (isNewEventListener) {
                return function (e) {
                    this.monitor.registerEvent (this, isNewEventListener, e);
                }.bind (this);
            }, // ~ getListener (...)

            initialize: function (monitor, eventDef) {
                this.eventDef = eventDef;
                this.monitor = monitor;

                // listeners are created but they are not registered yet
                this.oldListener = this.getListener (false);
                this.newListener = this.getListener (true);
            }, // ~ initialize (...)

            register: function (isRegistration) {
                if (isRegistration) {
                    //  one is an old way how the listener is registered
                    window.applicationCache["on" + this.eventDef.type] = this.oldListener;

                    //  another is a new via `setEventListener' function
                    window.applicationCache.addEventListener (
                        this.eventDef.type,
                        this.newListener,
                        false
                    ); 
                } else {
                    //  one is an old way how the listener is unregistered
                    window.applicationCache["on" + this.eventDef.type] = null;

                    //  another is a new via `setEventListener' function
                    window.applicationCache.removeEventListener (
                        this.eventDef.type,
                        this.newListener,
                        false
                    ); 
                }
            },

            /**
             * The function for the current event listener checks the event data and `applicationCache.status' value
             * If both event data (`type', `target', `currentType', `eventPhase' etc.)
             *  as well as status are equal, the the result is `true', in other case - `false'.
             */
            checkData: function (event, status) {
                return {
                    // the result validation is quite strict
                    //  it is expected that data have not only the same values but also
                    //  they are the same objects (in case of the ApplcaitonCache object)
                    result:
                               this.eventDef.status === status
                            // strings may be different objects, but they must be similar
                            && this.eventDef.type == event.type

                            // before checking `target' and `currentTarget'
                            //  ensure, that the `target' object is instance of ApplicationCache
                            && event.target instanceof ApplicationCache
                            && window.applicationCache === event.target

                            // note: it seems that event.currentTarget improperly implemented in Opera
                            // && window.applicationCache === event.currentTarget
                            && Event.AT_TARGET === event.eventPhase
                            
                            && false === event.bubbles
                            && true === event.cancelable   // all application cache events are cancelable
                            
                            // note: that in case of namespaceURI, the property may not be supported in Opera at all
                            //   in this case, ECMA script return `undefined'. But undefined is not exactly equal to `null'
                            //   but it is equal to `null' if type casting is performed (it is automatically done when
                            //   `==' operator is used)
                            && null == event.namespaceURI    // namespace URI must be `null'
                            
                            // note: it is not supported yet, because it is a part of DOM 3 extension
                            //&& false === event.defaultPrevented // by default `default action' is not prevented
                            
                            // note: `srcElement' property is present in the event
                            //  it is not a part of Event interface according to DOM spec.
                    ,

                    // dump data contains all expected results (`eventDef') as well as all current results
                    dump: JSON.stringify ({
                        "eventDef": this.eventDef,
                        "event": {
                        	type: event.type,
                        	bubbles: event.bubbles,
                        	cancelable: event.cancelable,
                        	defPrev: event.defaultPrevented,
                        	isTargetValid: window.applicationCache === event.target
                         },
                        "status": status
                    })
                };
            } //~ checkData (...)

        }); // ~ class DefEventListener

        var EVENTS = [
          new EventDef ("checking", ApplicationCache.CHECKING),
          new EventDef ("downloading", ApplicationCache.DOWNLOADING),
          new EventDef ("progress", ApplicationCache.DOWNLOADING),
          new EventDef ("cached", ApplicationCache.IDLE),

          new EventDef ("updateready", ApplicationCache.UPDATEREADY),
          new EventDef ("noupdate", ApplicationCache.IDLE),
          new EventDef ("obsolete", ApplicationCache.OBSOLETE),
          new EventDef ("error", ApplicationCache.UNCACHED),
      ];

      this.eventListeners = [];

      for (var ix = 0; ix < EVENTS.length; ix++) {
          this.eventListeners.push (new DefEventListener (this, EVENTS[ix]));
      }

    }, // ~ initialize

    innerEventListenerActivator: function (isStart) {
        for (var ix = 0; ix < this.eventListeners.length; ix++) {
            this.eventListeners[ix].register (isStart);
        }
    },
    
    /**
     * Start monitoring events
     * 
     * The function registers all old and new events listeners and starts monitoring all events
     */
    start: function () {
        this.innerEventListenerActivator (true);
        return this;
    },
    
    /**
     * Stop monitoring events
     * 
     * The unregisters all old and new events listeners and stops monitoring all events
     */
    stop: function () {
        this.innerEventListenerActivator (false);
        return this;
    },
    
    /**
     * Check that the event order is the same as it is expected
     * Event order has the format:
     * * => <a set of events> => (*)
     * Here,
     *      `*' - means the start of the event order
     *      `(*)' - means the end of the event order
     *      `[<event name>]' - the name of the event
     *          e.g.: `[checking]'
     *          if a couple of events may occur, then may be used reg-exp quantifiers, like
     *          `([checking]){1,}' - at least one
     *          `([checking])*' - zero or more
     * 
     * Note, that all `[' and `]' characters are escaped.
     * 
     * The order is checked for all events those have been generated both
     *  * by new event listeners (the listeners those have been added via `addEventListener');
     *  * by old event listener (the listeners those have been added via `applicaiotnCache.on<event type> = <listener>');
     * 
     * Complete example:
     * `[BEGIN] => [checking] => [downloading] => ([progress] => ){1,}=>[cached] => [END]'
     */
    checkEventOrder: function (expectedEventOrder) {

        // escape data
        var newEventOrder = expectedEventOrder
                                .replace (/\[/g, "\\[")
                                .replace (/\]/g, "\\]")
        ;

        var START_EVENT = "BEGIN";
        var END_EVENT = "END";

        var eventTypeDecorator = function (type) {
            return "[" + type + "]";
        };
        
        var eventLinker = function (first, next) {
            return first + " => " + next;
        };

        var basicChecker = function (historyRecords, etalonEventOrder) {
            // convert all event history to event order
            var currentEventOrder = eventTypeDecorator (START_EVENT);
            for (var ix = 0; ix < historyRecords.length; ix++) {
                var historyRecord = historyRecords[ix];
                currentEventOrder = eventLinker (
                        currentEventOrder,
                        // note, that from the history data  listener's data are taken into consideration (event type)
                        //  it is done for purification of the result
                        //  as a matter of fact, actual event may have improper `type' value, but the actual event has been fiered
                        //   hence it has to be taken into consideration 
                         eventTypeDecorator (historyRecord.listener.eventDef.type)
                );
            }

            // match event order with expected data
            currentEventOrder = eventLinker (
                currentEventOrder,
                eventTypeDecorator (END_EVENT)
            );
            
            return {
                result: new RegExp (etalonEventOrder).test (currentEventOrder),
                dump: currentEventOrder
            };
        };

        var oldListenerData = basicChecker (this.eventHistory.oldListener, newEventOrder);
        var newListenerData = basicChecker (this.eventHistory.newListener, newEventOrder);

        return {
            result: oldListenerData.result && newListenerData.result,
            dump: {
                oldListener: oldListenerData.dump,
                newListnere: newListenerData.dump
            }
        }; // ~ return
    }, // ~ checkEventOrder (..)

    // TODO: extend this function with validation exact event type values (like in `checkEventOrder')
    checkEventValues : function () {
        var basicChecker = function (historyRecords) {
            var result = true;
            var dump = [];
            for (var ix = 0; ix < historyRecords.length; ix++) {
                var historyRecord = historyRecords[ix];
                var recordTest = historyRecord.listener.checkData (historyRecord.event, historyRecord.status);
                result &= recordTest.result;
                dump.push (recordTest);
            }
            
            return {
                "result": result,
                "dump": dump
            };
        };

        var oldListenerData = basicChecker (this.eventHistory.oldListener);
        var newListenerData = basicChecker (this.eventHistory.newListener);

        // it is considered that result is valid, only when
        //  data both from old and new event listeners are valid
        //  and when data both from old and new event listeners is exact
        var result = 
                        oldListenerData.result
                     && newListenerData.result
                     // the test that data is exact is performed via translating data to JSON
                     // and then via comparing obtained strings
                     && (JSON.stringify (oldListenerData) == JSON.stringify (newListenerData))
        ;

        return {
            "result": result,
            dump: {
                oldListener: oldListenerData,
                newListnere: newListenerData
            }
        }; // ~ return
    } // ~ checkEventValues (...)
});


/**
 * Default Event Monitor
 * 
 * This is a default event monitor that is initialized during instantiation of the unit-test framework.
 * Usually, the object is created before any actions in application cache started.
 * 
 * The object shared among all tests in order to reduce resource utilization and in order to speed up
 * testing process.
 * 
 */
var DEFAULT_EVENT_MONITOR = null;

try {
    // event monitor may generate exception is ApplicationCache interface is not supported properly
    // event monitor is started as soon as it is created
    DEFAULT_EVENT_MONITOR = new DefEventMonitor().start ();
} catch (e) {
    // handle event in order to initialize all scripts
}


var DEFAULT_TIMEOUT = 2000; // default timeout is 2 secs

var DefCase = Class.create ({
    initialize: function (title, descr, checks, timeOut) {
        this.def = new Def (title, descr);
        this.checks = checks;
        this.exceptions = [];
        
        if (null == timeOut) {
            this.timeOut = DEFAULT_TIMEOUT;
        } else {
            this.timeOut = timeOut;
        }

        // add a so-called zero-test that ensures that initialization has been done
        this.checks.unshift ({
            "def": new Def (
                "zero test",
                "expected: during initialization no exception should be generated"
            ),

            check: function (context) {
                return {
                    result: 0 === this.exceptions.length,
                    dump: this.exceptions.toJSON ()
                };
            }.bind (this)
        });
    },

    post: function () {
        location.reload (true);
    },


    pre: function (robot) {
        window.onload = function (e) {
        
        	document.body.innerHTML += 	""
        							+	"<div"
        							+	" 	style=\"position: absolute; left: 10%; top: 10%; width: 80%; height: 80%; background-color: rgba(255, 0, 0, .75); z-index: 100\""
        							+	"><h1>Please, wait <div id='interval'>" + (this.timeOut/1000) +"</div> sec(s)</h1></div>"
			;
			
			window.setInterval (function () {
					var intervalObj = document.getElementById ('interval');
					var currVal = (intervalObj.innerHTML - 0.1).toFixed(2);
					intervalObj.innerHTML = currVal;
				},
				50
			);

            // when the page is loaded, then after a certain period,
            //  verify that the test has been passed
            window.setTimeout (
                function() {
                    // stop event monitor, thus prevent any garbage event
                    DEFAULT_EVENT_MONITOR. stop ();

                    robot.verifyChecks (this.checks);
                    this.post ();
                }.bind (this),

                this.timeOut
            );

        }.bind (this);
    },

    registerException: function (e) {
        this.exceptions.push (e);
    }
});

DefCase.buildDefaultFinalStatusChecker = function (expectedStatusName) {
    var expectedStatusValue = eval ("window.ApplicationCache." + expectedStatusName);

    return {
        "def": new Def (
            "`window.applicationCache.status'",
            "must be: " + expectedStatusValue + " (`" + expectedStatusName + "')"
        ),

        check: function (context) {
            return new DefRes (
                expectedStatusValue === window.applicationCache.status,
                window.applicationCache.status
            );
        }
    };
};

DefCase.buildDefaultEventOrderChecker = function (expectedEventOrder, eventMonitor) {
    if (null == eventMonitor) {
        eventMonitor = DEFAULT_EVENT_MONITOR;
    }

    return {
        "def": new Def (
            "check event order",
            "expected event order: " + expectedEventOrder
        ),

        check: function (context) {
            return eventMonitor.checkEventOrder (expectedEventOrder);
        }
    };
};

DefCase.buildDefaultEventValueChecker = function (eventMonitor) {
    if (null == eventMonitor) {
        eventMonitor = DEFAULT_EVENT_MONITOR;
    }

    return {
        "def": new Def (
            "check event values",
            "the test verifies the content of event values"
        ),

        check: function (context) {
            return eventMonitor.checkEventValues ();
        }
    };
};

/**
*/
DefCase.buildTestDataChecker = function (testContent) {
    return {
        "def": new Def (
            testContent.info,
            "expected values: " + testContent.client.expected.toJSON ()
        ),

        check: function (context) {
            var result = true;
            for (var ix = 0; ix < testContent.client.expected.length; ix++) {
                var expectedReg = testContent.client.expected[ix];
                var currentReg = testContent.client.current[ix];
                result &= expectedReg == currentReg;
            }
            
            return {
                "result": result,
                "dump": testContent.client.current
            };
        }
    };
};

var DefRes = Class.create ({
    initialize: function (result, dump) {
        this.result = result;
        this.dump = dump;
    }
});

var DefSrvInit = Class.create ({
    initialize: function (isRestart) {
        this.isRestart = isRestart;
    },
    
    init: function () {
        location.reload(this.isRestart);
    }
});

var DefCheck = Class.create ({
    initialize: function (title, descr) {
        this.def = new Def (title, descr);
    },
            
    check: function (context) {
        return new DefaultResult (false, "");
    }
});

/**
 * Default Environment Initializator
 *
 * The initializator set/rest manifest attribute for a particular master
 *
 * is maniefstIx is null it means that a master will not have `manifest' attributes
 */
var DefInitializator = Class.create (DefSrvInit, {
    initialize: function ($super, masterIx, manifestIx) {
        this.masterIx = masterIx;
        this.manifestIx = manifestIx;

        $super (true);
    },

    init: function ($super) {
        var newManifest = DEFAULT_MANIFEST;
        newManifest.meta.timestamp = new Date ().toUTCString(); // change the date, thus handle changes in the manifest
        var manifestIx = null == this.manifestIx ? 0 : this.manifestIx;
        new SimpleHTTP ().set (RES.MANIFEST, manifestIx, newManifest);

        // Exclude manifest from master
        var newMaster = DEFAULT_MASTER;

        // by setting manifest.ix as `null', it means that
        //  manifest attribute will not be included at all
        newMaster.manifest.ix = this.manifestIx;
        new SimpleHTTP ().set (RES.MASTER, this.masterIx, newMaster);

        // all magic is here
        $super ();
    }
});

// FIXME: the class is obsolete and it must be deleted
var BasicEventChecker = Class.create ({
    initialize: function (eventType) {
        this.eventType = eventType;
    },
    
    check: function (e) {
        return {
            isTypeOK: this.eventType == e.type,
            isTargetOK: window.applicationCache === e.target,
            isCurrentTargetOK: window.applicationCache === e.currentTarget,
            isEventPasheOK: 2 === e.eventPhase,
            isBubblesOK: false === e.bubbles,
            isCancelableOK: true === e.cancelable
            // left
            // timeStamp
            // namespaceURI
        };
    }
});

var BasicStatusChecker = Class.create ({
    initialize: function (expectedStatus) {
        this.expectedStatus = expectedStatus;
    },

    check: function() {
        return this.expectedStatus === window.applicationCache.status;
    }
});

function checkException (e, expected) {
    if (0 < JSON.stringify(e).search(expected)) {
        return true;
    } else {
        throw false;
    }
}

function handleException (fns, context, expected) {
    var handled = false;
    var exception = null;
    try {
        fns (context);
    } catch (e) {
        exception = e;
        try {
            handled = checkException (e, expected);
        } catch (ee) {
            exception = "" + e + "\n" + ee;
        }
    } finally {
        return new DefRes (handled, exception);
    }
}


function makeSrvInit (classObj) {
    return new classObj ();
}
