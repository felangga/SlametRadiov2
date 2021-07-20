#include <WiFi.h>
#include <SI4735.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include "patch_init.h"
#include "Rotary.h"


#define ENCODER_PIN_A 33
#define ENCODER_PIN_B 32
#define ESP32_I2C_SDA 21
#define ESP32_I2C_SCL 22
#define RESET_PIN 12

#define RXD2 16
#define TXD2 17

const uint16_t size_content = sizeof ssb_patch_content;

// Urusan Encoder
Rotary encoder = Rotary(ENCODER_PIN_A, ENCODER_PIN_B);
volatile int encoderCount = 0;

const char* wifiName = "CHIKO";
const char* wifiPass = "chikocik";
const char *host = "https://shortwave.nasihosting.com/api.php?page=";
const char *bandwidth[] = {"6", "4", "3", "2", "1", "1.8", "2.5"};

uint8_t bwIdxSSB = 2;
const char *bandwitdthSSB[] = {"1.2", "2.2", "3.0", "4.0", "0.5", "1.0"};

String station_name[6];
String rawMsg;
int station_freq[6];
int hour, minute;
int currentBFO = 0;
int step = 1;
bool bfoOn = false;
bool inetConnected = false;
uint8_t currentStep = 1;
uint8_t currentBFOStep = 25;

boolean fast = false;
bool isFM = false;
bool isSSB = false;
bool newUpdate = false;
bool freqBerubah = true;
bool ssbLoaded = false;
long elapsedRSSI = millis();
long refreshStations = millis();
uint8_t rssi = 0;
uint16_t halaman = 0;
uint16_t timezoneOffset = 7;
uint16_t freq = 10000;
uint8_t bandwidthIdx = 0;
unsigned long waktu;
unsigned long waktuTerakhir;

// URUSAN RDS
char *rdsMsg;
char *stationName;
char *rdsTime;
char bufferStatioName[255];
char bufferRdsMsg[255];
char bufferRdsTime[32];


WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "id.pool.ntp.org", 0);
SI4735 si4735;

void showRDSStation()
{
  if (String(stationName).length() > 0)
    Serial2.print("tRDStat.txt=\"" + String(stationName) + "\""); endNextion();


  delay(250);
}

void showRDSTime()
{
  if (String(rdsTime).length() > 0)
    Serial2.print("tRDSTime.txt=\"" + String(rdsTime) + "\""); endNextion();

  delay(250);
}

void checkRDS()
{

  si4735.getRdsStatus();
  if (si4735.getRdsReceived())
  {
    if (si4735.getRdsSync() && si4735.getRdsSyncFound())
    {
      stationName = si4735.getRdsText0A();
      rdsTime = si4735.getRdsTime();

      if (stationName != NULL)
        showRDSStation();
      if (rdsTime != NULL)
        showRDSTime();
    }
  }
}


void loadSSB()
{
  si4735.setI2CFastModeCustom(500000);
  si4735.queryLibraryId(); // Is it really necessary here? I will check it.
  si4735.patchPowerUp();
  delay(50);
  si4735.downloadPatch(ssb_patch_content, size_content);

  // Parameters
  // AUDIOBW - SSB Audio bandwidth; 0 = 1.2kHz (default); 1=2.2kHz; 2=3kHz; 3=4kHz; 4=500Hz; 5=1kHz;
  // SBCUTFLT SSB - side band cutoff filter for band passand low pass filter ( 0 or 1)
  // AVC_DIVIDER  - set 0 for SSB mode; set 3 for SYNC mode.
  // AVCEN - SSB Automatic Volume Control (AVC) enable; 0=disable; 1=enable (default).
  // SMUTESEL - SSB Soft-mute Based on RSSI or SNR (0 or 1).
  // DSP_AFCDIS - DSP AFC Disable or enable; 0=SYNC MODE, AFC enable; 1=SSB MODE, AFC disable.
  si4735.setSSBConfig(bandwidthIdx, 1, 0, 1, 0, 1);
  si4735.setI2CFastModeCustom(100000);
  ssbLoaded = true;
}

void setup() {
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial.begin(115200);
  Serial.setTimeout(100);
  Serial2.setTimeout(100);

  Wire.begin(ESP32_I2C_SDA, ESP32_I2C_SCL);
  si4735.setup(RESET_PIN, 1);
  //  si4735.setFM(8400, 10800, 9420, 10);
  si4735.setAM(10000, 150, 30000, 10);
  delay(500);
  freq = si4735.getFrequency();
  freqBerubah = true;
  si4735.setVolume(63);
  si4735.setBandwidth(0, 1);
  si4735.setTuneFrequencyAntennaCapacitor(1);

  Serial2.print("rest"); endNextion();



  // Encoder pins
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);

  xTaskCreate(
    inetTask,    // Function that should be called
    "Encoder Task",   // Name of the task (for debugging)
    8192,            // Stack size (bytes)
    NULL,            // Parameter to pass
    1,               // Task priority
    NULL             // Task handle
  );

}

