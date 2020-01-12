////////////////////////////////////////////////////////////////////////////////
// Adapted from Arduino USB Mouse HID demo                                    //
// By: Darran Hunt                                                            //
//                                                                            //
// Rotary encoder code adapted from examples by:                              //
// Simon Merrett, Oleg Mazurov, Nick Gammon, rt, Steve Spence                 //
// https://www.instructables.com/id/Improved-Arduino-Rotary-Encoder-AB_Pins/  //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////


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

volatile float encoderPos = 0; // stores our current value of encoder position. Change to int or uin16_t instead of byte if you want to record a larger range than 0-255
volatile byte  AB_Pins    = 0; // stores the direct values we read from our interrupt pins before checking to see if we have moved a whole detent
volatile int   scale      = 1;
volatile float inc        = 1.0;

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
   //attachInterrupt(digitalPinToInterrupt(Spin_A), doEncoderChange, CHANGE);   // 2 x pulses/rev
   attachInterrupt(digitalPinToInterrupt(Spin_A), doEncoderRising, RISING);     // 1 x pulses/rev

   pinMode(8, INPUT_PULLUP); // set Pin 8 as an input, pulled HIGH
   pinMode(9, INPUT_PULLUP); // set Pin 9 as an input, pulled HIGH
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

      #if (debug)
         if (encoderPos)
         {
            Serial.print("**Pos: \t");
            Serial.print(encoderPos);
            Serial.print("\tinc: \t");
            Serial.print(inc);
            mouseReport.x = encoderPos;
            Serial.print("\tSend: \t");
            Serial.print(mouseReport.x);
            Serial.print("\tScale: \t");
            Serial.println(scale);
         }
      #endif

      noInterrupts();
      scale = (~PINB & 0x07) << 1; // 00000111 scale = 2, 4, 8
      if (scale == 0) scale=1;
      inc = (1.0/scale);
      if (encoderPos>0) encoderPos=encoderPos+1; // This compensates for large dividers, so a small movement is registered
      if (encoderPos<0) encoderPos=encoderPos-1; // otherwise you have to move {divider} pulses to move 1 pixel
      mouseReport.x = encoderPos;

      #if (!debug)
        Serial.write((uint8_t *)&mouseReport, 4);
        Serial.write((uint8_t *)&nullReport, 4);
      #endif

      encoderPos=0;
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
   AB_Pins = PIND & 0xC;
   if ((AB_Pins == 0b00001100) || (AB_Pins == 0b00000000)) // Check if Spin_A == Spin_B
   {
      encoderPos+=inc;
   }
   else
   {
      encoderPos-=inc;
   }
   interrupts(); //restart interrupts
}


///////////////////////////////////////////////////////////////////
// Interrupt routine for Spin_A - samples on RISING only         //
// Dont skip loops to lower the resolution...                    //
// that road leads to backspin                                   //
///////////////////////////////////////////////////////////////////
void doEncoderRising()
{
   noInterrupts(); //stop interrupts happening before we read pin values
   AB_Pins = PIND & 0xC;
   if (AB_Pins == 0b00001100) // Check if Spin_A == Spin_B
   {
      encoderPos+=inc;
   }
   else
   {
      encoderPos-=inc;
   }
   interrupts(); //restart interrupts
}
