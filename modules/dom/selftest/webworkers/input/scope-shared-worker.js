/* Checking that scope contains the required functions, values and sinks.*/
var scope = { 'importScripts': typeof importScripts,
	      'navigator':     typeof navigator,
	      'location':      typeof location,
	      'setInterval':   typeof setInterval ,
	      'clearInterval': typeof clearInterval,
	      'setTimeout':    typeof setTimeout,
	      'clearTimeout':  typeof clearTimeout,
	      'onerror':       typeof onerror,
	      'self':          typeof self,
	      'onconnect':     typeof onconnect,
	      'name':          typeof name,
	      'applicationCache': typeof applicationCache,
              '$scope':        self instanceof WorkerGlobalScope && self instanceof DedicatedWorkerGlobalScope && ('WorkerGlobalScope' in self) && !('DedicatedWorkerGlobalScope' in self)};

onconnect = function(e) {
    e.ports[0].postMessage(scope);
};
