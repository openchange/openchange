/*
   OpenChange OCPF (OpenChange Property File) implementation.

   Copyright (C) Julien Kerihuel 2009.

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

#ifndef	__OCPF_PRIVATE_H_
#define	__OCPF_PRIVATE_H_

#include "config.h"
#include <stdlib.h>

#ifndef HAVE_COMPARISON_FN_T
#define HAVE_COMPARISON_FN_T
typedef int (*comparison_fn_t)(const void *, const void *);
#else
# ifndef comparison_fn_t
typedef __compar_fn_t comparison_fn_t;
# endif
#endif

#endif /* ! __OCPF_PRIVATE_H_ */
