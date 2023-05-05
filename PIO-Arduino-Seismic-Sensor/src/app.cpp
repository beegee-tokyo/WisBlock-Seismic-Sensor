/**
 * @file WisBlock-Kit-2.ino
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief Example for a GNSS tracker based on WisBlock modules
 * @version 0.1
 * @date 2021-09-11
 *
 * @copyright Copyright (c) 2021
 *
 */

#include "app.h"

#ifdef NRF52_SERIES
/** Timer to wakeup task frequently and send message */
SoftwareTimer delayed_sending;
#endif

/** Set the device name, max length is 10 characters */
char g_ble_dev_name[10] = "RAK-SEISM";

/** LoRaWAN packet */
WisCayenne g_solution_data(255);

/** Send Fail counter **/
uint8_t join_or_send_fail = 0;

/** Flag if RAK1901 temperature sensor is installed */
bool has_rak1901 = false;

/** Flag if RAK12002 RTC is installed */
bool has_rak12002 = false;

/** Flag if we rejoined the network after transmission error */
bool rejoin_network = false;

/**
 * @brief Timer function used to avoid sending packages too often.
 *       Delays the next package by 10 seconds
 *
 * @param unused
 *      Timer handle, not used
 */
void send_delayed(TimerHandle_t unused)
{
	api_wake_loop(STATUS);
	delayed_sending.stop();
}

/**
 * @brief Application specific setup functions
 *
 */
void setup_app(void)
{
	Serial.begin(115200);
	time_t serial_timeout = millis();
	// On nRF52840 the USB serial is not available immediately
	while (!Serial)
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
	digitalWrite(LED_GREEN, LOW);

	// Prepare delayed sending timer
	delayed_sending.begin(g_lorawan_settings.send_repeat_time, send_delayed, NULL, false);
	AT_PRINTF("Seismic Sensor\n");
	AT_PRINTF("Built with RAK's WisBlock\n");
	AT_PRINTF("SW Version %d.%d.%d\n", g_sw_ver_1, g_sw_ver_2, g_sw_ver_3);
	AT_PRINTF("LoRa(R) is a registered trademark or service mark of Semtech Corporation or its affiliates.\nLoRaWAN(R) is a licensed mark.\n");
	AT_PRINTF("============================\n");

#ifdef NRF52_SERIES
	// Enable BLE
	g_enable_ble = true;
#endif
}

/**
 * @brief Application specific initializations
 *
 * @return true Initialization success
 * @return false Initialization failure
 */
bool init_app(void)
{
	bool init_result = true;
	MYLOG("APP", "init_app");

	pinMode(WB_IO2, OUTPUT);
	digitalWrite(WB_IO2, LOW);

	// Start the I2C bus
	Wire.begin();
	Wire.setClock(400000);

	// Initialize Seismic module
	MYLOG("APP", "Initialize RAK12027");
	// Get saved threshold setting
	read_threshold_settings();
	init_result = init_rak12027();
	MYLOG("APP", "RAK12027 %s", init_result ? "success" : "failed");
	if (init_result)
	{
		Serial.println("+EVT: RAK12027 OK");
	}

	// Initialize Temperature sensor
	MYLOG("APP", "Initialize RAK1901");
	has_rak1901 = init_rak1901();
	MYLOG("APP", "RAK1901 %s", has_rak1901 ? "success" : "failed");
	if (has_rak1901)
	{
		Serial.println("+EVT: RAK1901 OK");
	}

	// Initialize RTC
	MYLOG("APP", "Initialize RAK12002");
	has_rak12002 = init_rak12002();
	MYLOG("APP", "RAK12002 %s", has_rak12002 ? "success" : "failed");
	if (has_rak12002)
	{
		Serial.println("+EVT: RAK12002 OK");
	}

	// Initialize AT commands
	init_user_at();

	api_log_settings();

	// Reset the packet
	g_solution_data.reset();

	return init_result;
}

/**
 * @brief Application specific event handler
 *        Requires as minimum the handling of STATUS event
 *        Here you handle as well your application specific events
 */
