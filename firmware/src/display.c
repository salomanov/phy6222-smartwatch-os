#include "display.h"
#include "spi.h"
#include "gpio.h"
#include "log.h"
#include "mcu.h"

static hal_spi_t s_spi;
static uint8_t _colstart = ST7735_COLSTART;
static uint8_t _rowstart = ST7735_ROWSTART;
static uint16_t _width = SCREEN_WIDTH;
static uint16_t _height = SCREEN_HEIGHT;
static uint8_t _rotation = 0;

static inline void ST7735_Command(uint8_t cmd) {
    hal_gpio_write(DC_PIN, 0);
    hal_spi_send_byte(&s_spi, cmd);
}

static inline void ST7735_Data(uint8_t* buff, uint8_t buff_size) {
    hal_gpio_write(DC_PIN, 1);
    hal_spi_transmit(&s_spi, SPI_TXD, buff, NULL, buff_size, 0);
}

static inline void ST7735_WriteCommand(uint8_t cmd) {
    ST7735_Command(cmd);
}

static inline void ST7735_WriteData(uint8_t data) {
    ST7735_Data(&data, 1);
}

static inline void ST7735_WriteDataMultiple(uint8_t* data, uint8_t size) {
    ST7735_Data(data, size);
}

static void display_delay_ms(uint32_t ms) {
    while (ms--) {
        for (volatile uint32_t i = 0; i < 2000; i++) {
            __asm__ volatile ("nop");
        }
    }
}

static void ST7735_ExecuteCommandList(const uint8_t *addr) {
    uint8_t numCommands = *addr++;
    while(numCommands--) {
        uint8_t cmd = *addr++;
        ST7735_WriteCommand(cmd);
        
        uint8_t numArgs = *addr++;
        uint16_t ms = (numArgs & 0x80) ? *addr++ : 0;
        numArgs &= 0x7F;
        
        if (numArgs) {
            ST7735_WriteDataMultiple((uint8_t*)addr, numArgs);
            addr += numArgs;
        }
        
        if (ms) {
            if (ms == 255) ms = 500;
            display_delay_ms(ms);
        }
    }
}

void display_set_addr_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    uint8_t data[4];
    
    // Column address set
    ST7735_WriteCommand(ST77XX_CASET);
    data[0] = 0x00;
    data[1] = x0 + _colstart;
    data[2] = 0x00;
    data[3] = x1 + _colstart;
    ST7735_WriteDataMultiple(data, 4);
    
    // Row address set
    ST7735_WriteCommand(ST77XX_RASET);
    data[0] = 0x00;
    data[1] = y0 + _rowstart;
    data[2] = 0x00;
    data[3] = y1 + _rowstart;
    ST7735_WriteDataMultiple(data, 4);
    
    // Memory write
    ST7735_WriteCommand(ST77XX_RAMWR);
}

void display_fill_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint16_t* color_buffer, uint32_t size) {
    display_set_addr_window(x0, y0, x1, y1);
    hal_gpio_write(DC_PIN, 1);
    hal_spi_transmit(&s_spi, SPI_TXD, (uint8_t*)color_buffer, NULL, size * 2, 0);
}

void display_set_rotation(uint8_t m) {
    m %= 4;
    _rotation = m;
    uint8_t madctl = 0;

    switch(m) {
        case 0: // Portrait
            madctl = ST77XX_MADCTL_MY | ST7735_MADCTL_BGR; // 0x88 (Un-mirrored factory orientation)
            _width  = SCREEN_WIDTH;
            _height = SCREEN_HEIGHT;
            _colstart = ST7735_COLSTART;
            _rowstart = ST7735_ROWSTART;
            break;
        case 1: // Landscape
            madctl = ST77XX_MADCTL_MV | ST7735_MADCTL_BGR;
            _width  = SCREEN_HEIGHT;
            _height = SCREEN_WIDTH;
            _colstart = ST7735_ROWSTART;
            _rowstart = ST7735_COLSTART;
            break;
        case 2: // Inverted Portrait
            madctl = ST77XX_MADCTL_MX | ST7735_MADCTL_BGR;
            _width  = SCREEN_WIDTH;
            _height = SCREEN_HEIGHT;
            _colstart = ST7735_COLSTART;
            _rowstart = ST7735_ROWSTART;
            break;
        case 3: // Inverted Landscape
            madctl = ST77XX_MADCTL_MX | ST77XX_MADCTL_MV | ST7735_MADCTL_BGR;
            _width  = SCREEN_HEIGHT;
            _height = SCREEN_WIDTH;
            _colstart = ST7735_ROWSTART;
            _rowstart = ST7735_COLSTART;
            break;
    }
    
    ST7735_WriteCommand(ST77XX_MADCTL);
    ST7735_WriteData(madctl);
}

