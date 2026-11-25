#pragma once
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

// Paketler:
// D,sequence,state,checksum\n
// A,sequence,checksum\n
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

inline bool parseDataPacket(
    char* line,
    uint32_t& sequence,
    uint8_t& state)
{
    char* lastComma = strrchr(line, ',');
    if (!lastComma)
        return false;

    uint8_t receivedChecksum =
        (uint8_t)strtoul(lastComma + 1, nullptr, 10);

    *lastComma = '\0';

    if (packetChecksum(line) != receivedChecksum)
        return false;

    char* firstComma = strchr(line, ',');
    if (!firstComma)
        return false;

    *firstComma = '\0';

    if (strcmp(line, "D") != 0)
        return false;

    char* secondComma = strchr(firstComma + 1, ',');
    if (!secondComma)
        return false;

    *secondComma = '\0';

    char* seqText = firstComma + 1;
    char* stateText = secondComma + 1;

    if (!*seqText || !*stateText)
        return false;

    if (strcmp(stateText, "0") != 0 &&
        strcmp(stateText, "1") != 0)
        return false;

    sequence = strtoul(seqText, nullptr, 10);
    state = (uint8_t)atoi(stateText);

    return true;
}

inline bool parseAckPacket(char* line, uint32_t& sequence)
{
    char* lastComma = strrchr(line, ',');
    if (!lastComma)
        return false;

    uint8_t receivedChecksum =
        (uint8_t)strtoul(lastComma + 1, nullptr, 10);

    *lastComma = '\0';

    if (packetChecksum(line) != receivedChecksum)
        return false;

    char* comma = strchr(line, ',');
    if (!comma)
        return false;

    *comma = '\0';

    if (strcmp(line, "A") != 0)
        return false;

    sequence = strtoul(comma + 1, nullptr, 10);
    return true;
}

inline void sendDataPacket(
    Stream& port,
    uint32_t sequence,
    uint8_t state)
{
    char body[40];
    snprintf(body, sizeof(body), "D,%lu,%u",
             (unsigned long)sequence, state);

    port.print(body);
    port.print(',');
    port.println((unsigned int)packetChecksum(body));
}

inline void sendAckPacket(Stream& port, uint32_t sequence)
{
    char body[32];
    snprintf(body, sizeof(body), "A,%lu",
             (unsigned long)sequence);

    port.print(body);
    port.print(',');
    port.println((unsigned int)packetChecksum(body));
}
