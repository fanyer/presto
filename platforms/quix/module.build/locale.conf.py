
langListFile = 'adjunct/resources/lang_list.txt'

def languages():
    lang = []
    for line in util.readOnce(config.langListFile):
        item = line.split('#')[0].strip()
        if item:
            lang.append(item)
    return lang

def makeLangFlags():
    return default.makeLangFlags + ['-u']

def englishDBFiles():
    return default.englishDBFiles + ['adjunct/quick/english_local.db']

class AutoPlatform(default.AutoPlatform):

    def lngSystem(self):
        if self.isLinux:
            return 'Linux'
        if self.isFreeBSD:
            return 'FreeBSD'
        assert False, 'Unsupported OS'
