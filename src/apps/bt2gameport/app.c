// app.c - BT2GAMEPORT App Entry Point
// Bluetooth controller (DualSense, DS4, Xbox, 8BitDo, ...) to PC analog
// game port, on a Pico W (or Pico 2 W) using its built-in CYW43 radio.
//
// Board controls:
//   Button (GP22)  click        - BT scan for 60 s (pair another controller)
//                  double-click - next game port mode
//                  hold 1.5 s   - disconnect everything + forget all pairings
//   LED (GP26)     slow blink   - waiting for a controller
//                  solid        - controller connected
//                  N quick flashes after a mode change = mode N
//                  (1 = 4-axis, 2 = 2-axis, 3 = 2-player, 4 = FCS)
//
// Controller hotkeys (Joypad OS defaults, hold ~0.7 s):
//   Create/Select + D-pad Down/Up    - next / previous mode
//   Create/Select + D-pad Left/Right - D-pad <-> stick swap slider

#include "app.h"
#include "profiles.h"
#include "core/router/router.h"
#include "core/services/players/manager.h"
#include "core/services/players/feedback.h"
#include "core/services/profiles/profile.h"
#include "core/services/button/button.h"
#include "core/input_interface.h"
#include "core/output_interface.h"
#include "native/device/gameport/gameport_device.h"
#include "bt/transport/bt_transport.h"
#include "bt/btstack/btstack_host.h"
#include "core/services/leds/leds.h"

#include "pico/cyw43_arch.h"
#include "pico/bootrom.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "platform/platform.h"
#include <stdio.h>

extern const bt_transport_t bt_transport_cyw43;

// Stubs for CDC commands (no USB device stack in this app)
void cdc_commands_send_input_event(uint32_t buttons, const uint8_t* axes) { (void)buttons; (void)axes; }
void cdc_commands_send_output_event(uint32_t buttons, const uint8_t* axes) { (void)buttons; (void)axes; }
void cdc_commands_send_connect_event(uint8_t port, const char* name, uint16_t vid, uint16_t pid) { (void)port; (void)name; (void)vid; (void)pid; }
void cdc_commands_send_disconnect_event(uint8_t port) { (void)port; }

// ============================================================================
// STATUS LED (GP26, mirrored on the Pico W's on-board LED)
// ============================================================================

#define LED_IDLE_BLINK_MS   500
#define LED_FLASH_ON_MS     120
#define LED_FLASH_OFF_MS    180
#define LED_FLASH_GAP_MS    600

static bool bt_initialized = false;
static bool led_state = false;
static uint32_t led_last_toggle = 0;
static uint8_t led_seq = 0;          // remaining on/off steps of the mode indicator
static uint32_t led_seq_due = 0;     // time of the next indicator step

static void status_led_set(bool on)
{
    gpio_put(BT2GP_PIN_STATUS_LED, on);
    if (bt_initialized) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
    }
    led_state = on;
}

// Dark gap, (mode_index + 1) short flashes, dark gap, then normal status.
static void led_indicate_mode(uint8_t mode_index)
{
    led_seq = (uint8_t)((mode_index + 1) * 2);
    status_led_set(false);
    led_seq_due = platform_time_ms() + LED_FLASH_GAP_MS;
}

static void led_status_update(void)
{
    uint32_t now = platform_time_ms();

    if (led_seq || (int32_t)(now - led_seq_due) < 0) {
        if (led_seq && (int32_t)(now - led_seq_due) >= 0) {
            bool on = (led_seq % 2) == 0;
            status_led_set(on);
            led_seq--;
            led_seq_due = now + (on ? LED_FLASH_ON_MS
                                    : (led_seq ? LED_FLASH_OFF_MS : LED_FLASH_GAP_MS));
        }
        return;
    }

    if (btstack_classic_get_connection_count() > 0) {
        if (!led_state) status_led_set(true);
    } else if (now - led_last_toggle >= LED_IDLE_BLINK_MS) {
        led_last_toggle = now;
        status_led_set(!led_state);
    }
}

// ============================================================================
// MODE SWITCH FEEDBACK
// ============================================================================

static void on_profile_switch(output_target_t output, uint8_t index)
{
    if (output != OUTPUT_TARGET_GAMEPORT) return;
    const profile_t* p = profile_get_by_index(OUTPUT_TARGET_GAMEPORT, index);
    uint8_t mode = p ? p->output_mode : 0;
    printf("[app:bt2gameport] Profile %u (%s) -> %s\n", index,
           p ? p->name : "?", gameport_mode_name((gameport_mode_t)mode));
    led_indicate_mode(mode);
}

