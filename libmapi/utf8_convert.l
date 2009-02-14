/*
   OpenChange MAPI implementation.

   Copyright (C) Julien Kerihuel 2007.

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
%option nounput
%option noinput

%{
#include <stdlib.h>
#include <string.h>

int yylex(void);

/*
 * Prototypes
 */

int	yyget_lineno(void);
FILE	*yyget_in(void);
FILE	*yyget_out(void);
int	yyget_leng(void);
char	*yyget_text(void);
void	yyset_lineno(int);
void	yyset_in (FILE *);
void	yyset_out (FILE *);
int	yyget_debug(void);
void	yyset_debug(int);
int	yylex_destroy(void);

int	yyreplace_utf8(char *);
int	yyparse_utf8(char *, char *);

char *newbuf;
int  newbuf_idx;

#define	U00A1	"\xc2\xa1"	/* ¡ */
#define	U00A2	"\xc2\xa2"	/* ¢ */
#define	U00A3	"\xc2\xa3"	/* £ */
#define	U00A4	"\xc2\xa4"	/* ¤ */
#define	U00A5	"\xc2\xa5"	/* ¥ */
#define	U00A6	"\xc2\xa6"	/* ¦ */
#define	U00A7	"\xc2\xa7"	/* § */
#define	U00A8	"\xc2\xa8"	/* ¨ */
#define	U00A9	"\xc2\xa9"	/* © */
#define	U00AA	"\xc2\xaa"	/* ª */
#define	U00AB	"\xc2\xab"	/* « */
#define	U00AC	"\xc2\xac"	/* ¬ */
#define U00AD	"\xc2\xad"	/* ­ */
#define U00AE	"\xc2\xae"	/* ® */
#define U00AF	"\xc2\xaf"	/* ¯ */
#define U00B0	"\xc2\xb0"	/* ° */
#define U00B1	"\xc2\xb1"	/* ± */
#define U00B2	"\xc2\xb2"	/* ² */
#define U00B3	"\xc2\xb3"	/* ³ */
#define U00B4	"\xc2\xb4"	/* ´ */
#define U00B5	"\xc2\xb5"	/* µ */
#define U00B6	"\xc2\xb6"	/* ¶ */
#define U00B7	"\xc2\xb7"	/* · */
#define U00B8	"\xc2\xb8"	/* ¸ */
#define U00B9	"\xc2\xb9"	/* ¹ */
#define U00BA	"\xc2\xba"	/* º */
#define U00BB	"\xc2\xbb"	/* » */
#define U00BC	"\xc2\xbc"	/* ¼ */
#define U00BD	"\xc2\xbd"	/* ½ */
#define U00BE	"\xc2\xbe"	/* ¾ */
#define U00BF	"\xc2\xbf"	/* ¿ */
#define U00C0	"\xc3\x80"	/* À */
#define U00C1	"\xc3\x81"	/* Á */
#define U00C2	"\xc3\x82"	/* Â */
#define U00C3	"\xc3\x83"	/* Ã */
#define U00C4	"\xc3\x84"	/* Ä */
#define U00C5	"\xc3\x85"	/* Å */
#define U00C6	"\xc3\x86"	/* Æ */
#define U00C7	"\xc3\x87"	/* Ç */
#define U00C8	"\xc3\x88"	/* È */
#define	U00C9	"\xc3\x89"	/* É */
#define	U00CA	"\xc3\x8a"	/* Ê */
#define	U00CB	"\xc3\x8b"	/* Ë */
#define	U00CC	"\xc3\x8c"	/* Ì */
#define	U00CD	"\xc3\x8d"	/* Í */
#define	U00CE	"\xc3\x8e"	/* Î */
#define	U00CF	"\xc3\x8f"	/* Ï */
#define	U00D0	"\xc3\x90"	/* Ð */
#define	U00D1	"\xc3\x91"	/* Ñ */
#define	U00D2	"\xc3\x92"	/* Ò */
#define	U00D3	"\xc3\x93"	/* Ó */
#define	U00D4	"\xc3\x94"	/* Ô */
#define	U00D5	"\xc3\x95"	/* Õ */
#define	U00D6	"\xc3\x96"	/* Ö */
#define	U00D7	"\xc3\x97"	/* × */
#define	U00D8	"\xc3\x98"	/* Ø */
#define	U00D9	"\xc3\x99"	/* Ù */
#define	U00DA	"\xc3\x9a"	/* Ú */
#define	U00DB	"\xc3\x9b"	/* Û */
#define	U00DC	"\xc3\x9c"	/* Ü */
#define	U00DD	"\xc3\x9d"	/* Ý */
#define	U00DE	"\xc3\x9e"	/* Þ */
#define	U00DF	"\xc3\x9f"	/* ß */
#define	U00E0	"\xc3\xa0"	/* à */
#define	U00E1	"\xc3\xa1"	/* á */
#define	U00E2	"\xc3\xa2"	/* â */
#define	U00E3	"\xc3\xa3"	/* ã */
#define	U00E4	"\xc3\xa4"	/* ä */
#define	U00E5	"\xc3\xa5"	/* å */
#define	U00E6	"\xc3\xa6"	/* æ */
#define	U00E7	"\xc3\xa7"	/* ç */
#define	U00E8	"\xc3\xa8"	/* è */
#define	U00E9	"\xc3\xa9"	/* é */
#define	U00EA	"\xc3\xaa"	/* ê */
#define	U00EB	"\xc3\xab"	/* ë */
#define	U00EC	"\xc3\xac"	/* ì */
#define	U00ED	"\xc3\xad"	/* í */
#define	U00EE	"\xc3\xae"	/* î */
#define	U00EF	"\xc3\xaf"	/* ï */
#define	U00F0	"\xc3\xb0"	/* ð */
#define	U00F1	"\xc3\xb1"	/* ñ */
#define	U00F2	"\xc3\xb2"	/* ò */
#define	U00F3	"\xc3\xb3"	/* ó */
#define	U00F4	"\xc3\xb4"	/* ô */
#define	U00F5	"\xc3\xb5"	/* õ */
#define	U00F6	"\xc3\xb6"	/* ö */
#define	U00F7	"\xc3\xb7"	/* ÷ */
#define	U00F8	"\xc3\xb8"	/* ø */
#define	U00F9	"\xc3\xb9"	/* ù */
#define	U00FA	"\xc3\xba"	/* ú */
#define	U00FB	"\xc3\xbb"	/* û */
#define	U00FC	"\xc3\xbc"	/* ü */
#define	U00FD	"\xc3\xbd"	/* ý */
#define	U00FE	"\xc3\xbe"	/* þ */
#define	U00FF	"\xc3\xbf"	/* ÿ */

