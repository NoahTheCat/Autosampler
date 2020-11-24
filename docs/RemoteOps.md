---
title: Remote Operation
nav_order: 4
---

# Using the Sampler over the internet

The sampler can be connected to the internet to:

1. Update the bag filling interval
2. Start/stop a sampling sequence
3. Take a single sample

NOTE: you can take a single sample while a regular sequence is running so long as it doesn't clash with the sechduled sample.

## Connecting to the internet

For instructions on how to set tp the WiFi connection see: **LINK**

By default the sampler is normally DISCONNECTED from the internet, and the main LED should be pulsing white (it's a RGB LED so you can still see the other colors a bit).

To connect the sampler to the internet press the Connect to Internet switch, the small LED near the USB connector should light up blue. At the next safe moment the sampler will try to connect to the internet.  The main LED will flash green while connecting to the local WiFi and then cyan when connecting to the device cloud.  When it has connected properly the main LED will slowly pulse cyan - it's like a very slow heartbeat.

**NOTE:** The sampler will NOT try to connect to the internet if it is mid sequence.  To go online, stop the sampling sequence, push the switch, and then re-start the sequence once connected.



## Controlling via the Particle Cloud console



