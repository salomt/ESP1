# ESP1 - Experiment in Synthesizer Programming

## Project Description

ESP1 is a MIDI synthesizer programmed in 2009 for Helsinki University audio programming course. It uses Portaudio and Portmidi libraries.

It is a simple software synthesizer capable of generating 4 different waveforms, and implements several basic MIDI functionalities, such as:
* Note numbers
* Velocity
* Pitch bend
* Aftertouch (vibrato)
* Modulation (PWM for pulse wave)

In addition, there is an envelope generator that can be adjusted via terminal.


## How to use

ESP1 needs to be run on the terminal. A MIDI keyboard or a software MIDI source is needed. Upon starting, ESP1 will scan all available MIDI inputs, and prompts the user to select one.

When active, there are three options for the user, that can be accessed by pressing the number key and [ENTER].
* 1: set waveform
* 2: set envelope
* 0: quit

The waveform menu has four options:
* 1: pulse
* 2: triangle
* 3: sawtooth
* 4: sine

Setting the envelope prompts for four values: attack, decay, sustain and release, in this order.

The value range for various stages of the envelope are:
* Attack: 0-3000 (msec)
* Decay: 0-3000 (msec)
* Sustain: 0-100 (percent)
* Release: 0-3000 (msec)
