/********************************************************************
 *      Sistema de Controle de Acesso com Autenticacao por Senha
 *******************************************************************/
#include <Arduino.h>
#include <LiquidCrystal.h>
#include <string.h>
#include <FS.h>
#include <SPIFFS.h>
#include <SimpleTimer.h>
#include <WiFi.h>
//#include <WiFiMulti.h>
#include <WiFiAP.h>
#include <WiFiClientSecure.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <bits/stdc++.h>
 
/* Estrutura que define o periferico de entrada */
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

/* Estrutura que define o formato de dados dos usuarios */
struct User {
    String _name;
    String password;
    char valid;
};

/* Variaveis de Definicoes de hardware */
#define LED 2
#define RESET 19

InputCapsense entry; // periferico de entrada 
const int rs = 4, en = 15, d4 = 26, d5 = 25, d6 = 33, d7 = 32; // define pinos do lcd
LiquidCrystal lcd(rs, en, d4, d5, d6, d7); // configura os pinos do lcd

/* Variaveis da conexao wifi */
char ssid[20]; // id da rede wifi
char wifi_password[10]; // senha da rede wifi
AsyncWebServer server(80); // Define a porta do servidor
const char* PARAM_MESSAGE = "message"; // recebe a mensagem passada pela pagina web

/* Variaveis do sistema de arquivo */
#define FORMAT_SPIFFS_IF_FAILED true

/* Variaveis de programa */
User usersData[20]; // vetor de usuarios
int usersCount = 0; // numero de usuarios
uint8_t i, trava = 0; // variaveis auxiliares
String indexS = "", _name = "", password = ""; // variaveis auxiliares
int _index = 0; // variavel auxiliares
bool flagName = true, flagIndex = true; // variaveis auxiliares
bool flag_reset = false; // variavel auxiliar
SimpleTimer timer;
int wd_timer_id; // id do setTimeOut reset
int tempo; // armazena um millis desde o ultimo digito pressionado

/* Definicoes de funcoes */
void IRAM_ATTR isr(void* arg); // Funcao chamada por interrupcao - captura senha inserida 
void notFound(AsyncWebServerRequest *request); // rota default
bool loadConfig(); // carrega arquivo json de configuracao 
void readFileUsers(); // le arquivo de usuarios
void updateFileUsers(); // atualiza arquivo de usuarios
void resetFactory(); // reset system

/* Funcoes das rotas */
String loadUsers(); // rotina para enviar usuarios para a pagina web
void addUser(String msg); // adiciona usuario na base de dados
void editUser(String msg); // edita um usuario na base de dados
void deleteUser(String msg); // apaga um usuario na base de dados

/* Funcoes do sistema de arquivo */
void listDir(fs::FS &fs, const char * dirname, uint8_t levels); // lista diretorios do sistema de arquivo
void readFile(fs::FS &fs, const char * path); // le arquivo do sistema de arquivos
void appendFile(fs::FS &fs, const char * path, const char * message); // adiciona no final do arquivo
void deleteFile(fs::FS &fs, const char * path); // deleta arquivo
void writeFile(fs::FS &fs, const char * path, const char * message); // escreve arquivo

