# QuietCool Whole House Attic Fan support in ESPHome and Home Assistant

This project aims to provide a way to add your QuietCool whole house attic fan to Home Assistant via ESP Home. Huge thank you to Caleb Crome who reverse engineered an older model and released the code on [his Github](https://github.com/ccrome/quiet-cool-rf-remote). My version of this works on the newer fans that use the "glass" remote with touchscreen buttons rather than physical ones.

The QC fans require the remote to be paired with the fan so you have two options for how to connect. You either need to know the ID value of your existing remote or you need to pair a "remote" with a new ID to your fan. See the comments in the sample [component.yaml](component.yaml) file for details.

# Quickstart

First you will need an ESP32 and a CC1101 chip. Both of these are easily available online. You'll need to wire them up together either just on a breadboard or soldered and mounted in a nice little box. Here is what my finished device looks like:

![ESP32 and CC1101 chip mounted on a prototyping board in a 3d printed box](images/esp32-cc1101.png "esp32-cc1101 complete")

You can use the ESP32's default SPI pins or any of the digital IO pins, you'll just need to make sure the pin settings in the component YAML match how you have things wired up. Note that my CC1101 chip has a max voltage of 3.6V so make sure you don't wire it to the 5V output of the ESP32 or direct to your power supply if you're using 5V.

Once you have your hardware all squared away, you'll need [ESPHome](https://esphome.io). For most uses, you're probably using ESPHome along side [Home Assistant](https://home-assistant.io). Either way, the easiest thing to do is to launch your ESPHome dashboard and add a new device. You'll probably want to add more configuration options like wifi, Home Assistant connection, and captive portal setup to the component.yaml. Typically your ESPHome configuration will do that and you can just paste the contents of this repository's [component.yaml](component.yaml) at the end of the stuff that the new device wizard creates.

Simply change the settings in the YAML to match your pins and remote ID (see comments for how to determine remote ID if you don't know it and aren't pairing), install to the ESP32 using the ESPHome dashboard, and you should now be able to control your house fan from the ESP32 device. If you're using ESPHome in Home Assistant it should show up as a fan device. If you're not using Home Assistant you can add `web_server:` to the component YAML and navigate to the device's IP address (visible from the device logs in ESPHome) in a browser and control it from there.

# Reverse Engineering the QuietCool RF Protocol

I'll describe my process below but will give the details of the protocol first. I believe the remote uses a Silicon Labs radio chip. That doesn't really matter, but the sync word used matches the Si4xxx series chips' default sync word so I think it's a safe bet. Everything else is somewhat standard RF formats with one exception.

## Radio settings

The following are the transmit settings for the signals:

| Parameter         | Value             | Notes                                                             |
|-------------------|-------------------|-------------------------------------------------------------------|
| Modulation        | 2-FSK             |                                                                   |
| Data rate         | 2400bps           | Recordings have it to 2390 but 2400 works                         |
| Packet mode       | Variable length   | All commands have 6 bytes of data but use variable length format  |
| Sync mode         | 16/16             | Two-byte sync word                                                |
| Sync word         | 0x2d 0xd4         |                                                                   |
| Preamble length   | 8 bytes           | 64 bits of repeating '101010...'                                  |

## Packet format

| Field             | Size (bits)   | Example                                                   |
|-------------------|---------------|-----------------------------------------------------------|
| Start             | 8 (ish)       | 0x55                                                      |
| Preamble          | 64            | 10101010...                                               |
| Sync word         | 16            | 0x2D 0xD4                                                 |
| Length            | 8             | 6                                                         |
| Remote ID         | 32            | 0xCB 0x00 0xD5 0x12                                       |
| Command           | 8             | 0xBF                                                      |
| Command Repeat    | 8             | 0xBF                                                      |

Each packet is sent 3 times, separated by 70ms. The command codes are:

| Command   | Value   |
|-----------|---------|
| Wake      | 66      |
| On        | bf      |
| Off       | 80      |
| Low       | 1f      |
| High      | 3f      |

I did not capture the commands for timer settings since almost everyone would do that using their home automation anyway.
