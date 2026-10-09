# Model Subway Train Project

### Project Summary

This project is a custom-built, affordable model train control system powered by ESP32 microcontrollers. Its goal is to combine realistic model railway operation with embedded electronics, wireless communication, NFC-based track identification, autonomous train control, and a modern graphical interface.
<p align="center">
  <img src="https://github.com/mostlytoast/Motor_Control_ESP32_AP/blob/main/pictures/train_and_controller.png?raw=true" width="100%" alt="Train and controller">
</p>

#### Affordability

The entire system can be built for under **$40**, making it significantly more affordable than comparable systems that cost $300 or more, while offering advanced automation and extensive customizability.

#### Control

Multiple trains can be controlled using an intuitive handheld controller or a website accessible from a phone. Both interfaces offer the same features through a shared UI framework built with **LVGL, WebAssembly, and Emscripten**.

The system is designed to be modular and easy to customize. New features and interface layouts can be added or modified without needing to rewrite the underlying control logic.

<p align="center">
  <img src="https://github.com/mostlytoast/Motor_Control_ESP32_AP/blob/main/pictures/controls.png?raw=true" width="50%" alt="Model train control interface">
</p>

#### Track Detection and Mapping

NFC tags embedded in track segments allow an equipped train to identify each segment it travels over. By scanning these tags and recording their connections, the system can build a map of the railway network, including routes, curves, and junctions.

Future releases will use this map to enable more advanced autonomous operation, including collision avoidance, route planning, and realistic station-to-station travel.

<p align="center">
  <img src="https://github.com/mostlytoast/Motor_Control_ESP32_AP/blob/main/pictures/map.png?raw=true" width="50%" alt="Automatically generated railway track map">
</p>

#### Realistic Audio and Movement

The system can recreate the sounds and movement characteristics of different types of trains. Steam locomotives produce speed-dependent chuffing sounds and whistle effects, while diesel locomotives can sound their horns as their engines accelerate.

Trains can also accelerate and decelerate gradually, simulating the inertia of real locomotives rather than changing speed instantaneously. These features help create a more realistic and immersive model railway experience.

#### Easy To Build

Using pioArduino you can easily upload the code to your esp32 with the touch of one button. 