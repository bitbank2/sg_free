SmartGear multi-system emulator<br>
Written by Larry Bank
Copyright (c) 1998-2017 BitBank Software, Inc.

bitbank@pobox.com

SmartGear is a multi-system game emulator capable of emulating various classic game system. This
repo contains code to emulate the GameBoy Color, GameGear/Sega Master System and NES. There are
plenty of other open source game emulators available. What makes SmartGear unique is its focus
on optimized performance for resource-limited devices. This repo also contains code to
efficiently update a SPI-connected LCD display using a dirty-tile scheme. Depending on how much
changes each frame, many games can achieve at or near 60 frames per second even though the
SPI interface limits the frame rate if a full screen were to be transmitted each frame.

The code is portable and can be compiled on any CPU, but I've included optimized functions for
ARM (32+64-bit) and x64 systems.

Licensing:<br>
----------<br>
SmartGear is released under the GPL3 license. You are free to use and distribute the code
under the terms of this license. For commercial licensing, please contact me for terms.

There are two separate executables that can be built from this source. One is a Linux/GTK GUI
which uses SDL2 or direct framebuffer access to render the video/audio. The other is designed
to run on a "GameBoyZero" which makes use of a SPI LCD (e.g. ILI9341).

Requirements:<br>
-------------<br>
For GTK - libgtk-3-dev<br>
For both - libpng-dev libsdl2-dev<br>
e.g. sudo apt install libgtk-3-dev<br>

Building:<br>
---------<br>
To build the GTK version, type: make<br>
To build the SPI version, type: make -f make_spi<br>

Configuring:<br>
------------<br>
SmartGear uses a plain text file to define the configuration. This is less
important for the GTK version since many of the controls are exposed as buttons.
It's critical for the LCD version since the LCD wiring and type must be known
by SmartGear in order to initialize the display properly. The configuration
file is read from the directory where the executable is run and must be named
'smartgear.config'. There are comments with examples for each section and
several pre-made files for a few of the popular gaming handhelds which use a
Raspberry Pi as the guts.

Running:<br>
--------<br>
The GTK executable is named 'sg' and the SPI LCD version is named 'sg_spi'. In
order to use the SPI kernel driver and the GPIO pins, the sg_spi executable
must run as root. If you run it with 'sudo', it can interfere with the audio
driver (e.g. USB audio adapters) which don't work when run as root. To solve
this problem, the makefile assigns root privilege to the executable. This
allows it to run as root and not interfere with audio drivers. To start either
version, just type ./sg or ./sg_spi on the command line.
<br>
Running:<br>
--------<br>
The GTK executable is named 'sg' and the SPI LCD version is named 'sg_spi'. In
order to use the SPI kernel driver and the GPIO pins, the sg_spi executable
must run as root. If you run it with 'sudo', it can interfere with the audio
driver (e.g. USB audio adapters) which don't work when run as root. To solve
this problem, the makefile assigns root privilege to the executable. This
allows it to run as root and not interfere with audio drivers. To start either
version, just type ./sg or ./sg_spi on the command line.
<br>
sg_spi: The menus and file selector are navigated with by pressing up/down/A on the given controller (gamepad/keyboard/GPIO).

