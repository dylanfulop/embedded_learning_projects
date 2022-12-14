/*
# Make sure to include "pico_multicore" in the "target_link_libraries" in CMakeLists.txt
# e.g. target_link_libraries(hello_multicore pico_stdlib pico_multicore)
# Where "hello_multicore" is the name of the project
# You may need to re-run "cmake .." from the build folder
*/

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include <stdio.h>

const uint LED_PIN25 = 25;
const uint LED_PIN15 = 16;

void core1_entry() {
  uint32_t d;

  gpio_init(LED_PIN15);
  gpio_set_dir(LED_PIN15, GPIO_OUT);
  while (true) {
    d = multicore_fifo_pop_blocking();
    gpio_put(LED_PIN15, d);
  }
}

int main() {
  stdio_init_all();

  multicore_launch_core1(core1_entry);

  gpio_init(LED_PIN25);
  gpio_set_dir(LED_PIN25, GPIO_OUT);

  while (true) {
    gpio_put(LED_PIN25, 1);
    multicore_fifo_push_blocking(1);
    printf("ON\n");
    sleep_ms(1000);
    gpio_put(LED_PIN25, 0);
    multicore_fifo_push_blocking(0);
    printf("OFF\n");
    sleep_ms(1000);
  }
  return 0;
}