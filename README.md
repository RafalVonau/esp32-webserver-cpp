# Simple WebServer for esp-idf
This project implements basic Web server protocol for esp-idf version 4 and above.

See details in examples folder.

# Configurable parameters
Configure WiFi SSID/PASSWORD in menuconfig or by manually editing sdkconfig.defaults.

* CONFIG_EXAMPLE_WIFI_SSID
* CONFIG_EXAMPLE_WIFI_PASSWORD

# Building under Linux
* install PlatformIO
* enter examples/basic directory
* type in terminal:
  platformio run
  platformio upload
* type http://express.local/ or http://express.lan/ in the web browser.

# Example page
The page is based on Next.js and Mantine UI.


You can also use IDE to build this project on Linux/Windows/Mac. My fvorite ones:
* [Code](https://code.visualstudio.com/) 
* [Atom](https://atom.io/)

Enjoy :-)
