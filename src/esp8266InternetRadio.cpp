/**
 * Program      esp8266InternetRadio.cpp
 * Author       2021-07-11 Charles Geiser (https://www.dodeka.ch)
 * 
 * History      2021-07-25 I received the long awaited DAC/amplifier 
 *                         MAX98357A from China and extended the program
 *                         for compilation with or without the breakout
 *                         board 
 * 
 * Purpose      This program shows how to use the excellent ESP8266audio
 *              library from Earle F. Philhower. It is an adaptation of
 *              his example program "StreamMP3FromHTTP".
 *              Using a pusbutton, stations can be selected forward or
 *              backward from a list. A short click selects the next
 *              station, a long click the previous one. If the selected
 *              URL cannot be played, the next station is tried
 *              automatically. A doubleclick shows the currently played
 *              station. 
 * 
 *              👉 For output no DAC like the Max98357, PCM5102 or 
 *                 VS1053B is needed.
 *                 If we use an external DAC we have to change the 
 *                 audio output class from AudioOutputI2SNoDAC to 
 *                 AudioOutputI2S, which we can achieve by defining
 *                 EXTERNAL_DAC (see line 81).
 * 
 * Board        Wemos D1 R2
 * 
 * ------------ Without external DAC -----------------------------------
 *                       _I_
 * Wiring           D3 --o o-- GND  Pushbutton from D3 to ground
 *                  RX --> Vin of speaker driver
 * 
 * Speaker driver   I connected an 8 Ω / 0.5 W loudspeaker via a MOS-FET
 *                  driver, which I usually use as an output stage for
 *                  driving a motor or other loads. This is certainly
 *                  not the optimal solution for an audio output, but it
 *                  is sufficient for the experiment.
 * 
 *                                     .-------o------o Vext (5..25V)
 *                                    _|_      |
 *                                    / \     |¨| Load: Motor,
 *                                    ¨|¨     |_|       Light bulb,
 *                                     |       |        Spkr,
 *                                     +-------o        etc.   
 *                                     |
 *                                 |¦--' N-CH MOSFET
 *                                 |¦<-. T40N03G
 *                  Vin o-----+----|¦--|
 *                            |        |
 *                           |¨|       |
 *                           |_| 10k   |
 *                            |        |
 *                  GND o-----+--------+--------------o GND
 * 
 * ------------ With external DA MAX98357 ------------------------------
 *                        _I_
 * Wiring           D3  --o o-- GND  Pushbutton from D3 to ground
 *                              .-----------------. 
 *                  RX  -->     o DIN             |  
 *                  D4  -->     o LRC        MAX  |    Spkr
 *                  D8  -->     o BCLK      98357 |    _/|
 *                  5V  -->     o Vin (5V)        o---|  |
 *                  GND -->     o GND             o---|_ |
 *                              `-----------------´     \|
 * 
 * ---------------------------------------------------------------------
 * 
 * Remarks      To run, set your ESP8266 build to 160MHz, 
 *              update the WiFi credentials, and compile.
 *              see: platformio.ini  
 *
 * References   https://github.com/earlephilhower
 *              https://www.hackster.io/earlephilhower/esp8266-digital-radio-ee747f
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include "AudioGeneratorMP3.h"
#include "AudioFileSourceICYStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioOutputI2S.h"
#include "AudioOutputI2SNoDAC.h"
#include "PushButton.h"

#define EXTERNAL_DAC  // this line only if we use an external DAC
#define pinButton D3

typedef struct { const char *name; const char *url; } Radiostation;
Radiostation station[] =
{
  { "SRF1 AG-SO",    "http://stream.srg-ssr.ch/m/regi_ag_so/mp3_128" },
  { "SRF2",          "http://stream.srg-ssr.ch/m/drs2/mp3_128" },
  { "SRF3",          "http://stream.srg-ssr.ch/m/drs3/mp3_128" },
  { "SRF4 NEWS",     "http://stream.srg-ssr.ch/m/drs4news/mp3_128" },
  { "SWISS CLASSIC", "http://stream.srg-ssr.ch/m/rsc_de/mp3_128" },
  { "SWISS JAZZ",    "http://stream.srg-ssr.ch/m/rsj/mp3_128" },
  { "MUSIKWELLE",    "http://stream.srg-ssr.ch/m/drsmw/mp3_128" },
  { "BLASMUSIK",     "http://stream.bayerwaldradio.com/allesblasmusik" },
  { "KVB",           "http://kvbstreams.dyndns.org:8000/wkvi-am" },
  { "Klassik Radio", "http://stream.klassikradio.de/live/mp3-128/stream.klassikradio.de/" },
  { "DLF",           "http://st01.dlf.de/dlf/01/128/mp3/stream.mp3" },
  { "WDR",           "http://wdr-1live-live.icecast.wdr.de/wdr/1live/live/mp3/128/stream.mp3" },
  { "SWR4",          "http://swr-swr4-bw.cast.addradio.de/swr/swr4/bw/mp3/128/stream.mp3}" },
};
constexpr uint8_t nbrRadiostations = sizeof(station) / sizeof(station[0]);

// Forward declaration of functions
void initStream();
void nextStation();
void prevStation();
void startPlaying();
void stopPlaying();
void showCurrent();

// Enter your WiFi ssid and password here:
const char ssid[]      = "YOUR SSID";
const char password[]  = "YOUR PSK";
uint8_t currentStation = 4; // preselect your favorite station
const char *currentUrl = station[currentStation].url;
int volume = 100;
char title[64];
char status[64];
const int preallocateBufferSize = 4*1024;
const int preallocateCodecSize  = 10*1024; // MP3 codec max mem needed
void *preallocateBuffer = NULL;
void *preallocateCodec  = NULL;

AudioGeneratorMP3         *decoder;
AudioFileSourceICYStream  *file;
//AudioFileSourceHTTPStream *file;  // there are more functioning stations than with ICYstream, but no metadata is displayed
AudioFileSourceBuffer     *buff;
#ifdef EXTERNAL_DAC
  AudioOutputI2S          *out;     // with external MAX98357 DAC/amplifier
#else
  AudioOutputI2SNoDAC     *out;     // no external DAC used
#endif

PushButton button(pinButton);

/**
 * Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
 */
