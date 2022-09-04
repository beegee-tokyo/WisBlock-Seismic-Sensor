# 1 "d:\\#Github\\Solutions\\WisBlock-Seismic-Sensor\\RUI3-Seismic-Sensor\\RUI3-Seismic-Sensor.ino"
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
# 12 "d:\\#Github\\Solutions\\WisBlock-Seismic-Sensor\\RUI3-Seismic-Sensor\\RUI3-Seismic-Sensor.ino"
# 13 "d:\\#Github\\Solutions\\WisBlock-Seismic-Sensor\\RUI3-Seismic-Sensor\\RUI3-Seismic-Sensor.ino" 2

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
# 46 "d:\\#Github\\Solutions\\WisBlock-Seismic-Sensor\\RUI3-Seismic-Sensor\\RUI3-Seismic-Sensor.ino"
void receiveCallback(SERVICE_LORA_RECEIVE_T *data)
{
 do { if ("RX-CB") Serial.printf("[%s] ", "RX-CB"); Serial.printf("RX, fP %d, DR %d, RSSI %d, SNR %d", data->Port, data->RxDatarate, data->Rssi, data->Snr); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);

 for (int i = 0; i < data->BufferSize; i++)
 {
  Serial.printf("%02X", data->Buffer[i]);
 }
 Serial.print("\r\n");

}

/**

 * @brief Callback after TX is finished

 *

 * @param status TX status

 */
# 63 "d:\\#Github\\Solutions\\WisBlock-Seismic-Sensor\\RUI3-Seismic-Sensor\\RUI3-Seismic-Sensor.ino"
void sendCallback(int32_t status)
{
 if (status != 0)
 {
  fail_counter++;
  if (fail_counter == 4)
  {
   do { if ("TX-CB") Serial.printf("[%s] ", "TX-CB"); Serial.printf("4 times TX fail, rejoin"); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);
   ret = api.lorawan.join();
   fail_counter = 0;
  }
 }
 do { if ("TX-CB") Serial.printf("[%s] ", "TX-CB"); Serial.printf("TX %d", status); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);
 digitalWrite(1/*LED2*/ /* IO_SLOT*//*PA1*/, 0x0);
}

/**

 * @brief Callback after join request cycle

 *

 * @param status Join result

 */
# 84 "d:\\#Github\\Solutions\\WisBlock-Seismic-Sensor\\RUI3-Seismic-Sensor\\RUI3-Seismic-Sensor.ino"
void joinCallback(int32_t status)
{
 if (status != 0)
 {
  if (!(ret = api.lorawan.join()))
  {
   do { if ("J-CB") Serial.printf("[%s] ", "J-CB"); Serial.printf("Fail! \r\n"); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);
  }
 }
 else
 {
  // MYLOG("J-CB", "Joined\r\n");
  // We need at least DR3 for the packet size
  api.lorawan.dr.set(3);
  // MYLOG("J-CB", "DR3 %s", api.lorawan.dr.set(3) ? "OK" : "NOK");
  digitalWrite(1/*LED2*/ /* IO_SLOT*//*PA1*/, 0x0);
  if (g_send_repeat_time != 0)
  {
   // Start a unified C timer
   api.system.timer.start(RAK_TIMER_0, g_send_repeat_time, 
# 103 "d:\\#Github\\Solutions\\WisBlock-Seismic-Sensor\\RUI3-Seismic-Sensor\\RUI3-Seismic-Sensor.ino" 3 4
                                                          __null
# 103 "d:\\#Github\\Solutions\\WisBlock-Seismic-Sensor\\RUI3-Seismic-Sensor\\RUI3-Seismic-Sensor.ino"
                                                              );
  }
 }
}

/**

 * @brief Arduino setup, called once after reboot/power-up

 *

 */
