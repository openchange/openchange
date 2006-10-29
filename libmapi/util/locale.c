/*
 *  OpenChange MAPI implementation.
 *  Locales handled by Microsoft Windows Exchange
 *
 *  Copyright (C) Julien Kerihuel 2005.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "openchange.h"
#include <core.h>
#include <util.h>

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
	uint32_t		language_group;
	struct combination	combination[6];
};

static const struct locale_struct locales[] =
{
	{ "Afrikaans", 0x436, CP_WESTERN_EUROPE_AND_US, { {0x436, 0x409}, {0x409, 0x409}, {0, 0} } },
	{ "Albanian", 0x41c, CP_CENTRAL_EUROPE, { {0x41c, 0x41c}, {0x409, 0x409}, {0, 0} } },
	{ "Arabic_Saudi_Arabia", 0x401, CP_ARABIC, { {0x409, 0x409},  {0x401, 0x401}, {0, 0} } },
	{ "Arabic_Iraq", 0x801, CP_ARABIC, { {0x409, 0x409}, {0x801, 0x401}, {0, 0} } },
	{ "Arabic_Egypt", 0xc01, CP_ARABIC, { {0x409, 0x409}, {0xc01, 0x401}, {0, 0} } },
	{ "Arabic_Libya ", 0x1001, CP_ARABIC, { {0x40c, 0x40c}, {0x1001, 0x20401}, {0, 0} } },
	{ "Arabic_Algeria", 0x1401, CP_ARABIC, { {0x40c, 0x40c}, {0x1401, 0x20401}, {0, 0} } },
	{ "Arabic_Morocco", 0x1801, CP_ARABIC, { {0x40c, 0x40c}, {0x1801, 0x20401}, {0, 0} } },
	{ "Arabic_Tunisia", 0x1c01, CP_ARABIC, { {0x40c, 0x40c}, {0x1c01, 0x20401}, {0, 0} } },
	{ "Arabic_Oman", 0x2001, CP_ARABIC, { {0x409, 0x409}, {0x2001, 0x401}, {0, 0} } },
	{ "Arabic_Yemen", 0x2401, CP_ARABIC, { {0x409, 0x409}, {0x2401, 0x401}, {0, 0} } },
	{ "Arabic_Syria", 0x2801, CP_ARABIC, { {0x409,0x409}, {0x2801, 0x401}, {0, 0} } },
	{ "Arabic_Jordan", 0x2c01, CP_ARABIC, { {0x409, 0x409}, {0x2c01, 0x401}, {0, 0} } },
	{ "Arabic_Lebanon", 0x3001, CP_ARABIC, { {0x409, 0x409}, {0x3001, 0x401}, {0, 0} } },
	{ "Arabic_Kuwait", 0x3401, CP_ARABIC, { {0x409, 0x409}, {0x3401, 0x401}, {0, 0} } },
	{ "Arabic_UAE", 0x3801, CP_ARABIC, { {0x409, 0x409}, {0x3801, 0x401}, {0, 0} } },
	{ "Arabic_Bahrain", 0x3c01, CP_ARABIC, { {0x409, 0x409}, {0x3c01, 0x401}, {0, 0} } },
	{ "Arabic_Qatar", 0x4001, CP_ARABIC, { {0x409, 0x409}, {0x4001, 0x401}, {0, 0} } },
	{ "Armenian", 0x42b, CP_ARMENIAN, { {0x42b, 0x42b}, {0x409, 0x409}, {0x419,0x419}, {0, 0} } },
	{ "Azeri_Latin", 0x42c, CP_TURKIC, { {0x42c, 0x42c}, {0x82c, 0x82c}, {0x419, 0x419}, {0, 0} } },
	{ "Azeri_Cyrillic", 0x82c, CP_CYRILLIC, { {0x82c, 0x82c}, {0x42c, 0x42c}, {0x419, 0x419}, {0, 0} } },
	{ "Basque", 0x42d, CP_WESTERN_EUROPE_AND_US, { {0x42d, 0x40a}, {0x409, 0x409}, {0, 0} } },
	{ "Belarusian", 0x423, CP_CYRILLIC, { {0x423, 0x423}, {0x409, 0x409}, {0x419, 0x419}, {0, 0} } },
	{ "Bulgarian", 0x402, CP_CYRILLIC, { {0x402, 0x402}, {0x409, 0x409}, {0, 0} } },
	{ "Catalan", 0x403, CP_WESTERN_EUROPE_AND_US, { {0x403, 0x40a}, {0x409, 0x409}, {0, 0} } },
	{ "Chinese_Taiwan", 0x404, CP_TRADITIONAL_CHINESE, { {0x404, 0x404}, {0x404, 0xe0080404}, {0x404, 0xe0010404}, {0, 0} } },
	{ "Chinese_PRC", 0x804, CP_SIMPLIFIED_CHINESE, { {0x804, 0x804}, {0x804, 0xe00e0804}, {0x804, 0xe0010804}, {0x804, 0xe0030804}, {0x804, 0xe0040804}, {0, 0} } },
	{ "Chinese_Hong_Kong", 0xc04, CP_TRADITIONAL_CHINESE, { {0x409, 0x409}, {0xc04, 0xe0080404}, {0, 0} } },
	{ "Chinese_Singapore", 0x1004, CP_SIMPLIFIED_CHINESE, { {0x409, 0x409}, {0x804, 0xe00e0804}, {0x804, 0xe0010804}, {0x804, 0xe0030804}, {0x804, 0xe0040804}, {0, 0} } },
	{ "Chinese_Macau", 0x1404, CP_TRADITIONAL_CHINESE, { {0x409, 0x409}, {0x804, 0xe00e0804}, {0x404, 0xe0020404}, {0x404, 0xe0080404}, {0, 0} } },
	{ "Croatian", 0x41a, CP_CENTRAL_EUROPE, { {0x41a, 0x41a}, {0x409, 0x409}, {0, 0} } },
	{ "Czech", 0x405, CP_CENTRAL_EUROPE, { {0x405, 0x405}, {0x409, 0x409}, {0, 0} } },
	{ "Danish", 0x406, CP_WESTERN_EUROPE_AND_US, { {0x406, 0x406}, {0x409, 0x409}, {0, 0} } },
	{ "Dutch_Standard", 0x413, CP_WESTERN_EUROPE_AND_US, { {0x409, 0x20409}, {0x413, 0x413}, {0x409, 0x409}, {0, 0} } },
	{ "Dutch_Belgian", 0x813, CP_WESTERN_EUROPE_AND_US, { {0x813, 0x813}, {0x409, 0x409}, {0, 0} } },
	{ "English_United_States", 0x409, CP_WESTERN_EUROPE_AND_US, { {0x409, 0x409}, {0, 0} } },
	{ "English_United_Kingdom", 0x809, CP_WESTERN_EUROPE_AND_US, { {0x809, 0x809}, {0, 0} } },
	{ "English_Australian", 0xc09, CP_WESTERN_EUROPE_AND_US, { {0xc09, 0x409}, {0, 0} } },
	{ "English_Canadian", 0xc09, CP_WESTERN_EUROPE_AND_US, { {0x1009, 0x409}, {0x1009, 0x11009}, {0x1009, 0x1009}, {0, 0} } },
	{ "English_New_Zealand", 0x1409, CP_WESTERN_EUROPE_AND_US, { {0x1409, 0x409}, {0, 0} } },
	{ "English_Irish", 0x1809, CP_WESTERN_EUROPE_AND_US, { {0x1809, 0x1809}, {0x1809, 0x11809}, {0, 0} } },
	{ "English_South_Africa", 0x1c09, CP_WESTERN_EUROPE_AND_US, { {0x1c09, 0x409}, {0, 0} } },
	{ "English_Jamaica", 0x2009, CP_WESTERN_EUROPE_AND_US, { {0x2009, 0x409}, {0, 0} } },
	{ "English_Caribbean", 0x2409, CP_WESTERN_EUROPE_AND_US, { {0x2409, 0x409}, {0, 0} } },
	{ "English_Belize", 0x2809, CP_WESTERN_EUROPE_AND_US, { {0x2809, 0x409}, {0, 0} } },
	{ "English_Trinidad", 0x2c09, CP_WESTERN_EUROPE_AND_US, { {0x2c09, 0x409}, {0, 0} } },
	{ "English_Zimbabwe", 0x3009, CP_WESTERN_EUROPE_AND_US, { {0x3009, 0x409}, {0, 0} } },
	{ "English_Philippines", 0x3409, CP_WESTERN_EUROPE_AND_US, { {0x3409, 0x409}, {0, 0} } },
	{ "Estonian", 0x425, CP_BALTIC, { {0x425, 0x425}, {0, 0} } },
	{ "Faeroese", 0x438, CP_WESTERN_EUROPE_AND_US, { {0x438, 0x406}, {0x409, 0x409}, {0, 0} } },
	{ "Farsi", 0x429, CP_ARABIC, { {0x409, 0x409}, {0x429, 0x429}, {0x429, 0x401}, {0, 0} } },
	{ "Finnish", 0x40b, CP_WESTERN_EUROPE_AND_US, { {0x40b, 0x40b}, {0x409, 0x409}, {0, 0} } },
	{ "French_Standard", 0x40c, CP_WESTERN_EUROPE_AND_US, { {0x40c, 0x40c}, {0x409, 0x409}, {0, 0} } },
	{ "French_Belgian", 0x80c, CP_WESTERN_EUROPE_AND_US, { {0x80c, 0x80c}, {0x409, 0x409}, {0, 0} } },
	{ "French_Canadian", 0xc0c, CP_WESTERN_EUROPE_AND_US, { {0xc0c, 0x11009}, {0x409, 0x409}, {0, 0} } },
	{ "French_Swiss", 0x100c, CP_WESTERN_EUROPE_AND_US, { {0x100c, 0x100c}, {0x409, 0x409}, {0, 0} } },
	{ "French_Luxembourg", 0x140c, CP_WESTERN_EUROPE_AND_US, { {0x140c, 0x40c}, {0x409, 0x409}, {0, 0} } },
	{ "French_Monaco", 0x180c, CP_WESTERN_EUROPE_AND_US, { {0x180c, 0x40c}, {0x409, 0x409}, {0, 0} } },
	{ "Georgian", 0x437, CP_GEORGIAN, { {0x437, 0x437}, {0x409, 0x409}, {0x419, 0x419}, {0, 0} } },
	{ "German_Standard", 0x407, CP_WESTERN_EUROPE_AND_US, { {0x407, 0x407}, {0x409, 0x409}, {0, 0} } },
	{ "German_Swiss", 0x807, CP_WESTERN_EUROPE_AND_US, { {0x807, 0x807}, {0x409, 0x409}, {0, 0} } },
	{ "German_Austrian", 0xc07, CP_WESTERN_EUROPE_AND_US, { {0xc07, 0x407}, {0x409, 0x409}, {0, 0} } },
	{ "German_Luxembourg", 0x1007, CP_WESTERN_EUROPE_AND_US, { {0x1007, 0x407}, {0x409, 0x409}, {0, 0} } },
	{ "German_Liechtenstein", 0x1407, CP_WESTERN_EUROPE_AND_US, { {0x1407, 0x407}, {0x409, 0x409}, {0, 0} } },
	{ "Greek", 0x408, CP_GREEK, { {0x409, 0x409}, {0x408, 0x408}, {0, 0} } },
	{ "Hebrew", 0x40d, CP_HEBREW, { {0x409, 0x409}, {0x40d, 0x40d}, {0, 0} } },
	{ "Hindi", 0x439, CP_INDIC, { {0x409, 0x409}, {0x439, 0x10439}, {0x439, 0x439}, {0, 0} } },
	{ "Hungarian", 0x40e, CP_CENTRAL_EUROPE, { {0x40e, 0x40e}, {0x409, 0x409}, {0, 0} } },
	{ "Icelandic", 0x40f, CP_WESTERN_EUROPE_AND_US, { {0x40f, 0x40f}, {0x409, 0x409}, {0, 0} } },
	{ "Indonesian", 0x421, CP_WESTERN_EUROPE_AND_US, { {0x421, 0x409}, {0x409, 0x409}, {0, 0} } },
	{ "Italian_Standard", 0x410, CP_WESTERN_EUROPE_AND_US, { {0x410, 0x410}, {0x409, 0x409}, {0, 0} } },
	{ "Italian_Swiss", 0x810, CP_WESTERN_EUROPE_AND_US, { {0x810, 0x410}, {0x409, 0x409}, {0, 0} } },
	{ "Japanese", 0x411, CP_JAPENESE, { {0x411, 0xe0010411}, {0, 0} } },
	{ "Kazakh", 0x43f, CP_CYRILLIC, { {0x43f, 0x43f}, {0x409, 0x409}, {0x419, 0x419}, {0, 0} } },
	{ "Konkani", 0x457, CP_INDIC, { {0x409, 0x409}, {0x457, 0x439}, {0, 0} } },
	{ "Korean", 0x412, CP_KOREAN, { {0x412, 0xe0010412}, {0, 0} } },
	{ "Latvian", 0x426, CP_BALTIC, { {0x426, 0x10426}, {0, 0} } },
	{ "Lithuanian", 0x427, CP_BALTIC, { {0x427, 0x10427}, {0, 0} } },
	{ "Macedonian", 0x42f, CP_CYRILLIC, { {0x42f, 0x42f}, {0x409, 0x409}, {0, 0} } },
	{ "Malay_Malaysia", 0x43e, CP_WESTERN_EUROPE_AND_US, { {0x409, 0x409}, {0, 0} } },
	{ "Malay_Brunei_Darussalam", 0x83e, CP_WESTERN_EUROPE_AND_US, { {0x409, 0x409}, {0, 0} } },
	{ "Marathi", 0x44e, CP_INDIC, { {0x409, 0x409}, {0x44e, 0x44e}, {0x44e, 0x439}, {0, 0} } },
	{ "Norwegian_Bokmal", 0x414, CP_WESTERN_EUROPE_AND_US, { {0x414, 0x414}, {0x409, 0x409}, {0, 0} } },
	{ "Norwegian_Nynorsk", 0x814, CP_WESTERN_EUROPE_AND_US, { {0x814, 0x414}, {0x409, 0x409}, {0, 0} } },
	{ "Polish", 0x415, CP_CENTRAL_EUROPE, { {0x415, 0x10415}, {0x415, 0x415}, {0x409, 0x409}, {0, 0} } },
	{ "Portuguese_Brazilian", 0x416, CP_WESTERN_EUROPE_AND_US, { {0x416, 0x416}, {0x409, 0x409}, {0, 0} } },
	{ "Portuguese_Standard", 0x816, CP_WESTERN_EUROPE_AND_US, { {0x816, 0x816}, {0x409, 0x409}, {0, 0} } },
	{ "Romanian", 0x418, CP_CENTRAL_EUROPE, { {0x418, 0x418}, {0x409, 0x409}, {0, 0} } },
	{ "Russian", 0x419, CP_CYRILLIC, { {0x419, 0x419}, {0x409, 0x409}, {0, 0} } },
	{ "Sanskrit", 0x44f, CP_INDIC, { {0x409, 0x409}, {0x44f, 0x439}, {0, 0} } },
	{ "Serbian_Latin", 0x81a, CP_CENTRAL_EUROPE, { {0x81a, 0x81a}, {0x409, 0x409}, {0, 0} } },
	{ "Serbian_Cyrillic", 0xc1a, CP_CYRILLIC, { {0xc1a, 0xc1a}, {0x409, 0x409}, {0, 0} } },
	{ "Slovak", 0x41b, CP_CENTRAL_EUROPE, { {0x41b, 0x41b}, {0x409, 0x409}, {0, 0}, {0, 0} } },
	{ "Slovenian", 0x424, CP_CENTRAL_EUROPE, { {0x424, 0x424}, {0x409, 0x409}, {0, 0}, {0, 0} } },
	{ "Spanish_Traditional_Sort", 0x40a, CP_WESTERN_EUROPE_AND_US, { {0x40a, 0x40a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Mexican", 0x80a, CP_WESTERN_EUROPE_AND_US, { {0x80a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Modern_Sort", 0xc0a, CP_WESTERN_EUROPE_AND_US, { {0xc0a, 0x40a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Guatemala", 0x100a, CP_WESTERN_EUROPE_AND_US, { {0x100a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Costa_Rica", 0x140a, CP_WESTERN_EUROPE_AND_US, { {0x140a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Panama", 0x180a, CP_WESTERN_EUROPE_AND_US, { {0x180a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Dominican_Republic", 0x1c0a, CP_WESTERN_EUROPE_AND_US, { {0x1c0a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Venezuela", 0x200a, CP_WESTERN_EUROPE_AND_US, { {0x200a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Colombia", 0x240a, CP_WESTERN_EUROPE_AND_US, { {0x240a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Peru", 0x280a, CP_WESTERN_EUROPE_AND_US, { {0x280a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Argentina", 0x2c0a, CP_WESTERN_EUROPE_AND_US, { {0x2c0a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Ecuador", 0x300a, CP_WESTERN_EUROPE_AND_US, { {0x300a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Chile", 0x340a, CP_WESTERN_EUROPE_AND_US, { {0x340a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Uruguay", 0x380a, CP_WESTERN_EUROPE_AND_US, { {0x380a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Paraguay", 0x3c0a, CP_WESTERN_EUROPE_AND_US, { {0x3c0a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Bolivia", 0x400a, CP_WESTERN_EUROPE_AND_US, { {0x400a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_El_Salvador", 0x440a, CP_WESTERN_EUROPE_AND_US, { {0x440a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Honduras", 0x480a, CP_WESTERN_EUROPE_AND_US, { {0x480a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Nicaragua", 0x4c0a, CP_WESTERN_EUROPE_AND_US, { {0x4c0a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Spanish_Puerto_Rico", 0x500a, CP_WESTERN_EUROPE_AND_US, { {0x500a, 0x80a}, {0x409, 0x409}, {0, 0} } },
	{ "Swahili", 0x441, CP_WESTERN_EUROPE_AND_US, { {0x409, 0x409}, {0, 0} } },
	{ "Swedish", 0x41d, CP_WESTERN_EUROPE_AND_US, { {0x41d, 0x41d}, {0x409, 0x409}, {0, 0} } },
	{ "Swedish_Finland", 0x81d, CP_WESTERN_EUROPE_AND_US, { {0x81d, 0x41d}, {0x409, 0x409}, {0, 0} } },
	{ "Tamil", 0x449, CP_INDIC, { {0x409, 0x409}, {0x449, 0x449}, {0, 0} } },
	{ "Tatar", 0x444, CP_CYRILLIC, { {0x444, 0x444}, {0x409, 0x409}, {0x419, 0x419}, {0, 0} } },
	{ "Thai", 0x41e, CP_THAI, { {0x409, 0x409}, {0x41e, 0x41e}, {0, 0} } },
	{ "Turkish", 0x41f, CP_TURKIC, { {0x41f, 0x41f}, {0x409, 0x41f}, {0, 0} } },
	{ "Ukrainian", 0x422, CP_CYRILLIC, { {0x422, 0x422}, {0x409, 0x409}, {0, 0} } },
	{ "Urdu", 0x420, CP_ARABIC, { {0x420, 0x401}, {0x409, 0x409}, {0, 0} } },
	{ "Uzbek_Latin", 0x443, CP_TURKIC, { {0x443, 0x409}, {0x843, 0x843}, {0x419, 0x419}, {0, 0} } },
	{ "Uzbek_Cyrillic", 0x843, CP_CYRILLIC, { {0x843, 0x843}, {0x443, 0x409}, {0x419, 0x419}, {0, 0} } },
	{ "Vietnamese", 0x42a, CP_VIETNAMESE, { {0x409, 0x409}, {0x42a, 0x42a}, {0, 0} } },
	{ NULL, 0, 0, { { 0, 0 } } }
};

/*
  Print locale_id information for a given Language
*/

