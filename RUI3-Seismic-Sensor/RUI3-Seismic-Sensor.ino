/**
 * @file RUI3-Seismic-Sensor.ino
 * @author Bernd Giesecke (bernd@giesecke.tk)
 * @brief
 * @version 0.1
 * @date 2022-09-03
 *
 * @copyright Copyright (c) 2022
 *
 */

#include "main.h"

/** Initialization results */
bool ret;

/** Event flag */
volatile uint16_t g_task_event_type = 0;

/** LoRaWAN packet */
WisCayenne g_solution_data(255);

/** Set the device name, max length is 10 characters */
char g_dev_name[64] = "RUI3 SEISMIC";

/** Fport to be used to send data */
uint8_t g_fport = 2;

/** Number of retries in case the confirmed uplink fails */
uint8_t g_repeat_send = 3;

/** Send frequency, default is off */
uint32_t g_send_repeat_time = 0;

/** Flag to enable confirmed messages */
bool confirmed_msg_enabled = true;

/** Counter for failed join attempts */
byte fail_counter = 0;

/** Flag if RAK1901 is installed */
bool has_rak1901 = false;

/**
 * @brief Callback after packet was received
 *
 * @param data Structure with the received data
 */
void receiveCallback(SERVICE_LORA_RECEIVE_T *data)
{
	MYLOG("RX-CB", "RX, fP %d, DR %d, RSSI %d, SNR %d", data->Port, data->RxDatarate, data->Rssi, data->Snr);
#if MY_DEBUG > 0
	for (int i = 0; i < data->BufferSize; i++)
	{
		Serial.printf("%02X", data->Buffer[i]);
	}
	Serial.print("\r\n");
#endif
}

/**
 * @brief Callback after TX is finished
 *
 * @param status TX status
 */
void sendCallback(int32_t status)
{
	if (status != 0)
	{
		// Reend the packet
		if (api.lorawan.send(g_solution_data.getSize(), g_solution_data.getBuffer(), g_fport, confirmed_msg_enabled, g_repeat_send))
		{
			MYLOG("APP", "Enqueued");
		}
		else
		{
			MYLOG("APP", "Send fail");
		}
		fail_counter++;
		if (fail_counter == 4)
		{
			MYLOG("TX-CB", "4 times TX fail, rejoin");
			ret = api.lorawan.join();
			fail_counter = 0;
		}
	}
	MYLOG("TX-CB", "TX %d", status);
	digitalWrite(LED_BLUE, LOW);
}

/**
 * @brief Callback after join request cycle
 *
 * @param status Join result
 */
void joinCallback(int32_t status)
{
	if (status != 0)
	{
		if (!(ret = api.lorawan.join()))
		{
			MYLOG("J-CB", "Fail! \r\n");
		}
	}
	else
	{
		// MYLOG("J-CB", "Joined\r\n");
		// We need at least DR3 for the packet size
		api.lorawan.dr.set(3);
		// MYLOG("J-CB", "DR3 %s", api.lorawan.dr.set(3) ? "OK" : "NOK");
		// Disable ADR
		api.lorawan.adr.set(0);
		digitalWrite(LED_BLUE, LOW);
		if (g_send_repeat_time != 0)
		{
			// Start a unified C timer
			api.system.timer.start(RAK_TIMER_0, g_send_repeat_time, NULL);
		}
		// Send first packet in 10 seconds
		api.system.timer.start(RAK_TIMER_1, 10000, NULL);

		MYLOG("APP", ">>>>> Set DR after join %d <<<<<", api.lorawan.dr.get());
		MYLOG("APP", "Enable D7S interrupts");
		enable_int_rak12027();
	}
}

/**
 * @brief Arduino setup, called once after reboot/power-up
 *
 */
