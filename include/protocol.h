#pragma once

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

// Veri paketi:
// D,sequence,state,btnState,led1State,led2State,checksum
//
// ACK paketi:
// A,sequence,led1State,led2State,checksum
//
// Checksum: Son virgülden önceki metnin XOR toplamı.

inline uint8_t packetChecksum(const char* text)
{
    uint8_t sum = 0;

    while (*text)
        sum ^= (uint8_t)*text++;

    return sum;
}

inline bool readLine(Stream& port, char* buffer, size_t size)
{
    static char line[80];
    static size_t pos = 0;

    while (port.available())
    {
        char c = (char)port.read();

        if (c == '\r')
            continue;

        if (c == '\n')
        {
            line[pos] = '\0';

            strncpy(buffer, line, size);
            buffer[size - 1] = '\0';

            pos = 0;
            return true;
        }

        if (pos < sizeof(line) - 1)
            line[pos++] = c;
        else
            pos = 0;
    }

    return false;
}

// --------------------------------------------------
// Veri paketi ayrıştırma
// D,sequence,state,btnState,led1State,led2State,checksum
// --------------------------------------------------

inline bool parseDataPacket(
    char* line,
    uint32_t& sequence,
    uint8_t& state,
    uint8_t& btnState,
    uint8_t& PompaState,
    uint8_t& DepoState)
{
    char* lastComma = strrchr(line, ',');
    if (!lastComma)
        return false;

    char* checksumText = lastComma + 1;

    if (!*checksumText)
        return false;

    char* endPtr;
    unsigned long checksumValue = strtoul(checksumText, &endPtr, 10);

    if (*endPtr != '\0' || checksumValue > 255)
        return false;

    uint8_t receivedChecksum = (uint8_t)checksumValue;

    *lastComma = '\0';

    if (packetChecksum(line) != receivedChecksum)
        return false;

    char* fields[6];
    fields[0] = line;

    for (uint8_t i = 1; i < 6; i++)
    {
        char* comma = strchr(fields[i - 1], ',');

        if (!comma)
            return false;

        *comma = '\0';
        fields[i] = comma + 1;
    }

    // Tam olarak 6 veri alanı bulunmalı.
    if (strchr(fields[5], ',') != nullptr)
        return false;

    if (strcmp(fields[0], "D") != 0)
        return false;

    for (uint8_t i = 1; i < 6; i++)
    {
        if (!*fields[i])
            return false;
    }

    // Durum alanları yalnızca 0 veya 1 olabilir.
    for (uint8_t i = 2; i <= 5; i++)
    {
        if (strcmp(fields[i], "0") != 0 &&
            strcmp(fields[i], "1") != 0)
            return false;
    }

    // Sayaç alanı yalnızca sayısal olmalı.
    for (char* p = fields[1]; *p; p++)
    {
        if (*p < '0' || *p > '9')
            return false;
    }

    sequence = strtoul(fields[1], nullptr, 10);
    state = (uint8_t)atoi(fields[2]);
    btnState = (uint8_t)atoi(fields[3]);
    PompaState = (uint8_t)atoi(fields[4]);
    DepoState = (uint8_t)atoi(fields[5]);

    return true;
}

// --------------------------------------------------
// ACK paketi ayrıştırma
// A,sequence,led1State,led2State,checksum
// --------------------------------------------------

inline bool parseAckPacket(
    char* line,
    uint32_t& sequence,
    uint8_t& btnState,
    uint8_t& motorState,
    uint8_t& termikState)
{
    char* lastComma = strrchr(line, ',');
    if (!lastComma)
        return false;

    char* checksumText = lastComma + 1;
    if (!*checksumText)
        return false;

    char* endPtr;
    unsigned long checksumValue = strtoul(checksumText, &endPtr, 10);

    if (*endPtr != '\0' || checksumValue > 255)
        return false;

    uint8_t receivedChecksum = (uint8_t)checksumValue;
    *lastComma = '\0';

    if (packetChecksum(line) != receivedChecksum)
        return false;

    char* fields[5];
    fields[0] = line;

    for (uint8_t i = 1; i < 5; i++)
    {
        char* comma = strchr(fields[i - 1], ',');
        if (!comma)
            return false;

        *comma = '\0';
        fields[i] = comma + 1;
    }

    if (strchr(fields[4], ',') != nullptr)
        return false;

    if (strcmp(fields[0], "A") != 0)
        return false;

    for (uint8_t i = 1; i < 5; i++)
    {
        if (!*fields[i])
            return false;
    }

    for (char* p = fields[1]; *p; p++)
    {
        if (*p < '0' || *p > '9')
            return false;
    }

    for (uint8_t i = 2; i <= 4; i++)
    {
        if (strcmp(fields[i], "0") != 0 &&
            strcmp(fields[i], "1") != 0)
            return false;
    }

    sequence = strtoul(fields[1], nullptr, 10);
    btnState = (uint8_t)atoi(fields[2]);
    motorState = (uint8_t)atoi(fields[3]);
    termikState = (uint8_t)atoi(fields[4]);

    return true;
}

// --------------------------------------------------
// Veri paketi gönderme
// --------------------------------------------------

inline void sendDataPacket(
    Stream& port,
    uint32_t sequence,
    uint8_t state,
    uint8_t btnState,
    uint8_t PompaState,
    uint8_t DepoState)
{
    char body[64];

    snprintf(
        body,
        sizeof(body),
        "D,%lu,%u,%u,%u,%u",
        (unsigned long)sequence,
        state,
        btnState,
        PompaState,
        DepoState
    );

    port.print(body);
    port.print(',');
    port.println((unsigned int)packetChecksum(body));
}

// --------------------------------------------------
// ACK gönderme
// --------------------------------------------------

inline void sendAckPacket(
    Stream& port,
    uint32_t sequence,
    uint8_t btnState,
    uint8_t motorState,
    uint8_t termikState)
{
    char body[48];

    snprintf(
        body,
        sizeof(body),
        "A,%lu,%u,%u,%u",
        (unsigned long)sequence,
        btnState,
        motorState,
        termikState
    );

    port.print(body);
    port.print(',');
    port.println((unsigned int)packetChecksum(body));
}