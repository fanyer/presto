#!/usr/bin/python

from CommandLineParameters  import CommandLineParameters
from Timer                  import Timer
from subprocess             import *

import os, sys

class Main:
    _pwd = None
    
    def __init__(self, widget = False, body = False, background = False, signatures = False, www = False, single = False):
        self._pwd = "/home/fuyara/projects/core/tasks/CORE-34031/testcases"
        
        print "Actions: widget [%s], body [%s], background [%s], signatures [%s], www [%s]" % (widget, body, background, signatures, www)
        
        fp      = open("combiantions.txt", "r")
        i       = 0
        
        for line in fp:
            line = line.strip()
            
            if (len(line) != 4): continue
            
            i          += 1
            
            if (single and (line!=single)): continue
            
            d = "%02d-%s" % (i, line)
            
            print "processing: %s" % (d)
            
            if (widget):        self.widget(d)
            
            if not (os.path.exists("test-cases/ocsp-1/%s" % (d))):
                print " - doesn't exist!"
                continue
            # if
            
            if not (os.path.isdir("test-cases/ocsp-1/%s/" % (d))): continue
            
            if (body):          self.body(d)
            if (background):    self.background(d)
            if (signatures):    self.signatures(d)
            if (www):           self.www(d)
        # for in
    # def __init__
    
    def widget(self, d):
        LOGFP       = open("log.create-test-case.txt", "a")
        LOGFP.write("\n")
        LOGFP.write("-----------------------------------------------\n")
        LOGFP.write("%s/tools/create-test-case.sh ocsp-1 %s\n" % (self._pwd, d))
        LOGFP.write("-----------------------------------------------\n")
        LOGFP.close()
        LOGFP       = open("log.sign-widget.txt", "a")
        
        handle      = Popen("%s/tools/create-test-case-modified.sh ocsp-1 %s" % (self._pwd, d), shell=True, stdout=LOGFP, stderr=LOGFP, cwd="%s/tools" % (self._pwd), close_fds=True)
        runTimer    = Timer()
        
        while True:
            if (runTimer.elapsed() > 60):
                handle.terminate()
            # if
            
            handle.poll()
            
            if (handle.returncode != None):
                break
            # if
        # while
        
        LOGFP.write("-------------------- done --------------------\n")
        LOGFP.close()
        print " - created widget [returncode: %d]" % (handle.returncode)
    # def
    
    def body(self, d):
        fp      = open("test-cases/ocsp-1/%s/index.html" % (d), "r")
        content = fp.read()
        fp.close()
        
        status  = {"g":"good", "r":"revoked", "u":"unknown"}
        
        author  = status[d[3]]
        sign_1  = status[d[4]]
        sign_2  = status[d[5]]
        sign_3  = status[d[6]]
        
        content = content.replace("</body>", "<p>Signed with:</p>\n<ol>\n\t<ul>author: %s</ul>\n\t<ul>sign_1: %s</ul>\n\t<ul>sign_2: %s</ul>\n\t<ul>sign_3: %s</ul>\n</ol>\n\n</body>" % (author, sign_1, sign_2, sign_3))
        
        fp      = open("test-cases/ocsp-1/%s/index.html" % (d), "w")
        fp.write(content)
        fp.close()
        print " - added body"
    # def body
    
    def background(self, d):
        fp      = open("test-cases/ocsp-1/%s/index.html" % (d), "r")
        content = fp.read()
        fp.close()
        
        if ((d[3] == "g") or (d[4] == "g") or (d[5] == "g") or (d[6] == "g")):  color = "green"
        else:                                                                   color = "red"
        
        content = content.replace("<body style=\"background:#cccccc;\">", "<body style=\"background:%s;\">" % (color))
        
        fp      = open("test-cases/ocsp-1/%s/index.html" % (d), "w")
        fp.write(content)
        fp.close()
        print " - changed background"
    # def background
    
    def signatures(self, d):
        print " - signing:"
        cert  = {"g":"good", "r":"revoked", "u":"unknown"}
        
        if (os.path.exists("test-cases/ocsp-1/%s/author-signature.xml"  % (d   ))): os.remove("test-cases/ocsp-1/%s/author-signature.xml"   % (d   ))
        if (os.path.exists("test-cases/ocsp-1/%s/signature1.xml"        % (d   ))): os.remove("test-cases/ocsp-1/%s/signature1.xml"         % (d   ))
        if (os.path.exists("test-cases/ocsp-1/%s/signature2.xml"        % (d   ))): os.remove("test-cases/ocsp-1/%s/signature2.xml"         % (d   ))
        if (os.path.exists("test-cases/ocsp-1/%s/signature3.xml"        % (d   ))): os.remove("test-cases/ocsp-1/%s/signature3.xml"         % (d   ))
        if (os.path.exists("test-cases/ocsp-1/%s/%s.wgt"                % (d, d))): os.remove("test-cases/ocsp-1/%s/%s.wgt"                 % (d, d))
        
        self._sign("author-signature.xml", d, cert[d[3]])
        self._sign("signature1.xml",       d, cert[d[4]])
        self._sign("signature2.xml",       d, cert[d[5]])
        self._sign("signature3.xml",       d, cert[d[6]])
        self._repack(d)
    # def background
    
    def _sign(self, f, d, cert):
        p12 = "%s/keys/%s.rsa.p12" % (self._pwd, cert)
        
        if (f == "author-signature.xml"):
            sig = "-a"
            sid = "%s-author" % (d)
        else:
            sig = "-o %s" % (f)
            
            if (f[9] == "1"):   sid = "%s" % (d)
            else:               sid = "%s-%s" % (d, f[9])
        # if elif
        
        crt = "%s/keys/2.rsa.cert.pem" % (self._pwd)
        wgt = "%s/test-cases/ocsp-1/%s" % (self._pwd, d)
        
        LOGFP       = open("log.sign-widget.txt", "a")
        LOGFP.write("\n")
        LOGFP.write("----------------------------------------------\n")
        LOGFP.write("%s/tools/sign-widget.sh --pwd secret --pkcs12  %s %s -i w3c-testsuite-id-ocsp-1-%s -c %s %s" % (self._pwd, p12, sig, sid, crt, wgt))
        LOGFP.write("----------------------------------------------\n")
        LOGFP.close()
        LOGFP       = open("log.sign-widget.txt", "a")
        
