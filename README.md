# 2 Ship 2 Harkinian Android - PAL 1.1 Test Build

> [!IMPORTANT]
> This is an **unofficial test build** of the LinkZenic Android port of
> 2 Ship 2 Harkinian.
>
> This fork adds support for the **European/PAL 1.1** version of
> *The Legend of Zelda: Majora's Mask*.
>
> It is not an official release from LinkZenic or HarbourMasters.

## PAL 1.1 Support

This fork adds:

- Majora's Mask N64 PAL/EU 1.1 ROM support
- English, French, German, and Spanish PAL assets
- PAL language selection and persistence
- PAL multilingual file select
- Localized PAL UI assets and minigame text
- Android-compatible PAL asset extraction

A legally obtained Majora's Mask PAL/EU 1.1 ROM is required.

**No game ROM is included with this project or its APK releases.**

This work is based on LinkZenic's `v5.0.1-android.2` Android port.

## Downloads

Prebuilt Android test APKs are available from this fork's Releases page:

https://github.com/alexbcz/2ship2harkinian-Android/releases

Please report issues specific to the PAL build on this repository while the
changes are still being tested.

---

## About the Android Port

Android port of 2 Ship 2 Harkinian, based on the HarbourMasters project and forked from Waterdish's original Android port.

HarbourMasters repository: https://github.com/HarbourMasters/2ship2harkinian

Original Android port: https://github.com/Waterdish/2ship2harkinian-Android

LinkZenic Android port: https://github.com/linkzenic/2ship2harkinian-Android

Base Android release: **v5.0.1-android.2**

Supported: Android 7+ with OpenGL ES 3.0+

Tested on: Android 13

## Installation

1. Download and install the APK from this fork's releases page:
   https://github.com/alexbcz/2ship2harkinian-Android/releases
2. Open the app once so it can create the data folder and copy bundled support files.
3. When prompted, select your legally obtained Majora's Mask PAL/EU 1.1 ROM so the app can generate `mm.o2r`.
4. Subsequent launches should start directly into the game.

Use the Back, Select, or minus controller button, or the Android back gesture/button, to open the 2 Ship 2 Harkinian menu. Use touch controls or a controller to navigate menus.

## Data Folder

The app stores user data in the selected 2S2H data folder. You can view the current folder and change it from Settings > General.

Mods and user preset files should be placed in the relevant folders inside the selected data folder.

## FAQs

**What is different with this fork?**

In addition to LinkZenic's Android-specific changes, this test fork adds support
for the Majora's Mask N64 PAL/EU 1.1 ROM and its English, French, German, and
Spanish assets.

The Android port also includes:

- Move the data folder to an SD card
- Turn touch controls on or off
- Scalable menu sizes

**Which ROM should I use?**

To test the PAL support added by this fork, use the **European/PAL 1.1 N64 ROM**.

You must provide your own legally obtained ROM. The ROM is used locally by the
application to generate `mm.o2r`.

**Why is it immediately crashing?**

Try deleting and regenerating `mm.o2r` from your own ROM.

**My controller is not doing anything.**

Open the menu and check Settings > Controls to confirm the controller is detected and mapped.

**Can I hide the on-screen touch controls?**

Yes. Use Settings > Touch Controls > Disable Touch Controls.

**Can I resize the menu?**

Yes. Use Settings > General > Menu Scale.

## Known Issues

This is a test build. PAL-specific issues may still exist and should be reported
on this fork.

Orientation lock is limited by SDL behavior on Android:
https://github.com/libsdl-org/SDL/issues/6090

Near-plane clipping can occur when the camera is close to walls.
