
#include <Arduino.h>
#include <SoftwareSerial.h>
#include "protocol.h"

#define SENSOR_PIN 2
#define ButtonPin 3
#define TuruncuLedPin 7
#define YesilLedPin 6
constexpr uint8_t SENSOR_ACTIVE_LEVEL = HIGH;
#define LORA_RX_PIN 10
#define LORA_TX_PIN 11

constexpr unsigned long SEND_INTERVAL_MS = 1000;
constexpr unsigned long RETRY_INTERVAL_MS = 700;

SoftwareSerial lora(LORA_RX_PIN, LORA_TX_PIN);

uint32_t nextSequence = 1;
uint32_t pendingSequence = 0;
bool pending = false;
uint8_t pendingState = 0;
uint8_t BtnState = 0;
uint8_t PompaState = 0;
uint8_t DepoState = 0;
uint8_t MotorState = 0;
uint8_t TermikState = 0;

unsigned long lastSendMs = 0;
unsigned long lastRetryMs = 0;

char rxLine[80];
bool btnMuhur = LOW;
uint8_t readSensorState()
{
    return digitalRead(SENSOR_PIN) == SENSOR_ACTIVE_LEVEL ? 1 : 0;
}

void transmitPending()
{
    char body[40];
    snprintf(body, sizeof(body), "D,%lu,%u",
             (unsigned long)pendingSequence, pendingState);

    sendDataPacket(lora, pendingSequence, pendingState, BtnState, PompaState, DepoState);

    // Yazma işlemi tamamlanana kadar bekle.
    lora.flush();

    lastRetryMs = millis();
}

void setup()
{
    pinMode(SENSOR_PIN, INPUT_PULLUP);
    pinMode(ButtonPin, INPUT_PULLUP);
    pinMode(TuruncuLedPin, OUTPUT);
    pinMode(YesilLedPin, OUTPUT);

    lora.begin(9600);

    delay(500);
}
void loop()
{
    unsigned long now = millis();
    uint8_t sensorState = readSensorState();
    DepoState = sensorState == 1 ? 0 : 1;


    if(!digitalRead(ButtonPin))
        btnMuhur = HIGH;

    // Gelen satirlari ve ACK durumunu goster.
    while (readLine(lora, rxLine, sizeof(rxLine)))
    {

        uint32_t ackSequence;

        if (parseAckPacket(rxLine, ackSequence, BtnState, MotorState, TermikState))
        {

            if (pending && ackSequence == pendingSequence)
            {
                pending = false;
                btnMuhur = LOW;
            }
        }
    }

    BtnState = btnMuhur == HIGH ? 1 : 0;

    if (!pending && now - lastSendMs >= SEND_INTERVAL_MS)
    {
        pendingSequence = nextSequence++;
        pendingState = sensorState;
        pending = true;
        lastSendMs = now;

        transmitPending();
    }
    else if (pending && now - lastRetryMs >= RETRY_INTERVAL_MS)
    {
        transmitPending();
    }

    digitalWrite(YesilLedPin, MotorState == 1 ? HIGH : LOW);
    digitalWrite(TuruncuLedPin, TermikState == 1 ? HIGH : LOW);
}