#        print "   - run: %s/tools/sign-widget.sh --pwd secret --pkcs12  %s %s -i w3c-testsuite-id-ocsp-1-%s -c %s %s" % (self._pwd, p12, sig, sid, crt, wgt)
        handle      = Popen("%s/tools/sign-widget-modified.sh --pwd secret --pkcs12  %s %s -i w3c-testsuite-id-ocsp-1-%s -c %s %s" % (self._pwd, p12, sig, sid, crt, wgt), shell=True, stdout=LOGFP, stderr=LOGFP, cwd="%s/tools" % (self._pwd), close_fds=True)
        runTimer    = Timer()
        
        while True:
            if (runTimer.elapsed() > 60):
                handle.terminate()
            # if
            
            handle.poll()
            
            if (handle.returncode != None):
                break
            # if
        # while
        
        LOGFP.write("-------------------- done --------------------\n")
        LOGFP.close()
        print "   - %s [returncode: %d]" % (f, handle.returncode)
     # def _sign
    
    def _repack(self, d):
        LOGFP       = open("log.repack-test-case.txt", "a")
        LOGFP.write("\n")
        LOGFP.write("----------------------------------------------\n")
        LOGFP.write("%s/tools/repack-test-case.sh ocsp-1 %s -u\n" % (self._pwd, d))
        LOGFP.write("----------------------------------------------\n")
        LOGFP.close()
        LOGFP       = open("log.sign-widget.txt", "a")
        
        handle      = Popen("%s/tools/repack-test-case.sh ocsp-1 %s" % (self._pwd, d), shell=True, stdout=LOGFP, stderr=LOGFP, cwd="%s/tools" % (self._pwd), close_fds=True)
        runTimer    = Timer()
        
        while True:
            if (runTimer.elapsed() > 60):
                handle.terminate()
            # if
            
            handle.poll()
            
            if (handle.returncode != None):
                break
            # if
        # while
        
        LOGFP.write("-------------------- done --------------------\n")
        LOGFP.close()
        print "   - repack widget [returncode: %d]" % (handle.returncode)
    # def _sign
    
    def www(self, d):
        if ((not os.path.exists("%s/test-cases/ocsp-1-www/" % (self._pwd))) or (not os.path.isdir("%s/test-cases/ocsp-1-www/" % (self._pwd)))):
            os.mkdir("%s/test-cases/ocsp-1-www/" % (self._pwd))
        
        os.rename("%s/test-cases/ocsp-1/%s/%s.wgt" % (self._pwd, d, d), "%s/test-cases/ocsp-1-www/%s.wgt" % (self._pwd, d))
        print " - moved to www folder"
    # def www
# class Main

if (__name__ == "__main__"):
    from CommandLineParameters import CommandLineParameters
    
    if (CommandLineParameters.readFromFile('auto.clp')): sys.exit(1)
    if (CommandLineParameters.readArgs(sys.argv)):       sys.exit(1)

    start = Main(
                    widget      = CommandLineParameters.getValue("widget"       ),
                    body        = CommandLineParameters.getValue("body"         ),
                    background  = CommandLineParameters.getValue("background"   ),
                    signatures  = CommandLineParameters.getValue("signatures"   ),
                    www         = CommandLineParameters.getValue("www"          ),
                    single      = CommandLineParameters.getValue("single"       )
                )
# if
