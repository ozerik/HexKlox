# HexKlox
A six-channel clock generator with 0.1BPM accuracy and variable clock divisions and start points WITH SWING

              MODULAR FOR THE MASSES
                    HexKlox
             Juanito Moore, April 2021

This is a six-channel clock. The channels can be set to     send triggers every 16th note, every 8th, quarter note,     half note, et cetera, up to 64 bars or even MORE. It     displays the BPM, and this clock can do swing, even with     an external trigger.

It uses a TM1637 four-digit display. I built my example     with one built by RobotDYN, which includes the decmial     points, but no colon (like for a clock display).

There's three buttons, a right/up, left/down, and a reset     button. There's one input for an external trigger. Finally,     there's a potentiometer serving as a selector knob.



-------USER MANUAL--------
There's eight positions the knob can be in.
Position 1:
BPM display and set mode. Right button increases the BPM. Left button decreases the BPM. Reset button held for one second resets the BPM to 120. All three buttons pressed at the same time resets all the clock divisions zero-points to the same zero point.
Positions 2 through 6:
The display will show a C for channel, the channel number, and the clock division. Right button increases the divisor. Left button decreases the divisor. Reset button resets the channel to start on the next clock tick.
Position 7:
SWING MODE! The display shows a S (or a 5 haha) and how many milliseconds your 16th note triggers will be swinging. The timing goes backward and forward, so your drum pattern can start on any clock tick... basically this makes the clock "downbeat agnostic" which, I would buy that album. Right button swings one way, left button swings the other way, reset button when held for two seconds saves the tempo and channel divisors and swing value into the Arduino's EEPROM memory.

With an external trigger connected, the module will automatically use the incoming triggers as 16th note triggers. So the module expects four pulses (or peaks) per beat -- 4 PPB. The MIDI standard is 24 PPB, so if your clock output is going that fast. this module won't behave as expected. It should still track, but the BPM won't make any sense, and the swing won't work. You'll just have to divide down the clocks more. Yikes... and 24 isn't a power-of-two, so the divisors won't work without changing the code.





Version history:
 1.01 OLED version for use with the SSD1306 OLED 128x64 pixel display. Uses the Adafruit libraries
 1.00 released on April 28, 2021, initial release

