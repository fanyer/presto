
import os
import glob
import errno

@flow(1, binary='opera')
def scanSources(self):
    yield hardcoreSetupStep(step='sources')
    path = "{src}/modules/hardcore/setup/{type}/sources".format(src=config.buildrootSources, type='jumbo' if config.jumbo else 'plain')
    sources = {}
    byModule = {}
    # Each source not listed in sources.component_* appears in exactly
    # one of these four:
    for pch in ('nopch', 'pch', 'pch_jumbo', 'pch_system_includes'):
        with open("{p}/sources.{suffix}".format(p=path, suffix=pch)) as f:
            for line in f:
                filename = line.strip()
                if filename and '/'.join(filename.split('/')[:2]) in otree.modules:
                    sources[filename] = pch
    for componentFile in glob.glob("{p}/sources.component_*".format(p=path)):
        if os.path.basename(componentFile) != 'sources.component_framework':
            with open(componentFile) as f:
                for line in f:
                    filename = line.strip()
                    if filename in sources:
                        del sources[filename]
    for source in sources:
        module = '/'.join(source.split('/')[:2])
        if module not in byModule:
            byModule[module] = {}
        byModule[module][source] = sources[source]
    self['sources'] = sources
    self['byModule'] = byModule

@flow(1, binary=util.hasPrefix('operapluginwrapper-'))
def scanSources(self):
    yield hardcoreSetupStep(step='sources')
    path = "{src}/modules/hardcore/setup/{type}/sources".format(src=config.buildrootSources, type='jumbo' if config.jumbo else 'plain')
    sources = {}
    for component in ('framework', 'plugin'):
        with open("{p}/sources.component_{c}".format(p=path, c=component)) as f:
            for line in f:
                filename = line.strip()
                if filename and '/'.join(filename.split('/')[:2]) in otree.modules:
                    sources[filename] = 'pch'
    self['sources'] = sources

@flow
def scanSources(self, binary):
    yield hardcoreSetupStep(step='sources')
    path = "{src}/modules/hardcore/setup/{type}/sources".format(src=config.buildrootSources, type='jumbo' if config.jumbo else 'plain')
    sources = {}
    with open("{p}/sources.component_{b}".format(p=path, b=binary)) as f:
        for line in f:
            filename = line.strip()
            if filename and '/'.join(filename.split('/')[:2]) in otree.modules:
                sources[filename] = 'pch'
    self['sources'] = sources

@flow
def precompiledHeader(self, stem, binary, lang):
    self['source'] = "core/{0}.h".format(stem)
    self['sourcePath'] = "{src}/{name}".format(src=config.buildrootSources, name=self['source'])
    compiler = config.targetPlatform.compiler
    self['target'] = compiler.precompiledHeaderTarget
    yield sourceSetup()
    if self < (util.readDepFile(compiler.precompiledHeaderDepFile) or [self['sourcePath']]):
        util.makeDirs(self)
        yield command(compiler.precompileCommand, "Precompiling {source} for {binary}")

@flow(1, source=util.hasSuffix('.cpp'))
def compileSource(self, source, binary, pgo, pch='nopch'):
    self['stem'] = util.removeSuffix(source, '.cpp')
    self['lang'] = 'c++'
    yield self

@flow(1, source=util.hasSuffix('.c'), pch='nopch')
def compileSource(self, source, binary, pgo):
    self['stem'] = util.removeSuffix(source, '.c')
    self['lang'] = 'c'
    yield self

@flow(1, source=util.hasSuffix('.m'))
def compileSource(self, source, binary, pgo, pch='nopch'):
    self['stem'] = util.removeSuffix(source, '.m')
    self['lang'] = 'objective-c'
    yield self

@flow(1, source=util.hasSuffix('.mm'))
def compileSource(self, source, binary, pgo, pch='nopch'):
    self['stem'] = util.removeSuffix(source, '.mm')
    self['lang'] = 'objective-c++'
    yield self

@flow(1, source=util.hasSuffix('.S'), pch='nopch')
def compileSource(self, source, binary, pgo):
    self['stem'] = util.removeSuffix(source, '.S')
    self['lang'] = 'assembler-with-cpp'
    yield self

