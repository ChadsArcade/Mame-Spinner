# Mame/Arcade Spinner
Spinner controller for MAME

This started as a project to get a spinner working in my DOS ZVG MAME cab.

The rotary encoder should be connected to 0v and +5v, and the A and B signals connected to pins D2 and D3. This allows for the use of hardware interrupts.

#### First attempt: Arduino Leonardo / Pro Micro 

First efforts used a Pro Micro as they are very easy to use as HID devices - simply include Mouse.h and do a mouse.move(x,0,0). Whilst this works under a Windowed OS, DOS did not see the device as a mouse. I think this may be because the Arduino presents itself as multiple USB devices.

The code for this is included as ProMicro-Spinner.ino and is the easiest solution for those wanting a USB working under a Windows/Linux environment. The device will be seen as a mouse and will move the cursor along the X axis. The code is heavily commented and details the connections to be made to the spinner, and the option to scale up or down the sensitivity. 

#### Second attempt: Arduino emulating a PS/2 Mouse

The next attempt was to use the Arduino as a PS/2 mouse emulator, using the Arduino "PS2Dev" library. Whilst I could get this to work well under Windows (XP), DOS again did not play ball. The device was recognised, but would either go crazy (Spinning all over the place and pressing fire) or lock up. Debug suggests the lock up may be a problem with the PS2Dev library, the "mouse" gets into a state where it thinks the host wants to send it a command, and it waits and waits for a command that never comes.

You can monitor the chat from the host to mouse by setting debug to 1 in the code. This lets you see the handshake when the mouse driver loads, something which seems to re-occur when a spinner enabled game loads.

#### Third attempt: Arduino UNO flashed as a USB Mouse

Next approach was to use an Arduino UNO with the USB chip firmware flashed to a HID Mouse. Important - you need an Arduino with an Atmel 16u2 USB chip which can be put into DFU mode. Many clones have a CH340G chip and these won't work!! Some clones have the 16u2 chip but won't enter DFU mode (I have an Elegoo branded UNO which does this), though it may be possible to flash them via the ICSP header - this has not yet been tested.

You can get the mouse firmware (and serial USB firmware to turn it back into an Arduino) from: https://github.com/harlequin-tech/arduino-usb

Detailed instructions on the DFU flashing process can be found here: http://anhnguyen.me/2014/01/turn-arduino-uno-r3-into-a-mouse/

Tip: I put the 3 commands to flash either firmware into 2 shell scripts, "GoArduino.sh" and "GoMouse.sh". 

DOS sees such a flashed device as a mouse (you probably need USB legacy device support enabled in the BIOS) so this approach works. However, one of my ZVG cabs continually crashed when using the Spinner. The reason for this appears to be that several devices (including USB, sound and video) share IRQs, and my motherboard does not have the options to reserve or re-allocate them :(

#### Other things to try

Other options would be (non exhaustive list):
* Try different DOS Mouse drivers and their various parameters
* Try HoodLoader2, though I think this may have the same issue as the Leonardo.
* Change the motherboard (my other ZVG cab works fine)
* Perform a "mouse hack" and hack the rotary encoder quadrature signals onto a mouse PCB
* Persevere with the PS/2 version and get it to work
* Move from DOS to Linux ZVG Mame.

I've opted for now to swap the PCs around in my cabs so the Multivector one has the motherboard which likes the USB Spinner, the other one is in an Asteroids Dlx cab and doesn't have a Spinner friendly CP anyway.

Update: I've successfully flashed the Elegoo UNO with HoodLoader2 so I'll experiment with this soon. "BootMouse" looks interesting.

#### Parts used

The rotary encoder used is a 600PPR photoelectric device from eBay, costing around £11 UKP. As there is no provision for a flywheel, a solid aluminium knob was used which adds some weight and allows free spinning with a reasonable amount of decay time (approx £5 from eBay). See the included pictures for details.

![alt text](https://raw.githubusercontent.com/ChadsArcade/Mame-Spinner/master/pics/Spinner1-Small.jpg "Endoer, knob and mounting plate")

#### Summary

The USB version works very well under Linux/Windows. However, DOS (at least the archaic version 0.104 I have in my ZVG cab) is waaaaay more sensitive to the pulses though and was very skittish. As such, a "divider" option was added to the USB version code. If you ground pin D8 you divide the Pulses/Revolution by 2, Gnd D9 for division by 4, and D10 for Division by 8.

You will also need to tweak the MAME analog device sensitivity and find a suitable value which avoids backspin and feels about the right speed. This page gives details on expected PPR values for various games: https://web.archive.org/web/20160421175734/http://wiki.arcadecontrols.com/wiki/Spinner_Turn_Count

The encoder reading is done via an interrupt routine and has been found to be very reliable with no backspin under a Windowed OS. A second ISR is included in the code which samples on rising and falling edges, thus doubling the PPR count, should you wish to experiment. You could also sample the "B" pin and double the PPR count again.

#### Panel mounting

A metal plate with a few holes drilled in it will provide a suitable mount for a Takeman ZVG multivector CP (= Tempest mounting holes). I made a template by cutting around a 1 pence piece on a piece of cardboard, that gave the exact size for the centre part of the encoder. Then poke a pen through the card where the mounting holes are to get the positions for the screws. Finally, tape the card to your metal sheet and centrepunch where the holes are. The centre hole needs to be a touch over 20mm diameter for the encoder to sit flush.
