
class GCC(default.GCC):

    def codeFlags(self):
        flags = super(GCC, self).codeFlags
        if config.targetPlatform.wordsize == 32:
            flags += ['-march=i686', '-mtune=generic']
            # We stick with the default (-march=x86-64) for 64-bit
        return flags
