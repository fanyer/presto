var WEB_SERVER = null;

var DEF_INDEX_FILE = "server/data/index.xhtml";
var WEB_SERVER_ROOT_MP = null;

var TRANSLATE_TABLE = [
    0,
    1,
    2,
];

var RESOURCES = {
    MASTER: [
        new Master (0), 
    ],

    MANIFEST: [
        new Manifest (0),
    ],

    RES_REG: [
        new Register (0, "A"),
        new Register (1, "B"),
        new Register (2, "X"),
    ],

    RES_IMG: [
        new Image (0, 0),
        new Image (1, 1),
        new Image (2, 2),
    ],

    HISTORY: [
    ],

    TODO: [
        new ToDo (),
    ],

    CHECK_LIST: []
};

var LAST_TRANSLATION = "default";

function WebServer (context) {
    this.context = context;
}

WebServer.prototype = {
    get res () {
        return this.context.connection.response;
    },

    get req () {
        return this.context.connection.request;
    },

    close: function () {
        this.res.close ();
    },

    writeFile: function (fileName) {
        var file = WEB_SERVER_ROOT_MP.resolve (fileName);
        this.res.writeFile (file);
    },

    redirect: function () {
        this.res.setStatusCode (302);
        this.res.setResponseHeader( 'Location', WEB_SERVER.currentServicePath );
    }
};

var LISTENERS = [
    {
        /**
            Default listener
        */
        path: "_request",
        listener: function (e) {
            var srv = new WebServer (e);
        }
    },

    {
        path: "_index",
        listener: function (e) {
            var srv = new WebServer (e);
            srv.writeFile ("/root/server/data/index.xhtml");
            srv.res.close ();
        }
    },
    
    {
        path: "res",
        listener: function (e) {
            var srv = new WebServer (e);

            var name = srv.req.queryItems["name"][0];
            var ix   = srv.req.queryItems["ix"][0];

            var obj = RESOURCES[name][ix];
            obj.write (srv);

            srv.close ();
        }
    },

    {
        /**
            Generate resource data
        */
        path: "resource",
        listener: function (e) {
            var srv = new WebServer (e);
            var file = null;
            var arr = srv.req.queryItems['static'];
            var isStatic = null == arr ? 'false' : arr[0];
            if (null == isStatic || 'false' == isStatic) {
                var ix = srv.req.queryItems['ix'][0];
                var xix = TRANSLATE_TABLE[ix];
                file = WEB_SERVER_ROOT_MP.open ("/root/server/data/static/img/img." + xix + ".png", opera.io.filemode.READ);
                srv.res.setResponseHeader ("Content-Type", "image/png");

            } else {
                var path = srv.req.queryItems['path'][0];
                file = WEB_SERVER_ROOT_MP.open ("/root/server/data/static/" + path, opera.io.filemode.READ);
            }
    
            var data = file.readBytes (file.bytesAvailable);

            srv.res.writeBytes (data);
            srv.close ();       
        }
    },

    {
        path: "get",
        listener: function (e) {
            var srv = new WebServer (e);

            var name  = srv.req.queryItems["name"][0];
            var ix    = srv.req.queryItems["ix"][0];

            var objStr;
            if (RESOURCES[name].length <= ix) {
                objStr = JSON.stringify(null);
            } else {
                var obj = RESOURCES[name][ix];
                if (null == obj.toJSON) {
                    objStr = JSON.stringify (obj);
                } else {
                    objStr = obj.toJSON ();
                }
            }
            srv.res.writeLine (escape (objStr));

            srv.close ();
        }
    },

    {
        path: "set",
        listener: function (e) {
            var srv   = new WebServer (e);

            var name  = srv.req.queryItems["name"][0];
            var ix    = srv.req.queryItems["ix"][0];
            var value = srv.req.queryItems["value"][0];

            var obj = JSON.parse (unescape (value));
            var castObj = null;
            var CAST_TYPES = [
                [RES.MASTER, Master], 
                [RES.MANIFEST, Manifest],
                [RES.RES_IMG, Image],
                [RES.RES_REG, Register],
                [RES.TODO, ToDo],
                [RES.CHECK_LIST, CheckList],
                //["HISTORY", History],
            ];

            if (RES.HISTORY == name) {
                castObj = obj;
            } else {
	            for (var jx = 0; jx < CAST_TYPES.length; jx++) {
	                if (CAST_TYPES[jx][0] === name) {
	                    castObj = Object.extend (new CAST_TYPES[jx][1](ix), obj);
	                    break;
	                }
	            }
            }

            if (RESOURCES[name].length < ix) {
                for (var kx = 0; kx <= ix; kx++)
                    RESOURCES[name].push (null);
            }
            RESOURCES[name][ix] = castObj;
            
            srv.close ();
        }
    },

        {
            path: "init",
            listener: function (e) {
                var srv   = new WebServer (e);

                RESOURCES.CHECK_LIST = []; // reset all check lists

                var testAmount = srv.req.bodyItems["test-amount"][0];
                var testNos = srv.req.bodyItems["test-no"];

                // init check list by default values
                for (var ix = 0; ix <= testAmount; ix++) {
                    RESOURCES.CHECK_LIST.push (new CheckList());
                }

                if (null == testNos) {
                    // nothing to do, escape this
                } else {
                    // enable certain ToDos
                    for (var ix = 0; ix < testNos.length; ix++) {
                        var runnableTestIx = testNos[ix];
                        RESOURCES.CHECK_LIST[runnableTestIx].isRunnable = true;
                    }
                }

                srv.writeFile ("/root/server/data/unit-test.xhtml");
                srv.res.close ();
            }
        }

];

window.onload = function () {
    WEB_SERVER = opera.io.webserver;

    // mount application folder
    WEB_SERVER_ROOT_MP = opera.io.filesystem.mountSystemDirectory("application", "root");

    if (WEB_SERVER) {
        // init Unite Serives
        for (var ix = 0; ix < LISTENERS.length; ix++) {
            var iniData = LISTENERS[ix];
            opera.io.webserver.addEventListener (iniData.path, iniData.listener, false);
        }
    }
}