void setup()
{
	// Setup the callbacks for joined and send finished
	api.lorawan.registerRecvCallback(receiveCallback);
	api.lorawan.registerSendCallback(sendCallback);
	api.lorawan.registerJoinCallback(joinCallback);

	if (!api.system.lpm.set(1))
	{
		MYLOG("SET", "Failed to set low power mode");
	}

	pinMode(LED_GREEN, OUTPUT);
	digitalWrite(LED_GREEN, HIGH);
	pinMode(LED_BLUE, OUTPUT);
	digitalWrite(LED_BLUE, HIGH);

	pinMode(WB_IO2, OUTPUT);
	digitalWrite(WB_IO2, LOW);

	// Use RAK_CUSTOM_MODE supresses AT command and default responses from RUI3
	// Serial.begin(115200, RAK_CUSTOM_MODE);
	// Use "normal" mode to have AT commands available
	Serial.begin(115200);

#ifdef _VARIANT_RAK4630_
	time_t serial_timeout = millis();
	// On nRF52840 the USB serial is not available immediately
	while (!Serial.available())
	{
		if ((millis() - serial_timeout) < 5000)
		{
			delay(100);
			digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));
		}
		else
		{
			break;
		}
	}
#else
	// For RAK3172 just wait a little bit for the USB to be ready
	delay(5000);
#endif

	init_custom_at();
	get_at_setting(SEND_FREQ_OFFSET);
	get_at_setting(SENSITIVITY_OFFSET);

	// Initialize Seismic module
	MYLOG("SET", "Initialize RAK12027");
	bool init_result = init_rak12027();
	MYLOG("SET", "Init %s", init_result ? "success" : "failed");

	// Initialize Temperature sensor
	MYLOG("SET", "Initialize RAK1901");
	has_rak1901 = init_rak1901();
	MYLOG("SET", "Init %s", has_rak1901 ? "success" : "failed");

	MYLOG("SET", "RAKwireless %s Node", g_dev_name);

	MYLOG("SET", "Setup your device with AT commands");

	digitalWrite(LED_GREEN, LOW);

	// Create a timer for frequent status packets
	api.system.timer.create(RAK_TIMER_0, sensor_handler, RAK_TIMER_PERIODIC);
	// Create a timer for delayed sending after join
	api.system.timer.create(RAK_TIMER_1, sensor_handler, RAK_TIMER_ONESHOT);

	// Get the confirmed mode settings
	confirmed_msg_enabled = api.lorawan.cfm.get();
	MYLOG("SET", "Confirmed message is %s", api.lorawan.cfm.get() == 0 ? "off" : "on");

	// if (!(ret = api.lorawan.join()))
	// {
	// 	MYLOG("SET", "Join request failed! \r\n");
	// }
}

/**
 * @brief sensor_handler is a timer function called every
 * g_send_repeat_time milliseconds. Default is 120000. Can be
 * changed with a custom AT command ATC+SENDFREQ
 *
 */
