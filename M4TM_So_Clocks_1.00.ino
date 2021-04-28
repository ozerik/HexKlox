#include <TimerOne.h>
#include <TM1637TinyDisplay.h>
#include <EEPROM.h>

/*
              MODULAR FOR THE MASSES
                    SO CLOCKS
             Juanito Moore, April 2021

     This is a six-channel clock. The channels can be set to
     send triggers every 16th note, every 8th, quarter note,
     half note, et cetera, up to 64 bars or even MORE. It
     displays the BPM, and this clock can do swing, even with
     an external trigger.

     It uses a TM1637 four-digit display. I built my example
     with one built by RobotDYN, which includes the decmial
     points, but no colon (like for a clock display).

     There's three buttons, a right/up, left/down, and a reset
     button. There's one input for an external trigger. Finally,
     there's a potentiometer serving as a selector knob.



     -------USER MANUAL--------
     There's eight positions the knob can be in.
     Position 1:
          BPM display and set mode. Right button increases the BPM.
          Left button decreases the BPM. Reset button held for one
          second resets the BPM to 120. All three buttons pressed
          at the same time resets all the clock divisions zero-points
          to the same zero point.
     Positions 2 through 6:
          The display will show a C for channel, the channel number,
          and the clock division. Right button increases the divisor.
          Left button decreases the divisor. Reset button resets the
          channel to start on the next clock tick.
     Position 7:
          SWING MODE! The display shows a S (or a 5 haha) and how
          many milliseconds your 16th note triggers will be swinging.
          The timing goes backward and forward, so your drum pattern
          can start on any clock tick... basically this makes the clock
          "downbeat agnostic" which, I would buy that album.
          Right button swings one way, left button swings the other way,
          reset button when held for two seconds saves the tempo and
          channel divisors and swing value into the Arduino's EEPROM memory.

     With an external trigger connected, the module will automatically
     use the incoming triggers as 16th note triggers. So the module
     expects four pulses (or peaks) per beat -- 4 PPB. The MIDI standard
     is 24 PPB, so if your clock output is going that fast. this module
     won't behave as expected. It should still track, but the BPM won't
     make any sense, and the swing won't work. You'll just have to divide
     down the clocks more. Yikes... and 24 isn't a power-of-two, so the
     divisors won't work without changing the code.

     Please find the schematic for this project over here:




    Version history:
      1.00 released on April 28, 2021, initial release



*/








TM1637TinyDisplay display(4, 5); // 4 = CLK   5 = DIO



unsigned long debounce;
unsigned long newTrig;
unsigned long oldTrig;
unsigned long EIS;
float BPM;
byte BPMcounter;
float BPMs[20];
word counts[6];
long Interval;
volatile unsigned long extInterval;
unsigned long extIntervalSum[10];
word Increment;
int swing;
bool swingTrack;
byte swingTrackV;
int analogKnob;
int knob;
byte switchPos;
byte tracker;
byte tracker2;
byte ext;
const word triggerLength = 8000; // trigger length in microseconds
byte divider[6];

/*
   The divisions[] constant is numerator of a fraction with 16 as the denominator.
   A 1 means "one sixteenth notes". 2 means "two sixteenth notes" which is 1/8th notes.
   16/16 is whole notes. 32 means one trigger per two bars. And so on. The number shown
   on the channel-division-select mode is which number in this array is selected.
   So "C1 1" means channel 1 will be playing 1/16th notes. "C1 5" means whole notes.
   "C1 b" means the 11th number in this array will be selected (B is hex for 11) so that
   will be playing 1024/16th notes, which is... uh... 16 bars?

   These numbers can be changed here and you can get creative weird patterns going if
   that's something you're interested in.
*/
const word divisions[] = {0, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};





