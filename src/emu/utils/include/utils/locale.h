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
#include <mem/ptr.h>

namespace eka2l1::epoc {
    using time = std::uint64_t;

    enum language {
        lang_test = 0,

        /** UK English. */
        lang_english = 1,

        /** French. */
        lang_french = 2,

        /** German. */
        lang_german = 3,

        /** Spanish. */
        lang_spanish = 4,

        /** Italian. */
        lang_italian = 5,

        /** Swedish. */
        lang_swedish = 6,

        /** Danish. */
        lang_danish = 7,

        /** Norwegian. */
        lang_norwegian = 8,

        /** Finnish. */
        lang_finnish = 9,

        /** American. */
        lang_american = 10,

        /** Swiss French. */
        lang_swissFrench = 11,

        /** Swiss German. */
        lang_swissGerman = 12,

        /** Portuguese. */
        lang_portuguese = 13,

        /** Turkish. */
        lang_turkish = 14,

        /** Icelandic. */
        lang_icelandic = 15,

        /** Russian. */
        lang_russian = 16,

        /** Hungarian. */
        lang_hungarian = 17,

        /** Dutch. */
        lang_dutch = 18,

        /** Belgian Flemish. */
        lang_belgianFlemish = 19,

        /** Australian English. */
        lang_australian = 20,

        /** Belgian French. */
        lang_belgianFrench = 21,

        /** Austrian German. */
        lang_austrian = 22,

        /** New Zealand English. */
        lang_newZealand = 23,

        /** International French. */
        lang_internationalFrench = 24,

        /** Czech. */
        lang_czech = 25,

        /** Slovak. */
        lang_slovak = 26,

        /** Polish. */
        lang_polish = 27,

        /** Slovenian. */
        lang_slovenian = 28,

        /** Taiwanese Chinese. */
        lang_taiwanChinese = 29,

        /** Hong Kong Chinese. */
        lang_hongKongChinese = 30,

        /** Peoples Republic of China's Chinese. */
        lang_prcChinese = 31,

        /** Japanese. */
        lang_japanese = 32,

        /** Thai. */
        lang_thai = 33,

        /** Afrikaans. */
        lang_afrikaans = 34,

        /** Albanian. */
        lang_albanian = 35,

        /** Amharic. */
        lang_amharic = 36,

        /** Arabic. */
        lang_arabic = 37,

        /** Armenian. */
        lang_armenian = 38,

        /** Tagalog. */
        lang_tagalog = 39,

        /** Belarussian. */
        lang_belarussian = 40,

        /** Bengali. */
        lang_bengali = 41,

        /** Bulgarian. */
        lang_bulgarian = 42,

        /** Burmese. */
        lang_burmese = 43,

        /** Catalan. */
        lang_catalan = 44,

        /** Croatian. */
        lang_croatian = 45,

        /** Canadian English. */
        lang_canadianEnglish = 46,

        /** International English. */
        lang_internationalEnglish = 47,

        /** South African English. */
        lang_southAfricanEnglish = 48,

        /** Estonian. */
        lang_estonian = 49,

        /** Farsi. */
        lang_farsi = 50,

        /** Canadian French. */
        lang_canadianFrench = 51,

        /** Gaelic. */
        lang_scotsGaelic = 52,

        /** Georgian. */
        lang_georgian = 53,

        /** Greek. */
        lang_greek = 54,

        /** Cyprus Greek. */
        lang_cyprusGreek = 55,

        /** Gujarati. */
        lang_gujarati = 56,

        /** Hebrew. */
        lang_hebrew = 57,

        /** Hindi. */
        lang_hindi = 58,

        /** Indonesian. */
        lang_indonesian = 59,

        /** Irish. */
        lang_irish = 60,

        /** Swiss Italian. */
        lang_swissItalian = 61,

        /** Kannada. */
        lang_kannada = 62,

        /** Kazakh. */
        lang_kazakh = 63,

        /** Khmer. */
        lang_khmer = 64,

        /** Korean. */
        lang_korean = 65,

        /** Lao. */
        lang_lao = 66,

        /** Latvian. */
        lang_latvian = 67,

        /** Lithuanian. */
        lang_lithuanian = 68,

        /** Macedonian. */
        lang_macedonian = 69,

        /** Malay. */
        lang_malay = 70,

        /** Malayalam. */
        lang_malayalam = 71,

        /** Marathi. */
        lang_marathi = 72,

        /** Moldavian. */
        lang_moldavian = 73,

        /** Mongolian. */
        lang_mongolian = 74,

