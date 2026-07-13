# Driving a Klipper-modded Adventurer 5M Pro from OrcaSlicer

This is the **working** OrcaSlicer configuration for a FlashForge Adventurer 5M Pro
running the [xblax `flashforge_ad5m_klipper_mod`](https://github.com/xblax/flashforge_ad5m_klipper_mod)
(Mainsail / Moonraker), verified end-to-end (slice → upload → home → bed mesh → prime → print).

`AD5M-Pro-Klipper-OctoPrint.json` in this folder is an importable OrcaSlicer machine preset
capturing all of the below (edit the IP for your printer).

## The key insight

Once you flash the Klipper mod, the printer is **no longer a FlashForge-protocol printer** —
it's a **Klipper/Moonraker (OctoPrint-compatible) host**. So:

- **Do NOT** use a *Flashforge-vendor* printer preset or the FlashForge "Device" tab — those
  drive the stock LAN protocol on **port 8899**, which the mod removed (`connect_failed`).
- **DO** use a **Generic Klipper** printer connected as an **OctoPrint** physical printer.

## Settings that matter

| Setting | Value | Why |
|---|---|---|
| Printer base | **Generic Klipper** (not Flashforge vendor) | uses the OctoPrint send path, not `:8899` |
| Bed shape | **220 × 220 mm, Origin 110,110** (centered) | the mod uses center coordinates (X/Y −110…110); a front-left origin prints off-bed |
| Max height (Z) | **220 mm** | matches `stepper_z` max |
| **Machine start G-code** | `START_PRINT BED_TEMP=[bed_temperature_initial_layer_single] EXTRUDER_TEMP=[nozzle_temperature_initial_layer]` | the mod's macro is **`START_PRINT`** with `BED_TEMP`/`EXTRUDER_TEMP` params. It homes, runs `AUTO_BED_LEVEL`, heats, primes. Generic presets ship `PRINT_START EXTRUDER=/BED=`, which the mod does **not** have → "Printer not homed / Must home axis first". |
| **Machine end G-code** | `END_PRINT` | the mod's end macro (park + cooldown) |

## Connection (Physical Printer)

- **Host Type:** OctoPrint
- **Hostname/URL:** `http://<printer-ip>:7125` (Moonraker's OctoPrint-compat API — this is the
  *upload* endpoint; note `:7125` is not in `nmap`'s default port list, so a plain scan hides it)
- **Device-tab web UI:** tick **"View print host webui in Device tab"** and set the URL to
  `http://<printer-ip>:4000` (Mainsail). `:4000`/`:4001` serve the UIs but do **not** proxy the
  OctoPrint API, so the upload host must stay `:7125` while the Device tab points at `:4000`.

## Ports on the modded printer

```
22   ssh (root / klipper)      4000 Mainsail       4001 Fluidd
7125 Moonraker API/OctoPrint   8080 camera (/?action=stream)
8899 (stock FlashForge LAN) — CLOSED after the mod
```

## Notes

- SSH: `root` / `klipper` (Dropbear). `reboot-stock-system` boots stock firmware;
  `remove-klipper-mod` uninstalls the mod.
- Do **not** accept stock FlashForge firmware updates — they overwrite the Klipper mod.
