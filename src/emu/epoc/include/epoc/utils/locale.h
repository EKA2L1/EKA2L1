/*
 * Copyright (c) 2020 EKA2L1 Team.
 *
 * This file is part of EKA2L1 project.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <cstdint>
#include <epoc/ptr.h>

namespace eka2l1::epoc {

    enum TLanguage {
        ELangTest = 0,

        /** UK English. */
        ELangEnglish = 1,

        /** French. */
        ELangFrench = 2,

        /** German. */
        ELangGerman = 3,

        /** Spanish. */
        ELangSpanish = 4,

        /** Italian. */
        ELangItalian = 5,

        /** Swedish. */
        ELangSwedish = 6,

        /** Danish. */
        ELangDanish = 7,

        /** Norwegian. */
        ELangNorwegian = 8,

        /** Finnish. */
        ELangFinnish = 9,

        /** American. */
        ELangAmerican = 10,

        /** Swiss French. */
        ELangSwissFrench = 11,

        /** Swiss German. */
        ELangSwissGerman = 12,

        /** Portuguese. */
        ELangPortuguese = 13,

        /** Turkish. */
        ELangTurkish = 14,

        /** Icelandic. */
        ELangIcelandic = 15,

        /** Russian. */
        ELangRussian = 16,

        /** Hungarian. */
        ELangHungarian = 17,

        /** Dutch. */
        ELangDutch = 18,

        /** Belgian Flemish. */
        ELangBelgianFlemish = 19,

        /** Australian English. */
        ELangAustralian = 20,

        /** Belgian French. */
        ELangBelgianFrench = 21,

        /** Austrian German. */
        ELangAustrian = 22,

        /** New Zealand English. */
        ELangNewZealand = 23,

        /** International French. */
        ELangInternationalFrench = 24,

        /** Czech. */
        ELangCzech = 25,

        /** Slovak. */
        ELangSlovak = 26,

        /** Polish. */
        ELangPolish = 27,

        /** Slovenian. */
        ELangSlovenian = 28,

        /** Taiwanese Chinese. */
        ELangTaiwanChinese = 29,

        /** Hong Kong Chinese. */
        ELangHongKongChinese = 30,

        /** Peoples Republic of China's Chinese. */
        ELangPrcChinese = 31,

        /** Japanese. */
        ELangJapanese = 32,

        /** Thai. */
        ELangThai = 33,

        /** Afrikaans. */
        ELangAfrikaans = 34,

        /** Albanian. */
        ELangAlbanian = 35,

        /** Amharic. */
        ELangAmharic = 36,

        /** Arabic. */
        ELangArabic = 37,

        /** Armenian. */
        ELangArmenian = 38,

        /** Tagalog. */
        ELangTagalog = 39,

        /** Belarussian. */
        ELangBelarussian = 40,

        /** Bengali. */
        ELangBengali = 41,

        /** Bulgarian. */
        ELangBulgarian = 42,

        /** Burmese. */
        ELangBurmese = 43,

        /** Catalan. */
        ELangCatalan = 44,

        /** Croatian. */
        ELangCroatian = 45,

        /** Canadian English. */
        ELangCanadianEnglish = 46,

        /** International English. */
        ELangInternationalEnglish = 47,

        /** South African English. */
        ELangSouthAfricanEnglish = 48,

        /** Estonian. */
        ELangEstonian = 49,

        /** Farsi. */
        ELangFarsi = 50,

        /** Canadian French. */
        ELangCanadianFrench = 51,

        /** Gaelic. */
        ELangScotsGaelic = 52,

        /** Georgian. */
        ELangGeorgian = 53,

        /** Greek. */
        ELangGreek = 54,

        /** Cyprus Greek. */
        ELangCyprusGreek = 55,

        /** Gujarati. */
        ELangGujarati = 56,

        /** Hebrew. */
        ELangHebrew = 57,

        /** Hindi. */
        ELangHindi = 58,

        /** Indonesian. */
        ELangIndonesian = 59,

        /** Irish. */
        ELangIrish = 60,

        /** Swiss Italian. */
        ELangSwissItalian = 61,

        /** Kannada. */
        ELangKannada = 62,

        /** Kazakh. */
        ELangKazakh = 63,

        /** Khmer. */
        ELangKhmer = 64,

        /** Korean. */
        ELangKorean = 65,

        /** Lao. */
        ELangLao = 66,

        /** Latvian. */
        ELangLatvian = 67,

        /** Lithuanian. */
        ELangLithuanian = 68,

        /** Macedonian. */
        ELangMacedonian = 69,

        /** Malay. */
        ELangMalay = 70,

        /** Malayalam. */
        ELangMalayalam = 71,

        /** Marathi. */
        ELangMarathi = 72,

        /** Moldavian. */
        ELangMoldavian = 73,

        /** Mongolian. */
        ELangMongolian = 74,

        /** Norwegian Nynorsk. */
        ELangNorwegianNynorsk = 75,

        /** Brazilian Portuguese. */
        ELangBrazilianPortuguese = 76,

        /** Punjabi. */
        ELangPunjabi = 77,

        /** Romanian. */
        ELangRomanian = 78,

        /** Serbian. */
        ELangSerbian = 79,

        /** Sinhalese. */
        ELangSinhalese = 80,

        /** Somali. */
        ELangSomali = 81,

        /** International Spanish. */
        ELangInternationalSpanish = 82,

        /** American Spanish. */
        ELangLatinAmericanSpanish = 83,

        /** Swahili. */
        ELangSwahili = 84,

        /** Finland Swedish. */
        ELangFinlandSwedish = 85,

        /** Reserved, not in use. */
        ELangReserved1 = 86, // This enum should not be used for new languages, see INC110543

        /** Tamil. */
        ELangTamil = 87,

        /** Telugu. */
        ELangTelugu = 88,

        /** Tibetan. */
        ELangTibetan = 89,

        /** Tigrinya. */
        ELangTigrinya = 90,

        /** Cyprus Turkish. */
        ELangCyprusTurkish = 91,

        /** Turkmen. */
        ELangTurkmen = 92,

        /** Ukrainian. */
        ELangUkrainian = 93,

        /** Urdu. */
        ELangUrdu = 94,

        /** Reserved, not in use. */
        ELangReserved2 = 95, // This enum should not be used for new languages, see INC110543

        /** Vietnamese. */
        ELangVietnamese = 96,

        /** Welsh. */
        ELangWelsh = 97,

        /** Zulu. */
        ELangZulu = 98,

        /**
        @deprecated

        Use of this value is deprecated.
        */
        ELangOther = 99,

        /** English with terms as used by the device manufacturer, if this needs to
        be distinct from the English used by the UI vendor. */
        ELangManufacturerEnglish = 100,

        /** South Sotho.

        A language of Lesotho also called Sesotho. SIL code sot. */
        ELangSouthSotho = 101,

        /** Basque. */
        ELangBasque = 102,

        /** Galician. */
        ELangGalician = 103,

        /** Javanese. */
        ELangJavanese = 104,

        /** Maithili. */
        ELangMaithili = 105,

        /** Azerbaijani(Latin alphabet). */
        ELangAzerbaijani_Latin = 106,

        /** Azerbaijani(Cyrillic alphabet). */
        ELangAzerbaijani_Cyrillic = 107,

        /** Oriya. */
        ELangOriya = 108,

        /** Bhojpuri. */
        ELangBhojpuri = 109,

        /** Sundanese. */
        ELangSundanese = 110,

        /** Kurdish(Latin alphabet). */
        ELangKurdish_Latin = 111,

        /** Kurdish(Arabic alphabet). */
        ELangKurdish_Arabic = 112,

        /** Pashto. */
        ELangPashto = 113,

        /** Hausa. */
        ELangHausa = 114,

        /** Oromo. */
        ELangOromo = 115,

        /** Uzbek(Latin alphabet). */
        ELangUzbek_Latin = 116,

        /** Uzbek(Cyrillic alphabet). */
        ELangUzbek_Cyrillic = 117,

        /** Sindhi(Arabic alphabet). */
        ELangSindhi_Arabic = 118,

        /** Sindhi(using Devanagari script). */
        ELangSindhi_Devanagari = 119,

        /** Yoruba. */
        ELangYoruba = 120,

        /** Cebuano. */
        ELangCebuano = 121,

        /** Igbo. */
        ELangIgbo = 122,

        /** Malagasy. */
        ELangMalagasy = 123,

        /** Nepali. */
        ELangNepali = 124,

        /** Assamese. */
        ELangAssamese = 125,

        /** Shona. */
        ELangShona = 126,

        /** Zhuang. */
        ELangZhuang = 127,

        /** Madurese. */
        ELangMadurese = 128,

        /** English as appropriate for use in Asia-Pacific regions. */
        ELangEnglish_Apac = 129,

        /** English as appropriate for use in Taiwan. */
        ELangEnglish_Taiwan = 157,

        /** English as appropriate for use in Hong Kong. */
        ELangEnglish_HongKong = 158,

        /** English as appropriate for use in the Peoples Republic of China. */
        ELangEnglish_Prc = 159,

        /** English as appropriate for use in Japan. */
        ELangEnglish_Japan = 160,

        /** English as appropriate for use in Thailand. */
        ELangEnglish_Thailand = 161,

        /** Fulfulde, also known as Fula */
        ELangFulfulde = 162,

        /** Tamazight. */
        ELangTamazight = 163,

        /** Bolivian Quechua. */
        ELangBolivianQuechua = 164,

        /** Peru Quechua. */
        ELangPeruQuechua = 165,

        /** Ecuador Quechua. */
        ELangEcuadorQuechua = 166,

        /** Tajik(Cyrillic alphabet). */
        ELangTajik_Cyrillic = 167,

        /** Tajik(using Perso-Arabic script). */
        ELangTajik_PersoArabic = 168,

        /** Nyanja, also known as Chichewa or Chewa. */
        ELangNyanja = 169,

        /** Haitian Creole. */
        ELangHaitianCreole = 170,

        /** Lombard. */
        ELangLombard = 171,

        /** Koongo, also known as Kongo or KiKongo. */
        ELangKoongo = 172,

        /** Akan. */
        ELangAkan = 173,

        /** Hmong. */
        ELangHmong = 174,

        /** Yi. */
        ELangYi = 175,

        /** Tshiluba, also known as Luba-Kasai */
        ELangTshiluba = 176,

        /** Ilocano, also know as Ilokano or Iloko. */
        ELangIlocano = 177,

        /** Uyghur. */
        ELangUyghur = 178,

        /** Neapolitan. */
        ELangNeapolitan = 179,

        /** Rwanda, also known as Kinyarwanda */
        ELangRwanda = 180,

        /** Xhosa. */
        ELangXhosa = 181,

        /** Balochi, also known as Baluchi */
        ELangBalochi = 182,

        /** Hiligaynon. */
        ElangHiligaynon = 183,

        /** Minangkabau. */
        ELangMinangkabau = 184,

        /** Makhuwa. */
        ELangMakhuwa = 185,

        /** Santali. */
        ELangSantali = 186,

        /** Gikuyu, sometimes written Kikuyu. */
        ELangGikuyu = 187,

        /** M�or�, also known as Mossi or More. */
        ELangMoore = 188,

        /** Guaran�. */
        ELangGuarani = 189,

        /** Rundi, also known as Kirundi. */
        ELangRundi = 190,

        /** Romani(Latin alphabet). */
        ELangRomani_Latin = 191,

        /** Romani(Cyrillic alphabet). */
        ELangRomani_Cyrillic = 192,

        /** Tswana. */
        ELangTswana = 193,

        /** Kanuri. */
        ELangKanuri = 194,

        /** Kashmiri(using Devanagari script). */
        ELangKashmiri_Devanagari = 195,

        /** Kashmiri(using Perso-Arabic script). */
        ELangKashmiri_PersoArabic = 196,

        /** Umbundu. */
        ELangUmbundu = 197,

        /** Konkani. */
        ELangKonkani = 198,

        /** Balinese, a language used in Indonesia (Java and Bali). */
        ELangBalinese = 199,

        /** Northern Sotho. */
        ELangNorthernSotho = 200,

        /** Wolof. */
        ELangWolof = 201,

        /** Bemba. */
        ELangBemba = 202,

        /** Tsonga. */
        ELangTsonga = 203,

        /** Yiddish. */
        ELangYiddish = 204,

        /** Kirghiz, also known as Kyrgyz. */
        ELangKirghiz = 205,

        /** Ganda, also known as Luganda. */
        ELangGanda = 206,

        /** Soga, also known as Lusoga. */
        ELangSoga = 207,

        /** Mbundu, also known as Kimbundu. */
        ELangMbundu = 208,

        /** Bambara. */
        ELangBambara = 209,

        /** Central Aymara. */
        ELangCentralAymara = 210,

        /** Zarma. */
        ELangZarma = 211,

        /** Lingala. */
        ELangLingala = 212,

        /** Bashkir. */
        ELangBashkir = 213,

        /** Chuvash. */
        ELangChuvash = 214,

        /** Swati. */
        ELangSwati = 215,

        /** Tatar. */
        ELangTatar = 216,

        /** Southern Ndebele. */
        ELangSouthernNdebele = 217,

        /** Sardinian. */
        ELangSardinian = 218,

        /** Scots. */
        ELangScots = 219,

        /** Meitei, also known as Meithei or Manipuri */
        ELangMeitei = 220,

        /** Walloon. */
        ELangWalloon = 221,

        /** Kabardian. */
        ELangKabardian = 222,

        /** Mazanderani, also know as Mazandarani or Tabri. */
        ELangMazanderani = 223,

        /** Gilaki. */
        ELangGilaki = 224,

        /** Shan. */
        ELangShan = 225,

        /** Luyia. */
        ELangLuyia = 226,

        /** Luo, also known as Dholuo, a language of Kenya. */
        ELanguageLuo = 227,

        /** Sukuma, also known as Kisukuma. */
        ELangSukuma = 228,

        /** Aceh, also known as Achinese. */
        ELangAceh = 229,

        /** English used in India. */
        ELangEnglish_India = 230,

        /** Malay as appropriate for use in Asia-Pacific regions. */
        ELangMalay_Apac = 326,

        /** Indonesian as appropriate for use in Asia-Pacific regions. */
        ELangIndonesian_Apac = 327,

        /**
        Indicates the final language in the language downgrade path.

        @see BaflUtils::NearestLanguageFile
        @see BaflUtils::GetDowngradePath
        */
        ELangNone = 0xFFFF, // up to 1023 languages * 16 dialects, in 16 bits
        ELangMaximum = ELangNone // This must always be equal to the last (largest) TLanguage enum.
    };

    struct locale_language {
        TLanguage language;
        eka2l1::ptr<char> date_suffix_table;
        eka2l1::ptr<char> day_table;
        eka2l1::ptr<char> day_abb_table;
        eka2l1::ptr<char> month_table;
        eka2l1::ptr<char> month_abb_table;
        eka2l1::ptr<char> am_pm_table;
        eka2l1::ptr<uint16_t> msg_table;
    };
}
