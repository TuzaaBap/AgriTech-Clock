"""Read the existing flash before each upload. A failed backup cancels upload."""
from datetime import datetime, timezone
from pathlib import Path
import hashlib
import subprocess
import os

Import("env")


def backup_before_upload(source, target, env):
    if os.environ.get("AGRITECH_SKIP_BACKUP") == "1":
        print("Skipping flash backup at the user's explicit request.")
        return
    env.AutodetectUploadPort()
    port = env.subst("$UPLOAD_PORT")
    if not port:
        raise RuntimeError("No controller port detected. WF1 Micro-USB is power-only.")
    tool_dir = env.PioPlatform().get_package_dir("tool-esptoolpy")
    esptool = str(Path(tool_dir) / "esptool.py")
    folder = Path(env.subst("$PROJECT_DIR")) / "backups"
    folder.mkdir(exist_ok=True)
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    backup = folder / ("wf1-before-upload-" + timestamp + ".bin")
    command = [env.subst("$PYTHONEXE"), esptool,
               "--chip", "esp32s2", "--port", port, "--baud", "115200",
               "--before", "no_reset", "--after", "no_reset", "read_flash", "0", "ALL", str(backup)]
    print("Backing up the entire controller flash before upload:", backup)
    subprocess.run(command, check=True, timeout=120)
    if backup.stat().st_size < 4 * 1024 * 1024:
        raise RuntimeError("Incomplete WF1 flash backup; upload cancelled.")
    digest = hashlib.sha256(backup.read_bytes()).hexdigest()
    backup.with_suffix(".sha256").write_text(digest + "  " + backup.name + "\n")
    print("Flash backup saved. SHA256:", digest)


env.AddPreAction("upload", backup_before_upload)

