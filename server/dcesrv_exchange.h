/* 
   Unix SMB/CIFS implementation.

   endpoint server for the exchange_* pipes

   Copyright (C) Julien Kerihuel 2005
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef _DCESRV_EXCHANGE_H_
# define _DCESRV_EXCHANGE_H_

#include <auth.h>

#define	NTLM_AUTH_IS_OK(dce_call) \
(dce_call->conn->auth_state.session_info->server_info->authenticated == True)

#define	NTLM_AUTH_USERNAME(dce_call) \
(dce_call->conn->auth_state.session_info->credentials->username)

#define	NTLM_AUTH_SESSION_INFO(dce_call) \
(dce_call->conn->auth_state.session_info)

#define	NTLM_AUTH_IS_OK_RETURN(dce_call) do { \
	if (!NTLM_AUTH_IS_OK(dce_call)) {\
		return True;\
	}\
} while (0)

#define LAST_INT_FROM_BINARY(bin, size, pi) \
        if ((bin) != NULL) { \
                uint32_t        counter = 1; \
\
                *(pi) = 0; \
                do { \
                        *(pi) |= (bin)[(size) - counter]; \
                        *(pi) <<= (8 * (sizeof(uint32_t) - counter)); \
                } while (++counter < sizeof(uint32_t)); \
        }

#define FIRST_INT_FROM_BINARY(bin, pi) \
        if ((bin) != NULL) { \
                uint32_t        counter = 1; \
\
                *(pi) = 0; \
                do { \
                        *(pi) |= (bin)[sizeof(uint32_t) - counter]; \
                        *(pi) <<= (8 * (sizeof(uint32_t) - counter)); \
                } while (++counter < sizeof(uint32_t)); \
        }


#endif /* !_DCESRV_EXCHANGE_H_ */

