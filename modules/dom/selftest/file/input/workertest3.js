onmessage = function (e) {
  var b = new Blob([e.data, "def"]);
  var r = new FileReaderSync();

  var buffer = r.readAsArrayBuffer(b);
  var str = "";
  for (var i = 0; i < buffer.length; i++)
    str += String.fromCharCode(buffer[i]);
  postMessage(str);
};

