///////////////////////////////////////////////////////////////////
// PS/2 Mouse emulator                                           //
///////////////////////////////////////////////////////////////////

#include <ps2dev.h>
#define debug 0

#define Spin_A      2  // Spinner Quadrature A
#define Spin_B      3  // Spinner Quadrature B
#define INTERRUPT   0  // that is, pin 2

#define mouse_data  4  // Mouse Data
#define mouse_clock 5  // Mouse Clock

volatile byte  AB_Pins    = 0; // stores the direct values of A and B to determine movement
volatile int   encoderPos = 0; // stores our current value of encoder position. Change to int or uin16_t instead of byte if you want to record a larger range than 0-255

// Initialise mouse device
PS2dev mouse(mouse_clock,mouse_data);

volatile long int delta_x = 0;

//we start off not enabled, host will enable us later
int enabled    = 0;
int rate       = 40;
int waittime   = (1000/rate);
int resolution = 3;

unsigned long  startMillis;
unsigned long  currentMillis;


///////////////////////////////////////////////////////////////////
// Set up variables etc                                          //
///////////////////////////////////////////////////////////////////
void setup()
{
  digitalWrite (Spin_A, HIGH);     // enable pull-ups
  digitalWrite (Spin_B, HIGH);

  #if (debug)
    Serial.begin(57600);
    delay(100);
    Serial.println("PS2 Mouse emulator Started");
  #endif

  // send the mouse initialisation sequence
  send_mouse_init();
  #if (debug)
    Serial.println("Waiting on host...");
  #endif
  //attachInterrupt (digitalPinToInterrupt(Spin_A), isr, CHANGE);
  startMillis = millis();  //initial start time
}


///////////////////////////////////////////////////////////////////
// Main program loop                                             //
///////////////////////////////////////////////////////////////////
void loop()
{
  unsigned char  c;
  currentMillis = millis();                 //get the current time

  if( (digitalRead(mouse_clock)==LOW) || (digitalRead(mouse_data) == LOW))  // Communication inhibited
  //if(digitalRead(mouse_clock)==LOW)         // host is inhibiting comms
  {
    detachInterrupt (digitalPinToInterrupt(Spin_A));
    #if (debug)
      Serial.print(PIND & 0b00110000);
      Serial.print("Waiting for host...");
    #endif
    //while(mouse.read(&c));
    c=0;
    mouse.read(&c);
    #if (debug)
      Serial.println(" While loop complete...");
    #endif
    if (c != 0) mousecommand(c);
    attachInterrupt (digitalPinToInterrupt(Spin_A), doEncoderRising, RISING);
  }

  // Update mouse every [rate] ms
  if ((currentMillis - startMillis) >= 40)
  {
    if (enabled)
    {
      detachInterrupt (digitalPinToInterrupt(Spin_A));
      #if (debug)
        Serial.print("Send mouse Packet: ");
        Serial.println(delta_x/8);
        //Serial.print("\t");
        //Serial.println(currentMillis);
      #endif
      write_packet();
      startMillis = currentMillis;        // Start timer again from after sending this packet
      attachInterrupt (digitalPinToInterrupt(Spin_A), doEncoderRising, RISING);
    }
  }
  //delay(1);
}


///////////////////////////////////////////////////////////////////
// Interrupt routine for Spin_A - samples on RISING only         //
// Dont skip loops to lower the resolution...                    //
// that road leads to backspin                                   //
///////////////////////////////////////////////////////////////////
void doEncoderRising()
{
  //detachInterrupt (digitalPinToInterrupt(Spin_A));
  noInterrupts(); //stop interrupts happening before we read pin values
  AB_Pins = PIND & 0xC;      // Read A and B, same = CW, different = CCW
  if ((AB_Pins == 0b00001100) || (AB_Pins == 0b00000000)) // Check if Spin_A == Spin_B
  {
    delta_x++;
  }
  else
  {
    delta_x--;
  }
  interrupts(); //restart interrupts
  //attachInterrupt (digitalPinToInterrupt(Spin_A), doEncoderRising, RISING);
}


///////////////////////////////////////////////////////////////////
// Acknowledge a host command                                    //
///////////////////////////////////////////////////////////////////
void send_ack()
{
  while(mouse.write(0xFA));
}


///////////////////////////////////////////////////////////////////
// Send a init sequence to the host                             //
///////////////////////////////////////////////////////////////////
void send_mouse_init()
{
   while(mouse.write(0xAA)!=0); // I have passed self tests
   enabled = 0;
   resolution = 4;
   rate = 100;
   while(mouse.write(0x00)!=0); // I am a standard PS/2 mouse
}


