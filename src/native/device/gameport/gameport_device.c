// gameport_device.c - PC analog game port output (MCP4352 rheostat + MOSFET buttons)
//
// Runs entirely on Core 0 as an output task: there is nothing timing-critical
// on our side. The PC's timer simply measures whatever resistance the
// rheostat currently presents, and buttons are static levels.

#include "gameport_device.h"
#include "mcp4352.h"
#include "core/router/router.h"
#include "core/input_event.h"
#include "core/services/profiles/profile.h"
#include "hardware/gpio.h"
#include <stdio.h>
#include <string.h>

#define GAMEPORT_PLAYERS 2

extern int playersCount;   // core/services/players/manager.c

static const uint8_t button_pins[4] = {
    GAMEPORT_PIN_BTN_A1, GAMEPORT_PIN_BTN_A2,
    GAMEPORT_PIN_BTN_B1, GAMEPORT_PIN_BTN_B2,
};

static const char* const mode_names[GAMEPORT_MODE_COUNT] = {
    [GAMEPORT_MODE_4AXIS]   = "4-axis 4-button",
    [GAMEPORT_MODE_2AXIS]   = "2-axis 2-button",
    [GAMEPORT_MODE_2PLAYER] = "2 players (2x 2-axis 2-button)",
    [GAMEPORT_MODE_FCS]     = "Thrustmaster FCS (hat)",
};

// Latest router state per player. router_get_output() only returns a pointer
// when a new version was published, so the last one is kept here. A
// disconnect publishes a cleared (centered, released) event, so this never
// goes stale with buttons held.
static input_event_t player_state[GAMEPORT_PLAYERS];
static profile_output_t player_mapped[GAMEPORT_PLAYERS];  // keeps autofire timers alive

static uint16_t last_step[MCP4352_CHANNELS];
static bool last_connected[MCP4352_CHANNELS];
static gameport_mode_t last_mode = GAMEPORT_MODE_COUNT;

// ============================================================================
// HELPERS
// ============================================================================

static uint8_t apply_deadzone(uint8_t v)
{
#if GAMEPORT_STICK_DEADZONE > 0
    int d = (int)v - 128;              // -128..127
    int mag = d < 0 ? -d : d;
    if (mag <= GAMEPORT_STICK_DEADZONE) return 128;
    // Rescale so the stick still reaches both extremes.
    int lim = d < 0 ? 128 : 127;
    int out = (mag - GAMEPORT_STICK_DEADZONE) * lim / (lim - GAMEPORT_STICK_DEADZONE);
    return (uint8_t)(128 + (d < 0 ? -out : out));
#else
    return v;
#endif
}

// 0..255 axis value -> rheostat step
static uint16_t axis_to_step(uint8_t v)
{
    return (uint16_t)(GAMEPORT_STEP_MIN +
                      ((uint32_t)v * (GAMEPORT_STEP_MAX - GAMEPORT_STEP_MIN) + 127) / 255);
}

// D-pad overrides the stick axis to its extreme when pressed.
static void dpad_to_axes(uint32_t buttons, uint8_t* x, uint8_t* y)
{
    if (buttons & JP_BUTTON_DL) *x = 0;
    else if (buttons & JP_BUTTON_DR) *x = 255;
    if (buttons & JP_BUTTON_DU) *y = 0;
    else if (buttons & JP_BUTTON_DD) *y = 255;
}

// Thrustmaster FCS hat encoding on the B-Y axis, as fraction of full scale
// (matches the Linux analog.c FCS decoder thresholds at 1/8, 3/8, 5/8, 7/8).
static uint16_t fcs_hat_step(uint32_t buttons)
{
    const uint32_t span = GAMEPORT_STEP_MAX - GAMEPORT_STEP_MIN;
    uint32_t q;
    if (buttons & JP_BUTTON_DU)      q = 0;   // up
    else if (buttons & JP_BUTTON_DD) q = 2;   // down
    else if (buttons & JP_BUTTON_DR) q = 1;   // right
    else if (buttons & JP_BUTTON_DL) q = 3;   // left
    else                             q = 4;   // centered
    return (uint16_t)(GAMEPORT_STEP_MIN + span * q / 4);
}