        /** Norwegian Nynorsk. */
        lang_norwegianNynorsk = 75,

        /** Brazilian Portuguese. */
        lang_brazilianPortuguese = 76,

        /** Punjabi. */
        lang_punjabi = 77,

        /** Romanian. */
        lang_romanian = 78,

        /** Serbian. */
        lang_serbian = 79,

        /** Sinhalese. */
        lang_sinhalese = 80,

        /** Somali. */
        lang_somali = 81,

        /** International Spanish. */
        lang_internationalSpanish = 82,

        /** American Spanish. */
        lang_latinAmericanSpanish = 83,

        /** Swahili. */
        lang_swahili = 84,

        /** Finland Swedish. */
        lang_finlandSwedish = 85,

        /** Reserved, not in use. */
        lang_reserved1 = 86, // This enum should not be used for new languages, see INC110543

        /** Tamil. */
        lang_tamil = 87,

        /** Telugu. */
        lang_telugu = 88,

        /** Tibetan. */
        lang_tibetan = 89,

        /** Tigrinya. */
        lang_tigrinya = 90,

        /** Cyprus Turkish. */
        lang_cyprusTurkish = 91,

        /** Turkmen. */
        lang_turkmen = 92,

        /** Ukrainian. */
        lang_ukrainian = 93,

        /** Urdu. */
        lang_urdu = 94,

        /** Reserved, not in use. */
        lang_reserved2 = 95, // This enum should not be used for new languages, see INC110543

        /** Vietnamese. */
        lang_vietnamese = 96,

        /** Welsh. */
        lang_welsh = 97,

        /** Zulu. */
        lang_zulu = 98,

        /**
        @deprecated

        Use of this value is deprecated.
        */
        lang_other = 99,

        /** English with terms as used by the device manufacturer, if this needs to
        be distinct from the English used by the UI vendor. */
        lang_manufacturerEnglish = 100,

        /** South Sotho.

        A language of Lesotho also called Sesotho. SIL code sot. */
        lang_southSotho = 101,

        /** Basque. */
        lang_basque = 102,

        /** Galician. */
        lang_galician = 103,

        /** Javanese. */
        lang_javanese = 104,

        /** Maithili. */
        lang_maithili = 105,

        /** Azerbaijani(Latin alphabet). */
        lang_azerbaijani_latin = 106,

        /** Azerbaijani(Cyrillic alphabet). */
        lang_azerbaijani_cyrillic = 107,

        /** Oriya. */
        lang_oriya = 108,

        /** Bhojpuri. */
        lang_bhojpuri = 109,

        /** Sundanese. */
        lang_sundanese = 110,

        /** Kurdish(Latin alphabet). */
        lang_kurdish_latin = 111,

        /** Kurdish(Arabic alphabet). */
        lang_kurdish_arabic = 112,

        /** Pashto. */
        lang_pashto = 113,

        /** Hausa. */
        lang_hausa = 114,

        /** Oromo. */
        lang_oromo = 115,

        /** Uzbek(Latin alphabet). */
        lang_uzbek_latin = 116,

        /** Uzbek(Cyrillic alphabet). */
        lang_uzbek_cyrillic = 117,

        /** Sindhi(Arabic alphabet). */
        lang_sindhi_arabic = 118,

        /** Sindhi(using Devanagari script). */
        lang_sindhi_devanagari = 119,

        /** Yoruba. */
        lang_yoruba = 120,

        /** Cebuano. */
        lang_cebuano = 121,

        /** Igbo. */
        lang_igbo = 122,

        /** Malagasy. */
        lang_malagasy = 123,

        /** Nepali. */
        lang_nepali = 124,

        /** Assamese. */
        lang_assamese = 125,

        /** Shona. */
        lang_shona = 126,

        /** Zhuang. */
        lang_zhuang = 127,

        /** Madurese. */
        lang_madurese = 128,

        /** English as appropriate for use in Asia-Pacific regions. */
        lang_english_apac = 129,

        /** English as appropriate for use in Taiwan. */
        lang_english_taiwan = 157,

        /** English as appropriate for use in Hong Kong. */
        lang_english_hongKong = 158,

        /** English as appropriate for use in the Peoples Republic of China. */
        lang_english_prc = 159,

        /** English as appropriate for use in Japan. */
        lang_english_japan = 160,

        /** English as appropriate for use in Thailand. */
        lang_english_thailand = 161,

        /** Fulfulde, also known as Fula */
        lang_fulfulde = 162,

        /** Tamazight. */
        lang_tamazight = 163,

