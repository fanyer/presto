
import glob

@flow
def hardcodedSearches(self):
    self['target'] = "{src}/desktop_util.hardcoded_searches".format(src=config.buildrootSources)
    script = 'adjunct/desktop_util/search/generate_hardcoded_searches.py'
    deps = [script, 'adjunct/resources/region', 'adjunct/resources/locale', 'adjunct/resources/defaults/search.ini']
    deps += glob.glob('adjunct/resources/region/*')
    deps += glob.glob('adjunct/resources/locale/*')
    deps += glob.glob('adjunct/resources/region/*/search.ini')
    deps += glob.glob('adjunct/resources/locale/*/search.ini')
    if self < deps:
        yield command([sys.executable, script, '-q', "{src}/adjunct/desktop_util/search/hardcoded_searches.inl".format(src=config.buildrootSources)],
                      "Generating hardcoded search engines")
        util.touch(self)
