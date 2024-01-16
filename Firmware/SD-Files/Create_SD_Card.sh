#!/bin/bash

# Prompt for SD-Card Drive Letter (in Unix, it will be the mount point)
echo "Please enter your SD-Card mount point (e.g., /media/username/sdcard):"
read driveMountPoint

# Unmount the SD Card to be safe
echo "Unmounting the SD card..."
sudo umount $driveMountPoint

# Format the SD Card to FAT32
echo "Formatting the SD card to FAT32..."
sudo mkfs.fat -F 32 -n R-Trickler $driveMountPoint

# Copy files to the SD Card
echo "Copying files to the SD card..."
sudo cp -Rv ./SD-Files/* $driveMountPoint/

# Sync to ensure all data is written
sync

echo "-----"
echo "Done!"
echo "-----"
