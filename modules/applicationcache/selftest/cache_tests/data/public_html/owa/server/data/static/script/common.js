/**
 * A set of common classes used both in client and server sides
 * 
 * Note: the file must be included after `prototype' library,
 * in othere case it will not work.
 */


/**
 * Common Constants
 * 
 */
var RES = {
  MASTER: "MASTER",
  MANIFEST: "MANIFEST",

  RES_IMG: "RES_IMG",
  RES_REG: "RES_REG",

  TODO: "TODO",
  HISTORY: "HISTORY",
  
  CHECK_LIST: "CHECK_LIST"
};

/**
 * Simple HTTP request class
 *
 * The class wraps some send/receive via HTTP routines
 */
var SimpleHTTP = function () {};

SimpleHTTP.prototype = {
    http: new XMLHttpRequest(),
    response: {
        text: null
    },

    read: function () {
    },

    send: function (request) {
        this.http.open ("GET", request, false);
        var http = this.http;
        var response = this.response;
        this.http.onreadystatechange = function () {
            if (4 == http.readyState) {
                response.text = this.responseText;
            }
        };
        this.http.send (null);
        this.response = response;
        return this;
    },

    innerGet: function (path, name, ix) {
       return   this.send (
                        path
                    +   "?name=" + name
                    +   "&ix=" + ix
       ).response.text;
    },
    
    get: function (name, ix) {
        return JSON.parse (unescape (this.innerGet ("get", name, ix)));
    },

    res: function (name, ix) {
        return this.innerGet ("res", name, ix);
    },

    set: function (name, ix, value) {
        this.send (
                "set"
            +   "?name=" + name
            +   "&ix=" + ix
            +   "&value=" + escape (Object.toJSON (value))
        );
    }
};


var BaseResource = Class.create ({
    initialize: function (name, ix) {
        this.id = {
            "name": name,
            "ix": ix
        };

        this.http  = {
            status: {
                code: 200,
                text: "'" + this.id.name + "' (" + this.id.ix + ") has been successfully generated."
            },
            headers: [
            ]
        };
    },

    write : function (srv) {
        srv.res.setStatusCode (this.http.status.code, this.http.status.text);
        for (var ix = 0; ix < this.http.headers.length; ix++) {
            var header = this.http.headers[ix];
            srv.res.setResponseHeader (header.name, header.value);
        }
    }
});

var Master = Class.create (BaseResource, {
    initialize: function ($super, ix) {
        $super ("Master", ix);

        this.docType    =    "<!DOCTYPE html PUBLIC   \"-//W3C//DTD XHTML 1.0 Strict//EN\"\n"
                +       "                        \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\"\n"
                +    ">\n";

        // set XHTML content type
        this.http.headers.push ({
            name: "Content-Type",
            value: "text/html"
        });

        this.manifest =  {
            ix: null
        };

        this.resoruce = null;
    },

    write: function ($super, srv) {
        $super (srv);

        srv.res.write (this.docType);
        var manifestAttr = "";
        if (null != this.manifest.ix) {
            manifestAttr    =     " "
                            +    "manifest=\""
                            +    "res?name=MANIFEST&ix=" + this.manifest.ix
                            +    "\""
            ;
        }

        srv.res.writeLine ("<html" + manifestAttr + ">");

        var file = WEB_SERVER_ROOT_MP.open ("/root/server/data/core.xhtml", opera.io.filemode.READ);
                var data = file.readBytes (file.bytesAvailable);
                srv.res.writeBytes (data);

        srv.res.writeLine ("</html>");
    }
});

var Manifest = Class.create (BaseResource, {
    initialize: function ($super, ix) {
        $super ("Manifest", ix);

        // default manifest content
        this.content = {
            CACHE: [
                ["res?name=RES_IMG&ix=0"],
                ["res?name=RES_IMG&ix=1"],
                ["res?name=RES_REG&ix=0"],
            ],
            
            FALLBACK: [
                ["res?name=RES_REG", "res?name=RES_REG&ix=1"],
            ],
            
            NETWORK: [
                ["get?"],
                ["set?"],
                ["resource?"], // allow static resources be always fetched online
            ],

            isOpened: false
            };

            this.meta = {
                    magicSign: "CACHE MANIFEST",
                    extraData: "# This is a dynamically generated manifest.",
                    timestamp: new Date ().toUTCString()
            };

        // set XHTML content type
        this.http.headers.push ({
            name: "Content-Type",
            value: "text/cache-manifest"
        });

    },
    write: function ($super, srv) {
        $super (srv);

        var helperFns = function (sectionName, data, res) {
            res.writeLine (sectionName);
            for (var ix = 0; ix < data.length; ix++) {
                var tokens = data[ix];
                var str = "";
                for (var jx = 0; jx < tokens.length; jx++) {
                    str += tokens[jx] + " ";
                }
                res.writeLine (str);
            }
        }

        srv.res.writeLine (this.meta.magicSign);
        srv.res.writeLine (this.meta.extraData);

        helperFns ("CACHE:", this.content.CACHE, srv.res);
        helperFns ("FALLBACK:", this.content.FALLBACK, srv.res);
        helperFns ("NETWORK:", this.content.NETWORK, srv.res);

        if (this.content.isOpened) {
            srv.res.writeLine ("NETWORK:");
            srv.res.writeLine ("*");
        }

        srv.res.writeLine ("# Generated at: " + this.meta.timestamp);
    }
});

var Register = Class.create (BaseResource, {
    initialize: function ($super, ix, text) {
        $super ("Register", ix);

        this.http.headers.push ({
            name: "Content-Type",
            value: "text/plain"
        });

        this.text = text;
    },

    write: function ($super, srv) {
        $super (srv);

        srv.res.writeLine (this.text);
    }
});

var Image = Class.create (BaseResource, {
    initialize: function ($super, ix, xix) {
        $super ("Image", ix);
        this.xix = xix;

        this.http.headers.push ({
            name: "Content-Type",
            value: "img/png"
        });
    },

    write: function ($super, srv) {
        $super (srv);

        var file = WEB_SERVER_ROOT_MP.open ("/root/server/data/static/img/img." + this.xix + ".png", opera.io.filemode.READ);
        var data = file.readBytes (file.bytesAvailable);
        srv.res.writeBytes (data);
    }
});

var ToDo = Class.create ({
    initialize: function () {
        this.testNo = 0;
        this.initNo = 0;
        this.caseNo = 0;
        this.isRunnable = false; // by default test is not runnable
    }
});

var History = Class.create ({
});

var CheckList = Class.create ({
    initialize: function () {
        this.isRunnable = false;
    }
});
