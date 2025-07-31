# Arduino Multi-Sensor Interrupt System - Reflection Report

## System Architecture and Logic

My system uses both Pin Change Interrupts and Timer Interrupts to allow the system to be in one of two states: **Sensor Mode** or **Timer Mode**.

There are three PIR Sensors which are connected to a breadboard. When one of those sensors detects movement, it sends an input to the connected pin. That pin has been set up to enable Pin Change Interrupts, and as such, the current process is interrupted to instead send an output to the LED from one or more of the output pins. Each sensor is assigned a different colour which I have colour coded with the wires, and that is the colour that the sensor will send to the LED to make it change to that colour. If multiple sensors are detected, the LED can receive multiple colours and become a combination of them (such as red and blue being purple).

If the sensors don't detect movement for a while, the system instead enters **Timer Mode**. In this mode, the LED simply flashes yellow (combination of red and green), alternating every second. This is achieved by having a timer, Timer1, which counts up. Once the timer reaches a certain point as defined by OCR1A, it runs an interrupt of its own, which toggles the state of the pins connected to red and green.

Finally, each time the state changes, either by the sensor or by the timer, it sets a flag that monitors the stage changes to true. Then, once the interrupt/s are finished and the code returns to the main loop, if either of the state changes flags are true, it sends a message to the serial monitor to print out information about the current state of each pin.

## Interrupt Configuration and Usage

Pin Change Interrupts were enabled for the PCINT2 group, and specifically enabled for pins 2, 3, and 4. This was achieved by setting:
- `PCICR = 0b00000100` to enable the PCINT2 group
- `PCMSK2 = 0b00011100` to specifically enable interrupts on the target pins

Timer1 was configured in Clear Timer on Compare mode, and was set to activate every 1 second by setting `OCR1A = 16000000/1024 - 1`.

Finally, two variables, `interruptTimer` and `interruptTimeout` were used to set up an interrupt timer. After the sensors detect movement the timer starts and when it reaches the timeout time, the system switches back to Timer Mode. Each time a sensor detects movement though this timer resets.

## Issues Encountered and Resolution

The most significant issue I encountered was a bug where the timer interrupt would run multiple times before the LED would actually toggle. This would result in the following during debugging:

```
TIMER MODE: R:ON G:ON B:OFF
TIMER MODE: R:ON G:ON B:OFF
TIMER MODE: R:ON G:ON B:OFF
TIMER MODE: R:ON G:ON B:OFF
TIMER MODE: R:ON G:ON B:OFF
TIMER MODE: R:ON G:ON B:OFF
TIMER MODE: R:OFF G:OFF B:OFF
TIMER MODE: R:OFF G:OFF B:OFF
TIMER MODE: R:OFF G:OFF B:OFF
TIMER MODE: R:OFF G:OFF B:OFF
TIMER MODE: R:OFF G:OFF B:OFF
TIMER MODE: R:OFF G:OFF B:OFF
TIMER MODE: R:ON G:ON B:OFF
TIMER MODE: R:ON G:ON B:OFF
TIMER MODE: R:ON G:ON B:OFF
TIMER MODE: R:ON G:ON B:OFF
TIMER MODE: R:ON G:ON B:OFF
TIMER MODE: R:ON G:ON B:OFF
```

I tried multiple different solutions after looking around on google and eventually capitulated to just asking AI to fix it for me. However, even the AI was not able to fix this issue as all of its code changes still resulted in the bug persisting.

I eventually decided to take a break and just focus on other stuff, and while testing around with some other changes, I managed to accidentally solve this problem. It was when I changed the function to update the LED. Initially, I used the arduino's built in `digitalWrite` function to send the instructions to the LED. However, by changing this to direct registry manipulation using `PORTB`, the issue was resolved. My understanding of why this is, is that the `digitalWrite` function simply took too long to process, and this would mean that the LED would fail to update each time the interrupt would occur, meaning multiple interrupts would occur before the LED would successfully change.
