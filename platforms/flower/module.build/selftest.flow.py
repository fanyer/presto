
import os

@flow
def generateSelftest(self, source):
    stem = source.split('/', 1)[1]
    self['target'] = "{src}/modules/selftest/generated/{dir}/ot_{name}.cpp.info".format(
        src=config.buildrootSources,
        dir=os.path.dirname(stem),
        name=util.removeSuffix(stem, '.ot').replace('/', '_').replace('.', '_'))
    if self < util.readDepFile(util.removeSuffix(self['target'], '.info') + '.d'):
        yield command([config.pike, 'modules/selftest/parser/parse_tests.pike', '-D', '-o', config.buildrootSources, '-q', source],
                      "Generating selftests from {source}")

@flow
def scanSelftestSources(self):
    yield hardcoreSetupStep(step='dom_atoms') # generates modules/dom/selftest/opatom.ot
    sources = []
    buildroot = config.buildrootSources + '/'
    try:
        with open('modules/selftest/selftest.modules') as f:
            moduleFilter = set([line.strip() for line in f])
    except EnvironmentError, e:
        if e.errno != errno.ENOENT:
            raise
        moduleFilter = None
    for m in otree.modules.itervalues():
        if moduleFilter is not None and m.name not in moduleFilter:
            continue
        sources += util.listDirRecursive(m.fullname, '*.ot', include_dirs=False)
        if os.path.exists(buildroot + m.fullname):
            sources += util.listDirRecursive(buildroot + m.fullname, '*.ot', include_dirs=False)
    self['sources'] = sorted(sources)
