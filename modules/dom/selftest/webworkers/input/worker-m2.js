/* from Mozilla bug612362 */ var i = 0; var interval = setInterval(function() { try {  i++; if (arguments.callee.done) return;  arguments.callee.done = true;  postMessage('FAIL'); } catch (e) {  postMessage('FAIL'); };}, 100);clearInterval(interval);setTimeout(function() {  if (i == 0) postMessage('PASS'); else postMessage('FAIL');}, 500);

