import socket
import os
import sys
from logger import logger
from threading import Thread, RLock
from pollfallback import poll, error, POLLIN, POLLOUT, EINTR

class Close: pass

class SocketHandler:
    def getSocket(self): return None
    def setSocket(self, socket): pass

    def getWriteBuffer(self): return ""
    def setWriteBuffer(self, buffer): pass

    def getReadBuffer(self): return ""
    def setReadBuffer(self, buffer): pass

class SocketManagerListener:
    def getSocketHandler(self, socket): pass

class SocketManager(Thread):
    def __init__(self):
        Thread.__init__(self)
        self.__sockets = []
        self.__listenSocket = None
        self.__listener = None
        self.__closed = False
        self.__lock = RLock()
        if sys.platform != "win32": self.__rfd, self.__wfd = os.pipe()

    def setListener(self, listener): self.__listener = listener

    def __add(self, handler):
        self.__sockets.append(handler)

    def __close(self, handler):
        self.__sockets.remove(handler)

        try: handler.setSocket(None)
        except: logger.logException()

    def __signal(self):
        if sys.platform != "win32": os.write(self.__wfd, " ")

    def listen(self, port):
        self.__lock.acquire()

        try:
            if self.__listenSocket:
                self.__listenSocket.close()

            self.__listenSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.__listenSocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.__listenSocket.bind(("", port))
            self.__listenSocket.listen(5)
        finally:
            self.__lock.release()

    def run(self):
        while not self.__closed:
            pollobject = poll()

            if sys.platform != "win32": pollobject.register(self.__rfd, POLLIN)

            self.__lock.acquire()
            for handler in self.__sockets:
                if handler.getSocket() is not None:
                    flags = 0

                    if handler.getReadBuffer() is not None: flags = POLLIN
                    if handler.getWriteBuffer() is not None: flags = POLLOUT

                    pollobject.register(handler.getSocket().fileno(), flags)
            if self.__listenSocket:
                pollobject.register(self.__listenSocket.fileno(), POLLIN)
            self.__lock.release()

            if sys.platform == "win32": timeout = 0.1
            else: timeout = None

            while True:
                try:
                    ready = pollobject.poll(timeout)
                    break
                except error, (code, msg):
                    if code != EINTR: raise

            self.__lock.acquire()
            for fileno, flag in ready:
                if sys.platform != "win32" and fileno == self.__rfd:
                    os.read(self.__rfd, 1024)
                    continue

                if self.__listenSocket and fileno == self.__listenSocket.fileno():
                    logger.logDebug("connections", "New connection.")
                    newSocket, newAddress = self.__listenSocket.accept()
                    newSocket.setblocking(0)
                    newHandler = self.__listener.getSocketHandler(newSocket)
                    if newHandler:
                        self.__add(newHandler)

                for handler in self.__sockets[:]:
                    if handler.getSocket() is not None and handler.getSocket().fileno() == fileno:
                        if flag == POLLIN:
                            try:
                                data = handler.getSocket().recv(16384)
                                if len(data) == 0: self.__close(handler)
                                else:
                                    try:
                                        handler.setReadBuffer(handler.getReadBuffer() + data)
                                    except Close:
                                        self.__close(handler)
                            except:
                                logger.logException()
                                self.__close(handler)
                                break
                        elif flag == POLLOUT:
                            try:
                                buffer = handler.getWriteBuffer()
                                written = handler.getSocket().send(buffer)
                                if written == len(buffer): handler.setWriteBuffer(None)
                                else: handler.setWriteBuffer(buffer[written:])
                            except:
                                logger.logException()
                                self.__close(handler)
                                break
                        else:
                            self.__close(handler)
                            break
            self.__lock.release()

    def exit(self):
        self.__lock.acquire()
        while self.__sockets: self.__close(self.__sockets[0])
        self.__closed = True
        self.__signal()
        self.__lock.release()
        self.join()

    def signal(self):
        self.__lock.acquire()
        self.__signal()
        self.__lock.release()