void app_event_handler(void)
{
	// Seismic sensor alert (collapse or shut down interrupt)
	if ((g_task_event_type & SEISMIC_ALERT) == SEISMIC_ALERT)
	{
		g_task_event_type &= N_SEISMIC_ALERT;
		switch (check_event_rak12027(true))
		{
		case 1:
			// Collapse alert
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
			collapse_alert = true;
			shutoff_alert = true;
			MYLOG("APP", "Earthquake collapse & shutoff alert!");
			break;
		default:
			// False alert
			digitalWrite(LED_BLUE, LOW);
			MYLOG("APP", "Earthquake false alert!");
			break;
		}
	}

	// Handle Seismic Events
	if ((g_task_event_type & SEISMIC_EVENT) == SEISMIC_EVENT)
	{
		MYLOG("APP", "Earthquake event");
		g_task_event_type &= N_SEISMIC_EVENT;
		switch (check_event_rak12027(false))
		{
		case 4:
			// Earthquake start
			MYLOG("APP", "Earthquake start alert!");
			read_rak12027(false);
			earthquake_end = false;
			g_solution_data.addPresence(LPP_CHANNEL_EQ_EVENT, true);
			// Make sure no packet is sent while analyzing
			api_timer_stop();
			delayed_sending.stop();

			// // Request packet sending
			// g_task_event_type = g_task_event_type | STATUS;

			break;
		case 5:
			// Earthquake end
			MYLOG("APP", "Earthquake end alert!");
			read_rak12027(true);
			earthquake_end = true;
			g_solution_data.addPresence(LPP_CHANNEL_EQ_EVENT, true);
			g_solution_data.addPresence(LPP_CHANNEL_EQ_SHUTOFF, shutoff_alert);
			g_solution_data.addPresence(LPP_CHANNEL_EQ_COLLAPSE, collapse_alert);

			// Reset flags
			shutoff_alert = false;
			collapse_alert = false;
			// Send another packet in 1 minute
			delayed_sending.setPeriod(60000);
			delayed_sending.start();
			// Restart frequent sending
			api_timer_restart(g_lorawan_settings.send_repeat_time);

			// Request packet sending
			g_task_event_type = g_task_event_type | STATUS;

			break;
		default:
			// False alert
			earthquake_end = true;
			MYLOG("APP", "Earthquake false alert!");
			break;
		}
	}

	// Timer triggered event
	if ((g_task_event_type & STATUS) == STATUS)
	{
		g_task_event_type &= N_STATUS;
		MYLOG("APP", "Timer wakeup");

#ifdef NRF52_SERIES
		// If BLE is enabled, restart Advertising
		if (g_enable_ble)
		{
			restart_advertising(15);
		}
#endif

		if (!rejoin_network)
		{
			// Check for seismic events
			// if ((earthquake_end) && !(g_task_event_type & SEISMIC_EVENT) && !(g_task_event_type & SEISMIC_ALERT))
			if (earthquake_end)
			{
				g_solution_data.addPresence(LPP_CHANNEL_EQ_SHUTOFF, shutoff_alert);
				g_solution_data.addPresence(LPP_CHANNEL_EQ_COLLAPSE, collapse_alert);
				g_solution_data.addAnalogInput(LPP_CHANNEL_EQ_SI, savedSI * 10.0);
				g_solution_data.addAnalogInput(LPP_CHANNEL_EQ_PGA, savedPGA * 10.0);
				earthquake_end = false;
				MYLOG("APP", "Sending earthquake end message");
			}
			else
			{
				g_solution_data.addPresence(LPP_CHANNEL_EQ_EVENT, false);
			}

			// Get battery level
			float batt_level_f = read_batt();
			g_solution_data.addVoltage(LPP_CHANNEL_BATT, batt_level_f / 1000.0);

			// Get temperature and humidity if sensor is installed
			if (has_rak1901)
			{
				read_rak1901();
			}
		}
		else
		{
			rejoin_network = false;
			MYLOG("APP", "Retry last packet after re-join");
		}

		MYLOG("APP", "Packetsize %d", g_solution_data.getSize());

		if (g_lorawan_settings.lorawan_enable)
		{
			if (g_lpwan_has_joined)
			{
				lmh_error_status result = send_lora_packet(g_solution_data.getBuffer(), g_solution_data.getSize());
				switch (result)
				{
				case LMH_SUCCESS:
					MYLOG("APP", "Packet enqueued");
					break;
				case LMH_BUSY:
					MYLOG("APP", "LoRa transceiver is busy");
					AT_PRINTF("+EVT:BUSY\n");

					/// \todo if 10 times busy, restart the node
					join_or_send_fail++;
					if (join_or_send_fail == 10)
					{
						// Too many failed send attempts, reset node and try to rejoin
						delay(100);
						api_reset();
					}
					break;
				case LMH_ERROR:
					AT_PRINTF("+EVT:SIZE_ERROR\n");
					MYLOG("APP", "Packet error, too big to send with current DR");

					/// \todo if 10 times busy, restart the node
					join_or_send_fail++;
					if (join_or_send_fail == 10)
					{
						// Too many failed send attempts, reset node and try to rejoin
						delay(100);
						api_reset();
					}
					break;
				}
			}
			else
			{
				MYLOG("APP", "LoRaWAN not joined yet, skip uplink");
			}
		}
		else
		{
			// Add the device DevEUI as a device ID to the packet
			uint8_t packet_buffer[g_solution_data.getSize() + 8];
			memcpy(packet_buffer, g_lorawan_settings.node_device_eui, 8);
			memcpy(&packet_buffer[8], g_solution_data.getBuffer(), g_solution_data.getSize());

			// Send packet over LoRa
			if (send_p2p_packet(packet_buffer, g_solution_data.getSize() + 8))
			{
				MYLOG("APP", "Packet enqueued");
			}
			else
			{
				AT_PRINTF("+EVT:SIZE_ERROR\n");
				MYLOG("APP", "Packet too big");
			}
		}
		// Reset the packet
		g_solution_data.reset();
	}
}

