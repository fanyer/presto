#!/usr/bin/env python

# Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
#
# This file is part of the Opera web browser.
# It may not be distributed under any circumstances.
#
# Generate and verify signatures for protected resource files.
# Requires openssl (either on system path or specified by '--openssl' option).
#
# 'create' mode
# - find all files in current directory and its subdirectories
#   that are covered by protection mechanism
# - for each file generate RSA signature and write it into
#   files.sig file in current directory
#
# 'verify' mode
# - load files.sig file
# - find all files in current directory and its subdirectories
#   that are covered by protection mechanism
# - check that all paths in files.sig are valid
# - check that all files are covered by files.sig
# - check that all signatures in files.sig are valid
#
# Exit code:
#  0 on success
#  1 on error(s)
#
# Examples:
#
# signatures.py create -k key.pem
#   generate signatures with private key loaded from 'key.pem'
#
# signatures.py verify -k key.pem -o c:\openssl\openssl.exe
#   verify signatures with public key loaded from 'key.pem' and
#   using OpenSSL executable c:\openssl\openssl.exe
#

import sys
import os

class Sys:
    def __init__(self, extra_verbose=False, extra_quiet=False):
        self.extra_verbose = extra_verbose and not extra_quiet
        self.extra_quiet = extra_quiet
        self.exit_code = 0

    def LogInfo(self, msg):
        if not self.extra_quiet:
            if len(msg) > 0 and msg[-1] != '\n':
                msg = '%s\n' % msg
            sys.stdout.write(msg)

    def LogDebug(self, msg):
        if self.extra_verbose:
            if len(msg) > 0 and msg[-1] != '\n':
                msg = '%s\n' % msg
            sys.stdout.write(msg)

    def LogError(self, msg):
        if not self.extra_quiet:
            if len(msg) > 0 and msg[-1] != '\n':
                msg = '%s\n' % msg
            sys.stdout.write(msg)

    def IsVerbose(self):
        return self.extra_verbose

    def SetExitCode(self, exit_code):
        self.exit_code = exit_code

    def Exit(self, exit_code=None):
        if exit_code == None:
            exit_code = self.exit_code
        sys.exit(exit_code)

    def Chdir(self, path):
        try:
            os.chdir(path)
        except:
            self.LogError('error while changing directory to %s' % path)
            self.Exit(1)


from subprocess import Popen, PIPE

class OpensslSignature:
    def __init__(self, sys_wrapper, openssl_path, key_path, passpath=None, passphrase=None):
        self.key_path = key_path
        self.passphrase = passphrase
        self.passpath = passpath
        self.openssl_path = openssl_path
        self.sys = sys_wrapper

    def generate(self, path):
        if self.sys.IsVerbose():
            self.sys.LogDebug('OpensslSignature.generate: %s' % path)
        digest = self.__digest(path)
        if digest:
            rsautl_out = self.__rsautl(digest, True)
            if rsautl_out:
                return rsautl_out.encode('hex')
        return None

    def verify(self, path, hexsig):
        if self.sys.IsVerbose():
            self.sys.LogDebug('OpensslSignature.verify: %s' % path)
        digest = self.__digest(path)
        if digest == None:
            return False
        try:
            sig = hexsig.decode('hex')
        except:
            if self.sys.IsVerbose():
                self.sys.LogDebug('invalid hex signature')
            return False
        rsautl_out = self.__rsautl(sig, False)
        if rsautl_out:
            return digest == rsautl_out
        return False

    def __rsautl(self, inp, sign):
        rsa_args = [self.openssl_path,'rsautl','-pkcs','-inkey',self.key_path]
        if sign:
            rsa_args.append('-sign')
        else:
            rsa_args.append('-pubin')
            rsa_args.append('-verify')
        if self.passpath:
            rsa_args.append('-passin')
            rsa_args.append('file:%s' % self.passpath)
        elif self.passphrase:
            rsa_args.append('-passin')
            rsa_args.append('pass:%s' % self.passphrase)
        try:
            rsa_proc = Popen(rsa_args, stdin=PIPE, stdout=PIPE, stderr=PIPE)
            (rsa_out, rsa_err) = rsa_proc.communicate(inp)
        except:
            self.sys.LogError('error while executing rsautl (openssl=%s)' % self.openssl_path)
            self.sys.Exit(1)
        self.__maybe_print_command_output(rsa_args, rsa_proc.returncode, rsa_err)
        if rsa_proc.returncode == 0:
            return rsa_out
        else:
            return None

    def __digest(self, path):
        # read file contents and remove all eoln characters - this ensures
        # that digest is the same in Linux/Mac/Windows checkouts
        try:
            f = open(path,'rb')
            text = f.read()
            f.close()
            text = text.translate(None, '\r\n')
        except IOError as err:
            self.sys.LogError('IOError while reading %s: %s' % (path, err.strerror))
            return None
        dgst_args = [self.openssl_path,'dgst','-sha1','-binary']
        try:
            dgst_proc = Popen(dgst_args, stdin=PIPE, stdout=PIPE, stderr=PIPE)
            (dgst_out, dgst_err) = dgst_proc.communicate(text)
        except:
            self.sys.LogError('error while executing dgst (openssl=%s)' % self.openssl_path)
            self.sys.Exit(1)
        self.__maybe_print_command_output(dgst_args, dgst_proc.returncode, dgst_err)
        if dgst_proc.returncode != 0:
            return None
        return dgst_out

    def __maybe_print_command_output(self, command_and_args, retcode, stderr):
        if retcode != 0 or self.sys.IsVerbose():
            output = '%s\n%sexit code: %d\n' % (' '.join(command_and_args), stderr, retcode)
            if retcode != 0:
                self.sys.LogError(output)
            else:
                self.sys.LogDebug(output)

