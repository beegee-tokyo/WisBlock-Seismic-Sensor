/**
 * @file RAK12027_seismic.cpp
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief Omron D7S Seismic Sensor
 * @version 0.1
 * @date 2022-08-27
 *
 * @copyright Copyright (c) 2022
 *
 */
#include "main.h"

#include <Wire.h>
#include <RAK12027_D7S.h> // Click here to get the library: http://librarymanager/All#RAK12027_D7S

RAK_D7S D7S;

#define RAK12027_SLOT 'D'

//******************************************************************//
// RAK12027 INT1_PIN
//******************************************************************//
// Slot A      WB_IO1
// Slot B      WB_IO2 ( not recommended, pin conflict with IO2)
// Slot C      WB_IO3
// Slot D      WB_IO5
// Slot E      WB_IO4
// Slot F      WB_IO6
//******************************************************************//
//******************************************************************//
// RAK12027 INT2_PIN
//******************************************************************//
// Slot A      WB_IO2 ( not recommended, pin conflict with IO2)
// Slot B      WB_IO1
// Slot C      WB_IO4
// Slot D      WB_IO6
// Slot E      WB_IO3
// Slot F      WB_IO5
//******************************************************************//

/** Interrupt pin, depends on slot */
// Slot A
#if RAK12027_SLOT == 'A'
#pragma message "Slot A"
#define INT1_PIN WB_IO1
#define INT2_PIN WB_IO2
// Slot B
#elif RAK12027_SLOT == 'B'
#pragma message "Slot B"
#define INT1_PIN WB_IO2
#define INT2_PIN WB_IO1
// Slot C
#elif RAK12027_SLOT == 'C'
#pragma message "Slot C"
#define INT1_PIN WB_IO3
#define INT2_PIN WB_IO4
// Slot D
#elif RAK12027_SLOT == 'D'
#pragma message "Slot D"
#define INT1_PIN WB_IO5
#define INT2_PIN WB_IO6
// Slot E
#elif RAK12027_SLOT == 'E'
#pragma message "Slot E"
#define INT1_PIN WB_IO4
#define INT2_PIN WB_IO3
// Slot F
#elif RAK12027_SLOT == 'F'
#pragma message "Slot F"
#define INT1_PIN WB_IO6
#define INT2_PIN WB_IO5
#endif

// flag variables to handle collapse/shutoff only one time during an earthquake
bool shutoff_alert = false;
bool collapse_alert = false;
bool earthquake_end = true;
bool earthquake_start = false;

uint8_t g_threshold = 0;
float savedSI = 0.0f;
float savedPGA = 0.0f;

bool int_1_triggered = false;

void report_status(void)
{
	return;

	uint8_t current_state = D7S.getState();
	char status_txt[128];
	switch (current_state)
	{
	case NORMAL_MODE:
		sprintf(status_txt, "Normal");
		break;
	case NORMAL_MODE_NOT_IN_STANBY:
		sprintf(status_txt, "Not in Standby");
		break;
	case INITIAL_INSTALLATION_MODE:
		sprintf(status_txt, "Initial Installation");
		break;
	case OFFSET_ACQUISITION_MODE:
		sprintf(status_txt, "Offset Acquisition");
		break;
	case SELFTEST_MODE:
		sprintf(status_txt, "Selftest");
		break;
	default:
		sprintf(status_txt, "Undefined");
		break;
	}
	MYLOG("SEIS", "Current mode: %s", status_txt);
}

/**
 * @brief Callback for INT 1
 * Wakes up application with signal SEISMIC_ALERT
 * Activated on Collapse and Shutoff signals
 *
 */
void d7s_int1_handler(void)
{
	MYLOG("SEIS", "INT1");
	g_task_event_type = SEISMIC_ALERT;
	// api.system.timer.start(RAK_TIMER_1, 500, NULL);
	int_1_triggered = true;
	// sensor_handler(NULL);

	// api.system.timer.start(RAK_TIMER_2, 100, NULL);
}

/**
 * @brief Callback for INT 2
 * Wakes up application with signal SEISMIC_EVENT
 * Activated on Earthquake start and end
 *
 */
