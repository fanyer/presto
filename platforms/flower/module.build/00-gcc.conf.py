
GCCSuffix = ''

class GCC(object):

    def __init__(self, role):
        assert role in ('preprocessor', 'compiler', 'linker')
        self.role = role

    def name(self, lang='c++'):
        if lang == 'c++':
            return 'g++' + config.GCCSuffix
        elif lang in ('c', 'assembler-with-cpp'):
            return 'gcc' + config.GCCSuffix
        else:
            assert False, 'Unsupported language'

    def compileCommand(self, target, sourcePath):
        return [self.name, '-c', '-o', target, sourcePath] + self.flags

    def precompileCommand(self, target, sourcePath):
        return [self.name, '-c', '-o', target, sourcePath] + self.flags

    def linkCommand(self, target, sourceList):
        return [self.name, '-o', target, '@' + sourceList] + self.flags

    def flags(self):
        flags = ['-pipe']
        if self.role == 'compiler':
            # We don't add -MMD for the preprocessor role because
            # that's only used in config.operaVersion where a .d file
            # is not needed.
            flags += ['-MMD']
        if self.role != 'preprocessor':
            flags += self.codeFlags
            flags += self.optimizeFlags
            flags += self.debugFlags
        if self.role == 'linker':
            flags += self.linkFlags
            flags += config.linkerFlags
        else:
            flags += self.warningFlags
            flags += config.preprocessorFlags
        return flags

    def codeFlags(self, lang=None, pgo=None):
        flags = ['-fshort-wchar', '-fsigned-char']
        if lang == 'c++':
            flags += ['-fno-threadsafe-statics', '-fno-exceptions', '-fno-rtti']
        if config.buildSharedLibrary:
            flags += ['-fpic']
        if pgo:
            assert pgo in ('generate', 'use')
            flags += ['-fprofile-' + pgo]
            if pgo == 'use':
                flags += ['-fprofile-correction']
        return flags

    def debugFlags(self):
        flags = []
        if config.framePointers:
            flags += ['-fno-omit-frame-pointer']
        else:
            flags += ['-fomit-frame-pointer']
        if config.debugSymbols:
            flags += ['-ggdb']
            if config.optimize and config.LTO:
                flags += ['-g1'] # prevents a gcc crash
        return flags

    def optimizeFlags(self):
        flags = []
        if config.optimize:
            flags += ['-ffunction-sections', '-fdata-sections']
            if self.role == 'linker':
                flags += ['-Wl,--gc-sections']
            if config.optimizeSize:
                flags += ['-Os', '-ffast-math', '-fno-unsafe-math-optimizations']
            else:
                flags += ['-O3', '-fno-tree-vectorize'] # tree vectorization causes a crash fillRect
            if config.LTO:
                flags += ['-flto=' + str(self.LTOJobs), '-fuse-linker-plugin', '-fno-fat-lto-objects']
        return flags

    def warningFlags(self, lang=None):
        flags = ['-Winvalid-pch', '-fmessage-length=0']
        if config.warnings:
            flags += ['-Wextra', '-Wall', '-Wvla']
        else:
            flags += ['-Wchar-subscripts', '-Wformat', '-Wformat-security', '-Wmultichar', '-Wpointer-arith', '-Wreturn-type', '-Wstrict-aliasing']
            if lang == 'c':
                flags += ['-Wimplicit', '-Wsequence-point']
        # Disable specific warnings we never want to see
        flags += ['-Wno-unknown-pragmas', '-Wno-format-y2k', '-Wno-switch', '-Wno-parentheses', '-Wno-unused-parameter']
        return flags

    def linkFlags(self):
        flags = []
        if config.buildSharedLibrary:
            flags += ['-shared']
        if config.buildMapfiles:
            flags += ['-Wl,-Map,' + config.binaryMapFile]
        return flags

    def target(self, source):
        suffix = source[source.rindex('.'):]
        assert suffix in ('.cpp', '.c', '.S')
        return "{obj}/{name}.o".format(obj=config.buildrootObjects, name=util.removeSuffix(source, suffix))

    def depFile(self):
        return self.target[:-2] + '.d'

    supportsPrecompiledHeaders = True

    def precompiledHeaderTarget(self, source):
        return "{obj}/{name}.gch".format(obj=config.buildrootObjects, name=source)

    def precompiledHeaderDepFile(self, source):
        return "{obj}/{name}.d".format(obj=config.buildrootObjects, name=source)

    def PGODataFile(self):
        return util.removeSuffix(self.target, '.o') + '.gcda'

    def expandMacro(self):
        def expandMacro(header, macro):
            return util.runOnce([self.name, '-E', '-P', '-imacros', header] + config.preprocessorFlags + ['-'], input=macro).strip()
        return expandMacro

    def LTOJobs(self):
        return config.processQuota
