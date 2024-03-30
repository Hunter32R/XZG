#include <Arduino.h>
#include <ArduinoJson.h>
#include <ETH.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <LittleFS.h>
#include <Update.h>
#include <WebServer.h>
#include <FS.h>
#include <WiFi.h>
#include <Ticker.h>

#include "config.h"
#include "web.h"
#include "log.h"
#include "etc.h"
#include "zb.h"
#include "zones.h"
// #include "hw.h"

#include "webh/PAGE_VPN.html.gz.h"
#include "webh/PAGE_MQTT.html.gz.h"
#include "webh/PAGE_ABOUT.html.gz.h"
#include "webh/PAGE_GENERAL.html.gz.h"
#include "webh/PAGE_LOADER.html.gz.h"
#include "webh/PAGE_ROOT.html.gz.h"
#include "webh/PAGE_SECURITY.html.gz.h"
#include "webh/PAGE_ZIGBEE.html.gz.h"
#include "webh/PAGE_TOOLS.html.gz.h"
#include "webh/PAGE_NETWORK.html.gz.h"
#include "webh/PAGE_LOGOUT.html.gz.h"
#include "webh/bootstrap.min.js.gz.h"
#include "webh/functions.js.gz.h"
#include "webh/jquery-min.js.gz.h"
#include "webh/masonry.js.gz.h"
#include "webh/style.css.gz.h"
#include "webh/icons.svg.gz.h"
#include "webh/logo.svg.gz.h"

// #define HTTP_DOWNLOAD_UNIT_SIZE 3000

// #define HTTP_UPLOAD_BUFLEN 3000

// #define HTTP_MAX_DATA_WAIT 10000 // ms to wait for the client to send the request
// #define HTTP_MAX_POST_WAIT 10000 // ms to wait for POST data to arrive
// #define HTTP_MAX_SEND_WAIT 10000 // ms to wait for data chunk to be ACKed
// #define HTTP_MAX_CLOSE_WAIT 4000 // ms to wait for the client to close the connection

// extern struct ConfigSettingsStruct ConfigSettings;
extern struct zbVerStruct zbVer;
// extern struct MqttSettingsStruct MqttSettings;
// extern struct WgSettingsStruct WgSettings;
extern struct BrdConfigStruct hwConfig;
extern BrdConfigStruct brdConfigs[BOARD_CFG_CNT];

// extern struct CurrentModesStruct modes;

extern struct SystemConfigStruct systemCfg;
extern struct NetworkConfigStruct networkCfg;
extern struct VpnConfigStruct vpnCfg;
extern struct MqttConfigStruct mqttCfg;

extern struct SysVarsStruct vars;

bool wifiWebSetupInProgress = false;
extern const char *coordMode;
extern const char *configFileSystem;
extern const char *configFileWifi;
extern const char *configFileEther;
extern const char *configFileMqtt;
extern const char *configFileWg;
extern const char *configFileGeneral;
extern const char *configFileSecurity;
extern const char *configFileSerial;
// extern const char *deviceModel;
const char *contTypeTextHtml = "text/html";
const char *contTypeTextJs = "text/javascript";
const char *contTypeTextCss = "text/css";
const char *checked = "true";
const char *respHeaderName = "respValuesArr";
const char *respTimeZonesName = "respTimeZones";
const char *contTypeJson = "application/json";
const char *contTypeText = "text/plain";

const char *tempFile = "/config/fw.hex";

bool opened = false;
File fwFile;

extern bool loadFileConfigMqtt();
extern bool loadFileConfigWg();

enum API_PAGE_t : uint8_t
{
    API_PAGE_ROOT,
    API_PAGE_GENERAL,
    API_PAGE_ETHERNET,
    API_PAGE_NETWORK,
    API_PAGE_ZIGBEE,
    API_PAGE_SECURITY,
    API_PAGE_TOOLS,
    API_PAGE_ABOUT,
    API_PAGE_MQTT,
    API_PAGE_VPN
};

WebServer serverWeb(80);

// HTTPClient clientWeb;
WiFiClient eventsClient;

void webServerHandleClient()
{
    serverWeb.handleClient();
}

void checkFwHexTask(void *param)
{
    const char *tempFile = static_cast<const char *>(param);
    checkFwHex(tempFile);

    vTaskDelete(NULL);
}

