//
// GPIO Map for LilyGO T-2CAN V1.0 (ESP32-S3-WROOM-1U N16R8)
//   CAN1: integrated TWAI transceiver (GPIO 6/7)
//   CAN2: MCP2515 via SPI (CS=10, INT=8)
//   SPI:  SCLK=12, MOSI=11, MISO=13  (MCP2515 16MHz crystal)
//   Modem: A7670E-FASE on "Uart" connector = UART0 GPIO43(TX)/44(RX)
//

#define VSPI_PIN_MISO             13
#define VSPI_PIN_MOSI             11
#define VSPI_PIN_CLK              12

// CAN1 = ESP32 TWAI controller (transceiver onboard, GPIO6=RX GPIO7=TX)
#define ESP32CAN_PIN_TX            7
#define ESP32CAN_PIN_RX            6

// CAN2 = MCP2515 #1
#define VSPI_PIN_MCP2515_1_CS     10
#define VSPI_PIN_MCP2515_1_INT     8

// CAN3 = MCP2515 #2 (not populated on T-2CAN)
#define VSPI_PIN_MCP2515_2_CS     -1
#define VSPI_PIN_MCP2515_2_INT    -1

// SIMCOM A7670E-FASE cellular modem (external module, UART0 43/44)
#define MODEM_GPIO_RX             44      // modem TX -> ESP32 RX
#define MODEM_GPIO_TX             43      // modem RX <- ESP32 TX
#define MODEM_GPIO_DTR            17      // modem DTR (sleep control, active low)
#define MODEM_GPIO_PWR            15      // modem PWRKEY via NPN (active low)
#define MODEM_GPIO_RST            16      // modem RESET (active low)
#define MODEM_EGPIO_PWR           MODEM_GPIO_PWR  // no MAX7317 on T-2CAN
#define MODEM_EGPIO_DTR           MODEM_GPIO_DTR