
import os.path
import sys

@flow
def charTables(self, bigendian):
    self['target'] = config.buildrootExtra + ('/chartables-be.bin' if bigendian else '/chartables.bin')
    source = config.charTablesSource
    script = 'data/i18ndata/tables/chrtblgen.py'
    if self < [source, script]:
        util.makeDirs(self)
        yield command([sys.executable, os.path.abspath(script),
                       '--big-endian' if bigendian else '--little-endian',
                       '--output', os.path.abspath(self['target']),
                       '--tempdir', os.path.abspath(config.buildrootExtra + ('/plain-be' if bigendian else '/plain-le')),
                       os.path.abspath(source)],
                      "Generating {target}",
                      cwd=os.path.abspath(os.path.dirname(script)))

@flow
def encodingBin(self):
    etc = config.buildrootExtra
    bigendian = config.targetPlatform.bigendian
    self['target'] = etc + '/encoding.bin'
    tables = charTables(bigendian=bigendian)
    yield tables
    script = 'data/i18ndata/tables/utilities/gettablelist.pl'
    if self < [script, tables]:
        yield command([script, '-b' if bigendian else '-l', '-e', etc, '-f', tables['target']],
                      "Generating {target}")

@goal('lng-file', 'Produce a language file.',
      {'arg': 'language', 'help': 'Language code.', 'default': 'en'})
@flow
def languageFile(self, language):
    etc = config.buildrootExtra
    self['target'] = "{etc}/{lang}.lng".format(etc=etc, lang=language)
    yield hardcoreSetupStep(step='strings')
    script = 'data/strings/scripts/makelang.pl'
    buildStrings = config.buildrootSources + '/data/strings/build.strings'
    poFile = "data/translations/{lang}/{lang}.po".format(lang=language)
    dbFiles = config.englishDBFiles
    deps = [script, buildStrings, poFile] + dbFiles
    if self < deps:
        util.makeDirs(self)
        yield command([script, '-d', etc, '-o', self['target'], '-L', '-t', poFile, '-b', buildStrings] + config.makeLangFlags + dbFiles,
                      "Processing translation for {language}")
        self['result'] = "Produced language file {target}".format(target=self['target'])