# 112 "d:\\#Github\\Solutions\\WisBlock-Seismic-Sensor\\RUI3-Seismic-Sensor\\RUI3-Seismic-Sensor.ino"
void setup()
{
 // Setup the callbacks for joined and send finished
 api.lorawan.registerRecvCallback(receiveCallback);
 api.lorawan.registerSendCallback(sendCallback);
 api.lorawan.registerJoinCallback(joinCallback);

 if (!api.system.lpm.set(1))
 {
  do { if ("SET") Serial.printf("[%s] ", "SET"); Serial.printf("Failed to set low power mode"); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);
 }

 pinMode(0/*LED1*/ /* IO_SLOT*//*PA0*/, 0x1);
 digitalWrite(0/*LED1*/ /* IO_SLOT*//*PA0*/, 0x1);
 pinMode(1/*LED2*/ /* IO_SLOT*//*PA1*/, 0x1);
 digitalWrite(1/*LED2*/ /* IO_SLOT*//*PA1*/, 0x1);

 pinMode(8/*IO2*/ /* SLOT_A SLOT_B*/, 0x1);
 digitalWrite(8/*IO2*/ /* SLOT_A SLOT_B*/, 0x0);

 // Use RAK_CUSTOM_MODE supresses AT command and default responses from RUI3
 // Serial.begin(115200, RAK_CUSTOM_MODE);
 // Use "normal" mode to have AT commands available
 Serial.begin(115200);
# 153 "d:\\#Github\\Solutions\\WisBlock-Seismic-Sensor\\RUI3-Seismic-Sensor\\RUI3-Seismic-Sensor.ino"
 // For RAK3172 just wait a little bit for the USB to be ready
 udrv_app_delay_ms(5000);


 // Initialize Seismic module
 do { if ("SET") Serial.printf("[%s] ", "SET"); Serial.printf("Initialize RAK12027"); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);
 bool init_result = init_rak12027();
 do { if ("SET") Serial.printf("[%s] ", "SET"); Serial.printf("Init %s", init_result ? "success" : "failed"); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);

 // Initialize Temperature sensor
 do { if ("SET") Serial.printf("[%s] ", "SET"); Serial.printf("Initialize RAK1901"); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);
 has_rak1901 = init_rak1901();
 do { if ("SET") Serial.printf("[%s] ", "SET"); Serial.printf("Init %s", has_rak1901 ? "success" : "failed"); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);

 do { if ("SET") Serial.printf("[%s] ", "SET"); Serial.printf("RAKwireless %s Node", g_dev_name); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);

 do { if ("SET") Serial.printf("[%s] ", "SET"); Serial.printf("Setup your device with AT commands"); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);

 digitalWrite(0/*LED1*/ /* IO_SLOT*//*PA0*/, 0x0);

 init_custom_at();
 get_at_setting(0x00000002 /* length 4 bytes*/);

 // Create a unified timer
 api.system.timer.create(RAK_TIMER_0, sensor_handler, RAK_TIMER_PERIODIC);
 api.system.timer.create(RAK_TIMER_1, sensor_handler, RAK_TIMER_ONESHOT);

 // Get the confirmed mode settings
 confirmed_msg_enabled = api.lorawan.cfm.get();
 do { if ("SET") Serial.printf("[%s] ", "SET"); Serial.printf("Confirmed message is %s", api.lorawan.cfm.get() == 0 ? "off" : "on"); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);

 if (!(ret = api.lorawan.join()))
 {
  do { if ("SET") Serial.printf("[%s] ", "SET"); Serial.printf("Join request failed! \r\n"); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);
 }
}

/**

 * @brief sensor_handler is a timer function called every

 * g_send_repeat_time milliseconds. Default is 120000. Can be

 * changed with a custom AT command ATC+SENDFREQ

 *

 */