// ============================================================================
// BOARD BUTTON
// ============================================================================

static void on_button_event(button_event_t event)
{
    switch (event) {
        case BUTTON_EVENT_CLICK:
            printf("[app:bt2gameport] Starting BT scan (60s)...\n");
            btstack_host_start_timed_scan(60000);
            break;

        case BUTTON_EVENT_DOUBLE_CLICK:
            profile_cycle_next(OUTPUT_TARGET_GAMEPORT, true);
            break;

        case BUTTON_EVENT_HOLD:
            printf("[app:bt2gameport] Disconnecting all devices and clearing bonds...\n");
            btstack_host_disconnect_all_devices();
            btstack_host_delete_all_bonds();
            break;

        default:
            break;
    }
}

// ============================================================================
// APP INTERFACES
// ============================================================================

static const profile_config_t app_profile_config = {
    .output_profiles = {
        [OUTPUT_TARGET_GAMEPORT] = &gameport_profile_set,
    },
    .shared_profiles = NULL,
};

// No InputInterface: the BT transport's bthid drivers call router_submit_input().
const InputInterface** app_get_input_interfaces(uint8_t* count)
{
    *count = 0;
    return NULL;
}

static const OutputInterface* output_interfaces[] = {
    &gameport_output_interface,
};

const OutputInterface** app_get_output_interfaces(uint8_t* count)
{
    *count = sizeof(output_interfaces) / sizeof(output_interfaces[0]);
    return output_interfaces;
}

// ============================================================================
// APP INIT / TASK
// ============================================================================

void app_init(void)
{
    printf("[app:bt2gameport] Initializing BT2GAMEPORT v%s\n", JOYPAD_VERSION);
    printf("[app:bt2gameport] Pico W Bluetooth -> PC game port (MCP4352)\n");

    gpio_init(BT2GP_PIN_STATUS_LED);
    gpio_set_dir(BT2GP_PIN_STATUS_LED, GPIO_OUT);
    gpio_put(BT2GP_PIN_STATUS_LED, 1);

    button_init();
    button_set_callback(on_button_event);

    router_config_t router_cfg = {
        .mode = ROUTING_MODE,
        .merge_mode = MERGE_MODE,
        .max_players_per_output = {
            [OUTPUT_TARGET_GAMEPORT] = GAMEPORT_OUTPUT_PORTS,
        },
        .merge_all_inputs = false,
        .transform_flags = TRANSFORM_FLAGS,
        .mouse_drain_rate = 8,
    };
    router_init(&router_cfg);
    router_add_route(INPUT_SOURCE_BLE_CENTRAL, OUTPUT_TARGET_GAMEPORT, 0);

    player_config_t player_cfg = {
        .slot_mode = PLAYER_SLOT_MODE,
        .max_slots = MAX_PLAYER_SLOTS,
        .auto_assign_on_press = AUTO_ASSIGN_ON_PRESS,
    };
    players_init_with_config(&player_cfg);

    profile_init(&app_profile_config);
    profile_set_switch_callback(on_profile_switch);

    const char* active_name = profile_get_name(OUTPUT_TARGET_GAMEPORT,
                                               profile_get_active_index(OUTPUT_TARGET_GAMEPORT));
    printf("[app:bt2gameport]   Mode: %s (profile %s)\n",
           gameport_mode_name(gameport_get_mode()), active_name ? active_name : "none");
    printf("[app:bt2gameport]   Button: click=scan, double=next mode, hold=forget pairings\n");
}

void app_task(void)
{
    // 'B' on the USB/UART console reboots into the UF2 bootloader
    int c = getchar_timeout_us(0);
    if (c == 'B') {
        reset_usb_boot(0, 0);
    }

    // BT init is deferred to the first loop iteration so the game port is
    // already driven (centered, buttons released) before the ~1 s CYW43 boot.
    if (!bt_initialized) {
        printf("[app:bt2gameport] Initializing Bluetooth...\n");
        bt_init(&bt_transport_cyw43);
        bt_initialized = true;
        led_indicate_mode((uint8_t)gameport_get_mode());
    }

    button_task();
    bt_task();

    leds_set_connected_devices(btstack_classic_get_connection_count());
    led_status_update();
}
