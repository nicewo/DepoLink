#include <Arduino.h>
#include <SoftwareSerial.h>
#include "protocol.h"

constexpr uint8_t BUTTON_PIN = 2;
constexpr uint8_t THERMAL_PIN = 3;
constexpr uint8_t PUMP_RELAY_PIN = 4;
constexpr uint8_t FAULT_RELAY_PIN = 5;

constexpr uint8_t LORA_RX_PIN = 10; // E22 TXD -> Uno D10
constexpr uint8_t LORA_TX_PIN = 11; // Uno D11 -> E22 RXD

// Aktif LOW röle modülü:
constexpr uint8_t RELAY_ON = LOW;
constexpr uint8_t RELAY_OFF = HIGH;

constexpr unsigned long LINK_TIMEOUT_MS = 3000;
constexpr unsigned long DEBOUNCE_MS = 40;

SoftwareSerial lora(LORA_RX_PIN, LORA_TX_PIN);

char rxLine[80];

uint32_t lastSequence = 0;
bool haveSequence = false;

bool autoDemand = false;
bool manualMode = false;
bool linkAlive = false;
bool thermalHealthy = false;
bool pumpOn = false;

unsigned long lastFreshPacketMs = 0;
unsigned long lastButtonChangeMs = 0;

bool lastRawButton = HIGH;
bool stableButton = HIGH;

void updateOutputs()
{
    // Termik arızası bütün çalışma modlarından üstündür.
    bool desiredPump = false;

    if (thermalHealthy)
    {
        if (manualMode)
        {
            // Manuel modda radyo bağlantısı ve sensör aranmaz.
            desiredPump = true;
        }
        else
        {
            // Otomatik çalışma için hem bağlantı hem sensör talebi gerekir.
            desiredPump = linkAlive && autoDemand;
        }
    }

    pumpOn = desiredPump;

    digitalWrite(PUMP_RELAY_PIN,
                 pumpOn ? RELAY_ON : RELAY_OFF);

    digitalWrite(FAULT_RELAY_PIN,
                 thermalHealthy ? RELAY_OFF : RELAY_ON);
}

void processRadio()
{
    while (readLine(lora, rxLine, sizeof(rxLine)))
    {
        //Serial.print("GELEN HAM PAKET: [");
        //Serial.print(rxLine);
        //Serial.println("]");
        uint32_t sequence;
        uint8_t state;

        if (!parseDataPacket(rxLine, sequence, state))
        {
            //Serial.println("PAKET GECERSIZ");
            continue;
        }
        // Geçerli her pakete ACK gönderilir.
        // ACK kaybolursa aynı paket yeniden geldiğinde de onaylanır.
        sendAckPacket(lora, sequence);
        // Tekrar gelen paketler de radyo baglantisinin canli oldugunu gosterir.
        lastFreshPacketMs = millis();

        // Aynı sequence yeniden geldiyse yeni komut sayma.
        if (haveSequence && sequence == lastSequence)
            continue;

        // Yeni paket: sensör talebini ve bağlantı zamanını güncelle.
        lastSequence = sequence;
        haveSequence = true;

        autoDemand = state == 1;

        //Serial.print("Yeni paket: seq=");
        //Serial.print(sequence);
        //Serial.print(" sensör=");
        //Serial.println(autoDemand ? "AKTIF" : "PASIF");
    }
}

void processButton()
{
    unsigned long now = millis();
    bool raw = digitalRead(BUTTON_PIN);

    if (raw != lastRawButton)
    {
        lastRawButton = raw;
        lastButtonChangeMs = now;
    }

    if (now - lastButtonChangeMs >= DEBOUNCE_MS &&
        raw != stableButton)
    {
        stableButton = raw;

        // Butonun basılma anı: HIGH -> LOW
        if (stableButton == LOW)
        {
            if (thermalHealthy)
            {
                manualMode = !manualMode;

                //Serial.println(manualMode
                                  // ? "MANUEL MOD: ACIK"
                                  // : "MANUEL MOD: KAPALI");
            }
            else
            {
                //Serial.println(
                  //  "Termik arizasi var: manuel mod acilamaz.");
            }
        }
    }
}

void setup()
{
    // Önce çıkış mandalını OFF yap, ardından çıkış moduna geç.
    digitalWrite(PUMP_RELAY_PIN, RELAY_OFF);
    digitalWrite(FAULT_RELAY_PIN, RELAY_OFF);

    pinMode(PUMP_RELAY_PIN, OUTPUT);
    pinMode(FAULT_RELAY_PIN, OUTPUT);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(THERMAL_PIN, INPUT_PULLUP);

    //Serial.begin(115200);
    lora.begin(9600);

    lastRawButton = digitalRead(BUTTON_PIN);
    stableButton = lastRawButton;

    //Serial.println("Istasyon B basladi.");
}

void loop()
{
    unsigned long now = millis();

    processRadio();

    // NC termik kontağı kapalı = LOW = termik sağlıklı.
    thermalHealthy = digitalRead(THERMAL_PIN) == LOW;

    if (!thermalHealthy)
    {
        // Termik atınca manuel modu da iptal et.
        manualMode = false;
    }

    linkAlive =
        haveSequence &&
        (now - lastFreshPacketMs < LINK_TIMEOUT_MS);

    processButton();
    updateOutputs();

    static bool previousLink = false;
    if (linkAlive != previousLink)
    {
        previousLink = linkAlive;
        //Serial.println(linkAlive
                          // ? "RADYO BAGLANTISI VAR"
                          // : "RADYO BAGLANTISI KAYIP");
    }

    static bool previousThermalHealthy = true;
    if (thermalHealthy != previousThermalHealthy)
    {
        previousThermalHealthy = thermalHealthy;
        //Serial.println(thermalHealthy
                          // ? "Termik normale dondu."
                          // : "TERMIK ARIZASI! POMPA DURDURULDU.");
    }

    static bool previousPumpOn = false;
    if (pumpOn != previousPumpOn)
    {
        previousPumpOn = pumpOn;
        //Serial.println(pumpOn
                          // ? "POMPA KOMUTU: CALIS"
                          // : "POMPA KOMUTU: DUR");
    }
}
