# DWIN Display – SD Card Download Instructions

This document explains how to properly prepare an SD card and download project files to a **DWIN HMI display**.

---

## 1. SD Card Formatting (Required)

The SD card **must be formatted as FAT32** using the command line.  
**Do NOT use right-click → Format**, as this often does not work correctly with DWIN displays.

### Windows Command Prompt (Run as Administrator)

- g: is the drive letter of your SD card. This may be different on your PC (e.g., h: or i:).
- Ensure there is a space after the colon (g: /fs:fat32).

```cmd
format g: /fs:fat32 /a:4096
```

### Linux Terminal

- Replace /dev/sdX with your actual device (e.g., /dev/sdb). Be absolutely certain you've identified the correct device, as formatting the wrong drive will erase all data.
- -s 8 in Linux creates 4096-byte sectors (8 × 512 bytes = 4096 bytes).

```cmd
# First, identify your SD card device (e.g., /dev/sdb, /dev/mmcblk0)
sudo fdisk -l

# Unmount all partitions on the device (if mounted)
sudo umount /dev/sdX*

# Format as FAT32 with 4096-byte allocation unit
sudo mkfs.fat -F 32 -s 8 /dev/sdX
```

## Preparing the SD Card Files

After formatting the SD card:

- Copy the folder named DWIN_SET in the root directory of the SD card.

## Downloading Files to the DWIN Display

- Power OFF the DWIN display.
- Insert the prepared SD card into the display.
- Power ON the display.
- The display will automatically:
  - Detect the SD card
  - Check for the DWIN_SET folder in the root directory
  - Download the files into internal FLASH memory

## Completing the Download

- When the blue screen indicates that the download is complete:
  - Power OFF the display.
  - Remove the SD card.
  - Power ON the display again.

The project is now successfully downloaded and stored in the DWIN display.
