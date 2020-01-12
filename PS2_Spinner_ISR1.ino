#include <ps2dev.h>

#define Spin_A      2  // Spinner Quadrature A
#define Spin_B      3  // Spinner Quadrature B
#define INTERRUPT   0  // that is, pin 2

#define mouse_data  4  // Mouse Data
#define mouse_clock 5  // Mouse Clock

volatile boolean fired;
volatile boolean up;

PS2dev mouse(mouse_clock,mouse_data); // 2 data 3clock

long int delta_x = 0;

//we start off not enabled, host will enable us later
int enabled =0;

unsigned long  startMillis;
unsigned long  currentMillis;


///////////////////////////////////////////////////////////////////
// Set up variables etc                                          //
///////////////////////////////////////////////////////////////////
void setup()
{
  unsigned char val;
  digitalWrite (Spin_A, HIGH);     // enable pull-ups
  digitalWrite (Spin_B, HIGH);

  //Serial.begin(9600);
  //delay(500);

  // send the mouse start up
  send_mouse_init();
  //attachInterrupt (digitalPinToInterrupt(Spin_A), isr, CHANGE);   // interrupt 0 is pin 2, interrupt 1 is pin 3
  startMillis = millis();  //initial start time
  //Serial.println("PS2 Mouse emulator Started");
}


///////////////////////////////////////////////////////////////////
// Main program loop                                             //
///////////////////////////////////////////////////////////////////
void loop() {
  unsigned char  c;
  //static int8_t val;
  currentMillis = millis();                 //get the current "time"
  if( (digitalRead(mouse_clock)==LOW) || (digitalRead(mouse_data) == LOW))
  {
    detachInterrupt (digitalPinToInterrupt(Spin_A));
    fired = false;
    while(mouse.read(&c)) ;
    mousecommand(c);
    attachInterrupt (digitalPinToInterrupt(Spin_A), isr, CHANGE);
  }
  //Serial.println(delta_x);

  // Check for spinner movement, if the previous and the current state of the Spin_A are different, a Pulse has occurred
  if (fired)
  {
    if (up)
      delta_x-=1;
    else
      delta_x+=1;
    fired = false;
    //if ((delta_x > 255) || (delta_x < -255)) detachInterrupt (digitalPinToInterrupt(Spin_A));
  }

  if ((enabled) && ((currentMillis - startMillis) >= 15))
  {
  //Serial.println("Send mouse Packet");
    detachInterrupt (digitalPinToInterrupt(Spin_A));
    fired = false;
    write_packet();
    attachInterrupt (digitalPinToInterrupt(Spin_A), isr, CHANGE);
    startMillis = currentMillis;        // Start timer again from after sending this packet
  }
}

///////////////////////////////////////////////////////////////////
// Interrupt Service Routine for a change to encoder pin A       //
///////////////////////////////////////////////////////////////////
void isr ()
{
 if (digitalRead (Spin_A))
   up = digitalRead (Spin_B);
 else
   up = !digitalRead (Spin_B);
 fired = true;
}


///////////////////////////////////////////////////////////////////
// Acknowledge a host command                                    //
///////////////////////////////////////////////////////////////////
void send_ack() {
  while(mouse.write(0xFA));
}


///////////////////////////////////////////////////////////////////
// Send a init sequence to the host                             //
///////////////////////////////////////////////////////////////////
void send_mouse_init()
{
   while(mouse.write(0xAA)!=0); // I have passed self tests
   while(mouse.write(0x00)!=0); // I am a standard PS/2 mouse
}


///////////////////////////////////////////////////////////////////
// Send mouse data to the host                                   //
///////////////////////////////////////////////////////////////////
void write_packet() {
  char overflowx =0;
  char overflowy =0;
  char data[3];
  int x,y;

  if (delta_x > 255) {
    overflowx =1;
    x=255;
  }
  if (delta_x < -255) {
    overflowx = 1;
    x=-255;
  }

  data[0] =
    ( ( 0) << 7) |
    ( (overflowx & 1) << 6) |
    ( ( 0) << 5) |
    ( ( ((delta_x &0x100)>>8)& 1) << 4) |
    ( ( 1) << 3) |
    ( ( 0) << 2) |
    ( ( 0) << 1) |
    ( ( 0) << 0) ;

  data[1] = delta_x & 0xff;

  mouse.write(data[0]);  // Flags and buttons
  mouse.write(data[1]);  // X movement
  mouse.write(0x00);     // Y movement, never changes

  delta_x = 0;
}


///////////////////////////////////////////////////////////////////
// Act upon a command from the host                              //
///////////////////////////////////////////////////////////////////
int mousecommand(int command) {
  unsigned char val;

  //This implements enough mouse commands to get by, most of them are
  //just acked without really doing anything

//Serial.print("Command: ");
//Serial.print(command,HEX);
//Serial.print("  ");
  switch (command) {
  case 0xFF: //reset
    send_ack();
    send_mouse_init();
    break;
  case 0xFE: //resend
    send_ack();
    break;
  case 0xF6: //set defaults
    //enter stream mode
    send_ack();
    break;
  case 0xF5:  //disable data reporting
    //FM
    send_ack();
    break;
  case 0xF4: //enable data reporting
    //FM
    enabled = 1;
    send_ack();
  //Serial.print("Enable Data reporting :-)");
    break;
  case 0xF3: //set sample rate
    send_ack();
    mouse.read(&val); // for now drop the new rate on the floor
  //Serial.print("Set Sample Rate: ");
  //Serial.print(val,HEX);
    send_ack();
    break;
  case 0xF2: //get device id
    send_ack();
    mouse.write(00);
  //Serial.print("Sent Device ID: 0");
    break;
  case 0xF0: //set remote mode
    send_ack();
    break;
  case 0xEE: //set wrap mode
    send_ack();
    break;
  case 0xEC: //reset wrap mode
    send_ack();
    break;
  case 0xEB: //read data
    send_ack();
    write_packet();
    break;
  case 0xEA: //set stream mode
    send_ack();
    break;
  case 0xE9: //status request
    send_ack();
    //      send_status();
    break;
  case 0xE8: //set resolution
    send_ack();
    mouse.read(&val);
  //Serial.print("Set Resolution: ");
  //Serial.print(val,HEX);
    send_ack();
    break;
  case 0xE7: //set scaling 2:1
    send_ack();
    break;
  case 0xE6: //set scaling 1:1
    send_ack();
    break;
  }
//Serial.println();
}
