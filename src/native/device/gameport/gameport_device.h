// gameport_device.h - PC 15-pin analog game port output for Joypad OS
//
// Hardware model (BT2Gameport board):
//   - Axes: an MCP4352 quad digital rheostat sits between +5V (B terminals)
//     and the game port axis pins (wipers, via 1k series resistors). The PC's
//     558 one-shot timer measures the resistance, so 0 ohm = up/left and
//     ~100k = down/right. One SPI write per axis change.
//   - Buttons: four N-MOSFETs (BSS138) pull the button pins to GND when their
//     gate GPIO is driven HIGH. GPIO high = pressed.
//
// DA-15 pinout (IBM PC game port):
//    1 +5V     2 A-Btn1   3 A-X   4 GND   5 GND   6 A-Y   7 A-Btn2   8 +5V
//    9 +5V    10 B-Btn1  11 B-X  12 GND  13 B-Y  14 B-Btn2  15 +5V
//
// Output modes are selected per profile (profile_t.output_mode), so the
// standard Joypad OS profile hotkey (SELECT/Create + D-pad Up/Down) switches
// them and the choice is persisted to flash.

#ifndef GAMEPORT_DEVICE_H
#define GAMEPORT_DEVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/buttons.h"
#include "core/output_interface.h"

// ============================================================================
// PIN DEFINITIONS (defaults match the BT2Gameport PCB; override in CMake)
// ============================================================================

#ifndef GAMEPORT_SPI
#define GAMEPORT_SPI            spi0
#endif
#ifndef GAMEPORT_PIN_SCK
#define GAMEPORT_PIN_SCK        2
#endif
#ifndef GAMEPORT_PIN_MOSI
#define GAMEPORT_PIN_MOSI       3
#endif
#ifndef GAMEPORT_PIN_CS
#define GAMEPORT_PIN_CS         5
#endif
#ifndef GAMEPORT_SPI_BAUD
#define GAMEPORT_SPI_BAUD       (2 * 1000 * 1000)
#endif

#ifndef GAMEPORT_PIN_BTN_A1     // DA-15 pin 2  (joystick A, button 1)
#define GAMEPORT_PIN_BTN_A1     16
#endif
#ifndef GAMEPORT_PIN_BTN_A2     // DA-15 pin 7  (joystick A, button 2)
#define GAMEPORT_PIN_BTN_A2     17
#endif
#ifndef GAMEPORT_PIN_BTN_B1     // DA-15 pin 10 (joystick B, button 1)
#define GAMEPORT_PIN_BTN_B1     18
#endif
#ifndef GAMEPORT_PIN_BTN_B2     // DA-15 pin 14 (joystick B, button 2)
#define GAMEPORT_PIN_BTN_B2     19
#endif

// Rheostat channel driving each axis pin
#define GAMEPORT_CH_AX          0   // DA-15 pin 3
#define GAMEPORT_CH_AY          1   // DA-15 pin 6
#define GAMEPORT_CH_BX          2   // DA-15 pin 11
#define GAMEPORT_CH_BY          3   // DA-15 pin 13

// ============================================================================
// AXIS CALIBRATION
// ============================================================================
// Rheostat steps (0..256) used for the axis extremes. With the 100k part
// (MCP4352-104) 0..256 spans ~1k..~101k including the series resistor, which
// is what PC joysticks use. Narrow this range if a game's timing loop is too
// short for full scale, or if you fit the 50k part and want to compensate.

#ifndef GAMEPORT_STEP_MIN
#define GAMEPORT_STEP_MIN       0
#endif
#ifndef GAMEPORT_STEP_MAX
#define GAMEPORT_STEP_MAX       256
#endif

// Per-axis center deadzone on the analog sticks (0..127 units of
// the 0..255 input range). Old games calibrate once, so stick drift around
// center otherwise shows up as a slow creep.
#ifndef GAMEPORT_STICK_DEADZONE
#define GAMEPORT_STICK_DEADZONE 8
#endif

// Open-circuit the axes a mode does not use, so the PC sees them as absent
// (e.g. "2-axis, 2-button joystick" in 2-axis mode).
#ifndef GAMEPORT_OPEN_UNUSED_AXES
#define GAMEPORT_OPEN_UNUSED_AXES 1
#endif

// ============================================================================
// OUTPUT MODES (profile_t.output_mode)
// ============================================================================

typedef enum {
    // One 4-axis, 4-button joystick (CH / Thrustmaster style "4 axis 4 button")
    //   A-X/A-Y = left stick (+ D-pad), B-X/B-Y = right stick,
    //   buttons 1..4 = JP B1..B4
    GAMEPORT_MODE_4AXIS = 0,

    // One classic 2-axis, 2-button joystick
    //   A-X/A-Y = left stick (+ D-pad), buttons 1..2 = JP B1..B2,
    //   B axes open-circuit (absent)
    GAMEPORT_MODE_2AXIS = 1,

    // Two 2-axis, 2-button joysticks, one per controller
    //   Player 1 -> joystick A (pins 2,3,6,7), player 2 -> joystick B
    //   (pins 10,11,13,14). Each uses its left stick (+ D-pad) and B1/B2.
    GAMEPORT_MODE_2PLAYER = 2,

    // Thrustmaster FCS compatible: 4 buttons + 4-way hat on B-Y
    //   A-X/A-Y = left stick, B-X = right stick X,
    //   B-Y = hat (D-pad): up 0%, right 25%, down 50%, left 75%, center 100%
    GAMEPORT_MODE_FCS = 3,

    GAMEPORT_MODE_COUNT
} gameport_mode_t;

// ============================================================================
// API
// ============================================================================

void gameport_device_init(void);
void gameport_device_task(void);

// Currently active output mode (from the active profile)
gameport_mode_t gameport_get_mode(void);
const char* gameport_mode_name(gameport_mode_t mode);

extern const OutputInterface gameport_output_interface;

#endif // GAMEPORT_DEVICE_H
