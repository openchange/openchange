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

#include "oc_timer.h"
#include <time.h>


struct oc_timer_ctx {
	const char *msg;
	enum oc_log_level log_level;
	struct timespec timestamp;
	float threshold;
};


static float _diff_in_seconds(struct timespec *start)
{
	struct timespec end;

	clock_gettime(CLOCK_MONOTONIC, &end);

	return ((float)((end.tv_sec * 1000000000 + end.tv_nsec) -
	                (start->tv_sec * 1000000000 + start->tv_nsec)))
	       / 1000000000;
}

struct oc_timer_ctx *oc_timer_start(enum oc_log_level log_level, const char *name)
{
#ifdef OC_TIMERS
	return oc_timer_start_with_threshold(log_level, name, 0.0);
#else
	return NULL;
#endif
}

struct oc_timer_ctx *oc_timer_start_with_threshold(
	enum oc_log_level log_level, const char *msg, float threshold)
{
	struct oc_timer_ctx *oc_t_ctx = talloc_zero(NULL, struct oc_timer_ctx);
	if (!oc_t_ctx) return NULL;

	if (msg) {
		oc_t_ctx->msg = talloc_strdup(oc_t_ctx, msg);
		if (!oc_t_ctx->msg) {
			talloc_free(oc_t_ctx);
			return NULL;
		}
	}
	oc_t_ctx->log_level = log_level;
	clock_gettime(CLOCK_MONOTONIC, &oc_t_ctx->timestamp);
	oc_t_ctx->threshold = threshold;

	return oc_t_ctx;
}

void oc_timer_end(struct oc_timer_ctx *oc_t_ctx)
{
	float diff;

	if (!oc_t_ctx) return;
	if (!oc_t_ctx->msg) goto cleanup;

	diff = _diff_in_seconds(&oc_t_ctx->timestamp);
	if (oc_t_ctx->threshold == 0.0)
		OC_DEBUG(oc_t_ctx->log_level, "[oc_timer] Spent time %s: %.3f",
		         oc_t_ctx->msg, diff);
	else if (diff > oc_t_ctx->threshold)
		OC_DEBUG(oc_t_ctx->log_level, "[oc_timer] Reached threshold %s: %.3f",
		         oc_t_ctx->msg, diff);
cleanup:
	talloc_free(oc_t_ctx);
}

float oc_timer_end_diff(struct oc_timer_ctx *oc_t_ctx)
{
	float diff = 0.0;

	if (!oc_t_ctx) return diff;

	diff = _diff_in_seconds(&oc_t_ctx->timestamp);

	talloc_free(oc_t_ctx);

	return diff;
}
