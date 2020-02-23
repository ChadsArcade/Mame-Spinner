///////////////////////////////////////////////////////////////////////////////////
//                                                                               //
// Arduino Pro Micro USB Spinner                                                 //
// Chad Gray, Jan 2020                                                           //
//                                                                               //
// Requires an Arduino Pro Micro                                                 //
//                                                                               //
// Use a PhotoElectric Rotary Encoder connected to digital pins D2 and D3        //
//                                                                               //
//   Tx Rx G  G  D2 D3                                                           //
//   o  o  o  o  o  o  Pins 2 and 3 support interrupts                           //
//               B  A  If the direction is reversed, swap wires A and B over     //
//                                                                               //
// Scale factor can be set by grounding one of the A0-A2 pins                    //
// This will scale down the effective PPR by the amount shown:                   //
//                                                                               //
// e.g. for a 600PPR spinner:                                                    //
// Gnd A2 for divide by 2, = 300PPR                                              //
// Gnd A1 for divide by 4, = 150PPR                                              //
// Gnd A0 for divide by 8, =  75PPR                                              //
//                                                                               //
// Rotary encoder code adapted from examples by:                                 //
// Simon Merrett, Oleg Mazurov, Nick Gammon, rt, Steve Spence                    //
// https://www.instructables.com/id/Improved-Arduino-Rotary-Encoder-AB_Pins/     //
//                                                                               //
///////////////////////////////////////////////////////////////////////////////////


#include <Mouse.h>

// Set debug to 1 for diagnostic output to the console, can be used to measure the effective PPR
// Set debug to 0 to enable spinner mode (normal use)
#define        debug        0

#define        Spin_A       2  // Hardware interrupt 0, digital pin 2
#define        Spin_B       3  // Hardware interrupt 1, digital pin 3

volatile int   encoderPos = 0; // Stores our current value of encoder position.
int            delta_x    = 0; // Store the encoder position prior to sending the mouse data
volatile byte  AB_Pins    = 0; // Stores the direct values we read from our interrupt pins
volatile int   scale      = 1; // Scale factor by which we divide the output of the Spinner.

// The Arduino Pro Micro has D2 and D3 on bits 0 and 1 of PortD, you may need to change this for different models!
byte           D2D3Mask   = 0b00000011;

// Pins A0 to A2 are use to set the scale factor, this corresponds to bits 6-8 on PortF for an Arduino Pro Micro.
byte           ScaleMask  = 0b11100000;

unsigned long  start_ms;       // Last time we sent mouse data
unsigned long  current_ms;     // Current time, used to check how long has elapsed since we last sent mouse data

void setup();                  // The initialisation routine
void loop();                   // The main program loop
void doEncoderRising_A();      // 1 x dpi ISR - gives  600 pulses for a 600DPI spinner
void doEncoderChange_A();      // 2 x dpi ISR - gives 1200 pulses for a 600DPI spinner
void doEncoderChange_B();      // 4 x dpi ISR - gives 2400 pulses for a 600DPI spinner when used in conjunction with the ISR for Change_A


///////////////////////////////////////////////////////////////////////////////////
// Set up variables and interrupts                                               //
///////////////////////////////////////////////////////////////////////////////////
void setup()
{
   pinMode(Spin_A, INPUT_PULLUP); // set Spin_A as an input, pulled HIGH
   pinMode(Spin_B, INPUT_PULLUP); // set Spin_B as an input, pulled HIGH
   attachInterrupt(digitalPinToInterrupt(Spin_A), doEncoderRising_A, RISING);     // 1 x pulses/rev
   //attachInterrupt(digitalPinToInterrupt(Spin_A), doEncoderChange_A, CHANGE);   // 2 x pulses/rev
   //attachInterrupt(digitalPinToInterrupt(Spin_B), doEncoderChange_B, CHANGE);   // 4 x pulses/rev, use alongside Change_A

   pinMode(18, INPUT_PULLUP);     // set A0 as an input, pulled HIGH
   pinMode(19, INPUT_PULLUP);     // set A1 as an input, pulled HIGH
   pinMode(20, INPUT_PULLUP);     // set A2 as an input, pulled HIGH

   // In debug mode we open the console for diagnostic output
   #if (debug)
      Serial.begin(115200);
      while (!Serial);            // Wait for Serial Port to open
      Serial.println("Arduino Pro Micro USB Spinner started");
   #endif

   start_ms=millis();
}


///////////////////////////////////////////////////////////////////////////////////
// Main Program Loop                                                             //
///////////////////////////////////////////////////////////////////////////////////
void loop()
{
   current_ms=millis();                   // Record the current time so we an determine how long since we last sent mouse data
   if (current_ms - start_ms >= 10)       // If 10ms have elapsed since we last sent mouse data, send updated data
   {
      scale = (~PINF & ScaleMask) >> 4;   // read scale = 2, 4, 8
      if (scale == 0) scale=1;            // scale = 1, 2, 4 or 8
      noInterrupts();                     // Prevent encoderPos from being updated whilst we sample the value
      delta_x = encoderPos;               // Save encoder position
      encoderPos = encoderPos%scale;      // reset encoderPos but retain the fractional part once divided by the scaler
      interrupts();                       // Allow changes to encoder position again
      #if (debug)
         if (delta_x)
         {
            Serial.print("Pos: \t");
            Serial.print(delta_x);
            Serial.print("\tDiv: \t");
            Serial.print(delta_x/scale);
            Serial.print("\tRem: \t");
            Serial.println(delta_x%scale);
         }
      #else
         Mouse.move(delta_x/scale, 0, 0); // Send the integer part once divided by the scaler
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
void doEncoderRising_A()
{
   noInterrupts(); //stop interrupts happening before we read pin values
   AB_Pins = PIND & D2D3Mask;
   if (AB_Pins == D2D3Mask) // Check if Spin_A == Spin_B
   {
      encoderPos++;
   }
   else
   {
      encoderPos--;
   }
   interrupts(); //restart interrupts
}


///////////////////////////////////////////////////////////////////////////////////
//                                                                               //
// Interrupt routine for Spin_A - samples on RISING or FALLING edge of input A   //
// This will read x2 PPR resolution                                              //
//                                                                               //
///////////////////////////////////////////////////////////////////////////////////
void doEncoderChange_A()
{
   noInterrupts(); //stop interrupts happening before we read pin values
   AB_Pins = PIND & D2D3Mask;   // For Pro Micro, D2 and D3 and bits D1 and D0 - Check if using a different device 
   if ((AB_Pins == D2D3Mask) || (AB_Pins == 0b00000000)) // Check if Spin_A == Spin_B
   {
      encoderPos++;
   }
   else
   {
      encoderPos--;
   }
   interrupts(); //restart interrupts
}


///////////////////////////////////////////////////////////////////////////////////
//                                                                               //
// Interrupt routine for Spin_B - samples on RISING and FALLING edge of input B  //
// This will read x4 PPR resolution when used alongside the CHANGE ISR for SpinA //
//                                                                               //
///////////////////////////////////////////////////////////////////////////////////
void doEncoderChange_B()
{
   noInterrupts(); //stop interrupts happening before we read pin values
   AB_Pins = PIND & D2D3Mask;   // For Pro Micro, D2 and D3, bits 1 and 0 - Check if using a different device
   if ((AB_Pins == 0b00000010) || (AB_Pins == 0b00000001)) // Check if Spin_A != Spin_B
   {
      encoderPos++;
   }
   else
   {
      encoderPos--;
   }
   interrupts(); //restart interrupts
}