void sensor_handler(void *)
{
	bool no_earthquake_event = true;

	// Reset the packet
	g_solution_data.reset();

	// Check for seismic events
	if (earthquake_start)
	{
		no_earthquake_event = false;
		// Handle Seismic Events
		if (g_task_event_type == SEISMIC_EVENT)
		{
			MYLOG("APP", "Earthquake event");
			g_task_event_type = 0;
			switch (check_event_rak12027(false))
			{
			case 4:
				// Earthquake start
				MYLOG("APP", "Earthquake start alert!");
				read_rak12027(false);
				earthquake_end = false;
				g_solution_data.addPresence(LPP_CHANNEL_EQ_EVENT, true);
				// Change frequency of sensor_handler call
				api.system.timer.stop(RAK_TIMER_0);
				api.system.timer.start(RAK_TIMER_0, 500, NULL);
				break;
			case 5:
				// Earthquake end
				MYLOG("APP", "Earthquake end alert!");

				// Restart frequent sending
				MYLOG("APP", "Restart Timer 0 with %ld ms", g_send_repeat_time);
				api.system.timer.stop(RAK_TIMER_0);
				api.system.timer.start(RAK_TIMER_0, g_send_repeat_time, NULL);

				// Reset the packet
				g_solution_data.reset();

				// Get battery level
				g_solution_data.addVoltage(LPP_CHANNEL_BATT, api.system.bat.get());

				read_rak12027(true);
				g_solution_data.addPresence(LPP_CHANNEL_EQ_SHUTOFF, shutoff_alert);
				g_solution_data.addPresence(LPP_CHANNEL_EQ_COLLAPSE, collapse_alert);

				// Reset flags
				shutoff_alert = false;
				collapse_alert = false;
				earthquake_start = false;
				earthquake_end = true;
				// Send another packet in 60 seconds
				api.system.timer.start(RAK_TIMER_1, 60000, NULL);
				digitalWrite(LED_GREEN, LOW);
				break;
			default:
				// False event
				earthquake_end = true;
				MYLOG("APP", "Earthquake false event!");
				earthquake_start = false;
				return;
				break;
			}
		}

		// else if (g_task_event_type == SEISMIC_ALERT)
		if (int_1_triggered)
		{
			int_1_triggered = false;
			g_task_event_type = 0;
			switch (check_event_rak12027(true))
			{
			case 1:
				// Collapse alert
				digitalWrite(LED_GREEN, HIGH);
				collapse_alert = true;
				MYLOG("APP", "Earthquake collapse alert!");
				break;
			case 2:
				// ShutDown alert
				shutoff_alert = true;
				MYLOG("APP", "Earthquake shutoff alert!");
				break;
			case 3:
				// Collapse & ShutDown alert
				digitalWrite(LED_GREEN, HIGH);
				collapse_alert = true;
				shutoff_alert = true;
				MYLOG("APP", "Earthquake collapse & shutoff alert!");
				break;
			default:
				// False alert
				// digitalWrite(LED_BLUE, LOW);
				// digitalWrite(LED_GREEN, LOW);
				MYLOG("APP", "Earthquake false alert!");
				break;
			}
		}
		// api.system.sleep.all(1000);
		// delay(1000);
		// digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));

		if (!earthquake_end)
		{
			digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));
			// api.system.timer.start(RAK_TIMER_1, 500, NULL);
			return;
		}
		// api.system.timer.stop(RAK_TIMER_1);
		digitalWrite(LED_GREEN, LOW);
		MYLOG("APP", "Earthquake end");
	}

	// if ((earthquake_end) && (g_task_event_type != SEISMIC_EVENT) && (g_task_event_type != SEISMIC_ALERT))
	if (no_earthquake_event)
	{
		MYLOG("APP", "Timer Wakeup");
		earthquake_end = false;

		g_solution_data.addPresence(LPP_CHANNEL_EQ_EVENT, false);
		g_solution_data.addPresence(LPP_CHANNEL_EQ_SHUTOFF, shutoff_alert);
		g_solution_data.addPresence(LPP_CHANNEL_EQ_COLLAPSE, collapse_alert);
		g_solution_data.addAnalogInput(LPP_CHANNEL_EQ_SI, savedSI * 10.0);
		g_solution_data.addAnalogInput(LPP_CHANNEL_EQ_PGA, savedPGA * 10.0);
	}

	// Get battery level
	g_solution_data.addVoltage(LPP_CHANNEL_BATT, api.system.bat.get());

	// Get temperature and humidity if sensor is installed
	if (has_rak1901)
	{
		read_rak1901();
	}
	MYLOG("APP", "Packetsize %d", g_solution_data.getSize());

	// Check datarate. Needs DR3 for the package size!
	if (api.lorawan.dr.get() < 3)
	{
		MYLOG("APP", ">>>>> DR changed to %d <<<<<", api.lorawan.dr.get());
		api.lorawan.dr.set(3);
	}

	// Send the packet
	if (api.lorawan.send(g_solution_data.getSize(), g_solution_data.getBuffer(), g_fport, confirmed_msg_enabled, g_repeat_send))
	{
		MYLOG("APP", "Enqueued");
	}
	else
	{
		MYLOG("APP", "Send fail");
	}
}

/**
 * @brief This example is complete timer
 * driven. The loop() does nothing than
 * sleep.
 *
 */
void loop()
{
	/* Destroy this busy loop and use timer to do what you want instead,
	 * so that the system thread can auto enter low power mode by api.system.lpm.set(1); */
	api.system.scheduler.task.destroy();
	// api.system.sleep.all();
}