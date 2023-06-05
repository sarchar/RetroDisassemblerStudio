# About Retro Disassembler Studio

RDS (Retro Disassembler Studio) aims to be a complete disassembler tool for
retro systems.  Currently, the only supported system is the Nintendo
Entertainment System (NES) but others are planned for the future.

RDS includes an emulator and disassembler and aims to make it easy to annotate
source code for old games.

## Features

* Inline editing of code
* Real time displays of VRAM
* Watches, breakpoints, labels, defines
* Multiple running instances with separate state

## TODO

Still to do are some pretty large tasks:

* Ability to export the project as source code (.ASM files)
* Search and filter changes in memory
* For the emulator: audio, more mappers, optimizations

## Screenshots

![image](https://github.com/sarchar/RetroDisassemblerStudio/assets/4928176/6f1b4540-84cc-48d9-a8b8-862f80dea5a2)

## How to use

In short, create a new project from the file menu.  In the listing window, press 'd' to disassemble.  Other listing commands:

* Shift-F to Follow: will take you to the address specified by the current operand
* Ctrl-R on a label line: show references to the selected label
* Delete: revert disassembled code to data
* Backspace: remove operand (but leave instruction)
* w, s: make data as (w)ord or (s)tring

For the debugger:
* F5 to run the system instance
* F9 to set a breakpoint
* F10 or single step

## Build

More detailed instructions will be written in the future. For now, clone this repository, update the submodules, and create an empty build directory. Use CMake to generate a project, and build it using a modern (C++20) compiler. As it stands now, I've only been developing in Windows with MSVC 2022. It likely doesn't build on other platforms.

## Contact

You can contact me by messaging me on github or sending me an email at <chuck+github@borboggle.com>

## License

This source code is licensed under the BSD-style license found in the
LICENSE file in the root directory of this source tree. 

