# DepoLink

**LoRa ile kablosuz su deposu – pompa kontrol sistemi**

Su deposundaki seviye şamandırası ile uzaktaki pompa arasında kablo çekmeden, Ebyte **E22 LoRa** modülleri üzerinden güvenilir otomatik kontrol. İki kart vardır: **Depo istasyonu** (verici) ve **Pompa istasyonu** (alıcı / röle kontrolü).

## Özellikler

- Sıra numaralı (sequence) paket + **ACK** ve otomatik tekrar gönderme
- XOR **checksum** ile bozuk paket reddi, aynı paket tekrar gelirse komut tekrar uygulanmaz
- **Bağlantı kopma koruması**: 3 sn paket gelmezse pompa otomatikte durur
- **Termik koruma**: NC termik kontağı açılırsa pompa her modda durur, arıza rölesi çeker
- **Manuel mod**: depodaki veya pompadaki butonla aç/kapat, 60 dk sonra otomatik kapanır
- Durum LED'leri her iki istasyonda (motor çalışıyor / termik / depo talebi)

## Donanım

| | Depo istasyonu | Pompa istasyonu |
|---|---|---|
| MCU | Arduino Uno | Arduino Uno |
| Radyo | E22 (UART, 9600) | E22 (UART, 9600) |
| Girişler | D2 seviye sensörü, D3 buton | D2 buton, D3 termik (NC) |
| Çıkışlar | D6 yeşil LED (motor), D7 turuncu LED (termik) | D4 pompa rölesi, D5 arıza rölesi, D6/D7 LED |
| LoRa | D10 ← E22 TXD, D11 → E22 RXD | D10 ← E22 TXD, D11 → E22 RXD |

Röle modülleri **aktif LOW** kabul edilmiştir. Girişler `INPUT_PULLUP`.

## Protokol

```
Veri (Depo → Pompa):  D,sequence,state,btnState,pompaState,depoState,checksum
ACK  (Pompa → Depo):  A,sequence,btnState,motorState,termikState,checksum
checksum = son virgülden önceki metnin XOR'u (ondalık)
```

Depo saniyede bir veri paketi gönderir; ACK gelmezse 700 ms'de bir tekrarlar.

## Derleme / Yükleme (PlatformIO)

```bash
pio run -e istasyon_Depo -t upload
pio run -e istasyon_Pompa -t upload
pio device monitor -b 115200
```

## Klasörler

- `src/` – `main_Depo.cpp`, `main_Pompa.cpp`
- `include/protocol.h` – ortak paket protokolü
- `Orjinal/` – ilk sürüm kodlar (referans)

## Lisans

MIT
