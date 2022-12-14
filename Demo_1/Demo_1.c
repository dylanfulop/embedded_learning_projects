#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "pico/multicore.h"


const uint16_t PWM_EXTERNAL_LED_PIN = 15;
uint32_t last_bounce = 0;
const int rows[4] = {7, 6, 5, 4};
const int cols[4] = {3, 2, 1, 0};
bool state = 1;
const int digs[4] = {13, 17, 9, 14};
const int segs[7] = {20, 28, 8, 18, 27, 21, 19}; //{18, 19, 20, 21, 27, 28, 8};
char word[4] = "    ";

const char letters[37] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z', ' '};
const int conversion[37] =
    {
        0b1110111, // 0
        0b1000100, // 1
        0b0111110, // 2
        0b1101110, // 3
        0b1001101, // 4
        0b1101011, // 5
        0b1111011, // 6
        0b1000110, // 7
        0b1111111, // 8
        0b1001111, // 9
        0b1011111, // a
        0b1111001, // b
        0b0110011, // c
        0b1111100, // d
        0b0111011, // e
        0b0011011, // f
        0b1111011, // g
        0b1011001, // h
        0b0010000, // i
        0b1100100, // j
        0b1011101, // k
        0b0110001, // L
        0b1011000, // m
        0b1011000, // n
        0b1111000, // o
        0b0011111, // p
        0b1001111, // q
        0b0011000, // r
        0b1101011, // s
        0b0011001, // t
        0b1110101, // u
        0b1110000, // v
        0b1110000, // w
        0b1011101, // x
        0b0011101, // y
        0b0111110,  // z
        0b0000000 //' '
};
const char grid[4][4] = 
{
   "123A", "456B", "789C", "E0FD"
};
int chars_to_int(char *c)
{
    return (int)c[0]<<24 | (int)c[1]<<16 | (int)c[2]<<8 | (int)c[3];
}
void int_to_chars(int i, char *buf)
{
    buf[0] = (char)(i>>24);
    buf[1] = (char)(i>>16);
    buf[2] = (char)(i>>8);
    buf[3] = (char)(i);
}

void write_letter(char letter, int slot)
{
    if (letter == ' ')
        letter = 36;
    else if (letter < 58)
        letter -= 48;
    else if (letter < 91)
        letter -= 55; // 65-10;
    else if (letter < 123)
        letter -= 87; // 97-10
    if (letter >= 37 || letter < 0)
        letter = letter % 37;

    int con = conversion[letter];

    for (int i = 0; i < 7; i++)
    {
        if (con & 1 << i)
            gpio_put(segs[i], 1);
        else
            gpio_put(segs[i], 0);
    }
    for (int i = 0; i < 4; i++)
    {
        gpio_put(digs[i], 1);
    }
    gpio_put(digs[slot % 4], 0);
}

void gpio_callback(uint gpio, uint32_t events)
{
    if (gpio == 16)
    {
        uint32_t time = to_ms_since_boot(get_absolute_time());
        if (time - last_bounce > 50)
        {
            last_bounce = time;
            if (!(GPIO_IRQ_EDGE_FALL & events))
                state = !state;    

        }
    }
    else
    {
        gpio_set_irq_enabled(gpio, GPIO_IRQ_EDGE_FALL, false);
        gpio_set_dir(gpio, GPIO_OUT);
        int row = 0;
        for (int i = 0; i < 4; i++)
            if (rows[i] == gpio)
                row = i;

        gpio_put(gpio, 1);
        for (int i = 0; i < 4; i++)
        {
            gpio_set_dir(cols[i], GPIO_IN);
            gpio_pull_down(cols[i]);
            if (gpio_get(cols[i]))
            {
                printf("%d, %d\n", row, i);
                word[0] = word[1];
                word[1] = word[2];
                word[2] = word[3];
                word[3] = grid[row][i];
                multicore_fifo_push_blocking(chars_to_int(word));

            }
        }
        for (int i = 0; i < 4; i++)
        {
            gpio_set_dir(cols[i], GPIO_OUT);
            gpio_put(gpio, 0);
        }
        gpio_set_dir(gpio, GPIO_IN);
        gpio_pull_up(gpio);
        gpio_set_irq_enabled_with_callback(gpio, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
        printf("gpio %d\n", gpio);
    }
}



void core1_entry() {
  uint32_t d;
  int word = multicore_fifo_pop_blocking();
  char buf[5];
  int runs = 0;
  int_to_chars(word, buf);
  while (true) {
    runs++;
    if (runs % 5 == 0)
    {
        if(multicore_fifo_rvalid())
            word = multicore_fifo_pop_blocking();
            int_to_chars(word, buf);
    }
    for (int i = 0; i < 4; i++)
    {
        write_letter(buf[i], i);
        sleep_ms(5);
    }

  }
}

int main()
{
    stdio_init_all();
    gpio_set_function(PWM_EXTERNAL_LED_PIN, GPIO_FUNC_PWM);

    multicore_launch_core1(core1_entry);


    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    gpio_put(25, 1);

    gpio_init(16);
    gpio_set_dir(16, GPIO_IN);
    gpio_pull_down(16);

    gpio_set_irq_enabled_with_callback(16, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    for (int i = 0; i < 4; i++)
    {
        gpio_init(rows[i]);
        gpio_set_dir(rows[i], GPIO_IN);
        gpio_pull_up(rows[i]);
        gpio_set_irq_enabled_with_callback(rows[i], GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    }
    for (int i = 0; i < 4; i++)
    {
        gpio_init(cols[i]);
        gpio_set_dir(cols[i], GPIO_OUT);
        gpio_put(cols[i], 0);
    }
    for (int i = 0; i < 4; i++)
    {
        gpio_init(digs[i]);
        gpio_set_dir(digs[i], GPIO_OUT);
        gpio_put(digs[i], 1);
    }
    for (int i = 0; i < 7; i++)
    {
        gpio_init(segs[i]);
        gpio_set_dir(segs[i], GPIO_OUT);
        gpio_put(segs[i], 1);
    }

    uint slice_num = pwm_gpio_to_slice_num(PWM_EXTERNAL_LED_PIN);

    pwm_config config = pwm_get_default_config();
    // sysclock / 4
    pwm_config_set_clkdiv(&config, 1.f);

    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(PWM_EXTERNAL_LED_PIN, 65535);

    pwm_set_enabled(slice_num, true);

    puts("Hello, world!");

    adc_init();

    adc_gpio_init(26);
    adc_select_input(0); // (GPIO26)

    int runs = 0;
    int sel = 0;
    int let = 0;
    multicore_fifo_push_blocking(chars_to_int(word));
    while (true)
    {
        uint16_t result = adc_read();
        result = result << 4;
        pwm_set_gpio_level(PWM_EXTERNAL_LED_PIN, result);
        gpio_put(25, state);
        sleep_ms(50);
        runs++;
    }

    return 0;
}