void initWebServer()
{
    serverWeb.on("/js/bootstrap.min.js", []()
                 { sendGzip(contTypeTextJs, bootstrap_min_js_gz, bootstrap_min_js_gz_len); });
    serverWeb.on("/js/masonry.js", []()
                 { sendGzip(contTypeTextJs, masonry_js_gz, masonry_js_gz_len); });
    serverWeb.on("/js/functions.js", []()
                 { sendGzip(contTypeTextJs, functions_js_gz, functions_js_gz_len); });
    serverWeb.on("/js/jquery-min.js", []()
                 { sendGzip(contTypeTextJs, jquery_min_js_gz, jquery_min_js_gz_len); });
    serverWeb.on("/css/style.css", []()
                 { sendGzip(contTypeTextJs, required_css_gz, required_css_gz_len); });
    serverWeb.on("/logo.svg", []()
                 { sendGzip("image/svg+xml", logo_svg_gz, logo_svg_gz_len); });
    serverWeb.on("/icons.svg", []()
                 { sendGzip("image/svg+xml", icons_svg_gz, icons_svg_gz_len); });
    serverWeb.onNotFound([]()
                         // { serverWeb.send(HTTP_CODE_OK, contTypeText, F("URL NOT FOUND")); }); // handleNotFound);
                         { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/", []()
                 { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/general", []()
                 { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/security", []()
                 { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/network", []()
                 { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/ethernet", []()
                 { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/zigbee", []()
                 { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/about", []()
                 { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/tools", []()
                 { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/mqtt", []()
                 { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/vpn", []()
                 { sendGzip(contTypeTextHtml, PAGE_LOADER_html_gz, PAGE_LOADER_html_gz_len); });
    serverWeb.on("/saveParams", HTTP_POST, handleSaveParams);
    serverWeb.on("/cmdZigRST", handleZigbeeRestart);
    serverWeb.on("/cmdZigBSL", handleZigbeeBSL);
    serverWeb.on("/saveFile", handleSavefile);
    serverWeb.on("/switch/firmware_update/toggle", handleZigbeeBSL); // for cc-2538.py ESPHome edition back compatibility | will be disabled someday
    serverWeb.on("/api", handleApi);
    serverWeb.on("/status", handleStatus);
    serverWeb.on("/logout", []()
                 { 
        serverWeb.sendHeader(F("Content-Encoding"), F("gzip"));
        serverWeb.send_P(401, contTypeTextHtml, (const char *)PAGE_LOGOUT_html_gz, PAGE_LOGOUT_html_gz_len); });

    serverWeb.on("/events", handleEvents);
    /*handling uploading esp32 firmware file */
    serverWeb.on(
        "/update", HTTP_POST, []()
        {
        serverWeb.sendHeader("Connection", "close");
        serverWeb.send(HTTP_CODE_OK, contTypeText, (Update.hasError()) ? "FAIL" : "OK"); },
        []()
        {
            HTTPUpload &upload = serverWeb.upload();
            static long contentLength = 0;
            if (upload.status == UPLOAD_FILE_START)
            {
                if (!checkAuth())
                    return;

                DEBUG_PRINTLN("hostHeader: " + serverWeb.hostHeader());
                // DEBUG_PRINTLN("header Content-Length: " + serverWeb.header("Content-Length"));
                contentLength = serverWeb.header("Content-Length").toInt();
                DEBUG_PRINTLN("contentLength: " + String(contentLength));

                DEBUG_PRINTLN("Update ESP from file " + String(upload.filename.c_str()) + " size: " + String(upload.totalSize));
                DEBUG_PRINTLN("upload.currentSize " + String(upload.currentSize));
                if (!Update.begin(contentLength))
                { // start with max available size
                    Update.printError(Serial);
                }
                Update.onProgress(progressFunc);
            }
            else if (upload.status == UPLOAD_FILE_WRITE)
            {
                /* flashing firmware to ESP*/
                if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
                {
                    Update.printError(Serial);
                }
                // DEBUG_PRINT(".");
                // DEBUG_PRINT(upload.currentSize);
            }
            else if (upload.status == UPLOAD_FILE_END)
            {
                // DEBUG_PRINTLN("Finish...");
                if (Update.end(true))
                { // true to set the size to the current progress
                    // DEBUG_PRINTLN("");
                    // DEBUG_PRINTLN("Update Success: " + upload.totalSize);
                    DEBUG_PRINTLN("Update success. Rebooting...");
                    ESP.restart();
                    // serverWeb.send(HTTP_CODE_OK, contTypeText, (Update.hasError()) ? "FAIL" : "OK");
                }
                else
                {
                    DEBUG_PRINTLN("Update error: ");
                    Update.printError(Serial);
                }
            }
        });

    /*handling uploading zigbee firmware file */
    serverWeb.on(
        "/updateZB", HTTP_POST, []()
        {
            serverWeb.sendHeader("Connection", "close");
            // serverWeb.send(HTTP_CODE_OK, contTypeText, (Update.hasError()) ? "FAIL" : "OK");
            serverWeb.send(HTTP_CODE_OK, contTypeText, "Upload OK. Try to flash...");
            // ESP.restart(); flash zigbee here
        },
        []()
        {
            HTTPUpload &upload = serverWeb.upload();
            if (opened == false)
            {
                // LittleFS.end();
                opened = true;
                DEBUG_PRINTLN("Try to removed file " + String(tempFile));
                if (LittleFS.remove(tempFile))
                {
                    DEBUG_PRINTLN(F("Removed file - OK"));
                }
                else
                {
                    DEBUG_PRINTLN(F("Error while removing file"));
                }
                delay(250);
                fwFile = LittleFS.open(tempFile, FILE_WRITE);

                if (!fwFile)
                {
                    DEBUG_PRINTLN(F("- failed to open file for writing"));
                    return;
                }
            }

            if (upload.status == UPLOAD_FILE_START)
            {
                if (!checkAuth())
                    return;
                DEBUG_PRINTLN(String(millis()) + " Upload zigbee fw file: " + String(upload.filename.c_str()));
                printLogMsg("[ZB_FW] upload: " + String(upload.filename.c_str()));
                // DEBUG_PRINTLN("size: " + String(String(upload.totalSize).c_str()));
                // printLogMsg("size:" + String(String(upload.totalSize).c_str()));
            }
            else if (upload.status == UPLOAD_FILE_WRITE)
            {
                // DEBUG_PRINT(".");
                /*                 //String temp;
                                size_t size = upload.totalSize
                            if (size)
                            {
                                // read up to 128 byte
                                int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
                 */

                // for (int i = 0; i < sizeof(upload.buf); i++)
                //{
                //     char a = upload.buf[i];
                //     Serial.write(a);
                // }
                // DEBUG_PRINTLN(temp);

                fwFile.write(upload.buf, sizeof(upload.buf));
            }
            else if (upload.status == UPLOAD_FILE_END)
            {
                // DEBUG_PRINTLN(F("Finish!"));
                delay(500);
                fwFile.close();
                delay(500);
                DEBUG_PRINTLN(String(millis()) + "UPLOAD_FILE_END");
                printLogMsg("[ZB_FW] upload finish!");
                opened = false;

                DEBUG_PRINTLN("Total file size: " + String(upload.totalSize));

                // checkFwHex(tempFile);
                xTaskCreate(checkFwHexTask,   // Функция задачи
                            "CheckFWHex",     // Имя задачи для удобства отладки
                            8192,             // Размер стека задачи (в байтах)
                            (void *)tempFile, // Параметр, передаваемый в задачу
                            3,                // Приоритет задачи
                            NULL);            // Указатель на задачу (не используется)
            }
        });

    const char *headerkeys[] = {"Content-Length"};
    size_t headerkeyssize = sizeof(headerkeys) / sizeof(char *);
    serverWeb.collectHeaders(headerkeys, headerkeyssize);
    serverWeb.begin();
    DEBUG_PRINTLN(F("webserver setup done"));
}

void handleEvents()
{
    eventsClient = serverWeb.client();
    if (eventsClient)
    { // send events header
        eventsClient.println("HTTP/1.1 200 OK");
        eventsClient.println("Content-Type: text/event-stream;");
        eventsClient.println("Connection: close");
        eventsClient.println("Access-Control-Allow-Origin: *");
        eventsClient.println("Cache-Control: no-cache");
        eventsClient.println();
        eventsClient.flush();
    }
}

void sendEvent(const char *event, const uint8_t evsz, const String data)
{
    if (eventsClient)
    {
        char evnmArr[10 + evsz];
        sprintf(evnmArr, "event: %s\n", event);
        eventsClient.print(evnmArr);
        eventsClient.print(String("data: ") + data + "\n\n");
        // eventsClient.println();
        eventsClient.flush();
    }
}

void sendGzip(const char *contentType, const uint8_t content[], uint16_t contentLen)
{
    serverWeb.sendHeader(F("Content-Encoding"), F("gzip"));
    serverWeb.send_P(HTTP_CODE_OK, contentType, (const char *)content, contentLen);
}

void hex2bin(uint8_t *out, const char *in)
{
    // uint8_t sz = 0;
    while (*in)
    {
        while (*in == ' ')
            in++; // skip spaces
        if (!*in)
            break;
        uint8_t c = *in >= 'a' ? *in - 'a' + 10 : *in >= 'A' ? *in - 'A' + 10
                                                             : *in - '0';
        in++;
        c <<= 4;
        if (!*in)
            break;
        c |= *in >= 'a' ? *in - 'a' + 10 : *in >= 'A' ? *in - 'A' + 10
                                                      : *in - '0';
        in++;
        *out++ = c;
        // sz++;
    }
}

void handleApi()
{ // http://192.168.0.116/api?action=0&page=0
    enum API_ACTION_t : uint8_t
    {
        API_GET_PAGE,
        API_GET_PARAM,
        API_STARTWIFISCAN,
        API_WIFISCANSTATUS,
        API_GET_FILELIST,
        API_GET_FILE,
        API_SEND_HEX,
        API_WIFICONNECTSTAT,
        API_CMD,
        API_GET_LOG,
        API_FLASH_ZB
    };
    const char *action = "action";
    const char *page = "page";
    const char *Authentication = "Authentication";
    const char *param = "param";
    const char *wrongArgs = "wrong args";
    const char *ok = "ok";

    if (systemCfg.webAuth)
    {
        if (!checkAuth())
        { // Authentication
            serverWeb.sendHeader(Authentication, "fail");
            serverWeb.send(401, contTypeText, F("wrong login or password"));
            return;
        }
        else
        {
            serverWeb.sendHeader(Authentication, ok);
        }
    }
    if (serverWeb.argName(0) != action)
    {
        DEBUG_PRINT(F("[handleApi] wrong arg 'action' "));
        DEBUG_PRINTLN(serverWeb.argName(0));
        serverWeb.send(500, contTypeText, wrongArgs);
    }
    else
    {
        const uint8_t action = serverWeb.arg(action).toInt();
        // DEBUG_PRINT(F("[handleApi] arg 0 is: "));
        // DEBUG_PRINTLN(action);
        switch (action)
        {
        case API_FLASH_ZB:
        {
            vars.zbFlashing = 1;
            const char *fwurlArg = "fwurl";
            const uint8_t eventLen = 11;
            const char *tagZB_FW_info = "ZB_FW_info";
            const char *tagZB_FW_err = "ZB_FW_err";
            const char *tagZB_FW_progress = "ZB_FW_prgs";
            if (serverWeb.hasArg(fwurlArg))
            {
                String fwUrl = serverWeb.arg(fwurlArg);
                serverWeb.send(HTTP_CODE_OK, contTypeText, ok);
                uint8_t evWaitCount = 0;
                while (!eventsClient.connected() && evWaitCount < 200)
                { // wait for events
                    webServerHandleClient();
                    delay(25);
                    evWaitCount++;
                }
                // sendEvent(tag, eventLen, String("FW Url: ") + fwUrl);
                // setClock();
                HTTPClient https;
                WiFiClientSecure client;
                client.setInsecure();
                https.begin(client, fwUrl); // https://raw.githubusercontent.com/Tarik2142/devHost/main/coordinator_20211217.bin
                https.addHeader("Content-Type", "application/octet-stream");
                const int16_t httpsCode = https.GET();
                // sendEvent(tag, eventLen, String("REQ result: ") + httpsCode);
                if (httpsCode == HTTP_CODE_OK)
                {
                    const uint32_t fwSize = https.getSize();
                    DEBUG_PRINTLN(F("[start] Downloading firmware..."));
                    sendEvent(tagZB_FW_info, eventLen, "[start]");
                    sendEvent(tagZB_FW_info, eventLen, "Downloading firmware...");
                    const char *tempFile2 = "/config/coordinator.bin";
                    LittleFS.remove(tempFile2);
                    File fwFile = LittleFS.open(tempFile2, "w", 1);
                    uint8_t buff[4];
                    uint32_t downloaded = 0;
                    while (client.readBytes(buff, sizeof(buff)))
                    {
                        downloaded += fwFile.write(buff, sizeof(buff));
                        if (!(downloaded % 8192))
                        {
                            const uint8_t d = ((float)downloaded / fwSize) * 100;
                            sendEvent(tagZB_FW_progress, eventLen, String(d));
                        }
                    }
                    fwFile.close();
                    // in development
                }
                else
                {
                    DEBUG_PRINTLN("REQ error: http_code " + String(httpsCode));
                    serverWeb.send(HTTP_CODE_BAD_REQUEST, contTypeText, String(httpsCode));
                    sendEvent(tagZB_FW_err, eventLen, "REQ error: http_code " + String(httpsCode));
                }
            }
            else
            {
                serverWeb.send(HTTP_CODE_BAD_REQUEST, contTypeText, "missing arg 1");
            }
            vars.zbFlashing = 0;
        }
        break;
        case API_GET_LOG:
        {
            String result;
            result = logPrint();
            serverWeb.send(HTTP_CODE_OK, contTypeText, result);
        }
        break;
        case API_CMD:
        {
            enum CMD_t : uint8_t
            { // cmd list for buttons starts from 0
                CMD_ZB_ROUTER_RECON,
                CMD_ZB_RST,
                CMD_ZB_BSL,
                CMD_ESP_RES,
                CMD_ADAP_LAN,
                CMD_ADAP_USB,
                CMD_LED_PWR_TOG,
                CMD_LED_USB_TOG,
                CMD_CLEAR_LOG,
                CMD_ESP_UPD_URL,
                CMD_ZB_CHK_REV,
                CMD_ZB_CHK_CON,
                CMD_ZB_LED_TOG
            };
            String result = wrongArgs;
            const char *argCmd = "cmd";
            const char *argUrl = "url";
            if (serverWeb.hasArg(argCmd))
            {
                result = "ok";
                switch (serverWeb.arg(argCmd).toInt())
                {
                case CMD_CLEAR_LOG:
                    logClear();
                    break;
                case CMD_ZB_ROUTER_RECON:
                    zigbeeRouterRejoin();
                    break;
                case CMD_ZB_RST:
                    zigbeeRestart();
                    break;
                case CMD_ZB_BSL:
                    zigbeeEnableBSL();
                    break;
                case CMD_ESP_RES:
                    serverWeb.send(HTTP_CODE_OK, contTypeText, result);
                    ESP.restart();
                    break;
                case CMD_ADAP_LAN:
                    adapterModeLAN();
                    break;
                case CMD_ADAP_USB:
                    adapterModeUSB();
                    break;
                case CMD_LED_PWR_TOG:
                    ledPwrToggle();
                    break;
                case CMD_LED_USB_TOG:
                    ledUsbToggle();
                    break;
                case CMD_ESP_UPD_URL:
                    if (serverWeb.hasArg(argUrl))
                        getEspUpdate(serverWeb.arg(argUrl));
                    else
                        getEspUpdate(UPD_FILE);
                    break;
                case CMD_ZB_CHK_REV:
                    getZbVer();
                    break;
                case CMD_ZB_CHK_CON:
                    zbCheck();
                    break;
                case CMD_ZB_LED_TOG:
                    zbLedToggle();
                    break;
                default:
                    break;
                }
                serverWeb.send(HTTP_CODE_OK, contTypeText, result);
            }
        }
        break;
        case API_WIFICONNECTSTAT:
        {
            String result;
            StaticJsonDocument<70> doc;
            const char *connected = "connected";
            if (WiFi.status() == WL_CONNECTED)
            {
                doc[connected] = true;
                doc["ip"] = WiFi.localIP().toString();
            }
            else
            {
                doc[connected] = false;
            }
            serializeJson(doc, result);
            serverWeb.send(HTTP_CODE_OK, contTypeJson, result);
        }
        break;
        case API_SEND_HEX:
        {
            String result = wrongArgs;
            const char *argSize = "size";
            const char *argHex = "hex";
            DEBUG_PRINTLN(F("[send_hex]"));
            if (serverWeb.hasArg(argHex) && serverWeb.hasArg(argSize))
            {
                result = ok;
                DEBUG_PRINTLN(F("try..."));
                DEBUG_PRINTLN(serverWeb.arg(argHex).c_str());
                uint8_t size = serverWeb.arg(argSize).toInt();
                byte resp[size];
                hex2bin(resp, serverWeb.arg(argHex).c_str());

                // Serial2.write(resp, size);
            }
            serverWeb.send(HTTP_CODE_OK, contTypeText, result);
        }
        break;
        case API_GET_FILE:
        {
            String result = wrongArgs;
            const char *argFilename = "filename";
            if (serverWeb.hasArg(argFilename))
            {
                String filename = "/config/" + serverWeb.arg(argFilename);
                File file = LittleFS.open(filename, "r");
                if (!file)
                    return;
                result = "";
                while (file.available())
                {
                    result += (char)file.read();
                }
                file.close();
            }
            serverWeb.send(HTTP_CODE_OK, contTypeText, result);
        }
        break;
        case API_GET_PARAM:
        {
            String resp = wrongArgs;
            if (serverWeb.hasArg(param))
            {
                if (serverWeb.arg(param) == "refreshLogs")
                {
                    resp = (String)systemCfg.refreshLogs;
                }
                else if (serverWeb.arg(param) == "coordMode")
                {
                    if (wifiWebSetupInProgress)
                    {
                        resp = "1";
                    }
                    else
                    {
                        resp = (String)vars.workMode;
                    }
                }
                else if (serverWeb.arg(param) == "zbRev")
                {
                    resp = zbVer.zbRev > 0 ? (String)zbVer.zbRev : "Unknown";
                }
                else if (serverWeb.arg(param) == "espVer")
                {
                    resp = VERSION;
                }
                else if (serverWeb.arg(param) == "wifiEnable")
                {
                    resp = networkCfg.wifiEnable;
                }
                else if (serverWeb.arg(param) == "all")
                {
                    resp = makeJsonConfig(&networkCfg, &vpnCfg, &mqttCfg, &systemCfg);
                }
                else if (serverWeb.arg(param) == "vars")
                {
                    resp = makeJsonConfig(NULL, NULL, NULL, NULL, &vars);
                }
                else if (serverWeb.arg(param) == "root")
                {
                    resp = "";
                    DynamicJsonDocument doc(1024);
                    doc = getRootData();
                    serializeJson(doc, resp);
                }
            }
            serverWeb.send(HTTP_CODE_OK, contTypeText, resp);
        }
        break;
        case API_STARTWIFISCAN:
            if (WiFi.getMode() == WIFI_OFF)
            { // enable wifi for scan
                WiFi.mode(WIFI_STA);
            }
            // } else if (WiFi.getMode() == WIFI_AP) {  // enable sta for scan
            //     WiFi.mode(WIFI_AP_STA);
            // }
            WiFi.scanNetworks(true);
            serverWeb.send(HTTP_CODE_OK, contTypeTextHtml, ok);
            break;
        case API_WIFISCANSTATUS:
        {
            static uint8_t timeout = 0;
            DynamicJsonDocument doc(1024);
            String result = "";
            int16_t scanRes = WiFi.scanComplete();
            const char *scanDone = "scanDone";
            doc[scanDone] = false;
            if (scanRes == -2)
            {
                WiFi.scanNetworks(true);
            }
            else if (scanRes > 0)
            {
                doc[scanDone] = true;
                JsonArray wifi = doc.createNestedArray("wifi");
                for (int i = 0; i < scanRes; ++i)
                {
                    JsonObject wifi_0 = wifi.createNestedObject();
                    wifi_0["ssid"] = WiFi.SSID(i);
                    wifi_0["rssi"] = WiFi.RSSI(i);
                    wifi_0["channel"] = WiFi.channel(i);
                    wifi_0["secure"] = WiFi.encryptionType(i);
                }
                WiFi.scanDelete();
                // if (vars.workMode == WORK_MODE_NETWORK) WiFi.mode(WIFI_OFF); no more wifi scan in lan mode
            }
            if (timeout < 10)
            {
                timeout++;
            }
            else
            {
                doc[scanDone] = true;
                WiFi.scanDelete();
                timeout = 0;
                // if (vars.workMode == WORK_MODE_NETWORK) WiFi.mode(WIFI_OFF);
            }

            serializeJson(doc, result);
            serverWeb.send(HTTP_CODE_OK, contTypeJson, result);
            break;
        }
        case API_GET_PAGE:
            if (!serverWeb.arg(page).length() > 0)
            {
                DEBUG_PRINTLN(F("[handleApi] wrong arg 'page'"));
                DEBUG_PRINTLN(serverWeb.argName(1));
                serverWeb.send(500, contTypeText, wrongArgs);
                return;
            }
            switch (serverWeb.arg(page).toInt())
            {
            case API_PAGE_ROOT:
                handleRoot();
                sendGzip(contTypeTextHtml, PAGE_ROOT_html_gz, PAGE_ROOT_html_gz_len);
                break;
            case API_PAGE_GENERAL:
                handleGeneral();
                sendGzip(contTypeTextHtml, PAGE_GENERAL_html_gz, PAGE_GENERAL_html_gz_len);
                break;
            /*case API_PAGE_ETHERNET:
                handleEther();
                sendGzip(contTypeTextHtml, PAGE_ETHERNET_html_gz, PAGE_ETHERNET_html_gz_len);
                break;*/
            case API_PAGE_NETWORK:
                handleNetwork();
                sendGzip(contTypeTextHtml, PAGE_NETWORK_html_gz, PAGE_NETWORK_html_gz_len);
                break;
            case API_PAGE_ZIGBEE:
                handleSerial();
                sendGzip(contTypeTextHtml, PAGE_ZIGBEE_html_gz, PAGE_ZIGBEE_html_gz_len);
                break;
            case API_PAGE_SECURITY:
                handleSecurity();
                sendGzip(contTypeTextHtml, PAGE_SECURITY_html_gz, PAGE_SECURITY_html_gz_len);
                break;
            case API_PAGE_TOOLS:
                handleSysTools();
                sendGzip(contTypeTextHtml, PAGE_TOOLS_html_gz, PAGE_TOOLS_html_gz_len);
                break;
            case API_PAGE_ABOUT:
                // handleAbout();
                sendGzip(contTypeTextHtml, PAGE_ABOUT_html_gz, PAGE_ABOUT_html_gz_len);
                break;
            case API_PAGE_MQTT:
                handleMqtt();
                sendGzip(contTypeTextHtml, PAGE_MQTT_html_gz, PAGE_MQTT_html_gz_len);
                break;
            case API_PAGE_VPN:
                handleVpn();
                sendGzip(contTypeTextHtml, PAGE_VPN_html_gz, PAGE_VPN_html_gz_len);
                break;
            default:
                break;
            }
            break;
        case API_GET_FILELIST:
        {
            String fileList = "";
            DynamicJsonDocument doc(512);
            JsonArray files = doc.createNestedArray("files");
            File root = LittleFS.open("/config");
            File file = root.openNextFile();
            while (file)
            {
                JsonObject jsonfile = files.createNestedObject();
                jsonfile["filename"] = String(file.name());
                jsonfile["size"] = file.size();
                file = root.openNextFile();
            }
            serializeJson(doc, fileList);
            serverWeb.send(HTTP_CODE_OK, contTypeJson, fileList);
            break;
        }

        default:
            DEBUG_PRINTLN(F("[handleApi] switch (action) err"));
            break;
        }
    }
}

void handleSaveParams()
{
    updateConfiguration(serverWeb, systemCfg, networkCfg, vpnCfg, mqttCfg);
}
/*
String result;
DynamicJsonDocument doc(512);
const char *pageId = "pageId";
const char *on = "on";
File configFile;
const uint8_t one = 1;
const uint8_t zero = 0;
if (serverWeb.hasArg(pageId))
{
    switch (serverWeb.arg(pageId).toInt())
    {
    case API_PAGE_GENERAL:
    {

        configFile = LittleFS.open(configFileGeneral, FILE_READ);
        deserializeJson(doc, configFile);
        configFile.close();

        if (serverWeb.hasArg(coordMode))
        {
            const uint8_t mode = serverWeb.arg(coordMode).toInt();
            if (mode <= 2 && mode >= zero)
            {
                // vars.workMode = static_cast<WORK_MODE_t>(mode);
                if (mode == 1)
                    wifiWebSetupInProgress = true;
                doc[coordMode] = static_cast<WORK_MODE_t>(mode);
            }
        }
        const char *keepWeb = "keepWeb";
        if (serverWeb.arg(keepWeb) == on)
        {
            doc[keepWeb] = one;
        }
        else
        {
            doc[keepWeb] = zero;
        }
        const char *disableLedPwr = "disableLedPwr";
        if (serverWeb.arg(disableLedPwr) == on)
        {
            doc[disableLedPwr] = one;
        }
        else
        {
            doc[disableLedPwr] = zero;
        }
        const char *disableLedUSB = "disableLedUSB";
        if (serverWeb.arg(disableLedUSB) == on)
        {
            doc[disableLedUSB] = one;
        }
        else
        {
            doc[disableLedUSB] = zero;
        }
        configFile = LittleFS.open(configFileGeneral, FILE_WRITE);
        serializeJson(doc, configFile);
        configFile.close();
    }
    break;
    case API_PAGE_ETHERNET:
    {
        configFile = LittleFS.open(configFileEther, FILE_READ);
        deserializeJson(doc, configFile);
        configFile.close();
        doc["ip"] = serverWeb.arg("ipAddress");
        doc["mask"] = serverWeb.arg("ipMask");
        doc["gw"] = serverWeb.arg("ipGW");
        const char *dhcp = "dhcp";
        if (serverWeb.arg(dhcp) == "on")
        {
            doc[dhcp] = one;
        }
        else
        {
            doc[dhcp] = zero;
            if (doc["mask"] == "")
            {
                doc["mask"] = "255.255.255.0";
            }
            if (doc["ip"] == "")
            {
                doc["ip"] = "192.168.0.1";
            }
            if (doc["gw"] == "")
            {
                doc["gw"] = "192.168.0.1";
            }
        }
        // const char* disablePingCtrl = "disablePingCtrl";
        // if (serverWeb.arg(disablePingCtrl) == on) {
        //     doc[disablePingCtrl] = one;
        // } else {
        //     doc[disablePingCtrl] = zero;
        // }
        configFile = LittleFS.open(configFileEther, FILE_WRITE);
        serializeJson(doc, configFile);
        configFile.close();
    }
    case API_PAGE_MQTT:
    {
        configFile = LittleFS.open(configFileMqtt, FILE_READ);
        deserializeJson(doc, configFile);
        configFile.close();
        doc["server"] = serverWeb.arg("MqttServer");
        doc["port"] = serverWeb.arg("MqttPort");
        doc["user"] = serverWeb.arg("MqttUser");
        doc["pass"] = serverWeb.arg("MqttPass");
        doc["topic"] = serverWeb.arg("MqttTopic");
        doc["interval"] = serverWeb.arg("MqttInterval");

        const char *enable = "enable";
        if (serverWeb.arg("MqttEnable") == "on")
        {
            doc[enable] = one;
        }
        else
        {
            doc[enable] = zero;
        }

        const char *discovery = "discovery";
        if (serverWeb.arg("MqttDiscovery") == "on")
        {
            doc[discovery] = one;
        }
        else
        {
            doc[discovery] = zero;
        }

        // const char* disablePingCtrl = "disablePingCtrl";
        // if (serverWeb.arg(disablePingCtrl) == on) {
        //     doc[disablePingCtrl] = one;
        // } else {
        //     doc[disablePingCtrl] = zero;
        // }
        configFile = LittleFS.open(configFileMqtt, FILE_WRITE);
        serializeJson(doc, configFile);
        configFile.close();
        loadFileConfigMqtt();
    }
    break;
    case API_PAGE_VPN:
    {
        configFile = LittleFS.open(configFileWg, FILE_READ);
        deserializeJson(doc, configFile);
        configFile.close();
        doc["localAddr"] = serverWeb.arg("WgLocalAddr");
        doc["localIP"] = serverWeb.arg("WgLocalPrivKey");
        doc["endAddr"] = serverWeb.arg("WgEndAddr");
        doc["endPubKey"] = serverWeb.arg("WgEndPubKey");
        doc["endPort"] = serverWeb.arg("WgEndPort");

        const char *enable = "enable";
        if (serverWeb.arg("WgEnable") == "on")
        {
            doc[enable] = one;
        }
        else
        {
            doc[enable] = zero;
        }

        configFile = LittleFS.open(configFileWg, FILE_WRITE);
        serializeJson(doc, configFile);
        configFile.close();
        loadFileConfigWg();
    }
    break;
    case API_PAGE_NETWORK:
    {
        configFile = LittleFS.open(configFileWifi, FILE_READ);
        deserializeJson(doc, configFile);
        configFile.close();
        doc["ssid"] = serverWeb.arg("WIFISSID");
        doc["pass"] = serverWeb.arg("WIFIpassword");
        const char *dhcpWiFi = "dhcpWiFi";
        if (serverWeb.arg(dhcpWiFi) == on)
        {
            doc[dhcpWiFi] = 1;
        }
        else
        {
            doc[dhcpWiFi] = 0;
        }
        doc["ip"] = serverWeb.arg("ipAddress");
        doc["mask"] = serverWeb.arg("ipMask");
        doc["gw"] = serverWeb.arg("ipGW");
        configFile = LittleFS.open(configFileWifi, FILE_WRITE);
        serializeJson(doc, configFile);
        configFile.close();
        WiFi.persistent(false);
        if (vars.apStarted)
        {
            WiFi.mode(WIFI_AP_STA);
        }
        else
        {
            WiFi.mode(WIFI_STA);
        }
        WiFi.begin(serverWeb.arg("WIFISSID").c_str(), serverWeb.arg("WIFIpassword").c_str());
    }
    break;
    case API_PAGE_ZIGBEE:
    {
        configFile = LittleFS.open(configFileSerial, FILE_READ);
        deserializeJson(doc, configFile);
        configFile.close();
        const char *baud = "baud";
        if (serverWeb.hasArg(baud))
        {
            doc[baud] = serverWeb.arg(baud);
        }
        else
        {
            doc[baud] = 115200;
        }
        const char *port = "port";
        if (serverWeb.hasArg(baud))
        {
            doc[port] = serverWeb.arg(port);
        }
        else
        {
            doc[port] = 6638;
        }
        configFile = LittleFS.open(configFileSerial, FILE_WRITE);
        serializeJson(doc, configFile);
        configFile.close();
    }
    break;
    case API_PAGE_SECURITY:
    {
        configFile = LittleFS.open(configFileSecurity, FILE_READ);
        deserializeJson(doc, configFile);
        configFile.close();
        const char *disableWeb = "disableWeb";
        if (serverWeb.arg(disableWeb) == on)
        {
            doc[disableWeb] = 1;
        }
        else
        {
            doc[disableWeb] = 0;
        }
        const char *webAuth = "webAuth";
        if (serverWeb.arg(webAuth) == on)
        {
            doc[webAuth] = 1;
        }
        else
        {
            doc[webAuth] = 0;
        }
        const char *webUser = "webUser";
        if (serverWeb.arg(webUser) != "")
        {
            doc[webUser] = serverWeb.arg(webUser);
        }
        else
        {
            doc[webUser] = "admin";
        }
        const char *fwEnabled = "fwEnabled";
        if (serverWeb.arg(fwEnabled) == on)
        {
            doc[fwEnabled] = 1;
        }
        else
        {
            doc[fwEnabled] = 0;
        }
        const char *fwIp = "fwIp";
        doc[fwIp] = serverWeb.arg(fwIp);
        doc["webPass"] = serverWeb.arg("webPass");

        configFile = LittleFS.open(configFileSecurity, FILE_WRITE);
        serializeJson(doc, configFile);
        configFile.close();
    }
    break;
    case API_PAGE_TOOLS:
    {
        const char *refreshLogs = "refreshLogs";
        const char *hostname = "hostname";
        configFile = LittleFS.open(configFileGeneral, FILE_READ);
        deserializeJson(doc, configFile);
        configFile.close();
        if (serverWeb.hasArg(refreshLogs))
        {
            systemCfg.refreshLogs = serverWeb.arg(refreshLogs).toInt();
            doc[refreshLogs] = systemCfg.refreshLogs;
        }
        if (serverWeb.hasArg(hostname))
        {
            doc[hostname] = serverWeb.arg(hostname);
            strlcpy(systemCfg.hostname, serverWeb.arg(hostname).c_str(), sizeof(systemCfg.hostname));
        }
        const char *timeZoneName = "timeZoneName";
        if (serverWeb.hasArg(timeZoneName))
        {
            doc[timeZoneName] = serverWeb.arg(timeZoneName);
        }
        configFile = LittleFS.open(configFileGeneral, FILE_WRITE);
        //configFile = LittleFS.open(configFileGeneral, FILE_WRITE);
        serializeJson(doc, configFile);
        configFile.close();
    }
    break;

    default:
        break;
    }
    serverWeb.send(HTTP_CODE_OK, contTypeText, "ok");
}
else
{
    serverWeb.send(500, contTypeText, "bad args");
}
}*/

bool checkAuth()
{
    if (!systemCfg.webAuth)
        return true;
    const char *www_realm = "Login Required";
    if (!serverWeb.authenticate(systemCfg.webUser, systemCfg.webPass))
    {
        serverWeb.requestAuthentication(DIGEST_AUTH, www_realm, "Authentication failed");
        return false;
    }
    else
    {
        return true;
    }
}

void handleGeneral()
{
    DynamicJsonDocument doc(1024);
    String result;

    // doc["pageName"] = "General";
    //  DEBUG_PRINTLN(ConfigSettings.usbMode);
    switch (vars.workMode)
    {
    case WORK_MODE_USB:
        doc["checkedUsbMode"] = checked;
        break;
    /*case COORDINATOR_MODE_WIFI:
        doc["checkedWifiMode"] = checked;
        break;*/
    case WORK_MODE_NETWORK:
        doc["checkedLanMode"] = checked;
        break;

    default:
        break;
    }
    // DEBUG_PRINTLN(systemCfg.disableLedPwr);
    if (systemCfg.keepWeb)
    {
        doc["keepWeb"] = checked;
    }
    if (systemCfg.disableLedPwr)
    {
        doc["checkedDisableLedPwr"] = checked;
    }
    // DEBUG_PRINTLN(systemCfg.disableLedUSB);
    if (systemCfg.disableLedUSB)
    {
        doc["checkedDisableLedUSB"] = checked;
    }
    // serializeJson(doc, result);
    // serverWeb.sendHeader(respHeaderName, result);

    // String result;
    // DynamicJsonDocument doc(512);

    doc["hostname"] = systemCfg.hostname;
    doc["refreshLogs"] = systemCfg.refreshLogs;
    if (systemCfg.timeZone)
    {
        doc["timeZoneName"] = systemCfg.timeZone;
    }
    doc["ntpServ1"] = systemCfg.ntpServ1; //.toString();
    doc["ntpServ2"] = systemCfg.ntpServ2; //.toString();

    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);

    DynamicJsonDocument zones(10240);
    String results;

    JsonArray zonesArray = zones.to<JsonArray>();
    for (int i = 0; i < timeZoneCount; i++)
    {
        zonesArray.add(timeZones[i].zone);
    }

    serializeJson(zones, results);
    serverWeb.sendHeader(respTimeZonesName, results);
}
void handleSecurity()
{
    String result;
    DynamicJsonDocument doc(1024);

    if (systemCfg.disableWeb)
    {
        doc["disableWeb"] = checked;
    }

    if (systemCfg.webAuth)
    {
        doc["webAuth"] = checked;
    }
    doc["webUser"] = (String)systemCfg.webUser;
    doc["webPass"] = (String)systemCfg.webPass;
    if (systemCfg.fwEnabled)
    {
        doc["fwEnabled"] = checked;
    }
    doc["fwIp"] = systemCfg.fwIp; //.toString();

    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);
}

void handleNetwork()
{
    String result;
    DynamicJsonDocument doc(1024);

    // doc["pageName"] = "Config WiFi";

    if (networkCfg.ethEnable)
    {
        doc["ethEnbl"] = checked;
    }
    if (networkCfg.ethDhcp)
    {
        doc["modeEther"] = checked;
    }
    doc["ipEther"] = networkCfg.ethAddr;
    doc["maskEther"] = networkCfg.ethMask;
    doc["GWEther"] = networkCfg.ethGate;

    doc["ethDns1"] = networkCfg.ethDns1;
    doc["ethDns2"] = networkCfg.ethDns2;

    if (networkCfg.wifiEnable)
    {
        doc["wifiEnbl"] = checked;
    }
    doc["ssid"] = String(networkCfg.wifiSsid);
    doc["passWifi"] = String(networkCfg.wifiPassword);
    if (networkCfg.wifiDhcp)
    {
        doc["dchp"] = checked;
    }
    doc["ip"] = networkCfg.wifiAddr;
    doc["mask"] = networkCfg.wifiMask;
    doc["gw"] = networkCfg.wifiGate;
    doc["wifiDns1"] = networkCfg.wifiDns1;
    doc["wifiDns2"] = networkCfg.wifiDns2;

    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);
}

void handleSerial()
{
    String result;
    DynamicJsonDocument doc(1024);

    if (systemCfg.serialSpeed == 9600)
    {
        doc["9600"] = checked;
    }
    else if (systemCfg.serialSpeed == 19200)
    {
        doc["19200"] = checked;
    }
    else if (systemCfg.serialSpeed == 38400)
    {
        doc["8400"] = checked;
    }
    else if (systemCfg.serialSpeed == 57600)
    {
        doc["57600"] = checked;
    }
    else if (systemCfg.serialSpeed == 115200)
    {
        doc["115200"] = checked;
    }
    else
    {
        doc["115200"] = checked;
    }
    doc["socketPort"] = String(systemCfg.socketPort);

    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);
}

void handleEther()
{
    /*
    String result;
    DynamicJsonDocument doc(1024);

    // doc["pageName"] = "Config Ethernet";

    if (networkCfg.ethDhcp)
    {
        doc["modeEther"] = checked;
    }
    doc["ipEther"] = networkCfg.ethAddr;
    doc["maskEther"] = networkCfg.ethMask;
    doc["GWEther"] = networkCfg.ethGate;

    // if (ConfigSettings.disablePingCtrl) {
    //     doc["disablePingCtrl"] = checked;
    // }

    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);
    */
}

void handleMqtt()
{
    String result;
    DynamicJsonDocument doc(1024);

    if (mqttCfg.enable)
    {
        doc["enableMqtt"] = checked;
    }
    doc["serverMqtt"] = mqttCfg.server;
    doc["portMqtt"] = mqttCfg.port;
    doc["userMqtt"] = mqttCfg.user;
    doc["passMqtt"] = mqttCfg.pass;
    doc["topicMqtt"] = mqttCfg.topic;
    doc["intervalMqtt"] = mqttCfg.updateInt;

    if (mqttCfg.discovery)
    {
        doc["discoveryMqtt"] = checked;
    }

    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);
}

void handleVpn()
{
    String result;
    DynamicJsonDocument doc(1024);

    if (vpnCfg.wgEnable)
    {
        doc["enableWg"] = checked;
    }
    doc["localAddrWg"] = vpnCfg.wgLocalIP.toString(); /// WgSettings.localAddr;
    doc["localPrivKeyWg"] = vpnCfg.wgLocalPrivKey;
    doc["endAddrWg"] = vpnCfg.wgEndAddr;
    doc["endPubKeyWg"] = vpnCfg.wgEndPubKey;
    doc["endPortWg"] = vpnCfg.wgEndPort;

    if (vpnCfg.hnEnable)
    {
        doc["enableHn"] = checked;
    }
    doc["joinCodeHn"] = vpnCfg.hnJoinCode;
    doc["hostNameHn"] = vpnCfg.hnHostName;
    doc["dashUrlHn"] = vpnCfg.hnDashUrl;

    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);
}

DynamicJsonDocument getRootData()
{
    DynamicJsonDocument doc(1024);

    char verArr[25];
    const char *env = STRINGIFY(BUILD_ENV_NAME);

    sprintf(verArr, "%s (%s)", VERSION, env);

    doc["VERSION"] = String(verArr);

    String readableTime;
    getReadableTime(readableTime, vars.socketTime);
    const char *connectedSocketStatus = "connectedSocketStatus";
    const char *connectedSocket = "connectedSocket";
    const char *notConnected = "Not connected";
    const char *yes = "Yes";
    const char *no = "No";
    const char *on = "On";
    const char *off = "Off";

    if (vars.connectedClients > 0)
    {
        if (vars.connectedClients > 1)
        {
            doc[connectedSocketStatus] = "Yes, " + String(vars.connectedClients) + "connection";
            doc[connectedSocket] = readableTime;
        }
        else
        {
            doc[connectedSocketStatus] = "Yes, " + String(vars.connectedClients) + " connections";
            doc[connectedSocket] = readableTime;
        }
    }
    else
    {
        doc[connectedSocketStatus] = no;
        doc[connectedSocket] = notConnected;
    }
    const char *operationalMode = "operationalMode";
    switch (vars.workMode)
    {
    case WORK_MODE_USB:
        doc[operationalMode] = "Zigbee-to-USB";
        break;
    /*case COORDINATOR_MODE_WIFI:
        doc[operationalMode] = "Zigbee-to-WiFi";
        break;*/
    case WORK_MODE_NETWORK:
        doc[operationalMode] = "Zigbee-to-Network";
        break;

    default:
        break;
    }

    // ETHERNET TAB
    const char *ethDhcp = "ethDhcp";

    doc[ethDhcp] = networkCfg.ethDhcp ? on : off;

    const char *connectedEther = "connectedEther";
    const char *ethConnection = "ethConnection";
    const char *ethMac = "ethMac";
    const char *ethSpd = "ethSpd";
    const char *ethIp = "ethIp";
    const char *etchMask = "etchMask";
    const char *ethGate = "ethGate";
    const char *ethDns = "ethDns";
    //
    if (networkCfg.ethEnable)
    {
        if (vars.connectedEther)
        {

            doc[connectedEther] = yes;
            doc[ethConnection] = "Connected";
            doc[ethMac] = ETH.macAddress();
            doc[ethSpd] = String(ETH.linkSpeed()) + String(" Mbps");
            doc[ethIp] = ETH.localIP().toString();
            doc[etchMask] = ETH.subnetMask().toString();
            doc[ethGate] = ETH.gatewayIP().toString();
            doc[ethDns] = ETH.dnsIP().toString();
        }
        else
        {
            doc[connectedEther] = no;
            doc[ethConnection] = notConnected;
            doc[ethMac] = notConnected;
            doc[ethSpd] = notConnected;
            doc[ethIp] = notConnected;
            doc[etchMask] = notConnected;
            doc[ethGate] = notConnected;
        }
    }

    getReadableTime(readableTime, 0);
    doc["uptime"] = readableTime;

    float CPUtemp = getCPUtemp();
    doc["deviceTemp"] = String(CPUtemp);
    doc["hwRev"] = hwConfig.board;
    doc["espModel"] = String(ESP.getChipModel());
    doc["espCores"] = String(ESP.getChipCores());
    doc["espFreq"] = String(ESP.getCpuFreqMHz());
    doc["espHeapFree"] = String(ESP.getFreeHeap() / 1024);
    doc["espHeapSize"] = String(ESP.getHeapSize() / 1024);
    if (zbVer.zbRev > 0)
    {
        doc["zigbeeFwRev"] = String(zbVer.zbRev);
    }
    else
    {
        doc["zigbeeFwRev"] = "unknown";
    }
    if (zbVer.chipID)
    {
        doc["zigbeeHwRev"] = String(zbVer.chipID);
    }
    else
    {
        doc["zigbeeHwRev"] = "unknown";
    }

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    const char *espFlashType = "espFlashType";
    if (chip_info.features & CHIP_FEATURE_EMB_FLASH)
    {
        doc[espFlashType] = "embedded";
    }
    else
    {
        doc[espFlashType] = "external";
    }

    doc["espFlashSize"] = String(ESP.getFlashChipSize() / (1024 * 1024));

    // wifi
    const char *wifiSsid = "wifiSsid";
    const char *wifiRssi = "wifiRssi";
    const char *wifiIp = "wifiIp";
    const char *wifiConnected = "wifiConnected";
    const char *wifiMask = "wifiMask";
    const char *wifiGate = "wifiGate";
    const char *wifiEnabled = "wifiEnabled";
    const char *wifiMode = "wifiMode";
    const char *wifiModeAPStatus = "wifiModeAPStatus";
    const char *wifiModeAP = "wifiModeAP";
    const char *wifiDhcp = "wifiDhcp";
    const char *wifiDns = "wifiDns";

    doc["wifiMac"] = String(WiFi.macAddress().c_str());
    // if (vars.workMode == WORK_MODE_NETWORK)
    if (networkCfg.wifiEnable)
    {
        // doc[wifiEnabled] = yes;
        doc[wifiMode] = "Client";
        if (WiFi.status() == WL_CONNECTED)
        { // STA connected
            String rssiWifi = String(WiFi.RSSI()) + String(" dBm");
            doc[wifiSsid] = WiFi.SSID();
            doc[wifiRssi] = rssiWifi;
            doc[wifiConnected] = "Connected to " + WiFi.SSID();
            doc[wifiIp] = WiFi.localIP().toString();
            doc[wifiMask] = WiFi.subnetMask().toString();
            doc[wifiGate] = WiFi.gatewayIP().toString();
            doc[wifiDns] = WiFi.dnsIP().toString();
        }
        else
        {
            const char *connecting = "Connecting...";
            doc["wifiSsid"] = networkCfg.wifiSsid;
            doc[wifiRssi] = connecting;
            doc[wifiConnected] = connecting;
            doc[wifiIp] = networkCfg.wifiDhcp ? WiFi.localIP().toString() : connecting;
            doc[wifiMask] = networkCfg.wifiDhcp ? WiFi.subnetMask().toString() : connecting;
            doc[wifiGate] = networkCfg.wifiDhcp ? WiFi.gatewayIP().toString() : connecting;
        }
        doc[wifiDhcp] = networkCfg.wifiDhcp ? on : off;
    }
    /*else
    {
        doc[wifiEnabled] = no;
        doc[wifiConnected] = notConnected;
        doc[wifiMode] = off;
        doc[wifiSsid] = off;
        doc[wifiIp] = off;
        doc[wifiMask] = off;
        doc[wifiGate] = off;
        doc[wifiRssi] = off;
        doc[wifiDhcp] = off;
    }*/
    if (vars.apStarted)
    { // AP active
        String AP_NameString;
        char apSsid[MAX_DEV_ID_LONG];
        getDeviceID(apSsid);
        char wifiSsidBuf[MAX_DEV_ID_LONG + 25];
        sprintf(wifiSsidBuf, "%s (no password)", apSsid);
        doc[wifiSsid] = wifiSsidBuf;
        doc[wifiIp] = "192.168.1.1 (XZG web interface)";
        doc[wifiMask] = "255.255.255.0 (Access point)";
        doc[wifiGate] = "192.168.1.1 (this device)";
        doc[wifiDhcp] = "On (Access point)";
        doc[wifiModeAPStatus] = "AP started";
        doc[wifiMode] = "AP";
        doc[wifiModeAP] = yes;
        doc[wifiRssi] = "N/A";
    }
    else
    {
        doc[wifiModeAP] = no;
        doc[wifiModeAPStatus] = "Not started";
        // doc[wifiMode] = "Client";
    }

    // MQTT
    if (mqttCfg.enable)
    {
        const char *mqConnect = "mqConnect";
        const char *mqBroker = "mqBroker";

        doc[mqBroker] = mqttCfg.server;

        if (vars.mqttConn)
        {
            doc[mqConnect] = yes;
        }
        else
        {
            doc[mqConnect] = no;
        }
    }

    // VPN WireGuard
    if (vpnCfg.wgEnable)
    {
        const char *wgInit = "wgInit";
        const char *wgDeviceAddr = "wgDeviceAddr";
        const char *wgRemoteAddr = "wgRemoteAddr";
        const char *wgConnect = "wgConnect";
        const char *wgRemoteIP = "wgRemoteIp";
        // const char *wgEndPort = "wgEndPort";

        doc[wgInit] = vars.vpnWgInit ? yes : no;
        doc[wgDeviceAddr] = vpnCfg.wgLocalIP.toString();
        doc[wgRemoteAddr] = vpnCfg.wgEndAddr;
        // doc[wgEndPort] = vpnCfg.wgEndPort;

        doc[wgConnect] = vars.vpnWgConnect ? yes : no;

        doc[wgRemoteIP] = vars.vpnWgPeerIp.toString();
    }
    // VPN Husarnet
    if (vpnCfg.hnEnable)
    {
        const char *hnInit = "hnInit";
        const char *hnHostName = "hnHostName";

        // doc[wgDeviceAddr] = vpnCfg.wgLocalIP.toString();//WgSettings.localAddr;
        doc[hnHostName] = vpnCfg.hnHostName;

        if (vars.vpnHnInit)
        {
            doc[hnInit] = yes;
        }
        else
        {
            doc[hnInit] = no;
        }
    }

    return doc;
}

void handleRoot()
{
    String result;
    DynamicJsonDocument doc(1024);
    doc = getRootData();
    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);
}

void handleStatus()
{
    String result;
    DynamicJsonDocument doc(1024);

    // Authentication is needed for status page as well (if enabled)
    if (systemCfg.webAuth)
    {
        if (!checkAuth())
        {
            serverWeb.sendHeader("Authentication", "fail");
            serverWeb.send(HTTP_CODE_UNAUTHORIZED, contTypeText, F("wrong login or password"));
            return;
        }
        else
        {
            serverWeb.sendHeader("Authentication", "ok");
        }
    }

    doc = getRootData();
    serializeJsonPretty(doc, result);
    serverWeb.send(HTTP_CODE_OK, contTypeJson, result);
}

void handleSysTools()
{

    String result;
    DynamicJsonDocument doc(512);

    // doc["hostname"] = systemCfg.hostname;
    doc["refreshLogs"] = systemCfg.refreshLogs;
    // if (systemCfg.timeZone)
    //{
    //     doc["timeZoneName"] = systemCfg.timeZone;
    // }
    serializeJson(doc, result);
    serverWeb.sendHeader(respHeaderName, result);
    /*
        DynamicJsonDocument zones(10240);
        String results;

        JsonArray zonesArray = zones.to<JsonArray>();
        for (int i = 0; i < timeZoneCount; i++)
        {
            zonesArray.add(timeZones[i].zone);
        }

        serializeJson(zones, results);
        serverWeb.sendHeader(respTimeZonesName, results);
        */
}

void handleSavefile()
{
    if (checkAuth())
    {
        if (serverWeb.method() != HTTP_POST)
        {
            serverWeb.send(405, contTypeText, F("Method Not Allowed"));
        }
        else
        {
            String filename = "/config/" + serverWeb.arg(0);
            String content = serverWeb.arg(1);
            File file = LittleFS.open(filename, "w");
            if (!file)
            {
                DEBUG_PRINT(F("Failed to open file for reading\r\n"));
                return;
            }

            int bytesWritten = file.print(content);

            if (bytesWritten > 0)
            {
                DEBUG_PRINTLN(F("File was written"));
                DEBUG_PRINTLN(bytesWritten);
            }
            else
            {
                DEBUG_PRINTLN(F("File write failed"));
            }

            file.close();
            serverWeb.sendHeader(F("Location"), F("/tools"));
            serverWeb.send(303);
        }
    }
}

void handleZigbeeBSL()
{ // todo move to api
    if (checkAuth())
    {
        zigbeeEnableBSL();
        serverWeb.send(HTTP_CODE_OK, contTypeText, "");
    }
}

void handleZigbeeRestart()
{
    if (checkAuth())
    {
        zigbeeRestart();
        serverWeb.send(HTTP_CODE_OK, contTypeText, "");
    }
}

void printLogTime()
{
    String tmpTime;
    unsigned long timeLog = millis();
    tmpTime = String(timeLog, DEC);
    logPush('[');
    for (int j = 0; j < tmpTime.length(); j++)
    {
        logPush(tmpTime[j]);
    }
    logPush(']');
}

void printLogMsg(String msg)
{
    printLogTime();
    logPush(' ');
    logPush('|');
    logPush(' ');
    for (int j = 0; j < msg.length(); j++)
    {
        logPush(msg[j]);
    }
    logPush('\n');
}

void progressFunc(unsigned int progress, unsigned int total)
{

    const char *tagESP_FW_progress = "ESP_FW_prgs";
    const uint8_t eventLen = 11;

    float percent = ((float)progress / total) * 100.0;

    sendEvent(tagESP_FW_progress, eventLen, String(percent));
    // printLogMsg(String(percent));

#ifdef DEBUG
    if (int(percent) % 5 == 0)
    {
        DEBUG_PRINTLN("Update ESP32 progress: " + String(progress) + " of " + String(total) + " | " + String(percent) + "%");
    }
#endif
};

int totalLength;       // total size of firmware
int currentLength = 0; // current size of written firmware

void getEspUpdate(String esp_fw_url)
{
    DEBUG_PRINTLN("getEspUpdate: " + esp_fw_url);
    // setClock();
    HTTPClient clientWeb;
    WiFiClientSecure client;
    client.setInsecure(); // the magic line, use with caution
    clientWeb.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    clientWeb.begin(client, esp_fw_url);
    clientWeb.addHeader("Content-Type", "application/octet-stream");

    // Get file, just to check if each reachable
    int resp = clientWeb.GET();
    DEBUG_PRINTLN("Response: " + resp);
    // If file is reachable, start downloading
    if (resp == HTTP_CODE_OK)
    {
        // get length of document (is -1 when Server sends no Content-Length header)
        totalLength = clientWeb.getSize();
        // transfer to local variable
        int len = totalLength;
        // this is required to start firmware update process
        Update.begin(totalLength);
        Update.onProgress(progressFunc);
        DEBUG_PRINT("FW Size: ");

        DEBUG_PRINTLN(totalLength);
        // create buffer for read
        uint8_t buff[128] = {0};
        // get tcp stream
        WiFiClient *stream = clientWeb.getStreamPtr();
        // read all data from server
        DEBUG_PRINTLN("Updating firmware...");
        while (clientWeb.connected() && (len > 0 || len == -1))
        {
            // get available data size
            size_t size = stream->available();
            if (size)
            {
                // read up to 128 byte
                int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
                // pass to function tagZB_FW_progress
                runEspUpdateFirmware(buff, c);

                if (len > 0)
                {
                    len -= c;
                }
            }
            // DEBUG_PRINT("Bytes left to flash ");
            // DEBUG_PRINTLN(len);
            // delay(1);
        }
    }
    else
    {
        DEBUG_PRINTLN("Cannot download firmware file.");
    }
    clientWeb.end();
}

void runEspUpdateFirmware(uint8_t *data, size_t len)
{
    Update.write(data, len);
    currentLength += len;

    // if current length of written firmware is not equal to total firmware size, repeat
    if (currentLength != totalLength)
        return;
    // only if currentLength == totalLength
    Update.end(true);
    DEBUG_PRINTLN("Update success. Rebooting...");
    // Restart ESP32 to see changes
    ESP.restart();
}