void display_fill_screen(uint16_t color) {
    #define CHUNK_PIXELS 80
    static uint16_t line_buffer[CHUNK_PIXELS];
    for (int i = 0; i < CHUNK_PIXELS; i++) {
        // Swap bytes for big-endian SPI transmission
        line_buffer[i] = (color >> 8) | ((color & 0xFF) << 8);
    }
    
    display_set_addr_window(0, 0, _width - 1, _height - 1);
    hal_gpio_write(DC_PIN, 1);
    
    uint32_t total_pixels = (uint32_t)_width * _height;
    while (total_pixels > 0) {
        uint16_t chunk = (total_pixels > CHUNK_PIXELS) ? CHUNK_PIXELS : total_pixels;
        hal_spi_transmit(&s_spi, SPI_TXD, (uint8_t*)line_buffer, NULL, chunk * 2, 0);
        total_pixels -= chunk;
    }
}

void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    if (x >= _width || y >= _height) return;
    
    uint8_t color_data[2] = { (uint8_t)(color >> 8), (uint8_t)(color & 0xFF) };
    display_set_addr_window(x, y, x, y);
    hal_gpio_write(DC_PIN, 1);
    hal_spi_transmit(&s_spi, SPI_TXD, color_data, NULL, 2, 0);
}

uint16_t display_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void backlight_on(void) {
    hal_gpio_write(BKL_PIN, 0); // Active LOW
}

void backlight_off(void) {
    hal_gpio_write(BKL_PIN, 1);
}

uint16_t display_get_width(void) {
    return _width;
}

uint16_t display_get_height(void) {
    return _height;
}

void display_init(uint8_t rotation) {
    _rotation = rotation;

    // 1. Initialize GPIO pins
    hal_gpio_pin_init(DC_PIN, GPIO_OUTPUT);
    hal_gpio_pin_init(RST_PIN, GPIO_OUTPUT);
    hal_gpio_pin_init(CS_PIN, GPIO_OUTPUT);
    hal_gpio_pin_init(BKL_PIN, GPIO_OUTPUT);
    
    // Keep CS active low
    hal_gpio_write(CS_PIN, 0);
    
    // Ensure backlight is off during init
    backlight_off();
    
    // 2. Hardware Reset Pulse
    hal_gpio_write(RST_PIN, 0);
    display_delay_ms(20);
    hal_gpio_write(RST_PIN, 1);
    display_delay_ms(50);
    
    // 3. SPI Peripheral Configuration (16 MHz)
    spi_Cfg_t spi_cfg = {
        .sclk_pin   = SCLK_PIN,
        .ssn_pin    = CS_PIN,
        .MOSI       = MOSI_PIN,
        .MISO       = 0,
        .baudrate   = 16000000,
        .spi_tmod   = SPI_TXD,
        .spi_scmod  = SPI_MODE0,
        .spi_dfsmod = SPI_8BIT,
        .int_mode   = false,
        .force_cs   = false,
        .evt_handler= NULL,
    };
    
    s_spi.spi_index = SPI0;
    hal_spi_bus_init(&s_spi, spi_cfg);

    // 4. Send exact ST7735S initialization sequence extracted from factory ROM dump
    static const uint8_t factory_init_cmds[] = {
        13,                                 // 13 commands in sequence
        ST77XX_SWRESET,   0x80, 120,        // Software reset, wait 120ms
        ST77XX_SLPOUT,    0x80, 120,        // Sleep out, wait 120ms
        ST77XX_INVON,     0,                // IPS Inversion ON
        ST7735_FRMCTR1,   3, 0x05, 0x3A, 0x3A,
        ST7735_FRMCTR2,   3, 0x05, 0x3A, 0x3A,
        ST7735_FRMCTR3,   6, 0x05, 0x3A, 0x3A, 0x05, 0x3A, 0x3A,
        ST7735_INVCTR,    1, 0x03,
        ST7735_PWCTR1,    3, 0x62, 0x02, 0x04,
        ST7735_PWCTR2,    1, 0xC0,
        ST7735_PWCTR3,    2, 0x0D, 0x00,
        ST7735_PWCTR4,    2, 0x8D, 0x6A,
        ST7735_PWCTR5,    2, 0x8D, 0xEE,
        ST7735_VMCTR1,    1, 0x0E,
    };
    ST7735_ExecuteCommandList(factory_init_cmds);

    // Exact Factory Gamma Tables
    static const uint8_t factory_gamma_cmds[] = {
        4,
        ST7735_GMCTRP1, 16,
        0x0E, 0x02, 0x03, 0x0E, 0x07, 0x02, 0x07, 0x0A, 0x12, 0x27, 0x37, 0x00, 0x0D, 0x0E, 0x10, 0x00,
        ST7735_GMCTRN1, 16,
        0x0E, 0x03, 0x03, 0x0F, 0x06, 0x02, 0x08, 0x0A, 0x13, 0x26, 0x36, 0x00, 0x0D, 0x0E, 0x10, 0x00,
        ST77XX_COLMOD, 1, 0x05,             // 16-bit RGB565
        ST77XX_DISPON, 0x80, 50,            // Main screen turn on, wait 50ms
    };
    ST7735_ExecuteCommandList(factory_gamma_cmds);

    // 5. Set orientation & address offsets
    display_set_rotation(_rotation);

    // 6. Clear display to black before enabling backlight
    display_fill_screen(COLOR_BLACK);

    // 7. Turn backlight ON
    backlight_on();
}