void loadStations() {

  if (!inetConnected) return;

  Serial2.print("vis loading,1"); endNextion();
  HTTPClient http;
  http.begin(host + String(halaman));

  int httpCode = http.GET();
  String payload = http.getString();

  if (httpCode == 200) {
    StaticJsonDocument<768> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      Serial2.print("vis loading,0"); endNextion();
      return;
    }

    memset(station_name, 0, sizeof(station_name));
    memset(station_freq, 0, sizeof(station_freq));

    int idx = 0;
    for (JsonObject elem : doc.as<JsonArray>()) {

      const int freq = elem["freq"]; // "1026", "1035", "1062", "1062", "1062", "1071"
      //  const char* jam = elem["jam"]; // "0000-2400", "1000-1700", "0000-2400", "0000-2400", "0000-2400", ...
      const char* name = elem["name"]; // "PM7CKB R.Suara Enim Jaya", "RRI Pro-1", "RKPDT2", "R.Swara Lembah ...

      station_name[idx] = name;
      station_freq[idx] = freq;
      idx++;
    }
    newUpdate = true;
  }
  Serial2.print("vis loading,0"); endNextion();
  http.end();
}

void endNextion() {
  Serial2.write(0xff); Serial2.write(0xff); Serial2.write(0xff);
}

void setFreq(int newFreq) {
  if (newFreq < 150) return;
  freq = newFreq;
  si4735.setFrequency(freq);
  freqBerubah = true;
  Serial.println(freq);
}

