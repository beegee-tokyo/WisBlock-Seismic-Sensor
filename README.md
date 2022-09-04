# RAK12027 WisBlock Seismic Sensor

| <img src="./assets/RAK-Whirls.png" alt="RAKWireless"  width="150"> | <img src="./assets/unify-solar.png" alt="RAK Unify"  width="150"> | <img src="./assets/rakstar.jpg" alt="RAKStars"  width="150"> | <img src="./assets/Assembly-Animation.gif" alt="Assembly"  width="200"> |
| -- | -- | -- | -- |

Earthquakes can be dangerous and are often unpredictable. Seismic stations all around the world are measuring seismic activities. With the D7S module Omron created an affordable sensor that can detect earthquakes and measure their intensity as SI values. The SI values have a high correlation with the seismic intensity scale that indicates the magnitude of an earthquake.    
While this product cannot be used to predict earthquakes, it is a good solution to send warnings and protect sensitive machinery in case an earthquake occurs. Beside of measuring the strength, it generates a warning signal if the SI level is higher than 5 that can be used to shutdown machinery. It detects as well if the horizontal position of the sensor changes, which points towards a collapse of the structure where the sensor was deployed.

Please check out Omrons documentation for the D7S module:
- [_**D7S Vibration Sensor**_](https://components.omron.com/us-en/products/sensors/D7S)
- [_**D7S Seismic Sensor**_](https://components.omron.com/us-en/solutions/sensor/seismic-sensors)
- [_**D7S datasheet**_](https://components.omron.com/us-en/datasheet_pdf/A252-E1.pdf)
- [_**D7S Video 1**_](https://www.youtube.com/embed/d7PJ3fCwQ-8?rel=0&autoplay=1)
- [_**D7S Video 2**_](https://www.youtube.com/embed/vNTC2ONmp0I?rel=0&autoplay=1)

Using the small sized [_**RAK12027 Seismic Sensor**_](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK12027/Overview) together with the WisBlock Core and WisBlock Base modules makes it very easy to build up the earthquake warning system.

This example can be used as a start point to write a low power consumption seismic alarm system that can be powered by battery and solar panel. The consumption in sleep mode is ~90uA. The code is completely interrupt based to keep the MCU as much as possible in sleep mode to save battery. The collected data of an earthquake is sent over LoRaWAN, but it can be used as well with LoRa P2P. It will send data packets after the D7S has finished it's data processing with the information of the SI level, PGA, shutdown alert and collapse allert signal.

# Content
- [RAK products used in this project](#rak_products_used_in_this_project)
   - [Assembly](#assembly)
- [How it works](#how_it_works)
- [Seismic Sensor code for RAK4631 using the RAK-nRF52 BSP for Arduino](#seismic_sensor_code_for_rak4631_using_the_rak-nrf52_bsp_for_arduino)
- [Seismic Sensor code for RAK4631-R and RAK3172 using the RAK RUI3 API](#seismic_sensor_code_for_rak4631-r_and_rak3172_using_the_rak_rui3_api)
- [Data packet format](#data_packet_format)
- [Example for a visualization and alert message](#example_for_a_visualization_and_alert_message)

# RAK products used in this project

This project uses the [_**RAK19007**_](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK19007/Overview) Base Board, the [_**RAK12027**_](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK12027/Overview) Seismic Sensor and optional the [_**RAK1901**_](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK1901/Overview) Temperature and Humidity Sensor.    
In case the firmware is built with the RAK-nRF52 Arduino BSP, it uses the [_**RAK4631**_](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK4631/Overview) Core Module.    
In case the RAK RUI3 API is used for the firmware, it uses the [_**RAK4631-R**_](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK4631-R/Overview) Core Module.    

## Assembly
Place the WisBlock Core and WisBlock Sensor module in the following slots of the RAK19007 Base Board:    
![Assembly](./assets/Assembly-Animation.gif)

# How it works

# Seismic Sensor code for RAK4631 using the RAK-nRF52 BSP for Arduino

# Seismic Sensor code for RAK4631-R and RAK3172 using the RAK RUI3 API

# Data packet format

# Example for a visualization and alert message