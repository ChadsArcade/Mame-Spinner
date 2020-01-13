# Mame-Spinner
Spinner controller for MAME

This started as a project to get a spinner working in my DOS ZVG MAME cab.

First efforts used a Leonardo as they are very easy to use as HID devices. Whilst this works under a Windowed OS, DOS does not see the device as a mouse. I think this is because the Arduino is seen as multiple USB devices.

The next attempt was to use the Arduino as a PS/2 mouse emulator, using the Arduino "PS2Dev" library. Whilst I could get this to work well under Windows (XP), DOS again did not play ball. The device was recognised, but would lock up. Debug suggests this may be a problem with the PS2Dev library, the "mouse" gets into a state where it thinks the host wants to send it a command, and it waits and waits for a command that never comes.

Next approach was to use an Arduino UNO with the USB chip firmware flashed to a HID Mouse. Important - you need an Arduino with an Atmel 16u2 USB chip, many clones have a CH340G chip and these won't work!! DOS sees such a flashed device as a mouse so this approach works. However, one of my ZVG cabs continually crashed when using the Spinner. The reason for this appears to be that several devices (including USB) share IRQs, and my motherboard does not have the options to reserve or allocate these :(

Other options would be (non exhaustive list):
Try HoodLoader2, though I thnik this may have the same issue as the Leonardo.
Change the motherboard (my other ZVG cab works fine)
Perform a "mouse hack" and hack the rotary encoder quadrature signals onto a mouse PCB
Moving from DOS to Linux ZVG Mame.

I've opted for now to swap the PCs around in my cabs so the Multivector one has the motherboard which likes the USB Spinner, the other one is in an Asteroids and doesn't have a Spinner friendly CP anyway.

The rotary encoder used is a 600PPR photoelectric device from eBay, costing around £11 UKP. As there is no provision for a flywheel, a solid aluminium knob was used which adds some weight and allows free spinning with a reasonable amount of decay time (approx £5 from eBay).

The USB version works very well under Linux/Windows. However, DOS (at least the archaic version 0.104 I have in my ZVG cab) is waaaaay more sensitive to the pulses though and was very skittish. As such, a "divider" option was added to the USB version code. If you ground pin D8 you divide the Pulses/Revolution by 2, Gnd D9 for division by 4, and D10 for Division by 8.

You will also need to tweak the MAME analog device sensitivity and find a suitable value which avoids backspin and feels about the right speed. Google will help you find resources on expected PPR values for various games.

The encoder reading is done via an interrupt routine and has been found to be very reliable with no backspin under a Windowed OS. A second ISR is included in the code which samples on rising and falling edges, thus doubling the PPR count, should you wish to experiment. You could also sample the "B" pin and double the PPR count again.

A metal plate with a few holes drilled in it will provide a suitable mount for a Takeman ZVG multivector CP (Tempest mounting holes).
