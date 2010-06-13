/*
   OpenChange OCPF (OpenChange Property File) implementation.

   Copyright (C) Julien Kerihuel 2008.

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

#ifndef __OCPF_DUMP_H_
#define	__OCPF_DUMP_H_

#define	INDENT()			\
do {					\
	uint32_t	i;		\
					\
	for (i = 0; i < indent; i++) {	\
		printf("\t");		\
	}				\
} while (0);


#define	OCPF_DUMP(x) (ocpf_do_dump x)

#define	OCPF_DUMP_TITLE(indent, txt, type)		\
do {							\
	size_t	odt_i;					\
	size_t	txt_len;				\
							\
	printf("\n");					\
	INDENT();					\
	printf("%s:\n", txt);				\
							\
	INDENT();					\
	txt_len = strlen(txt) + 1;     			\
	for (odt_i = 0; odt_i < txt_len; odt_i++) {	\
		printf("%c", type ? '-' : '=');		\
	}						\
	printf("\n");					\
} while (0);


#define	OCPF_DUMP_TOPLEVEL	0
#define	OCPF_DUMP_SUBLEVEL	1


unsigned int indent;



#endif /* ! __OCPF_DUMP_H_ */
