/*
 * BT2Gameport LED Configuration
 * Controller lightbar / player LED colors (DualSense lightbar, DS4, etc.)
 */

#ifndef CONSOLE_LED_CONFIG_H
#define CONSOLE_LED_CONFIG_H

// Player 1 - Blue (joystick A)
#define LED_P1_R 0
#define LED_P1_G 0
#define LED_P1_B 64
#define LED_P1_PATTERN 0b00100

// Player 2 - Red (joystick B in 2-player mode)
#define LED_P2_R 64
#define LED_P2_G 0
#define LED_P2_B 0
#define LED_P2_PATTERN 0b01010

// Player 3 - Green (not used by the game port, kept for completeness)
#define LED_P3_R 0
#define LED_P3_G 64
#define LED_P3_B 0
#define LED_P3_PATTERN 0b10101

// Player 4 - Yellow
#define LED_P4_R 64
#define LED_P4_G 64
#define LED_P4_B 0
#define LED_P4_PATTERN 0b11011

// Player 5 - Purple
#define LED_P5_R 20
#define LED_P5_G 0
#define LED_P5_B 40
#define LED_P5_PATTERN 0b11111

// Default/Unassigned - White
#define LED_DEFAULT_R 32
#define LED_DEFAULT_G 32
#define LED_DEFAULT_B 32
#define LED_DEFAULT_PATTERN 0

// Neopixel patterns (board has no NeoPixel; required by the LED service)
#define NEOPIXEL_PATTERN_0 pattern_purples
#define NEOPIXEL_PATTERN_1 pattern_blue
#define NEOPIXEL_PATTERN_2 pattern_red
#define NEOPIXEL_PATTERN_3 pattern_green
#define NEOPIXEL_PATTERN_4 pattern_yellow
#define NEOPIXEL_PATTERN_5 pattern_purple

#endif // CONSOLE_LED_CONFIG_H
