/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/util/LanguageCodes.h"

const uni_char* gISO639LanguageCodes[] =
{
	/* http://www.egt.ie/standards/iso639/iso639-en.html */
	/* http://lcweb.loc.gov/standards/iso639-2/langhome.html */
	NULL, 0,
//	UNI_L("(Afan) Oromo"),		UNI_L("om"),
//	UNI_L("(Hebrew)"),			UNI_L("iw"), /* Withdrawn */
//	UNI_L("(Indonesian)"),		UNI_L("in"), /* Withdrawn*/
//	UNI_L("(Serbo-Croatian)"),	UNI_L("sh"), /* Withdrawn */
//	UNI_L("(Yiddish)"),			UNI_L("ji"),
	UNI_L("Abkhazian"),			UNI_L("ab"),
	UNI_L("Afar"),				UNI_L("aa"),
	UNI_L("Afrikaans"),			UNI_L("af"),
	UNI_L("Albanian"),			UNI_L("sq"),
	UNI_L("Amharic"),			UNI_L("am"),
	UNI_L("Arabic"),			UNI_L("ar"),
	UNI_L("Armenian"),			UNI_L("hy"),
	UNI_L("Assamese"),			UNI_L("as"),
	UNI_L("Avestan"),			UNI_L("ae"),
	UNI_L("Aymara"),			UNI_L("ay"),
	UNI_L("Azerbaijani"),		UNI_L("az"),
	UNI_L("Bashkir"),			UNI_L("ba"),
	UNI_L("Basque"),			UNI_L("eu"),
	UNI_L("Bengali; Bangla"),	UNI_L("bn"),
	UNI_L("Bhutani"),			UNI_L("dz"),
	UNI_L("Bihari"),			UNI_L("bh"),
	UNI_L("Bislama"),			UNI_L("bi"),
	UNI_L("Bosnian"),			UNI_L("bs"),
	UNI_L("Breton"),			UNI_L("br"),
	UNI_L("Bulgarian"),			UNI_L("bg"),
	UNI_L("Burmese"),			UNI_L("my"),
	UNI_L("Byelorussian"),		UNI_L("be"),
	UNI_L("Cambodian"),			UNI_L("km"),
	UNI_L("Catalan"),			UNI_L("ca"),
	UNI_L("Chamorro"),			UNI_L("ch"),
	UNI_L("Chechen"),			UNI_L("ce"),
	UNI_L("Chichewa; Nyanja"),	UNI_L("ny"),
	UNI_L("Chinese"),			UNI_L("zh"),
	UNI_L("Chinese CN"),		UNI_L("zh_CN"),
	UNI_L("Chinese HK"),		UNI_L("zh_HK"),
	UNI_L("Chinese TW"),		UNI_L("zh_TW"),
	UNI_L("Church Slavic"),		UNI_L("cu"),
	UNI_L("Chuvash"),			UNI_L("cv"),
	UNI_L("Cornish"),			UNI_L("kw"),
	UNI_L("Corsican"),			UNI_L("co"),
	UNI_L("Croatian"),			UNI_L("hr"),
	UNI_L("Czech"),				UNI_L("cs"),
	UNI_L("Danish"),			UNI_L("da"),
	UNI_L("Dutch"),				UNI_L("nl"),
	UNI_L("English"),			UNI_L("en"),
	UNI_L("Esperanto"),			UNI_L("eo"),
	UNI_L("Estonian"),			UNI_L("et"),
	UNI_L("Faeroese"),			UNI_L("fo"),
	UNI_L("Fiji"),				UNI_L("fj"),
	UNI_L("Finnish"),			UNI_L("fi"),
	UNI_L("French"),			UNI_L("fr"),
	UNI_L("Frisian"),			UNI_L("fy"),
	UNI_L("Galician"),			UNI_L("gl"),
	UNI_L("Georgian"),			UNI_L("ka"),
	UNI_L("German"),			UNI_L("de"),
	UNI_L("Greek"),				UNI_L("el"),
	UNI_L("Greenlandic"),		UNI_L("kl"),
	UNI_L("Guarani"),			UNI_L("gn"),
	UNI_L("Gujarati"),			UNI_L("gu"),
	UNI_L("Hausa"),				UNI_L("ha"),
	UNI_L("Hebrew"),			UNI_L("he"),
	UNI_L("Herero"),			UNI_L("hz"),
	UNI_L("Hindi"),				UNI_L("hi"),
	UNI_L("Hiri Motu"),			UNI_L("ho"),
	UNI_L("Hungarian"),			UNI_L("hu"),
	UNI_L("Icelandic"),			UNI_L("is"),
	UNI_L("Indonesian"),		UNI_L("id"),
	UNI_L("Interlingua"),		UNI_L("ia"),
	UNI_L("Interlingue"),		UNI_L("ie"),
	UNI_L("Inuktitut"),			UNI_L("iu"),
	UNI_L("Inupiak"),			UNI_L("ik"),
	UNI_L("Irish"),				UNI_L("ga"),
	UNI_L("Italian"),			UNI_L("it"),
	UNI_L("Japanese"),			UNI_L("ja"),
	UNI_L("Javanese"),			UNI_L("jw"),
	UNI_L("Kannada"),			UNI_L("kn"),
	UNI_L("Kashmiri"),			UNI_L("ks"),
	UNI_L("Kazakh"),			UNI_L("kk"),
	UNI_L("Kinyarwanda"),		UNI_L("rw"),
	UNI_L("Kirghiz"),			UNI_L("ky"),
	UNI_L("Kirundi"),			UNI_L("rn"),
	UNI_L("Komi"),				UNI_L("kv"),
	UNI_L("Korean"),			UNI_L("ko"),
	UNI_L("Kurdish"),			UNI_L("ku"),
	UNI_L("Laothian"),			UNI_L("lo"),
	UNI_L("Latin"),				UNI_L("la"),
	UNI_L("Latvian, Lettish"),	UNI_L("lv"),
	UNI_L("Lingala"),			UNI_L("ln"),
	UNI_L("Lithuanian"),		UNI_L("lt"),
	UNI_L("Luxembourgish"),		UNI_L("lb"),
	UNI_L("Macedonian"),		UNI_L("mk"),
	UNI_L("Malagasy"),			UNI_L("mg"),
	UNI_L("Malay"),				UNI_L("ms"),
	UNI_L("Malayalam"),			UNI_L("ml"),
	UNI_L("Maltese"),			UNI_L("mt"),
	UNI_L("Manx Gaelic"),		UNI_L("gv"),
	UNI_L("Maori"),				UNI_L("mi"),
	UNI_L("Marathi"),			UNI_L("mr"),
	UNI_L("Marshall"),			UNI_L("mh"),
	UNI_L("Moldavian"),			UNI_L("mo"),
	UNI_L("Mongolian"),			UNI_L("mn"),
	UNI_L("Nauru"),				UNI_L("na"),
	UNI_L("Navajo"),			UNI_L("nv"),
	UNI_L("Ndebele, North"),	UNI_L("nd"),
	UNI_L("Ndebele, South"),	UNI_L("nr"),
	UNI_L("Ndonga"),			UNI_L("ng"),
	UNI_L("Nepali"),			UNI_L("ne"),
	UNI_L("Northern Sami"),		UNI_L("se"),
	UNI_L("Norwegian"),			UNI_L("no"),
	UNI_L("Occitan"),			UNI_L("oc"),
	UNI_L("Oriya"),				UNI_L("or"),
	UNI_L("Ossetian; Ossetic"),	UNI_L("os"),
	UNI_L("Pali"),				UNI_L("pi"),
	UNI_L("Pashto, Pushto"),	UNI_L("ps"),
	UNI_L("Persian"),			UNI_L("fa"),
	UNI_L("Polish"),			UNI_L("pl"),
	UNI_L("Portuguese"),		UNI_L("pt"),
	UNI_L("Punjabi"),			UNI_L("pa"),
	UNI_L("Quechua"), 			UNI_L("qu"),
	UNI_L("Rhaeto-Romance"),	UNI_L("rm"),
	UNI_L("Romanian"),			UNI_L("ro"),
	UNI_L("Russian"),			UNI_L("ru"),
	UNI_L("Samoan"),			UNI_L("sm"),
	UNI_L("Sangro"),			UNI_L("sg"),
	UNI_L("Sanskrit"),			UNI_L("sa"),
	UNI_L("Sardinian"),			UNI_L("sc"),
	UNI_L("Scots Gaelic"),		UNI_L("gd"),
	UNI_L("Serbian"),			UNI_L("sr"),
	UNI_L("Sesotho"),			UNI_L("st"),
	UNI_L("Setswana"),			UNI_L("tn"),
	UNI_L("Shona"),				UNI_L("sn"),
	UNI_L("Sindhi"),			UNI_L("sd"),
	UNI_L("Singhalese"),		UNI_L("si"),
	UNI_L("Siswati"),			UNI_L("ss"),
	UNI_L("Slovak"),			UNI_L("sk"),
	UNI_L("Slovenian"),			UNI_L("sl"),
	UNI_L("Somali"),			UNI_L("so"),
	UNI_L("Spanish"),			UNI_L("es"),
	UNI_L("Sundanese"),			UNI_L("su"),
	UNI_L("Swahili"),			UNI_L("sw"),
	UNI_L("Swedish"),			UNI_L("sv"),
	UNI_L("Tagalog"),			UNI_L("tl"),
	UNI_L("Tajik"),				UNI_L("tg"),
	UNI_L("Tamil"),				UNI_L("ta"),
	UNI_L("Tatar"),				UNI_L("tt"),
	UNI_L("Telugu"),			UNI_L("te"),
	UNI_L("Thai"),				UNI_L("th"),
	UNI_L("Tibetan"),			UNI_L("bo"),
	UNI_L("Tigrinya"),			UNI_L("ti"),
	UNI_L("Tonga"),				UNI_L("to"),
	UNI_L("Tsonga"),			UNI_L("ts"),
	UNI_L("Turkish"),			UNI_L("tr"),
	UNI_L("Turkmen"),			UNI_L("tk"),
	UNI_L("Twi"),				UNI_L("tw"),
	UNI_L("Ukrainian"),			UNI_L("uk"),
	UNI_L("Urdu"),				UNI_L("ur"),
	UNI_L("Uzbek"),				UNI_L("uz"),
	UNI_L("Vietnamese"), 		UNI_L("vi"),
	UNI_L("Volapuk"), 			UNI_L("vo"),
	UNI_L("Welsh"),				UNI_L("cy"),
	UNI_L("Wolof"),				UNI_L("wo"),
	UNI_L("Xhosa"),				UNI_L("xh"),
	UNI_L("Yiddish"),			UNI_L("yi"),
	UNI_L("Zhuang"),			UNI_L("za")
};

