/*
   Stand-alone MAPI testsuite

   OpenChange Project

   Copyright (C) Julien Kerihuel 2008

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
#include <samba/popt.h>
#include <param.h>
#include <param/proto.h>

#include <utils/openchange-tools.h>
#include <utils/mapitest/mapitest.h>

void mapitest_calls(struct mapitest *mt)
{
	mapitest_calls_ring1(mt);
	mapitest_calls_ring2(mt);
	mapitest_calls_ring3(mt);
	mapitest_calls_ring4(mt);
}
