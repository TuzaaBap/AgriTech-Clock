# Development notes

## Memory and rendering

The tested ESP32-S2 has 4 MB external flash and **no PSRAM**. Flash stores program code and cached readings; it does not replace the internal RAM needed by TLS, JSON and HUB75 DMA.

The display uses **4-bit color depth and one DMA buffer**. The previous high-memory display configuration left roughly 44 KB of heap, causing TLS allocation failures and HTTP -1. Sequential HTTPS requests, filtered 4 KB JSON documents and releasing TLS before JSON parsing restored successful requests; observed stable free heap was about 87.8 KB.

The worker task has a 12 KB stack and keeps network work away from the display loop. The display redraws on page changes; clock digits update once a second using opaque text. Do not reintroduce repeated full-screen clears. The configuration is in `include/display_config.h` and the page table is in `src/main.cpp`.

## Clock, cache and network

A generated build epoch seeds certificate validation before NTP becomes available. Keep the build host clock correct. Network time comes from NTP or the HTTPS Date header, with IST display. The seed alone does not mark the visible clock synchronized.

Preferences/NVS namespace `agri-weather` stores complete weather/air snapshots. Once a complete snapshot is available, writes are limited to approximately every 15 minutes. Startup restores cached readings; stale markers depend on their timestamps. This cache survives restart, but does not turn a model forecast into an observation.

TLS verifies the server against ISRG Root X1 in `weather_ca.h`. Check certificate validity and CA changes when maintaining the firmware. Do not use setInsecure().

## AQI calculation

`include/india_aqi.h` is independent C++ and can be tested without hardware. It rounds each 24-hour mean to an integer and interpolates within CPCB bounded bands, then selects the maximum valid subindex. Included pollutants need >=16 hourly samples; at least three valid pollutants, including PM2.5 or PM10, are required. Missing values are excluded. NH3 can be unavailable at this location. Results omit CO, O3 and lead and are explicitly model estimates in documentation.

```sh
c++ -std=c++11 -Wall -Wextra -pedantic -Iinclude tests/test_india_aqi.cpp -o /tmp/agritech-aqi-test
/tmp/agritech-aqi-test
```

Tests cover PM2.5 breakpoint transitions, representative other pollutants, interpolation and missing/insufficient coverage. CI also builds the firmware with placeholder credentials. Integration validation still requires the actual board, Wi-Fi and live APIs.

## Reproducible configuration

PlatformIO 6.2.0, espressif32 platform 6.12.0, Arduino-ESP32 2.0.17 and pinned dependencies match the tested development environment. The HUB75 library is pinned to a commit. Build timestamps and local credentials intentionally make binaries differ between builders.

The public code preserves the deployed UI and networking logic. Packaging adds an automatically generated clock seed and makes the backup port check portable to Windows; it also explicitly preserves ROM mode during backup. Those packaging changes require no new soldering.

Before sharing changes, inspect staged files for credentials, binary images and backups. Keep public issues free of Wi-Fi passwords and device-identifying logs.
