#include "M5Unified.h"
#include "WiFi.h"
#include "EspUsbHostSerial.h"
#include "ThingSpeak.h"
#include "config.h" //enter api key and wifi information

#define DATA_DRAW_INTERVAL 500
#define BTN_GPIO 41 // Define the button GPIO pin

M5Canvas canvas(&M5.Display);

unsigned long prev_moment_thingspeak = 0;
unsigned long prev_moment_draw = 0;
unsigned long current_moment;

enum DisplayMode { CO2, TEMPERATURE, HUMIDITY, ABSOLUTE_HUMIDITY };
DisplayMode currentMode = CO2; // Declare and initialize currentMode

float calculateSaturatedWaterVaporPressure(float temperature, bool ignoreConstant) {
    // temperature is in Celsius
    // https://ja.wikipedia.org/wiki/%E9%A3%BD%E5%92%8C%E6%B0%B4%E8%92%B8%E6%B0%97%E9%87%8F
    if (ignoreConstant) {
        return pow(10, ((7.5 * temperature) / (temperature + 237.3)));
    } else {
        return 6.1078 * pow(10, ((7.5 * temperature) / (temperature + 237.3)));
    }
}

float calculateCorrectedHumidity(float temp0, float temp1, float hum0) {
    // https://blog.mono0x.net/2023/09/03/ud-co2s-temperature-and-humidity/
    // temp0: temperature before correction (read from sensor)
    // temp1: temperature after correction
    // hum0: humidity before correction (read from sensor)
    float sat0 = calculateSaturatedWaterVaporPressure(temp0, true);
    float sat1 = calculateSaturatedWaterVaporPressure(temp1, true);
    return (hum0) * (sat0 / sat1);
}

float calculateAbsoluteHumidity(float temperature, float humidity) {
    // Absolute humidity (volumetric humidity) H (g/m3) is calculated using the formula:
    // H = 216.67 * ( P_w / (T + 273.15) )
    // when
    // P_w: water vapor pressure (hPa), function of temperature and humidity
    // T: temperature (C)

    float saturatedWaterVaporPressure = calculateSaturatedWaterVaporPressure(temperature, false); // P_s (hPa)
    float waterVaporPressure = saturatedWaterVaporPressure * (humidity / 100); // P_w (hPa)

    float absoluteHumidity = 216.67 * ( waterVaporPressure / (temperature + 273.15 ) );

    return absoluteHumidity;
}

void SendSensorData(int co2, float hum, float temp) {
    // Initialize ThingSpeak
    WiFiClient client;
    ThingSpeak.begin(client);

    // Set the fields with the values
    ThingSpeak.setField(1, float(co2));
    ThingSpeak.setField(2, temp);
    ThingSpeak.setField(3, hum);

    // Write to the ThingSpeak channel
    int res = ThingSpeak.writeFields(THING_SPEAK_CHANNEL, THING_SPEAK_API_KEY);
    if (res == 200) {
        Serial.println("Channel update successful.");
    } else {
        Serial.println(String("Problem updating channel. HTTP error code ") + res);
    }
}

void drawData(float val, DisplayMode type) {
    uint32_t BG_COLOR; // RGB888 (24bit)
    int TEXT_COLOR;
    uint32_t MY_BLUE = 0x38ADFF;
    uint32_t MY_GREEN = 0x85CC43;
    uint32_t MY_ORANGE = 0xFFD232;
    uint32_t MY_RED = 0xE63953;
    uint32_t MY_PURPLE = 0xA433FF;

    if (type == CO2) {
        if (val <= 700) {
            BG_COLOR = BLACK;
            TEXT_COLOR = WHITE;
        } else if (val <= 1000) {
            BG_COLOR = MY_GREEN;
            TEXT_COLOR = WHITE;
        } else if (val <= 1200) {
            BG_COLOR = MY_ORANGE;
            TEXT_COLOR = WHITE;
        } else if (val <= 1800) {
            BG_COLOR = MY_RED;
            TEXT_COLOR = WHITE;
        } else {
            BG_COLOR = MY_PURPLE;
            TEXT_COLOR = WHITE;
        }
    } else {
        BG_COLOR = BLACK; // Default background for humidity and temperature
        TEXT_COLOR = WHITE;
    }

    canvas.fillScreen(BG_COLOR);
    canvas.setTextColor(TEXT_COLOR);
    canvas.setTextDatum(MC_DATUM);
    canvas.setFont(&fonts::Font4);
    canvas.setTextSize(1.5); // Set text size to a larger value
    canvas.setCursor(0, (M5.Display.height() / 2)); // Adjust Y position upwards

    if (type == CO2) {
        canvas.drawCentreString(String((int)val), M5.Display.width() / 2, M5.Display.height() / 2 - 15); // Display CO2 as integer
    } else if (type == HUMIDITY) {
        canvas.drawCentreString(String(val, 1), M5.Display.width() / 2, M5.Display.height() / 2 - 15); // Display humidity value
        canvas.setTextSize(0.9); // Set text size for mode label
        canvas.drawString("%", M5.Display.width() - 30, 30);
    } else if (type == ABSOLUTE_HUMIDITY ) {
        canvas.drawCentreString(String(val, 1), M5.Display.width() / 2, M5.Display.height() / 2 - 15); // Display absolute humidity value
        canvas.setTextSize(0.7); // Set text size for mode label
        canvas.drawString("g/m3", M5.Display.width() - 30, 30);
    } else if (type == TEMPERATURE) {
        canvas.drawCentreString(String(val, 1), M5.Display.width() / 2, M5.Display.height() / 2 - 15); // Display temperature value
        canvas.setTextSize(0.9); // Set text size for mode label
        canvas.drawString("`C", M5.Display.width() - 30, 30);
    }

    // Push image to display
    canvas.pushSprite(0, 0);
}

