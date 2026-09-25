# 2 Ship 2 Harkinian

Unofficial fork of 2 Ship 2 Harkinian adding support for the European / PAL 1.1 release of The Legend of Zelda: Majora's Mask.

This fork supports the PAL 1.1 ROM on Android, Windows, and Linux, including English, French, German, and Spanish.

Original repository: https://github.com/HarbourMasters/2ship2harkinian

Original Android port: https://github.com/Waterdish/2ship2harkinian-Android

Android fork used as the base for this port: https://github.com/linkzenic/2ship2harkinian-Android

## Supported Platforms

- Android ARM64
- Windows x64
- Linux x86_64

## Supported ROM

Majora's Mask European / PAL 1.1.

Supported languages:

- English
- French
- German
- Spanish

## Installation

Download the appropriate build from:

https://github.com/alexbcz/2ship2harkinian-PAL/releases

### Android

1. Install the APK.
2. Open the app.
3. When prompted, select your PAL 1.1 Majora's Mask ROM so the app can generate `mm.o2r`.
4. Subsequent launches should start directly into the game.

Android 7+ with OpenGL ES 3.0+ is required.

Use the Back, Select, or minus controller button, or the Android back gesture/button, to open the 2 Ship 2 Harkinian menu.

### Windows

Extract the Windows archive and launch `2ship.exe`.

Keep `2ship.o2r` in the same directory as the executable.

On first launch, select your PAL 1.1 Majora's Mask ROM when prompted.

### Linux

Extract the Linux archive and launch `2s2h.elf`.

Keep `2ship.o2r` in the same directory as the executable.

On first launch, select your PAL 1.1 Majora's Mask ROM when prompted.

## Data Folder

On Android, the app stores user data in the selected 2S2H data folder. You can view the current folder and change it from Settings > General.

Mods and user preset files should be placed in the relevant folders inside the selected data folder.

## FAQs

**What is different with this fork?**

This fork adds support for the Majora's Mask European / PAL 1.1 ROM on Android, Windows, and Linux.

The Android version also includes:

- Move the data folder to an SD card
- Turn touch controls on or off
- Scalable menu sizes

**Why is it immediately crashing?**

Try deleting and regenerating `mm.o2r` from your ROM.

**My controller is not doing anything.**

Open the menu and check Settings > Controls to confirm the controller is detected and mapped.

**Can I hide the on-screen touch controls?**

Yes. Use Settings > Touch Controls > Disable Touch Controls.

**Can I resize the menu?**

Yes. Use Settings > General > Menu Scale.

## Known Issues

Orientation lock is limited by SDL behavior on Android: https://github.com/libsdl-org/SDL/issues/6090

Near-plane clipping can occur when the camera is close to walls.
