# Troubleshooting

| Symptom | Check / explanation |
| --- | --- |
| No USB port | WF1 Micro-USB is power-only. Check dock power/connection, data cable, USB-A wiring and ground. Inspect for solder shorts with everything unpowered. |
| USB adapter appears but no ESP32 | VID 067B/PID 2303 is a Prolific USB-UART adapter, not native raw USB. Connect its UART pins correctly; do not wire TX/RX to USB D+/D−. |
| Blank panel after upload | Remove P13/H2 bridge and fully power-cycle both USB and external power. ROM download mode does not run the display. |
| Upload hangs or fails partway | Close other monitors, re-enter ROM manually, use a stable direct data connection, then repeat a complete upload. Verify hashes. |
| Correct time but HTTP -1 / no weather | NTP can work while HTTPS fails. Read serial TLS details. The original issue was insufficient free RAM, not absent internet time. Do not disable TLS verification as a workaround. |
| WX=OK but AIR fails | Endpoints are separate. Check serial HTTP/JSON status and network access to air-quality-api.open-meteo.com. Pollutant gaps can prevent AQI even when other values exist. |
| Values show -- | No valid value has arrived. Check Wi-Fi, endpoint response, time, available samples and serial diagnostics. |
| A value has * | Cached data is stale: weather >30 minutes, air >2 hours. |
| AQI says 401+ | A pollutant exceeds the highest bounded concentration; this is a Severe lower bound. |
| Humidity says % RH | Relative humidity, expressed as a percentage. |
| Brief dim/bright flashes | Preserve the change-driven redraw logic. Clearing a single DMA buffer repeatedly produced visible flashes during development. |
| Flicker, wrong colors or rows | Check scan pattern, geometry, ribbon direction, ground, power at the far panel under load and configured pins. Other panel types may need different mapping. |
| Display remains in download mode | USB may keep the board powered after the external supply is removed. Disconnect both and remove the boot bridge. |

The tested installation measured 4.98 V at the furthest panel and became stable after reducing unnecessary redraws. That observation does not establish that every future flicker is software-related. Do not increase the supply beyond the hardware's rated voltage to mask a wiring problem.
