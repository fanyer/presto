from socketmanager import SocketHandler, Close
from session import Session
from message import MessageData, Message
from logger import logger

class Connection(SocketHandler):
    def __init__(self, socket):
        self.__session = None
        self.__socketmanager = None
        self.__socket = socket
        self.__rbuffer = ""
        self.__wbuffer = None

    def setSession(self, session): self.__session = session
    def getSession(self): return self.__session

    def setSocketManager(self, socketmanager): self.__socketmanager = socketmanager
    def getSocketMandler(self): return self.__socketmanager

    def sendMessage(self, message):
        if self.__wbuffer is None: self.__wbuffer = str(message)
        else: self.__wbuffer += str(message)
        self.__socketmanager.signal()

    def getSocket(self):
        return self.__socket

    def setSocket(self, socket):
        self.__socket = socket
        if not socket: self.__session.connectionClosed()

    def getWriteBuffer(self):
        return self.__wbuffer

    def setWriteBuffer(self, buffer):
        self.__wbuffer = buffer

    def getReadBuffer(self):
        return self.__rbuffer

    def setReadBuffer(self, buffer):
        logger.logDebug("send", "Buffer length (in): %d" % len(buffer))

        while len(buffer) >= 12:
            size = int(buffer[0:8], 16)
            length = int(buffer[8:12], 16)

            if len(buffer) - 12 < size: break

            try: self.parseMessage(size, length, buffer[12:12 + size])
            except Close: raise
            except: logger.logException()

            buffer = buffer[12 + size:]
            logger.logDebug("send", "Buffer length (loop): %d" % len(buffer))

        logger.logDebug("send", "Buffer length (out): %d" % len(buffer))
        self.__rbuffer = buffer

    def parseMessage(self, size, length, data):
        message = Message.parseMessage(self.__session, MessageData(size, length, data))
        self.__session.handleMessage(message)

    def getAddress(self):
        if self.__socket: return "%s:%d" % self.__socket.getpeername()
        else: return "<no socket>"

    def getShortDescription(self):
        if self.__socket: return "connected to %s" % self.getAddress()
        else: return "not connected"
