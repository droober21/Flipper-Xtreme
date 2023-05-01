#include "avr_isp_spi_sw.h"

#include <furry.h>

#define AVR_ISP_SPI_SW_MISO &gpio_ext_pa6
#define AVR_ISP_SPI_SW_MOSI &gpio_ext_pa7
#define AVR_ISP_SPI_SW_SCK &gpio_ext_pb3
#define AVR_ISP_RESET &gpio_ext_pb2

struct AvrIspSpiSw {
    AvrIspSpiSwSpeed speed_wait_time;
    const GpioPin* miso;
    const GpioPin* mosi;
    const GpioPin* sck;
    const GpioPin* res;
};

AvrIspSpiSw* avr_isp_spi_sw_init(AvrIspSpiSwSpeed speed) {
    AvrIspSpiSw* instance = malloc(sizeof(AvrIspSpiSw));
    instance->speed_wait_time = speed;
    instance->miso = AVR_ISP_SPI_SW_MISO;
    instance->mosi = AVR_ISP_SPI_SW_MOSI;
    instance->sck = AVR_ISP_SPI_SW_SCK;
    instance->res = AVR_ISP_RESET;

    furry_hal_gpio_init(instance->miso, GpioModeInput, GpioPullNo, GpioSpeedVeryHigh);
    furry_hal_gpio_write(instance->mosi, false);
    furry_hal_gpio_init(instance->mosi, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    furry_hal_gpio_write(instance->sck, false);
    furry_hal_gpio_init(instance->sck, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    furry_hal_gpio_init(instance->res, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);

    return instance;
}

void avr_isp_spi_sw_free(AvrIspSpiSw* instance) {
    furry_assert(instance);
    furry_hal_gpio_init(instance->res, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furry_hal_gpio_init(instance->miso, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furry_hal_gpio_init(instance->mosi, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furry_hal_gpio_init(instance->sck, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    free(instance);
}

uint8_t avr_isp_spi_sw_txrx(AvrIspSpiSw* instance, uint8_t data) {
    furry_assert(instance);
    for(uint8_t i = 0; i < 8; ++i) {
        furry_hal_gpio_write(instance->mosi, (data & 0x80) ? true : false);

        furry_hal_gpio_write(instance->sck, true);
        if(instance->speed_wait_time != AvrIspSpiSwSpeed1Mhz)
            furry_delay_us(instance->speed_wait_time - 1);

        data = (data << 1) | furry_hal_gpio_read(instance->miso); //-V792

        furry_hal_gpio_write(instance->sck, false);
        if(instance->speed_wait_time != AvrIspSpiSwSpeed1Mhz)
            furry_delay_us(instance->speed_wait_time - 1);
    }
    return data;
}

void avr_isp_spi_sw_res_set(AvrIspSpiSw* instance, bool state) {
    furry_assert(instance);
    furry_hal_gpio_write(instance->res, state);
}

void avr_isp_spi_sw_sck_set(AvrIspSpiSw* instance, bool state) {
    furry_assert(instance);
    furry_hal_gpio_write(instance->sck, state);
}