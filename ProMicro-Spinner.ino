////////////////////////////////////////////////////////////////////////////////
// Ardiono Pro Micro USB SPinner for MAME / other emulators                   //
// Chad Gray, Jan 2020                                                        //
//                                                                            //
// Rotary encoder code adapted from examples by:                              //
// Simon Merrett, Oleg Mazurov, Nick Gammon, rt, Steve Spence                 //
// https://www.instructables.com/id/Improved-Arduino-Rotary-Encoder-AB_Pins/  //
//                                                                            //
// Attach encoder A and B outputs to D2 and D3, these support interrupts      //
// If the direction is reversed then awap the A and B pins over               //
//                                                                            //
// The PPR can be divided to reduce sensitivity by grounding pins A0-A2       //
// e.g. for a 600PPR spinner:                                                 //
// Gnd A2 for divide by 2, = 300PPR                                           //
// Gnd A1 for divide by 4, = 150PPR                                           //
// Gnd A0 for divide by 8, =  75PPR                                           //
//                                                                            //
// An alternate ISR is included which will double the PPR (e.g. to 1200)      //
// should you require MORE sensitivity. This could be set via the state of    //
// another pin if so desired.                                                 //
////////////////////////////////////////////////////////////////////////////////

#include <Mouse.h>

// Set debug to 1 for diagnostic output to the console, can be used to measure the effective PPR
// Set debug to 0 to enable spinner mode (normal use)
#define debug 0

#define        Spin_A       2  // Hardware interrupt 0, digital pin 2
#define        Spin_B       3  // Hardware interrupt 1, digital pin 3

volatile float encoderPos = 0; // stores our current value of encoder position. Change to int or uin16_t instead of byte if you want to record a larger range than 0-255
volatile byte  AB_Pins    = 0; // stores the direct values we read from our interrupt pins before checking to see if we have moved a whole detent
volatile int   scale      = 1; // Scale factor by which we divide the output of the Spinner.
volatile float inc        = 1.0; // size of the increment per pulse, this is 1/scale

#if debug
float lastEncPos          = 0; // In debug mode we track changes to the encoder position so we can count the PPR
#endif

//The Arduino Pro Micro has D2 and D3 on bits 0 and 1 of PortD, you may need to change this for different models!
byte           D2D3Mask   = 0b00000011;

// Pins A0 to A2 are use to set the scale factor, this corresponds to bits 6-8 on PortF for an Arduino Pro Micro.
byte           ScaleMask  = 0b11100000;

unsigned long  start_ms;       // Last time we sent mouse data
unsigned long  current_ms;     // Current time, used to check how long has elapsed since we last sent mouse data

void setup();                  // The initialisation routine
void loop();                   // The main program loop
void doEncoderChange();        // 2 x dpi ISR - gives 1200 pulses for a 600DPI spinner. You could sample the B pin as well for 4 x DPI
void doEncoderRising();        // 1 x dpi ISR - gives  600 pulses for a 600DPI spinner


///////////////////////////////////////////////////////////////////
// Set up variables and interrupts                               //
///////////////////////////////////////////////////////////////////
void setup()
{
   pinMode(Spin_A, INPUT_PULLUP); // set Spin_A as an input, pulled HIGH
   pinMode(Spin_B, INPUT_PULLUP); // set Spin_B as an input, pulled HIGH
   //attachInterrupt(digitalPinToInterrupt(Spin_A), doEncoderChange, CHANGE);   // 2 x pulses/rev
   attachInterrupt(digitalPinToInterrupt(Spin_A), doEncoderRising, RISING);     // 1 x pulses/rev

   pinMode(18, INPUT_PULLUP);     // set A0 as an input, pulled HIGH
   pinMode(19, INPUT_PULLUP);     // set A1 as an input, pulled HIGH
   pinMode(20, INPUT_PULLUP);     // set A2 as an input, pulled HIGH

   // In debug mode we open the console for diagnostic output
   #if (debug)
      Serial.begin(9600);
      while (!Serial);            // Wait for Serial Port to open
      Serial.println("Arduino Pro Micro USB Spinner started");
   #endif

   start_ms=millis();
}


///////////////////////////////////////////////////////////////////
// Main Program Loop                                             //
///////////////////////////////////////////////////////////////////
void loop()
{
   current_ms=millis();    // Record the current time so we an determine how long since we last sent mouse data
   // If 15ms have elapsed since we last sent mouse data, send updated data
   if (current_ms - start_ms >= 15)
   {
      noInterrupts();
      scale = (~PINF & ScaleMask) >> 4; // mask 11100000 scale = 2, 4, 8
      if (scale == 0) scale=1;
      inc = (1.0/scale);
      #if (debug)
         if (encoderPos != lastEncPos)
         {
            Serial.print("Pos: \t");
            Serial.print(encoderPos);
            Serial.print("\tinc: \t");
            Serial.print(inc);
            Serial.print("\tScale: \t");
            Serial.println(scale);
            lastEncPos = encoderPos;
         }
      #else
         if (scale != 1)
         {
            if (encoderPos > 0) encoderPos++; // This compensates for large scale factors, so a small movement is still registered,
            if (encoderPos < 0) encoderPos--; // otherwise you have to move {scale factor} pulses to move 1 pixel and small movements are ignored
         }
         Mouse.move(encoderPos, 0, 0);
         encoderPos=0;                        // In debug mode we don't reset encoderPos so we can count the PPR
      #endif
      start_ms=current_ms;
      interrupts();
   }
}


///////////////////////////////////////////////////////////////////
// Interrupt routine for Spin_A - samples on RISING and FALLING  //
// This will double the number of pulses per revolution          //
///////////////////////////////////////////////////////////////////
void doEncoderChange()
{
   noInterrupts(); //stop interrupts happening before we read pin values
   AB_Pins = PIND & D2D3Mask;   // For Pro Micro, D2 and D3 and bits D1 and D0 - Check if using a different device 
   if ((AB_Pins == D2D3Mask) || (AB_Pins == 0b00000000)) // Check if Spin_A == Spin_B
   {
      encoderPos-=inc;
   }
   else
   {
      encoderPos+=inc;
   }
   interrupts(); //restart interrupts
}


///////////////////////////////////////////////////////////////////
// Interrupt routine for Spin_A - samples on RISING only         //
// Dont skip loops to lower the resolution...                    //
// That road led to backspin when tested                         //
///////////////////////////////////////////////////////////////////
void doEncoderRising()
{
   noInterrupts(); //stop interrupts happening before we read pin values
   AB_Pins = PIND & D2D3Mask;
   if (AB_Pins == D2D3Mask) // Check if Spin_A == Spin_B
   {
      encoderPos+=inc;
   }
   else
   {
      encoderPos-=inc;
   }
   interrupts(); //restart interrupts
}
