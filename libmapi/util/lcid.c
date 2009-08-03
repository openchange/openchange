/*
   OpenChange MAPI implementation.
   Codepages handled by Microsoft Exchange Server

   Copyright (C) Julien Kerihuel 2005.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libmapi/libmapi.h>

#define	CP_WESTERN_EUROPE_AND_US	1	/* 1  (Western Europe & US) */
#define	CP_CENTRAL_EUROPE		2	/* 2  (Central Europe) */
#define	CP_BALTIC			3   	/* 3  (Baltic) */
#define	CP_GREEK			4	/* 4  (Greek) */
#define	CP_CYRILLIC			5	/* 5  (Cyrillic) */
#define	CP_TURKIC			6	/* 6  (Turkic) */
#define	CP_JAPENESE			7   	/* 7  (Japanese) */
#define	CP_KOREAN			8   	/* 8  (Korean) */
#define	CP_TRADITIONAL_CHINESE		9   	/* 9  (Traditional Chinese) */
#define	CP_SIMPLIFIED_CHINESE		10	/* 10 (Simplified Chinese) */
#define	CP_THAI				11  	/* 11 (Thai) */
#define	CP_HEBREW			12  	/* 12 (Hebrew) */
#define	CP_ARABIC			13  	/* 13 (Arabic) */
#define	CP_VIETNAMESE			14  	/* 14 (Vietnamese) */
#define	CP_INDIC			15  	/* 15 (Indic) */
#define	CP_GEORGIAN			16  	/* 16 (Georgian) */
#define	CP_ARMENIAN			17   	/* 17 (Armenian) */

#define	LANG_GROUP		"language group"
#define	LOCALE_ID		"locale id"
#define	INPUT_COMBINATIONS	"input combinations"

static const char *language_group[] = 
{
	"CP_NONEXISTENT",
	"CP_WESTERN_EUROPE_AND_US",
	"CP_CENTRAL_EUROPE",
	"CP_BALTIC",
	"CP_GREEK",
	"CP_CYRILLIC",
	"CP_TURKIC",
	"CP_JAPANESE",
	"CP_KOREAN",
	"CP_TRADITIONAL_CHINESE",
	"CP_SIMPLIFIED_CHINESE",
	"CP_THAI",
	"CP_HEBREW",
	"CP_ARABIC",
	"CP_VIETNAMESE",
	"CP_INDIC",
	"CP_GEORGIAN",
	"CP_ARMENIAN",
	NULL
};

struct combination {
	uint32_t	lcid;
	uint32_t	input_locale;
};

struct locale_struct {
	const char		*locale_str;
	uint32_t		lcid;
	const char		*lang_tag; /* RFC4646 language tag */
	int			language_group;
	struct combination	combination[6];
};

