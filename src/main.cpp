#include "M5Unified.h"
#include "WiFi.h"
#include "EspUsbHostSerial.h"
#include "ThingSpeak.h"
#include "config.h" //enter api key and wifi information

unsigned long prev_moment, current_moment;

void SendSensorData(int co2, float temp, float hum)
{
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
                    // tmp -= 4.5F;
                    // hum = (int)(10.0F * hum * 4.0F / 3.0F + 0.5F) / 10.0F;
                    Serial.println(toString());

                    int BG_COLOR;

                    if (co2 <= 1000)
                    {
                        BG_COLOR = BLUE;
                    }
                    else if (co2 <= 1500)
                    {
                        BG_COLOR = GREEN;
                    }
                    else if (co2 <= 2500)
                    {
                        BG_COLOR = ORANGE;
                    }
                    else if (co2 <= 3500)
                    {
                        BG_COLOR = RED;
                    }
                    else
                    {
                        BG_COLOR = PURPLE;
                    }

                    M5.Display.clear(BG_COLOR);
                    M5.Display.setTextColor(WHITE);
                    M5.Display.setTextDatum(middle_left);
                    M5.Display.setFont(&fonts::Font4);
                    M5.Display.setCursor(0, 24);
                    M5.Display.printf("CO2:%4d", co2);
                    M5.Display.setCursor(0, 64);
                    M5.Display.printf("HUM:%.1f", hum);
                    M5.Display.setCursor(0, 104);
                    M5.Display.printf("TMP:%.1f", tmp);

                    // intervalごとにデータ送信
                    current_moment = millis();
                    if ((current_moment - prev_moment) >= THING_SPEAK_INTERVAL)
                    {
                        SendSensorData(co2, tmp, hum);
                        prev_moment = current_moment;
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
    prev_moment = 0; // used for ThingSpeak interval

    M5.begin();
    M5.Display.setRotation(1);
    Serial.begin(9600);

    // Setup Wi-Fi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        M5.Lcd.print('.');
    }
    M5.Lcd.println("\nWiFi connected");
    delay(3000);

    M5.Display.clear();
    usbDev.begin();
}

void loop()
{
    usbDev.task();
}
