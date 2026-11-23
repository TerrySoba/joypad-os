// mcp4352.c - Microchip MCP4352 quad digital rheostat driver (write-only SPI)
//
// SPI frame (16 bit, MSB first, mode 0,0):
//   [AD3 AD2 AD1 AD0 C1 C0 D9 D8] [D7 .. D0]
//   C1:C0 = 00 -> write data
//
// Volatile memory map (MCP435x datasheet, "Memory Map"):
//   0x00 Wiper 0     0x01 Wiper 1     0x04 TCON0 (wipers 0/1)
//   0x06 Wiper 2     0x07 Wiper 3     0x0A TCON1 (wipers 2/3)
//
// TCONn bit layout (per register, two wipers):
//   bit 8      reserved, write 1
//   bits 7..4  RxHW RxA RxW RxB  for the odd  wiper (1 / 3)
//   bits 3..0  RxHW RxA RxW RxB  for the even wiper (0 / 2)
// Each bit = 1 connects that terminal (HW = "not in hardware shutdown").

#include "mcp4352.h"
#include "hardware/gpio.h"

#define REG_WIPER0  0x00
#define REG_WIPER1  0x01
#define REG_TCON0   0x04
#define REG_WIPER2  0x06
#define REG_WIPER3  0x07
#define REG_TCON1   0x0A

#define CMD_WRITE   0x0

#define TCON_RESERVED  0x100
#define TCON_NIBBLE_ALL 0xF      // HW | A | W | B
#define TCON_W_BIT      0x2      // within a nibble

static const uint8_t wiper_reg[MCP4352_CHANNELS] = {
    REG_WIPER0, REG_WIPER1, REG_WIPER2, REG_WIPER3,
};

static spi_inst_t* s_spi;
static uint8_t s_cs;
static uint16_t s_tcon[2];   // shadow of TCON0 / TCON1

static void write_reg(uint8_t addr, uint16_t data)
{
    uint8_t frame[2] = {
        (uint8_t)((addr << 4) | (CMD_WRITE << 2) | ((data >> 8) & 0x03)),
        (uint8_t)(data & 0xFF),
    };
    gpio_put(s_cs, 0);
    spi_write_blocking(s_spi, frame, sizeof(frame));
    gpio_put(s_cs, 1);
}

void mcp4352_init(const mcp4352_config_t* cfg)
{
    s_spi = cfg->spi;
    s_cs = cfg->pin_cs;

    spi_init(s_spi, cfg->baud);
    spi_set_format(s_spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(cfg->pin_sck, GPIO_FUNC_SPI);
    gpio_set_function(cfg->pin_mosi, GPIO_FUNC_SPI);

    // CS is driven manually so a full 16-bit frame stays inside one CS pulse.
    gpio_init(s_cs);
    gpio_put(s_cs, 1);
    gpio_set_dir(s_cs, GPIO_OUT);

    s_tcon[0] = TCON_RESERVED | (TCON_NIBBLE_ALL << 4) | TCON_NIBBLE_ALL;
    s_tcon[1] = s_tcon[0];
    write_reg(REG_TCON0, s_tcon[0]);
    write_reg(REG_TCON1, s_tcon[1]);

    for (uint8_t ch = 0; ch < MCP4352_CHANNELS; ch++) {
        mcp4352_set_wiper(ch, MCP4352_STEP_MAX / 2);
    }
}

void mcp4352_set_wiper(uint8_t channel, uint16_t step)
{
    if (channel >= MCP4352_CHANNELS) return;
    if (step > MCP4352_STEP_MAX) step = MCP4352_STEP_MAX;
    write_reg(wiper_reg[channel], step);
}

void mcp4352_set_connected(uint8_t channel, bool connected)
{
    if (channel >= MCP4352_CHANNELS) return;

    uint8_t idx = channel >> 1;                       // 0,1 -> TCON0; 2,3 -> TCON1
    uint16_t bit = (uint16_t)TCON_W_BIT << ((channel & 1) ? 4 : 0);
    uint16_t next = connected ? (s_tcon[idx] | bit) : (s_tcon[idx] & ~bit);
    if (next == s_tcon[idx]) return;

    s_tcon[idx] = next;
    write_reg(idx ? REG_TCON1 : REG_TCON0, next);
}
