#include "secrets.h"
#include "text_utils.h"
#include "time_utils.h"
#include "web_server.h"
#include <Arduino.h>
#include <ESPmDNS.h>
#include <HardwareSerial.h>
#include <WebSocketsServer.h>
#include <WiFi.h>
#include <qrcode.h>

extern WebSocketsServer webSocket;

HardwareSerial ThermalPrinter(1);

#define PRINTER_RX_PIN 2
#define PRINTER_TX_PIN 4

const int MAX_ROW_BYTES = 72;

bool connectToWiFi()
{
    Serial.println("Connecting to WiFi...");
    Serial.print("SSID: ");
    Serial.println(WIFI_SSID);

    WiFi.disconnect(true);
    delay(1000);
    WiFi.mode(WIFI_STA);
    WiFi.hostname("printer");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
        delay(1000);
        Serial.print(".");
        attempts++;
        if (attempts % 10 == 0)
        {
            Serial.println();
        }
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\n✅ Successful connection to WiFi!");

        WiFi.setSleep(true);

        Serial.print("📡 IP address: ");
        Serial.println(WiFi.localIP());

        if (!MDNS.begin("printer"))
        {
            Serial.println("❌ Error setting up MDNS responder!");
        }
        else
        {
            Serial.println("🌐 mDNS responder started. Accessible via "
                           "http://printer.local");
            MDNS.addService("http", "tcp", 80);
        }

        wifiConnected = true;
        wifiReconnectAttempts = 0;

        return true;
    }
    else
    {
        Serial.println("\n❌ Failed to connect to WiFi");
        wifiConnected = false;
        return false;
    }
}

void checkWiFiConnection()
{
    if (millis() - lastWifiCheck > WIFI_CHECK_INTERVAL)
    {
        lastWifiCheck = millis();

        if (WiFi.status() != WL_CONNECTED)
        {
            wifiConnected = false;
            Serial.println("❌ WiFi connection lost");

            if (wifiReconnectAttempts < MAX_RECONNECT_ATTEMPTS)
            {
                wifiReconnectAttempts++;
                Serial.print("🔄 Attempt to reconnect #");
                Serial.println(wifiReconnectAttempts);

                if (connectToWiFi())
                {
                    Serial.println("✅ WiFi connection restored");
                }
                else
                {
                    Serial.println("❌ Failed to restore WiFi connection");
                }
            }
            else
            {
                Serial.println("⚠️ The maximum number of reconnection attempts "
                               "has been reached.");
                Serial.println("🔄 ESP32 rebooting in 10 seconds...");
                delay(10000);
                ESP.restart();
            }
        }
        else
        {
            if (!wifiConnected)
            {
                wifiConnected = true;
                Serial.println("✅ WiFi the connection is active");
            }
        }
    }
}

void initThermalPrinter()
{
    ThermalPrinter.begin(19200, SERIAL_8N1, PRINTER_RX_PIN, PRINTER_TX_PIN,
                         false);
    delay(2000);

    while (ThermalPrinter.available())
    {
        ThermalPrinter.read();
    }

    Serial.println("Thermal printer UART stabilized at 19200 baud");

    const uint8_t initCmd[] = {0x1B, 0x40};
    ThermalPrinter.write(initCmd, sizeof(initCmd));
    delay(500);

    Serial.println("Thermal printer ready");
}

static const int PRINT_LINE_WIDTH = 44;
static const int PRINT_CONTENT_WIDTH = PRINT_LINE_WIDTH - 2;
static const int PRINT_TEXT_PADDING = 2;
static const int PRINT_TEXT_WIDTH =
    PRINT_CONTENT_WIDTH - (PRINT_TEXT_PADDING * 2);

String makeRepeatedCharLine(char character, int count)
{
    String line;
    line.reserve(count);
    for (int i = 0; i < count; i++)
    {
        line += character;
    }
    return line;
}

void printFramedLine(const String &textLine)
{
    String paddedLine = textLine;
    if (paddedLine.length() > PRINT_TEXT_WIDTH)
    {
        paddedLine = paddedLine.substring(0, PRINT_TEXT_WIDTH);
    }

    while (paddedLine.length() < PRINT_TEXT_WIDTH)
    {
        paddedLine += ' ';
    }

    String framedLine = "#";
    framedLine += makeRepeatedCharLine(' ', PRINT_TEXT_PADDING);
    framedLine += paddedLine;
    framedLine += makeRepeatedCharLine(' ', PRINT_TEXT_PADDING);
    framedLine += "#";
    ThermalPrinter.println(framedLine);
}

