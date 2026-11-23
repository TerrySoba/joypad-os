// mcp4352.h - Microchip MCP4352 quad 8-bit digital rheostat (SPI, volatile)
//
// The MCP4352 is the rheostat (B + W terminals only) member of the MCP435x
// family. Each channel's B-to-W resistance is:
//
//     R_BW(N) = R_AB * N / 256 + R_W        N = 0..256 (257 steps)
//
// so N=0 is the minimum (~R_W, about 75 ohm) and N=256 is full scale.
// The part powers up with every wiper at mid-scale (N=128) and all terminals
// connected, so the game port reads "centered" even before firmware runs.
//
// Write-only driver: the board does not route SDO, so nothing is read back.

#ifndef MCP4352_H
#define MCP4352_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/spi.h"

#define MCP4352_CHANNELS   4
#define MCP4352_STEP_MAX   256   // full scale (D8 set)

typedef struct {
    spi_inst_t* spi;
    uint8_t pin_sck;
    uint8_t pin_mosi;
    uint8_t pin_cs;
    uint32_t baud;
} mcp4352_config_t;

// Configure SPI + CS and push mid-scale / all-connected to the chip.
void mcp4352_init(const mcp4352_config_t* cfg);

// Set channel 0..3 wiper to step 0..256 (values above 256 are clamped).
void mcp4352_set_wiper(uint8_t channel, uint16_t step);

// Connect (true) or open-circuit (false) a channel's wiper terminal.
// Opening it makes the channel look like an unplugged axis (infinite
// resistance), which is how a PC detects that an axis is absent.
void mcp4352_set_connected(uint8_t channel, bool connected);

#endif // MCP4352_H
