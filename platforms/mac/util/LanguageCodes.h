const uni_char *ConvertAppleLanguageNameToCode(const uni_char *languageName);
const uni_char* ConvertLanguageNameToCode(const uni_char *inLangName, int inLength);
const uni_char* ConvertLanguageCodeToName(const uni_char *inLangCode, int inLength);
const uni_char* GetLanguageName(int inIndex);

extern const uni_char* gISO639LanguageCodes[];
extern const uni_char* gISO639LanguageNames[];
