#line 1 "d:\\#Github\\Solutions\\WisBlock-Seismic-Sensor\\RUI3-Seismic-Sensor\\RAK1901_temp.cpp"
/**
 * @file RAK1901_temp.cpp
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief Initialize and read data from SHTC3 sensor
 * @version 0.2
 * @date 2022-01-30
 *
 * @copyright Copyright (c) 2022
 *
 */
#include "main.h"
#include "SparkFun_SHTC3.h"

/** Sensor instance */
SHTC3 shtc3;

/**
 * @brief Initialize the temperature and humidity sensor
 *
 * @return true if initialization is ok
 * @return false if sensor could not be found
 */
bool init_rak1901(void)
{
	Wire.setClock(100000); // Wire.setClock(400000);
	if (shtc3.begin(Wire) != SHTC3_Status_Nominal)
	{
		MYLOG("T_H", "Could not initialize SHTC3");
		return false;
	}
	return true;
}

/**
 * @brief Read the temperature and humidity values
 *     Data is added to Cayenne LPP payload as channel
 *     LPP_CHANNEL_HUMID, LPP_CHANNEL_TEMP
 *
 */
void read_rak1901(void)
{
	MYLOG("T_H", "Reading SHTC3");
	shtc3.update();

	if (shtc3.lastStatus == SHTC3_Status_Nominal)
	{
		int16_t temp_int = (int16_t)(shtc3.toDegC() * 10.0);
		uint16_t humid_int = (uint16_t)(shtc3.toPercent() * 2);

		MYLOG("T_H", "T: %.2f H: %.2f", (float)temp_int / 10.0, (float)humid_int / 2.0);

		g_solution_data.addRelativeHumidity(LPP_CHANNEL_HUMID, shtc3.toPercent());
		g_solution_data.addTemperature(LPP_CHANNEL_TEMP, shtc3.toDegC());
	}
	else
	{
		MYLOG("T_H", "Reading SHTC3 failed");
	}
}
