
languages = ['en']

englishDBFiles = ['data/strings/english.db']

charTablesSource = 'data/i18ndata/tables/tables-all.txt'

def makeLangFlags():
    return ['-c', '9', '-C', '-p', ','.join((config.targetPlatform.lngSystem, config.operaVersion, str(config.operaBuild)))]
