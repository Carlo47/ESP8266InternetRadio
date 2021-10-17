# ESP8266InternetRadio
Implements an Internet Radio operated by a single pushbutton.

This program shows how to use the excellent ESP8266audio
library from Earle F. Philhower. It is an adaptation of
his example program "StreamMP3FromHTTP".
Using a pusbutton, stations can be selected forward or
backward from a list. A short click selects the next
station, a long click the previous one. If the selected
URL cannot be played, the next station is tried
automatically. A doubleclick shows the currently played
station. 
