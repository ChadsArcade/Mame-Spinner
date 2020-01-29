///////////////////////////////////////////////////////////////////////////////////
//                                                                               //
// HoodLoader2 Arduino USB Spinner using BootMouse                               //
// Chad Gray, Jan 2020                                                           //
//                                                                               //
// Requires an Arduino UNO flashed with HoodLoader2                              //
//                                                                               //
// See https://github.com/NicoHood/HoodLoader2/wiki/Arduino-Uno-Mega-16u2-Pinout //
// for the pinout connections for the 16u2                                       //
//                                                                               //
// Use a PhotoElectric Rotary Encoder connected to the 16u2 ICSP header          //
// (this is the 6 pin header nearest the USB port)                               //
//                                                                               //
//    Gnd o o                                                                    //
//      A o o B   If the direction is reversed, swap wires A and B over          //
//    +5v o o                                                                    //
//                                                                               //
// Scale factor can be set by grounding one of the D4-D6 pins on the small       //
// 4-way header next to the ICSP connector. This will scale down the             //
// effective PPR by the amount shown:                                            //
//                                                                               //
//    1/2 PB6 o o PB7 n/c                                                        //
//    1/4 PB4 o o PB5 1/8                                                        //
//                                                                               //
// Flash this program to the 16u2 chip!                                          //
// Board: Hoodloader2 16u2                                                       //
// Board / BootLoader / USB-Core:"HoodLoader2 Uno"                               //
// Port: {port} (HoodLoader2 16u2)                                               //
//                                                                               //
///////////////////////////////////////////////////////////////////////////////////

#include "HID-Project.h"          // Add this under "Manage Libraries"
#include "PinChangeInterrupt.h"   // Add this under "Manage Libraries"
#define        debug        0     // Set to 1 for debug output to console

#define        Spin_A       1     // Hardware interrupt 1, digital pin PB1
#define        Spin_B       2     // Hardware interrupt 2, digital pin PB2

volatile float encoderPos = 0;    // stores our current value of encoder position.
volatile byte  AB_Pins    = 0;    // stores the direct values we read from our interrupt pins

float          increment  = 0.5;  // How much to add to the encoderPos per pulse, used to scale down resolution
byte           scale      = 0;    // Scale factor used for calculating increment

unsigned long  start_ms;          // Last time we sent mouse data
unsigned long  current_ms;        // Current time, used to check how long has elapsed since we last sent mouse data

const int      pinLed     = LED_BUILTIN;

////////////////////////////////////////////////////////////////////////////////
// Setup pins and interrupts                                                  //
////////////////////////////////////////////////////////////////////////////////
void setup()
{
   pinMode(pinLed, OUTPUT);
   digitalWrite(pinLed, HIGH);

   pinMode(Spin_A, INPUT_PULLUP); // set Spin_A as an input, pulled HIGH
   pinMode(Spin_B, INPUT_PULLUP); // set Spin_B as an input, pulled HIGH
   attachPinChangeInterrupt(Spin_A, doEncoderRising, RISING);

   pinMode(4, INPUT_PULLUP);      // Input pin to set scale factor
   pinMode(5, INPUT_PULLUP);      // Input pin to set scale factor 
   pinMode(6, INPUT_PULLUP);      // Input pin to set scale factor

   #if (debug)
      Serial.begin(57600);
      Serial.println("BootMouse initialised...");
   #endif

   BootMouse.begin();
}


////////////////////////////////////////////////////////////////////////////////
// Main Program Loop                                                          //
////////////////////////////////////////////////////////////////////////////////
void loop()
{
   current_ms=millis();    // Record the current time so we an determine how long since we last sent mouse data
   // If 25ms have elapsed since we last sent mouse data, send updated data
   if ((current_ms - start_ms) >= 25)
   {
      scale = (~PINB & 0b01110000) >> 3;
      if (scale == 0) scale=1;
      increment=(1.0/scale);
      #if (debug)
         Serial.print("Pos: \t");
         Serial.print((int)encoderPos);
         Serial.print("\tScale: \t");
         Serial.println(scale);
      #endif

      noInterrupts();
      #if (!debug)
        BootMouse.move((int)encoderPos, 0);
        encoderPos=encoderPos-(int)encoderPos;  // Keep the fractional part
      #endif
      digitalWrite(pinLed, (int)encoderPos%2);
      start_ms=current_ms;
      interrupts();
   }
}


///////////////////////////////////////////////////////////////////
// Interrupt routine for Spin_A - samples on RISING only         //
// This will read x1 PPR resolution                              //
///////////////////////////////////////////////////////////////////
//void doEncoderRising()
void doEncoderRising(void)
{
   noInterrupts();                //stop interrupts happening before we read pin values
   digitalWrite(pinLed, LOW);
   AB_Pins = PINB & 0b00000110;   // mask off the 2 pins of interest
   if (AB_Pins == 0b00000110)     // Check if Spin_A == Spin_B
   {
      encoderPos+=increment;
   }
   else
   {
      encoderPos-=increment;
   }
   digitalWrite(pinLed, HIGH);
   interrupts();                  //restart interrupts
}
