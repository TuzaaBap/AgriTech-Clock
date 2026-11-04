# Flashing and recovery

## Enter ROM download mode

1. Close serial monitors. Disconnect **both external 5 V and USB**, so the board is fully off.
2. Temporarily bridge the two **P13/H2** pads beside Micro-USB. See the [component-side photo](HARDWARE.md). Do not bridge unrelated pads or use S1.
3. Reconnect the verified USB data connection and board power with the bridge in place. Keep external and USB power rails isolated as described in the wiring guide.
4. Run `pio device list`. Native ROM USB enumerated as Espressif VID `303A`, PID `0002`, with product ESP32-S2 on the tested Mac. Port names vary: `/dev/cu.usbmodem...`, `/dev/ttyACM...` or `COM...`.
5. Run `pio run -t upload --upload-port YOUR_PORT`. The target must identify as **ESP32-S2**.
6. Wait until every written section passes hash verification and upload reports success. Do not disconnect during writing.
7. Disconnect **both** supplies, remove the P13/H2 bridge, and reconnect normally. Leaving the bridge in place boots back into ROM and leaves the panel blank.

The project uses `before_reset = no_reset` to preserve a manually entered download mode. A software/1200-baud reset is not reliable on every running build; manual entry is the recovery path. It does not erase flash or burn eFuses.

## Backups

The pre-upload script reads the entire 4 MB flash into ignored `backups/` and creates a SHA-256 sidecar. A failed, incomplete or timed-out backup cancels the upload. Keep a verified original backup somewhere private; it may contain Wi-Fi credentials.

During development, stock-flash reading stalled. **No complete original backup was obtained**; having a spare board is not the same as having a backup. To intentionally skip backup after accepting replacement of the current firmware:

```sh
# macOS / Linux
AGRITECH_SKIP_BACKUP=1 pio run -t upload --upload-port YOUR_PORT
```

```powershell
# Windows PowerShell
$env:AGRITECH_SKIP_BACKUP = "1"
pio run -t upload --upload-port COM3
Remove-Item Env:AGRITECH_SKIP_BACKUP
```

After a backup read, the board remains in ROM download mode for uploading. Never treat a partial .bin as a recovery image.

## Interrupted upload

A partial write can make normal boot fail. Re-enter ROM mode with the full power-off sequence and perform a complete PlatformIO upload again. Successful hash verification is required. Do not guess offsets or flash an unrelated WF2 image.

## Serial logs

Use 115200 baud, DTR asserted and RTS deasserted for application USB logs. Some monitors need a restart after ROM switches to application USB. A new USB port number can appear after flashing.

```sh
pio device monitor --port YOUR_PORT --baud 115200 --dtr 1 --rts 0
```

Look for the display settings, Wi-Fi IP, `WX=OK AIR=OK`, free heap and readings. Never paste credentials into public issues.

## OTA after the first USB installation

ArduinoOTA uses hostname `agritech-wf1.local`, port 3232, and the configured Wi-Fi password as its OTA password. The firmware must be running and reachable on the same network. Keep that password private. Prefer a separate OTA password if adapting the project for a shared network; this release preserves the deployed authentication behavior. USB ROM recovery remains available if an OTA update fails.

## Compiled files

PlatformIO creates bootloader, partition and application files under `.pio/build/huidu_wf1/`. Let PlatformIO choose their addresses. Application binaries contain the compiled Wi-Fi/OTA credentials and are deliberately excluded from the public repository.

## Restore a verified full-flash backup

Only use a known-good complete 4 MB image from the same controller, and verify its recorded SHA-256 first. Enter ROM mode and close monitors. With esptool 4.9.0 installed in your Python environment:

```sh
python3 -m esptool --chip esp32s2 --port YOUR_PORT --baud 115200 --before no_reset --after no_reset write_flash 0x0 /path/to/verified-full-backup.bin
```

This overwrites the entire flash, including saved settings. Wait for hash verification, then remove the boot bridge and power-cycle both supplies. This project supplies no stock image; do not substitute an application-only firmware.bin at offset zero.
