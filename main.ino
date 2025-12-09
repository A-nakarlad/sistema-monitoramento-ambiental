#include <BH1750.h>
#include <Wire.h>
#include "DHT.h"

#define DHTPIN 19
#define DHTTYPE DHT11 

#define readingInterval 10000
#define alertTime 120000
#define maximumAlertTime 180000
#define buzzerInterval 30000
#define silenceDuration 600000

#define tempMin 22.0
#define tempMax 26.0
#define humidityMin 40.0
#define humidityMax 60.0

BH1750 lightMeter;
DHT dht(DHTPIN, DHTTYPE);

//leds
const int redLedPin = 15;
const int greenLedPin = 4;
const int yellowLedPin = 2;

const int buzzerPin = 18;
const int buttonPin = 23;

//timers
unsigned long lastTime = 0;
int counter = 0;

//noite = false, dia = true
bool stateDay = false; 

//BH variables
unsigned long startOutsideRangeBH = 0;
float sumBH = 0.0;
float averageBH = 0.0;
bool onAlertBH = false;
bool onMaximumAlertBH = false;

//DHT variables
float sumHum = 0.0;
float averageHum = 0.0;
int consecutiveCounterHum = 0;
bool onAlertHum = false;
bool onMaximumAlertHum = false;
unsigned long startOutsideRangeHum = 0;

float sumTemp = 0.0;
float averageTemp = 0.0;
int consecutiveCounterTemp = 0;
bool onAlertTemp = false;
bool onMaximumAlertTemp = false;
unsigned long startOutsideRangeTemp = 0;

bool redAlert = true;

unsigned long lastBuzzerTime = 0;     
unsigned long silenceStartTime = 0;    
bool buzzerSilenced = false;           
bool buzzerOn = false;                
unsigned long buzzerOnStartTime = 0; 
const int buzzerBeepDuration = 200;  

int buttonState = HIGH;
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;


void defineInitialStateDay () {

  float lux = 0.0;
  unsigned long init_lastTime = millis();
  int init_counter = 0;

  while (init_counter < 6) {
    if (millis() - init_lastTime  >= readingInterval) {
      init_lastTime += readingInterval;

      lux = lightMeter.readLightLevel();
      sumBH += lux;
      init_counter++;
    }
  }

  averageBH = sumBH / 6.0;
  stateDay = (averageBH >= 1.0);

  Serial.print("média inicial: ");
  Serial.println(averageBH);
  Serial.print("estado do dia: ");
  Serial.println(stateDay ? "dia" : "noite");

  sumBH = 0.0;
  init_lastTime = millis();
  init_counter = 0;
  
  while(true){
    if (millis() - init_lastTime >= readingInterval) {
      init_lastTime += readingInterval; 
      init_counter++;

      lux = lightMeter.readLightLevel(); 
      sumBH += lux; 

      if (init_counter >= 6 ) {
        averageBH = sumBH / 6.0;
        init_counter = 0;
        sumBH = 0.0;

        Serial.println("media em 1min ");
        Serial.print(averageBH);
        Serial.println(" lx");

        if (averageBH < 1) {
          if (stateDay) {
            stateDay = false;
            break;
          } 
        }else if (averageBH >= 1) {
          if (!stateDay) {
            stateDay = true;
            break;
          } 
        }
      }
    }    
  }
}