        /** Bolivian Quechua. */
        lang_bolivianQuechua = 164,

        /** Peru Quechua. */
        lang_peruQuechua = 165,

        /** Ecuador Quechua. */
        lang_ecuadorQuechua = 166,

        /** Tajik(Cyrillic alphabet). */
        lang_tajik_cyrillic = 167,

        /** Tajik(using Perso-Arabic script). */
        lang_tajik_persoArabic = 168,

        /** Nyanja, also known as Chichewa or Chewa. */
        lang_nyanja = 169,

        /** Haitian Creole. */
        lang_haitianCreole = 170,

        /** Lombard. */
        lang_lombard = 171,

        /** Koongo, also known as Kongo or KiKongo. */
        lang_koongo = 172,

        /** Akan. */
        lang_akan = 173,

        /** Hmong. */
        lang_hmong = 174,

        /** Yi. */
        lang_yi = 175,

        /** Tshiluba, also known as Luba-Kasai */
        lang_tshiluba = 176,

        /** Ilocano, also know as Ilokano or Iloko. */
        lang_ilocano = 177,

        /** Uyghur. */
        lang_uyghur = 178,

        /** Neapolitan. */
        lang_neapolitan = 179,

        /** Rwanda, also known as Kinyarwanda */
        lang_rwanda = 180,

        /** Xhosa. */
        lang_xhosa = 181,

        /** Balochi, also known as Baluchi */
        lang_balochi = 182,

        /** Hiligaynon. */
        lang_hiligaynon = 183,

        /** Minangkabau. */
        lang_minangkabau = 184,

        /** Makhuwa. */
        lang_makhuwa = 185,

        /** Santali. */
        lang_santali = 186,

        /** Gikuyu, sometimes written Kikuyu. */
        lang_gikuyu = 187,

        /** M�or�, also known as Mossi or More. */
        lang_moore = 188,

        /** Guaran�. */
        lang_guarani = 189,

        /** Rundi, also known as Kirundi. */
        lang_rundi = 190,

        /** Romani(Latin alphabet). */
        lang_romani_latin = 191,

        /** Romani(Cyrillic alphabet). */
        lang_romani_cyrillic = 192,

        /** Tswana. */
        lang_tswana = 193,

        /** Kanuri. */
        lang_kanuri = 194,

        /** Kashmiri(using Devanagari script). */
        lang_kashmiri_devanagari = 195,

        /** Kashmiri(using Perso-Arabic script). */
        lang_kashmiri_persoArabic = 196,

        /** Umbundu. */
        lang_umbundu = 197,

        /** Konkani. */
        lang_konkani = 198,

        /** Balinese, a language used in Indonesia (Java and Bali). */
        lang_balinese = 199,

        /** Northern Sotho. */
        lang_northernSotho = 200,

        /** Wolof. */
        lang_wolof = 201,

        /** Bemba. */
        lang_bemba = 202,

        /** Tsonga. */
        lang_tsonga = 203,

        /** Yiddish. */
        lang_yiddish = 204,

        /** Kirghiz, also known as Kyrgyz. */
        lang_kirghiz = 205,

        /** Ganda, also known as Luganda. */
        lang_ganda = 206,

        /** Soga, also known as Lusoga. */
        lang_soga = 207,

        /** Mbundu, also known as Kimbundu. */
        lang_mbundu = 208,

        /** Bambara. */
        lang_bambara = 209,

        /** Central Aymara. */
        lang_centralAymara = 210,

        /** Zarma. */
        lang_zarma = 211,

        /** Lingala. */
        lang_lingala = 212,

        /** Bashkir. */
        lang_bashkir = 213,

        /** Chuvash. */
        lang_chuvash = 214,

        /** Swati. */
        lang_swati = 215,

        /** Tatar. */
        lang_tatar = 216,

        /** Southern Ndebele. */
        lang_southernNdebele = 217,

        /** Sardinian. */
        lang_sardinian = 218,

        /** Scots. */
        lang_scots = 219,

        /** Meitei, also known as Meithei or Manipuri */
        lang_meitei = 220,

        /** Walloon. */
        lang_walloon = 221,

        /** Kabardian. */
        lang_kabardian = 222,

        /** Mazanderani, also know as Mazandarani or Tabri. */
        lang_mazanderani = 223,

        /** Gilaki. */
        lang_gilaki = 224,

        /** Shan. */
        lang_shan = 225,

        /** Luyia. */
        lang_luyia = 226,

        /** Luo, also known as Dholuo, a language of Kenya. */
        lang_uageLuo = 227,

