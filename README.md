## esp32 play wav file from spiffs to bluetooth speaker

**Important! for some reason, the first few seconds of sound are not played on the bluetooth speaker.**

This plays a wav file from an esp32's spiffs storage to a bluetooth speaker. 

It is a modified version of this example: https://github.com/espressif/esp-adf/tree/master/examples/player/pipeline_bt_source

You'll need to install the esp32 ADF - instructions here: https://docs.espressif.com/projects/esp-adf/en/latest/get-started/

To run this example you need to first erase the flash on your esp32

You must then partition/format the esp32 flash. I do this by running:
```
make erase_flash
```

Next, format & partition the esp32 flash. 

I do this by first setting Arduino menu option **Tools/Partition Scheme/No OTA (Large APP)** the running initspiffs/initspiffs.ino from the Arduino IDE. 

After that, run initspiffs.ino in this project. initspiffs.ino should format and partition your esp32's flash.

CAREFUL PITFALL! you need to ensure that the Arduino IDE is using the same partition configuration at that in the partitions.csv in this repo. You'll find when you flash code to an esp32, it displays the partitions there. 

After erasing and partitioning your esp32 flash/spiffs, you must then upload the sample wav file to your esp32 spiffs.  The sample file is located in:
```
data/pcm1644s.wav 
```
To do the upload, I use this utility which is an addon for the Arduino IDE:
https://github.com/me-no-dev/arduino-esp32fs-plugin

You must modify main/app_main.c to specify the device name of your bluetooth speaker.

look for this line, change BT-12 to the device name of your bluetooth speaker.
```
        .remote_name = "BT-12",
```

## Compile and run

```
git clone --recursive https://github.com/bootrino/esp32_spiffs_wav_bluetooth.git
cd esp32_spiffs_wav_bluetooth
make menuconfig
make flash
make monitor
```

