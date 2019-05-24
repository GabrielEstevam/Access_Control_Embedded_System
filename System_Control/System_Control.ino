#include <Arduino.h>
#include <LiquidCrystal.h>
#include <string.h>
#include <FS.h>
#include <SPIFFS.h>
#include <SimpleTimer.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#define LED 2

struct InputCapsense {
    const uint8_t INTERRUPT_LINE = 18;
    const uint8_t PINbit1 = 27;
    const uint8_t PINbit2 = 14;
    const uint8_t PINbit3 = 12;
    const uint8_t PINbit4 = 13;
    int value = 0;
    uint32_t numberKeyPresses = 0;
    String password = "";
    bool enter = false;
};

InputCapsense entry;// = {18, 13, 12, 14, 27, 0, 0, false};
const int rs = 4, en = 15, d4 = 26, d5 = 25, d6 = 33, d7 = 32;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
uint8_t lenBase = 4;
String base[] = {"1234", "6548", "6498", "0055"};
uint8_t i, flag = 0;
char ssid[20];
char password[10];
AsyncWebServer server(80);

bool loadConfig();

void IRAM_ATTR isr(void* arg) {
    InputCapsense* s = static_cast<InputCapsense*>(arg);
    uint8_t number = digitalRead(s->PINbit1) + digitalRead(s->PINbit2)*2 + digitalRead(s->PINbit3)*4 + digitalRead(s->PINbit4)*8;
    int8_t digit;
    char password_char[4] = "";
    if (s->enter == false) {
        switch (number) {
            case 0:
              digit = 1;
              break;
            case 1:
              digit = 2;
              break;
            case 2:
              digit = 3;
              break;
            case 4:
              digit = 4;
              break;
            case 5:
              digit = 5;
              break;
            case 6:
              digit = 6;
              break;
            case 8:
              digit = 7;
              break;
            case 9:
              digit = 8;
              break;
            case 10:
              digit = 9;
              break;
            case 13:
              digit = 0;
              break;
            case 15:
              digit = -1;
              break;
            default:
              digit = -2;
              break;
        }
        if (digit != -2) {
            s->numberKeyPresses += 1;
            if (digit == -1) {
                s->value = 0;
                s->numberKeyPresses = 0;
            } else {
                s->value = s->value*10 + digit;
                Serial.println(digit);
                if (s->numberKeyPresses == 4) {
                    Serial.print("Senha:");
                    sprintf(password_char, "%04d", s->value);
                    s->password = password_char;
                    Serial.println(password_char);
                    Serial.println(s->password);
                    s->numberKeyPresses = 0;
                    s->value = 0;
                    s->enter = true;
                }
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(entry.INTERRUPT_LINE, INPUT);
    pinMode(entry.PINbit1, INPUT);
    pinMode(entry.PINbit2, INPUT);
    pinMode(entry.PINbit3, INPUT);
    pinMode(entry.PINbit4, INPUT);
    pinMode(LED, OUTPUT);
    attachInterruptArg(entry.INTERRUPT_LINE, isr, &entry, RISING);
    lcd.begin(16, 2);  
    if(!SPIFFS.begin()){ // inicializa sistema de arquivos
        Serial.println("\n<erro> Falha enquando montava SPIFFS");
    if (!loadConfig())  // carrega as configuraçõe
        Serial.println("\n<erro> Falha ao ler arquivo |config.json|");
    
    /* ===============    CONEXAO WIFI     ============== */
    
    //IPAddress local_IP(192, 168, 1, 184); // Set your Static IP address
    //IPAddress gateway(192, 168, 1, 1); // Set your Gateway IP address
    //IPAddress subnet(255, 255, 0, 0);
    //IPAddress primaryDNS(8, 8, 8, 8);   //optional
    //IPAddress secondaryDNS(8, 8, 4, 4); //optional
    //if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) // Configures static IP address
    //    Serial.println("STA Failed to configure");

    WiFi.mode(WIFI_AP); // WIFI_STA (conecta-se em alguem), WIFI_AP (é um ponto de acesso)
    WiFi.softAP(ssid, password,1,0,1); // SSID max[63], password max[8] - NULL é aberto, channel (1-13), ssid_hidden (0=broadcast SSID, 1=hide SSID), max_connection clientes (1-4)
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    
    // https://techtutorialsx.com/2018/08/24/esp32-web-server-serving-html-from-file-system/
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/test_file.html", "text/html");
    });

    server.on("/test", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "text/html");
    });
 
    server.begin(); // Inicia servidor
}

void loop() {
    if (entry.enter) {
        for (i = 0; i < lenBase; i++) {
            if (entry.password == base[i]) {
                flag = 1;
                break;
            }
        }
        if (flag) {
            lcd.setCursor(0, 0);
            lcd.clear();
            lcd.print("Senha Valida");
            flag = 0;
            digitalWrite(LED, HIGH);
            delay(2000);
            digitalWrite(LED, LOW);
        } else {
            lcd.setCursor(0, 0);
            lcd.clear();
            lcd.print("Senha Invalida");  
        }
        entry.enter = false;
    }
}



/* ===============   FUNCAO LOAD CONFIG   ============== */
bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", FILE_READ);
  
  if (!configFile)
    return false;

  StaticJsonDocument<256> doc;
  
  DeserializationError error = deserializeJson(doc, configFile);
  if (error)
    return false;
  
  JsonObject json = doc.as<JsonObject>();

  strcpy(ssid, json["servername"]);
  strcpy(password, json["password"]);

  configFile.close();

  return true;
}