# 196 "d:\\#Github\\Solutions\\WisBlock-Seismic-Sensor\\RUI3-Seismic-Sensor\\RUI3-Seismic-Sensor.ino"
void sensor_handler(void *)
{
 // Reset the packet
 g_solution_data.reset();

 // Get battery level
 g_solution_data.addVoltage(1 /* Base Board*/, api.system.bat.get());

 // Check for seismic events
 if ((earthquake_end) && (g_task_event_type != 0b0000100000000000) && (g_task_event_type != 0b0000010000000000))
 {
  g_solution_data.addPresence(43 /* RAK12027*/, false);
 }

 // Handle Seismic Events
 if (g_task_event_type == 0b0000100000000000)
 {
  do { if ("APP") Serial.printf("[%s] ", "APP"); Serial.printf("Earthquake event"); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);
  g_task_event_type = 0;
  switch (check_event_rak12027(false))
  {
  case 4:
   // Earthquake start
   do { if ("APP") Serial.printf("[%s] ", "APP"); Serial.printf("Earthquake start alert!"); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);
   read_rak12027(false);
   earthquake_end = false;
   g_solution_data.addPresence(43 /* RAK12027*/, true);
   // Make sure no packet is sent while analyzing
   api.system.timer.stop(RAK_TIMER_0);
   return;
   break;
  case 5:
   // Earthquake end
   do { if ("APP") Serial.printf("[%s] ", "APP"); Serial.printf("Earthquake end alert!"); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);
   read_rak12027(true);
   earthquake_end = true;
   g_solution_data.addPresence(46 /* RAK12027*/, shutoff_alert);
   shutoff_alert = false;

   g_solution_data.addPresence(47 /* RAK12027*/, collapse_alert);
   collapse_alert = false;

   // Reset flags
   shutoff_alert = false;
   collapse_alert = false;

   // Send another packet in 10 seconds
   api.system.timer.start(RAK_TIMER_1, 10000, 
# 243 "d:\\#Github\\Solutions\\WisBlock-Seismic-Sensor\\RUI3-Seismic-Sensor\\RUI3-Seismic-Sensor.ino" 3 4
                                             __null
# 243 "d:\\#Github\\Solutions\\WisBlock-Seismic-Sensor\\RUI3-Seismic-Sensor\\RUI3-Seismic-Sensor.ino"
                                                 );
   // Restart frequent sending
   api.system.timer.start(RAK_TIMER_0, g_send_repeat_time, 
# 245 "d:\\#Github\\Solutions\\WisBlock-Seismic-Sensor\\RUI3-Seismic-Sensor\\RUI3-Seismic-Sensor.ino" 3 4
                                                          __null
# 245 "d:\\#Github\\Solutions\\WisBlock-Seismic-Sensor\\RUI3-Seismic-Sensor\\RUI3-Seismic-Sensor.ino"
                                                              );
   break;
  default:
   // False alert
   earthquake_end = true;
   do { if ("APP") Serial.printf("[%s] ", "APP"); Serial.printf("Earthquake false alert!"); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);
   return;
   break;
  }
 }
 else if (g_task_event_type == 0b0000010000000000)
 {
  g_task_event_type = 0;
  switch (check_event_rak12027(true))
  {
  case 1:
   // Collapse alert
   collapse_alert = true;
   do { if ("APP") Serial.printf("[%s] ", "APP"); Serial.printf("Earthquake collapse alert!"); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);
   break;
  case 2:
   // ShutDown alert
   shutoff_alert = true;
   do { if ("APP") Serial.printf("[%s] ", "APP"); Serial.printf("Earthquake shutoff alert!"); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);
   break;
  case 3:
   // Collapse & ShutDown alert
   collapse_alert = true;
   shutoff_alert = true;
   do { if ("APP") Serial.printf("[%s] ", "APP"); Serial.printf("Earthquake collapse & shutoff alert!"); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);
   break;
  default:
   // False alert
   digitalWrite(1/*LED2*/ /* IO_SLOT*//*PA1*/, 0x0);
   do { if ("APP") Serial.printf("[%s] ", "APP"); Serial.printf("Earthquake false alert!"); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);
   break;
  }
  return;
 }
 else
 {
  do { if ("APP") Serial.printf("[%s] ", "APP"); Serial.printf("Timer Wakeup"); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);
 }

 // Get temperature and humidity if sensor is installed
 if (has_rak1901)
 {
  read_rak1901();
 }
 do { if ("APP") Serial.printf("[%s] ", "APP"); Serial.printf("Packetsize %d", g_solution_data.getSize()); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);

 // Send the packet
 if (api.lorawan.send(g_solution_data.getSize(), g_solution_data.getBuffer(), g_fport, confirmed_msg_enabled))
 {
  do { if ("APP") Serial.printf("[%s] ", "APP"); Serial.printf("Enqueued"); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);
 }
 else
 {
  do { if ("APP") Serial.printf("[%s] ", "APP"); Serial.printf("Send fail"); Serial.printf("\n"); } while (0); udrv_app_delay_ms(100);
 }
}

/**

 * @brief This example is complete timer

 * driven. The loop() does nothing than

 * sleep.

 *

 */
# 313 "d:\\#Github\\Solutions\\WisBlock-Seismic-Sensor\\RUI3-Seismic-Sensor\\RUI3-Seismic-Sensor.ino"
void loop()
{
 /* Destroy this busy loop and use timer to do what you want instead,

	 * so that the system thread can auto enter low power mode by api.system.lpm.set(1); */
# 317 "d:\\#Github\\Solutions\\WisBlock-Seismic-Sensor\\RUI3-Seismic-Sensor\\RUI3-Seismic-Sensor.ino"
 api.system.scheduler.task.destroy();
 // api.system.sleep.all();
}
