# ledc
Really ugly opcode to controll LED-Shows in constrained environments.

LEGO has built an incredible [ferris wheel](https://shop.lego.com/en-US/Ferris-Wheel-10247),
but it is even more awesome when you add some more visual effects to it.

The project that powers this repository features the following components:

 - LEGO ferris wheel
 - 12V DC Current
 - 12V to 5V 6A Step-Down Voltage regulator
 - A really huge electronic capacitor
 - A Conrad [C-Control IoT WiFi Board](https://www.conrad.de/de/evaluationsboard-c-control-open-iot-wifi-board-arduino-nano-compatibel-1387029.html) (comparable with an Arduino Nano, but with an ESP8266 added)
 - 12x Pimoroni [Mote APA102 RGB LED-Strip](https://shop.pimoroni.com/products/mote)

As the Arduino Nano is very limited with space there was a need to stick
LED-Shows into a format that consumes less memory - so `ledc` was born.

`ledc` introduces something assembler-like to execute certain
"draw"-operations on LED-Strips. Each instruction is stuffed into a
32-bit word while there are 15 instructions (4 bit of instruction, 28
bit parameters).

There is one special "meta-instruction" `SETCOLOR` that actually has a
3-bit instruction and 29 bit for parameters. This is very ugly but
needed if you want to use the whole set of features of an APA102-LED.
At some places in code this is referenced as `SETCOLOR`, some other
places use `SETCOLOR0` and `SETCOLOR1` for this.

The virtual-machine here is capabale of driving 16 LED-Strips and has
up to 16 registers to store things in background. You may also define
up to 16 groups of LED-Strips and registers that could be controlled
using normal instructions as you would controll a single LED-Strip or
register.

## Assembler

| Instruction | Parameter                                  | Description                                                       |
|-------------|--------------------------------------------|-------------------------------------------------------------------|
| SETCOLOR    | Brightness, Red, Green, Blue               | Set the current color to draw                                     |
| SETDEST     | Destinaiton                                | Set the current destination for drawing                           |
| SETS        | Start, Length                              | Set color for series of pixels on current destination             |
| SETI        | Index1 ... Index6                          | Set color for pixels by their index                               |
| COPY        | Source, Destination, Start, Length, Offset | Copy set of pixels from source to destination, shifting by offset |
| MIRRORS     | Source, Destination, Length                | Mirror an entire strip to a set of other strips                   |
| SHIFTR      | Start, Length                              | Shift all of LED-Strips contents to the right                     |
| GROUPADDS   | Group, Start, Length                       | Append a series of LED-Strips or registers to a group             |
| GROUPADDI   | Group, Index1 ... Index3                   | Append given LED-Strips or registers to a group                   |
| FRAME       | Wait                                       | Display the changes on the LED-Strips and Wait                    |
| GOTO        | Label                                      | Jump to a given label and continue execution from there on        |

## Compiler
There is a small compiler written in PHP located in `compiler/`. It's a
CLI-Script that is invoked in the way

~~~ {.bash}
php led-compile.php {path-to-led-programm}
~~~

It produces an `.ledc`-file that is suitable for upload to the
Arduino-Interpreter.

## Emulator
There is an Emulator to run `ledc`-programms via Javascript in HTML located in `emulator/`.

## Incomplete
This repository is incomplete at the moment as components are in different
places and have to be concentrated here.