void cbMetaData(void *cbData, const char *type, bool isUnicode, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void) isUnicode; // Punt this ball for now
  // Note that the type and string may be in PROGMEM, so copy them to RAM for printf
  char s1[32], s2[120];
  strncpy_P(s1, type, sizeof(s1));
  s1[sizeof(s1)-1]=0;
  strncpy_P(s2, string, sizeof(s2));
  s2[sizeof(s2)-1]=0;
  Serial.printf_P(PSTR("METADATA(%s) '%s' = '%s'\n"), ptr, s1, s2);
  Serial.flush();
}

/**
 * Called when there's a warning or error (like a buffer underflow or decode hiccup)
 */
void cbStatus(void *cbData, int code, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  // Note that the string may be in PROGMEM, so copy it to RAM for printf
  char s1[120];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1)-1]=0;
  Serial.printf_P(PSTR("STATUS(%s) '%d' = '%s'\n"), ptr, code, s1);
  Serial.flush();
}

/**
 * Free allocated resources decoder, buffer, file 
 */
void stopPlaying()
{
  if (decoder) 
  {
    decoder->stop();
    delete decoder;
    decoder = NULL;
  }
  if (buff) 
  {
    buff->close();
    delete buff;
    buff = NULL;
  }
  if (file) 
  {
    file->close();
    delete file;
    file = NULL;
  }
  Serial.printf_P(PSTR("Playing stopped\n"));
}

/**
 * Delete decoder, buffer and file, reallocate them
 * and restart playing
 */
void startPlaying()
{
  Serial.println(F("DEBUG: startPlaying() starting"));
  stopPlaying();
  Serial.println(F("DEBUG: startPlaying calling initStream()"));
  initStream();
  Serial.println(F("DEBUG: startPlaying initStream() returned"));
  if (out) out->SetGain(((float)volume)/100.0); // Null check for out
  Serial.println(F("DEBUG: out->SetGain() called"));
  if (decoder && !decoder->isRunning()) // Null check for decoder
  {
    Serial.println(F("DEBUG: decoder->isRunning() checked"));
    Serial.printf_P(PSTR("Can't connect to URL, try next station\n"));
    nextStation();
  }
  else if (decoder) // Check if decoder is not null before printing success
    Serial.printf_P(PSTR("URL connected, now playing\n"));
  else
    Serial.println(F("DEBUG: Decoder is null, cannot play."));
  Serial.println(F("DEBUG: startPlaying() completed or deferred"));
}

