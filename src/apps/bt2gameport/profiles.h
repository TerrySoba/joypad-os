// profiles.h - BT2Gameport profile definitions
//
// Each profile selects a game port output mode (profile_t.output_mode, see
// gameport_mode_t) plus a button layout for it. Switch with the standard
// Joypad OS hotkey: hold Create/Select + D-pad Down (next) / Up (previous),
// or double-click the board button. The choice is saved to flash.
//
// Game port buttons are fed from JP_BUTTON_B1..B4 after this mapping:
//   B1 -> button 1 (pin 2)    B2 -> button 2 (pin 7)
//   B3 -> button 3 (pin 10)   B4 -> button 4 (pin 14)
// (In 2-player mode each controller's B1/B2 go to its own joystick.)
//
// DualSense names: B1 = Cross, B2 = Circle, B3 = Square, B4 = Triangle.

#ifndef BT2GAMEPORT_PROFILES_H
#define BT2GAMEPORT_PROFILES_H

#include "core/services/profiles/profile.h"
#include "native/device/gameport/gameport_device.h"

#define GAMEPORT_PROFILE_COMMON                         \
    .l2_behavior = TRIGGER_PASSTHROUGH,                 \
    .r2_behavior = TRIGGER_PASSTHROUGH,                 \
    .l2_threshold = 64,                                 \
    .r2_threshold = 64,                                 \
    .l2_analog_value = 0,                               \
    .r2_analog_value = 0,                               \
    .left_stick_sensitivity = 1.0f,                     \
    .right_stick_sensitivity = 1.0f,                    \
    .adaptive_triggers = false,                         \
    .socd_mode = SOCD_PASSTHROUGH

// ----------------------------------------------------------------------------
// 4 buttons: face buttons straight through, triggers/shoulders as extras
// ----------------------------------------------------------------------------
static const button_map_entry_t gameport_4btn_map[] = {
    MAP_BUTTON(JP_BUTTON_R2, JP_BUTTON_B1),   // R2 = fire (button 1)
    MAP_BUTTON(JP_BUTTON_L2, JP_BUTTON_B2),   // L2 = button 2
    MAP_BUTTON(JP_BUTTON_R1, JP_BUTTON_B3),   // R1 = button 3
    MAP_BUTTON(JP_BUTTON_L1, JP_BUTTON_B4),   // L1 = button 4
};

// ----------------------------------------------------------------------------
// 2 buttons: every face button / trigger lands on one of the two buttons
// ----------------------------------------------------------------------------
static const button_map_entry_t gameport_2btn_map[] = {
    MAP_BUTTON(JP_BUTTON_B1, JP_BUTTON_B1),   // Cross    -> button 1
    MAP_BUTTON(JP_BUTTON_B3, JP_BUTTON_B1),   // Square   -> button 1
    MAP_BUTTON(JP_BUTTON_R2, JP_BUTTON_B1),   // R2       -> button 1
    MAP_BUTTON(JP_BUTTON_R1, JP_BUTTON_B1),   // R1       -> button 1
    MAP_BUTTON(JP_BUTTON_B2, JP_BUTTON_B2),   // Circle   -> button 2
    MAP_BUTTON(JP_BUTTON_B4, JP_BUTTON_B2),   // Triangle -> button 2
    MAP_BUTTON(JP_BUTTON_L2, JP_BUTTON_B2),   // L2       -> button 2
    MAP_BUTTON(JP_BUTTON_L1, JP_BUTTON_B2),   // L1       -> button 2
};

static const profile_t gameport_profiles[] = {
    {
        .name = "4axis",
        .description = "4-axis 4-button joystick (left stick + right stick)",
        .button_map = gameport_4btn_map,
        .button_map_count = sizeof(gameport_4btn_map) / sizeof(gameport_4btn_map[0]),
        .output_mode = GAMEPORT_MODE_4AXIS,
        GAMEPORT_PROFILE_COMMON,
    },
    {
        .name = "2axis",
        .description = "Classic 2-axis 2-button joystick",
        .button_map = gameport_2btn_map,
        .button_map_count = sizeof(gameport_2btn_map) / sizeof(gameport_2btn_map[0]),
        .output_mode = GAMEPORT_MODE_2AXIS,
        GAMEPORT_PROFILE_COMMON,
    },
    {
        .name = "2player",
        .description = "Two 2-axis 2-button joysticks, one per controller",
        .button_map = gameport_2btn_map,
        .button_map_count = sizeof(gameport_2btn_map) / sizeof(gameport_2btn_map[0]),
        .output_mode = GAMEPORT_MODE_2PLAYER,
        GAMEPORT_PROFILE_COMMON,
    },
    {
        .name = "fcs",
        .description = "Thrustmaster FCS: 4 buttons + D-pad hat",
        .button_map = gameport_4btn_map,
        .button_map_count = sizeof(gameport_4btn_map) / sizeof(gameport_4btn_map[0]),
        .output_mode = GAMEPORT_MODE_FCS,
        GAMEPORT_PROFILE_COMMON,
    },
};

static const profile_set_t gameport_profile_set = {
    .profiles = gameport_profiles,
    .profile_count = sizeof(gameport_profiles) / sizeof(gameport_profiles[0]),
    .default_index = 0,
};

#endif // BT2GAMEPORT_PROFILES_H
