# , Model Subway Train Project

### Project Summary

The project is a custom-built, modern, affordable model train control system powered by ESP32 microcontroller. Its goal is to combine realistic model railway operation with embedded electronics, wireless communication, NFC-based track identification to enable automatic train control, and a modern graphical interface.

#### Affordability 
This entire system can be built for under 40$ far cheaper then 300$ equivalents which lack the same advanced automatic control and customizability. 

#### Control
Multiple trains can be controlled from an intuitive handheld controller or through a website on your phone. Both offer the same features and ease of use thanks to the use of a write once deploy anywhere framework utilizing LVGL, WebAssembly, and Emscript. Designed with easy modularity any user feature can be easily added or layout modified without needing to modify the underlying backend.

#### Track Detection
Using NFC tags inside track segments a train with an equipped nfc reader can identify each track it goes over and like a rat in a maze map out all routes and junctions anonymously. In future releases this will enable more complex autonomous train control allowing for collision avoidance, route planning, and realistic station to station travel. 

#### Realistic Audio and Movement
Any train can be realistically recreated at small scale with realistic audio sounds and queues. Steam engines chuff along matching their current speed and blow their whistles, while diesel trains can blow their horns as their engines ramp up. Trains are also able to ramp up in speed or slow down much like real trains due with their massive inertia.
 