BOOL get_locales(const char *lang)
{
	int idx = 0;
	
	if (!lang) {
		return False;
	}
		
	while (locales[idx].locale_str != NULL) {
		if (!strcmp(lang, locales[idx].locale_str)) {
			DEBUG(0, ("locale:language = 0x%x\n", locales[idx].lcid));
			/* Fix me */
			DEBUG(0, ("locale:method = 0x%x\n", locales[idx].combination[1].lcid));
			return True;
		}
		idx++;
	}
	return False;
}

/*
  Check if the locale provided exists
*/

BOOL valid_locale(uint32_t locale)
{
	int idx = 0;

	while (locales[idx].locale_str != NULL) {
		if (locales[idx].lcid == locale)
			return True;
		idx++;
	}
	return False;
}

/*
  Print locale information for a given locale id
 */

BOOL print_locale(uint32_t locale)
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
			return True;
		}
		idx++;
	}
	return False;
}

/*
  Print language groups
 */

void print_group(void)
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

BOOL print_groupmember(uint32_t group)
{
	uint32_t idx = 0;

	if (group == -1) {
	  DEBUG(0, ("Invalid language group "));
	  return False;
	}

	DEBUG(0, ("%s:\n", language_group[group]));

	for (idx = 0; locales[idx].locale_str != NULL; idx++) {
		if (locales[idx].language_group == group) {
			printf("\t\t\t%s\n", locales[idx].locale_str);
		}
	}
	return True;
}

/*
  Convert language group from string to integer
 */

uint32_t lang2nb(const char *name)
{
	int idx = 0;

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