const uni_char* gISO639LanguageNames[] = {
	/* http://www.egt.ie/standards/iso639/iso639-en.html */
	/* http://lcweb.loc.gov/standards/iso639-2/langhome.html */
	NULL, 0,
	UNI_L("aa"), UNI_L("Afar"),
	UNI_L("ab"), UNI_L("Abkhazian"),
	UNI_L("ae"), UNI_L("Avestan"),
	UNI_L("af"), UNI_L("Afrikaans"),
	UNI_L("am"), UNI_L("Amharic"),
	UNI_L("ar"), UNI_L("Arabic"),
	UNI_L("as"), UNI_L("Assamese"),
	UNI_L("ay"), UNI_L("Aymara"),
	UNI_L("az"), UNI_L("Azerbaijani"),
	UNI_L("ba"), UNI_L("Bashkir"),
	UNI_L("be"), UNI_L("Byelorussian"),
	UNI_L("bg"), UNI_L("Bulgarian"),
	UNI_L("bh"), UNI_L("Bihari"),
	UNI_L("bi"), UNI_L("Bislama"),
	UNI_L("bn"), UNI_L("Bengali; Bangla"),
	UNI_L("bo"), UNI_L("Tibetan"),
	UNI_L("br"), UNI_L("Breton"),
	UNI_L("bs"), UNI_L("Bosnian"),
	UNI_L("ca"), UNI_L("Catalan"),
	UNI_L("ce"), UNI_L("Chechen"),
	UNI_L("ch"), UNI_L("Chamorro"),
	UNI_L("co"), UNI_L("Corsican"),
	UNI_L("cs"), UNI_L("Czech"),
	UNI_L("cu"), UNI_L("Church Slavic"),
	UNI_L("cv"), UNI_L("Chuvash"),
	UNI_L("cy"), UNI_L("Welsh"),
	UNI_L("da"), UNI_L("Danish"),
	UNI_L("de"), UNI_L("German"),
	UNI_L("dz"), UNI_L("Bhutani"),
	UNI_L("el"), UNI_L("Greek"),
	UNI_L("en"), UNI_L("English"),
	UNI_L("eo"), UNI_L("Esperanto"),
	UNI_L("es"), UNI_L("Spanish"),
	UNI_L("et"), UNI_L("Estonian"),
	UNI_L("eu"), UNI_L("Basque"),
	UNI_L("fa"), UNI_L("Persian"),
	UNI_L("fi"), UNI_L("Finnish"),
	UNI_L("fj"), UNI_L("Fiji"),
	UNI_L("fo"), UNI_L("Faeroese"),
	UNI_L("fr"), UNI_L("French"),
	UNI_L("fy"), UNI_L("Frisian"),
	UNI_L("ga"), UNI_L("Irish"),
	UNI_L("gd"), UNI_L("Scots Gaelic"),
	UNI_L("gl"), UNI_L("Galician"),
	UNI_L("gn"), UNI_L("Guarani"),
	UNI_L("gu"), UNI_L("Gujarati"),
	UNI_L("gv"), UNI_L("Manx Gaelic"),
	UNI_L("ha"), UNI_L("Hausa"),
	UNI_L("he"), UNI_L("Hebrew"),
	UNI_L("hi"), UNI_L("Hindi"),
	UNI_L("ho"), UNI_L("Hiri Motu"),
	UNI_L("hr"), UNI_L("Croatian"),
	UNI_L("hu"), UNI_L("Hungarian"),
	UNI_L("hy"), UNI_L("Armenian"),
	UNI_L("hz"), UNI_L("Herero"),
	UNI_L("ia"), UNI_L("Interlingua"),
	UNI_L("id"), UNI_L("Indonesian"),
	UNI_L("ie"), UNI_L("Interlingue"),
	UNI_L("ik"), UNI_L("Inupiak"),
//	UNI_L("in"), UNI_L("(Indonesian)"), /* Withdrawn*/
	UNI_L("is"), UNI_L("Icelandic"),
	UNI_L("it"), UNI_L("Italian"),
	UNI_L("iu"), UNI_L("Inuktitut"),
//	UNI_L("iw"), UNI_L("(Hebrew)"), /* Withdrawn */
	UNI_L("ja"), UNI_L("Japanese"),
//	UNI_L("ji"), UNI_L("(Yiddish)"),
	UNI_L("jw"), UNI_L("Javanese"),
	UNI_L("ka"), UNI_L("Georgian"),
	UNI_L("kk"), UNI_L("Kazakh"),
	UNI_L("kl"), UNI_L("Greenlandic"),
	UNI_L("km"), UNI_L("Cambodian"),
	UNI_L("kn"), UNI_L("Kannada"),
	UNI_L("ko"), UNI_L("Korean"),
	UNI_L("ks"), UNI_L("Kashmiri"),
	UNI_L("ku"), UNI_L("Kurdish"),
	UNI_L("kv"), UNI_L("Komi"),
	UNI_L("kw"), UNI_L("Cornish"),
	UNI_L("ky"), UNI_L("Kirghiz"),
	UNI_L("la"), UNI_L("Latin"),
	UNI_L("lb"), UNI_L("Luxembourgish"),
	UNI_L("ln"), UNI_L("Lingala"),
	UNI_L("lo"), UNI_L("Laothian"),
	UNI_L("lt"), UNI_L("Lithuanian"),
	UNI_L("lv"), UNI_L("Latvian, Lettish"),
	UNI_L("mg"), UNI_L("Malagasy"),
	UNI_L("mh"), UNI_L("Marshall"),
	UNI_L("mi"), UNI_L("Maori"),
	UNI_L("mk"), UNI_L("Macedonian"),
	UNI_L("ml"), UNI_L("Malayalam"),
	UNI_L("mn"), UNI_L("Mongolian"),
	UNI_L("mo"), UNI_L("Moldavian"),
	UNI_L("mr"), UNI_L("Marathi"),
	UNI_L("ms"), UNI_L("Malay"),
	UNI_L("mt"), UNI_L("Maltese"),
	UNI_L("my"), UNI_L("Burmese"),
	UNI_L("na"), UNI_L("Nauru"),
	UNI_L("nd"), UNI_L("Ndebele, North"),
	UNI_L("ne"), UNI_L("Nepali"),
	UNI_L("ng"), UNI_L("Ndonga"),
	UNI_L("nl"), UNI_L("Dutch"),
	UNI_L("no"), UNI_L("Norwegian"),
	UNI_L("nr"), UNI_L("Ndebele, South"),
	UNI_L("nv"), UNI_L("Navajo"),
	UNI_L("ny"), UNI_L("Chichewa; Nyanja"),
	UNI_L("oc"), UNI_L("Occitan"),
//	UNI_L("om"), UNI_L("(Afan) Oromo"),
	UNI_L("or"), UNI_L("Oriya"),
	UNI_L("os"), UNI_L("Ossetian; Ossetic"),
	UNI_L("pa"), UNI_L("Punjabi"),
	UNI_L("pi"), UNI_L("Pali"),
	UNI_L("pl"), UNI_L("Polish"),
	UNI_L("ps"), UNI_L("Pashto, Pushto"),
	UNI_L("pt"), UNI_L("Portuguese"),
	UNI_L("qu"), UNI_L("Quechua"),
	UNI_L("rm"), UNI_L("Rhaeto-Romance"),
	UNI_L("rn"), UNI_L("Kirundi"),
	UNI_L("ro"), UNI_L("Romanian"),
	UNI_L("ru"), UNI_L("Russian"),
	UNI_L("rw"), UNI_L("Kinyarwanda"),
	UNI_L("sa"), UNI_L("Sanskrit"),
	UNI_L("sc"), UNI_L("Sardinian"),
	UNI_L("sd"), UNI_L("Sindhi"),
	UNI_L("se"), UNI_L("Northern Sami"),
	UNI_L("sg"), UNI_L("Sangro"),
//	UNI_L("sh"), UNI_L("(Serbo-Croatian)"), /* Withdrawn */
	UNI_L("si"), UNI_L("Singhalese"),
	UNI_L("sk"), UNI_L("Slovak"),
	UNI_L("sl"), UNI_L("Slovenian"),
	UNI_L("sm"), UNI_L("Samoan"),
	UNI_L("sn"), UNI_L("Shona"),
	UNI_L("so"), UNI_L("Somali"),
	UNI_L("sq"), UNI_L("Albanian"),
	UNI_L("sr"), UNI_L("Serbian"),
	UNI_L("ss"), UNI_L("Siswati"),
	UNI_L("st"), UNI_L("Sesotho"),
	UNI_L("su"), UNI_L("Sundanese"),
	UNI_L("sv"), UNI_L("Swedish"),
	UNI_L("sw"), UNI_L("Swahili"),
	UNI_L("ta"), UNI_L("Tamil"),
	UNI_L("te"), UNI_L("Telugu"),
	UNI_L("tg"), UNI_L("Tajik"),
	UNI_L("th"), UNI_L("Thai"),
	UNI_L("ti"), UNI_L("Tigrinya"),
	UNI_L("tk"), UNI_L("Turkmen"),
	UNI_L("tl"), UNI_L("Tagalog"),
	UNI_L("tn"), UNI_L("Setswana"),
	UNI_L("to"), UNI_L("Tonga"),
	UNI_L("tr"), UNI_L("Turkish"),
	UNI_L("ts"), UNI_L("Tsonga"),
	UNI_L("tt"), UNI_L("Tatar"),
	UNI_L("tw"), UNI_L("Twi"),
	UNI_L("uk"), UNI_L("Ukrainian"),
	UNI_L("ur"), UNI_L("Urdu"),
	UNI_L("uz"), UNI_L("Uzbek"),
	UNI_L("vi"), UNI_L("Vietnamese"),
	UNI_L("vo"), UNI_L("Volapuk"),
	UNI_L("wo"), UNI_L("Wolof"),
	UNI_L("xh"), UNI_L("Xhosa"),
	UNI_L("yi"), UNI_L("Yiddish"),
	UNI_L("za"), UNI_L("Zhuang"),
	UNI_L("zh"), UNI_L("Chinese"),
	UNI_L("zh_CN"), UNI_L("Chinese CN"),
	UNI_L("zh_HK"), UNI_L("Chinese HK"),
	UNI_L("zh_TW"), UNI_L("Chinese TW")
};