        /** Sukuma, also known as Kisukuma. */
        lang_sukuma = 228,

        /** Aceh, also known as Achinese. */
        lang_aceh = 229,

        /** English used in India. */
        lang_english_india = 230,

        /** Malay as appropriate for use in Asia-Pacific regions. */
        lang_malay_apac = 326,

        /** Indonesian as appropriate for use in Asia-Pacific regions. */
        lang_indonesian_apac = 327,

        /**
        Indicates the final language in the language downgrade path.
        */
        lang_none = 0xFFFF, // up to 1023 languages * 16 dialects, in 16 bits
        lang_maximum = lang_none // This must always be equal to the last (largest) language enum.
    };

    enum date_format {
        date_format_america,
        date_format_european,
        date_format_japan
    };

    enum time_format {
        time_format_twelve_hours,
        time_format_twenty_four_hours
    };

    enum locale_pos {
        locale_before,
        locale_after,
    };

    enum negative_currency_format {
        negative_currency_leading_minus_sign,
        negative_currency_in_brackets,
        negative_currency_invervening_minus_sign_with_spaces,
        negative_currency_invervening_minus_sign_without_spaces,
        negative_currency_trailing_minus_sign,
    };

    enum daylight_saving_zone {
        daylight_saving_zone_dst_home = 0x40000000,
        daylight_saving_zone_none = 0,
        daylight_saving_zone_european = 1,
        daylight_saving_zone_northern = 2,
        daylight_saving_zone_southern = 4
    };

    enum day {
        monday,
        tuesday,
        wednesday,
        thursday,
        friday,
        saturday,
        sunday
    };

    enum clock_format {
        clock_analog,
        clock_digital
    };

    enum units_format {
        units_imperal,
        units_metric
    };

    enum digit_type {
        digit_type_unknown = 0x0000,
        digit_type_western = 0x0030,
        digit_type_arabic_indic = 0x0660,
        digit_type_eastern_arabic_indic = 0x6F0,
        digit_type_devanagari = 0x0966,
        digit_type_bengali = 0x09E6,
        digit_type_gurmukhi = 0x0A66,
        digit_type_gujarati = 0x0AE6,
        digit_type_oriya = 0x0B66,
        digit_type_tamil = 0x0BE6,
        digit_type_telugu = 0x0C66,
        digit_type_kannada = 0x0CE6,
        digit_type_malayalam = 0x0D66,
        digit_type_thai = 0x0E50,
        digit_type_lao = 0x0ED0,
        digit_type_tibetan = 0x0F20,
        digit_type_mayanmar = 0x1040,
        digit_type_khmer = 0x17E0,
        digit_type_all_types = 0xFFFF
    };

    enum device_time_state {
        device_user_time,
        nitz_network_time_sync
    };

    struct locale {
        std::int32_t country_code_;
        std::int32_t universal_time_offset_;
        date_format date_format_;
        time_format time_format_;
        locale_pos currency_symbol_position_;
        std::int32_t currency_space_between_;
        std::int32_t currency_decimal_places;
        negative_currency_format negative_currency_format_;
        std::int32_t currency_triads_allowed_;
        std::int32_t thousands_separator_;
        std::int32_t decimal_separator_;
        std::int32_t date_separator_[4];
        std::int32_t time_separator_[4];
        locale_pos am_pm_symbol_position_;
        std::int32_t am_pm_space_between_;
        std::uint32_t daylight_saving_;
        daylight_saving_zone home_daylight_saving_zone_;
        std::uint32_t work_days_;
        day start_of_week_;
        clock_format clock_format_;
        units_format units_general_;
        units_format units_dist_short_;
        units_format units_dist_long_;
        std::uint32_t extra_negative_currency_format_flags_;
        std::uint16_t language_downgrades_[3];
        std::uint16_t region_code_;
        digit_type digit_type_;
        device_time_state device_time_state_;
        std::int32_t spare_[0x12];
    };

    struct locale_language {
        epoc::language language;
        eka2l1::ptr<char> date_suffix_table;
        eka2l1::ptr<char> day_table;
        eka2l1::ptr<char> day_abb_table;
        eka2l1::ptr<char> month_table;
        eka2l1::ptr<char> month_abb_table;
        eka2l1::ptr<char> am_pm_table;
        eka2l1::ptr<uint16_t> msg_table;
    };

    struct locale_locale_settings {
        char16_t currency_symbols[9];
        std::uint32_t locale_extra_settings_dll_ptr;
    };
}
