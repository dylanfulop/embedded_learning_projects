#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"
#include "hardware/rtc.h"
#include "hardware/adc.h"
#include "pico/binary_info.h"
#include "pico/util/datetime.h"
#include "lcd.h"
// SPI Defines
// We are going to use SPI 0, and allocate it to the following GPIO pins
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define SPI_PORT spi0
#define PIN_MISO 16
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9

#define PWM_EXTERNAL_SERVO_PIN 5
#define TRIG_PIN 6
#define ECHO_PIN 7
#define PWM_EXTERNAL_LED_PIN 14

#define ENC_SW 17
#define ENC_DT 18
#define ENC_CLK 19

#define AVOIDANCE 2

uint64_t start_time = 0;
uint64_t end_time = 0;
uint64_t distance = 0;


// int val = 0;

bool screen = 0;
uint selection = 0;

void ping(uint gpio)
{
    gpio_put(TRIG_PIN, 1);
    sleep_us(15);
    gpio_put(TRIG_PIN, 0);
}


void gpio_callback(uint gpio, uint32_t events)
{
    if (gpio == ECHO_PIN)
    {
        if  (events & GPIO_IRQ_EDGE_RISE)
            start_time = to_us_since_boot(get_absolute_time());
        else if (events & GPIO_IRQ_EDGE_FALL)
        {
            end_time = to_us_since_boot(get_absolute_time());
            uint64_t dtime = end_time - start_time;
            distance = dtime/58;
        }
    }
    else if (gpio == ENC_SW)
    {
        selection = (selection + 1) %  6;
    }
    else if (gpio == 16)
    {
        screen = !screen;
    }
    else if (gpio == ENC_CLK)
    {
        // if (gpio_get(ENC_DT))
        //     val--;
        // else 
        //     val++;
    }

}

float read_temp()
{
    adc_select_input(4);
    const float conversionFactor = 3.3f / (1 << 12);
    float adc = (float)adc_read() * conversionFactor;
    float tempC = 27.0f - (adc - 0.706f) / 0.001721f;
    return tempC;
}

const char day_strs[7][5] = {
    " Sun",
    " Mon",
    "Tues",
    " Wed",
    "Thur",
    " Fri",
    " Sat"
};
const char month_strs[12][4] =
{
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"
};