/**
 * Select next station on button click
 */
void nextStation()
{
  currentStation++;
  if (currentStation == nbrRadiostations) currentStation = 0;
  startPlaying();
  showCurrent();
}

/**
 * Select previous station on button longclick
 */
void prevStation()
{
  if (currentStation == 0) currentStation = nbrRadiostations;
  currentStation--;
  Serial.println(station[currentStation].name);
  startPlaying();
  showCurrent();
}

/**
 * Print name and url of current station
 */
void showCurrent() 
{
  Serial.printf_P(PSTR("Current Station: %s --> %s\n"), station[currentStation].name, station[currentStation].url);
};

/**
 * Returns true, as soon as msWait milliseconds have passed.
 * Supply a reference to an unsigned long variable to hold 
 * the previous milliseconds.
 */
bool waitIsOver(uint32_t &msPrevious, uint32_t msWait)
{
  return (millis() - msPrevious >= msWait) ? (msPrevious = millis(), true) : false;
}

/**
 * Play the stream or retry on failure
 */
void playStream(bool verbose)
{
  static int retries = 0;
  static uint32_t msPrevious = millis();
  static uint32_t msPrevious1 = millis();
  if (!decoder->loop()) 
  {
    Serial.printf_P(PSTR("Stopping decoder\n"));
    stopPlaying();
    if (waitIsOver(msPrevious, 2000)) 
    {
      if (retries < 2) 
      {
        retries++;
        Serial.printf_P(PSTR("Retrie playing... %d\n"), retries);
        startPlaying();
      }
      else
      {
        retries = 0;
        Serial.printf_P(PSTR("Giving up, try next station... %d\n"), retries);
        nextStation(); // In case of error try the next station
      } 
    }
  }
  else
  {
    if (verbose && waitIsOver(msPrevious1, 5000)) Serial.printf_P(PSTR("Playing\n"));
  }
}

    /**
     * Use a raw string literal to print a formatted
     * string of WiFi connection details
     */
    void printConnectionDetails()
    {
      Serial.printf(R"(
Connection Details:
------------------
  SSID       : %s
  Hostname   : %s
  IP-Address : %s
  MAC-Address: %s
  RSSI       : %d (received signal strength indicator)
  )", WiFi.SSID().c_str(),
      WiFi.hostname().c_str(),  // ESP8266
      // WiFi.getHostname(),    // ESP32 
      WiFi.localIP().toString().c_str(),
      WiFi.macAddress().c_str(),
      WiFi.RSSI());  
    }

/**
 * Initialize the stream, buffer and decoder
 */
void initStream()
{
  Serial.println(F("DEBUG: initStream() starting"));
  file = new AudioFileSourceICYStream(station[currentStation].url);
  if (file == NULL) {
    Serial.println(F("FATAL: new AudioFileSourceICYStream() failed!"));
    while(true) { delay(1000); ESP.wdtFeed(); }
  }
  Serial.println(F("DEBUG: AudioFileSourceICYStream object created."));
  //file = new AudioFileSourceHTTPStream(station[currentStation].url);
  file->RegisterMetadataCB(cbMetaData, (void *)"ICY");

  buff = new AudioFileSourceBuffer(file, preallocateBuffer, preallocateBufferSize);
  if (buff == NULL) {
    Serial.println(F("FATAL: new AudioFileSourceBuffer() failed!"));
    while(true) { delay(1000); ESP.wdtFeed(); }
  }
  Serial.println(F("DEBUG: AudioFileSourceBuffer object created."));
  buff->RegisterStatusCB(cbStatus, (void *)"buffer");

  decoder = new AudioGeneratorMP3(preallocateCodec, preallocateCodecSize);
  if (decoder == NULL) {
    Serial.println(F("FATAL: new AudioGeneratorMP3() failed!"));
    while(true) { delay(1000); ESP.wdtFeed(); }
  }
  Serial.println(F("DEBUG: AudioGeneratorMP3 object created."));
  decoder->RegisterStatusCB(cbStatus, (void *)"mp3");
  decoder->begin(buff, out);
  Serial.println(F("DEBUG: decoder->begin() called"));
  Serial.println(F("DEBUG: initStream() completed"));
}