void setup() {
  Serial.begin(9600);

  Wire.begin();
  lightMeter.begin();
  dht.begin();

  defineInitialStateDay();

  pinMode (redLedPin, OUTPUT);
  pinMode (greenLedPin, OUTPUT);
  pinMode (yellowLedPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  lastTime = millis();
  sumBH = 0.0;

  digitalWrite(buzzerPin, LOW);
}

void checkHumidity (float humidity) { 
  bool withinRange = (humidity >= humidityMin && humidity <= humidityMax);

  if (!onAlertHum) {
    if (!withinRange){
      consecutiveCounterHum++;

      if (startOutsideRangeHum == 0) {
        startOutsideRangeHum = millis();
      } else if (millis() - startOutsideRangeHum >= maximumAlertTime ) {
        Serial.println("alerta - mais de 3min");
        onMaximumAlertHum = true;
      } else if (millis() - startOutsideRangeHum >= alertTime ) {
        Serial.println("alerta - mais de 2min");
        onAlertHum = true;
      } 

      if (consecutiveCounterHum >= 3) {
        //onAlertHum = true;
        Serial.println("alerta hum");
      }
    } else {
      consecutiveCounterHum = 0;
      consecutiveCounterHum = 0;
    }
  } else {
    if (withinRange) {
      consecutiveCounterHum++;

        if (consecutiveCounterHum >= 3) {
        onAlertHum = false;
        consecutiveCounterHum = 0;
        startOutsideRangeTemp = 0;
        Serial.println("hum ok");
      }
    } else {
      consecutiveCounterHum = 0;
    }    
  }
}

void checkTemperature (float temperature) { 
  bool withinRange = (temperature >= tempMin && temperature <= tempMax);

  if (!onAlertTemp) {
    if (!withinRange){
      consecutiveCounterTemp++;

      if (startOutsideRangeTemp == 0) {
        startOutsideRangeTemp = millis();
      } else if (millis() - startOutsideRangeTemp >= maximumAlertTime) {
        Serial.println("alerta - mais de 2min");
        onMaximumAlertTemp = true;
      } else if (millis() - startOutsideRangeTemp >= alertTime ) {
        Serial.println("alerta - mais de 2min");
        onAlertTemp = true;
      } 

      if (consecutiveCounterTemp >= 3) {
        //onAlertTemp = true;
        Serial.println("alerta temp");
      }
    } else {
      consecutiveCounterTemp = 0;
      startOutsideRangeTemp = 0;
    }
  } else {
    if (withinRange) {
      consecutiveCounterTemp++;

        if (consecutiveCounterTemp >= 3) {
        onAlertTemp = false;
        consecutiveCounterTemp = 0;
        startOutsideRangeTemp = 0;
        Serial.println("temp ok");
      }
    } else {
      consecutiveCounterTemp = 0;
    }    
  }
}

void checkBH (float lux) {
  if (!stateDay){
    if (lux >= 1 && lux <= 3) {
      if (startOutsideRangeBH == 0) {
        startOutsideRangeBH = millis();
      } else if (!onAlertBH && (millis() - startOutsideRangeBH >= maximumAlertTime)) {
        Serial.println("alerta - mais de 2min");
        onMaximumAlertBH = true;
      } else if (millis() - startOutsideRangeBH >= alertTime ) {
        Serial.println("alerta - mais de 2min");
        onAlertBH = true;
      } 
    } else {
      if (onAlertBH) {
        onAlertBH = false;
      }
      startOutsideRangeBH = 0;
    }
  } else {
    if (onAlertBH) {
      onAlertBH = false;
      startOutsideRangeBH = 0;
    }
  } 
}


void activateAlerts () {
  bool yellowAlert = (onAlertBH || (onAlertTemp || onAlertHum));
  bool maximumAlert = (onMaximumAlertBH || (onMaximumAlertTemp || onMaximumAlertHum));
  int alerts = 0;

  if (onAlertBH) alerts++;
  if (onAlertHum) alerts++;
  if (onAlertTemp) alerts++;

  if (alerts >= 2 || maximumAlert) {
    digitalWrite (yellowLedPin, LOW);
    digitalWrite (redLedPin, HIGH);
    digitalWrite (greenLedPin, LOW);
    redAlert = true;
    Serial.println(redAlert);
  } else if (yellowAlert) {
    digitalWrite (yellowLedPin, HIGH);
    digitalWrite (redLedPin, LOW);
    digitalWrite (greenLedPin, LOW);
    redAlert = false;
  } else {
    digitalWrite (yellowLedPin, LOW);
    digitalWrite (redLedPin, LOW);
    digitalWrite (greenLedPin, HIGH);
    redAlert = false;
  }
}

void handleBuzzer () {
  if (!redAlert) {
    digitalWrite(buzzerPin, LOW);
    buzzerSilenced = false;
    return;
  } else if (buzzerSilenced) {
    if (millis() - silenceStartTime >= silenceDuration) {
      buzzerSilenced = false;
    } else {
      digitalWrite(buzzerPin, LOW);
      return;
    }
  }

  unsigned long currentTime = millis();
  if (currentTime - lastBuzzerTime >= buzzerInterval) {
    lastBuzzerTime = currentTime;
    buzzerOn = true;
    buzzerOnStartTime = currentTime;
    digitalWrite(buzzerPin, HIGH);
  }

  if (buzzerOn && (currentTime - buzzerOnStartTime >= buzzerBeepDuration)) {
    buzzerOn = false;
    digitalWrite(buzzerPin, LOW);
  }
}

void handleButton () {
  int reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      
      if (buttonState == LOW) {
        if (redAlert) {
          buzzerSilenced = true;
          silenceStartTime = millis();
          digitalWrite(buzzerPin, LOW);
          buzzerOn = false;
        }
      }
    }
  }
  lastButtonState = reading;
}

void loop() {

  handleButton();

  if (millis() - lastTime >= readingInterval) {
    lastTime += readingInterval; 
    counter++;

    float lux = lightMeter.readLightLevel();
    float humidity = dht.readHumidity();
    float temp = dht.readTemperature();

    sumBH += lux;
    sumHum += humidity;
    sumTemp += temp;
   
    checkBH(lux);
    checkHumidity(humidity);
    checkTemperature(temp);

    Serial.print("leitura ");
    Serial.println(counter);
    Serial.print(lux);
    Serial.println(" lx");
    Serial.print(temp);
    Serial.println(" °c");
    Serial.print(humidity);
    Serial.println(" %");  

    if (counter >= 6 ) {
      averageBH = sumBH / 6.0;
      averageHum = sumHum / 6.0;
      averageTemp = sumTemp / 6.0;

      counter = 0;
      sumBH = 0.0;
      sumHum = 0.0;
      sumTemp = 0.0;

      Serial.println("media em 1min ");
      Serial.print(averageBH);
      Serial.println(" lx");
      Serial.print(averageHum);
      Serial.println(" %");
      Serial.print(averageTemp);
      Serial.println(" °c");
    }

    activateAlerts();
  }    

  handleBuzzer();
}