#ifdef NRF52_SERIES
/**
 * @brief Handle BLE UART data
 *
 */
void ble_data_handler(void)
{
	if (g_enable_ble)
	{
		// BLE UART data handling
		if ((g_task_event_type & BLE_DATA) == BLE_DATA)
		{
			MYLOG("AT", "RECEIVED BLE");
			/** BLE UART data arrived */
			g_task_event_type &= N_BLE_DATA;

			while (g_ble_uart.available() > 0)
			{
				at_serial_input(uint8_t(g_ble_uart.read()));
				delay(5);
			}
			at_serial_input(uint8_t('\n'));
		}
	}
}
#endif

/**
 * @brief Handle received LoRa Data
 *
 */
void lora_data_handler(void)
{
	// LoRa Join finished handling
	if ((g_task_event_type & LORA_JOIN_FIN) == LORA_JOIN_FIN)
	{
		g_task_event_type &= N_LORA_JOIN_FIN;
		if (g_join_result)
		{
			MYLOG("APP", "Successfully joined network");
			// Reset join failed counter
			join_or_send_fail = 0;

			// // Force a sensor reading in 10 seconds
			delayed_sending.setPeriod(10000);
			delayed_sending.start();
		}
		else
		{
			MYLOG("APP", "Join network failed");
			/// \todo here join could be restarted.
			lmh_join();

			// If BLE is enabled, restart Advertising
			if (g_enable_ble)
			{
				restart_advertising(15);
			}

			join_or_send_fail++;
			if (join_or_send_fail == 10)
			{
				// Too many failed join requests, reset node and try to rejoin
				delay(100);
				api_reset();
			}
		}
	}

	// LoRa data handling
	if ((g_task_event_type & LORA_DATA) == LORA_DATA)
		if ((g_task_event_type & LORA_DATA) == LORA_DATA)
		{
			g_task_event_type &= N_LORA_DATA;
			MYLOG("APP", "Received package over LoRa");
			// Check if uplink was a send frequency change command
			if ((g_last_fport == 3) && (g_rx_data_len == 6))
			{
				if (g_rx_lora_data[0] == 0xAA)
				{
					if (g_rx_lora_data[1] == 0x55)
					{
						uint32_t new_send_frequency = 0;
						new_send_frequency |= (uint32_t)(g_rx_lora_data[2]) << 24;
						new_send_frequency |= (uint32_t)(g_rx_lora_data[3]) << 16;
						new_send_frequency |= (uint32_t)(g_rx_lora_data[4]) << 8;
						new_send_frequency |= (uint32_t)(g_rx_lora_data[5]);

						MYLOG("APP", "Received new send frequency %ld s\n", new_send_frequency);
						// Save the new send frequency
						g_lorawan_settings.send_repeat_time = new_send_frequency * 1000;

						// Set the timer to the new send frequency
						api_timer_restart(g_lorawan_settings.send_repeat_time);
						// Save the new send frequency
						save_settings();
					}
				}
			}

			if (g_lorawan_settings.lorawan_enable)
			{
				AT_PRINTF("+EVT:RX_1, RSSI %d, SNR %d\n", g_last_rssi, g_last_snr);
				AT_PRINTF("+EVT:%d:", g_last_fport);
				for (int idx = 0; idx < g_rx_data_len; idx++)
				{
					AT_PRINTF("%02X", g_rx_lora_data[idx]);
				}
				AT_PRINTF("\n");
			}
			else
			{
				AT_PRINTF("+EVT:RXP2P, RSSI %d, SNR %d\n", g_last_rssi, g_last_snr);
				AT_PRINTF("+EVT:");
				for (int idx = 0; idx < g_rx_data_len; idx++)
				{
					AT_PRINTF("%02X", g_rx_lora_data[idx]);
				}
				AT_PRINTF("\n");
			}
		}

	// LoRa TX finished handling
	if ((g_task_event_type & LORA_TX_FIN) == LORA_TX_FIN)
	{
		g_task_event_type &= N_LORA_TX_FIN;

		MYLOG("APP", "LPWAN TX cycle %s", g_rx_fin_result ? "finished ACK" : "failed NAK");
	}
}