void d7s_int2_handler(void)
{
	MYLOG("SEIS", "INT2");
	if (digitalRead(INT2_PIN) == LOW)
	{
		digitalWrite(LED_BLUE, HIGH);
		earthquake_start = true;
		// Wake the loop to handle the interrupt
		MYLOG("SEIS", "TIM2");
		api.system.timer.start(RAK_TIMER_2, 500, NULL);
	}
	else
	{
		digitalWrite(LED_BLUE, LOW);
		// earthquake_start = false;
	}
	g_task_event_type = SEISMIC_EVENT;
	// sensor_handler(NULL);
}

void check_alarm(void *)
{
	switch (check_event_rak12027(true))
	{
	case 1:
		// Collapse alert
		digitalWrite(LED_GREEN, HIGH);
		collapse_alert = true;
		MYLOG("SEIS", "Earthquake collapse alert!");
		break;
	case 2:
		// ShutDown alert
		shutoff_alert = true;
		MYLOG("SEIS", "Earthquake shutoff alert!");
		break;
	case 3:
		// Collapse & ShutDown alert
		digitalWrite(LED_GREEN, HIGH);
		collapse_alert = true;
		shutoff_alert = true;
		MYLOG("SEIS", "Earthquake collapse & shutoff alert!");
		break;
	default:
		// False alert
		digitalWrite(LED_BLUE, LOW);
		digitalWrite(LED_GREEN, LOW);
		MYLOG("SEIS", "Earthquake false alert!");
		break;
	}
}

/**
 * @brief Initialize Omron D7S seismic sensor
 *
 * @return true If sensor was found and is initialized
 * @return false If sensor initialization failed
 */
bool init_rak12027(void)
{
	Wire.begin();
	Wire.setClock(400000);
	// start D7S connection
	D7S.begin();

	// wait until the D7S is ready
	time_t start_wait_time = millis();
	while (!D7S.isReady())
	{
		if ((millis() - start_wait_time) > 10000)
		{
			MYLOG("SEIS", "Timeout waiting for D7S");
			return false;
		}
		delay(500);
	}

	//--- SETTINGS ---
	// setting the D7S to switch the axis at inizialization time
	MYLOG("SEIS", "Setting D7S sensor to switch axis at inizialization time.");
	D7S.setAxis(SWITCH_AT_INSTALLATION);

	/*********************************************************************/
	/** Calling calibration, this should be done from AT command instead */
	/*********************************************************************/
	if (!calib_rak12027())
	{
		MYLOG("SEIS", "Calibration failed with timeout");
		return false;
	}

	// Set threshold
	D7S.setThreshold((D7S_threshold_t)g_threshold);

	//--- RESETTING EVENTS ---
	// reset the events shutoff/collapse memorized into the D7S
	D7S.resetEvents();

	//--- READY TO GO ---
	MYLOG("SEIS", "Listening for earthquakes!");

	//--- Report status
	report_status();

	// //--- INTERRUPT SETTINGS ---
	// // registering event handler
	// pinMode(INT1_PIN, INPUT_PULLUP);
	// pinMode(INT2_PIN, INPUT_PULLUP);
	// attachInterrupt(INT1_PIN, d7s_int1_handler, FALLING); // Shutoff or Collapse interrupt
	// attachInterrupt(INT2_PIN, d7s_int2_handler, CHANGE);  // Earthquake start/end interrupt

	// Create a timer for earthquake alarm handling
	api.system.timer.create(RAK_TIMER_2, sensor_handler, RAK_TIMER_ONESHOT);

	return true;
}

void enable_int_rak12027(void)
{
	// reset the events shutoff/collapse memorized into the D7S
	D7S.resetEvents();
	//--- INTERRUPT SETTINGS ---
	// registering event handler
	pinMode(INT1_PIN, INPUT_PULLUP);
	pinMode(INT2_PIN, INPUT_PULLUP);
	attachInterrupt(INT1_PIN, d7s_int1_handler, FALLING); // Shutoff or Collapse interrupt
	attachInterrupt(INT2_PIN, d7s_int2_handler, CHANGE);  // Earthquake start/end interrupt
}

/**
 * @brief Calibration of D7S sensor
 * Should be called if position of sensor is changing
 *
 * @return true if calibrarion succeed
 * @return false if calibration timeout
 */
