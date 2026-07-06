# Wiring Guide

Pin assignments mirror `firmware/include/config.h` — if you change one,
change both.

## QTR-8A Reflectance Sensor Array (8 channels, analog output)

| QTR-8A pin | ESP32 pin | Notes |
|---|---|---|
| CH1 | GPIO32 | ADC1_CH4 |
| CH2 | GPIO33 | ADC1_CH5 |
| CH3 | GPIO34 | ADC1_CH6 (input-only pin, fine for ADC) |
| CH4 | GPIO35 | ADC1_CH7 (input-only pin, fine for ADC) |
| CH5 | GPIO36 | ADC1_CH0 (input-only pin, fine for ADC) |
| CH6 | GPIO37 | ADC1_CH1 (input-only pin, fine for ADC) |
| CH7 | GPIO38 | ADC1_CH2 (input-only pin, fine for ADC) |
| CH8 | GPIO39 | ADC1_CH3 (input-only pin, fine for ADC) |
| LEDON | GPIO27 | digital output, emitter control |
| VCC | 5V (or 3.3V per your QTR-8A variant) | |
| GND | GND | |

**Do not move the 8 sensor channels to ADC2 pins (0,2,4,12-15,25-27)** —
ADC2 becomes unusable once WiFi is active on the ESP32, and this firmware
runs WiFi continuously.

## TB6612FNG Motor Driver

| TB6612FNG pin | ESP32 pin | Notes |
|---|---|---|
| AIN1 | GPIO16 | |
| AIN2 | GPIO17 | |
| PWMA | GPIO18 | LEDC PWM |
| BIN1 | GPIO19 | |
| BIN2 | GPIO21 | |
| PWMB | GPIO22 | LEDC PWM |
| STBY | GPIO23 | driven HIGH at boot to leave standby |
| VM | Motor battery voltage | |
| VCC | 3.3V | logic supply |
| GND | GND | shared with ESP32 GND |
| AO1/AO2 | Left motor | |
| BO1/BO2 | Right motor | |

## Board swap checklist

To use a different ESP32 board/variant:
1. Update `SENSOR_PINS` in `config.h` — must be 8 ADC1-capable pins.
2. Update the motor driver pins — avoid strapping pins (0, 2, 5, 12, 15 on
   classic ESP32; check your variant's datasheet for its own strapping
   pins, e.g. ESP32-S3 uses 0, 3, 45, 46).
3. Re-flash and re-run the Task 9 manual verification checklist.
