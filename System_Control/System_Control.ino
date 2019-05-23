#include <Arduino.h>
#include <LiquidCrystal.h>

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
