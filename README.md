# BlueSCSI tools for the Commodore Amiga.

Currently there are 2 utilities for managing a BlueSCSI V2 in a Commodore Amiga.
For more information on BlueSCSI please see https://github.com/BlueSCSI/BlueSCSI-v2

BlueSCSI is copyright Eric Helgeson. The BlueSCSI name and logo used with permission.

You can select the device/unit from the properties of the icon.
Optionally you can specify these on the command line.

## CD Changer
The CD Changer allows you to swap between CD ISO images on your SD card on the fly.

![CD Changer](CDChanger.png)

## SD Transfer
The SD Transfer tool allows you to transfer files from the SD card to the Amiga.

![CD Changer](SDTransfer.png)

**History**
* 1.1 (13.5.2024)
- As per the Toolbox Developer Docs corrected the Command Descriptor Block length from 6 to 10. I believe this was causing the SCSI stack crashes I was seeing when selecting a non-BlueSCSI device.