///////////////////////////////////////////////////////////////////
// Send mouse data to the host                                   //
///////////////////////////////////////////////////////////////////
void write_packet() {
  char overflowx =0;
  char data[3];
  int x;

  if (delta_x > 255) {
    overflowx =1;
    x=255;
  }
  if (delta_x < -255) {
    overflowx = 1;
    x=-255;
  }

  data[0] =
    ( ( 0 ) << 7 ) |
    ( ( overflowx & 1 ) << 6 ) |
    ( ( 0 ) << 5 ) |
    ( ( ( (delta_x & 0x100 ) >>8 ) & 1) << 4 ) | // Sign bit
    ( ( 1 ) << 3 ) |
    ( ( 0 ) << 2 ) |
    ( ( 0 ) << 1 ) |
    ( ( 0 ) << 0 ) ;

  data[1] = delta_x & 0xff; // mask off sign bit

  mouse.write(data[0] & 0xFF);     // Flags and buttons
//  mouse.write(data[1]);     // X movement
  mouse.write(delta_x & 0xff);
  mouse.write(0x00 & 0xFF);        // Y movement, never changes for a spinner

  delta_x = 0;
}


///////////////////////////////////////////////////////////////////
// Act upon a command from the host                              //
///////////////////////////////////////////////////////////////////
int mousecommand(int command) {
  unsigned char val;

  //This implements enough mouse commands to get by, most of them are
  //just acked without really doing anything

#if (debug)
  Serial.print("-> Recv    : ");
  Serial.print(command,HEX);
  Serial.print("  ");
#endif
  switch (command) {

  case 0xFF: //reset
    send_ack();
    send_mouse_init();
    #if (debug)
      Serial.print("Mouse Reset");
    #endif
    break;

  case 0xFE: //resend
    send_ack();
    #if (debug)
      Serial.print("Resend");
    #endif
    break;

  case 0xF6: //set defaults
    send_ack();
    enabled    = 0;
    rate       = 100;
    resolution = 4;
    #if (debug)
      Serial.print("Set Defaults (Rate:100 Res:4 Disabled)");
    #endif
    break;

  case 0xF5:  //disable data reporting
    send_ack();
    enabled = 0;
    #if (debug)
      Serial.print("Disable Data Reporting");
    #endif
    break;

  case 0xF4: //enable data reporting
    enabled = 1;
    send_ack();
    #if (debug)
      Serial.print("Enable Data Reporting");
    #endif
    break;

  case 0xF3: //set sample rate
    send_ack();
    mouse.read(&val);
    rate = val;
    waittime=1000/rate;
    send_ack();
    #if (debug)
      Serial.print("Set Sample Rate: ");
      Serial.print(val,DEC);
      Serial.print(" = ");
      Serial.print(waittime,DEC);
      Serial.print("ms");
    #endif
    break;

  case 0xF2: //get device id
    send_ack();
    mouse.write(00);
    #if (debug)
      Serial.print("Get Device ID: 0");
    #endif
    break;

  case 0xF0: //set remote mode
    send_ack();
    #if (debug)
      Serial.print("Set Remote Mode");
    #endif
    break;

  case 0xEE: //set wrap mode
    send_ack();
    #if (debug)
      Serial.print("Set Wrap Mode");
    #endif
    break;

  case 0xEC: //reset wrap mode
    send_ack();
    #if (debug)
      Serial.print("Reset Wrap Mode");
    #endif
    break;

  case 0xEB: //read data
    send_ack();
    write_packet();
    #if (debug)
      Serial.print("Read Data - Packet Sent");
    #endif
    break;

  case 0xEA: //set stream mode
    send_ack();
    #if (debug)
      Serial.print("Set Stream Mode");
    #endif
    break;

  case 0xE9: //status request
    send_ack();
    mouse.write(0x00);
    mouse.write(resolution);        //mouse.write(0x03);
    mouse.write(rate);              //mouse.write(0x64);
    #if (debug)
      Serial.print("Send Status Request: Res=");
      Serial.print(resolution);
      Serial.print(" Rate=");
      Serial.print(rate);
    #endif
    break;

  case 0xE8: //set resolution
    send_ack();
    mouse.read(&val);
    resolution=val;
    send_ack();
    #if (debug)
      Serial.print("Set Resolution: ");
      Serial.print(resolution,DEC);
    #endif
    break;

  case 0xE7: //set scaling 2:1
    send_ack();
    #if (debug)
      Serial.print("Set Scaling 2:1");
    #endif
    break;

  case 0xE6: //set scaling 1:1
    send_ack();
    #if (debug)
      Serial.print("Set Scaling 1:1");
    #endif
    break;
  }
  #if (debug)
    Serial.println();
  #endif
}

/*
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

*/