/**
 * Establish the WiFi connection
 */
void initWiFi()
{
  Serial.printf_P(PSTR("DEBUG: Free heap before WiFi.begin(): %u\n"), ESP.getFreeHeap());
  Serial.printf_P(PSTR("DEBUG: Attempting to connect to SSID: %s\n"), ssid);
  Serial.println("Connecting to WiFi");
  WiFi.begin(ssid, password);

  // Try forever
  while (WiFi.status() != WL_CONNECTED) {
    Serial.printf_P(PSTR("DEBUG: WiFi status: %d\n"), WiFi.status());
    Serial.println("...Connecting to WiFi");
    delay(1000);
  }
  Serial.println("Connected");
  printConnectionDetails();
}

/**
 * Initialize audio output and start playing
 */
void initAudio()
{
  Serial.println(F("DEBUG: initAudio_internal starting"));
  audioLogger = &Serial;
  #ifdef EXTERNAL_DAC
    out = new AudioOutputI2S();  // with external MAX98357 DAC/amplifier
    if (out == NULL) { 
      Serial.println(F("FATAL: new AudioOutput object failed!")); 
      while(true) { delay(1000); ESP.wdtFeed(); } 
    }
    Serial.println(F("DEBUG: AudioOutput object created."));
  #else
    out = new AudioOutputI2SNoDAC();
    if (out == NULL) { 
      Serial.println(F("FATAL: new AudioOutput object failed!")); 
      while(true) { delay(1000); ESP.wdtFeed(); } 
    }
    Serial.println(F("DEBUG: AudioOutput object created."));
  #endif
  Serial.println(F("DEBUG: initAudio_internal calling startPlaying()"));
  startPlaying();
  Serial.println(F("DEBUG: initAudio_internal completed"));
}

/**
 * First, preallocate all the memory needed for the buffering and codecs, never to be freed
 */
void initBuffers()
{
  Serial.println(F("DEBUG: initBuffers_internal starting"));
  preallocateBuffer = malloc(preallocateBufferSize);
  preallocateCodec  = malloc(preallocateCodecSize);
  if (!preallocateBuffer || !preallocateCodec) 
  {
    Serial.printf_P(PSTR("FATAL ERROR:  Unable to preallocate %d bytes for app\n"), preallocateBufferSize+preallocateCodecSize);
    while (true) { delay(1000); ESP.wdtFeed(); } // Infinite halt
  }
  Serial.println(F("DEBUG: initBuffers_internal completed"));
}

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println(F("DEBUG: Setup starting")); // F() macro for PROGMEM strings

  Serial.println(F("DEBUG: Initializing LittleFS..."));
  if (!LittleFS.begin()) {
    Serial.println(F("FATAL: LittleFS.begin() failed!"));
    while (true) { delay(1000); ESP.wdtFeed(); } // Halt with WDT feed
  }
  Serial.println(F("DEBUG: LittleFS initialized successfully."));

  // add callbacks to pushbutton
  button.addOnClickCB(nextStation);
  button.addOnLongClickCB(prevStation);
  button.addOnDoubleClickCB(showCurrent);

  Serial.println(F("DEBUG: initBuffers() starting"));
  initBuffers();
  Serial.println(F("DEBUG: initBuffers() completed"));
  Serial.println(F("DEBUG: initWiFi() starting"));
  initWiFi();
  Serial.println(F("DEBUG: initWiFi() completed"));
  Serial.println(F("DEBUG: initAudio() starting"));
  initAudio();
  Serial.println(F("DEBUG: initAudio() completed"));
  Serial.println(F("DEBUG: Setup completed"));
}

void loop()
{
  if (decoder && decoder->isRunning()) 
  {
    playStream(false);
  }
  button.loop();
}