class UDCO2S : public EspUsbHostSerial {
public:
    String value;
    int co2 = 0;
    float hum = 0.0F;
    float tmp = 0.0F;
    float absHum = 0.0F;

    UDCO2S() : EspUsbHostSerial(0x04d8, 0xe95a) {};

    String toString() {
        char bufs[30];
        sprintf(bufs, "CO2=%d,HUM=%.1f,TMP=%.1f", co2, hum, tmp);
        return String(bufs);
    }

    virtual void onNew() override {
        Serial.println(("Manufacturer:" + getManufacturer()).c_str());
        Serial.println(("Product:" + getProduct()).c_str());
        submit((uint8_t *)"STA\r", 4);
    }

    virtual void onReceive(const uint8_t *data, const size_t length) override {
        for (int i = 0; i < length; i++) {
            if (data[i] >= 0x20)
                value += (char)data[i];
            else if (value.length()) {
                if (sscanf(value.c_str(), "CO2=%d,HUM=%f,TMP=%f", &co2, &hum, &tmp) == 3) {
                    // Sensor value correction
                    hum = calculateCorrectedHumidity(tmp, tmp-4.5F, hum);
                    tmp -= 4.5F;
                    absHum = calculateAbsoluteHumidity(tmp, hum);

                    Serial.println(toString());

                    // Store the latest values without immediate redraw
                    current_moment = millis();

                    // Upload to ThingSpeak every THING_SPEAK_INTERVAL (ms)
                    if ((current_moment - prev_moment_thingspeak) >= THING_SPEAK_INTERVAL) {
                        SendSensorData(co2, hum, tmp);
                        prev_moment_thingspeak = current_moment;
                    }
                }
                value.clear();
            }
        }
    }

    virtual void onGone() {
        Serial.println("disconnected");
    }
};

UDCO2S usbDev;

void setup() {
    M5.begin();
    Serial.begin(9600);

    // Setup display and canvas
    M5.Display.setBrightness(90);
    M5.Display.setRotation(1);
    canvas.createSprite(M5.Display.width(), M5.Display.height());

    // Setup Wi-Fi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(200);
        canvas.print('.');
    }
    canvas.println("\nWiFi connected");
    delay(3000);

    usbDev.begin();

    // Draw
    M5.Display.startWrite(); 
    canvas.pushSprite(0, 0);

    // Set button pin mode
    pinMode(BTN_GPIO, INPUT_PULLUP); // Set button pin as input with pull-up
}

unsigned long lastReconnectTime = 0; // Store the last reconnect time

void loop() {
    usbDev.task();

    // Check if the button is pressed
    if (digitalRead(BTN_GPIO) == LOW) { // Check if button is pressed
        Serial.println("Button pressed!"); // Debug output
        currentMode = static_cast<DisplayMode>((currentMode + 1) % 4); // Cycle through modes
        delay(200); // Debounce delay
    }

    // Update the display based on DATA_DRAW_INTERVAL
    current_moment = millis();
    if ((current_moment - prev_moment_draw) >= DATA_DRAW_INTERVAL) {
        prev_moment_draw = current_moment; // Update the last draw time

        // Display based on the current mode
        if (currentMode == CO2) {
            drawData(usbDev.co2, CO2);
        } else if (currentMode == HUMIDITY) {
            drawData(usbDev.hum, HUMIDITY);
        } else if (currentMode == ABSOLUTE_HUMIDITY) {
            drawData(usbDev.absHum, ABSOLUTE_HUMIDITY);
        } else if (currentMode == TEMPERATURE) {
            drawData(usbDev.tmp, TEMPERATURE);
        }
    }

    // Check if 24 hours have passed since the last reconnect
    if (millis() - lastReconnectTime >= 24 * 60 * 60 * 1000) {
        // Attempt to reconnect to WiFi
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        lastReconnectTime = millis(); // Update the last reconnect time

        // Wait for connection
        while (WiFi.status() != WL_CONNECTED) {
            delay(200);
        }
        Serial.println("WiFi reconnected");
    }
}