void setup() {
    Serial.begin(115200);
    pinMode(entry.INTERRUPT_LINE, INPUT);
    pinMode(entry.PINbit1, INPUT);
    pinMode(entry.PINbit2, INPUT);
    pinMode(entry.PINbit3, INPUT);
    pinMode(entry.PINbit4, INPUT);
    pinMode(LED, OUTPUT); // RELE
    pinMode(RESET, INPUT_PULLUP);
    
    attachInterruptArg(entry.INTERRUPT_LINE, isr, &entry, RISING);
    lcd.begin(16, 2);  
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Digite a senha");
    
    if(!SPIFFS.begin()) // inicializa sistema de arquivos
        Serial.println("\n<erro> Falha enquando montava SPIFFS");
    if (!loadConfig())  // carrega as configurações
        Serial.println("\n<erro> Falha ao ler arquivo |config.json|");
    
    if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
        Serial.println("SPIFFS Mount Failed");
        return;
    }
    
    listDir(SPIFFS, "/", 0);
    readFileUsers();
    
    /* ===============    CONEXAO WIFI     ============== */
    
    WiFi.mode(WIFI_AP); // WIFI_STA (conecta-se em alguem), WIFI_AP (é um ponto de acesso)
    //Serial.println("SSID_1: "+String(ssid)+" , PASSWORD_1: "+String(wifi_password));
    
    WiFi.softAP(ssid,wifi_password,1,0,1); // SSID max[63], password max[8] - NULL é aberto, channel (1-13), ssid_hidden (0=broadcast SSID, 1=hide SSID), max_connection clientes (1-4)
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    
    // https://techtutorialsx.com/2018/08/24/esp32-web-server-serving-html-from-file-system/
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/index.html", "text/html");
    });

    // Send a GET request to <IP>/loadusers
    server.on("/loadusers", HTTP_GET, [] (AsyncWebServerRequest *request) {
        String msg = loadUsers();
        request->send(200, "text/plain", msg);
    });

    // Send a GET request to <IP>/adduser
    server.on("/adduser", HTTP_GET, [] (AsyncWebServerRequest *request) {
        String msg;
        if (request->hasParam(PARAM_MESSAGE)) {
            msg = request->getParam(PARAM_MESSAGE)->value();
            addUser(msg);
            msg = loadUsers();
            request->send(200, "text/plain", msg);
        }
    });

    // Send a GET request to <IP>/edituser
    server.on("/edituser", HTTP_GET, [] (AsyncWebServerRequest *request) {
        String msg;
        if (request->hasParam(PARAM_MESSAGE)) {
            msg = request->getParam(PARAM_MESSAGE)->value();
            editUser(msg);
            msg = loadUsers();
            request->send(200, "text/plain", msg);
        }
    });

    // Send a GET request to <IP>/deleteuser
    server.on("/deleteuser", HTTP_GET, [] (AsyncWebServerRequest *request) {
        String msg;
        if (request->hasParam(PARAM_MESSAGE)) {
            msg = request->getParam(PARAM_MESSAGE)->value();
            deleteUser(msg);
            msg = loadUsers();
            request->send(200, "text/plain", msg);
        }
    });

       // Send a GET request to <IP>/editadmin
    server.on("/editadmin", HTTP_GET, [] (AsyncWebServerRequest *request) {
        String msg;
        if (request->hasParam(PARAM_MESSAGE)) {
            msg = request->getParam(PARAM_MESSAGE)->value();
            editAdmin(msg);
            request->send(200, "text/plain", "ADMIN ALTERADO COM SUCESSO!!!");
            vTaskDelay(1000);
            ESP.restart();
        }
    });
    
    server.onNotFound(notFound);
    server.begin(); // Inicia servidor
}

void loop() {

  timer.run();
  
    if (entry.enter) {
        for (i = 0; i < usersCount; i++) {
            if (entry.password.toInt() == usersData[i].password.toInt()) {
                trava = 1;
                lcd.setCursor(0,0);
                lcd.clear();
                lcd.print("   Bem Vindo!   ");
                 /*Centraliza o nome do Usuário*/
                if (usersData[i]._name.length() <= 16){
                  lcd.setCursor(7-(usersData[i]._name.length()/2),1);
                }else { 
                  lcd.setCursor(0,1);
                }
                lcd.print(usersData[i]._name);
                break;
            }
        }
        
        if (trava) {
            /*Abrir a trava*/
            digitalWrite(LED, HIGH);
            trava = 0;
        } else {
            lcd.setCursor(0, 0);
            lcd.clear();
            lcd.print("Senha Invalida");  
        }
        delay(2000);
        lcd.clear();
        lcd.print("Digite a senha");
        digitalWrite(LED, LOW);
        entry.enter = false;
    }
    
    if(!digitalRead(RESET)) {
      wd_timer_id = timer.setTimeout(5000, resetFactory);
      flag_reset = true;
    } else {
      timer.restartTimer(wd_timer_id);
      flag_reset = false;
    }
}

void IRAM_ATTR isr(void* arg) {
    InputCapsense* s = static_cast<InputCapsense*>(arg);
    uint8_t number = digitalRead(s->PINbit1) + digitalRead(s->PINbit2)*2 + digitalRead(s->PINbit3)*4 + digitalRead(s->PINbit4)*8;
    int8_t digit;
    char password_char[4] = "";
    
    if(millis() > tempo+10e3) {
        /* Reseta a senha */
        s->numberKeyPresses = 0;
        s->value = 0;
        s->password = "";   
    }
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
    tempo = millis();
}

void resetFactory(){
  if(flag_reset){
    deleteFile(SPIFFS, "/users.txt");
    writeFile(SPIFFS, "/users.txt", "");
    flag_reset = false; 
    deleteFile(SPIFFS, "/config.json");
    ESP.restart();
  }
}
void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "ERROR: PAGE NOT FOUND!");
}

