#define USER_SETUP_INFO "User_Setup"

#if defined(BOARD_DEV)

// ============================================================
// ORIGINAL ESP32 DEVELOPMENT BOARD
// ============================================================

#define ILI9341_2_DRIVER

#define TFT_WIDTH 240
#define TFT_HEIGHT 320

#define TFT_BL 21
#define TFT_BACKLIGHT_ON HIGH

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14

#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1

#define TOUCH_CS 33

#define USE_HSPI_PORT

#define SPI_FREQUENCY 40000000
#define SPI_READ_FREQUENCY 16000000
#define SPI_TOUCH_FREQUENCY 2500000

#elif defined(BOARD_S3)

// ============================================================
// HOSYOND ESP32-S3
// ============================================================

#define ST7796_DRIVER

#define TFT_WIDTH 320
#define TFT_HEIGHT 480

#define TFT_MISO 13
#define TFT_MOSI 11
#define TFT_SCLK 12

#define TFT_CS 10
#define TFT_DC 46
#define TFT_RST -1

#define TFT_BL 45
#define TFT_BACKLIGHT_ON HIGH

// Important ESP32-S3 TFT_eSPI workaround
#define USE_HSPI_PORT

#define SPI_FREQUENCY 40000000
#define SPI_READ_FREQUENCY 16000000

#else

#error "No board configuration selected! Define BOARD_DEV or BOARD_S3."

#endif