@goal('compile', 'Compile a single source file.',
      {'arg': 'source', 'help': 'The source file to compile.'},
      {'arg': 'binary', 'help': 'The binary to compile it for.', 'default': 'opera'},
      {'arg': 'pgo', 'value': False},
      {'arg': 'pch', 'value': 'nopch'})
@flow
def compileSource(self, source, binary, pgo, lang, stem, pch):
    compiler = config.targetPlatform.compiler
    self['target'] = compiler.target
    deps = []
    if pch != 'nopch' and compiler.supportsPrecompiledHeaders:
        deps.append(precompiledHeader(stem=pch, binary=binary, lang=lang))
    if pgo == 'use':
        deps.append(PGODataFile(source=source, binary=binary))
    yield [sourceSetup()] + deps
    generatedSource = "{src}/{name}".format(src=config.buildrootSources, name=source)
    self['sourcePath'] = generatedSource if os.path.exists(generatedSource) else source
    deps += util.readDepFile(compiler.depFile) or [self['sourcePath']]
    if self < deps:
        util.makeDirs(self)
        yield command(compiler.compileCommand,
                      "Compiling instrumented {source}" if pgo == 'generate' else "Compiling {source}")
        self['result'] = "Produced object file {target}".format(target=self['target'])

@goal('link', 'Link a single binary.',
      {'arg': 'binary', 'help': 'Which binary to build.', 'default': 'opera'},
      {'arg': 'pgo', 'value': None})
@flow
def linkBinary(self, binary, pgo, module=None):
    self['target'] = config.binaryTarget
    scan = scanSources(binary=binary)
    yield scan
    if module:
        sources = [compileSource(source=source, binary=binary, pgo=pgo, pch=pch) for source, pch in scan['byModule'][module].iteritems()]
    elif binary == 'opera' and config.libPerModule:
        self['mainBinary'] = True
        sources = [linkBinary(binary=binary, pgo=pgo, module=m) for m in scan['byModule']]
    else:
        sources = [compileSource(source=source, binary=binary, pgo=pgo, pch=pch) for source, pch in scan['sources'].iteritems()]
    if not module and  binary == 'opera':
        sources.append(libvpx())
    yield sources
    if self < sources:
        util.makeDirs(self)
        self['sourceList'] = "{obj}/{stem}.list".format(obj=config.buildrootObjects, stem=module or binary)
        with open(self['sourceList'], 'w') as output:
            for s in sources:
                output.write(s['target'] + '\n')
        yield command(config.targetPlatform.linker.linkCommand,
                      "Linking instrumented {target}" if pgo == 'generate' else "Linking {target}")
        self['result'] = "Produced binary {target}".format(target=self['target'])

@goal('show-macro', 'Show what a preprocessor macro expands to using the precompiled header.',
      {'arg': 'macro', 'help': 'The macro to expand.'},
      {'arg': 'binary', 'help': 'The target binary (for context).', 'default': 'opera'})
@flow
def expandMacro(self, macro, binary):
    pch = precompiledHeader(stem='pch', binary=binary, lang='c++')
    yield pch
    test = "#ifdef {m}\n{m}\n#else\n#error\n#endif""".format(m=macro)
    try:
        expansion = config.targetPlatform.preprocessor.expandMacro(pch['sourcePath'], test)
        if expansion:
            self['result'] = "{m} is defined as {e}".format(m=macro, e=expansion)
        else:
            self['result'] = "{m} is defined".format(m=macro)
    except errors.CommandFailed:
        self['result'] = "{m} is undefined".format(m=macro)

@flow
def PGODataFile(self, source, binary):
    self['target'] = config(pgo='use').targetPlatform.compiler.PGODataFile
    self['source'] = config(pgo='generate').targetPlatform.compiler.PGODataFile
    yield PGOData()
    if os.path.exists(self['source']):
        try:
            util.makeDirs(self)
            os.symlink(os.path.relpath(self['source'], os.path.dirname(self['target'])), self['target'])
        except EnvironmentError, e:
            if e.errno != errno.EEXIST:
                raise
    else:
        self['changed'] = False
        try:
            os.unlink(self['target'])
        except EnvironmentError, e:
            if e.errno != errno.ENOENT:
                raise
