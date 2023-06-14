# atom-s3-co2
CO2 monitor integration for M5Stack AtomS3. Reads sensor values from [UD-CO2S](https://www.iodata.jp/product/tsushin/iot/ud-co2s/), and sends values to ThingsSpeak IoT platform.

## Materials
- M5Stack AtomS3
- [UD-CO2S](https://www.iodata.jp/product/tsushin/iot/ud-co2s/)
- 5V power supply
- USB type C to A adaptor (used to connect AtomS3 and UD-CO2S)

## Setup
- Prepare environment: VSCode + platform.io
- Setup `src/config.h`
    - ThingSpeak API keys, channel ID, data sending interval
    - Wi-Fi SSID and password
- Build and write the program to AtomS3 using USB
- Connect UD-CO2S to AtomS3
- Provide power 5V supply
- When AtomS3 is booted up, it will automatically connect to Wi-Fi and start reading values from the sensor.

## References
- This project was developed based on codes/libraries from [wakwak-koba/EspUsbHost](https://github.com/wakwak-koba/EspUsbHost).