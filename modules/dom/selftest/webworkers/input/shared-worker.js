onconnect = function (e) { var prt = e.ports[0]; prt.onmessage = function(e) { prt.postMessage(e.data + '=' + e.data); }; };
