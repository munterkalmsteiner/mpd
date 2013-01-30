/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include "ConfigGlobal.hxx"
#include "ConfigParser.hxx"
#include "ConfigData.hxx"
#include "ConfigFile.hxx"

extern "C" {
#include "utils.h"
}

#include "mpd_error.h"

#include <glib.h>

#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "config"

static ConfigData config_data;

static void
config_param_free_callback(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	struct config_param *param = (struct config_param *)data;

	config_param_free(param);
}

void config_global_finish(void)
{
	for (auto i : config_data.params) {
		g_slist_foreach(i, config_param_free_callback, NULL);
		g_slist_free(i);
	}
}

void config_global_init(void)
{
}

bool
ReadConfigFile(const Path &path, GError **error_r)
{
	return ReadConfigFile(config_data, path, error_r);
}

static void
config_param_check(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	struct config_param *param = (struct config_param *)data;

	if (!param->used)
		/* this whole config_param was not queried at all -
		   the feature might be disabled at compile time?
		   Silently ignore it here. */
		return;

	for (unsigned i = 0; i < param->num_block_params; i++) {
		struct block_param *bp = &param->block_params[i];

		if (!bp->used)
			g_warning("option '%s' on line %i was not recognized",
				  bp->name, bp->line);
	}
}

void config_global_check(void)
{
	for (auto i : config_data.params)
		g_slist_foreach(i, config_param_check, NULL);
}

const struct config_param *
config_get_next_param(ConfigOption option, const struct config_param * last)
{
	GSList *node = config_data.params[unsigned(option)];

	if (last) {
		node = g_slist_find(node, last);
		if (node == NULL)
			return NULL;

		node = g_slist_next(node);
	}

	if (node == NULL)
		return NULL;

	struct config_param *param = (struct config_param *)node->data;
	param->used = true;
	return param;
}

const char *
config_get_string(ConfigOption option, const char *default_value)
{
	const struct config_param *param = config_get_param(option);

	if (param == NULL)
		return default_value;

	return param->value;
}

char *
config_dup_path(ConfigOption option, GError **error_r)
{
	assert(error_r != NULL);
	assert(*error_r == NULL);

	const struct config_param *param = config_get_param(option);
	if (param == NULL)
		return NULL;

	char *path = parsePath(param->value, error_r);
	if (G_UNLIKELY(path == NULL))
		g_prefix_error(error_r,
			       "Invalid path at line %i: ",
			       param->line);

	return path;
}

unsigned
config_get_unsigned(ConfigOption option, unsigned default_value)
{
	const struct config_param *param = config_get_param(option);
	long value;
	char *endptr;

	if (param == NULL)
		return default_value;

	value = strtol(param->value, &endptr, 0);
	if (*endptr != 0 || value < 0)
		MPD_ERROR("Not a valid non-negative number in line %i",
			  param->line);

	return (unsigned)value;
}

unsigned
config_get_positive(ConfigOption option, unsigned default_value)
{
	const struct config_param *param = config_get_param(option);
	long value;
	char *endptr;

	if (param == NULL)
		return default_value;

	value = strtol(param->value, &endptr, 0);
	if (*endptr != 0)
		MPD_ERROR("Not a valid number in line %i", param->line);

	if (value <= 0)
		MPD_ERROR("Not a positive number in line %i", param->line);

	return (unsigned)value;
}

bool
config_get_bool(ConfigOption option, bool default_value)
{
	const struct config_param *param = config_get_param(option);
	bool success, value;

	if (param == NULL)
		return default_value;

	success = get_bool(param->value, &value);
	if (!success)
		MPD_ERROR("Expected boolean value (yes, true, 1) or "
			  "(no, false, 0) on line %i\n",
			  param->line);

	return value;
}