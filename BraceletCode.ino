// Library
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <ESP8266HTTPClient.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>

// VARIABLES
Ticker flipper;                             // Declare an object of class Ticker
TinyGPSPlus gps;                            // Declare an object of class TinyGPSPlus
HTTPClient http;                            // Declare an object of class HTTPClient
SoftwareSerial gpsSerial(4, 5);             // GPS port connection
double longitude;                           // holds the longitude data read by GPS
double latitude;                            // holds the latitude data read by GPS
const int maxAvgSample = 10;                // Number ot the IBI values
volatile int rate[maxAvgSample];            // used to hold last ten IBI values
boolean sendok = false;                     // true when read a heartbeat
volatile unsigned long sampleCounter = 0;   // used to determine pulse timing
volatile unsigned long lastBeatTime = 0;    // used to find the inter beat interval
volatile int P = 512;                       // used to find peak in pulse wave
volatile int T = 512;                       // used to find trough in pulse wave
volatile int thresh = 600;                  // used to find instant moment of heart beat
volatile int amp = 100;                     // used to hold amplitude of pulse waveform
volatile boolean firstBeat = true;          // used to seed rate array so we startup with reasonable BPM
volatile boolean secondBeat = true;         // used to seed rate array so we startup with reasonable BPM
volatile int BPM;                           // used to hold the pulse rate
volatile int Signal;                        // holds the incoming raw data
volatile int IBI = 600;                     // holds the time between beats, the Inter-Beat Interval
volatile boolean Pulse = false;             // true when pulse wave is high, false when it's low
volatile boolean QS = false;                // true when sensor finds a heartbeat
int count = 0;
int serialNumber = 198123;
const char* username = "iPhone (2)";        // wifi login user name
const char* password = "abc12345";          // wifi login password


