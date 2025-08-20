# ESP Assistant

ESP Assistant is a DIY voice assistant powered by an ESP32.

## Description
When reset, the ESP32 starts listening through an INMP441 microphone, captures your voice, and sends the audio via local HTTP to a PC.  

The PC handles speech-to-text conversion, queries the OpenAI API, and then uses text-to-speech (TTS) to generate the audio response. The audio file is sent back via HTTP to the ESP32, stored on SPIFFS, and played on a JBL speaker using the Bluetooth A2DP library.

Currently, only short audio clips can be played. Future versions with SD card support will allow longer and higher-quality audio responses.

## Links
- Project website: [gelotek.com/espAssist](https://gelotek.com/espAssist)  
- YouTube: [gelotek1](https://www.youtube.com/@gelotek1)

## License
Personal use, modification, and redistribution are allowed. Commercial use is prohibited without prior consent. © Gelotek
