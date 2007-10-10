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
#include <libmapi/proto_private.h>

#define	CP_VAL	"codepage"
#define	CP_NAME	"codepage name"
#define	CP_DESC	"codepage description"

struct codepages
{
	const char     *cp_name;
	uint32_t	cpid;
	const char     *cp_desc;
};

static struct codepages codepages[] = 
{
	{ "cpANSI", 0x0, "ANSI code page" },
	{ "cpOEM", 0x1, "OEM code page" },
	{ "cpMAC", 0x2, "Macintosh code page." },
	{ "cpebcdiccpus", 0x25, "IBM EBCDIC - U.S./Canada, IBM EBCDIC (US-Canada), Charset Label:ebcdic-cp-us" },
	{ "cpIBM437", 0x1B5, "OEM - United States, OEM United States, Charset Label:IBM437, Aliases:437, cp437, csPC8, CodePage437" },
	{ "cpIBMEBCDICInternational", 0x1F4, "IBM EBCDIC - International" },
	{ "cpASMO708", 0x2C4, "Arabic - ASMO 708, Arabic (ASMO 708), Charset Label:ASMO-708" },
	{ "cpArabicASMO449BCONV4", 0x2C5, "Arabic - ASMO 449+, BCON V4" },
	{ "cpArabicTransparentArabic", 0x2C6, "Arabic - Transparent Arabic" },
	{ "cpDOS720", 0x2D0, "Arabic - Transparent ASMO, Arabic (DOS), Charset Label:DOS-720" },
	{ "cpibm737", 0x2E1, "OEM - Greek (formerly 437G), Greek (DOS), Charset Label:ibm737" },
	{ "cpibm775", 0x307, "OEM - Baltic, Baltic (DOS), Charset Label:ibm775, Aliases:CP500" },
	{ "cpibm850", 0x352, "OEM - Multilingual Latin I, Western European (DOS), Charset Label:ibm850" },
	{ "cpibm852", 0x354, "OEM - Latin II, Central European (DOS), Charset Label:ibm852, Aliases:cp852" },
	{ "cpOEMCyrillicprimarilyRussian", 0x357, "OEM - Cyrillic (primarily Russian)" },
	{ "cpibm857", 0x359, "OEM - Turkish, Turkish (DOS), Charset Label:ibm857" },
	{ "cpOEMMultlingualLatinIEurosymbol", 0x35A, "OEM - Multlingual Latin I + Euro symbol" },
	{ "cpOEMPortuguese", 0x35C, "OEM - Portuguese" },
	{ "cpibm861", 0x35D, "OEM - Icelandic, Icelandic (DOS), Charset Label:ibm861" },
	{ "cpDOS862", 0x35E, "OEM - Hebrew, Hebrew (DOS), Charset Label:DOS-862" },
	{ "cpOEMCanadianFrench", 0x35F, "OEM - Canadian-French, " },
	{ "cpOEMArabic", 0x360, "OEM - Arabic, " },
	{ "cpOEMNordic", 0x361, "OEM - Nordic, " },
	{ "cpcp866", 0x362, "OEM - Russian, Cyrillic (DOS), Charset Label:cp866, Aliases:ibm866" },
	{ "cpibm869", 0x365, "OEM - Modern Greek, Greek, Modern (DOS), Charset Label:ibm869" },
	{ "cpCP870", 0x366, "IBM EBCDIC - Multilingual/ROECE (Latin-2), IBM EBCDIC (Multilingual Latin-2), Charset Label:CP870" },
	{ "cpwindows874", 0x36A, "ANSI/OEM - Thai (same as 28605, ISO 8859-15), Thai (Windows), Charset Label:windows-874, Aliases:DOS-874, iso-8859-11, TIS-620" },
	{ "cpxEBCDICGreekModern", 0x36B, "IBM EBCDIC - Modern Greek, IBM EBCDIC (Greek Modern), Charset Label:x-EBCDIC-GreekModern" },
	{ "cpshiftjis", 0x3A4, "ANSI/OEM - Japanese, Shift-JIS, Japanese (Shift-JIS), Charset Label:shift_jis, Aliases:csShiftJIS, csWindows31J, ms_Kanji, shift-jis, x-ms-cp932, x-sjis" },
	{ "cpgb2312", 0x3A8, "ANSI/OEM - Simplified Chinese (PRC, Singapore), Chinese Simplified (GB2312), Charset Label:gb2312, Aliases:chinese, CN-GB, csGB2312, csGB231280, csISO58GB231280, GB_2312-80, GB231280, GB2312-80, GBK, iso-ir-58" },
	{ "cpksc56011987", 0x3B5, "ANSI/OEM - Korean (Unified Hangeul Code), Korean, Charset Label:ks_c_5601-1987, Aliases:csKSC56011987, euc-kr, iso-ir-149, korean, ks_c_5601, ks_c_5601_1987, ks_c_5601-1989, KSC_5601, KSC5601" },
	{ "cpbig5", 0x3B6, "ANSI/OEM - Traditional Chinese (Taiwan; Hong Kong SAR, PRC), Chinese Traditional (Big5), Charset Label:big5, Aliases:cn-big5, csbig5, x-x-big5" },
	{ "cpCP1026", 0x402, "IBM EBCDIC - Turkish (Latin-5), IBM EBCDIC (Turkish Latin-5), Charset Label:CP1026" },
	{ "cpIBMEBCDICLatin1OpenSystem", 0x417, "IBM EBCDIC - Latin 1/Open System, " },
	{ "cpxebcdiccpuseuro", 0x474, "IBM EBCDIC - U.S./Canada (037 + Euro symbol), IBM EBCDIC (US-Canada-Euro), Charset Label:x-ebcdic-cp-us-euro" },
	{ "cpxebcdicgermanyeuro", 0x475, "IBM EBCDIC - Germany (20273 + Euro symbol), IBM EBCDIC (Germany-Euro), Charset Label:x-ebcdic-germany-euro" },
	{ "cpxebcdicdenmarknorwayeuro", 0x476, "IBM EBCDIC - Denmark/Norway (20277 + Euro symbol), IBM EBCDIC (Denmark-Norway-Euro), Charset Label:x-ebcdic-denmarknorway-euro" },
	{ "cpxebcdicfinlandswedeneuro", 0x477, "IBM EBCDIC - Finland/Sweden (20278 + Euro symbol), IBM EBCDIC (Finland-Sweden-Euro), Charset Label:x-ebcdic-finlandsweden-euro, Aliases:X-EBCDIC-France" },
	{ "cpxebcdicitalyeuro", 0x478, "IBM EBCDIC - Italy (20280 + Euro symbol), IBM EBCDIC (Italy-Euro), Charset Label:x-ebcdic-italy-euro" },
	{ "cpxebcdicspaineuro", 0x479, "IBM EBCDIC - Latin America/Spain (20284 + Euro symbol), IBM EBCDIC (Spain-Euro), Charset Label:x-ebcdic-spain-euro" },
	{ "cpxebcdicukeuro", 0x47A, "IBM EBCDIC - United Kingdom (20285 + Euro symbol), IBM EBCDIC (UK-Euro), Charset Label:x-ebcdic-uk-euro" },
	{ "cpxebcdicfranceeuro", 0x47B, "IBM EBCDIC - France (20297 + Euro symbol), IBM EBCDIC (France-Euro), Charset Label:x-ebcdic-france-euro" },
	{ "cpxebcdicinternationaleuro", 0x47C, "IBM EBCDIC - International (500 + Euro symbol), IBM EBCDIC (International-Euro), Charset Label:x-ebcdic-international-euro" },
	{ "cpxebcdicicelandiceuro", 0x47D, "IBM EBCDIC - Icelandic (20871 + Euro symbol), IBM EBCDIC (Icelandic-Euro), Charset Label:x-ebcdic-icelandic-euro" },
	{ "cpunicode", 0x4B0, "Unicode UCS-2 Little-Endian (BMP of ISO 10646), Unicode, Charset Label:unicode, Aliases:utf-16" },
	{ "cpunicodeFFFE", 0x4B1, "Unicode UCS-2 Big-Endian, Unicode (Big-Endian), Charset Label:unicodeFFFE" },
	{ "cpwindows1250", 0x4E2, "ANSI - Central European, Central European (Windows), Charset Label:windows-1250, Aliases:x-cp1250" },
	{ "cpwindows1251", 0x4E3, "ANSI - Cyrillic, Cyrillic (Windows), Charset Label:windows-1251, Aliases:x-cp1251" },
	{ "cpWindows1252", 0x4E4, "ANSI - Latin I, Western European (Windows), Charset Label:Windows-1252, Aliases:ANSI_X3.4-1968, ANSI_X3.4-1986, ascii, cp367, cp819, csASCII, IBM367, ibm819, ISO_646.irv:1991, iso_8859-1, iso_8859-1:1987, ISO646-US, iso8859-1, iso-8859-1, iso-ir-100, i..." },
	{ "cpwindows1253", 0x4E5, "ANSI - Greek, Greek (Windows), Charset Label:windows-1253" },
	{ "cpwindows1254", 0x4E6, "ANSI - Turkish, Turkish (Windows), Charset Label:windows-1254, Aliases:ISO_8859-9, ISO_8859-9:1989, iso-8859-9, iso-ir-148, latin5" },
	{ "cpwindows1255", 0x4E7, "ANSI - Hebrew, Hebrew (Windows), Charset Label:windows-1255, Aliases:ISO_8859-8-I, ISO-8859-8, visual" },
	{ "cpwindows1256", 0x4E8, "ANSI - Arabic, Arabic (Windows), Charset Label:windows-1256, Aliases:cp1256" },
	{ "cpwindows1257", 0x4E9, "ANSI - Baltic, Baltic (Windows), Charset Label:windows-1257" },
	{ "cpwindows1258", 0x4EA, "ANSI/OEM - Vietnamese, Vietnamese (Windows), Charset Label:windows-1258" },
	{ "cpJohab", 0x551, "Korean (Johab), Korean (Johab), Charset Label:Johab" },
	{ "cpmacintosh", 0x2710, "MAC - Roman, Western European (Mac), Charset Label:macintosh" },
	{ "cpxmacjapanese", 0x2711, "MAC - Japanese, Japanese (Mac), Charset Label:x-mac-japanese" },
	{ "cpxmacchinesetrad", 0x2712, "MAC - Traditional Chinese (Big5), Chinese Traditional (Mac), Charset Label:x-mac-chinesetrad" },
	{ "cpxmackorean", 0x2713, "MAC - Korean, Korean (Mac), Charset Label:x-mac-korean" },
	{ "cpxmacarabic", 0x2714, "MAC - Arabic, Arabic (Mac), Charset Label:x-mac-arabic" },
	{ "cpxmachebrew", 0x2715, "MAC - Hebrew, Hebrew (Mac), Charset Label:x-mac-hebrew" },
	{ "cpxmacgreek", 0x2716, "MAC - Greek I, Greek (Mac), Charset Label:x-mac-greek" },
	{ "cpxmaccyrillic", 0x2717, "MAC - Cyrillic, Cyrillic (Mac), Charset Label:x-mac-cyrillic" },
	{ "cpxmacchinesesimp", 0x2718, "MAC - Simplified Chinese (GB 2312), Chinese Simplified (Mac), Charset Label:x-mac-chinesesimp" },
	{ "cpMACRomania", 0x271A, "MAC - Romania, " },
	{ "cpMACUkraine", 0x2721, "MAC - Ukraine, " },
	{ "cpMACThai", 0x2725, "MAC - Thai, " },
	{ "cpxmacce", 0x272D, "MAC - Latin II, Central European (Mac), Charset Label:x-mac-ce" },
	{ "cpxmacicelandic", 0x275F, "MAC - Icelandic, Icelandic (Mac), Charset Label:x-mac-icelandic" },
	{ "cpxmacturkish", 0x2761, "MAC - Turkish, Turkish (Mac), Charset Label:x-mac-turkish" },
	{ "cpMACCroatia", 0x2762, "MAC - Croatia, " },
	{ "cpUnicodeUCS4LittleEndian", 0x2EE0, "Unicode UCS-4 Little-Endian, " },
	{ "cpUnicodeUCS4BigEndian", 0x2EE1, "Unicode UCS-4 Big-Endian, " },
	{ "cpxChineseCNS", 0x4E20, "CNS - Taiwan, Chinese Traditional (CNS), Charset Label:x-Chinese-CNS" },
	{ "cpTCATaiwan", 0x4E21, "TCA - Taiwan, " },
	{ "cpxChineseEten", 0x4E22, "Eten - Taiwan, Chinese Traditional (Eten), Charset Label:x-Chinese-Eten" },
	{ "cpIBM5550Taiwan", 0x4E23, "IBM5550 - Taiwan, " },
	{ "cpTeleTextTaiwan", 0x4E24, "TeleText - Taiwan, " },
	{ "cpWangTaiwan", 0x4E25, "Wang - Taiwan, " },
	{ "cpxIA5", 0x4E89, "IA5 IRV International Alphabet No. 5 (7-bit), Western European (IA5), Charset Label:x-IA5" },
	{ "cpxIA5German", 0x4E8A, "IA5 German (7-bit), German (IA5), Charset Label:x-IA5-German" },
	{ "cpxIA5Swedish", 0x4E8B, "IA5 Swedish (7-bit), Swedish (IA5), Charset Label:x-IA5-Swedish" },
	{ "cpxIA5Norwegian", 0x4E8C, "IA5 Norwegian (7-bit), Norwegian (IA5), Charset Label:x-IA5-Norwegian" },
	{ "cpusascii", 0x4E9F, "US-ASCII (7-bit), US-ASCII, Charset Label:us-ascii, Aliases:ANSI_X3.4-1968, ANSI_X3.4-1986, ascii, cp367, csASCII, IBM367, ISO_646.irv:1991, ISO646-US, iso-ir-6us" },
	{ "cpT61", 0x4F25, "T.61, " },
	{ "cpISO6937NonSpacingAccent", 0x4F2D, "ISO 6937 Non-Spacing Accent, " },
	{ "cpxEBCDICGermany", 0x4F31, "IBM EBCDIC - Germany, IBM EBCDIC (Germany), Charset Label:x-EBCDIC-Germany" },
	{ "cpxEBCDICDenmarkNorway", 0x4F35, "IBM EBCDIC - Denmark/Norway, IBM EBCDIC (Denmark-Norway), Charset Label:x-EBCDIC-DenmarkNorway" },
	{ "cpxEBCDICFinlandSweden", 0x4F36, "IBM EBCDIC - Finland/Sweden, IBM EBCDIC (Finland-Sweden), Charset Label:x-EBCDIC-FinlandSweden" },
	{ "cpxEBCDICItaly", 0x4F38, "IBM EBCDIC - Italy, IBM EBCDIC (Italy), Charset Label:x-EBCDIC-Italy" },
	{ "cpXEBCDICSpain", 0x4F3C, "IBM EBCDIC - Latin America/Spain, IBM EBCDIC (Spain), Charset Label:X-EBCDIC-Spain" },
	{ "cpxEBCDICUK", 0x4F3D, "IBM EBCDIC - United Kingdom, IBM EBCDIC (UK), Charset Label:x-EBCDIC-UK" },
	{ "cpxEBCDICJapaneseKatakana", 0x4F42, "IBM EBCDIC - Japanese Katakana Extended, IBM EBCDIC (Japanese katakana), Charset Label:x-EBCDIC-JapaneseKatakana" },
	{ "cpIBMEBCDICFrance", 0x4F49, "IBM EBCDIC - France, " },
	{ "cpxEBCDICArabic", 0x4FC4, "IBM EBCDIC - Arabic, IBM EBCDIC (Arabic), Charset Label:x-EBCDIC-Arabic" },
	{ "cpxEBCDICGreek", 0x4FC7, "IBM EBCDIC - Greek, IBM EBCDIC (Greek), Charset Label:x-EBCDIC-Greek" },
	{ "cpxEBCDICHebrew", 0x4FC8, "IBM EBCDIC - Hebrew, IBM EBCDIC (Hebrew), Charset Label:x-EBCDIC-Hebrew" },
	{ "cpxEBCDICKoreanExtended", 0x5161, "IBM EBCDIC - Korean Extended, IBM EBCDIC (Korean Extended), Charset Label:x-EBCDIC-KoreanExtended" },
	{ "cpxEBCDICThai", 0x5166, "IBM EBCDIC - Thai, IBM EBCDIC (Thai), Charset Label:x-EBCDIC-Thai" },
	{ "cpkoi8r", 0x5182, "Russian - KOI8-R, Cyrillic (KOI8-R), Charset Label:koi8-r, Aliases:csKOI8R, koi, koi8, koi8r" },
	{ "cpxEBCDICIcelandic", 0x5187, "IBM EBCDIC - Icelandic, IBM EBCDIC (Icelandic), Charset Label:x-EBCDIC-Icelandic" },
	{ "cpxEBCDICCyrillicRussian", 0x5190, "IBM EBCDIC - Cyrillic (Russian), IBM EBCDIC (Cyrillic Russian), Charset Label:x-EBCDIC-CyrillicRussian" },
	{ "cpxEBCDICTurkish", 0x51A9, "IBM EBCDIC - Turkish, IBM EBCDIC (Turkish), Charset Label:x-EBCDIC-Turkish" },
	{ "cpIBMEBCDICLatin1OpenSystem1047Eurosymbol", 0x51BC, "IBM EBCDIC - Latin-1/Open System (1047 + Euro symbol), " },
	{ "cpJISX0208199001211990", 0x51C4, "JIS X 0208-1990 & 0121-1990, " },
	{ "cpSimplifiedChineseGB2312", 0x51C8, "Simplified Chinese (GB2312), " },
	{ "cpxEBCDICCyrillicSerbianBulgarian", 0x5221, "IBM EBCDIC - Cyrillic (Serbian, Bulgarian), IBM EBCDIC (Cyrillic Serbian-Bulgarian), Charset Label:x-EBCDIC-CyrillicSerbianBulgarian" },
	{ "cpExtendedAlphaLowercase", 0x5223, "Extended Alpha Lowercase, " },
	{ "cpkoi8u", 0x556A, "Ukrainian (KOI8-U), Cyrillic (KOI8-U), Charset Label:koi8-u, Aliases:koi8-ru" },
	{ "cpiso88591", 0x6FAF, "ISO 8859-1 Latin I, Western European (ISO), Charset Label:iso-8859-1, Aliases:cp819, csISO, Latin1, ibm819, iso_8859-1, iso_8859-1:1987, iso8859-1, iso-ir-100, l1, latin1" },
	{ "cpiso88592", 0x6FB0, "ISO 8859-2 Central Europe, Central European (ISO), Charset Label:iso-8859-2, Aliases:csISOLatin2, iso_8859-2, iso_8859-2:1987, iso8859-2, iso-ir-101, l2, latin2" },
	{ "cpiso88593", 0x6FB1, "ISO 8859-3 Latin 3, Latin 3 (ISO), Charset Label:iso-8859-3, Aliases:csISO, Latin3, ISO_8859-3, ISO_8859-3:1988, iso-ir-109, l3, latin3" },
	{ "cpiso88594", 0x6FB2, "ISO 8859-4 Baltic, Baltic (ISO), Charset Label:iso-8859-4, Aliases:csISOLatin4, ISO_8859-4, ISO_8859-4:1988, iso-ir-110, l4, latin4" },
	{ "cpiso88595", 0x6FB3, "ISO 8859-5 Cyrillic, Cyrillic (ISO), Charset Label:iso-8859-5, Aliases:csISOLatin5, csISOLatinCyrillic, cyrillic, ISO_8859-5, ISO_8859-5:1988, iso-ir-144, l5" },
	{ "cpiso88596", 0x6FB4, "ISO 8859-6 Arabic, Arabic (ISO), Charset Label:iso-8859-6, Aliases:arabic, csISOLatinArabic, ECMA-114, ISO_8859-6, ISO_8859-6:1987, iso-ir-127" },
	{ "cpiso88597", 0x6FB5, "ISO 8859-7 Greek, Greek (ISO), Charset Label:iso-8859-7, Aliases:csISOLatinGreek, ECMA-118, ELOT_928, greek, greek8, ISO_8859-7, ISO_8859-7:1987, iso-ir-126" },
	{ "cpiso88598", 0x6FB6, "ISO 8859-8 Hebrew, Hebrew (ISO-Visual), Charset Label:iso-8859-8, Aliases:csISOLatinHebrew, hebrew, ISO_8859-8, ISO_8859-8:1988, ISO-8859-8, iso-ir-138, visual" },
	{ "cpiso88599", 0x6FB7, "ISO 8859-9 Latin 5, Turkish (ISO), Charset Label:iso-8859-9, Aliases:csISO, Latin5, ISO_8859-9, ISO_8859-9:1989, iso-ir-148, l5, latin5" },
	{ "cpiso885915", 0x6FBD, "ISO 8859-15 Latin 9, Latin 9 (ISO), Charset Label:iso-8859-15, Aliases:csISO, Latin9, ISO_8859-15, l9, latin9" },
	{ "cpxEuropa", 0x7149, "Europa 3, Europa, Charset Label:x-Europa" },
	{ "cpiso88598i", 0x96C6, "ISO 8859-8 Hebrew, Hebrew (ISO-Logical), Charset Label:iso-8859-8-i, Aliases:logical" },
	{ "cpiso2022jp", 0xC42C, "ISO 2022 Japanese with no halfwidth Katakana, Japanese (JIS), Charset Label:iso-2022-jp" },
	{ "cpcsISO2022JP", 0xC42D, "ISO 2022 Japanese with halfwidth Katakana, Japanese (JIS-Allow 1 byte Kana), Charset Label:csISO2022JP, Aliases:_iso-2022-jp" },
	{ "cpiso2022jp1", 0xC42E, "ISO 2022 Japanese JIS X 0201-1989, Japanese (JIS-Allow 1 byte Kana - SO/SI), Charset Label:iso-2022-jp-1, Aliases:_iso-2022-jp$SIO" },
	{ "cpiso2022kr", 0xC431, "ISO 2022 Korean, Korean (ISO), Charset Label:iso-2022-kr, Aliases:csISO2022KR" },
	{ "cpISO2022SimplifiedChinese", 0xC433, "ISO 2022 Simplified Chinese, " },
	{ "cpISO2022TraditionalChinese", 0xC435, "ISO 2022 Traditional Chinese, " },
	{ "cpxEBCDICJapaneseAndKana", 0xC6F2, "Japanese (Katakana) Extended, IBM EBCDIC (Japanese and Japanese Katakana), Charset Label:x-EBCDIC-JapaneseAndKana" },
	{ "cpxEBCDICJapaneseAndUSCanada", 0xC6F3, "US/Canada and Japanese, IBM EBCDIC (Japanese and US-Canada), Charset Label:x-EBCDIC-JapaneseAndUSCanada" },
	{ "cpxEBCDICKoreanAndKoreanExtended", 0xC6F5, "Korean Extended and Korean, IBM EBCDIC (Korean and Korean Extended), Charset Label:x-EBCDIC-KoreanAndKoreanExtended" },
	{ "cpxEBCDICSimplifiedChinese", 0xC6F7, "Simplified Chinese Extended and Simplified Chinese, IBM EBCDIC (Simplified Chinese), Charset Label:x-EBCDIC-SimplifiedChinese" },
	{ "cpSimplifiedChinese", 0xC6F8, "Simplified Chinese, " },
	{ "cpxEBCDICTraditionalChinese", 0xC6F9, "US/Canada and Traditional Chinese, IBM EBCDIC (Traditional Chinese), Charset Label:x-EBCDIC-TraditionalChinese" },
	{ "cpxEBCDICJapaneseAndJapaneseLatin", 0xC6FB, "Japanese (Latin) Extended and Japanese, IBM EBCDIC (Japanese and Japanese-Latin), Charset Label:x-EBCDIC-JapaneseAndJapaneseLatin" },
	{ "cpeucjp", 0xCADC, "EUC - Japanese, Japanese (EUC), Charset Label:euc-jp, Aliases:csEUCPkdFmtJapanese, Extended_UNIX_Code_Packed_Format_for_Japanese, x-euc, x-euc-jp" },
	{ "cpEUCCN", 0xCAE0, "EUC - Simplified Chinese, Chinese Simplified (EUC), Charset Label:EUC-CN, Aliases:x-euc-cn" },
	{ "cpeuckr", 0xCAED, "EUC - Korean, Korean (EUC), Charset Label:euc-kr, Aliases:csEUCKR" },
	{ "cpEUCTraditionalChinese", 0xCAEE, "EUC - Traditional Chinese, " },
	{ "cphzgb2312", 0xCEC8, "HZ-GB2312 Simplified Chinese, Chinese Simplified (HZ), Charset Label:hz-gb-2312" },
	{ "cpWindowsXPGB18030SimplifiedChinese4Byte", 0xD698, "Windows XP: GB18030 Simplified Chinese (4 Byte), " },
	{ "cpxisciide", 0xDEAA, "ISCII Devanagari, ISCII Devanagari, Charset Label:x-iscii-de" },
	{ "cpxisciibe", 0xDEAB, "ISCII Bengali, ISCII Bengali, Charset Label:x-iscii-be" },
	{ "cpxisciita", 0xDEAC, "ISCII Tamil, ISCII Tamil, Charset Label:x-iscii-ta" },
	{ "cpxisciite", 0xDEAD, "ISCII Telugu, ISCII Telugu, Charset Label:x-iscii-te" },
	{ "cpxisciias", 0xDEAE, "ISCII Assamese, ISCII Assamese, Charset Label:x-iscii-as" },
	{ "cpxisciior", 0xDEAF, "ISCII Oriya, ISCII Oriya, Charset Label:x-iscii-or" },
	{ "cpxisciika", 0xDEB0, "ISCII Kannada, ISCII Kannada, Charset Label:x-iscii-ka" },
	{ "cpxisciima", 0xDEB1, "ISCII Malayalam, ISCII Malayalam, Charset Label:x-iscii-ma" },
	{ "cpxisciigu", 0xDEB2, "ISCII Gujarati, ISCII Gujarathi, Charset Label:x-iscii-gu" },
	{ "cpxisciipa", 0xDEB3, "ISCII Punjabi, ISCII Panjabi, Charset Label:x-iscii-pa" },
	{ "cputf7", 0xFDE8, "Unicode UTF-7, Unicode (UTF-7), Charset Label:utf-7, Aliases:csUnicode11UTF7, unicode-1-1-utf-7, x-unicode-2-0-utf-7" },
	{ "cputf8", 0xFDE9, "Unicode UTF-8, Unicode (UTF-8), Charset Label:utf-8, Aliases:unicode-1-1-utf-8, unicode-2-0-utf-8, x-unicode-2-0-utf-8" },
	{NULL, 0, NULL}
};

/*
 Check for a valid codepage
*/

_PUBLIC_ bool valid_codepage(uint32_t cpid)
{
	uint32_t idx = 0;

	while (codepages[idx].cp_name) {
		if (codepages[idx].cpid == cpid) {
			return true;
		}
		idx++;
	}
	return false;
}

bool print_codepage_infos(uint32_t cpid)
{
	uint32_t idx = 0;

	if (valid_codepage(cpid) == false) {
		return false;
	}
	
	while (codepages[idx].cp_name) {
		if (codepages[idx].cpid == cpid) {
			printf("\t%-25s: 0x%x\n", CP_VAL, cpid);
			printf("\t%-25s: %s\n", CP_NAME, codepages[idx].cp_name);
			printf("\t%-25s: %s\n", CP_DESC, codepages[idx].cp_desc);
			return true;
		}
		idx++;
	}
	return false;
}
