/***

Designed for Longan CANBed 2040
15/09/2024
Gijón, Spain

Note that the printf() will all go to UART. Running this via USB does not work reliably.
The default uart uses GPIO0 and GPIO1, J3 on the CANBed 2040.
For this program to work optimally, you will need to wired the RS pin to an avaialble GPIO.
I used GPIO 10. This needs to be done with a soldering iron, no software solution here.

***/

#include <stdio.h>
#include "pico/runtime_init.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"               // For interrupt enable and disable
#include "hardware/pll.h"               // for pll_sys and pll_init() and pll_deinit()
#include "hardware/sync.h"              // Include this header for __wfi()
#include "hardware/watchdog.h"
#include "pico-mcp2515/include/mcp2515/mcp2515.h"


const uint16_t WATCHDOG_TIMEOUT = 8388; // ms, cannot be more due to 24 bit register counting at 2 MHz
const uint8_t LED_PIN = 18;             // General purpose LED on the Longan CANBed 2040
const uint8_t MCP2515_CS = 9;
const uint8_t MCP2515_MOSI = 3;
const uint8_t MCP2515_MISO = 4;
const uint8_t MCP2515_SCK = 2;
const uint8_t MCP2515_INT = 11;

const uint8_t SN65HVD230_RS = 10;       // Needs to be done manually!

MCP2515 can0(spi0, MCP2515_CS, MCP2515_MOSI, MCP2515_MISO, MCP2515_SCK);
struct can_frame rx;

inline static void ledON() { gpio_put(LED_PIN, true); }
inline static void ledOFF() { gpio_put(LED_PIN, false); }

void can_arrived(uint, uint32_t){ // The interrupt routine!
    if(gpio_get(SN65HVD230_RS)) // That is, if that pin is high, we were in low power mode..
    {
        printf("Put the SN65HVD230 into normal mode by letting that pin float!\n");
        // This is better than pulling it to ground, because this way slew control is still possible.
        gpio_set_dir(SN65HVD230_RS, GPIO_IN);
        gpio_disable_pulls(SN65HVD230_RS);

        printf("Put the MCP2515 into normal mode!\n");
        while(can0.setNormalMode() != MCP2515::ERROR_OK);
    }
    while(can0.readMessage(&rx) == MCP2515::ERROR_OK) {
        printf("CAN message was received!\n");
        watchdog_update();
    }
    gpio_acknowledge_irq(MCP2515_INT, GPIO_IRQ_LEVEL_LOW);
    can0.clearInterrupts();
}

int main()
{
    // GPIO
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(SN65HVD230_RS);
    gpio_set_dir(SN65HVD230_RS, GPIO_OUT);
    gpio_put(SN65HVD230_RS, true);      // Low power mode is with the pin pulled high. This makes it listen only!!

    gpio_init(MCP2515_INT);
    gpio_set_dir(MCP2515_INT, GPIO_IN);
    gpio_pull_up(MCP2515_INT);

    can0.reset();
    can0.setBitrate(CAN_125KBPS, MCP_16MHZ);
    can0.setSleepWakeup(true);
    while(can0.setSleepMode() != MCP2515::ERROR_OK);    // Gotta force that sleep mode.

    stdio_init_all();

    ledON();
    sleep_ms(200);
    ledOFF();
    
    printf("\nStartup!\n");
    if (watchdog_caused_reboot()) {
        printf("Rebooted by Watchdog!\n");
    } else {
        printf("Clean boot\n");
    }

    printf("Wait for 5 seconds to allow to measure current.\n");
    absolute_time_t start_time = get_absolute_time();
    while (absolute_time_diff_us(start_time, get_absolute_time()) < 5000000) {
        tight_loop_contents();  // Keep the CPU busy
    }

    // Busy wait current consumption at 3.3V:
    // 31.2 mA if no CAN messages are sent
    // 29.23 mA with no CAN messages and no RX LED and the SN65HVD230 in low power mode
    // 27.45 mA with no CAN messages and no RX LED and the SN65HVD230 in low power mode and the MCP2515 sleeping

    // CLK_REF = XOSC
    // This is default like this, but just in case.
    clock_configure(clk_ref,
                    CLOCKS_CLK_REF_CTRL_SRC_VALUE_XOSC_CLKSRC,
                    0, // No aux mux
                    XOSC_HZ,
                    XOSC_HZ);

    // CLK SYS = CLK_REF
    // This lowers the clk_sys from 125 MHz to 12 Mhz!
    clock_configure(clk_sys,
                    CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLK_REF,
                    0, // Using glitchless mux
                    XOSC_HZ,
                    XOSC_HZ);

    // This is the pll that was generating the 125 MHz for the clk_sys before.
    // Now that clk_sys runs from the XOSC, there is no need for it.
    pll_deinit(pll_sys);

    // CLK USB = 0MHz
    // This is a 48 MHz clock before shutting down.
    clock_stop(clk_usb);

    // CLK ADC = 0MHz
    // This is a 48 MHz clock before shutting down.
    clock_stop(clk_adc);

    // This is the pll that was generating the 48 MHz for clk_usb and clk_adc.
    // Now that they are both shut off, there is no need for it.
    pll_deinit(pll_usb);

    // CLK PERI = clk_sys. Used as reference clock for Peripherals. No dividers so just select and enable
    clock_configure(clk_peri,
                    0,  //not used for clk_peri!
                    CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                    XOSC_HZ,
                    XOSC_HZ);

    // The UART is going to be all confused with the new clocks, so we need to reinitialise.
    stdio_init_all();


    // Save these register states to be able to restore afer waking up
    uint clock0_orig = clocks_hw->sleep_en0;
    uint clock1_orig = clocks_hw->sleep_en1;
    uint save = scb_hw->scr;

    printf("Going to sleep with dog and enabled interrupt...\n\n");
    uint32_t irq_status = save_and_disable_interrupts();  // Disable interrupts and save status
    watchdog_enable(WATCHDOG_TIMEOUT, 1);
    gpio_set_irq_enabled_with_callback(MCP2515_INT, GPIO_IRQ_LEVEL_LOW, true, &can_arrived);


    // Turn off all clocks when in sleep mode except for RTC - which is needed for the watchdog!
    clocks_hw->sleep_en0 = CLOCKS_SLEEP_EN0_CLK_RTC_RTC_BITS;
    clocks_hw->sleep_en1 = 0x0;
    // Enable deep sleep at the proc
    scb_hw->scr = save | M0PLUS_SCR_SLEEPDEEP_BITS;



    __wfi();    // ----- THIS IS WHERE EXECUTION STOPS FOR THE DEEP SLEEP -----
    // Current consumption at 3.3V:
    // 7.58 mA with no CAN messages and no RX LED
    // 5.50 mA with no CAN messages and no RX LED and the SN65HVD230 in low power mode
    // 3.40 mA with no CAN messages and no RX LED and the SN65HVD230 in low power mode and the MCP2515 sleeping



    // If these registers are not restored, any __wfi() will take the RP2040 to deep sleep!
    clocks_hw->sleep_en0 = clock0_orig;
    clocks_hw->sleep_en1 = clock1_orig;
    scb_hw->scr = save;

    runtime_init_clocks();
    stdio_init_all();
    restore_interrupts(irq_status);  // Restore interrupts to previous state

    printf("Demonstrating light sleep while waiting for more interrupts to pat the dog.\n");
    while(1)
    {
        printf(".\n");
        sleep_ms(1000);
    }
    // Light sleep current consumption wtih occasional CAN interrupt: 29.55 mA
} // main
