# SpinIt3D

# Digital Centrifuge Capstone
This repository contains the Arduino IDE firmware and touchscreen UI headers for a biomedical engineering capstone project: a digitally controlled centrifuge using a Raspberry Pi Pico/Pico W, SPI TFT touchscreen, motor speed control, and safety interlocks.

## Safety Disclaimer

This project is a biomedical engineering capstone prototype and is not intended for clinical, diagnostic, or commercial use. High-speed rotating systems can be dangerous. Proper mechanical containment, balancing, interlocks, and supervised testing are required.

## Features
- Touchscreen UI with home, safety, and test screens
- Motor speed control through PWM-to-0–10V module
- Lid safety interlock
- Solenoid control
- RPM, temperature, and vibration sensor support
- Emergency/safety shutdown logic

## Hardware
- Raspberry Pi Pico / Pico W / Pico 2 W
- 480x320 SPI TFT display
- XPT2046 resistive touch controller
- PWM-to-0–10V digital potentiometer
- 100V DC motor power supply
- 24V DC power supply unit
- 5V DC step-down converter
- 3.3V step-down converter
- MOSFET for the solenoid
- Solenoid
- Power switch
- hall effect sensor
- Temperature sensor
- Vibration sensor
- RPM IR or tachometer sensor
- 3 cord power cable
- 100V DC brushed motor

## Firmware Location
Open this file in Arduino IDE:

firmware/centrifuge_controller/centrifuge_controller.ino

## Required Libraries

- TFT_eSPI
- XPT2046_Touchscreen
- SPI

## Display Configuration

The project was tested with a 480x320 TFT display using TFT_eSPI.

Recommended TFT_eSPI setup:
- ILI9486 or ILI9488 driver
- SPI MOSI: GP19
- SPI MISO: GP16
- SPI SCK: GP18
- TFT CS: GP17
- TFT DC: GP21
- TFT RST: GP20
- Touch CS: GP22

## Notes
The UI images are stored as C header files generated from bitmap assets.