int main()
{
    stdio_init_all();

    // SPI initialisation. This example will use SPI at 1MHz.
    // spi_init(SPI_PORT, 1000*1000);
    // gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    // gpio_set_function(PIN_CS,   GPIO_FUNC_SIO);
    // gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    // gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    
    // Chip select is active-low, so we'll initialise it to a driven-high state
    // gpio_set_dir(PIN_CS, GPIO_OUT);
    // gpio_put(PIN_CS, 1);
    



    // // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    lcd_init();
    lcd_clear();

    gpio_init(AVOIDANCE);
    gpio_set_dir(AVOIDANCE, GPIO_IN);

    gpio_init(TRIG_PIN);
    gpio_init(ECHO_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &gpio_callback);


    gpio_init(ENC_SW);
    gpio_set_dir(ENC_SW, GPIO_IN);
    gpio_pull_up(ENC_SW);
    // gpio_set_irq_enabled_with_callback(ENC_SW, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    gpio_init(ENC_CLK);
    gpio_set_dir(ENC_CLK, GPIO_IN);

    gpio_init(ENC_DT);
    gpio_set_dir(ENC_DT, GPIO_IN);

    // gpio_set_irq_enabled_with_callback(ENC_CLK, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);


    gpio_init(16);
    gpio_set_dir(16, GPIO_IN);
    gpio_pull_up(16);
    // gpio_set_irq_enabled_with_callback(16, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    
    gpio_set_function(PWM_EXTERNAL_SERVO_PIN, GPIO_FUNC_PWM);

    uint slice_num = pwm_gpio_to_slice_num(PWM_EXTERNAL_SERVO_PIN);
    
    pwm_config config = pwm_get_default_config();
    // sysclock / 4
    pwm_config_set_clkdiv(&config, 16.f);
    pwm_config_set_wrap(&config, 39999);

    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(PWM_EXTERNAL_SERVO_PIN, 12000);

    pwm_set_enabled(slice_num, true);


    gpio_set_function(PWM_EXTERNAL_LED_PIN, GPIO_FUNC_PWM);

    uint slice_num2 = pwm_gpio_to_slice_num(PWM_EXTERNAL_LED_PIN);
    
    pwm_config config2 = pwm_get_default_config();
    // sysclock / 4
    pwm_config_set_clkdiv(&config2, 1.f);

    pwm_init(slice_num2, &config2, true);
    pwm_set_gpio_level(PWM_EXTERNAL_LED_PIN, 65535);

    pwm_set_enabled(slice_num2, true);


    adc_init();

    adc_set_temp_sensor_enabled(true);
    adc_gpio_init(26);
    adc_gpio_init(27);

    adc_select_input(0);

    uint16_t level = 0;
    int degree = 0;
    pwm_set_gpio_level(PWM_EXTERNAL_SERVO_PIN, degree * 84 + 5000);

    printf("hgello?\n");

    const char * s = "hello world";
    lcd_clear();

    float temp = 0.0;
    
    datetime_t t = {
        .year = 2023,
        .month = 1,
        .day = 1,
        .dotw = 0,
        .hour = 16,
        .min = 45,
        .sec = 00
    };
    rtc_init();
    rtc_set_datetime(&t); 

    bool prev_state_clk = gpio_get(ENC_CLK);
    bool prev_state_enc_sw = gpio_get(ENC_SW);
    bool prev_state_joy_sw = gpio_get(16);
    int val = 0;
    int run = 0;
    while (true)
    {

        
        if (run % 25 == 0) //debounce for 25 ms for switches
        {
            bool cur_joy_state = gpio_get(16);
            if (cur_joy_state != prev_state_joy_sw && !cur_joy_state)
            {
                screen = !screen;
            }

            bool cur_enc_state = gpio_get(ENC_SW);
            if (cur_enc_state != prev_state_enc_sw && !cur_enc_state)
            {
                selection = (selection + 1) % 6;
            }  

            prev_state_enc_sw = gpio_get(ENC_SW);
            prev_state_joy_sw = gpio_get(16);


        }
        bool cur_clk_state = gpio_get(ENC_CLK);
        if (cur_clk_state != prev_state_clk && cur_clk_state)
        {
            if(gpio_get(ENC_DT))
            {  
                val++;
            }
            else
            {
                val--;
            }
        }
        prev_state_clk = cur_clk_state;

        if (run % 50 == 0) //dont need to read adc inputs every ms
        {
            adc_select_input(0);
            uint16_t result = adc_read();
            degree = ((result-20)*180)/4000;
            if (degree > 95)
                degree += ((degree-90)*12)/90; // over the remaining 90 degrees add 12 extra degrees
            if (degree < 95)
                degree -= 2;
            if (degree > 180)
                degree = 180;
            if (degree < 0)
                degree = 0;
            pwm_set_gpio_level(PWM_EXTERNAL_SERVO_PIN, degree * 84 + 5000);

            adc_select_input(1);
            uint16_t result2 = adc_read();
            pwm_set_gpio_level(PWM_EXTERNAL_LED_PIN, result2<<2);
        }


       
        sleep_ms(1); //recommended to do > 50 ms between ping times for distance sensor
        // printf("Distance is %llu\n", distance);
        char buf[40] = 
        {0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 
         0, 0, 0, 0, 0, 
         0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 
         0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 
         0, 0, 0, 0, 0};

        if (run % 100 == 0)
        {
            if (!gpio_get(AVOIDANCE))
                gpio_put(25, 0);
            else 
                gpio_put(25, 1);
        }

        if(run % 100 == 0) //update clock, ping
        {
            rtc_get_datetime(&t);
            if (val != 0)
            {
                // if (val < 0)
                //     val = -1;
                // else 
                //     val = 1;
                if (selection == 0)
                    t.year = abs(t.year+val)%3000;
                else if (selection == 1)
                    t.month = abs(t.month+val)%12;
                else if (selection == 2)
                {
                    t.day = abs(t.day+val)%31;
                    t.dotw = abs(t.dotw+val)%7;
                }
                else if (selection == 3)
                    t.hour = abs(t.hour+val)%24;
                else if (selection == 4)
                    t.min = abs(t.min+val)%60;
                else if (selection == 5)
                    t.sec = abs(t.sec+val)%60;
                
                rtc_set_datetime(&t);
                val = 0;

                
            }
            ping(TRIG_PIN);//recommended to do > 50 ms between ping times for distance sensor
            if(screen)
            {
                lcd_set_cursor(0, 0);
                sprintf(buf, "Distance %llu cm   ", distance);
                lcd_string(buf);
                temp = read_temp();
                lcd_set_cursor(1, 0);
                sprintf(buf, "Temp %.02f C     ", temp);
                lcd_string(buf);
            }
            else 
            {
                lcd_set_cursor(0,0);

                sprintf(buf, "%s %02d %s %0002d", day_strs[t.dotw%7], t.day, month_strs[t.month % 12], t.year);
                lcd_string(buf);
                lcd_set_cursor(1, 0);
                sprintf(buf, "    %02d:%02d:%02d    ", t.hour, t.min, t.sec);
                lcd_string(buf);
            }
            char c = '>';
            if (selection == 0)
                lcd_set_cursor(0,11);
            else if (selection == 1)
                lcd_set_cursor(0,7);
            else if (selection == 2)
                lcd_set_cursor(0,4);
            else if (selection == 3)
                lcd_set_cursor(1,3);
            else if (selection == 5)
            {
                lcd_set_cursor(1,12);
                c = '<';
            }
            else if (selection == 4)
            {
                lcd_set_cursor(1,14);
                c = ' ';
            }
            if (screen == 0)
            {
                buf[0] = c;
                buf[1] = 0;
                lcd_string(buf);
            }
        }

        




        run++;


        // printf("adc %d, %d (%d)\n", result, result2, degree);
    }
    




    return 0;
}