void setup() {
  Serial.begin(9600);                       // Start  serial
  delay(10);

  Serial.print("Connecting to ");           // Start connect to WiFi network
  Serial.println(username);
  WiFi.begin(username, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");


  gpsSerial.begin(9600);                    // Start GPS serial
  flipper.attach_ms(2, collectheartbeat);   // sets up to read Pulse Sensor signal every 2mS
}



void loop() {
  if (sendok) {                             // to check if the collectheartbeat() function have been applied
    SendGPS();                              // Start sending the GPS data

    if (BPM < 60) {                         // A Heartbeat Was Found
      // BPM and IBI have been Determined
      sendDataToSerial(BPM);                // A Beat Happened, Output that to serial.
      QS = false;                           // reset the Quantified Self flag for next time
    }
    if ( BPM > 60 && BPM < 120 )  {         // A Heartbeat Was Found
      // BPM and IBI have been Determined
      sendDataToSerial(BPM);                // A Beat Happened, Output that to serial.
      QS = false;                           // reset the Quantified Self flag for next time
    }
    if ( BPM > 120 && BPM < 140 )  {        // A Heartbeat Was Found
      // BPM and IBI have been Determined
      sendDataToSerial(BPM);                // A Beat Happened, Output that to serial.
      QS = false;                           // reset the Quantified Self flag for next time
    }

    delay(10);
    sendok = false;                         // used to change the flag state after send
    flipper.attach_ms(2, collectheartbeat); // sets up to read Pulse Sensor signal every 2mS
  }
}



void sendDataToSerial(int BPM) {
  http.begin("http://us-central1-guard-project-b47ec.cloudfunctions.net/addData");  //Specify destination for HTTP request
  http.addHeader("Content-Type", "application/json");  //Specify content-type header

  // used to analyse the heartbeat state
  if ( BPM > 120 && BPM < 140) {
    state = "Abnormal";
  } else if ( BPM > 60 && BPM < 120) {
    state = "Normal";
  } else if (BPM < 60) {
    BPM = 0;
    state = "NoHeartRate";
  }

  Serial.println(BPM);                          //send the BPM to serial
  Serial.println(state);                        //send the state to serial
  String postBody = "{\"serialNumber\":" + String(serialNumber) + ",\"heartBeat\":" + String(BPM) + ",\"state\":\" " + state + " \"}"; //Specify content
  int httpResponseCode = http.POST(postBody);   //Send the actual POST request
  if (httpResponseCode > 0) {                   //Check the returning code
    String payload = http.getString();          //Get the request response payload
    Serial.println(payload);                    // Output payload serial.
  } else {
    Serial.println("Something went wrong");
  }  http.end();                                //Close connection
}



int count = 0;
void collectheartbeat() {                      // Used to start collect valid  heartbeats
  count++;
  if (count == 1000) {                         // Start set every second.
    flipper.detach();                          // to detach (interrupt) every 1 second.
    count = 0;
    sendok = true;                             // change the flag state to allow it to send
  }

  Signal = analogRead(A0);                    // read the Pulse Sensor
  sampleCounter += 2;                         // keep track of the time in mS with this variable
  int N = sampleCounter - lastBeatTime;       // monitor the time since the last beat to avoid noise

  //  find the peak and trough of the pulse wave
  if (Signal < thresh && N > (IBI / 5) * 3) { // avoid dichrotic noise by waiting 3/5 of last IBI
    if (Signal < T) {                         // T is the trough
      T = Signal;                             // keep track of lowest point in pulse wave
    }
  }

  if (Signal > thresh && Signal > P) {        // thresh condition helps avoid noise
    P = Signal;                               // P is the peak
  }                                           // keep track of highest point in pulse wave

  // signal surges up in value every time there is a pulse
  if (N > 250) {                              // avoid high frequency noise
    if ( (Signal > thresh) && (Pulse == false) && (N > (IBI / 5) * 3) ) {
      Pulse = true;                            // set the Pulse flag when we think there is a pulse
      IBI = sampleCounter - lastBeatTime;      // measure time between beats in mS
      lastBeatTime = sampleCounter;            // keep track of time for next pulse

      if (firstBeat) {                         // if it's the first time we found a beat, if firstBeat == TRUE
        firstBeat = false;                     // clear firstBeat flag
        return;                                // IBI value is unreliable so discard it
      }
      if (secondBeat) {                        // if this is the second beat, if secondBeat == TRUE
        secondBeat = false;                    // clear secondBeat flag
        for (int i = 0; i <= maxAvgSample - 1; i++) { // seed the running total to get a realisitic BPM at startup
          rate[i] = IBI;
        }
      }

      // keep a running total of the last 10 IBI values
      word runningTotal = 0;                   // clear the runningTotal variable

      for (int i = 0; i <= (maxAvgSample - 2); i++) {        // shift data in the rate array
        rate[i] = rate[i + 1];                 // and drop the oldest IBI value
        runningTotal += rate[i];               // add up the 9 oldest IBI values
      }

      rate[maxAvgSample - 1] = IBI;            // add the latest IBI to the rate array
      runningTotal += rate[maxAvgSample - 1];  // add the latest IBI to runningTotal
      runningTotal /= maxAvgSample;            // average the last 10 IBI values
      BPM = 60000 / runningTotal;              // how many beats can fit into a minute? that's BPM!
      QS = true;                               // set Quantified Self flag
    }
  }

  if (Signal < thresh && Pulse == true) {     // when the values are going down, the beat is over
    Pulse = false;                            // reset the Pulse flag so we can do it again
    amp = P - T;                              // get amplitude of the pulse wave
    thresh = amp / 2 + T;                     // set thresh at 50% of the amplitude
    P = thresh;                               // reset these for next time
    T = thresh;
  }

  if (N > 2500) {                             // if 2.5 seconds go by without a beat
    thresh = 512;                             // set thresh default
    P = 512;                                  // set P default
    T = 512;                                  // set T default
    lastBeatTime = sampleCounter;             // bring the lastBeatTime up to date
    firstBeat = true;                         // set these to avoid noise
    secondBeat = true;                        // when we get the heartbeat back
  }
}


void SendGPS() {
  http.begin("http://us-central1-guard-project-b47ec.cloudfunctions.net/addData");  //Specify destination for HTTP request
  http.addHeader("Content-Type", "application/json");    //Specify content-type header
  // Check if the GPS Coordinates avalible
  while (gpsSerial.available() > 0) {            // Read the Coordinates frome the GPS
    if (gps.encode(gpsSerial.read())) {
      Serial.println(F("Location: "));
      if (gps.location.isValid())
      {
        latitude = gps.location.lat();           // Save the latitude Coordinates
        Serial.println(gps.location.lat(), 6);   // Send the latitude Coordinates serial monitor
        Serial.print(F(","));
        longitude = gps.location.lng();          // Save the longitude Coordinates
        Serial.println(gps.location.lng(), 6);   // Send the longitude Coordinates serial monitor

        String postBody = "{\"serialNumber\":" + String(serialNumber) + ",\"latitude\":" + String(latitude, 6) + ",\"longitude\":" + String(longitude, 6) + "}";
        delay(500);                              // Send a request every 30 seconds

        int httpResponseCode = http.POST(postBody); // Send the actual POST request
        if (httpResponseCode > 0) {              // Check the returning code
          String payload = http.getString();     // Get the request response payload
          Serial.println(payload);               // Print the response payload
        } else {
          Serial.println("Something went wrong ");
        }

        http.end();                                //Close connection

      } else {
        Serial.print(F("Invalid \n"));
      }
      
    }
  }

}
