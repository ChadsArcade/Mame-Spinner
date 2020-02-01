///////////////////////////////////////////////////////////////////////////////////
//                                                                               //
// Arduino USB Spinner using 16u2 reprogrammed as a USB Mouse                    //
// Chad Gray, Jan 2020                                                           //
//                                                                               //
// Requires an Arduino UNO with a 16u2 USB CPU which can enter DFU mode          //
// UNOs with a CH340G USB chip WILL NOT WORK!                                    //
//                                                                               //
// Use a PhotoElectric Rotary Encoder connected to digital pins 2 and 3          //
//                                                                               //
//        D3 D2 D1 D0                                                            //
//         o  o  o  o   Pins 2 and 3 support interrupts                          //
//         B  A  Tx Rx  If the direction is reversed, swap wires A and B over    //
//                                                                               //
// Scale factor can be set by grounding one of the D8-D10 pins                   //
// This will scale down the effective PPR by the amount shown:                   //
//                                                                               //
//         D10 D9  D8                                                            //
//          o   o   o                                                            //
//         1/8 1/4 1/2                                                           //
//                                                                               //
// Adapted from Arduino USB Mouse HID demo by Darran Hunt                        //
//                                                                               //
// Rotary encoder code adapted from examples by:                                 //
// Simon Merrett, Oleg Mazurov, Nick Gammon, rt, Steve Spence                    //
// https://www.instructables.com/id/Improved-Arduino-Rotary-Encoder-AB_Pins/     //
//                                                                               //
///////////////////////////////////////////////////////////////////////////////////

// Set debug to 1 when in Arduino mode and it will print info to the console
// Set debug to 0 when in mouse mode
#define debug 0

struct  { uint8_t buttons;
          int8_t x;
          int8_t y;
          int8_t wheel;  /* Not yet implemented */
        } mouseReport;

uint8_t        nullReport[4] = { 0, 0, 0, 0 };

#define        Spin_A       2  // Hardware interrupt 0, digital pin 2
#define        Spin_B       3  // Hardware interrupt 1, digital pin 3

volatile int   encoderPos = 0; // stores our current value of encoder position. Change to int or uin16_t instead of byte if you want to record a larger range than 0-255
int            delta_x    = 0; // Used to store encoder position before sending to mouse
volatile byte  AB_Pins    = 0; // stores the direct values we read from our interrupt pins before checking to see if we have moved a whole detent
volatile int   scale      = 1;

unsigned long  start_ms;       // Last time we sent mouse data
unsigned long  current_ms;     // Current time, used to check how long has elapsed since we last sent mouse data

void setup();
void loop();
void doEncoderChange();        // 2 x dpi ISR - gives 1200 pulses for a 600DPI spinner. You could sample the B pin as well for 4 x DPI
void doEncoderRising();        // 1 x dpi ISR - gives  600 pulses for a 600DPI spinner


///////////////////////////////////////////////////////////////////
// Set up variables and interrupts                               //
///////////////////////////////////////////////////////////////////
void setup()
{
   mouseReport.buttons = 0;
   mouseReport.x = 0;
   mouseReport.y = 0;
   mouseReport.wheel = 0;

   pinMode(Spin_A, INPUT_PULLUP); // set Spin_A as an input, pulled HIGH
   pinMode(Spin_B, INPUT_PULLUP); // set Spin_B as an input, pulled HIGH
   attachInterrupt(digitalPinToInterrupt(Spin_A), doEncoderRising_A, RISING);     // 1 x pulses/rev
   //attachInterrupt(digitalPinToInterrupt(Spin_A), doEncoderChange_A, CHANGE);   // 2 x pulses/rev when used on its own
   //attachInterrupt(digitalPinToInterrupt(Spin_B), doEncoderChange_B, CHANGE);   // 4 x pulses/rev in conjunction with above ISR

   pinMode( 8, INPUT_PULLUP); // set Pin  8 as an input, pulled HIGH
   pinMode( 9, INPUT_PULLUP); // set Pin  9 as an input, pulled HIGH
   pinMode(10, INPUT_PULLUP); // set Pin 10 as an input, pulled HIGH

   Serial.begin(9600);
   delay(200);
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
      scale = (~PINB & 0x07) << 1;                // 00000111 scale = 0, 2, 4, 8
      if (scale == 0) scale=1;                    // default, scale is now = 1, 2, 4 or 8
      noInterrupts();
      delta_x = encoderPos;                       // Store encoder position
      encoderPos = encoderPos%scale;              // Retain the scaled remainder component
      interrupts();
      mouseReport.x = delta_x/scale;              // We send the scaled integer component only

      #if (debug)
         if (encoderPos)
         {
            Serial.print("**Pos: \t");
            Serial.print(delta_x);
            Serial.print("\tDiv: \t");
            Serial.print(delta_x/scale);
            Serial.print("\tRem: \t");
            Serial.print(delta_x%scale);
            Serial.print("\tScale: \t");
            Serial.println(scale);
         }
      #else
        Serial.write((uint8_t *)&mouseReport, 4);
        Serial.write((uint8_t *)&nullReport, 4);
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
   noInterrupts();            //stop interrupts before we read pin values
   AB_Pins = PIND & 0xC;
   if (AB_Pins == 0b00001100) // Check if Spin_A == Spin_B
   {
      encoderPos++;
   }
   else
   {
      encoderPos--;
   }
   interrupts();              //restart interrupts
}


///////////////////////////////////////////////////////////////////////////////////
//                                                                               //
// Interrupt routine for Spin_A - samples on RISING or FALLING edge of input A   //
// This will read x2 PPR resolution                                              //
//                                                                               //
///////////////////////////////////////////////////////////////////////////////////
void doEncoderChange_A()
{
   noInterrupts();            //stop interrupts before we read pin values
   AB_Pins = PIND & 0xC;
   if ((AB_Pins == 0b00001100) || (AB_Pins == 0b00000000)) // Check if Spin_A == Spin_B
   {
      encoderPos++;
   }
   else
   {
      encoderPos--;
   }
   interrupts();              //restart interrupts
}


///////////////////////////////////////////////////////////////////////////////////
//                                                                               //
// Interrupt routine for Spin_B - samples on RISING and FALLING edge of input B  //
// This will read x4 PPR resolution when used alongside the CHANGE ISR for SpinA //
//                                                                               //
///////////////////////////////////////////////////////////////////////////////////
void doEncoderChange_B()
{
   noInterrupts();            // Stop interrupts before we read pin values
   AB_Pins = PIND & 0xC;
   if ((AB_Pins == 0b00000100) || (AB_Pins == 0b00001000))   // Check if Spin_A != Spin_B
   {
      encoderPos++;
   }
   else
   {
      encoderPos--;
   }
   interrupts();              // restart interrupts
}
