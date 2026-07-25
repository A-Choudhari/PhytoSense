# Plant Environment Logger (ESP-IDF / FreeRTOS)

## Status
Codebase scaffolded pre-hardware. All sensor/SD I/O is behind a
`MOCK_HARDWARE` flag (see `sensors.c` / `logger.c`) so the FreeRTOS task
structure, queue-based producer/consumer pattern, and logging format can
be built and sanity-checked in simulation before the board arrives.

## Structure
- `main/sensors.{h,c}` — soil moisture (ADC) + BME280 (I2C) reads.
  Mocked with plausible fake values for now.
- `main/logger.{h,c}` — CSV logging, currently to console; will move to
  SD over SPI in Week 2.
- `main/main.c` — `app_main`, creates the reading queue and the two
  FreeRTOS tasks (sampling = producer, logging = consumer).

## Switching to real hardware (once board + parts arrive)
1. In `sensors.c` and `logger.c`, flip `MOCK_HARDWARE` to `0`.
2. Work through the `TODO` comments in the `#else` branches — these are
   the real ADC/I2C/SPI driver calls, left as an exercise rather than
   pre-written, since walking the datasheets is most of the point.
3. Confirm pin assignments against the DevKitC pinout before wiring.

## Plan
- Week 1: real ADC (soil) + I2C (BME280) reads, verify against mock
  pipeline already in place.
- Week 2: SD logging over SPI, button interrupt for manual events, deep
  sleep between samples.
- Week 3: power profiling (active vs. sleep current draw, set a battery
  life target), begin TinyML — collect a real dataset, train a small
  anomaly detector (Edge Impulse or TFLite Micro).
- Week 4: deploy trained model on-device, integrate anomaly flagging
  into the logging path, watchdog timer, final README with real data +
  power/inference numbers.

## Build (once ESP-IDF is installed and board is connected)
```
idf.py set-target esp32
idf.py build
idf.py -p <PORT> flash monitor
```