%}

chars [0-9A-za-z\_\'\.\"/\+\-=\{\}:]
numbers ([0-9])+
delim [" "\n\t\s]
whitespace {delim}+
words {chars}+

WIN_U00A1	"\xc3\xad"
WIN_U00A2	"\xc3\xb3"
WIN_U00A3	"\xc3\xba"
WIN_U00A4	"\xc3\xb1"
WIN_U00A5	"\xc3\x91"
WIN_U00A6	"\xc2\xaa"
WIN_U00A7	"\xc2\xba"
WIN_U00A8	"\xc2\xbf"
WIN_U00A9	"\xc2\xae"
WIN_U00AA	"\xc2\xac"
WIN_U00AB	"\xc2\xbd"
WIN_U00AC	"\xc2\xbc"
WIN_U00AD	"\xc2\xa1"
WIN_U00AE	"\xc2\xab"
WIN_U00AF	"\xc2\xbb"
WIN_U00B0	"\xe2\x96\x91"
WIN_U00B1	"\xe2\x96\x92"
WIN_U00B2	"\xe2\x96\x93"
WIN_U00B3	"\xe2\x94\x82"
WIN_U00B4	"\xe2\x94\xa4"
WIN_U00B5	"\xc3\x81"
WIN_U00B6	"\xc3\x82"
WIN_U00B7	"\xc3\x80"
WIN_U00B8	"\xc2\xa9"
WIN_U00B9	"\xe2\x95\xa3"
WIN_U00BA	"\xe2\x95\x91"
WIN_U00BB	"\xe2\x95\x97"
WIN_U00BC	"\xe2\x95\x9d"
WIN_U00BD	"\xc2\xa2"
WIN_U00BE	"\xc2\xa5"
WIN_U00BF	"\xe2\x94\x90"
WIN_U00C0	"\xe2\x94\x94"
WIN_U00C1	"\xe2\x94\xb4"
WIN_U00C2	"\xe2\x94\xac"
WIN_U00C3	"\xe2\x94\x9c"
WIN_U00C4	"\xe2\x94\x80"
WIN_U00C5	"\xe2\x94\xbc"
WIN_U00C6	"\xc3\xa3"
WIN_U00C7	"\xc3\x83"
WIN_U00C8	"\xe2\x95\x9a"
WIN_U00C9	"\xe2\x95\x94"
WIN_U00CA	"\xe2\x95\xa9"
WIN_U00CB	"\xe2\x95\xa6"
WIN_U00CC	"\xe2\x95\xa0"
WIN_U00CD	"\xe2\x95\x90"
WIN_U00CE	"\xe2\x95\xac"
WIN_U00CF	"\xc2\xa4"
WIN_U00D0	"\xc3\xb0"
WIN_U00D1	"\xc3\x90"
WIN_U00D2	"\xc3\x8a"
WIN_U00D3	"\xc3\x8b"
WIN_U00D4	"\xc3\x88"
WIN_U00D5	"\xc4\xb1"
WIN_U00D6	"\xc3\x8d"
WIN_U00D7	"\xc3\x8e"
WIN_U00D8	"\xc3\x8f"
WIN_U00D9	"\xe2\x94\x98"
WIN_U00DA	"\xe2\x94\x8c"
WIN_U00DB	"\xe2\x96\x88"
WIN_U00DC	"\xe2\x96\x84"
WIN_U00DD	"\xc2\xa6"
WIN_U00DE	"\xc3\x8c"
WIN_U00DF	"\xe2\x96\x80"
WIN_U00E0	"\xc3\x93"
WIN_U00E1	"\xc3\x9f"
WIN_U00E2	"\xc3\x94"
WIN_U00E3	"\xc3\x92"
WIN_U00E4	"\xc3\xb5"
WIN_U00E5	"\xc3\x95"
WIN_U00E6	"\xc2\xb5"
WIN_U00E7	"\xc3\xbe"
WIN_U00E8	"\xc3\x9e"
WIN_U00E9	"\xc3\x9a"
WIN_U00EA	"\xc3\x9b"
WIN_U00EB	"\xc3\x99"
WIN_U00EC	"\xc3\xbd"
WIN_U00ED	"\xc3\x9d"
WIN_U00EE	"\xc2\xaf"
WIN_U00EF	"\xc2\xb4"
WIN_U00F0	"\xc2\xad"
WIN_U00F1	"\xc2\xb1"
WIN_U00F2	"\xe2\x80\x97"
WIN_U00F3	"\xc2\xbe"
WIN_U00F4	"\xc2\xb6"
WIN_U00F5	"\xc2\xa7"
WIN_U00F6	"\xc3\xb7"
WIN_U00F7	"\xc2\xb8"
WIN_U00F8	"\xc2\xb0"
WIN_U00F9	"\xc2\xa8"
WIN_U00FA	"\xc2\xb7"
WIN_U00FB	"\xc2\xb9"
WIN_U00FC	"\xc2\xb3"
WIN_U00FD	"\xc2\xb2"
WIN_U00FE	"\xe2\x96\xa0"
WIN_U00FF	"\xc2\xa0"


%%
{words} { yyreplace_utf8(yytext); }
{delim} { yyreplace_utf8(yytext); }
{whitespace} { yyreplace_utf8(yytext); }
{WIN_U00A1} { yyreplace_utf8(U00A1);}
{WIN_U00A2} { yyreplace_utf8(U00A2);}
{WIN_U00A3} { yyreplace_utf8(U00A3);}
{WIN_U00A4} { yyreplace_utf8(U00A4);}
{WIN_U00A5} { yyreplace_utf8(U00A5);}
{WIN_U00A6} { yyreplace_utf8(U00A6);}
{WIN_U00A7} { yyreplace_utf8(U00A7);}
{WIN_U00A8} { yyreplace_utf8(U00A8);}
{WIN_U00A9} { yyreplace_utf8(U00A9);}
{WIN_U00AA} { yyreplace_utf8(U00AA);}
{WIN_U00AB} { yyreplace_utf8(U00AB);}
{WIN_U00AC} { yyreplace_utf8(U00AC);}
{WIN_U00AD} { yyreplace_utf8(U00AD);}
{WIN_U00AE} { yyreplace_utf8(U00AE);}
{WIN_U00AF} { yyreplace_utf8(U00AF);}
{WIN_U00B0} { yyreplace_utf8(U00B0);}
{WIN_U00B1} { yyreplace_utf8(U00B1);}
{WIN_U00B2} { yyreplace_utf8(U00B2);}
{WIN_U00B3} { yyreplace_utf8(U00B3);}
{WIN_U00B4} { yyreplace_utf8(U00B4);}
{WIN_U00B5} { yyreplace_utf8(U00B5);}
{WIN_U00B6} { yyreplace_utf8(U00B6);}
{WIN_U00B7} { yyreplace_utf8(U00B7);}
{WIN_U00B8} { yyreplace_utf8(U00B8);}
{WIN_U00B9} { yyreplace_utf8(U00B9);}
{WIN_U00BA} { yyreplace_utf8(U00BA);}
{WIN_U00BB} { yyreplace_utf8(U00BB);}
{WIN_U00BC} { yyreplace_utf8(U00BC);}
{WIN_U00BD} { yyreplace_utf8(U00BD);}
{WIN_U00BE} { yyreplace_utf8(U00BE);}
{WIN_U00BF} { yyreplace_utf8(U00BF);}
{WIN_U00C0} { yyreplace_utf8(U00C0);}
{WIN_U00C1} { yyreplace_utf8(U00C1);}
{WIN_U00C2} { yyreplace_utf8(U00C2);}
{WIN_U00C3} { yyreplace_utf8(U00C3);}
{WIN_U00C4} { yyreplace_utf8(U00C4);}
{WIN_U00C5} { yyreplace_utf8(U00C5);}
{WIN_U00C6} { yyreplace_utf8(U00C6);}
{WIN_U00C7} { yyreplace_utf8(U00C7);}
{WIN_U00C8} { yyreplace_utf8(U00C8);}
{WIN_U00C9} { yyreplace_utf8(U00C9);}
{WIN_U00CA} { yyreplace_utf8(U00CA);}
{WIN_U00CB} { yyreplace_utf8(U00CB);}
{WIN_U00CC} { yyreplace_utf8(U00CC);}
{WIN_U00CD} { yyreplace_utf8(U00CD);}
{WIN_U00CE} { yyreplace_utf8(U00CE);}
{WIN_U00CF} { yyreplace_utf8(U00CF);}
{WIN_U00D0} { yyreplace_utf8(U00D0);}
{WIN_U00D1} { yyreplace_utf8(U00D1);}
{WIN_U00D2} { yyreplace_utf8(U00D2);}
{WIN_U00D3} { yyreplace_utf8(U00D3);}
{WIN_U00D4} { yyreplace_utf8(U00D4);}
{WIN_U00D5} { yyreplace_utf8(U00D5);}
{WIN_U00D6} { yyreplace_utf8(U00D6);}
{WIN_U00D7} { yyreplace_utf8(U00D7);}
{WIN_U00D8} { yyreplace_utf8(U00D8);}
{WIN_U00D9} { yyreplace_utf8(U00D9);}
{WIN_U00DA} { yyreplace_utf8(U00DA);}
{WIN_U00DB} { yyreplace_utf8(U00DB);}
{WIN_U00DC} { yyreplace_utf8(U00DC);}
{WIN_U00DD} { yyreplace_utf8(U00DD);}
{WIN_U00DE} { yyreplace_utf8(U00DE);}
{WIN_U00DF} { yyreplace_utf8(U00DF);}
{WIN_U00E0} { yyreplace_utf8(U00E0);}
{WIN_U00E1} { yyreplace_utf8(U00E1);}
{WIN_U00E2} { yyreplace_utf8(U00E2);}
{WIN_U00E3} { yyreplace_utf8(U00E3);}
{WIN_U00E4} { yyreplace_utf8(U00E4);}
{WIN_U00E5} { yyreplace_utf8(U00E5);}
{WIN_U00E6} { yyreplace_utf8(U00E6);}
{WIN_U00E7} { yyreplace_utf8(U00E7);}
{WIN_U00E8} { yyreplace_utf8(U00E8);}
{WIN_U00E9} { yyreplace_utf8(U00E9);}
{WIN_U00EA} { yyreplace_utf8(U00EA);}
{WIN_U00EB} { yyreplace_utf8(U00EB);}
{WIN_U00EC} { yyreplace_utf8(U00EC);}
{WIN_U00ED} { yyreplace_utf8(U00ED);}
{WIN_U00EE} { yyreplace_utf8(U00EE);}
{WIN_U00EF} { yyreplace_utf8(U00EF);}
{WIN_U00F0} { yyreplace_utf8(U00F0);}
{WIN_U00F1} { yyreplace_utf8(U00F1);}
{WIN_U00F2} { yyreplace_utf8(U00F2);}
{WIN_U00F3} { yyreplace_utf8(U00F3);}
{WIN_U00F4} { yyreplace_utf8(U00F4);}
{WIN_U00F5} { yyreplace_utf8(U00F5);}
{WIN_U00F6} { yyreplace_utf8(U00F6);}
{WIN_U00F7} { yyreplace_utf8(U00F7);}
{WIN_U00F8} { yyreplace_utf8(U00F8);}
{WIN_U00F9} { yyreplace_utf8(U00F9);}
{WIN_U00FA} { yyreplace_utf8(U00FA);}
{WIN_U00FB} { yyreplace_utf8(U00FB);}
{WIN_U00FC} { yyreplace_utf8(U00FC);}
{WIN_U00FD} { yyreplace_utf8(U00FD);}
{WIN_U00FE} { yyreplace_utf8(U00FE);}
{WIN_U00FF} { yyreplace_utf8(U00FF);}

%%

#ifndef yywrap
int 
yywrap(void) 
{
	return 1;
}
#endif

int
yyreplace_utf8(char *str)
{
	memcpy(&newbuf[newbuf_idx], str, strlen(str));
	newbuf_idx += strlen(str);
	return 0;
}

int
yyparse_utf8(char *mybuf, char *str)
{
	newbuf = mybuf;
	newbuf_idx = 0;
	yy_scan_string(str);
	yylex();
	newbuf[newbuf_idx] = 0;
	libmapi_utf8_convert_lex_destroy();
	return 0;
}