static void set_axis(uint8_t ch, uint16_t step, bool connected)
{
#if !GAMEPORT_OPEN_UNUSED_AXES
    connected = true;
#endif
    if (connected != last_connected[ch]) {
        mcp4352_set_connected(ch, connected);
        last_connected[ch] = connected;
    }
    if (step != last_step[ch]) {
        mcp4352_set_wiper(ch, step);
        last_step[ch] = step;
    }
}

// buttons: bit0..3 = game port button 1..4 (A1, A2, B1, B2)
static void set_buttons(uint8_t buttons)
{
    uint32_t mask = 0, value = 0;
    for (int i = 0; i < 4; i++) {
        mask |= 1u << button_pins[i];
        if (buttons & (1u << i)) value |= 1u << button_pins[i];
    }
    gpio_put_masked(mask, value);
}

// ============================================================================
// PUBLIC
// ============================================================================

gameport_mode_t gameport_get_mode(void)
{
    uint8_t m = profile_get_active_output_mode(OUTPUT_TARGET_GAMEPORT);
    return m < GAMEPORT_MODE_COUNT ? (gameport_mode_t)m : GAMEPORT_MODE_4AXIS;
}

const char* gameport_mode_name(gameport_mode_t mode)
{
    return mode < GAMEPORT_MODE_COUNT ? mode_names[mode] : "unknown";
}

void gameport_device_init(void)
{
    // Buttons first: drive gates low (released) before anything else.
    for (int i = 0; i < 4; i++) {
        gpio_init(button_pins[i]);
        gpio_put(button_pins[i], 0);
        gpio_set_dir(button_pins[i], GPIO_OUT);
    }

    const mcp4352_config_t cfg = {
        .spi = GAMEPORT_SPI,
        .pin_sck = GAMEPORT_PIN_SCK,
        .pin_mosi = GAMEPORT_PIN_MOSI,
        .pin_cs = GAMEPORT_PIN_CS,
        .baud = GAMEPORT_SPI_BAUD,
    };
    mcp4352_init(&cfg);

    for (int ch = 0; ch < MCP4352_CHANNELS; ch++) {
        last_step[ch] = MCP4352_STEP_MAX / 2;
        last_connected[ch] = true;
    }
    for (int p = 0; p < GAMEPORT_PLAYERS; p++) {
        init_input_event(&player_state[p]);
    }
}

