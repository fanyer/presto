from sys import argv, exit
from getopt import gnu_getopt
from os import environ, access, R_OK
from os.path import join, isfile
from logger import logger
from configuration import configuration

options, arguments = gnu_getopt(argv[1:], "fnp:x:", "listen-port")

listenPort = 9999
initFile = join(environ.get("HOME", ""), ".esdbinit")
commandFile = None

for option, value in options:
    if option == "-f": configuration["gud"] = True
    elif option == "-x": commandFile = value
    elif option == "-n": initFile = None
    elif option in ("-p", "--listen-port"):
        try:
            listenPort = int(value)
            if listenPort < 1 or listenPort > 65535: raise ValueError
        except ValueError:
            logger.logFatal("%s: invalid port (must be integer between 0 and 65536)" % argv[0])
            exit(1)

lines = []

if initFile and isfile(initFile) and access(initFile, R_OK): lines.extend([line.strip() for line in open(initFile, "r").readlines()])
if commandFile and isfile(commandFile) and access(commandFile, R_OK): lines.extend([line.strip() for line in open(commandFile, "r").readlines()])

from debugger import Debugger, DebuggerError
from commands import CommandError, loadCommandsFromDirectory

debugger = Debugger()

try:
    commands = loadCommandsFromDirectory("commands")
    for name, command in commands: debugger.addCommand(name, command)
except CommandError, error:
    logger.logFatal(error[0])
    exit(1)
except DebuggerError, error:
    logger.logFatal(error[0])
    exit(1)

debugger.getSocketManager().listen(listenPort)
debugger.run(lines)