void setup() {
  pinMode(2, INPUT_PULLUP); // input for external trigger

  pinMode(8,  OUTPUT);  //PORTB B0000000x, trigger 1 output
  pinMode(9,  OUTPUT);  //PORTB B000000x0, trigger 2 output
  pinMode(10, OUTPUT);  //PORTB B00000x00, trigger 3 output
  pinMode(11, OUTPUT);  //PORTB B0000x000, trigger 4 output
  pinMode(12, OUTPUT);  //PORTB B000x0000, trigger 5 output
  pinMode(13, OUTPUT);  //PORTB B00x00000, trigger 6 output

  pinMode(14, INPUT_PULLUP); //PINC B0000000x "left" button, A0
  pinMode(15, INPUT_PULLUP); //PINC B000000x0 "right" button, A1
  pinMode(16, INPUT_PULLUP); //PINC B00000x00 reset timing button, A2

  //initialize the display
  display.setBrightness(7);
  uint8_t data[] = {0, 0, 0, 0};
  display.setSegments(data);

  { // handles reading the contents of the EEPROM memory
    // this must be saved by moving knob to the swing setting
    // (fully clockwise) and holding the reset button for 2 seconds

    EEPROM.get(0, divider);
    byte address = sizeof(divider);
    EEPROM.get(address, Interval);
    address = address + sizeof(Interval);
    EEPROM.get(address, swing);
    if (divider[0] == 255) { // only gets run when the memory has never been written to
      divider[0] = 1;
      divider[1] = 1;
      divider[2] = 2;
      divider[3] = 4;
      divider[4] = 5;
      divider[5] = 9;
      Interval = 125000;  // 4 PPB, 120 BPM
    }
  } // end of EEPROM reading


  Timer1.initialize();
  Timer1.attachInterrupt(clockISR); // clock advance and trigger stuff

  attachInterrupt(digitalPinToInterrupt(2), extISR, FALLING);

//  Serial.begin(115200);

}

void extISR() {   // this runs when the module gets an external trigger
  
  ext = 1;        // tells the rest of the program that there's external triggering happening

  // averages ten incoming trigger intervals for a more steady BPM display
  if (BPMcounter > 9) BPMcounter = 0;
  newTrig = micros();
  extIntervalSum[BPMcounter] = newTrig - oldTrig;
  oldTrig = newTrig;
  BPMcounter++;
  

  if (swing == 0) {
    swingTrackV = 1;  // forces the clockISR() to play the triggers
    clockISR();
  } else {
    swingTrack = !swingTrack;
    if (swingTrack) {  // every other trigger THIS happens
      if (swing > 0) {  // delays this trigger
        Timer1.setPeriod(swing);
        swingTrackV = 1;
      } else {
        for (byte i = 0; i < 6; i++) {
          counts[i]++;
          if (counts[i] >= divisions[divider[i]]) {
            counts[i] = 0;
            bitWrite(PORTB, i, 1);
          }
        }
        swingTrackV = 0;
        Timer1.setPeriod(triggerLength);
      }
    } else {      // every other OTHER trigger THIS happens
      if (swing < 0) { // delays this trigger!
        Timer1.setPeriod(abs(swing));
        swingTrackV = 1;
      } else {
        for (byte i = 0; i < 6; i++) {
          counts[i]++;
          if (counts[i] >= divisions[divider[i]]) {
            counts[i] = 0;
            bitWrite(PORTB, i, 1);
          }
        }
        swingTrackV = 0;
        Timer1.setPeriod(triggerLength);
      }
    }
  }
  EIS = 0;
  for (byte i = 0; i < 10; i++) {
    EIS = EIS + extIntervalSum[i];
  }
  extInterval = EIS * 0.1;
}


void clockISR()  { // here's the free-running clock
  swingTrackV++;

  if (bitRead(swingTrackV, 0) == 0) { // here's the PLAY NOW section
    for (byte i = 0; i < 6; i++) {
      counts[i]++;
      if (counts[i] >= divisions[divider[i]]) {
        counts[i] = 0;
        bitWrite(PORTB, i, 1);
      }
    }
    Timer1.setPeriod(triggerLength);
  } else if (ext == 0) {
    PORTB &= 11000000;   //turns off triggers
    if (bitRead(swingTrackV, 1) == 0) {
      Timer1.setPeriod(Interval - triggerLength + swing);
    } else {
      Timer1.setPeriod(Interval - triggerLength - swing);
    }
  } else {     // if the external trigger is in play...
    PORTB &= 11000000;
    Timer1.setPeriod(500000);  // restart internal clock in 0.5 seconds unless there's another external trigger event
  }
}