void inetTask( void * pvParameters ) {

  Serial.print("Connecting to ");
  Serial.println(wifiName);

  WiFi.begin(wifiName, wifiPass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  inetConnected = true;
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  timeClient.begin();
  loadStations();

  for (;;) {



    if (inetConnected) {
      // Get Time
      timeClient.update();
      hour = timeClient.getHours();
      minute = timeClient.getMinutes();

      // refresh stations every 5 minutes
      if ((millis() - refreshStations) > 1000 * 60 * 10) {
        halaman = 0;
        loadStations();
        refreshStations = millis();
      }
    }

  }
}

void loop() {
  if (millis() - waktuTerakhir > 700) fast = false;
  unsigned char result = encoder.process();
  if (result) {

    if (result == DIR_CW)
    {
      encoderCount = 1;
    }
    else
    {
      encoderCount = -1;
    }


    if (encoderCount != 0)
    {

      if (bfoOn)
      {
        currentBFO = (encoderCount == 1) ? (currentBFO + currentBFOStep) : (currentBFO - currentBFOStep);
        si4735.setSSBBfo(currentBFO);
        Serial2.print("btnBFO.txt=\"BFO: " + String(currentBFO) + " KHz\""); endNextion();
      }
      else
      {
        if (millis() - waktuTerakhir < 80) {
          fast = true;
        }

        if (fast) {
          step = 10;

        } else {
          step = 1;

        }
        if (encoderCount == -1) {
          freq += step;
        } else {
          freq -= step;
        }
        setFreq(freq);
      }

      waktuTerakhir = millis();
      encoderCount = 0;
    }

  }
  if (Serial2.available()) {
    String rawMsg = Serial2.readStringUntil('\n');
    int potong = rawMsg.indexOf("|");
    String command = rawMsg.substring(1, potong);
    String params = rawMsg.substring(potong + 1, rawMsg.length() - 3);

    if (command == "nextPage") {
      halaman++;
      loadStations();
    } else if (command == "prevPage") {
      if (halaman > 0) {
        halaman --;
        loadStations();
      }
    } else if (command == "mem1") {
      if (isFM) si4735.setAM(10000, 150, 30000, 10);
      setFreq(station_freq[0]);
    } else if (command == "mem2") {
      if (isFM) si4735.setAM(10000, 150, 30000, 10);
      setFreq(station_freq[1]);
    } else if (command == "mem3") {
      if (isFM) si4735.setAM(10000, 150, 30000, 10);
      setFreq(station_freq[2]);
    } else if (command == "mem4") {
      if (isFM) si4735.setAM(10000, 150, 30000, 10);
      setFreq(station_freq[3]);
    } else if (command == "mem5") {
      if (isFM) si4735.setAM(10000, 150, 30000, 10);
      setFreq(station_freq[4]);
    } else if (command == "freq") {

      setFreq(params.toInt());
      newUpdate = true;
      freqBerubah = true;
    } else if (command == "refresh") {
      newUpdate = true;
    } else if (command == "bw") {
      if (!isFM) {
        if (isSSB)
        {
          bwIdxSSB++;
          if (bwIdxSSB > 5)
            bwIdxSSB = 0;
          si4735.setSSBAudioBandwidth(bwIdxSSB);
          // If audio bandwidth selected is about 2 kHz or below, it is recommended to set Sideband Cutoff Filter to 0.
          if (bwIdxSSB == 0 || bwIdxSSB == 4 || bwIdxSSB == 5)
            si4735.setSBBSidebandCutoffFilter(0);
          else
            si4735.setSBBSidebandCutoffFilter(1);

          Serial2.print("b0.txt=\"BW: " + String(bandwitdthSSB[bwIdxSSB]) + " KHz\""); endNextion();
        } else {
          bandwidthIdx++;
          if (bandwidthIdx > 6)
            bandwidthIdx = 0;
          si4735.setBandwidth(bandwidthIdx, 1);
          Serial2.print("b0.txt=\"BW: " + String(bandwidth[bandwidthIdx]) + " KHz\""); endNextion();
        }

      }

    } else if (command == "agc") {
      si4735.setAutomaticGainControl((params == "on") ? true : false, 1);
    } else if (command == "FM") {
      isFM = true;
      si4735.setTuneFrequencyAntennaCapacitor(0);
      si4735.setFM(8600, 10800, 10020, 10);
      si4735.setRdsConfig(1, 2, 2, 2, 2);
      Serial2.print("satuan.txt=\"MHz\""); endNextion();
      ssbLoaded = false;
      freq  = si4735.getFrequency();
      freqBerubah = true;
    } else if (command == "AM") {
      isFM = false;
      isSSB = false;
      si4735.setAM(freq, 150, 30000, 10);
      freq  = si4735.getFrequency();
      freqBerubah = true;
      Serial2.print("satuan.txt=\"KHz\""); endNextion();
      ssbLoaded = false;
      Serial2.print("b0.txt=\"BW: " + String(bandwidth[bandwidthIdx]) + " KHz\""); endNextion();
    } else if (command == "LSB") {
      if (!ssbLoaded) loadSSB();
      isFM = false;
      isSSB = true;
      si4735.setTuneFrequencyAntennaCapacitor(1);
      si4735.setSSB(2000, 30000, freq, 1, 1);
      si4735.setSSBAutomaticVolumeControl(1);
      si4735.setSsbSoftMuteMaxAttenuation(0);

      Serial2.print("b0.txt=\"BW: " + String(bandwitdthSSB[bwIdxSSB]) + " KHz\""); endNextion();
    } else if (command == "USB") {
      isSSB = true;
      if (!ssbLoaded) loadSSB();
      isFM = false;
      si4735.setTuneFrequencyAntennaCapacitor(1);
      si4735.setSSB(2000, 30000, freq, 1, 2);
      si4735.setSSBAutomaticVolumeControl(1);
      si4735.setSsbSoftMuteMaxAttenuation(0);

      Serial2.print("b0.txt=\"BW: " + String(bandwitdthSSB[bwIdxSSB]) + " KHz\""); endNextion();
    } else if (command == "BFO") {
      bfoOn = (params == "1");
    }

    rawMsg = "";
  }

  if (newUpdate) {
    for (int i = 0; i < 6; i++) {
      if (station_name[i].length() > 0) {
        Serial2.print("mem" + String(i + 1) + ".txt=\"" + String(station_freq[i]) + " KHz - " + station_name[i] + "\""); endNextion();
      } else {
        Serial2.print("mem" + String(i + 1) + ".txt=\"-\""); endNextion();
      }
    }
    newUpdate = false;
  }

  // refresh signal gauge
  if ((millis() - elapsedRSSI) > 500)
  {
    si4735.getCurrentReceivedSignalQuality();
    int aux = si4735.getCurrentRSSI();

    if (rssi != aux)
    {
      rssi = aux;
      Serial2.print("signal=" + String(map(rssi, 0, 127, 0, 28))); endNextion();
      Serial2.print("tUTC.txt=\"" + String(hour) + ":" + String(minute) + " UTC\""); endNextion();
      Serial2.print("tLocal.txt=\"" + String((hour + timezoneOffset) % 24) + ":" + String(minute) + " LOCAL\""); endNextion();
      Serial2.print("btnAGC.val=" + si4735.isAgcEnabled() ? 1 : 0); endNextion();
      if (si4735.getCurrentPilot()) {
        Serial2.print("tRDS.txt=\"STEREO\""); endNextion();
      } else {
        Serial2.print("tRDS.txt=\"MONO\""); endNextion();
      }
    }
    elapsedRSSI = millis();
  }

  if (isFM) {
    checkRDS();
  }

  if (freqBerubah) {
    Serial2.print("tFreq.txt=\"" + String(freq) + "\""); endNextion();
    freqBerubah = false;
  }

}
