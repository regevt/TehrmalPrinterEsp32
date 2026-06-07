#define WEBSOCKETS_MAX_DATA_SIZE 16384
#include "web_server.h"
#include "time_utils.h"

#include <LittleFS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>

extern HardwareSerial ThermalPrinter;
extern void printToThermalPrinter(String text);
const int MAX_ROW_BYTES = 72;
WebServer server(80);

unsigned long lastWifiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 30000;
bool wifiConnected = false;
int wifiReconnectAttempts = 0;
const int MAX_RECONNECT_ATTEMPTS = 5;

size_t incomingBytesTracked = 0;
int expectedContentSize = 0;

WebSocketsServer webSocket = WebSocketsServer(81);
bool isPrintingImage = false;
static int imageByteCounter = 0;
#define STREAM_ROW_BYTES 72
static uint8_t streamBuffer[STREAM_ROW_BYTES];
static bool isStreamingImage = false;
int currentByteOffset = 0;

uint8_t rowBuffer[MAX_ROW_BYTES];

struct Message
{
    String text;
    String timestamp;
};

Message messageHistory[10];
int historyCount = 0;

SemaphoreHandle_t xSemaphore;

void initFileSystem()
{
    if (!LittleFS.begin(true))
    {
        Serial.println("❌ An Error has occurred while mounting LittleFS");
        return;
    }
    Serial.println("📂 LittleFS mounted successfully");
}

void handlePrintImageUploadComplete()
{
    for (int i = 0; i < 6; i++)
    {
        ThermalPrinter.println();
    }
    delay(500);

    const uint8_t cutCmd[] = {0x1B, 0x6D, 0x00};
    ThermalPrinter.write(cutCmd, sizeof(cutCmd));

    server.send(200, "text/plain",
                "Image data streamed and processed successfully.");
}

void handleRawImageStreamChunk()
{
    HTTPUpload &uploadObj = server.upload();

    if (uploadObj.status == UPLOAD_FILE_START)
    {
        incomingBytesTracked = 0;

        ThermalPrinter.write(0x1B);
        ThermalPrinter.write(0x40);
        delay(100);

        Serial.println("📥 Starting raw graphic stream upload pipeline...");
    }
    else if (uploadObj.status == UPLOAD_FILE_WRITE)
    {
        int currentChunkSize = uploadObj.currentSize;
        int processedOffset = 0;

        while (processedOffset + MAX_ROW_BYTES <= currentChunkSize)
        {
            uint8_t rowBuffer[MAX_ROW_BYTES];

            memcpy(rowBuffer, &(uploadObj.buf[processedOffset]), MAX_ROW_BYTES);

            ThermalPrinter.write(0x11);
            ThermalPrinter.write(rowBuffer, MAX_ROW_BYTES);

            processedOffset += MAX_ROW_BYTES;
            incomingBytesTracked += MAX_ROW_BYTES;

            delay(4);
        }
    }
}

void handlePrintImageUpload()
{
    if (server.method() != HTTP_POST)
    {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }

    if (!server.hasArg("imgData"))
    {
        server.send(400, "text/plain", "Error: Missing imgData payload");
        return;
    }

    String rawArrayText = server.arg("imgData");
    int startIdx = rawArrayText.indexOf('[');
    if (startIdx == -1)
    {
        server.send(400, "text/plain", "Error: Invalid payload format");
        return;
    }

    ThermalPrinter.write(0x1B);
    ThermalPrinter.write(0x40);
    delay(100);

    uint8_t rowBuffer[MAX_ROW_BYTES];
    int byteCounter = 0;
    int rowsPrinted = 0;
    String currentByteStr = "";

    for (int i = startIdx; i < rawArrayText.length(); i++)
    {
        char c = rawArrayText.charAt(i);

        if (isDigit(c))
        {
            currentByteStr += c;
        }
        else if (c == ',' || c == ']')
        {
            if (currentByteStr.length() > 0)
            {
                rowBuffer[byteCounter] = (uint8_t)currentByteStr.toInt();
                currentByteStr = "";
                byteCounter++;

                if (byteCounter == MAX_ROW_BYTES)
                {
                    ThermalPrinter.write(0x11);
                    ThermalPrinter.write(rowBuffer, MAX_ROW_BYTES);

                    byteCounter = 0;
                    rowsPrinted++;

                    delay(4);
                }
            }
            if (c == ']')
                break; // Array parsing complete
        }
    }

    for (int i = 0; i < 6; i++)
    {
        ThermalPrinter.println();
    }
    delay(500);

    const uint8_t cutCmd[] = {0x1D, 0x56, 0x00};
    ThermalPrinter.write(cutCmd, sizeof(cutCmd));

    server.send(200, "text/plain",
                "Printed " + String(rowsPrinted) +
                    " graphic rows successfully.");
}

void handleGetHistoryJSON()
{
    String json = "[";

    if (xSemaphoreTake(xSemaphore, portMAX_DELAY))
    {
        int startIndex = (historyCount > 5) ? historyCount - 5 : 0;
        bool firstItem = true;

        for (int i = historyCount - 1; i >= startIndex; i--)
        {
            if (!firstItem)
                json += ",";
            firstItem = false;

            String cleanText = messageHistory[i].text;
            cleanText.replace("\"", "\\\"");

            json += "{";
            json += "\"id\":" + String(historyCount - 1 - i) + ",";
            json += "\"text\":\"" + cleanText + "\",";
            json += "\"time\":\"" +
                    messageHistory[i].timestamp.substring(11, 16) + "\"";
            json += "}";
        }
        xSemaphoreGive(xSemaphore);
    }
    json += "]";
    server.send(200, "application/json; charset=UTF-8", json);
}