const uni_char *MacLanguages[] =
{
	UNI_L("Norwegian"),		UNI_L("no"),
	UNI_L("English"), 		UNI_L("en"),
	UNI_L("Japanese"), 		UNI_L("ja"),
	UNI_L("German"), 		UNI_L("de"),
	UNI_L("French"), 		UNI_L("fr"),
	UNI_L("Dutch"),			UNI_L("nl"),
	UNI_L("Italian"),		UNI_L("it"),
	UNI_L("Spanish"),		UNI_L("es"),
	UNI_L("Danish"),		UNI_L("da"),
	UNI_L("Finnish"),		UNI_L("fi"),
	UNI_L("Korean"),		UNI_L("ko"),
	UNI_L("Portuguese"),	UNI_L("pt"),
	UNI_L("Swedish"),		UNI_L("sv"),
	UNI_L("Chinese"),		UNI_L("zh"),
	UNI_L("zh-Hans"),		UNI_L("zh_CN"),
	UNI_L("zh-Hant"),		UNI_L("zh_TW")
};

const uni_char *ConvertAppleLanguageNameToCode(const uni_char *languageName)
{
	if(!languageName)
		return 0;

	for(unsigned int i = 0; i < sizeof(MacLanguages) / sizeof(MacLanguages[0]); i+=2)
	{
		if(!uni_stricmp(MacLanguages[i], languageName))
			return MacLanguages[i+1];
	}
	if (uni_strlen(languageName) == 2 && languageName[0] >= 'a' && languageName[0] <= 'z' &&
		languageName[1] >= 'a' && languageName[1] <= 'z')
	{
		// language not in our list, but it looks like an ISO language code...
		return languageName;
	}
	else if (uni_strlen(languageName) == 5 && languageName[0] >= 'a' && languageName[0] <= 'z' &&
		languageName[1] >= 'a' && languageName[1] <= 'z' && languageName[2] == '_' &&
		((languageName[3] >= 'a'&& languageName[3] <= 'z') || (languageName[3] >= 'A'&& languageName[3] <= 'Z')) &&
		((languageName[4] >= 'a'&& languageName[4] <= 'z') || (languageName[4] >= 'A'&& languageName[4] <= 'Z')) )
	{
		// language not in our list, but it looks like an ISO language code...
		return languageName;
	}
	else if (uni_strlen(languageName) == 5 && languageName[0] >= 'a' && languageName[0] <= 'z' &&
		languageName[1] >= 'a' && languageName[1] <= 'z' && languageName[2] == '-' &&
		((languageName[3] >= 'a'&& languageName[3] <= 'z') || (languageName[3] >= 'A'&& languageName[3] <= 'Z')) &&
		((languageName[4] >= 'a'&& languageName[4] <= 'z') || (languageName[4] >= 'A'&& languageName[4] <= 'Z')) )
	{
		// language not in our list, but it looks like an ISO language code...
		return languageName;
	}
	return 0;
}

