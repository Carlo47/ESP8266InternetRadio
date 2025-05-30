# ESP8266InternetRadio

This program shows how to use the excellent [**ESP8266audio**](https://github.com/earlephilhower)
library from [*Earle F. Philhower*](https://www.hackster.io/earlephilhower/esp8266-digital-radio-ee747f). It is an adaptation of
his example program *StreamMP3FromHTTP*.
Using a pusbutton, stations can be selected forward or
backward from a list. A short click selects the next
station, a long click the previous one. If the selected
URL cannot be played, the next station is tried
automatically. A doubleclick shows the currently played
station.

👉 For output no DAC like the Max98357, PCM5102 or VS1053B 
is needed. If we use an external DAC we have to change the 
audio output class from AudioOutputI2SNoDAC to 
EXTERNAL_DAC (see line 81).

### Operation without external DAC
```
                       _I_
 Wiring           D3 --o o-- GND  Pushbutton from D3 to ground
                  RX --> Vin of speaker driver

 Speaker driver   I connected an 8 Ω / 0.5 W loudspeaker via a MOS-FET
                  driver, which I usually use as an output stage for
                  driving a motor or other loads. This is certainly
                  not the optimal solution for an audio output, but it
                  is sufficient for the experiment.
 
                                     .-------o------o Vext (5..25V)
                                    _|_      |
                                    / \     |¨| Load: Motor,
                                    ¨|¨     |_|       Light bulb,
                                     |       |        Spkr,
                                     +-------o        etc.   
                                     |
                                 |¦--' N-CH MOSFET
                                 |¦<-. T40N03G
                  Vin o-----+----|¦--|
                            |        |
                           |¨|       |
                           |_| 10k   |
                            |        |
                  GND o-----+--------+--------------o GND  

```
### Operation with external Max98357
```
Wiring           D3  --o o-- GND  Pushbutton from D3 to ground
                              .-----------------. 
                  RX  -->     o DIN             |  
                  D4  -->     o LRC        MAX  |    Spkr
                  D8  -->     o BCLK      98357 |    _/|
                  5V  -->     o Vin (5V)        o---|  |
                  GND -->     o GND             o---|_ |
                              `-----------------´     \|
 
```
> To run, set your ESP8266 build to 160MHz, 
> update the WiFi credentials, and compile.
> see: platformio.ini