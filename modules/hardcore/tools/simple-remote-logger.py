# -*- coding: utf-8 -*-
"""
@author: mpawlowski
"""

import SocketServer

# Simple remote logger example. Prints whatever it receives
# from a socket to stdout. 
#
# Usage: Launch this script without parameters and then launch the debugged
# Opera instance. Obviously, same host and port must be set here
# and in Opera.

# NOTE: Not for production use.
class PrintingHandler(SocketServer.StreamRequestHandler):
    def handle(self):
        while(True):
            data = self.rfile.readline().strip()
            if(not data):
                break;
            # print "{} wrote:".format(self.client_address[0])
            print data

if __name__ == "__main__":
    HOST, PORT = "127.0.0.1", 9999
    server = SocketServer.TCPServer((HOST, PORT), PrintingHandler)
    server.serve_forever()
