
# usb_guard

**usb_guard** is a Linux kernel module + user-space launcher that restricts access to an application unless a specific USB “key” (flash drive) is plugged in.

The USB key is identified by **VID/PID** and (optionally) by **USB serial number**.  
The module exposes the key presence status via **/proc/usb_guard** (0 or 1).  
A launcher script/program uses that status to allow or deny execution of the protected app.

> Security note: this is a coursework/demo-grade access control mechanism.  
> A user with root privileges can bypass it. For normal users, it is effective when combined with file permissions (see “Hardening” below).

---

## Features

### Kernel module (`usb_guard.c`)
- Detects USB device presence using **USB subsystem notifications** (no raw disk reads, no filesystem modifications).
- Matches by:
  - Vendor ID (VID)
  - Product ID (PID)
  - Optional USB **serial string** (exact match)
- Provides a simple status interface:
  - `cat /proc/usb_guard` → `1` if the target USB key is present, otherwise `0`
- Safe by design:
  - Does **not** write to disks
  - Does **not** read raw sectors
  - Does **not** mount/unmount anything
- Works even if the USB key is already plugged in before loading the module (initial scan on init).

### User-space access control
You have two options:

1) **Script wrapper** (`run_with_usb_key.sh`)
- Checks `/proc/usb_guard`
- Starts an application only when the USB key is present

2) **C launcher** (`usb_launch.c` → `usb_mines`)
- Same idea but as a compiled binary (useful for setgid hardening)

---

## Requirements

- Ubuntu/Linux with kernel headers installed
- Build tools:
  - `build-essential`
  - `linux-headers-$(uname -r)`

Install dependencies:
```bash
sudo apt update
sudo apt install -y build-essential linux-headers-$(uname -r)
```

---

## Build the kernel module

In the project directory:

```bash
make
```

Output:

* `usb_guard.ko` — the kernel module

Clean build artifacts:

```bash
make clean
```

---

## Find your USB key VID/PID and serial

Plug in the USB device and run:

```bash
lsusb
```

Example line:

```
Bus 003 Device 008: ID abcd:1234 LogiLink UDisk flash drive
```

Here:

* VID = `abcd`
* PID = `1234`

Optional serial (often useful for uniqueness):

```bash
udevadm info -q property -n /dev/sdX | grep SERIAL
```

(Replace `/dev/sdX` with your USB disk device, e.g. `/dev/sda` or `/dev/sdb`.)

Try using `ID_SERIAL_SHORT` as the serial value.

---

## Load / unload the module

### Load (VID/PID only)

```bash
sudo insmod usb_guard.ko vendor=0xabcd product=0x1234
```

### Load (VID/PID + serial)

```bash
sudo insmod usb_guard.ko vendor=0xabcd product=0x1234 serial="2507081206300719092151"
```

### Check status

```bash
cat /proc/usb_guard
```

* `1` → USB key present
* `0` → USB key missing

### Unload

```bash
sudo rmmod usb_guard
```

### Debug logs

```bash
dmesg | tail -n 80
```

---

## Use the script wrapper (simple demo)

Edit `run_with_usb_key.sh` and set `APP` to the command you want to protect.

Example:

```bash
./run_with_usb_key.sh
```

If the key is absent → access denied.
If present → the application starts.

---

## Use the compiled launcher (`usb_launch.c` → usb_mines)

Build:

```bash
gcc usb_launch.c -o usb_mines
```

Run:

```bash
./usb_mines
```

It checks `/proc/usb_guard` and starts the protected program only when the key is present.

---

## Hardening (prevent direct start)

To prevent launching the protected app directly, store the *real* executable in a restricted directory and run it only through a launcher that has controlled permissions.

A common coursework-friendly approach is **setgid** with a dedicated group:

1. Create group:

```bash
sudo groupadd usbkey
```

2. Put the real app into a restricted directory (example):

```bash
sudo mkdir -p /opt/usbkey
sudo cp /usr/bin/gnome-mines /opt/usbkey/gnome-mines.real
sudo chown root:usbkey /opt/usbkey/gnome-mines.real
sudo chmod 750 /opt/usbkey/gnome-mines.real
```

3. Install launcher with setgid:

```bash
sudo cp usb_mines /usr/local/bin/usb_mines
sudo chown root:usbkey /usr/local/bin/usb_mines
sudo chmod 2755 /usr/local/bin/usb_mines
```

Now:

* Normal users cannot execute `/opt/usbkey/gnome-mines.real` directly
* Running `/usr/local/bin/usb_mines` will succeed only when the USB key is present

> Note: This does not “lock down” the original `/usr/bin/gnome-mines` system binary; it protects your *restricted copy* (`*.real`). For a clean demo, protect your own application binary.

---

## Troubleshooting

### `/proc/usb_guard` always shows 1 even after unplug

This was fixed by tracking the exact USB device pointer and releasing it on REMOVE events (no serial reads during removal).

### `insmod: ERROR: could not insert module ... Required key not available`

This is usually caused by **Secure Boot**. Either disable Secure Boot in BIOS/UEFI or sign the module.

### `gnome-mines: command not found`

Install Mines:

```bash
sudo apt install -y gnome-mines
```

---

## Project structure

* `usb_guard.c` — kernel module (USB key detection, `/proc/usb_guard`)
* `Makefile` — Kbuild makefile for module compilation
* `run_with_usb_key.sh` — simple script wrapper (demo access control)
* `usb_launch.c` — compiled launcher source (reads `/proc/usb_guard`, executes real app)

---

