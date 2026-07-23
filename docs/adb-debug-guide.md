# CardputerZero ADB Debug Mode (USB) — User Guide

CardputerZero can expose a standard **Android Debug Bridge (ADB)** endpoint over
its USB‑C port, just like Arduino UNO Q. Once enabled you can use the ordinary
`adb` tool from any computer to get a root shell, push/pull files, and forward
ports — no Wi‑Fi required.

> 中文版本见 [`ADB调试模式使用指南.md`](ADB调试模式使用指南.md)。

---

## 1. What this is (and how it compares to Android)

| | CardputerZero |
|---|---|
| Transport | USB FunctionFS (`ffs.adb`, bulk endpoints) via the dwc2 USB controller |
| Daemon | Debian's official `adbd` package (the AOSP source) |
| Host tool | The standard `adb` (Android platform‑tools) |
| Gate | **Settings → Developer → ADB** toggle (the equivalent of Android's "USB debugging") |

What works: `adb shell`, `adb push`, `adb pull`, `adb forward`, `adb reverse`.
What does **not** work (this is Debian, not Android): `adb install`, `pm`, `am`,
`logcat`.

Typical throughput is ~**38 MB/s** for `adb push` (about 3× the legacy CDC‑ACM
serial bridge).

---

## 2. Enable it on the device

1. On first use, authorize at least one host public key through an existing SSH
   connection or a local device terminal, as described in section 4.
2. Open **Settings** (the gear app).
3. Scroll to **Developer**, then press **OK/→** to enter.
4. Toggle **ADB** to **O** (on).

> The host-key pairing views are currently hidden in Settings. Enabling ADB with
> no authorized host fails; use `cardputer-adb authorize` first.

What happens under the hood when you turn it on:

- installs the `adbd` package if it is missing;
- verifies that `/adb_keys` contains at least one authorized host public key;
- brands the USB device as **CardputerZero** (see §5);
- makes sure the USB controller is in **peripheral** mode;
- enables and starts `adbd.service` (so it also comes back automatically after a
  reboot).

### The reboot prompt

The USB controller (dwc2) can run either as a USB **host** or as a USB
**peripheral**. ADB needs peripheral mode. If the controller is not already in
peripheral mode, the toggle edits `config.txt` and then shows a **"Reboot?"**
prompt — choose **Yes** to apply the change. After the reboot ADB comes up
automatically; you will not be asked again.

If the device is already in peripheral mode (the usual case once provisioned),
ADB starts immediately with **no reboot**.

---

## 3. Use it from your computer

Install `adb` if you don't have it:

- macOS: `brew install android-platform-tools` (or use the Android SDK's
  `platform-tools`).
- Windows/Linux: install Android SDK platform‑tools.

Then connect the USB‑C cable and run:

```bash
adb devices
# List of devices attached
# CardputerZero-xxxxxxxx   device

adb shell                 # root shell on the device
adb push ./file.bin /home/eggfly/   # send a file
adb pull /home/eggfly/log.txt .     # fetch a file
adb forward tcp:8080 tcp:80         # forward a device port to your computer
```

> On macOS the first connection shows an **"Allow accessory to connect?"** system
> dialog — click **Allow**. This is a macOS USB‑security prompt and is unrelated
> to ADB; it is remembered per device.

---

## 4. Authorization & the "allow this key" question

On Android, plugging into a new computer pops an on‑screen **"Allow USB
debugging?"** dialog where you tap *Allow* to trust that computer's key.

**Stock Debian `adbd` does not have that on‑screen per‑key dialog** — it has no
Android framework to talk to, so it cannot show the prompt. CardputerZero
therefore uses a key model that fits a headless‑style Linux device:

- **The Settings toggle is the security gate.** ADB is OFF by default; nothing
  listens until you explicitly turn it on (exactly like Android's "USB debugging"
  master switch).
- **There is no shared default public key.** At least one host must be explicitly
  authorized before ADB can be enabled.
- **To trust a computer**, inspect its public key on the host, then authorize it
  from a local device terminal or an existing SSH connection:

  ```bash
  # on your computer: print your public key
  cat ~/.android/adbkey.pub
  # on the device (first-time authorization requires a local terminal or existing SSH):
  sudo /usr/share/APPLaunch/adb/cardputer-adb authorize "<paste the pubkey line>"
  ```

> Note: current Debian `adbd` builds log `authentication not required` when no
> framework auth socket is present, i.e. they accept any host while ADB is on.
> Treat the **ADB toggle itself** as the trust boundary and turn it off when you
> are done.

---

## 5. The device name ("CardputerZero")

`adb devices` shows a *serial*. By default Debian's adbd derives it from a SHA‑256
of the machine‑id, which looks like `c7ead0dd7fb4384c...`. CardputerZero replaces
this with friendly USB string descriptors:

| USB descriptor | Value |
|----------------|-------|
| manufacturer | `M5Stack` |
| product | `CardputerZero` |
| serialnumber | `CardputerZero-<8 hex>` (short, stable, per‑unit) |

So `adb devices` shows e.g. `CardputerZero-c7ead0dd`, and the macOS accessory
dialog reads "M5Stack CardputerZero".

To change these, edit `cardputer-adb-gadget` (the `MANUFACTURER` / `PRODUCT`
variables) and toggle ADB off/on.

---

## 6. Disable it

Settings → Developer → toggle **ADB** to **X**. This immediately stops and
disables `adbd.service` and tears down the USB gadget. If the controller is still
in peripheral mode, the helper exits with code `10`; reboot to restore host/hub
mode and the onboard USB hub.

---

## 7. Command‑line helper (advanced)

The toggle calls a small device‑side script you can also run directly:

```bash
sudo /usr/share/APPLaunch/adb/cardputer-adb enable      # start ADB (exit 10 => reboot required)
sudo /usr/share/APPLaunch/adb/cardputer-adb disable     # stop ADB (exit 10 => reboot required)
     /usr/share/APPLaunch/adb/cardputer-adb status      # USB/adbd/enabled and authorization state
sudo /usr/share/APPLaunch/adb/cardputer-adb authorize <pubkey|file>
sudo /usr/share/APPLaunch/adb/cardputer-adb revoke <64-character-SHA-256-fingerprint>
sudo /usr/share/APPLaunch/adb/cardputer-adb clear-authorizations
sudo /usr/share/APPLaunch/adb/cardputer-adb migrate     # package-upgrade helper: remove legacy shared keys
```

Exit codes: `0` means success; `10` means configuration was updated but a reboot
is required for the USB mode change; `11` means no host has been authorized; and
`1` means an error. `status` prints `peripheral`, `adbd`, `enabled`,
`authorizations`, and the fingerprint and label of each authorization.

Files installed:

| Path | Purpose |
|------|---------|
| `/usr/share/APPLaunch/adb/cardputer-adb` | ADB switching, status, and host authorization management |
| `/usr/share/APPLaunch/adb/cardputer-adb-gadget` | branded USB gadget (name/serial) |
| `/etc/systemd/system/adbd.service.d/cardputer.conf` | drop‑in that wires the branded gadget into `adbd.service` |
| `/adb_keys` | authorized keys read by adbd |

---

## 8. Troubleshooting

| Symptom | Fix |
|---------|-----|
| `adb devices` empty | Make sure the ADB toggle is **on**; try `adb kill-server`; use a USB **data** cable directly to the CM0 (not through a hub). |
| Device shows `unauthorized` | Authorize your host key (see §4). |
| Toggle asks to reboot every time | The board could not enter peripheral mode; check `config.txt` has `dtoverlay=dwc2,dr_mode=peripheral` under `[all]`. |
| Conflicts with the old serial bridge | A single USB controller can host only one gadget; enabling ADB automatically stops the legacy PiBridge serial gadget. |
