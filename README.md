# Mame/Arcade Spinner
Spinner controller for MAME/Emulators under various operating systems

![alt text](https://raw.githubusercontent.com/ChadsArcade/Mame-Spinner/master/Spinner3-small.jpg "Completed Spinner")


This started as a project to get a spinner working in my DOS ZVG MAME cab. I wanted to build something other people could replicate exactly, rather than use a mouse hack with an unknown sensitivity/PPR count. A photoelectric rotary encoder is used along with an Arduino to simulate a 1-axis mouse (= Spinner control).


#### First attempt: Arduino Leonardo / Pro Micro 

First efforts used a Pro Micro as they are very easy to use as HID devices - simply include Mouse.h and do a mouse.move(x,0,0). Whilst this works under a Windowed OS, DOS did not see the device as a mouse. I think this may be because the Arduino presents itself as multiple USB devices rather than as a simple mouse device.

The code for this is included as ProMicro-Spinner.ino and is the easiest solution for those wanting a USB spinner working under a Windows/Linux environment. The device will be seen as a mouse and will move the cursor along the X axis. The code is heavily commented and details the connections to be made to the spinner, and the options to scale up or down the sensitivity. 

#### Second attempt: Arduino emulating a PS/2 Mouse

The next attempt was to use the Arduino as a PS/2 mouse emulator, using the Arduino "PS2Dev" library. Whilst I could get this to work well under Windows (XP), DOS again did not play ball. The device was recognised, but would either go crazy (Spinning all over the place and pressing fire) or lock up. Debug suggests the lock up may be a problem with the PS2Dev library, the "mouse" gets into a state where it thinks the host wants to send it a command, and it waits and waits for a command that never comes.

You can monitor the chat from the host to mouse by setting debug to 1 in the code. This lets you see the handshake when the mouse driver loads, something which seems to re-occur when a spinner enabled game loads.

Here's the chat that occurs under Win XP when the mouse is reset:

```
PS2 Mouse emulator Started...
   Send -> : 0xAA : Self Test Passed
   Send -> : 0x00 : I am a standard PS/2 Mouse
-> Recv    : 0xFF : Mouse Reset
   Send -> : 0xAA : Self Test Passed
   Send -> : 0x00 : I am a standard PS/2 Mouse
-> Recv    : 0xF2 : Get Device ID
   Send -> : 0x00 : I am a standard PS/2 Mouse
-> Recv    : 0xE8 : Set Resolution...
-> Recv    : 0x00 : ...to "0"
-> Recv    : 0xE6 : Set Scaling 1:1
-> Recv    : 0xE6 : Set Scaling 1:1
-> Recv    : 0xE6 : Set Scaling 1:1
-> Recv    : 0xE9 : Send Status Request
   Send -> : 0x00 : Status 1: (Flags)
   Send -> : 0x00 : Status 2: (Resolution)
   Send -> : 0x64 : Status 3: (Rate)
-> Recv    : 0xE8 : Set Resolution...
-> Recv    : 0x03 : ...to "3"
-> Recv    : 0xF2 : Get Device ID
   Send -> : 0x00 : I am a standard PS/2 Mouse
-> Recv    : 0xF3 : Set Sample Rate...
-> Recv    : 0x64 : ...to "100" (Report every 10ms)
-> Recv    : 0xE8 : Set Resolution...
-> Recv    : 0x03 : ...to "3"
-> Recv    : 0xF4 : Enable Data Reporting - Mouse enabled
```

#### Third attempt: Arduino UNO flashed as a USB Mouse

Next approach was to use an Arduino UNO with the USB chip firmware flashed to a HID Mouse. Important - you need an Arduino with an Atmel 16u2 USB chip which can be put into DFU mode. Many clones have a CH340G chip and these won't work!! Some clones have the 16u2 chip but won't enter DFU mode (I have an Elegoo branded UNO which does this), though it may be possible to flash them via the ICSP header - this has not yet been tested.

You can get the mouse firmware (and serial USB firmware to turn it back into an Arduino) from: https://github.com/harlequin-tech/arduino-usb

Detailed instructions on the DFU flashing process can be found here: http://anhnguyen.me/2014/01/turn-arduino-uno-r3-into-a-mouse/

Tip: I put the 3 commands to flash either firmware into 2 shell scripts, "GoArduino.sh" and "GoMouse.sh". 

DOS sees such a flashed device as a mouse (you probably need USB legacy device support enabled in the BIOS) so this approach works. Sometimes DOS mis-detects this as a Y-axis Spinner rather than an X-axis Spinner. This means you have to reconfigure games to look for movement on the other axis. A workaround may be to have the spinner update both axes... but this is a bit of a hack.

One of my ZVG cabs continually crashed when using this Spinner. The reason for this appears to be that several devices (including USB, sound and video) share the same IRQs, and my motherboard does not have the options to reserve or re-allocate them :(

#### Fourth attempt: Arduino UNO flashed with HoodLoader2

HoodLoader2 is the next step up from the method above which flashes the 16u2 as a mouse device. HoodLoader2 lets you configure the 16u2 as various USB HID devices (and more) whilst still retaining the ability to flash the Arduino itself. All the necessary code to turn the Arduino into a USB Spinner resides on the 16u2 chip itself, which leaves open the option of using the main CPU for other purposes.

My cheap Elegoo UNO which won't enter DFU mode successfully flashed HoodLoader2 and so is being used for this experiment.

Test results in the DOS ZVG Mame cab were unfortunately not good. DOS detected the device as a PS2 mouse (which is good) but MAME didn't see it at all. Subsequent reboots had DOS not see the device after all, only after a cold boot. I'm not sure what's goin gon here.

#### Other things to try

Other options would be (non exhaustive list):
* Try different DOS Mouse drivers and their various parameters - can lower sensitivity this way.
* Try HoodLoader2, ~~though I think this may have the same issue as the Leonardo.~~ DOS detects it intermittently.
* Change the motherboard (my other ZVG cab works fine)
* Perform a "mouse hack" and hack the rotary encoder quadrature signals onto a mouse PCB
* Persevere with the PS/2 version and get it to work
* ~~Move from DOS to Linux ZVG Mame.~~ discounted, see ZVG Menu pages.


Update: I've successfully flashed the Elegoo UNO with HoodLoader2 so I'll experiment with this soon. "BootMouse" looks interesting.

#### Current Status

I've opted for now to swap the PCs around in my cabs so the Multivector one has the motherboard which likes the USB Spinner, the other one is in an Asteroids Dlx cab and doesn't have a Spinner friendly CP anyway.

Currently I'm running with the UNO DFU code with the divider set to 8 (this gives an effective 75PPR, which is close to the Spinners used in Tempest (72PPR) and the Sega games (64PPR)). I'm using a logitech mouse driver with the parameters: AOFF S01
(Acceleration OFF, Sensitivity 01)

These settings enable you to use an analogue sensitivity of near 100% within MAME. Actual values should be:

    72 PPR Spinner - Tempest:    72/75 = 96%
    64 PPR Spinner - Sega/Midway 64/75 = 85%

Note that Zektor in particular seems to bias reading towards the CCW direction, in fact I found that if you are using a very low analogue sensitivity in MAME (to compensate for a high PPR Spinner) then CW turns are ignored altogether and you can only turn CCW. The same goes when you are using keys without a mouse even attached, so this looks like it's maybe a bug in the MAME driver, unless the original behaved like this?

#### Parts used

The rotary encoder used is a 600PPR photoelectric device from eBay, costing around £11 UKP. As there is no provision for a flywheel, a solid aluminium knob resembling that used in Tempest (approx £5 from eBay) was used which adds some weight and allows free spinning with a reasonable amount of decay time, though not quite as good as with a flywheel/more weight. See the included pictures for details.

![alt text](https://raw.githubusercontent.com/ChadsArcade/Mame-Spinner/master/Spinner1-small.jpg "Encoder, knob and mounting plate")

#### Summary

All versions work very well under Linux/Windows. However, DOS is more fussy and the DFU USB code seems to work best, though sometimes the device isn't detected correctly and needs to be hot plugged. DOS MAME (at least the archaic version 0.104 I have in my ZVG cab) is very sensitive to the pulse count and was very skittish. As such, a "divider" option was added to the  code. If you ground pin D8 you divide the Pulses/Revolution by 2, Gnd D9 for division by 4, and D10 for Division by 8. (See comments in the code for the pin assignments of the NicoHood version).

Division by 8 simulates a 75PPR spinner when using a 600PPR encoder, this is in the same ball park as the spinners used in Vector arcade games (typically 72 or 64).

You will also need to tweak the MAME analog device sensitivity and find a suitable value which avoids backspin and feels about the right speed. This page gives details on expected PPR values for various games: https://web.archive.org/web/20160421175734/http://wiki.arcadecontrols.com/wiki/Spinner_Turn_Count

The encoder reading is done via an interrupt routine and has been found to be very reliable with no backspin under a Windowed OS. A second ISR is included in the code which samples on rising and falling edges, thus doubling the PPR count, should you wish to experiment. You could also sample the "B" pin and double the PPR count again. You may need to decrease the time delay in sending the mouse data if using higher PPR spinners/ISRs.

#### Panel mounting

A metal plate with a few holes drilled in it will provide a suitable mount for a Takeman ZVG multivector CP (= Tempest mounting holes). I made a template by cutting around a 1 pence piece on a piece of cardboard, that gave the exact size for the centre part of the encoder. Then poke a pen through the card where the mounting holes are to get the positions for the screws. Finally, tape the card to your metal sheet and centrepunch where the holes are. The centre hole needs to be a touch over 20mm diameter for the encoder to sit flush.

![alt text](https://raw.githubusercontent.com/ChadsArcade/Mame-Spinner/master/Spinner2-small.jpg "Encoder mounted on plate")
