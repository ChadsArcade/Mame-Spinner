///////////////////////////////////////////////////////////////////////////////////
//                                                                               //
// Arduino UNO PS2 Mouse emulator                                                //
// Chad Gray, Jan 2020                                                           //
//                                                                               //
// Requires an Arduino UNO (other models will work with code mods)               //
//                                                                               //
// Use a PhotoElectric Rotary Encoder connected to digital pins 2 and 3          //
// PS/2 is connected to pins 4 (data) and 5 (clock)                              //
//                                                                               //
//      D5 D4 D3 D2 D1 D0                                                        //
//       o  o  o  o  o  o   Pins 2 and 3 support interrupts                      //
//      ClkDat B  A  Tx Rx  If the direction is reversed, swap wires A and B     //
//                                                                               //
// Connect PS/2 5v to V(in) so it powers the Arduino                             //
//                                                                               //
// Scale factor can be set by grounding one of the D8-D10 pins                   //
// This will scale down the effective PPR by the amount shown:                   //
//                                                                               //
//         D10 D9  D8                                                            //
//          o   o   o                                                            //
//         1/8 1/4 1/2                                                           //
//                                                                               //
// Rotary encoder code adapted from examples by:                                 //
// Simon Merrett, Oleg Mazurov, Nick Gammon, rt, Steve Spence                    //
// https://www.instructables.com/id/Improved-Arduino-Rotary-Encoder-AB_Pins/     //
//                                                                               //
///////////////////////////////////////////////////////////////////////////////////

#include <ps2dev.h>

#define        debug         1                 // 0 = off, 1 = show commands, 2 = show acks, 3 = host comms debug, 4 show movement packets
#define        Spin_A        2                 // Spinner Quadrature A
#define        Spin_B        3                 // Spinner Quadrature B
#define        mouse_data    4                 // Mouse Data
#define        mouse_clock   5                 // Mouse Clock

volatile byte  AB_Pins     = 0;                // Stores the direct values of A and B to determine movement
volatile int   encoderPos  = 0;                // Stores our current value of encoder position.
long int       delta_x     = 0;                // Mouse X movement
long int       old_delta_x = 0;
byte           scale       = 1;                // Scale factor to reduce PPR of the rotary encoder

byte           MouseMask   = 0b00110000;       // Mouse   pins are bits 4 and 5 (0x30)
byte           Spin_Mask   = 0b00001100;       // Spinner pins are bits 2 and 3 (0x0C)

int            enabled     = 0;                // We start off not enabled, host will enable us later
int            rate        = 40;               // Default sample rate
int            waittime    = (1000/rate);      // Time to wait for {rate} reports per second
int            resolution  = 3;                // Default resolution

unsigned long  startMillis;
unsigned long  currentMillis;

PS2dev         mouse(mouse_clock, mouse_data); // Initialise mouse device

///////////////////////////////////////////////////////////////////////////////////
// Set up variables etc                                                          //
///////////////////////////////////////////////////////////////////////////////////
void setup()
{
   pinMode(Spin_A, INPUT_PULLUP); // set Spin_A as an input, pulled HIGH
   pinMode(Spin_B, INPUT_PULLUP); // set Spin_B as an input, pulled HIGH

   pinMode( 8, INPUT_PULLUP);     // set Pin  8 as an input, pulled HIGH
   pinMode( 9, INPUT_PULLUP);     // set Pin  9 as an input, pulled HIGH
   pinMode(10, INPUT_PULLUP);     // set Pin 10 as an input, pulled HIGH

   #if (debug)
      Serial.begin(115200);
      delay(100);
      Serial.println("PS/2 Mouse emulator Started");
      Serial.println();
   #endif

   delay(500);
   send_mouse_init();             // send the mouse initialisation sequence

   attachInterrupt(digitalPinToInterrupt(Spin_A), doEncoderRising_A, RISING);
   startMillis = millis();        //initial start time
}