void printWrappedFramedText(const String &inputText)
{
    String remainingText = inputText;
    remainingText.replace("\r", "");

    int paragraphStart = 0;
    while (paragraphStart <= remainingText.length())
    {
        int paragraphEnd = remainingText.indexOf('\n', paragraphStart);
        String paragraph =
            (paragraphEnd == -1)
                ? remainingText.substring(paragraphStart)
                : remainingText.substring(paragraphStart, paragraphEnd);

        int wordStart = 0;
        String currentLine = "";

        while (wordStart <= paragraph.length())
        {
            while (wordStart < paragraph.length() &&
                   paragraph[wordStart] == ' ')
            {
                wordStart++;
            }

            if (wordStart >= paragraph.length())
            {
                break;
            }

            int wordEnd = paragraph.indexOf(' ', wordStart);
            String word = (wordEnd == -1)
                              ? paragraph.substring(wordStart)
                              : paragraph.substring(wordStart, wordEnd);

            if (word.length() > PRINT_TEXT_WIDTH)
            {
                if (currentLine.length() > 0)
                {
                    printFramedLine(currentLine);
                    currentLine = "";
                }

                int chunkStart = 0;
                while (chunkStart < word.length())
                {
                    String chunk = word.substring(
                        chunkStart,
                        min(chunkStart + PRINT_TEXT_WIDTH, (int)word.length()));
                    printFramedLine(chunk);
                    chunkStart += PRINT_TEXT_WIDTH;
                }
            }
            else
            {
                String candidateLine =
                    currentLine.length() == 0 ? word : currentLine + " " + word;

                if (candidateLine.length() > PRINT_TEXT_WIDTH)
                {
                    if (currentLine.length() > 0)
                    {
                        printFramedLine(currentLine);
                    }
                    currentLine = word;
                }
                else
                {
                    currentLine = candidateLine;
                }
            }

            if (wordEnd == -1)
            {
                break;
            }

            wordStart = wordEnd + 1;
        }

        if (currentLine.length() > 0)
        {
            printFramedLine(currentLine);
        }

        if (paragraph.length() == 0)
        {
            printFramedLine("");
        }

        if (paragraphEnd == -1)
        {
            break;
        }

        paragraphStart = paragraphEnd + 1;
    }
}

void printToThermalPrinter(String text)
{
    String transliteratedText = transliterate(text);

    const uint8_t initCmd[] = {0x1B, 0x40};
    ThermalPrinter.write(initCmd, sizeof(initCmd));
    delay(100);

    Serial.print("Printing safely: ");
    Serial.println(transliteratedText);

    ThermalPrinter.println(makeRepeatedCharLine('#', PRINT_LINE_WIDTH));
    printWrappedFramedText(transliteratedText);
    ThermalPrinter.println(makeRepeatedCharLine('#', PRINT_LINE_WIDTH));

    for (int i = 0; i < 5; i++)
    {
        ThermalPrinter.println();
    }

    delay(500);
    const uint8_t cutCmd[] = {0x1B, 0x6D, 0x00};
    ThermalPrinter.write(cutCmd, sizeof(cutCmd));
    delay(200);
}

void printQRCode(String qrText)
{
    QRCode qrcode;
    uint8_t qrcodeData[qrcode_getBufferSize(4)];
    qrcode_initText(&qrcode, qrcodeData, 4, ECC_LOW, qrText.c_str());

    const int scale = 6;
    int qrWidthPixels = qrcode.size * scale;

    int totalPrinterWidthPixels = MAX_ROW_BYTES * 8;
    int paddingPixelsLeft = (totalPrinterWidthPixels - qrWidthPixels) / 2;
    if (paddingPixelsLeft < 0)
        paddingPixelsLeft = 0;

    const uint8_t initCmd[] = {0x1B, 0x40};
    ThermalPrinter.write(initCmd, sizeof(initCmd));
    delay(100);

    for (int y = 0; y < qrcode.size; y++)
    {
        for (int sY = 0; sY < scale; sY++)
        {

            uint8_t rowBuffer[MAX_ROW_BYTES] = {0};

            for (int x = 0; x < qrWidthPixels; x++)
            {
                int qrX = x / scale;

                if (qrcode_getModule(&qrcode, qrX, y))
                {
                    int targetPixelX = paddingPixelsLeft + x;
                    if (targetPixelX < totalPrinterWidthPixels)
                    {
                        int byteIdx = targetPixelX / 8;
                        int bitIdx = 7 - (targetPixelX % 8);
                        rowBuffer[byteIdx] |= (1 << bitIdx);
                    }
                }
            }

            ThermalPrinter.write(0x11);
            ThermalPrinter.write(rowBuffer, MAX_ROW_BYTES);
            delay(3);
        }
    }

    ThermalPrinter.println("\n" + qrText);

    for (int i = 0; i < 6; i++)
    {
        ThermalPrinter.println();
    }

    delay(500);
    const uint8_t cutCmd[] = {0x1B, 0x6D, 0x00};
    ThermalPrinter.write(cutCmd, sizeof(cutCmd));
    delay(200);
}

void setup()
{
    Serial.begin(115200);

    initThermalPrinter();
    initFileSystem();

    if (!connectToWiFi())
    {
        Serial.println("❌ Failed to connect to WiFi on startup");
    }

    initTimeService();

    initWebServer();

    String startupTime = getShortLocationTime();
    String locationTime = getLocationTime();
    Serial.println(startupTime +
                   ": The system is running. Ready to receive messages.");
    Serial.println(startupTime + ": Current local time: " + locationTime);
}

void loop()
{
    checkWiFiConnection();
    tickTimeService();
    webSocket.loop();
    delay(100);
}