/*
Missing entries from this table (compared to MS-LCID v20081024:
 - everything from 0x0001 to 0x0091
 - 0x0417 =-> rm-CH
 - 0x0428 =-> tg-Cyrl-TJ
 - 0x042E =-> wen-DE
 - 0x0430 to 0x0435 (*-ZA)
 - 0x043A =-> mt-MT
 - 0x043B =-> se-NO
 - 0x0440 =-> ky-KG
 - 0x0442 =-> tk-TM
 - 0x0445 =-> bn-IN
 - 0x0446 =-> pa-IN
 - 0x0447 =-> gu-IN
 - 0x0448 =-> or-IN
 - 0x044A =-> te-IN
 - 0x044B =-> kn-IN
 - 0x044C =-> ml-IN
 - 0x044D =-> as-IN
 - 0x0450 =-> mn-MN
 - 0x0451 =-> bo-CN
 - 0x0452 =-> cy-GB
 - 0x0453 =-> km-KH
 - 0x0454 =-> lo-LA
 - 0x0455 =-> my-MM
 - 0x0456 =-> gl-ES
 - everything between 0x0458 and 0x048D
 - 0x0818 =-> ro-MO
 - 0x0819 =-> ru-MO
 - 0x0820 =-> ur-IN
 - 0x082E =-> dsb-DE
 - 0x083B =-> se-SE
 - 0x083C =-> ga-IE
 - 0x0845 =-> bn-BD
 - 0x0846 =-> pa-PK
 - 0x0850 =-> mn-Mong-CN
 - 0x0851 =-> bo-BT
 - 0x0859 =-> sd-PK
 - 0x085D =-> iu-Latn-CA
 - 0x085F =-> tzm-Latn-DZ
 - 0x0861 =-> ne-IN
 - 0x086B =-> quz-EC
 - 0x0873 =-> ti-ET
 - 0x0C3B =-> se-FI
 - 0x0C5F =-> tmz-MA
 - 0x0C6B =-> quz-PE
 - 0x101A =-> hr-BA
 - 0x103B =-> smj-NO
 - 0x141A =-> bs-Latn-BA
 - 0x143B =-> smj-SE
 - 0x181A =-> sr-Latn-BA
 - 0x183B =-> sma-NO
 - 0x1C0C =-> fr-West Indies
 - 0x1C1A =-> sr-Cyrl-BA
 - 0x1C3B =-> sma-SE
 - 0x200C =-> fr-RE
 - 0x201A =-> bs-Cyrl->BA
 - 0x203b =-> sms-FI
 - 0x240c =-> fr-CG
 - 0x243B =-> smn-FI
 - 0x280C =-> fr-SN
 - 0x2C0C =-> fr-CM
 - 0x300C =-> fr-CI
 - 0x340C =-> fr-ML
 - 0x3809 =-> en-ID
 - 0x380C =-> fr-MA
 - 0x3C09 =-> en-HK
 - 0x3C0C =-> fr-HT
 - 0x4009 =-> en-IN
 - 0x4409 =-> en-MY
 - 0x4809 =-> en-SG
 - everything from 0x540a to 0x7c68 (and after)
*/
static const struct locale_struct locales[] =
{
	{ "Afrikaans", 0x436, "af-ZA", CP_WESTERN_EUROPE_AND_US, { {0x436, 0x409}, {0x409, 0x409}, {0, 0} } },
	{ "Albanian", 0x41c, "sq-AL", CP_CENTRAL_EUROPE, { {0x41c, 0x41c}, {0x409, 0x409}, {0, 0} } },
	{ "Arabic_Saudi_Arabia", 0x401, "ar-SA", CP_ARABIC, { {0x409, 0x409},  {0x401, 0x401}, {0, 0} } },
	{ "Arabic_Iraq", 0x801, "ar-IQ", CP_ARABIC, { {0x409, 0x409}, {0x801, 0x401}, {0, 0} } },
	{ "Arabic_Egypt", 0xc01, "ar-EG", CP_ARABIC, { {0x409, 0x409}, {0xc01, 0x401}, {0, 0} } },
	{ "Arabic_Libya ", 0x1001, "ar-LY", CP_ARABIC, { {0x40c, 0x40c}, {0x1001, 0x20401}, {0, 0} } },
	{ "Arabic_Algeria", 0x1401, "ar-DZ", CP_ARABIC, { {0x40c, 0x40c}, {0x1401, 0x20401}, {0, 0} } },
	{ "Arabic_Morocco", 0x1801, "ar-MA", CP_ARABIC, { {0x40c, 0x40c}, {0x1801, 0x20401}, {0, 0} } },
	{ "Arabic_Tunisia", 0x1c01, "ar-TN", CP_ARABIC, { {0x40c, 0x40c}, {0x1c01, 0x20401}, {0, 0} } },
	{ "Arabic_Oman", 0x2001, "ar-OM", CP_ARABIC, { {0x409, 0x409}, {0x2001, 0x401}, {0, 0} } },
	{ "Arabic_Yemen", 0x2401, "ar-YE", CP_ARABIC, { {0x409, 0x409}, {0x2401, 0x401}, {0, 0} } },
	{ "Arabic_Syria", 0x2801, "ar-SY", CP_ARABIC, { {0x409,0x409}, {0x2801, 0x401}, {0, 0} } },
	{ "Arabic_Jordan", 0x2c01, "ar-JO", CP_ARABIC, { {0x409, 0x409}, {0x2c01, 0x401}, {0, 0} } },
	{ "Arabic_Lebanon", 0x3001, "ar-LB", CP_ARABIC, { {0x409, 0x409}, {0x3001, 0x401}, {0, 0} } },
	{ "Arabic_Kuwait", 0x3401, "ar-KW", CP_ARABIC, { {0x409, 0x409}, {0x3401, 0x401}, {0, 0} } },
	{ "Arabic_UAE", 0x3801, "ar-AE", CP_ARABIC, { {0x409, 0x409}, {0x3801, 0x401}, {0, 0} } },
	{ "Arabic_Bahrain", 0x3c01, "ar-BH", CP_ARABIC, { {0x409, 0x409}, {0x3c01, 0x401}, {0, 0} } },
	{ "Arabic_Qatar", 0x4001, "ar-QA", CP_ARABIC, { {0x409, 0x409}, {0x4001, 0x401}, {0, 0} } },
	{ "Armenian", 0x42b, "hy-AM", CP_ARMENIAN, { {0x42b, 0x42b}, {0x409, 0x409}, {0x419,0x419}, {0, 0} } },
	{ "Azeri_Latin", 0x42c, "az-Latn-AZ", CP_TURKIC, { {0x42c, 0x42c}, {0x82c, 0x82c}, {0x419, 0x419}, {0, 0} } },
	{ "Azeri_Cyrillic", 0x82c, "az-Cyrl-AZ", CP_CYRILLIC, { {0x82c, 0x82c}, {0x42c, 0x42c}, {0x419, 0x419}, {0, 0} } },
	{ "Basque", 0x42d, "eu-ES", CP_WESTERN_EUROPE_AND_US, { {0x42d, 0x40a}, {0x409, 0x409}, {0, 0} } },
	{ "Belarusian", 0x423, "be-BY", CP_CYRILLIC, { {0x423, 0x423}, {0x409, 0x409}, {0x419, 0x419}, {0, 0} } },
	{ "Bulgarian", 0x402, "bg-BG", CP_CYRILLIC, { {0x402, 0x402}, {0x409, 0x409}, {0, 0} } },
	{ "Catalan", 0x403, "ca-ES", CP_WESTERN_EUROPE_AND_US, { {0x403, 0x40a}, {0x409, 0x409}, {0, 0} } },
	{ "Chinese_Taiwan", 0x404, "zh-TW", CP_TRADITIONAL_CHINESE, { {0x404, 0x404}, {0x404, 0xe0080404}, {0x404, 0xe0010404}, {0, 0} } },
	{ "Chinese_PRC", 0x804, "zh-CN", CP_SIMPLIFIED_CHINESE, { {0x804, 0x804}, {0x804, 0xe00e0804}, {0x804, 0xe0010804}, {0x804, 0xe0030804}, {0x804, 0xe0040804}, {0, 0} } },
	{ "Chinese_Hong_Kong", 0xc04, "zh-HK", CP_TRADITIONAL_CHINESE, { {0x409, 0x409}, {0xc04, 0xe0080404}, {0, 0} } },
	{ "Chinese_Singapore", 0x1004, "zh-SG", CP_SIMPLIFIED_CHINESE, { {0x409, 0x409}, {0x804, 0xe00e0804}, {0x804, 0xe0010804}, {0x804, 0xe0030804}, {0x804, 0xe0040804}, {0, 0} } },
	{ "Chinese_Macau", 0x1404, "zh-MO", CP_TRADITIONAL_CHINESE, { {0x409, 0x409}, {0x804, 0xe00e0804}, {0x404, 0xe0020404}, {0x404, 0xe0080404}, {0, 0} } },
	{ "Croatian", 0x41a, "hr-HR", CP_CENTRAL_EUROPE, { {0x41a, 0x41a}, {0x409, 0x409}, {0, 0} } },
	{ "Czech", 0x405, "cz-CZ", CP_CENTRAL_EUROPE, { {0x405, 0x405}, {0x409, 0x409}, {0, 0} } },
	{ "Danish", 0x406, "da-DK", CP_WESTERN_EUROPE_AND_US, { {0x406, 0x406}, {0x409, 0x409}, {0, 0} } },
	{ "Dutch_Standard", 0x413, "nl-NL", CP_WESTERN_EUROPE_AND_US, { {0x409, 0x20409}, {0x413, 0x413}, {0x409, 0x409}, {0, 0} } },
	{ "Dutch_Belgian", 0x813, "nl-BE", CP_WESTERN_EUROPE_AND_US, { {0x813, 0x813}, {0x409, 0x409}, {0, 0} } },
	{ "English_United_States", 0x409, "en-US", CP_WESTERN_EUROPE_AND_US, { {0x409, 0x409}, {0, 0} } },
	{ "English_United_Kingdom", 0x809, "en-GB", CP_WESTERN_EUROPE_AND_US, { {0x809, 0x809}, {0, 0} } },
	{ "English_Australia", 0xc09, "en-AU", CP_WESTERN_EUROPE_AND_US, { {0xc09, 0x409}, {0, 0} } },
	{ "English_Canada", 0x1009, "en-CA", CP_WESTERN_EUROPE_AND_US, { {0x1009, 0x409}, {0x1009, 0x11009}, {0x1009, 0x1009}, {0, 0} } },
	{ "English_New_Zealand", 0x1409, "en-NZ", CP_WESTERN_EUROPE_AND_US, { {0x1409, 0x409}, {0, 0} } },
	{ "English_Ireland", 0x1809, "en-IE", CP_WESTERN_EUROPE_AND_US, { {0x1809, 0x1809}, {0x1809, 0x11809}, {0, 0} } },
	{ "English_South_Africa", 0x1c09, "en-ZA", CP_WESTERN_EUROPE_AND_US, { {0x1c09, 0x409}, {0, 0} } },
	{ "English_Jamaica", 0x2009, "en-JM", CP_WESTERN_EUROPE_AND_US, { {0x2009, 0x409}, {0, 0} } },
	{ "English_Caribbean", 0x2409, "en-CB", CP_WESTERN_EUROPE_AND_US, { {0x2409, 0x409}, {0, 0} } },
	{ "English_Belize", 0x2809, "en-BZ", CP_WESTERN_EUROPE_AND_US, { {0x2809, 0x409}, {0, 0} } },
	{ "English_Trinidad", 0x2c09, "en-TT", CP_WESTERN_EUROPE_AND_US, { {0x2c09, 0x409}, {0, 0} } },
	{ "English_Zimbabwe", 0x3009, "en-ZW", CP_WESTERN_EUROPE_AND_US, { {0x3009, 0x409}, {0, 0} } },
	{ "English_Philippines", 0x3409, "en-PH", CP_WESTERN_EUROPE_AND_US, { {0x3409, 0x409}, {0, 0} } },
	{ "Estonian", 0x425, "et-EE", CP_BALTIC, { {0x425, 0x425}, {0, 0} } },
	{ "Faeroese", 0x438, "fo-FO", CP_WESTERN_EUROPE_AND_US, { {0x438, 0x406}, {0x409, 0x409}, {0, 0} } },
	{ "Farsi", 0x429, "fa-IR", CP_ARABIC, { {0x409, 0x409}, {0x429, 0x429}, {0x429, 0x401}, {0, 0} } },
	{ "Finnish", 0x40b, "fi-FI", CP_WESTERN_EUROPE_AND_US, { {0x40b, 0x40b}, {0x409, 0x409}, {0, 0} } },
	{ "French_Standard", 0x40c, "fr-FR", CP_WESTERN_EUROPE_AND_US, { {0x40c, 0x40c}, {0x409, 0x409}, {0, 0} } },
	{ "French_Belgian", 0x80c, "fr-BE", CP_WESTERN_EUROPE_AND_US, { {0x80c, 0x80c}, {0x409, 0x409}, {0, 0} } },
	{ "French_Canadian", 0xc0c, "fr-CA", CP_WESTERN_EUROPE_AND_US, { {0xc0c, 0x11009}, {0x409, 0x409}, {0, 0} } },
	{ "French_Swiss", 0x100c, "fr-CH", CP_WESTERN_EUROPE_AND_US, { {0x100c, 0x100c}, {0x409, 0x409}, {0, 0} } },
	{ "French_Luxembourg", 0x140c, "fr-LU", CP_WESTERN_EUROPE_AND_US, { {0x140c, 0x40c}, {0x409, 0x409}, {0, 0} } },
	{ "French_Monaco", 0x180c, "fr-MC", CP_WESTERN_EUROPE_AND_US, { {0x180c, 0x40c}, {0x409, 0x409}, {0, 0} } },
	{ "Georgian", 0x437, "ka-GE", CP_GEORGIAN, { {0x437, 0x437}, {0x409, 0x409}, {0x419, 0x419}, {0, 0} } },
	{ "German_Standard", 0x407, "de-DE", CP_WESTERN_EUROPE_AND_US, { {0x407, 0x407}, {0x409, 0x409}, {0, 0} } },
	{ "German_Swiss", 0x807, "de-CH", CP_WESTERN_EUROPE_AND_US, { {0x807, 0x807}, {0x409, 0x409}, {0, 0} } },
	{ "German_Austria", 0xc07, "de-AT", CP_WESTERN_EUROPE_AND_US, { {0xc07, 0x407}, {0x409, 0x409}, {0, 0} } },
	{ "German_Luxembourg", 0x1007, "de-LU", CP_WESTERN_EUROPE_AND_US, { {0x1007, 0x407}, {0x409, 0x409}, {0, 0} } },
	{ "German_Liechtenstein", 0x1407, "de-LI", CP_WESTERN_EUROPE_AND_US, { {0x1407, 0x407}, {0x409, 0x409}, {0, 0} } },
	{ "Greek", 0x408, "el-GR", CP_GREEK, { {0x409, 0x409}, {0x408, 0x408}, {0, 0} } },
	{ "Hebrew", 0x40d, "he-IL", CP_HEBREW, { {0x409, 0x409}, {0x40d, 0x40d}, {0, 0} } },
	{ "Hindi", 0x439, "hi-IN", CP_INDIC, { {0x409, 0x409}, {0x439, 0x10439}, {0x439, 0x439}, {0, 0} } },
	{ "Hungarian", 0x40e, "hu-HU", CP_CENTRAL_EUROPE, { {0x40e, 0x40e}, {0x409, 0x409}, {0, 0} } },
	{ "Icelandic", 0x40f, "is-IS", CP_WESTERN_EUROPE_AND_US, { {0x40f, 0x40f}, {0x409, 0x409}, {0, 0} } },
	{ "Indonesian", 0x421, "id-ID", CP_WESTERN_EUROPE_AND_US, { {0x421, 0x409}, {0x409, 0x409}, {0, 0} } },
	{ "Italian_Standard", 0x410, "it-IT", CP_WESTERN_EUROPE_AND_US, { {0x410, 0x410}, {0x409, 0x409}, {0, 0} } },
	{ "Italian_Swiss", 0x810, "it-CH", CP_WESTERN_EUROPE_AND_US, { {0x810, 0x410}, {0x409, 0x409}, {0, 0} } },
	{ "Japanese", 0x411, "ja-JP", CP_JAPENESE, { {0x411, 0xe0010411}, {0, 0} } },
	{ "Kazakh", 0x43f, "kk-KZ", CP_CYRILLIC, { {0x43f, 0x43f}, {0x409, 0x409}, {0x419, 0x419}, {0, 0} } },
	{ "Konkani", 0x457, "kok-IN", CP_INDIC, { {0x409, 0x409}, {0x457, 0x439}, {0, 0} } },
	{ "Korean", 0x412, "ko-KR", CP_KOREAN, { {0x412, 0xe0010412}, {0, 0} } },
	{ "Latvian", 0x426, "lv-LV", CP_BALTIC, { {0x426, 0x10426}, {0, 0} } },
	{ "Lithuanian", 0x427, "lt-LT", CP_BALTIC, { {0x427, 0x10427}, {0, 0} } },
	{ "Macedonian", 0x42f, "mk-MK", CP_CYRILLIC, { {0x42f, 0x42f}, {0x409, 0x409}, {0, 0} } },
	{ "Malay_Malaysia", 0x43e, "ms-MY", CP_WESTERN_EUROPE_AND_US, { {0x409, 0x409}, {0, 0} } },
	{ "Malay_Brunei_Darussalam", 0x83e, "ms-BN", CP_WESTERN_EUROPE_AND_US, { {0x409, 0x409}, {0, 0} } },
	{ "Marathi", 0x44e, "mr-IN", CP_INDIC, { {0x409, 0x409}, {0x44e, 0x44e}, {0x44e, 0x439}, {0, 0} } },
	{ "Norwegian_Bokmal", 0x414, "nb-NO", CP_WESTERN_EUROPE_AND_US, { {0x414, 0x414}, {0x409, 0x409}, {0, 0} } },
	{ "Norwegian_Nynorsk", 0x814, "nn-NO", CP_WESTERN_EUROPE_AND_US, { {0x814, 0x414}, {0x409, 0x409}, {0, 0} } },
	{ "Polish", 0x415, "pl-PL", CP_CENTRAL_EUROPE, { {0x415, 0x10415}, {0x415, 0x415}, {0x409, 0x409}, {0, 0} } },
	{ "Portuguese_Brazilian", 0x416, "pt-BR", CP_WESTERN_EUROPE_AND_US, { {0x416, 0x416}, {0x409, 0x409}, {0, 0} } },
	{ "Portuguese_Standard", 0x816, "pt-PT", CP_WESTERN_EUROPE_AND_US, { {0x816, 0x816}, {0x409, 0x409}, {0, 0} } },
	{ "Romanian", 0x418, "ro-RM", CP_CENTRAL_EUROPE, { {0x418, 0x418}, {0x409, 0x409}, {0, 0} } },
	{ "Russian", 0x419, "ru-RU", CP_CYRILLIC, { {0x419, 0x419}, {0x409, 0x409}, {0, 0} } },
	{ "Sanskrit", 0x44f, "sa-IN", CP_INDIC, { {0x409, 0x409}, {0x44f, 0x439}, {0, 0} } },
	{ "Serbian_Latin", 0x81a, "sr-Latn-CS", CP_CENTRAL_EUROPE, { {0x81a, 0x81a}, {0x409, 0x409}, {0, 0} } },
	{ "Serbian_Cyrillic", 0xc1a, "sr-Cyrl-CS", CP_CYRILLIC, { {0xc1a, 0xc1a}, {0x409, 0x409}, {0, 0} } },
	{ "Slovak", 0x41b, "sk-SK", CP_CENTRAL_EUROPE, { {0x41b, 0x41b}, {0x409, 0x409}, {0, 0}, {0, 0} } },
	{ "Slovenian", 0x424, "sl-SI", CP_CENTRAL_EUROPE, { {0x424, 0x424}, {0x409, 0x409}, {0, 0}, {0, 0} } },
	{ "Spanish_Traditional_Sort", 0x40a, "es-ES_tradnl", CP_WESTERN_EUROPE_AND_US, { {0x40a, 0x40a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Mexican", 0x80a, "es-MX", CP_WESTERN_EUROPE_AND_US, { {0x80a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Modern_Sort", 0xc0a, "es-ES", CP_WESTERN_EUROPE_AND_US, { {0xc0a, 0x40a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Guatemala", 0x100a, "es-GT", CP_WESTERN_EUROPE_AND_US, { {0x100a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Costa_Rica", 0x140a, "es-CR", CP_WESTERN_EUROPE_AND_US, { {0x140a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Panama", 0x180a, "es-PA", CP_WESTERN_EUROPE_AND_US, { {0x180a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Dominican_Republic", 0x1c0a, "es-DO", CP_WESTERN_EUROPE_AND_US, { {0x1c0a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Venezuela", 0x200a, "es-VE", CP_WESTERN_EUROPE_AND_US, { {0x200a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Colombia", 0x240a, "es-CO", CP_WESTERN_EUROPE_AND_US, { {0x240a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Peru", 0x280a, "es-PE", CP_WESTERN_EUROPE_AND_US, { {0x280a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Argentina", 0x2c0a, "es-AR", CP_WESTERN_EUROPE_AND_US, { {0x2c0a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Ecuador", 0x300a, "es-EC", CP_WESTERN_EUROPE_AND_US, { {0x300a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Chile", 0x340a, "es-CL", CP_WESTERN_EUROPE_AND_US, { {0x340a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Uruguay", 0x380a, "es-UY",CP_WESTERN_EUROPE_AND_US, { {0x380a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Paraguay", 0x3c0a, "es-PY", CP_WESTERN_EUROPE_AND_US, { {0x3c0a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Bolivia", 0x400a, "es-BO", CP_WESTERN_EUROPE_AND_US, { {0x400a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_El_Salvador", 0x440a, "es-SV", CP_WESTERN_EUROPE_AND_US, { {0x440a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Honduras", 0x480a, "es-HN", CP_WESTERN_EUROPE_AND_US, { {0x480a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Nicaragua", 0x4c0a, "es-NI", CP_WESTERN_EUROPE_AND_US, { {0x4c0a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Puerto_Rico", 0x500a, "es-PR", CP_WESTERN_EUROPE_AND_US, { {0x500a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Swahili", 0x441, "sw-KE", CP_WESTERN_EUROPE_AND_US, { {0x409, 0x409}, {0, 0} } },
	{ "Swedish", 0x41d, "sv-SE", CP_WESTERN_EUROPE_AND_US, { {0x41d, 0x41d}, {0x409, 0x409}, {0, 0} } },
	{ "Swedish_Finland", 0x81d, "sv-FI", CP_WESTERN_EUROPE_AND_US, { {0x81d, 0x41d}, {0x409, 0x409}, {0, 0} } },
	{ "Tamil", 0x449, "ta-IN", CP_INDIC, { {0x409, 0x409}, {0x449, 0x449}, {0, 0} } },
	{ "Tatar", 0x444, "tt-RU", CP_CYRILLIC, { {0x444, 0x444}, {0x409, 0x409}, {0x419, 0x419}, {0, 0} } },
	{ "Thai", 0x41e, "th-TH", CP_THAI, { {0x409, 0x409}, {0x41e, 0x41e}, {0, 0} } },
	{ "Turkish", 0x41f, "tr-TR", CP_TURKIC, { {0x41f, 0x41f}, {0x409, 0x41f}, {0, 0} } },
	{ "Ukrainian", 0x422, "uk-UA", CP_CYRILLIC, { {0x422, 0x422}, {0x409, 0x409}, {0, 0} } },
	{ "Urdu", 0x420, "ur-PK", CP_ARABIC, { {0x420, 0x401}, {0x409, 0x409}, {0, 0} } },
	{ "Uzbek_Latin", 0x443, "uz-Latn-UZ", CP_TURKIC, { {0x443, 0x409}, {0x843, 0x843}, {0x419, 0x419}, {0, 0} } },
	{ "Uzbek_Cyrillic", 0x843, "uz-Cyrl-UZ", CP_CYRILLIC, { {0x843, 0x843}, {0x443, 0x409}, {0x419, 0x419}, {0, 0} } },
	{ "Vietnamese", 0x42a, "vi-VN", CP_VIETNAMESE, { {0x409, 0x409}, {0x42a, 0x42a}, {0, 0} } },
	{ NULL, 0, NULL, 0, { { 0, 0 } } }
};

/*
  Print locale_id information for a given Language
*/

_PUBLIC_ bool lcid_get_locales(const char *lang)
{
	int idx = 0;
	
	if (!lang) {
		return false;
	}
		
	while (locales[idx].locale_str != NULL) {
		if (!strcmp(lang, locales[idx].locale_str)) {
			DEBUG(0, ("locale:language = 0x%x\n", locales[idx].lcid));
			/* Fix me */
			DEBUG(0, ("locale:method = 0x%x\n", locales[idx].combination[1].lcid));
			return true;
		}
		idx++;
	}
	return false;
}

/*
  Check if the locale provided exists
*/

_PUBLIC_ bool lcid_valid_locale(uint32_t locale)
{
	int idx = 0;

	while (locales[idx].locale_str != NULL) {
		if (locales[idx].lcid == locale)
			return true;
		idx++;
	}
	return false;
}

/*
  Find the short language code for a given language code
*/

_PUBLIC_ const char *lcid_langcode2langtag(uint32_t langcode)
{
	int idx = 0;

	while (locales[idx].locale_str != NULL) {
		if (locales[idx].lcid == langcode)
			return locales[idx].lang_tag;
		idx++;
	}
	return NULL;
}

/*
  Print locale information for a given locale id
 */

_PUBLIC_ bool lcid_print_locale(uint32_t locale)
{
	int idx = 0;
	int i = 0;

	while (locales[idx].locale_str != NULL) {
		if (locales[idx].lcid == locale) {
			printf("%s:\n", locales[idx].locale_str);
			printf("\t%-25s:    %s\n", LANG_GROUP, language_group[locales[idx].language_group]);
			printf("\t%-25s:    0x%x\n", LOCALE_ID, locales[idx].lcid);
			printf("\t%-25s:\n", INPUT_COMBINATIONS);
			for (i = 0; locales[idx].combination[i].lcid != 0; i++) {
				printf("\t\t\t\t      0x%x:0x%x\n", 
				       locales[idx].combination[i].lcid,
				       locales[idx].combination[i].input_locale);
			}
			return true;
		}
		idx++;
	}
	return false;
}

/*
  Print all languages
*/
_PUBLIC_ void lcid_print_languages(void)
{
	int idx = 0;

	while (locales[idx].locale_str != NULL) {
		printf("\t%s\n", locales[idx].locale_str);
		idx++;
	}
}

/*
  Convert language name (as a string) to an integer language code
 */

_PUBLIC_ uint32_t lcid_lang2lcid(const char *name)
{
	int idx = 0;

	if (!name) {
		return 0xFFFFFFFF;
	}
		
	while (locales[idx].locale_str != NULL) {
		if (strncasecmp(locales[idx].locale_str, name, strlen(name)) == 0) {
			// we found a match
			return locales[idx].lcid;
		}
		idx++;
	}
	return 0xFFFFFFFF;
}

/*
  Print language groups
 */

_PUBLIC_ void lcid_print_group(void)
{
	int idx = 1;

	while (language_group[idx]) {
		printf("\t\t%s\n", language_group[idx]);
		idx++;
	}
}

/*
  Print languages associated to a single language group
 */

_PUBLIC_ bool lcid_print_groupmember(int group)
{
	uint32_t idx = 0;

	if (group == -1) {
	  DEBUG(0, ("Invalid language group "));
	  return false;
	}

	DEBUG(0, ("%s:\n", language_group[group]));

	for (idx = 0; locales[idx].locale_str != NULL; idx++) {
		if (locales[idx].language_group == group) {
			printf("\t\t\t%s\n", locales[idx].locale_str);
		}
	}
	return true;
}

/*
  Convert language group from string to integer
 */

_PUBLIC_ int lcid_lang2nb(const char *name)
{
	int		idx = 0;

	if (!name) {
		return -1;
	}
		
	while (language_group[idx]) {
		if (!strcmp(language_group[idx], name)) {
			return idx++;
		}
		idx++;
	}
	return -1;
}
