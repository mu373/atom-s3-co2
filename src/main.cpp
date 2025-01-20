#include "M5Unified.h"
#include "WiFi.h"
#include "EspUsbHostSerial.h"
#include "ThingSpeak.h"
#include "config.h" //enter api key and wifi information

#define DATA_DRAW_INTERVAL 3000

M5Canvas canvas(&M5.Display);

unsigned long prev_moment_thingspeak = 0;
unsigned long prev_moment_draw = 0;
unsigned long current_moment;

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
    if (res == 200)
    {
        Serial.println("Channel update successful.");
    }
    else
    {
        Serial.println(String("Problem updating channel. HTTP error code ") + res);
    }
}

void drawCO2data(int co2, float hum, float tmp) {
    uint32_t BG_COLOR; // RGB888 (24bit)
    uint32_t MY_BLUE = 0x38ADFF;
    uint32_t MY_GREEN = 0x85CC43;
    uint32_t MY_ORANGE = 0xFFD232;
    uint32_t MY_RED = 0xE63953;
    uint32_t MY_PURPLE = 0xA433FF;

    if (co2 <= 700) {
        BG_COLOR = MY_BLUE;
    }
    else if (co2 <= 1000) {
        BG_COLOR = MY_GREEN;
    }
    else if (co2 <= 1200) {
        BG_COLOR = MY_ORANGE;
    }
    else if (co2 <= 1800) {
        BG_COLOR = MY_RED;
    }
    else {
        BG_COLOR = MY_PURPLE;
    }

    canvas.fillScreen(BG_COLOR);
    canvas.setTextColor(WHITE);
    canvas.setTextDatum(middle_left);
    canvas.setFont(&fonts::Font4);
    canvas.setCursor(0, 24);
    canvas.printf("CO2:%4d", co2);
    canvas.setCursor(0, 64);
    canvas.printf("HUM:%.1f", hum);
    canvas.setCursor(0, 104);
    canvas.printf("TMP:%.1f", tmp);

    // Push image to display
    canvas.pushSprite(0, 0);
}

class UDCO2S : public EspUsbHostSerial
{
public:
    String value;
    int co2 = 0;
    float hum = 0.0F;
    float tmp = 0.0F;

    UDCO2S() : EspUsbHostSerial(0x04d8, 0xe95a){};

    String toString()
    {
        char bufs[30];
        sprintf(bufs, "CO2=%d,HUM=%.1f,TMP=%.1f", co2, hum, tmp);
        return String(bufs);
    }

    virtual void onNew() override
    {
        Serial.println(("Manufacturer:" + getManufacturer()).c_str());
        Serial.println(("Product:" + getProduct()).c_str());
        // submit((uint8_t *)"STP\r", 4);
        // delay(500);
        submit((uint8_t *)"STA\r", 4);
    }
    virtual void onReceive(const uint8_t *data, const size_t length) override
    {
        for (int i = 0; i < length; i++)
        {
            if (data[i] >= 0x20)
                value += (char)data[i];
            else if (value.length())
            {
                if (sscanf(value.c_str(), "CO2=%d,HUM=%f,TMP=%f", &co2, &hum, &tmp) == 3)
                {
                    // Need correction ?
                    tmp -= 4.5F;
                    // hum = (int)(10.0F * hum * 4.0F / 3.0F + 0.5F) / 10.0F;
                    Serial.println(toString());

                    current_moment = millis();
                    
                    // DATA_DRAW_INTERVAL (ms)ごとに画面描画を更新
                    if ((current_moment - prev_moment_draw) >= DATA_DRAW_INTERVAL)
                    {
                        drawCO2data(co2, hum, tmp);
                        prev_moment_draw = current_moment;
                    }

                    // THING_SPEAK_ITNERVAL (ms)ごとにThingSpeakにアップロード
                    if ((current_moment - prev_moment_thingspeak) >= THING_SPEAK_INTERVAL)
                    {
                        SendSensorData(co2, hum, tmp);
                        prev_moment_thingspeak = current_moment;
                    }

                }
                value.clear();
            }
        }
    }
    virtual void onGone()
    {
        Serial.println("disconnected");
    }
};

UDCO2S usbDev;

void setup()
{

    M5.begin();
    Serial.begin(9600);

    // Setup display and canavs
    M5.Display.setBrightness(100);
    M5.Display.setRotation(1);
    // M5.Display.setColorDepth(24);
    canvas.createSprite(M5.Display.width(), M5.Display.height());

    // Setup Wi-Fi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(200);
        canvas.print('.');
    }
    canvas.println("\nWiFi connected");
    delay(3000);

    // M5.Display.clear();
    usbDev.begin();

    // Draw
    M5.Display.startWrite(); 
    canvas.pushSprite(0, 0);
}

unsigned long lastReconnectTime = 0; // Store the last reconnect time

void loop()
{
    usbDev.task();

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
