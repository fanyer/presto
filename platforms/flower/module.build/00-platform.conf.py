
import sys

def hostPlatform():
    return config.AutoPlatform()

def targetPlatform():
    return config.hostPlatform

class AutoPlatform(object):

    def wordsize(self):
        bytes = len(format(sys.maxint, 'x')) / 2
        assert bytes in (4, 8)
        return bytes * 8

    def bigendian(self):
        return sys.byteorder == 'big'

    def preprocessor(self):
        return config.GCC('preprocessor')

    def compiler(self, lang='c++'):
        if lang in ('c++', 'c', 'assembler-with-cpp'):
            return config.GCC('compiler')
        else:
            assert False, "No compiler configured for language {lang}".format(lang=lang)

    def linker(self):
        return config.GCC('linker')

    def gmake(self):
        return 'make' if sys.platform.startswith('linux') else 'gmake'

    def gmakeFlags(self):
        flags = []
        jobs = config.processQuota
        if jobs > 1:
            flags += ['-j', str(jobs)]
        if not config.failEarly:
            flags += ['-k']
        return flags
