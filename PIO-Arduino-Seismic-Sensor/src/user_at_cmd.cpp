/**
 * @file user_at_cmd.cpp
 * @author Bernd Giesecke (bernd@giesecke.tk)
 * @brief User AT commands
 * @version 0.1
 * @date 2022-09-23
 *
 * @copyright Copyright (c) 2022
 *
 */
#include "app.h"
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
using namespace Adafruit_LittleFS_Namespace;

/** Filename to save Seismic Sensor threshold setting */
static const char sensitivy_name[] = "SEISM";

/** File to save Seismic Sensor threshold setting */
File sensitivy_sett(InternalFS);

/** Flag for threshold level, default high threshold */
uint8_t threshold_level = 0;

/*****************************************
 * RTC AT commands
 *****************************************/

/**
 * @brief Set RTC time
 *
 * @param str time as string, format <year>:<month>:<date>:<hour>:<minute>
 * @return int 0 if successful, otherwise error value
 */
int at_set_rtc(char *str)
{
	uint16_t year;
	uint8_t month;
	uint8_t date;
	uint8_t hour;
	uint8_t minute;

	char *param;

	param = strtok(str, ":");

	// year:month:date:hour:minute

	if (param != NULL)
	{
		/* Check year */
		year = strtoul(param, NULL, 0);

		if (year > 3000)
		{
			return AT_ERRNO_PARA_VAL;
		}

		/* Check month */
		param = strtok(NULL, ":");
		if (param != NULL)
		{
			month = strtoul(param, NULL, 0);

			if ((month < 1) || (month > 12))
			{
				return AT_ERRNO_PARA_VAL;
			}

			// Check day
			param = strtok(NULL, ":");
			if (param != NULL)
			{
				date = strtoul(param, NULL, 0);

				if ((date < 1) || (date > 31))
				{
					return AT_ERRNO_PARA_VAL;
				}

				// Check hour
				param = strtok(NULL, ":");
				if (param != NULL)
				{
					hour = strtoul(param, NULL, 0);

					if (hour > 24)
					{
						return AT_ERRNO_PARA_VAL;
					}

					// Check minute
					param = strtok(NULL, ":");
					if (param != NULL)
					{
						minute = strtoul(param, NULL, 0);

						if (minute > 59)
						{
							return AT_ERRNO_PARA_VAL;
						}

						set_rak12002(year, month, date, hour, minute);

						return 0;
					}
				}
			}
		}
	}
	return AT_ERRNO_PARA_NUM;
}

/**
 * @brief Get RTC time
 *
 * @return int 0
 */
int at_query_rtc(void)
{
	// Get date/time from the RTC
	read_rak12002();
	AT_PRINTF("%d.%02d.%02d %d:%02d:%02d", g_date_time.year, g_date_time.month, g_date_time.date, g_date_time.hour, g_date_time.minute, g_date_time.second);
	return 0;
}

atcmd_t g_user_at_cmd_list_rtc[] = {
	/*|    CMD    |     AT+CMD?      |    AT+CMD=?    |  AT+CMD=value |  AT+CMD  | AT permission*/
	// RTC commands
	{"+RTC", "Get/Set RTC time and date", at_query_rtc, at_set_rtc, at_query_rtc, "RW"},
};

/*****************************************
 * Seismic Sensor threshold setting AT commands
 *****************************************/

/**
 * @brief Switch Seismic threshold
 *
 * @param str
 * @return int
 */
int at_set_threshold(char *str)
{
	long threshold_request = strtol(str, NULL, 0);
	if (threshold_request == 1)
	{
		threshold_level = 1;
		save_threshold_settings(threshold_level);
		threshold_rak12027(threshold_level);
	}
	else if (threshold_request == 0)
	{
		threshold_level = 0;
		save_threshold_settings(threshold_level);
		threshold_rak12027(threshold_level);
	}
	else
	{
		return AT_ERRNO_PARA_VAL;
	}
	return 0;
}

/**
 * @brief Get Seismic threshold
 *
 * @return int 0
 */
int at_query_threshold(void)
{
	// Wet calibration value query
	AT_PRINTF("threshold is %s", threshold_level == 1 ? "low" : "high");
	return 0;
}

/**
 * @brief Read saved setting for precision and packet format
 *
 */
void read_threshold_settings(void)
{
	if (InternalFS.exists(sensitivy_name))
	{
		threshold_level = 1;
		MYLOG("USR_AT", "File found, threshold set low");
	}
	else
	{
		threshold_level = 0;
		MYLOG("USR_AT", "File not found, threshold set high");
	}
	save_threshold_settings(threshold_level);
}

/**
 * @brief Save the GPS settings
 *
 */
void save_threshold_settings(uint8_t sens_level)
{
	if (threshold_level == 1)
	{
		sensitivy_sett.open(sensitivy_name, FILE_O_WRITE);
		sensitivy_sett.write("1");
		sensitivy_sett.close();
		MYLOG("USR_AT", "Created File for threshold set low");
	}
	else
	{
		InternalFS.remove(sensitivy_name);
		MYLOG("USR_AT", "Remove File for threshold set high");
	}
}

atcmd_t g_user_at_cmd_list_threshold[] = {
	/*|    CMD    |     AT+CMD?      |    AT+CMD=?    |  AT+CMD=value |  AT+CMD  | AT permission */
	// Seismic threshold commands
	{"+SENS", "Set Seismic threshold 1 = low, 0 = high", at_query_threshold, at_set_threshold, at_query_threshold, "RW"},
};

/** Number of user defined AT commands */
uint8_t g_user_at_cmd_num = 0;

/** Pointer to the combined user AT command structure */
atcmd_t *g_user_at_cmd_list;

/**
 * @brief Initialize the user defined AT command list
 *
 */
void init_user_at(void)
{
	uint16_t index_next_cmds = 0;
	uint16_t required_structure_size = 0;

	// Get required size of structure
	required_structure_size += sizeof(g_user_at_cmd_list_threshold);

	MYLOG("USR_AT", "Structure size %d threshold", required_structure_size);

	if (has_rak12002)
	{
		required_structure_size += sizeof(g_user_at_cmd_list_rtc);

		MYLOG("USR_AT", "Structure size %d RTC", required_structure_size);
	}
	// Reserve memory for the structure
	g_user_at_cmd_list = (atcmd_t *)malloc(required_structure_size);

	// Add AT commands to structure
	MYLOG("USR_AT", "Adding threshold AT commands");
	g_user_at_cmd_num += sizeof(g_user_at_cmd_list_threshold) / sizeof(atcmd_t);
	memcpy((void *)&g_user_at_cmd_list[index_next_cmds], (void *)g_user_at_cmd_list_threshold, sizeof(g_user_at_cmd_list_threshold));
	index_next_cmds += sizeof(g_user_at_cmd_list_threshold) / sizeof(atcmd_t);
	MYLOG("USR_AT", "Index after adding threshold check %d", index_next_cmds);

	if (has_rak12002)
	{
		MYLOG("USR_AT", "Adding RTC user AT commands");
		g_user_at_cmd_num += sizeof(g_user_at_cmd_list_rtc) / sizeof(atcmd_t);
		memcpy((void *)&g_user_at_cmd_list[index_next_cmds], (void *)g_user_at_cmd_list_rtc, sizeof(g_user_at_cmd_list_rtc));
		index_next_cmds += sizeof(g_user_at_cmd_list_rtc) / sizeof(atcmd_t);
		MYLOG("USR_AT", "Index after adding RTC %d", index_next_cmds);
	}
}
