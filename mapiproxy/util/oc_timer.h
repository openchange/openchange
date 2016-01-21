/*
   Timer functions

   OpenChange Project

   Copyright (C) Jesús García Sáez 2016

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

#ifndef __OC_TIMER_H__
#define __OC_TIMER_H__

#include <libmapi/oc_log.h>

struct oc_timer_ctx;

#define OC_TIMER_DEFAULT_THRESHOLD 1.0
#define OC_TIMER_DEFAULT_LOG_LEVEL 3

/**
  \details Create a log timer with a message that will be reported when
  oc_timer_end is called

  \param log_level The log level used when the timer will be log
  (using oc_log.h, OC_DEBUG macro)
  \param name String that will be used when the timer will be log
  (using oc_log.h, OC_DEBUG macro). It can be NULL if you are not planning
  to log the timer and just want to know the diff in seconds

  \see oc_timer_end
  \see oc_timer_end_diff
 */
struct oc_timer_ctx *oc_timer_start(
	enum oc_log_level log_level, const char *name);

/**
  \details Create a log timer with a message that will be reported when
  oc_timer_end is called if the spent time is higher than the given parameter

  \param log_level The log level used when the timer will be log
  (using oc_log.h, OC_DEBUG macro)
  \param name String that will be used when the timer will be log
  (using oc_log.h, OC_DEBUG macro). It can be NULL if you are not planning
  to log the timer and just want to know the diff in seconds
  \param threshold time in seconds, if the spent time is lower than this value
  nothing will be output when oc_timer_end is called. In case this value is
  0.0 the message will always be displayed

  \see oc_timer_end
 */
struct oc_timer_ctx *oc_timer_start_with_threshold(
	enum oc_log_level log_level, const char *name, float threshold);

/**
  \details Log the timer using OC_DEBUG macro

  \param oc_t_ctx info to log, the object will be freed
 */
void oc_timer_end(struct oc_timer_ctx *oc_t_ctx);

/**
  \details Return the seconds from the start of the timer

  \param oc_t_ctx info to log, the object will be freed
 */
float oc_timer_end_diff(struct oc_timer_ctx *oc_t_ctx);


#ifdef OC_TIMERS
#define OC_TIMER_START \
	oc_timer_start_with_threshold(OC_TIMER_DEFAULT_LOG_LEVEL, __PRETTY_FUNCTION__, OC_TIMER_DEFAULT_THRESHOLD)
#else
#define OC_TIMER_START NULL
#endif

#endif /* __OC_TIMER_H__ */