bool calib_rak12027(void)
{
	//--- INITIALIZZATION ---
	MYLOG("SEIS", "Initializing the D7S sensor in 2 seconds. Please keep it steady during the initializing process.");
	delay(2000);
	MYLOG("SEIS", "Initializing...");
	// start the initial installation procedure
	D7S.initialize();
	// wait until the D7S is ready (the initializing process is ended)
	int blue_status = digitalRead(LED_BLUE);
	int green_status = digitalRead(LED_GREEN);
	digitalWrite(LED_BLUE, HIGH);
	digitalWrite(LED_GREEN, LOW);
	time_t start_wait_time = millis();
	while (!D7S.isReady())
	{
		if ((millis() - start_wait_time) > 5000)
		{
			MYLOG("SEIS", "Timeout waiting initialization of D7S");
			digitalWrite(LED_BLUE, blue_status);
			digitalWrite(LED_GREEN, green_status);
			return false;
		}
		digitalWrite(LED_BLUE, !digitalRead(LED_BLUE));
		digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));
		delay(500);
	}
	MYLOG("SEIS", "INITIALIZED!");
	digitalWrite(LED_BLUE, blue_status);
	digitalWrite(LED_GREEN, green_status);

	return true;
}

/**
 * @brief Get events from the D7S after interrupt occured
 *
 * @param is_int1 true if it was INT1, false if it was INT2
 * @return uint8_t event code
 * 			0 no event found
 * 			1 Collapse alert
 * 			2 Shutoff alert
 * 			3 Collapse and Shutoff alert
 * 			4 Earthquake start detected
 * 			5 Earthquake end detected
 */
uint8_t check_event_rak12027(bool is_int1)
{
	MYLOG("SEIS", "Check Event");
	//--- Report status
	report_status();

	uint8_t return_val = 0;
	if (is_int1)
	{
		// Check for collapse event
		if (D7S.isInCollapse() == 1)
		{
			D7S.resetEvents();
			return_val = 1;
		}
		// Check for SI > 5
		if (D7S.isInShutoff() == 1)
		{
			D7S.resetEvents();
			return_val = return_val + 2;
		}
	}
	else
	{
		// Check if earthquake started or ended
		if (D7S.isEarthquakeOccuring())
		{
			earthquake_start = true;
			return_val = 4;
		}
		else
		{
			D7S.resetEvents();
			if (earthquake_start)
			{
				return_val = 5;
				earthquake_start = false;
			}
		}
	}
	return return_val;
}

/**
 * @brief Read latest saved SI and PGA values
 *
 * @param add_values if true, values will be added to payload, false will just read
 */
void read_rak12027(bool add_values)
{
	// Seismic Intensity vs PGA
	// I = 2.14 log10 (PGV) + 1.89

	MYLOG("SEIS", "Read values");
	//--- Report status
	report_status();

	// get information about the current earthquake
	float currentSI = D7S.getInstantaneusSI();
	float currentPGA = D7S.getInstantaneusPGA();

	float lastSI = D7S.getLastestSI(0);
	float lastPGA = D7S.getLastestPGA(0);

	// for (int idx = 0; idx < 5; idx++)
	// {
	// 	MYLOG("SEIS", "SI level at %d %.4f", idx, D7S.getLastestSI(idx));
	// 	MYLOG("SEIS", "PGA level at %d %.4f", idx, D7S.getLastestPGA(idx));
	// }

	savedSI = lastSI;
	savedPGA = lastPGA;

	if (add_values)
	{
		g_solution_data.addPresence(LPP_CHANNEL_EQ_EVENT, true);
		g_solution_data.addAnalogInput(LPP_CHANNEL_EQ_SI, lastSI * 10.0);
		g_solution_data.addAnalogInput(LPP_CHANNEL_EQ_PGA, lastPGA * 10.0);
	}
	MYLOG("SEIS", "SI level %.4f %.4f", currentSI, lastSI);
	MYLOG("SEIS", "PGA level %.4f %.4f", currentPGA, lastPGA);
}

void set_threshold_rak12027(void)
{
	D7S.setThreshold((D7S_threshold_t)g_threshold);
}