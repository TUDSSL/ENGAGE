#!/bin/bash
cd external
wget -q https://mikrocontroller.bplaced.net/wordpress/wp-content/uploads/2018/04/F746_Gameboy_v123.zip
unzip -q F746_Gameboy_v123.zip
mv F746_Gameboy F746_Gameboy_tmp
mkdir F746_Gameboy
mkdir F746_Gameboy/inc
mkdir F746_Gameboy/roms
mkdir F746_Gameboy/src

mv F746_Gameboy_tmp/inc/z80_opcode_cb.h F746_Gameboy/inc/
mv F746_Gameboy_tmp/inc/z80_opcode_table.h F746_Gameboy/inc/

mv F746_Gameboy_tmp/roms/Boot_Rom.c F746_Gameboy/roms/
mv F746_Gameboy_tmp/src/gameboy_ub.c F746_Gameboy/src/ # diff
mv F746_Gameboy_tmp/inc/gameboy_ub.h F746_Gameboy/inc/ # diff
mv F746_Gameboy_tmp/src/z80_ub.c F746_Gameboy/src/     # diff
mv F746_Gameboy_tmp/inc/z80_ub.h F746_Gameboy/inc/     # diff
mv F746_Gameboy_tmp/inc/z80_opcode.h F746_Gameboy/inc/ # diff

mv F746_Gameboy_tmp/src/z80_opcode_cb.c F746_Gameboy/inc/z80_opcode_cb_func.h
mv F746_Gameboy_tmp/src/z80_opcode.c F746_Gameboy/inc/z80_opcode_func.h
touch F746_Gameboy/inc/z80_opcode_goto.h
touch F746_Gameboy/inc/gameboy_file.h

rm F746_Gameboy_v123.zip 
rm -r F746_Gameboy_tmp
cd ..

# Apply optimization patches
patch external/F746_Gameboy/src/gameboy_ub.c < scripts/patches/gameboy_ub-c.patch
patch external/F746_Gameboy/inc/gameboy_ub.h < scripts/patches/gameboy_ub-h.patch

patch external/F746_Gameboy/inc/gameboy_file.h < scripts/patches/gameboy_file-h.patch
patch external/F746_Gameboy/roms/Boot_Rom.c < scripts/patches/rom.patch
patch external/F746_Gameboy/inc/z80_opcode.h < scripts/patches/z80_opcode-h.patch

patch external/F746_Gameboy/src/z80_ub.c < scripts/patches/z80_ub-c.patch
patch external/F746_Gameboy/inc/z80_ub.h < scripts/patches/z80_ub-h.patch

patch external/F746_Gameboy/inc/z80_opcode_cb_func.h < scripts/patches/z80_opcode_cb_func-h.patch
patch external/F746_Gameboy/inc/z80_opcode_func.h < scripts/patches/z80_opcode_func-h.patch
patch external/F746_Gameboy/inc/z80_opcode_goto.h < scripts/patches/z80_opcode_goto-h.patch

