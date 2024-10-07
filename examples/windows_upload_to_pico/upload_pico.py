import serial
import subprocess
from pathlib import Path
import shutil
import sys
import time


def reset_pico_port(port: str, reset_baudrate=1200):
    try:
        ser = serial.Serial(port=port, baudrate=reset_baudrate, timeout=1)
        ser.close()
    except serial.SerialException as e:
        print("Warning: Encountered serial exception, but continuing anyway")


def get_win_comport():
    """Return a list of USB Serial Device Comports
    NOTE: this may work on linux as well
    """
    import serial.tools.list_ports
    ports = serial.tools.list_ports.comports()

    comports = [
        port for port, desc, hwid in ports
        if 'USB Serial Device' in desc
    ]

    if comports is None:
        raise RuntimeError("No USB Serial Device found")

    if len(comports) > 1:
        raise RuntimeError(f"More than one port found: {comports}")

    return comports[0]


def get_pico_drive_win():
    """Check if rpi drive is mounted
    return drive or None
    """
    drive_names_result = subprocess.run(
        ["wmic", "logicaldisk", "get", "caption"], capture_output=True, text=True)
    drive_labels_result = subprocess.run(
        ["wmic", "logicaldisk", "get", "volumename"], capture_output=True, text=True)

    # print(drive_labels_result.stdout.strip().split("\n\n"))
    # print(drive_names_result.stdout.strip().split("\n\n"))

    for idx, drive_label in enumerate(drive_labels_result.stdout.strip().split("\n\n")):
        if drive_label.strip() == 'RPI-RP2':
            drive_names = drive_names_result.stdout.strip().split("\n\n")
            return drive_names[idx].strip()

    return None


def mount_pico_drive_win() -> str:
    """Helper routine to mount pico if it is connected as a com port

    return pico drive (if mounted)
    raise Error if not mounted
    """
    pico_drive = get_pico_drive_win()

    # If pico is not mounted, find pico com port and reset
    if pico_drive is None:
        print("Pico drive is not mounted. Attempting to mount it")
        reset_pico_port(get_win_comport())

        # Wait for pico drive to mount
        time.sleep(0.5)

        # Check if pico drive is mounted
        pico_drive = get_pico_drive_win()
        if pico_drive is None:
            raise ConnectionError("Pico filesystem did not mount")

    return pico_drive


def mount_pico_drive_linux() -> str:
    raise NotImplementedError


def upload_pico_file_win(buildfile="dshot_example.uf2"):
    """Upload build file to pico"""

    # Check if buildfile exists
    if not Path(buildfile).is_file():
        raise ValueError(f"buildfile: {buildfile} is not a file")
    print(f"Found buildfile at: {buildfile}")

    # Mount pico drive (depends on system)
    mount_pico_drive = {'win32': mount_pico_drive_win,
                        'linux': mount_pico_drive_linux}
    pico_drive = mount_pico_drive[sys.platform]()
    print(f"Found pico drive at: {pico_drive}")

    # Upload buildfile to pico drive
    print(f"Uploading file to pico")
    shutil.copy(buildfile, pico_drive)


if __name__ == "__main__":
    upload_pico_file_win()
    print("Upload Finished")
