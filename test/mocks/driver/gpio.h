#pragma once

using gpio_num_t = int;
using esp_err_t = int;

esp_err_t gpio_hold_dis(gpio_num_t pin);
esp_err_t gpio_hold_en(gpio_num_t pin);
void gpio_deep_sleep_hold_en();
