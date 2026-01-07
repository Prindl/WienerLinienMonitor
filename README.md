[![PlatformIO Build](https://github.com/Prindl/WienerLinienMonitor/actions/workflows/main.yml/badge.svg)](https://github.com/Prindl/WienerLinienMonitor/actions/workflows/main.yml)
# Wiener Linien + ÖBB ESP32-S3 Public Transport Departure Monitor

This repository is a reworked version of the [original one by coppermilk](https://github.com/coppermilk/wiener_linien_esp32_monitor).

**Brief description:** The ESP32-S3 Public Transport Departure Monitor project is a small device based on the ESP32-S3 platform. It allows you to track public transport departures in real time and receive a countdown to the next departures. The project is based on the use of open data from the City of Vienna provided by Wiener Linien. (Data source: City of Vienna - https://data.wien.gv.at)

## Key Features
- **Real-time Countdown**: Stay informed about the next public transport departure times.
- **Timely Updates**: Data is automatically refreshed every 20 seconds to ensure accuracy.
- **Easy Startup**: The device starts up automatically when the ESP32-S3 is booted.
- **User-Friendly Interface**: A thoughtfully designed, intuitive interface for ease of use.
- **Data Source**: Utilizes open data from the City of Vienna provided by Wiener Linien.

## Enhancements In This Repository:
- **Support for ÖBB Trainstations**
- **Screen Dimming**: The backlight of the Display can be dimmed to 4 different brightness levels (25%, 50%, 75% and 100%)
- **Power Saving Modes**: Different modes for power saving are enabled automatically when no data is received.
- **User Button Control**: Each Button now supports 3 different types of control - short, long and double press.
- **Automatic Layout Switch**: When the number of monitors is less than the configured number, then the layout changes.
- **Reconfiguration without SoftReset**: To reconfigure the device when a WiFi is connected press the reset button twice.
- **Improvements**: Buttons now work interrrupt based, more information is displayed for each monitor and scrolling text has a short delay at start and finish.

## More Information
Each monitor now displays the name of the stop and if the vehicle is accessible to wheelchairs. Vehicles that have different directions are combined in a single monitor - e.g. the tram is being withdrawn from the railway. When only one monitor is available it can show up to 6 vehicles, depending on the configured layout.

Enter an EVA number to track trainstation from the ÖBB!

## Dimming Button
Short pressing on the Dimming Button (right of the USB port) will dim the screen.

Double pressing will switch between the different layouts. It is possible to switch between display 1,2 or 3 monitors at the same time.

Long Pressing will activate the power saving mode until it is deactivated in the same way.

## Reset Button
Short pressing on the Reset Button (left of the USB port) is currently unused.

Double pressing on the Reset Button opens the configuration portel.

Long pressing will perform a soft or factory rest depending on the amount of time the button is held pressed:
- **5 - 30s**: a soft reset will delete the wifi settings and the device can be configured again.
- **>30s**: a factory reset will delete all saved settings on the device.

## Power Saving Modes
Three modes were added and each mode progressively adds more power savings:
- *ECO Light*: This power saving mode will only turn the display off.
- *ECO Medium*: In addition to *ECO Light* this mode will reduce the CPU frequency to 80MHz and suspend the screen updates.
- *ECO Heavy*: In addition to *ECO Medium* this mode will turn the WiFi off.