void addMessageToHistory(String text, String timestamp)
{
    if (xSemaphoreTake(xSemaphore, portMAX_DELAY))
    {
        Message newMessage;
        newMessage.text = text;
        newMessage.timestamp = timestamp;

        if (historyCount < 10)
        {
            messageHistory[historyCount] = newMessage;
            historyCount++;
        }
        else
        {
            for (int i = 0; i < 9; i++)
            {
                messageHistory[i] = messageHistory[i + 1];
            }
            messageHistory[9] = newMessage;
        }
        xSemaphoreGive(xSemaphore);
    }
}

void handleWifiStatus()
{
    String jsonResponse = "{";
    jsonResponse += "\"connected\":" +
                    String(WiFi.status() == WL_CONNECTED ? "true" : "false") +
                    ",";
    jsonResponse += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    jsonResponse += "\"attempts\":" + String(wifiReconnectAttempts) + ",";
    jsonResponse += "\"timezone\":\"" + String(LOCATION_TIMEZONE) + "\"";
    jsonResponse += "}";

    server.send(200, "application/json", jsonResponse);
}

void handlePrint()
{
    if (server.hasArg("index"))
    {
        int index = server.arg("index").toInt();
        int actualIndex = historyCount - 1 - index;

        if (actualIndex >= 0 && actualIndex < historyCount)
        {
            String messageToPrint;
            if (xSemaphoreTake(xSemaphore, portMAX_DELAY))
            {
                messageToPrint = messageHistory[actualIndex].text;
                xSemaphoreGive(xSemaphore);
            }
            Serial.println("Reprinting message: " + messageToPrint);
            printToThermalPrinter(messageToPrint);
            server.send(200, "text/plain", "Reprint success");
        }
        else
        {
            server.send(400, "text/plain", "Error: Invalid index");
        }
    }
    else
    {
        server.send(400, "text/plain", "Error: Missing index parameter");
    }
}

void handleSubmit()
{
    if (server.hasArg("inputValue"))
    {
        String inputValue = server.arg("inputValue");
        String currentTime = getLocationTime();
        String shortTime = getShortLocationTime();

        Serial.println(shortTime + ": " + inputValue);
        printToThermalPrinter(inputValue);
        addMessageToHistory(inputValue, currentTime);

        server.send(200, "text/plain", "Success");
    }
    else
    {
        server.send(400, "text/plain", "Error: Field not found");
    }
}

void handleNotFound()
{
    String uri = server.uri();

    if (uri.endsWith(".ico") || uri.endsWith(".png") || uri.endsWith(".map"))
    {
        server.send(404, "text/plain", "Not Found");
        return;
    }

    Serial.println("ℹ️ Redirecting background scan from URI: " + uri);
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}
void webServerTask(void *parameter)
{
    for (;;)
    {
        server.handleClient();
        webSocket.loop();
        delay(1);
    }
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload,
                      size_t length)
{
    switch (type)
    {
        case WStype_CONNECTED:
            Serial.printf("[%d] WebSocket Opened\n", num);
            break;

        case WStype_DISCONNECTED:
            Serial.printf("[%d] WebSocket Closed\n", num);
            isStreamingImage = false;
            break;

        case WStype_TEXT:
            if (strcmp((char *)payload, "START_PRINT") == 0)
            {
                isStreamingImage = true;
                imageByteCounter = 0;
                Serial.println("📥 Image stream started...");
            }
            else if (strcmp((char *)payload, "END_PRINT") == 0)
            {
                isStreamingImage = false;
                Serial.println("✨ Image stream finished completely.");
                webSocket.sendTXT(num, "PRINT_SUCCESS");

                for (int i = 0; i < 5; i++)
                {
                    ThermalPrinter.println();
                }

                delay(500);
                const uint8_t cutCmd[] = {0x1B, 0x6D, 0x00};
                ThermalPrinter.write(cutCmd, sizeof(cutCmd));
                delay(200);
            }
            break;

        case WStype_BIN:
            if (!isStreamingImage)
                return;

            for (size_t i = 0; i < length; i++)
            {
                streamBuffer[imageByteCounter] = payload[i];
                imageByteCounter++;

                if (imageByteCounter == STREAM_ROW_BYTES)
                {
                    ThermalPrinter.write(0x11);
                    ThermalPrinter.write(streamBuffer, STREAM_ROW_BYTES);

                    imageByteCounter = 0;
                    delay(4);
                }
            }
            break;

        default:
            break;
    }
}

void initWebServer()
{
    if (xSemaphore == nullptr)
    {
        xSemaphore = xSemaphoreCreateMutex();
    }

    server.serveStatic("/", LittleFS, "/index.html");
    server.serveStatic("/script.js", LittleFS, "/script.js");

    server.on("/submit", HTTP_POST, handleSubmit);
    server.on("/history-data", HTTP_GET, handleGetHistoryJSON);
    server.on("/print", handlePrint);
    server.on("/wifi-status", handleWifiStatus);
    server.onNotFound(handleNotFound);

    webSocket.begin();
    webSocket.onEvent(onWebSocketEvent);

    server.begin();

    Serial.println("🌐 Hybrid HTTP & WebSocket engine initialized.");

    xTaskCreatePinnedToCore(webServerTask, "WebServer", 10000, NULL, 1, NULL,
                            0);
}
