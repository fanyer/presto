
import os.path

@flow(10, step='strings')
def hardcoreSetupStep(self):
    yield hardcoreSystemDefines()
    yield self

@flow
def hardcoreSystemDefines(self):
    sourceDir = config.buildrootSources
    self['target'] = sourceDir + '/platforms/unix/product/compilerdefines.h'
    self['source'] = sourceDir + '/platforms/unix/product/compilerdefines.c'
    self['lang'] = 'c'
    if not os.path.exists(self['target']):
        util.makeDirs(self)
        open(self['source'], 'w').close()
        yield command(config.targetPlatform.compiler.dumpSystemDefinesCommand)
