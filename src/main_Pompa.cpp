#include <Arduino.h>
#include <SoftwareSerial.h>
#include "protocol.h"

constexpr uint8_t BUTTON_PIN = 2;
constexpr uint8_t THERMAL_PIN = 3;
constexpr uint8_t PUMP_RELAY_PIN = 4;
constexpr uint8_t FAULT_RELAY_PIN = 5;
#define YESIL_LED_PIN 6
constexpr uint8_t TURUNCU_LED_PIN = 7;

constexpr uint8_t LORA_RX_PIN = 10; // E22 TXD -> Uno D10
constexpr uint8_t LORA_TX_PIN = 11; // Uno D11 -> E22 RXD

// Aktif LOW röle modülü:
constexpr uint8_t RELAY_ON = LOW;
constexpr uint8_t RELAY_OFF = HIGH;

constexpr unsigned long LINK_TIMEOUT_MS = 3000UL;
constexpr unsigned long DEBOUNCE_MS = 40UL;
constexpr unsigned long MANUAL_TIMEOUT_MS = 60UL * 60UL * 1000UL;

SoftwareSerial lora(LORA_RX_PIN, LORA_TX_PIN);
char rxLine[80];

uint8_t BtnState = 0;
uint8_t PompaState = 0;
uint8_t DepoState = 0;
uint8_t MotorState = 0;
uint8_t TermikState = 0;

uint32_t lastSequence = 0;
bool haveSequence = false;

bool autoDemand = false;
bool manualMode = false;
bool linkAlive = false;
bool thermalHealthy = false;
bool pumpOn = false;

unsigned long lastFreshPacketMs = 0;
unsigned long lastButtonChangeMs = 0;
unsigned long manualStartMs = 0;

bool lastRawButton = HIGH;
bool stableButton = HIGH;

void setManualMode(bool enabled)
{
    // Zamanı yalnızca manuel moda ilk geçişte başlat.
    if (enabled && !manualMode)
        manualStartMs = millis();

    manualMode = enabled;
}

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

    digitalWrite(PUMP_RELAY_PIN, pumpOn ? RELAY_ON : RELAY_OFF);
    digitalWrite(FAULT_RELAY_PIN, thermalHealthy ? RELAY_OFF : RELAY_ON);
}

void sendAck(uint32_t sequence)
{
    // ACK yalnızca komutun onaylandığını bildirir; komut alanı sıfırlanır.
    updateOutputs();
    MotorState = pumpOn ? 1 : 0;
    TermikState = thermalHealthy ? 1 : 0;
    sendAckPacket(lora, sequence, 0, MotorState, TermikState);
}

void processRadio()
{
    while (readLine(lora, rxLine, sizeof(rxLine)))
    {
        uint32_t sequence;
        uint8_t state;
        uint8_t rxBtnState;
        uint8_t rxPompaState;
        uint8_t rxDepoState;

        if (!parseDataPacket(rxLine, sequence, state,
                             rxBtnState, rxPompaState, rxDepoState))
        {
            continue;
        }

        // Her geçerli paket bağlantının canlı olduğunu gösterir.
        lastFreshPacketMs = millis();
        linkAlive = true;

        // Depodan gelen durumları güncelle.
        DepoState = rxDepoState;
        PompaState = rxPompaState;

        // Aynı sequence yeniden geldiyse komutu tekrar çalıştırma.
        // Ancak depo ACK alamamış olabileceği için tekrar ACK gönder.
        if (haveSequence && sequence == lastSequence)
        {
            sendAck(sequence);
            continue;
        }

        // Yeni paket.
        lastSequence = sequence;
        haveSequence = true;
        autoDemand = (state == 1);

        // Buton komutu yalnızca yeni sequence için bir kez uygulanır.
        if (rxBtnState == 1 && thermalHealthy)
            setManualMode(!manualMode);

        // Termik arızasında manuel mod kapalı kalmalı.
        if (!thermalHealthy)
            setManualMode(false);

        sendAck(sequence);
    }
}

void setup()
{
    // Röle çıkışlarını OFF yapıp sonra OUTPUT moduna geçir.
    digitalWrite(PUMP_RELAY_PIN, RELAY_OFF);
    digitalWrite(FAULT_RELAY_PIN, RELAY_OFF);

    pinMode(PUMP_RELAY_PIN, OUTPUT);
    pinMode(FAULT_RELAY_PIN, OUTPUT);
    pinMode(YESIL_LED_PIN, OUTPUT);
    pinMode(TURUNCU_LED_PIN, OUTPUT);

    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(THERMAL_PIN, INPUT_PULLUP);

    lora.begin(9600);

    lastRawButton = digitalRead(BUTTON_PIN);
    stableButton = lastRawButton;
    lastButtonChangeMs = millis();

    thermalHealthy = !digitalRead(THERMAL_PIN);
    updateOutputs();
}

void loop()
{
    // NC termik kontağı kapalı = LOW = termik sağlıklı.
    thermalHealthy = !digitalRead(THERMAL_PIN);
    if (!thermalHealthy)
        setManualMode(false);

    processRadio();

    // Radyo işlemi millis() değerini ilerletebileceği için zamanı sonra al.
    unsigned long now = millis();

    // Fiziksel buton: debounce ve basma anında tek tetikleme.
    bool rawButton = digitalRead(BUTTON_PIN);
    if (rawButton != lastRawButton)
    {
        lastRawButton = rawButton;
        lastButtonChangeMs = now;
    }

    if ((now - lastButtonChangeMs) >= DEBOUNCE_MS && rawButton != stableButton)
    {
        stableButton = rawButton;

        // INPUT_PULLUP: HIGH -> LOW, yani basma anı.
        if (stableButton == LOW && thermalHealthy)
            setManualMode(!manualMode);
    }

    // Manuel mod 60 dakika sonra otomatik olarak kapanır.
    if (manualMode && (now - manualStartMs >= MANUAL_TIMEOUT_MS))
        setManualMode(false);

    // Termik arızası manuel modu da iptal eder.
    if (!thermalHealthy)
        setManualMode(false);

    linkAlive = haveSequence && (now - lastFreshPacketMs < LINK_TIMEOUT_MS);

    updateOutputs();

    // Yeşil: pompa gerçekten çalışıyor. Turuncu: bağlantı canlı ve depo talebi aktif.
    digitalWrite(YESIL_LED_PIN, digitalRead(PUMP_RELAY_PIN)== HIGH ? HIGH : LOW);
    digitalWrite(TURUNCU_LED_PIN, DepoState == 0 ? HIGH : LOW);
}