if __name__ == '__main__':
    import os
    from optparse import OptionParser

    usage = 'usage: %prog [options] [create|verify]'
    parser = OptionParser(usage=usage)
    parser.add_option('-o', '--openssl', default='openssl', dest='openssl',
                      help='path to openssl executable',
                      metavar='FILE')
    parser.add_option('-k', '--key', dest='key',
                      help="path to file with private ('create' mode) or public ('verify' mode) RSA key",
                      metavar='FILE')
    parser.add_option('-v', '--verbose', default=False,
                      action='store_true', dest='verbose',
                      help='be extra verbose')
    parser.add_option('-q', '--quiet', default=False,
                      action='store_true', dest='quiet',
                      help='be extra quiet')
    parser.add_option('-d', '--dest', dest='dest',
                      help='directory with protected files and files.sig',
                      metavar='DIRECTORY')
    parser.add_option('-g', '--getpass', default=None, dest='getpass',
                      help='path to file with pass phrase for key, or \'stdin\'',
                      metavar='FILE')

    (options, args) = parser.parse_args()

    sys_wrapper = Sys(options.verbose, options.quiet)

    if not options.key:
        sys_wrapper.LogError('--key option is required')
        sys_wrapper.Exit(1)
    if not os.path.isfile(options.key):
        sys_wrapper.LogError('%s does not exist' % options.key)
        sys_wrapper.Exit(1)

    create = False
    verify = False
    for arg in args:
        if arg == 'create':
            create = True
        elif arg == 'verify':
            verify = True
        else:
            sys_wrapper.LogError('unknown command: %s' % arg)
            sys_wrapper.Exit(1)
    if create and verify:
        sys_wrapper.LogError("run with 'create' or 'verify', not both")
        sys_wrapper.Exit(1)
    elif not create and not verify:
        sys_wrapper.LogError("nothing to do (run with 'create' or 'verify')")
        sys_wrapper.Exit(1)

    passpath = None
    passphrase = None
    if options.getpass:
        if options.getpass == 'stdin':
            import getpass
            passphrase = getpass.getpass('Enter pass phrase for %s:' % os.path.basename(options.key))
        else:
            passpath = options.getpass

    if passpath and not os.path.isfile(passpath):
        sys_wrapper.LogError('%s does not exist' % options.key)
        sys_wrapper.Exit(1)

    if options.dest:
        options.key = os.path.abspath(options.key)
        if options.openssl and os.path.isfile(options.openssl):
            options.openssl = os.path.abspath(options.openssl)
        if passpath:
            passpath = os.path.abspath(passpath)
        sys_wrapper.Chdir(options.dest)

    signature = OpensslSignature(sys_wrapper, options.openssl, options.key, passpath, passphrase)

    if create:
        path_and_sig_list = []
        for root,dirs,names in os.walk('.'):
            if 'search.ini' in names:
                path = os.path.normpath(os.path.join(root, 'search.ini'))
                # standardize on '/' to avoid unnecessary changes if files.sig is updated on systems
                # with different path separators
                path = path.replace('\\','/')
                sig = signature.generate(path)
                sys_wrapper.LogInfo('%s: %s' % (path, 'ok' if sig else 'error'))
                if not sig:
                    sys_wrapper.Exit(1)
                path_and_sig_list.append((path, sig))

        path_and_sig_list.sort(None, lambda x: x[0])
        try:
            out_f = open('files.sig', 'w')
            for path_and_sig in path_and_sig_list:
                out_f.write('%s %s\n' % path_and_sig)
            out_f.close()
        except IOError as err:
            sys_wrapper.LogError('IOError while writing to files.sig: %s' % err.strerror)
            sys_wrapper.Exit(1)

    if verify:
        path_to_sig_map = {}
        try:
            for line in file('files.sig'):
                path_and_sig = line.split(' ')
                path = os.path.normpath(path_and_sig[0])
                path_to_sig_map[path] = path_and_sig[1].rstrip()
        except IOError as err:
            sys_wrapper.LogError('IOError while reading from files.sig: %s' % err.strerror)
            sys_wrapper.Exit(1)

        for root,dirs,names in os.walk('.'):
            if 'search.ini' in names:
                path = os.path.normpath(os.path.join(root, 'search.ini'))
                if path_to_sig_map.has_key(path):
                    if signature.verify(path, path_to_sig_map[path]):
                        sys_wrapper.LogInfo('%s: ok' % path)
                    else:
                        sys_wrapper.LogInfo('%s: signature check failed' % path)
                        sys_wrapper.SetExitCode(1)
                    del(path_to_sig_map[path])
                else:
                    sys_wrapper.LogInfo('%s: not in files.sig' % path)
                    sys_wrapper.SetExitCode(1)
        for path in path_to_sig_map.iterkeys():
            sys_wrapper.LogInfo('%s: only in files.sig' % path)
            sys_wrapper.SetExitCode(1)

    sys_wrapper.Exit()
