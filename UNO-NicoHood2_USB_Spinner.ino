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

volatile int   encoderPos = 0;    // stores our current value of encoder position.
int            delta_x    = 0;
volatile byte  AB_Pins    = 0;    // stores the direct values we read from our interrupt pins

byte           scale      = 0;    // Scale factor used for calculating increment

unsigned long  start_ms;          // Last time we sent mouse data
unsigned long  current_ms;        // Current time, used to check how long has elapsed since we last sent mouse data

const int      pinLed     = LED_BUILTIN;

///////////////////////////////////////////////////////////////////////////////////
//                                                                               //
// Setup pins and interrupts                                                     //
//                                                                               //
///////////////////////////////////////////////////////////////////////////////////
void setup()
{
   pinMode(pinLed, OUTPUT);
   digitalWrite(pinLed, HIGH);

   pinMode(Spin_A, INPUT_PULLUP); // set Spin_A as an input, pulled HIGH
   pinMode(Spin_B, INPUT_PULLUP); // set Spin_B as an input, pulled HIGH
   attachPCINT(Spin_A, doEncoderRising, RISING);   // Use this ISR for x1 PPR
   //attachPCINT(Spin_A, doEncoderChange_A, CHANGE); // Use this ISR for x2 PPR
   //attachPCINT(Spin_B, doEncoderChange_B, CHANGE); // Use this AND the one above for x4 PPR

   pinMode(4, INPUT_PULLUP);      // Input pin to set scale factor 1/2
   pinMode(5, INPUT_PULLUP);      // Input pin to set scale factor 1/4
   pinMode(6, INPUT_PULLUP);      // Input pin to set scale factor 1/8

   #if (debug)
      Serial.begin(57600);
      Serial.println("BootMouse initialised...");
   #endif

   BootMouse.begin();
}


///////////////////////////////////////////////////////////////////////////////////
//                                                                               //
// Main Program Loop                                                             //
//                                                                               //
///////////////////////////////////////////////////////////////////////////////////
void loop()
{
   current_ms=millis();                 // Record the current time so we an determine how long since we last sent mouse data
   if ((current_ms - start_ms) >= 20)   // Send position every 10ms. Decrease this for highrt PPRs or if you get backspin
   {
      scale = (~PINB & 0b01110000) >> 3;
      if (scale == 0) scale=1;          // scale = 1, 2, 4 or 8, just ground one pin but it should handle combinations
      noInterrupts();                   // Stop encoderPos updating whilst we snapshot the value
      delta_x=encoderPos;
      encoderPos=encoderPos%scale;      // comment this in debug mode if you want to count the PPR
      interrupts();
      #if (debug)
         Serial.print("Pos: \t");
         Serial.print(delta_x);
         Serial.print("\tDiv: \t");
         Serial.print(delta_x/scale);
         Serial.print("\tRem: \t");
         Serial.println(delta_x%scale);
      #else
        BootMouse.move(delta_x/scale, 0);
      #endif
      start_ms=current_ms;
   }
}


///////////////////////////////////////////////////////////////////////////////////
//                                                                               //
// Interrupt routine for Spin_A - samples on RISING edge of input A only         //
// This will read x1 PPR resolution                                              //
//                                                                               //
///////////////////////////////////////////////////////////////////////////////////
void doEncoderRising(void)
{
   noInterrupts();                // Stop interrupts happening before we read pin values
   //digitalWrite(pinLed, LOW);   // Visual feedback
   AB_Pins = PINB & 0b00000110;   // Mask off the 2 pins of interest
   if ((AB_Pins == 0b00000110))   // Check if Spin_A == Spin_B
   {
      encoderPos++;
   }
   else
   {
      encoderPos--;
   }
   //digitalWrite(pinLed, HIGH);  // Visual fedback
   interrupts();                  // Restart interrupts
}


///////////////////////////////////////////////////////////////////////////////////
//                                                                               //
// Interrupt routine for Spin_A - samples on RISING and FALLING edges of input A //
// This will read x2 PPR resolution                                              //
//                                                                               //
///////////////////////////////////////////////////////////////////////////////////
void doEncoderChange_A(void)
{
   noInterrupts();                // Stop interrupts before we read pin values
   //digitalWrite(pinLed, LOW);   // Visual feedback
   AB_Pins = PINB & 0b00000110;   // Mask off the 2 pins of interest
   if ((AB_Pins == 0b00000110) || (!AB_Pins))   // Check if Spin_A == Spin_B
   {
      encoderPos++;
   }
   else
   {
      encoderPos--;
   }
   //digitalWrite(pinLed, HIGH);  // Visual feedback
   interrupts();                  // restart interrupts
}


///////////////////////////////////////////////////////////////////////////////////
//                                                                               //
// Interrupt routine for Spin_B - samples on RISING and FALLING edges of input B //
// This will read x4 PPR resolution when used alongside the CHANGE ISR for SpinA //
//                                                                               //
///////////////////////////////////////////////////////////////////////////////////
void doEncoderChange_B(void)
{
   noInterrupts();                // Stop interrupts before we read pin values
   //digitalWrite(pinLed, LOW);   // Visual feedback
   AB_Pins = PINB & 0b00000110;   // Mask off the 2 pins of interest
   if ((AB_Pins == 0b00000100) || (AB_Pins == 0b00000010))   // Check if Spin_A != Spin_B
   {
      encoderPos++;
   }
   else
   {
      encoderPos--;
   }
   //digitalWrite(pinLed, HIGH);  // Visual feedback
   interrupts();                  // restart interrupts
}
