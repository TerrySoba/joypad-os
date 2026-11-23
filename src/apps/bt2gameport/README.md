# bt2gameport — Bluetooth controller → PC game port

Firmware for the BT2Gameport PCB: a Pico W (or Pico 2 W) receives a Bluetooth
controller (DualSense, DS4, Xbox, 8BitDo, …) and drives a 15-pin IBM PC game port.

```
make bt2gameport_pico_w     # or bt2gameport_pico2_w
# -> releases/joypad_os_<commit>_bt2gameport_pico_w.uf2
```

Flash: hold BOOTSEL while plugging the Pico into USB, then copy the `.uf2` to the
`RPI-RP2` drive. Once firmware is running, sending `B` on the USB serial console
reboots straight into the bootloader.

## Hardware mapping

| Function | Pico pin | Game port |
|---|---|---|
| MCP4352 SCK / SDI / CS (SPI0) | GP2 / GP3 / GP5 | — |
| Rheostat W0 / W1 / W2 / W3 | — | X1 (3) / Y1 (6) / X2 (11) / Y2 (13) |
| Button A1 / A2 / B1 / B2 (BSS138, high = pressed) | GP16 / GP17 / GP18 / GP19 | 2 / 7 / 10 / 14 |
| User button (to GND) | GP22 | — |
| Status LED | GP26 | — |

Assumes the **MCP4352-104** (100 kΩ) variant. Axis range is
`GAMEPORT_STEP_MIN..GAMEPORT_STEP_MAX` (0..256) in `gameport_device.h`.

## Pairing a DualSense

The adapter scans at power-on. On the controller hold **Create + PS** until the
lightbar flashes quickly. Once it's paired, pressing PS reconnects it. Click the
board button to scan again for 60 s (for a second controller). Hold the button for
1.5 s to forget all pairings.

## Modes

| # | Mode | Axes | Buttons |
|---|---|---|---|
| 1 | 4-axis 4-button (default) | X1/Y1 = left stick + D-pad, X2/Y2 = right stick | Cross/R2, Circle/L2, Square/R1, Triangle/L1 |
| 2 | 2-axis 2-button | X1/Y1 = left stick + D-pad, X2/Y2 open (absent) | Cross/Square/R1/R2, Circle/Triangle/L1/L2 |
| 3 | 2 players | controller 1 → joystick A, controller 2 → joystick B | per controller as mode 2 |
| 4 | Thrustmaster FCS | X1/Y1 = left stick, X2 = right stick X, Y2 = D-pad hat | as mode 1 |

To switch modes, hold **Create + D-pad Down/Up** (~0.7 s) or double-click the board
button. The controller rumbles and the board LED flashes N times for mode N. The
mode is saved to flash.

## LED

Slow blink means it's waiting for a controller. Solid means a controller is
connected. N quick flashes means mode N was selected.
