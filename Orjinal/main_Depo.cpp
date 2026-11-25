
#include <Arduino.h>
#include <SoftwareSerial.h>
#include "protocol.h"

#define SENSOR_PIN  2
constexpr uint8_t SENSOR_ACTIVE_LEVEL = HIGH;
#define LORA_RX_PIN  10
#define LORA_TX_PIN  11

constexpr unsigned long SEND_INTERVAL_MS = 1000;
constexpr unsigned long RETRY_INTERVAL_MS = 700;

SoftwareSerial lora(LORA_RX_PIN, LORA_TX_PIN);

uint32_t nextSequence = 1;
uint32_t pendingSequence = 0;
bool pending = false;
uint8_t pendingState = 0;

unsigned long lastSendMs = 0;
unsigned long lastRetryMs = 0;

char rxLine[80];

uint8_t readSensorState()
{
    return digitalRead(SENSOR_PIN) == SENSOR_ACTIVE_LEVEL ? 1 : 0;
}

void transmitPending()
{
    char body[40];
    snprintf(body, sizeof(body), "D,%lu,%u",
             (unsigned long)pendingSequence, pendingState);

    //Serial.print("[TX] Paket: ");
    //Serial.println(body);

    //Serial.print("[TX] XOR checksum: ");
    //Serial.println((unsigned int)packetChecksum(body));

    //Serial.print("[TX] LoRa satiri: ");
    //Serial.print(body);
    //Serial.print(',');
    //Serial.println((unsigned int)packetChecksum(body));

    sendDataPacket(lora, pendingSequence, pendingState);

    // Yazma işlemi tamamlanana kadar bekle.
    lora.flush();

    lastRetryMs = millis();
    //Serial.println("[TX] UART yazma tamamlandi.");
}

void setup()
{
    pinMode(SENSOR_PIN, INPUT_PULLUP);

    //Serial.begin(115200);
    lora.begin(9600);

    //Serial.println();
    //Serial.println("=== ISTASYON A DEBUG ===");
    //Serial.println("USB: 115200 baud");
    //Serial.println("LoRa UART: 9600 baud");
    //Serial.println("D10=RX, D11=TX");

    delay(500);
}
void loop()
{
    unsigned long now = millis();
    uint8_t sensorState = readSensorState();


    // Gelen satirlari ve ACK durumunu goster.
    while (readLine(lora, rxLine, sizeof(rxLine)))
    {

        uint32_t ackSequence;

        if (parseAckPacket(rxLine, ackSequence))
        {
           // Serial.print("[RX] Gecerli ACK: ");
           // Serial.println(ackSequence);

            if (pending && ackSequence == pendingSequence)
            {
                pending = false;
                //Serial.println("[ACK] Bekleyen paket onaylandi.");
            }
        }
        else
        {
           // Serial.println("[RX] ACK formati gecersiz.");
        }
    }

    if (!pending && now - lastSendMs >= SEND_INTERVAL_MS)
    {
        pendingSequence = nextSequence++;
        pendingState = sensorState;
        pending = true;
        lastSendMs = now;

        //Serial.print("[SENSOR] D2 = ");
        //Serial.print(digitalRead(SENSOR_PIN));
        //Serial.print(" | durum = ");
        //Serial.println(pendingState);

        transmitPending();
    }
    else if (pending && now - lastRetryMs >= RETRY_INTERVAL_MS)
    {
        //Serial.println("[RETRY] ACK gelmedi, ayni paket tekrar gonderiliyor.");
        transmitPending();
               // Serial.print("[SENSOR] D2 = ");
        //Serial.print(digitalRead(SENSOR_PIN));
       // Serial.print(" | durum = ");
       // Serial.println(pendingState);
    }
}