void gameport_device_task(void)
{
    for (uint8_t p = 0; p < GAMEPORT_PLAYERS; p++) {
        const input_event_t* ev = router_get_output(OUTPUT_TARGET_GAMEPORT, p);
        if (ev) player_state[p] = *ev;
        // Slots can shift down when a controller leaves; never let an empty
        // slot hold on to its last state.
        if (p >= playersCount) init_input_event(&player_state[p]);
    }

    gameport_mode_t mode = gameport_get_mode();
    if (mode != last_mode) {
        printf("[gameport] Mode: %s\n", gameport_mode_name(mode));
        last_mode = mode;
    }

    const profile_t* profile = profile_get_active(OUTPUT_TARGET_GAMEPORT);
    uint8_t lx[GAMEPORT_PLAYERS], ly[GAMEPORT_PLAYERS], rx[GAMEPORT_PLAYERS], ry[GAMEPORT_PLAYERS];
    uint32_t btn[GAMEPORT_PLAYERS];

    for (uint8_t p = 0; p < GAMEPORT_PLAYERS; p++) {
        const input_event_t* e = &player_state[p];
        profile_output_t* m = &player_mapped[p];
        profile_apply(profile, e->buttons,
                      e->analog[ANALOG_LX], e->analog[ANALOG_LY],
                      e->analog[ANALOG_RX], e->analog[ANALOG_RY],
                      e->analog[ANALOG_L2], e->analog[ANALOG_R2],
                      e->analog[ANALOG_RZ], m);
        btn[p] = m->buttons;
        lx[p] = apply_deadzone(m->left_x);
        ly[p] = apply_deadzone(m->left_y);
        rx[p] = apply_deadzone(m->right_x);
        ry[p] = apply_deadzone(m->right_y);
        if (mode != GAMEPORT_MODE_FCS) {
            dpad_to_axes(btn[p], &lx[p], &ly[p]);
        }
    }

    uint8_t out = 0;
    const uint32_t b = btn[0];

    switch (mode) {
        case GAMEPORT_MODE_4AXIS:
        default:
            set_axis(GAMEPORT_CH_AX, axis_to_step(lx[0]), true);
            set_axis(GAMEPORT_CH_AY, axis_to_step(ly[0]), true);
            set_axis(GAMEPORT_CH_BX, axis_to_step(rx[0]), true);
            set_axis(GAMEPORT_CH_BY, axis_to_step(ry[0]), true);
            out = ((b & JP_BUTTON_B1) ? 0x1 : 0) | ((b & JP_BUTTON_B2) ? 0x2 : 0) |
                  ((b & JP_BUTTON_B3) ? 0x4 : 0) | ((b & JP_BUTTON_B4) ? 0x8 : 0);
            break;

        case GAMEPORT_MODE_2AXIS:
            set_axis(GAMEPORT_CH_AX, axis_to_step(lx[0]), true);
            set_axis(GAMEPORT_CH_AY, axis_to_step(ly[0]), true);
            set_axis(GAMEPORT_CH_BX, MCP4352_STEP_MAX / 2, false);
            set_axis(GAMEPORT_CH_BY, MCP4352_STEP_MAX / 2, false);
            out = ((b & JP_BUTTON_B1) ? 0x1 : 0) | ((b & JP_BUTTON_B2) ? 0x2 : 0);
            break;

        case GAMEPORT_MODE_2PLAYER: {
            const uint32_t b2 = btn[1];
            set_axis(GAMEPORT_CH_AX, axis_to_step(lx[0]), true);
            set_axis(GAMEPORT_CH_AY, axis_to_step(ly[0]), true);
            set_axis(GAMEPORT_CH_BX, axis_to_step(lx[1]), true);
            set_axis(GAMEPORT_CH_BY, axis_to_step(ly[1]), true);
            out = ((b  & JP_BUTTON_B1) ? 0x1 : 0) | ((b  & JP_BUTTON_B2) ? 0x2 : 0) |
                  ((b2 & JP_BUTTON_B1) ? 0x4 : 0) | ((b2 & JP_BUTTON_B2) ? 0x8 : 0);
            break;
        }

        case GAMEPORT_MODE_FCS:
            set_axis(GAMEPORT_CH_AX, axis_to_step(lx[0]), true);
            set_axis(GAMEPORT_CH_AY, axis_to_step(ly[0]), true);
            set_axis(GAMEPORT_CH_BX, axis_to_step(rx[0]), true);
            set_axis(GAMEPORT_CH_BY, fcs_hat_step(b), true);
            out = ((b & JP_BUTTON_B1) ? 0x1 : 0) | ((b & JP_BUTTON_B2) ? 0x2 : 0) |
                  ((b & JP_BUTTON_B3) ? 0x4 : 0) | ((b & JP_BUTTON_B4) ? 0x8 : 0);
            break;
    }

    set_buttons(out);
}

// ============================================================================
// OUTPUT INTERFACE
// ============================================================================

static uint8_t gp_get_profile_count(void) { return profile_get_count(OUTPUT_TARGET_GAMEPORT); }
static uint8_t gp_get_active_profile(void) { return profile_get_active_index(OUTPUT_TARGET_GAMEPORT); }
static void gp_set_active_profile(uint8_t i) { profile_set_active(OUTPUT_TARGET_GAMEPORT, i); }
static const char* gp_get_profile_name(uint8_t i) { return profile_get_name(OUTPUT_TARGET_GAMEPORT, i); }

const OutputInterface gameport_output_interface = {
    .name               = "PC Gameport",
    .target             = OUTPUT_TARGET_GAMEPORT,
    .init               = gameport_device_init,
    .task               = gameport_device_task,
    .core1_task         = NULL,
    .get_feedback       = NULL,
    .get_rumble         = NULL,
    .get_player_led     = NULL,
    .get_profile_count  = gp_get_profile_count,
    .get_active_profile = gp_get_active_profile,
    .set_active_profile = gp_set_active_profile,
    .get_profile_name   = gp_get_profile_name,
};