///////////////////////////////////////////////////////////////////////////////////
// Main program loop                                                             //
///////////////////////////////////////////////////////////////////////////////////
void loop()
{
   unsigned char  c;
   currentMillis = millis();                                           //get the current time
   if (!(PIND & 0b00100000) && !(PIND & 0b00010000))                   // Faster implementation of below
   //if( (digitalRead(mouse_clock)==LOW) || (digitalRead(mouse_data) == LOW))  // Communication inhibited, host wants to talk
   {
      #if (debug==3)
         Serial.println("Waiting for host...");
      #endif
      while(mouse.read(&c));      // Hangs on this line under DOS ZVG Mame, is the timing/line assertion right?
      #if (debug==3)
         Serial.println("Host sent a command!");
      #endif
      mousecommand(c);
   }

   if (((currentMillis - startMillis) >= waittime) && enabled)         // Update mouse every [rate] ms
   {
      scale = (~PINB & 0x07) << 1;                                     // 00000111 scale = 0, 2, 4, 8
      if (scale == 0) scale = 1;                                       // default, scale is now = 1, 2, 4 or 8
      #if (debug == 4)
         Serial.print("WaitTime: ");
         Serial.print(waittime);
         Serial.print("\tEncoderPos: ");
         Serial.print(encoderPos);
         Serial.print("\tScale: ");
         Serial.print(scale);
         Serial.print("\tSend mouse Packet: X=");
         Serial.println(encoderPos/scale);
      #endif
      detachInterrupt (digitalPinToInterrupt(Spin_A));                 // Disable our ISR whilst reading encoderPos value
      delta_x=encoderPos/scale;                                        // Mouse x = integer of movement / scale
      old_delta_x = delta_x;
      encoderPos=encoderPos%scale;                                     // Retain remainder part after scale applied
      attachInterrupt (digitalPinToInterrupt(Spin_A), doEncoderRising_A, RISING);
      write_packet();                                                  // Send the mouse data to the host
      startMillis = currentMillis;                                     // Start timer again from after sending this packet
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
   noInterrupts();            // Stop interrupts before we read pin values
   AB_Pins = PIND & 0x0C;
   if (AB_Pins == 0b00001100) // Check if Spin_A == Spin_B
   {
      encoderPos++;
   }
   else
   {
      encoderPos--;
   }
   interrupts();              // Restart interrupts
}


///////////////////////////////////////////////////////////////////////////////////
//                                                                               //
// Interrupt routine for Spin_A - samples on RISING or FALLING edge of input A   //
// This will read x2 PPR resolution                                              //
//                                                                               //
///////////////////////////////////////////////////////////////////////////////////
void doEncoderChange_A()
{
   noInterrupts();            // Stop interrupts before we read pin values
   AB_Pins = PIND & Spin_Mask;
   if ((AB_Pins == 0b00001100) || (AB_Pins == 0b00000000)) // Check if Spin_A == Spin_B
   {
      encoderPos++;
   }
   else
   {
      encoderPos--;
   }
   interrupts();              // Restart interrupts
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
   AB_Pins = PIND & Spin_Mask;
   if ((AB_Pins == 0b00000100) || (AB_Pins == 0b00001000))   // Check if Spin_A != Spin_B
   {
      encoderPos++;
   }
   else
   {
      encoderPos--;
   }
   interrupts();              // Restart interrupts
}


///////////////////////////////////////////////////////////////////////////////////
// Acknowledge a host command                                                    //
///////////////////////////////////////////////////////////////////////////////////
void send_ack()
{
   while(mouse.write(0xFA));
   #if (debug==2)
      Serial.println("   Send -> : 0xFA : Acknowledge");
   #endif
}


///////////////////////////////////////////////////////////////////////////////////
// Send a init sequence to the host                                              //
///////////////////////////////////////////////////////////////////////////////////
void send_mouse_init()
{
   while(mouse.write(0xAA)!=0);
   #if (debug)
      Serial.println("   Send -> : 0xAA : Self Test Passed");
   #endif
   enabled    = 0;              // set default values
   resolution = 4;              // Set default values
   rate       = 100;            // Set default values
   waittime   = 1000/rate;
   encoderPos = 0;
   while(mouse.write(0x00)!=0);
   #if (debug)
      Serial.println("   Send -> : 0x00 : I am a standard PS/2 Mouse");
   #endif
}


///////////////////////////////////////////////////////////////////////////////////
// Send mouse data to the host                                                   //
///////////////////////////////////////////////////////////////////////////////////
void write_packet()
{
  // As we're only modifying the X axis we can ignore the Y axis and buttons
   char overflowx = 0;
   char data[3];

   if (delta_x > 255)                              // Check if we have +overflowed
   {
      overflowx = 1;                               // Set overflow flag
      delta_x = 255;                               // Cap delta_x value
   }
   if (delta_x < -255)                             // Check if we have -overflowed
   {
      overflowx = 1;                               // Set overflow flag
      delta_x = -255;                              // Cap delta_x value
   }

   data[0] =
      ( ( 0 ) << 7 ) |                             // Y Overflow flag (Always 0)
      ( ( overflowx & 1 ) << 6 ) |                 // X Overflow flag
      ( ( 0 ) << 5 ) |                             // Y Sign bit      (Always 0)
      ( ( ( (delta_x & 0x100 ) >>8 ) & 1) << 4 ) | // X Sign bit
      ( ( 1 ) << 3 ) |                             // 1               (Always 1)
      ( ( 0 ) << 2 ) |                             // MMB             (Always 0)
      ( ( 0 ) << 1 ) |                             // RMB             (Always 0)
      ( ( 0 ) << 0 ) ;                             // LMB             (Always 0)

   data[1] = delta_x & 0xff;                       // mask off sign bit

   mouse.write(data[0] & 0xFF);                    // Flags and buttons
   mouse.write(data[1]);                           // X movement
   mouse.write(0x00 & 0xFF);                       // Y movement, never changes for a spinner so write a zero
}


///////////////////////////////////////////////////////////////////////////////////
// Act upon a command from the host                                              //
///////////////////////////////////////////////////////////////////////////////////
int mousecommand(int command)
{
   unsigned char val;
   //This implements enough mouse commands to get by, some of them are just acked without really doing anything

   #if (debug)
      Serial.print("-> Recv    : 0x");
      Serial.print(command, HEX);
      Serial.print(" : ");
   #endif
   switch (command)
   {
   case 0xFF: //reset
      #if (debug)
         Serial.println("Mouse Reset");
      #endif
      send_ack();
      send_mouse_init();
      break;

   case 0xFE: //resend
      #if (debug)
         Serial.println("Resend");
      #endif
      send_ack();
      delta_x = old_delta_x;
      write_packet();                                                  // Send the mouse data to the host
      break;

   case 0xF6: //set defaults
      #if (debug)
         Serial.println("Set Defaults (Rate = 100, Resolution = 4, Mouse disabled)");
      #endif
      enabled    = 0;
      rate       = 100;
      waittime   = 1000/rate;
      resolution = 4;
      encoderPos = 0;
      send_ack();
      break;

   case 0xF5:  //disable data reporting
      #if (debug)
         Serial.println("Disable Data Reporting - Mouse disabled");
      #endif
      enabled = 0;
      encoderPos = 0;
      send_ack();
      break;

   case 0xF4: //enable data reporting
      #if (debug)
         Serial.println("Enable Data Reporting - Mouse enabled");
      #endif
      enabled = 1;
      encoderPos = 0;
      send_ack();
      break;

   case 0xF3: //set sample rate
      #if (debug)
         Serial.println("Set Sample Rate...");
      #endif
      send_ack();
      mouse.read(&val);
      rate = val;
      waittime=1000/rate;
      #if (debug)
         Serial.print("-> Recv    : ");
         printHex(val, 2);
         Serial.print(" : ...to \"");
         Serial.print(val, DEC);
         Serial.print("\" (Report every ");
         Serial.print(waittime, DEC);
         Serial.println("ms)");
      #endif
      send_ack();
      break;

   case 0xF2: //get device id
      #if (debug)
         Serial.println("Get Device ID");
      #endif
      send_ack();
      mouse.write(00);
      #if (debug)
         Serial.println("   Send -> : 0x00 : I am a standard PS/2 Mouse");
      #endif
      encoderPos = 0;
      break;

   case 0xF0: //set remote mode
      #if (debug)
         Serial.println("Set Remote Mode");
      #endif
      send_ack();
      encoderPos = 0;
      break;

   case 0xEE: //set wrap mode
      #if (debug)
         Serial.println("Set Wrap Mode");
      #endif
      send_ack();
      encoderPos = 0;
      break;

   case 0xEC: //reset wrap mode
      #if (debug)
         Serial.println("Reset Wrap Mode");
      #endif
      send_ack();
      encoderPos = 0;
      break;

   case 0xEB: //read data
      #if (debug)
         Serial.println("Read Data");
      #endif
      send_ack();
      write_packet();
      #if (debug)
         Serial.println("Packet Sent");
      #endif
      break;

   case 0xEA: //set stream mode
      #if (debug)
         Serial.println("Set Stream Mode");
      #endif
      send_ack();
      encoderPos = 0;
      break;

   case 0xE9: //status request
      #if (debug)
         Serial.println("Send Status Request");
      #endif
      send_ack();
      mouse.write(0x00);       // 0 | Mode | enabled | scaling | 0 | LMB | MMB | RMB
      mouse.write(resolution); // Resolution
      mouse.write(rate);       // Rate
      #if (debug)
         Serial.println("   Send -> : 0x00 : Status 1: (Flags)");
         Serial.print("   Send -> : ");
         printHex(resolution, 2);
         Serial.println(" : Status 2: (Resolution)");
         Serial.print("   Send -> : ");
         printHex(rate, 2);
         Serial.println(" : Status 3: (Rate)");
      #endif
      break;

   case 0xE8: //set resolution
      #if (debug)
         Serial.println("Set Resolution...");
      #endif
      send_ack();
      mouse.read(&val);
      #if (debug)
         Serial.print("-> Recv    : ");
         printHex(val, 2);
         Serial.print(" : ...to \"");
         Serial.print(val, DEC);
         Serial.println("\"");
      #endif
      resolution=val;
      send_ack();
     break;

   case 0xE7: //set scaling 2:1
      #if (debug)
         Serial.println("Set Scaling 2:1");
      #endif
      send_ack();
      break;

   case 0xE6: //set scaling 1:1
      #if (debug)
         Serial.println("Set Scaling 1:1");
      #endif
      send_ack();
      break;
   default:
      #if (debug)
         Serial.println("Unknown command...");
      #endif
      break;
   }
}

///////////////////////////////////////////////////////////////////////////////////
// Print a Hex value to xx digits                                                //
///////////////////////////////////////////////////////////////////////////////////
void printHex(int num, int precision)
{
   char tmp[16];
   char format[128];
   sprintf(format, "0x%%.%dX", precision);
   sprintf(tmp, format, num);
   Serial.print(tmp);
}




/*
 * Mouse command set
 * 
0xFF (Reset)                  - The mouse responds to this command with "acknowledge" (0xFA) then enters Reset Mode.
0xFE (Resend)                 - The host sends this command whenever it receives invalid data from the mouse. The mouse responds by resending the last packet it sent to the host.   If the mouse responds to the "Resend" command with another invalid packet, the host may either issue another "Resend" command, issue an "Error" command, cycle the mouse's power supply to reset the mouse, or it may inhibit communication (by bringing the Clock line low).  The action taken depends on the host.
0xF6 (Set Defaults)           - The mouse responds with "acknowledge" (0xFA) then loads the following values:  Sampling rate = 100, Resolution = 4 counts/mm, Scaling = 1:1, Disable Data Reporting.  The mouse then resets its movement counters and enters stream mode.
0xF5 (Disable Data Reporting) - The mouse responds with "acknowledge" (0xFA) then disables data reporting and resets its movement counters.  This only effects data reporting in Stream mode and does not disable sampling.  Disabled stream mode functions the same as remote mode.
0xF4 (Enable Data Reporting)  - The mouse responds with "acknowledge" (0xFA) then enables data reporting and resets its movement counters.  This command may be issued while the mouse is in Remote Mode (or Stream mode), but it will only effect data reporting in Stream mode.
0xF3 (Set Sample Rate)        - The mouse responds with "acknowledge" (0xFA) then reads one more byte from the host.  The mouse saves this byte as the new sample rate. After receiving the sample rate, the mouse again responds with "acknowledge" (0xFA) and resets its movement counters.  Valid sample rates are 10, 20, 40, 60, 80, 100, and 200 samples/sec.
0xF2 (Get Device ID)          - The mouse responds with "acknowledge" (0xFA) followed by its device ID (0x00 for the standard PS/2 mouse.)  The mouse should also reset its movement counters.
0xF0 (Set Remote Mode)        - The mouse responds with "acknowledge" (0xFA) then resets its movement counters and enters remote mode.
0xEE (Set Wrap Mode)          - The mouse responds with "acknowledge" (0xFA) then resets its movement counters and  enters wrap mode.
0xEC (Reset Wrap Mode)        - The mouse responds with "acknowledge" (0xFA) then resets its movement counters and enters the mode it was in prior to wrap mode (Stream Mode or Remote Mode.)
0xEB (Read Data)              - The mouse responds with acknowledge (0xFA) then sends a movement data packet. This is the only way to read data in Remote Mode.  After the data packets has been successfully sent, it resets its movement counters.
0xEA (Set Stream Mode)        - The mouse responds with "acknowledge" then resets its movement counters and enters stream mode.
0xE9 (Status Request)         - The mouse responds with "acknowledge" then sends the following 3-byte status packet (then resets its movement counters.):
Byte 1
Bit 7   Always 0
Bit 6   Mode
Bit 5   Enable
Bit 4   Scaling
Bit 3   Always 0
Bit 2   Left Button
Bit 1   Middle Button
Bit 0   Right Button

Byte 2
Resolution

Byte 3
Sample Rate

Right, Middle, Left Btn = 1 if button pressed; 0 if button is not pressed.
Scaling = 1 if scaling is 2:1; 0 if scaling is 1:1. (See commands 0xE7 and 0xE6)
Enable = 1 if data reporting is enabled; 0 if data reporting is disabled. (See commands 0xF5 and 0xF4)
Mode = 1 if Remote Mode is enabled; 0 if Stream mode is enabled. (See commands 0xF0 and 0xEA)


0xE8 (Set Resolution) - The mouse responds with acknowledge (0xFA) then reads one byte from the host and again responds with acknowledge (0xFA) then resets its movement counters.
                        The byte read from the host determines the resolution as follows:

Byte from Host    Resolution
0x00              1 count/mm
0x01              2 count/mm
0x02              4 count/mm
0x03              8 count/mm

0xE7 (Set Scaling 2:1) - The mouse responds with acknowledge (0xFA) then enables 2:1 scaling (see table below)
0xE6 (Set Scaling 1:1) - The mouse responds with acknowledge (0xFA) then enables 1:1 scaling

Scaling factor 2:1 is as follows
Movement Counter     Reported Movement
0                    0
1                    1
2                    1
3                    3
4                    6
5                    9
N > 5                2 * N

Valid sample rates are 10, 20, 40, 60, 80, 100, and 200 samples/sec.
 10 = 1000/10  = 100ms
 20 = 1000/20  = 50ms
 40 = 1000/40  = 25ms
 60 = 1000/60  = 16.6ms
 80 = 1000/80  = 12.5ms
100 = 1000/100 = 10ms
200 = 1000/200 = 5ms

Mouse packet is:
Byte1: Y overflow | X overflow | Y sign | X sign | 1 | MMB | RMB | LMB
Byte2: X movement
Byte3: Y movement

*/
