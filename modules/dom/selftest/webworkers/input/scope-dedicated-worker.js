/* Checking that scope contains the required functions, values and sinks.*/
var scope = { 'importScripts': typeof importScripts,
	      'navigator':     typeof navigator,
	      'location':      typeof location,
	      'setInterval':   typeof setInterval,
	      'clearInterval': typeof clearInterval,
	      'setTimeout':    typeof setTimeout,
	      'clearTimeout':  typeof clearTimeout,
	      'onmessage':     typeof this.onmessage,
	      'onerror':       typeof this.onerror,
	      'postMessage':   typeof this.postMessage,
	      'self':          typeof self,
              '$scope':        self instanceof WorkerGlobalScope && self instanceof DedicatedWorkerGlobalScope && ('WorkerGlobalScope' in self) && !('SharedWorkerGlobalScope' in self)};

postMessage(scope);
