/*
   OpenChange MAPI torture suite implementation.

   Regression testing related functions

   Copyright (C) Julien Kerihuel 2007
   Copyright (C) Fabien Le Mentec 2007
   
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

#ifndef __OC_TEST_H
#define __OC_TEST_H


/* how to use it:
   . @ torture command line, use --option="openchange:oc_test_out=<filename>"
   .oc_test_begin() before doing anything else
   .oc_test_assert() and oc_test_step() to log a test record
   .oc_test_skip(n) to skip n tests
   .oc_test_end() to terminate the testing
   You can then use regress.pl to check the resulting file against the ref one
 */


#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <param.h>


typedef struct
{
  int id_test;
  FILE *f_out;
  const char *desc;
  int is_enabled;
} oc_test_t;


static oc_test_t gbl_oc_test;


static void oc_test_begin(void)
{
  const char *path;

  gbl_oc_test.is_enabled = 0;
  gbl_oc_test.id_test = 0;
  gbl_oc_test.desc = 0;

  path = lp_parm_string(-1, "openchange", "oc_test_out");
  if (path == 0)
    return ;
  gbl_oc_test.f_out = fopen(path, "w");

  if (gbl_oc_test.f_out != 0)
    gbl_oc_test.is_enabled = 1;

  /* write header */
  if (gbl_oc_test.is_enabled)
    fprintf(gbl_oc_test.f_out, "<tests>\n");
}


static void oc_test_end(void)
{
  if (gbl_oc_test.is_enabled == 0)
    return ;

  /* footer + fclose */
  if (gbl_oc_test.f_out != 0)
    {
      fprintf(gbl_oc_test.f_out, "</tests>\n");
      fclose(gbl_oc_test.f_out);
    }
}


static void oc_test_describe(const char *desc)
{
  if (gbl_oc_test.is_enabled == 0)
    return ;

  gbl_oc_test.desc = desc;
}


static void oc_test_skip(int cn_ids)
{
  if (gbl_oc_test.is_enabled == 0)
    return ;
  
  gbl_oc_test.id_test += cn_ids;
}


static void oc_test_next(int is_success, const char *file, int line, const char *reason)
{
  if (gbl_oc_test.is_enabled == 0)
    return ;

  /* write test */
  fprintf(gbl_oc_test.f_out, "  <test>\n");
  fprintf(gbl_oc_test.f_out, "    <id>%d</id>\n", gbl_oc_test.id_test);
  fprintf(gbl_oc_test.f_out, "    <desc>%s</desc>\n", gbl_oc_test.desc ? gbl_oc_test.desc : "");
  fprintf(gbl_oc_test.f_out, "    <pass>%s</pass>\n", is_success ? "true" : "false");
  fprintf(gbl_oc_test.f_out, "    <file>%s</file>\n", file);
  fprintf(gbl_oc_test.f_out, "    <line>%d</line>\n", line);
  fprintf(gbl_oc_test.f_out, "    <reason>%s</reason>\n", reason);
  fprintf(gbl_oc_test.f_out, "  </test>\n");

  /* next test */
  gbl_oc_test.id_test++;
}

#define oc_test_assert( cond )				\
do {							\
  int __cond = cond;					\
  const char *__reason;					\
  if (__cond == 0) __reason = "(" #cond ") is not true";\
  else __reason = "(" #cond ") is true";		\
  oc_test_next(__cond, __FILE__, __LINE__, __reason);	\
  if (__cond == 0) return false;			\
} while (0)

#define oc_test_step()					\
do {							\
  oc_test_next(1, __FILE__, __LINE__, "always true");	\
}  while (0)


#endif /* ! __OC_TEST_H */
