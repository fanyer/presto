
import sys
import os

@flow
def hardcoreSetup(self):
    yield [hardcoreSetupStep(step=s) for s in
           ['dom_atoms',
            'dom_prototypes',
            'keys',
            'prefs',
            'probes',
            'stashed_defines',
            'strings',
            'viewers']]

@goal('setup-hardcore', 'Run an individual hardcore setup step.',
      {'arg': 'step', 'help': 'Which step to run (see operasetup.py --help).'})
@flow
def hardcoreSetupStep(self, step):
    self['target'] = config.hardcoreTimestamp
    yield opensslLink()
    if self < util.readDepFile(self['target'] + '.d'):
        yield hardcoreSelftest()
        util.makeDirs(self)
        yield command([sys.executable, 'modules/hardcore/scripts/operasetup.py'] + config.hardcoreFlags + ['--only', '--generate_' + step])

@flow(1, step='selftests')
def hardcoreSetupStep(self):
    self['target'] = config.hardcoreTimestamp
    scan = scanSelftestSources()
    yield scan
    nodes = [generateSelftest(source=s) for s in scan['sources']]
    yield nodes
    if self < nodes:
        yield command([config.pike, 'modules/selftest/parser/parse_tests.pike', '-F', '-o', config.buildrootSources, '-q'])
        util.touch(self)

@flow(1, step='sources')
def hardcoreSetupStep(self):
    deps = [hardcoreSetupStep(step='protobuf')]
    if config.selftests:
        deps += [hardcoreSetupStep(step='selftests')]
    yield deps
    yield self

@flow(1, step='strings')
def hardcoreSetupStep(self):
    yield [hardcoreSetupStep(step=s) for s in
           ['actions',
            'components',
            'css_values',
            'features',
            'markup_names',
            'messages',
            'opera_h',
            'pch',
            'properties',
            'protobuf',
            'system',
            'tweaks']]
    yield self

@flow
def hardcoreSelftest(self):
    yield command([sys.executable, 'modules/hardcore/scripts/operasetup.py', '--only', '--selftest'])

@flow
def opensslLink(self):
    self['target'] = config.buildrootSources + '/openssl'
    if not os.path.exists(self['target']):
        util.makeDirs(self)
        os.symlink(os.path.relpath('modules/libopeay/openssl', os.path.dirname(self['target'])), self['target'])
