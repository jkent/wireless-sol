<img align="left" src="https://raw.githubusercontent.com/jkent/wireless-sol/master/html/images/logo.png" />
# wireless-sol</h1>
A smart LED controller for white APA104/WS2812 LEDs.

## Building instructions

If you're using the esp-open-sdk, create a `Makefile.local` similar to the following:

    OPENSDK     := /path/to/esp-open-sdk
    PATH        := $(OPENSDK)/xtensa-lx106-elf/bin:$(PATH)

Otherwise you'll need to set the paths of the SDK and toolchain to use manually:

    SDK_BASE    := /path/to/esp_iot_sdk_v1.1.2
    ESPTOOL     := /path/to/esptool.py
    PATH        := /path/to/xtensa-lx106-elf/bin:$(PATH)
    USE_OPENSDK := 0

Additional settings you may want to change in your `Makefile.local`:

    ESPPORT := /dev/ttyUSB0
    ESPBAUD := 921600

    #SPI flash size, in K
    ESP_SPI_FLASH_SIZE_K := 512

    #0: QIO, 1: QOUT, 2: DIO, 3: DOUT
    ESP_FLASH_MODE := 0

    #0: 40MHz, 1: 26MHz, 2: 20MHz, 0xf: 80MHz
    ESP_FLASH_FREQ_DIV := 0

Then just run `make`, `make flash` or even `make run`.

UART0 and UART1 both run at 115200 baud.  A REPL interface for wireless configuration runs on UART0 and debug output is on UART1.