void loop() {
  knob = map(analogRead(A7), 1, 1000, 0, 7);
  switchPos = 0;
  if (bitRead(PINC, 0) == 0) switchPos = 2;
  if (bitRead(PINC, 2) == 0) switchPos = 1;
  if (bitRead(PINC, 1) == 0) switchPos = 3;
  if ((PINC & B00000111) == 0) { // all three buttons pressed simultaneously -- resets all counts to zero
    switchPos = 0;
    for (byte i = 0; i < 6; i++) {
      counts[i] = 0;
    }
  }
  if (micros() - newTrig > 500000) ext = 0;
  if (switchPos == 0) tracker = 0;
  { // knob zero means show BPM, left and right buttons adjust BPM
    if (knob == 0) {
      display.showNumber(BPM, 1);

      { // change BPM
        if (tracker == 0 && switchPos > 0) {
          debounce = millis();
          tracker = 1;
          switch (switchPos) {
            case 1:
              Interval += Increment;
              break;
            case 2:
              Interval -= Increment;
              break;
          }
        }
        if (tracker == 1) {
          if (millis() - debounce > 200) {
            switch (switchPos) {
              case 1:
                Interval += Increment;
                break;
              case 2:
                Interval -= Increment;
                break;
            }
          }
          if (switchPos == 3 && millis() - debounce > 2000) {
            tracker = 2;
            Interval = 125000;
          }
        }
        Interval = constrain(Interval, 68193, 272848);
      } // end of change BPM

    } else if (knob < 7) {        // change the divisions of each channel
      if (bitRead(PINC, 1) == 0 && tracker == 0) {
        counts[knob - 1] = 6000;
        tracker = 1;
      }
      uint8_t data[] = {57, 0, 0, 0};
      data[1] = display.encodeDigit(knob);
      data[3] = display.encodeDigit(divider[knob - 1]);
      display.setSegments(data);
      if (switchPos == 1 && tracker == 0) {
        divider[knob - 1]--;
        tracker = 10;
      }
      if (switchPos == 2 && tracker == 0) {
        divider[knob - 1]++;
        tracker = 10;
      }
      divider[knob - 1] = constrain(divider[knob - 1], 1, 13);
    }
  }


  if (knob == 7) {    // swing adjustment
    uint8_t data[] = {109, 0, 0, 0}; // sets up the display
    if (tracker == 0) {
      if (switchPos == 2) {
        swing += 1000;
        tracker = 1;
      }
      if (switchPos == 1) {
        swing -= 1000;
        tracker = 1;
      }
      if (switchPos == 3) {
        debounce = millis();
        tracker = 4;
      }
    }
    if (tracker == 4 && millis() - debounce > 2000) {
      EEPROM.put(0, divider);
      byte address = sizeof(divider);
      EEPROM.put(address, Interval);
      address = address + sizeof(Interval);
      EEPROM.put(address, swing);
      display.showNumber(8888);
      tracker = 5;
    }
    swing = constrain(swing, -60000, 60000);
    if (swing <= 1) {
      data[1] = 64; // minus sign for negative swing values
    }
    int TS1 = swing * 0.0001;
    data[2] = display.encodeDigit(abs(TS1));
    int TS2 = (swing * 0.001);
    TS2 = TS2 % 10;
    data[3] = display.encodeDigit(abs(TS2));
    display.setSegments(data);
  }
  
  Increment = Interval * 0.000834;  // calculate increment so the BPM will go up by one decmial point per button press

  // snags the important values from the interrupt service routine for external BPM calculations
  noInterrupts();
  byte ext1 = ext;
  unsigned long extInterval1 = extInterval;
  interrupts();

  if (ext1 == 1) {
    BPM = 15000000.0 / extInterval1;
  } else BPM = 15000000.0 / Interval;      // calculate the BPM when self-clocked
}
