/* Checking that scope contains the required functions, values and sinks.*/
var result = {};
try
{
    result.FileReaderSync = typeof FileReaderSync;
    result.FileException = typeof FileException;
    result.FileReaderSyncPrototype = typeof FileReaderSync.prototype;
    result.FileReaderSyncPrototypeConstructor = typeof FileReaderSync.prototype.constructor;
    result.FileExceptionPrototype = typeof FileException.prototype;
    result.FileExceptionPrototypeConstructor = typeof FileException.prototype.constructor;
}
catch(e)
{
}

 postMessage(result);