const uni_char *ConvertLanguageNameToCode(const uni_char *inLangName, int inLength)
{
	if(!inLangName || inLength <= 0)
		return gISO639LanguageCodes[1];

	int count = sizeof(gISO639LanguageCodes) / (2 * sizeof(char*));
	int left = 1;
	int right = count -1;

	do{
		if(left == right)
		{
			if(uni_strnicmp(inLangName, gISO639LanguageCodes[left * 2], inLength) == 0)
				return gISO639LanguageCodes[left * 2 + 1];
			break;
		}
		else
		{
			int i = (left + right)>>1; // divide by 2

			int res = uni_strnicmp(inLangName, gISO639LanguageCodes[i * 2], inLength);

			if(res == 0)
				return gISO639LanguageCodes[i * 2 + 1];
			else if (res < 0)
				right = i-1;
			else
				left = i+1;
		}
	}while(left <= right);

	return gISO639LanguageCodes[1];
}

const uni_char *ConvertLanguageCodeToName(const uni_char *inLangCode, int inLength)
{
	if(!inLangCode || inLength <= 0)
		return gISO639LanguageNames[1];

	int count = sizeof(gISO639LanguageNames) / (2 * sizeof(char*));
	int left = 1;
	int right = count -1;

	do{
		if(left == right)
		{
			if(uni_strnicmp(inLangCode, gISO639LanguageNames[left * 2], inLength) == 0)
				return gISO639LanguageNames[left * 2 + 1];
			break;
		}
		else
		{
			int i = (left + right)>>1; // divide by 2

			int res = uni_strnicmp(inLangCode, gISO639LanguageNames[i * 2], inLength);

			if(res == 0)
				return gISO639LanguageNames[i * 2 + 1];
			else if (res < 0)
				right = i-1;
			else
				left = i+1;
		}
	}while(left <= right);

	return gISO639LanguageNames[1];
}

const uni_char* GetLanguageName(int inIndex)
{
	int count = sizeof(gISO639LanguageCodes) / (2 * sizeof(char*));
	if (inIndex <= 0 || inIndex >= count) {
		return NULL;
	}
	return (gISO639LanguageCodes[inIndex*2]);
}
