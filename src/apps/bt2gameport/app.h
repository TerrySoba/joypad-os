// app.h - BT2GAMEPORT App Manifest
// Bluetooth controller (e.g. DualSense) to PC analog game port, Pico W
//
// This manifest is a human-readable summary of what this app uses.
// It is NOT consumed by the build system. The authoritative per-target
// configuration lives in src/CMakeLists.txt (see issue #198).

#ifndef APP_BT2GAMEPORT_H
#define APP_BT2GAMEPORT_H

// ============================================================================
// APP METADATA
// ============================================================================
#define APP_NAME "BT2GAMEPORT"
#define APP_DESCRIPTION "Bluetooth to PC game port adapter (Pico W)"
#define APP_AUTHOR "BT2Gameport"

// ============================================================================
// CORE DEPENDENCIES
// ============================================================================
#define REQUIRE_BT_CYW43 1              // Pico W built-in Bluetooth
#define REQUIRE_USB_HOST 0
#define MAX_USB_DEVICES 0
#define REQUIRE_USB_DEVICE 0            // USB is only used for stdio / flashing
#define REQUIRE_NATIVE_GAMEPORT_OUTPUT 1
#define GAMEPORT_OUTPUT_PORTS 2         // Joystick A + joystick B

#define REQUIRE_FLASH_SETTINGS 1        // Active mode/profile persisted
#define REQUIRE_PROFILE_SYSTEM 1
#define REQUIRE_PLAYER_MANAGEMENT 1

// ============================================================================
// ROUTING CONFIGURATION
// ============================================================================
#define ROUTING_MODE ROUTING_MODE_SIMPLE // Controller 1 -> player 0, controller 2 -> player 1
#define MERGE_MODE MERGE_ALL
#define APP_MAX_ROUTES 4
#define TRANSFORM_FLAGS 0

// ============================================================================
// PLAYER MANAGEMENT
// ============================================================================
#define PLAYER_SLOT_MODE PLAYER_SLOT_SHIFT  // If player 1 leaves, player 2 takes over joystick A
#define MAX_PLAYER_SLOTS 2
#define AUTO_ASSIGN_ON_PRESS 1

// ============================================================================
// HARDWARE CONFIGURATION (BT2Gameport PCB)
// ============================================================================
#define BOARD "pico_w"
#define UART_DEBUG 1

#ifndef BT2GP_PIN_STATUS_LED
#define BT2GP_PIN_STATUS_LED 26         // D1 via 330R, active high
#endif
// User button: GP22 to GND, configured as BUTTON_USER_GPIO in CMake

// ============================================================================
// BLUETOOTH CONFIGURATION
// ============================================================================
#define BT_MAX_CONNECTIONS 2
#define BT_SCAN_ON_STARTUP 1

// ============================================================================
// APP INTERFACE (OS calls these)
// ============================================================================
void app_init(void);
void app_task(void);

#endif // APP_BT2GAMEPORT_H