bool editAdmin(String msg){

   _name = "";
    password = "";
    flagName = 1;
    for (i = 0; i < msg.length(); i++) {
        if (flagName) {
            if (msg[i] == ':') {
                flagName = 0;
            } else {
                _name += msg[i];
            }
        } else {
            password += msg[i];
        }
    }
  
  deleteFile(SPIFFS,"/config.json");
  String stringAppendFile = "{\"servername\": \""+String(_name)+"\",\"password\": \""+String(password)+"\"}";
  Serial.println(stringAppendFile);
  char char_array[stringAppendFile.length() + 1];
  strcpy(char_array, stringAppendFile.c_str());
  writeFile(SPIFFS, "/config.json", char_array);

  return true;
  
}

bool loadConfig() {
  
  if (!SPIFFS.exists("/config.json")){
    writeFile(SPIFFS,"/config.json","{\"servername\": \"ServidorTrava\",\"password\": \"12345678\"}");
  }
  
  File configFile = SPIFFS.open("/config.json", FILE_READ);
  if (!configFile){
    return false;
  }
  
  StaticJsonDocument<256> doc;
  
  DeserializationError error = deserializeJson(doc, configFile);
  if (error)
    return false;
  
  JsonObject json = doc.as<JsonObject>();

  strcpy(ssid, json["servername"]);
  strcpy(wifi_password, json["password"]);
  Serial.println("SSID: "+String(ssid)+" , PASSWORD: "+String(wifi_password));
  configFile.close();

  return true;
}

void readFileUsers() {
    usersCount = 0;
    readFile(SPIFFS, "/users.txt");
}

void updateFileUsers() {
    deleteFile(SPIFFS, "/users.txt");
    writeFile(SPIFFS, "/users.txt", "");
    for (i = 0; i < usersCount; i++) {
        if (usersData[i].valid) {
            String line = String(usersData[i]._name) + String (" ") + String(usersData[i].password) + String("\n");
            char char_array[line.length() + 1];
            strcpy(char_array, line.c_str());
            appendFile(SPIFFS, "/users.txt", char_array);
        }
    }
    appendFile(SPIFFS, "/users.txt", "\n");
    readFileUsers();
}

String loadUsers() {
    String msg = "";
    for (i = 0; i < usersCount; i++)
        msg += usersData[i]._name + ":" + usersData[i].password + "&";
    return msg;
}

void addUser(String msg) {
    _name = "";
    password = "";
    flagName = 1;
    for (i = 0; i < msg.length(); i++) {
        if (flagName) {
            if (msg[i] == ':') {
                usersData[usersCount]._name = _name;
                flagName = 0;
            } else {
                _name += msg[i];
            }
        } else {
            password += msg[i];
        }
    }
    usersData[usersCount].password = password;
    usersData[usersCount].valid = 1;
    usersCount++;
    updateFileUsers();
}

void editUser(String msg) {
    indexS = "";
    _name = "";
    password = "";
    flagName = 0;
    flagIndex = 1;
    for (i = 0; i < msg.length(); i++) {
        if (flagIndex) {
            if (msg[i] == ':') {
                flagIndex = 0;
                flagName = 1;
            } else {
                indexS += msg[i];
            }
        } else if (flagName) {
            if (msg[i] == ':') {
                flagName = 0;
            } else {
                _name += msg[i];
            }
        } else {
            password += msg[i];
        }
    }
    _index = indexS.toInt();
    usersData[_index]._name = _name;
    usersData[_index].password = password;
    usersData[_index].valid = 1;
    updateFileUsers();
}

void deleteUser(String msg) {
    _index = msg.toInt();
    usersData[_index].valid = 0;
    updateFileUsers();
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("- failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\r\n", path);

    File file = fs.open(path);
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return;
    }
    
    _name = "";
    password = "";
    flagName = true;
    while (file.available()) {
        char c = file.read();
        if (flagName) {
            if (c == ' ') {
                usersData[usersCount]._name = _name;
                _name = "";
                flagName = false;
            } else {
                _name += c;  
            }
        } else {
            if (c == 10 /* \n */ || c == 0 /* EOF */) {
                usersData[usersCount].password = password;
                usersData[usersCount].valid = 1;
                usersCount++;
                password = "";
                flagName = true;
            } else {
                password += c;  
            }
        }
    }
}

void appendFile(fs::FS &fs, const char * path, const char * message) {
    Serial.printf("Appending to file: %s\r\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("- failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("- message appended");
    } else {
        Serial.println("- append failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\r\n", path);
    if(fs.remove(path)){
        Serial.println("- file deleted");
    } else {
        Serial.println("- delete failed");
    }
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("- failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("- file written");
    } else {
        Serial.println("- frite failed");
    }
}
