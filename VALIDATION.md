# Validation record

Public package checked on 4 November 2026.

- PlatformIO 6.2.0 build: **PASS**, espressif32 6.12.0, ESP32-S2 Saola target, Arduino framework 2.0.17.
- Built with placeholder credentials copied from secrets.h.example; no real network password used in the public build.
- Static RAM reported: 57,020 / 327,680 bytes (17.4%). This excludes dynamic display/TLS allocations.
- Application flash reported: 949,522 / 1,310,720 bytes (72.4%).
- Native C++ AQI checks: **PASS** with C++11, -Wall -Wextra -pedantic.
- Working firmware's src/main.cpp, display_config.h, india_aqi.h and weather_ca.h preserved byte-for-byte from the deployed project.
- Packaging-only changes: generated build-time clock seed, public credential template, portable backup port handling and explicit no_reset before backup read.
- The public package was compiled, not flashed over the working installation during publication preparation.

Earlier on-device validation observed successful weather/air API parsing and approximately 87.8 KB free heap after the display memory fix. The owner confirmed stable display operation. The most recent installation upload passed esptool hash verification.

No complete original stock-flash backup exists in this project. Hardware solder-pad signal assignments inferred from photographs are not electrically verified; use continuity measurements before wiring.

Git-tracked files were scanned against the installation Wi-Fi password before the initial commit. Compiled files, private credentials, backups and runtime logs are excluded from the repository.
