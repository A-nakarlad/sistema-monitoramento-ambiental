#include <BH1750.h>
#include <Wire.h>
#include "DHT.h"

// ============ DEFINIÇÕES DE PINOS ============
#define DHTPIN 19
#define DHTTYPE DHT11 
#define KY037Analog 34
#define KY037Digital 35

const int redLedPin = 15;
const int greenLedPin = 4;
const int yellowLedPin = 2;
const int buzzerPin = 18;
const int buttonPin = 23;

// ============ INTERVALOS E TEMPOS ============
#define readingInterval 10000        // 10s entre leituras
#define alertTime 50000              // 50s para alerta amarelo (ajustado de 2min)
#define maximumAlertTime 60000       // 60s para alerta vermelho (ajustado de 3min)
#define buzzerInterval 30000         // 30s entre toques do buzzer
#define silenceDuration 600000       // 10min sem buzzer após silenciar

// ============ FAIXAS DOS SENSORES ============
#define tempMin 22.0
#define tempMax 26.0
#define humidityMin 40.0
#define humidityMax 60.0
#define soundScoreThreshold 40       // Score médio máximo
#define soundPeakThreshold 60        // Threshold para picos

const unsigned long twelveHours = 12 * 60 * 60 * 1000UL;
const unsigned long twentyFourHours = 24UL * 60 * 60 * 1000UL;  

// ============ OBJETOS DOS SENSORES ============
BH1750 lightMeter;
DHT dht(DHTPIN, DHTTYPE);

// ============ CONTADORES E ESTADOS ============
unsigned long lastTime = 0;
int counter = 0;
unsigned long counterStateDay = 0;
bool stateDay = false; // false = noite, true = dia

// ============ VARIÁVEIS BH1750 (LUZ) ============
unsigned long startOutsideRangeBH = 0;
float sumBH = 0.0;
float averageBH = 0.0;
bool onAlertBH = false;
bool onMaximumAlertBH = false;

// ============ VARIÁVEIS DHT (UMIDADE) ============
float sumHum = 0.0;
float averageHum = 0.0;
int consecutiveCounterHum = 0;
bool onAlertHum = false;
bool onMaximumAlertHum = false;
unsigned long startOutsideRangeHum = 0;

// ============ VARIÁVEIS DHT (TEMPERATURA) ============
float sumTemp = 0.0;
float averageTemp = 0.0;
int consecutiveCounterTemp = 0;
bool onAlertTemp = false;
bool onMaximumAlertTemp = false;
unsigned long startOutsideRangeTemp = 0;

// ============ VARIÁVEIS KY-037 (SOM) ============
float sumSound = 0.0;
float averageSound = 0.0;
bool onAlertSound = false;
bool onMaximumAlertSound = false;
unsigned long startOutsideRangeSound = 0;
int consecutivePeaksSound = 0;
unsigned long startPeakSound = 0;

// ============ CONTROLE DE ALERTAS ============
bool redAlert = false;

// ============ CONTROLES DO BUZZER ============
unsigned long lastBuzzerTime = 0;
unsigned long silenceStartTime = 0;
bool buzzerSilenced = false;
bool buzzerOn = false;
unsigned long buzzerOnStartTime = 0;
const int buzzerBeepDuration = 200;

// ============ CONTROLES DO BOTÃO ============
int buttonState = HIGH;
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// ============ ESTRUTURA PARA ESTATÍSTICAS 24H ============
struct SensorStats {
  float minValue;
  float maxValue;
  float avgValue;
  float sumValue;
  int readingCount;
  unsigned long timeOutOfRange; // tempo fora da faixa em ms
};

SensorStats luxStats24h = {9999.0, -9999.0, 0.0, 0.0, 0, 0}; 
SensorStats tempStats24h = {9999.0, -9999.0, 0.0, 0.0, 0, 0};
SensorStats humStats24h = {9999.0, -9999.0, 0.0, 0.0, 0, 0};
SensorStats soundStats24h = {9999.0, -9999.0, 0.0, 0.0, 0, 0};

// ============ FUNÇÃO: LER SENSOR DE SOM KY-037 ============
float readSoundSensor() {
  const int N = 200;
  int minimo = 4095;
  int maximo = 0;

  // Mede pico-a-pico em N amostras
  for (int i = 0; i < N; i++) {
    int val = analogRead(KY037Analog);
    if (val > maximo) maximo = val;
    if (val < minimo) minimo = val;
    delayMicroseconds(100);
  }

  int vpp = maximo - minimo;
  
  // Converte VPP para score de 0-100
  // VPP típico: 0-1023, mapeamos para 0-100
  float score = map(vpp, 0, 4095, 0, 100);
  score = constrain(score, 0, 100);
  
  return score;
}

// ============ FUNÇÃO: VERIFICAR RUÍDO ============
void checkSound(float soundScore) {
  // Verifica picos (> 60 por > 3s)
  if (soundScore > soundPeakThreshold) {
    if (startPeakSound == 0) {
      startPeakSound = millis();
      consecutivePeaksSound = 1;
    } else if (millis() - startPeakSound >= 3000) {
      // Pico sustentado por 3s
      if (!onAlertSound) {
        onAlertSound = true;
      }
    }
  } else {
    startPeakSound = 0;
    consecutivePeaksSound = 0;
  }

  // Verifica média alta (> 40 por > 5min)
  if (!onAlertSound) {
    if (averageSound > soundScoreThreshold) {
      if (startOutsideRangeSound == 0) {
        startOutsideRangeSound = millis();
      } else if (millis() - startOutsideRangeSound >= 300000) { // 5 min
        onAlertSound = true;
      } else if (millis() - startOutsideRangeSound >= 240000) { // 4 min - pre-alerta
        onMaximumAlertSound = false;
      }
    } else {
      startOutsideRangeSound = 0;
    }
  } else {
    // Se está em alerta, verifica se voltou ao normal
    if (averageSound <= soundScoreThreshold && soundScore <= soundPeakThreshold) {
      if (startOutsideRangeSound == 0) {
        startOutsideRangeSound = millis();
      } else if (millis() - startOutsideRangeSound >= 30000) { // 30s normal
        onAlertSound = false;
        onMaximumAlertSound = false;
        startOutsideRangeSound = 0;
      }
    } else {
      startOutsideRangeSound = 0;
    }
  }
}

// ============ FUNÇÃO: VERIFICAR UMIDADE ============
void checkHumidity(float humidity) { 
  bool withinRange = (humidity >= humidityMin && humidity <= humidityMax);

  if (!onAlertHum) {
    if (!withinRange) {
      consecutiveCounterHum++;

      if (startOutsideRangeHum == 0) {
        startOutsideRangeHum = millis();
      } else if (millis() - startOutsideRangeHum >= maximumAlertTime) {
        onMaximumAlertHum = true;
      } else if (millis() - startOutsideRangeHum >= alertTime) {
        onAlertHum = true;
      }

      if (consecutiveCounterHum >= 3) {
        onAlertHum = true;
      }
    } else {
      consecutiveCounterHum = 0;
      startOutsideRangeHum = 0;
    }
  } else {
    if (withinRange) {
      consecutiveCounterHum++;
      if (consecutiveCounterHum >= 3) {
        onAlertHum = false;
        onMaximumAlertHum = false;
        consecutiveCounterHum = 0;
        startOutsideRangeHum = 0;
      }
    } else {
      consecutiveCounterHum = 0;
    }    
  }
}

// ============ FUNÇÃO: VERIFICAR TEMPERATURA ============
void checkTemperature(float temperature) { 
  bool withinRange = (temperature >= tempMin && temperature <= tempMax);

  if (!onAlertTemp) {
    if (!withinRange) {
      consecutiveCounterTemp++;

      if (startOutsideRangeTemp == 0) {
        startOutsideRangeTemp = millis();
      } else if (millis() - startOutsideRangeTemp >= maximumAlertTime) {
        onMaximumAlertTemp = true;
      } else if (millis() - startOutsideRangeTemp >= alertTime) {
        onAlertTemp = true;
      }

      if (consecutiveCounterTemp >= 3) {
        onAlertTemp = true;
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
        onMaximumAlertTemp = false;
        consecutiveCounterTemp = 0;
        startOutsideRangeTemp = 0;
      }
    } else {
      consecutiveCounterTemp = 0;
    }    
  }
}

// ============ FUNÇÃO: VERIFICAR LUZ (BH1750) ============
void checkBH(float lux) {
  bool withinRange = false;
  
  if (!stateDay) {
    // Noite: deve estar entre 0-1 lux (praticamente escuro)
    withinRange = (lux >= 0.0 && lux <= 1.0);
  } else {
    // Dia: deve estar entre 150-300 lux
    withinRange = (lux >= 150.0 && lux <= 300.0);
  }

  if (!onAlertBH) {
    if (!withinRange) {
      if (startOutsideRangeBH == 0) {
        startOutsideRangeBH = millis();
      } else if (millis() - startOutsideRangeBH >= maximumAlertTime) {
        onMaximumAlertBH = true;
      } else if (millis() - startOutsideRangeBH >= alertTime) {
        onAlertBH = true;
      }
    } else {
      startOutsideRangeBH = 0;
    }
  } else {
    if (withinRange) {
      onAlertBH = false;
      onMaximumAlertBH = false;
      startOutsideRangeBH = 0;
    }
  }
}

// ============ FUNÇÃO: ATIVAR ALERTAS (LEDs) ============
void activateAlerts() {
  int alerts = 0;
  if (onAlertBH) alerts++;
  if (onAlertHum) alerts++;
  if (onAlertTemp) alerts++;
  if (onAlertSound) alerts++;

  bool maximumAlert = (onMaximumAlertBH || onMaximumAlertTemp || 
                       onMaximumAlertHum || onMaximumAlertSound);

  // Alerta vermelho: 2+ sensores OU qualquer alerta máximo
  if (alerts >= 2 || maximumAlert) {
    digitalWrite(yellowLedPin, LOW);
    digitalWrite(redLedPin, HIGH);
    digitalWrite(greenLedPin, LOW);
    redAlert = true;
  } 
  // Alerta amarelo: 1 sensor em alerta
  else if (alerts >= 1) {
    digitalWrite(yellowLedPin, HIGH);
    digitalWrite(redLedPin, LOW);
    digitalWrite(greenLedPin, LOW);
    redAlert = false;
  } 
  // Estado OK
  else {
    digitalWrite(yellowLedPin, LOW);
    digitalWrite(redLedPin, LOW);
    digitalWrite(greenLedPin, HIGH);
    redAlert = false;
  }
}

// ============ FUNÇÃO: CONTROLAR BUZZER ============
void handleBuzzer() {
  if (!redAlert) {
    noTone(buzzerPin);
    buzzerSilenced = false;
    return;
  } else if (buzzerSilenced) {
    if (millis() - silenceStartTime >= silenceDuration) {
      buzzerSilenced = false;
    } else {
      noTone(buzzerPin);
      return;
    }
  }

  unsigned long currentTime = millis();

  if (currentTime - lastBuzzerTime >= buzzerInterval) {
    lastBuzzerTime = currentTime;
    buzzerOn = true;
    buzzerOnStartTime = currentTime;
    tone(buzzerPin, 2000, buzzerBeepDuration);
  }

  if (buzzerOn && (currentTime - buzzerOnStartTime >= buzzerBeepDuration)) {
    buzzerOn = false;
    noTone(buzzerPin);
  }
}

// ============ FUNÇÃO: GERENCIAR BOTÃO ============
void handleButton() {
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
          noTone(buzzerPin);
          buzzerOn = false;
        }
      }
    }
  }
  lastButtonState = reading;
}

// ============ FUNÇÃO: ATUALIZAR ESTATÍSTICAS 24H ============
void calculate24hStats(float lux, float hum, float temp, float sound) {
  // Luminosidade
  if (lux < luxStats24h.minValue) luxStats24h.minValue = lux;
  if (lux > luxStats24h.maxValue) luxStats24h.maxValue = lux;
  luxStats24h.sumValue += lux;
  luxStats24h.readingCount++;
  
  bool luxInRange = stateDay ? (lux >= 150.0 && lux <= 300.0) : (lux >= 0.0 && lux <= 1.0);
  if (!luxInRange) luxStats24h.timeOutOfRange += readingInterval;

  // Umidade
  if (hum < humStats24h.minValue) humStats24h.minValue = hum;
  if (hum > humStats24h.maxValue) humStats24h.maxValue = hum;
  humStats24h.sumValue += hum;
  humStats24h.readingCount++;
  
  if (hum < humidityMin || hum > humidityMax) {
    humStats24h.timeOutOfRange += readingInterval;
  }

  // Temperatura
  if (temp < tempStats24h.minValue) tempStats24h.minValue = temp;
  if (temp > tempStats24h.maxValue) tempStats24h.maxValue = temp;
  tempStats24h.sumValue += temp;
  tempStats24h.readingCount++;
  
  if (temp < tempMin || temp > tempMax) {
    tempStats24h.timeOutOfRange += readingInterval;
  }

  // Som
  if (sound < soundStats24h.minValue) soundStats24h.minValue = sound;
  if (sound > soundStats24h.maxValue) soundStats24h.maxValue = sound;
  soundStats24h.sumValue += sound;
  soundStats24h.readingCount++;
  
  if (sound > soundScoreThreshold) {
    soundStats24h.timeOutOfRange += readingInterval;
  }
}

// ============ FUNÇÃO: CALCULAR MÉDIAS 24H ============
void calculateAverage24h() {
  if (luxStats24h.readingCount > 0) {
    luxStats24h.avgValue = luxStats24h.sumValue / luxStats24h.readingCount;
  }
  if (humStats24h.readingCount > 0) {
    humStats24h.avgValue = humStats24h.sumValue / humStats24h.readingCount;
  }
  if (tempStats24h.readingCount > 0) {
    tempStats24h.avgValue = tempStats24h.sumValue / tempStats24h.readingCount;
  }
  if (soundStats24h.readingCount > 0) {
    soundStats24h.avgValue = soundStats24h.sumValue / soundStats24h.readingCount;
  }
}

// ============ FUNÇÃO: CALCULAR PORCENTAGEM FORA DA FAIXA ============
float calculatePercentageOutOfRange(unsigned long timeOut, unsigned long totalTime) {
  if (totalTime == 0) return 0.0;
  return (float)(timeOut * 100.0) / (float)totalTime;
}

// ============ FUNÇÃO: RESETAR DADOS ACUMULADOS ============
void resetAccumulatedData() {
  sumBH = 0.0;
  sumHum = 0.0;
  sumTemp = 0.0;
  sumSound = 0.0;
  counter = 0;
  
  onAlertBH = false;
  onAlertHum = false;
  onAlertTemp = false;
  onAlertSound = false;
  onMaximumAlertBH = false;
  onMaximumAlertHum = false;
  onMaximumAlertTemp = false;
  onMaximumAlertSound = false;

  startOutsideRangeBH = 0;
  startOutsideRangeHum = 0;
  startOutsideRangeTemp = 0;
  startOutsideRangeSound = 0;
  
  consecutiveCounterHum = 0;
  consecutiveCounterTemp = 0;
  consecutivePeaksSound = 0;
  startPeakSound = 0;

  luxStats24h = {9999.0, -9999.0, 0.0, 0.0, 0, 0};
  tempStats24h = {9999.0, -9999.0, 0.0, 0.0, 0, 0};
  humStats24h = {9999.0, -9999.0, 0.0, 0.0, 0, 0};
  soundStats24h = {9999.0, -9999.0, 0.0, 0.0, 0, 0};
}

// ============ FUNÇÃO: DEFINIR ESTADO INICIAL DO DIA ============
void defineInitialStateDay() {
  float lux = 0.0;
  unsigned long init_lastTime = millis();
  int init_counter = 0;

  // 6 amostras para determinar estado inicial
  while (init_counter < 6) {
    if (millis() - init_lastTime >= readingInterval) {
      init_lastTime += readingInterval;
      lux = lightMeter.readLightLevel();
      sumBH += lux;
      init_counter++;
    }
  }  
  
  averageBH = sumBH / 6.0;
  stateDay = (averageBH >= 20.0);

  sumBH = 0.0;
  init_lastTime = millis();
  init_counter = 0;
  
  // Aguarda transição de estado
  while(true) {
    if (millis() - init_lastTime >= readingInterval) {
      init_lastTime += readingInterval; 
      init_counter++;

      lux = lightMeter.readLightLevel(); 
      sumBH += lux; 

      if (init_counter >= 6) {
        averageBH = sumBH / 6.0;
        init_counter = 0;
        sumBH = 0.0;

        if (averageBH < 20.0) {
          if (stateDay) {
            stateDay = false;
            break;
          } 
        } else if (averageBH >= 20.0) {
          if (!stateDay) {
            stateDay = true;
            break;
          } 
        }
      }
    }    
  }
  counterStateDay = millis();
}

// ============ SETUP ============
void setup() {
  Serial.begin(9600);
  Wire.begin();

  lightMeter.begin();
  dht.begin();
  pinMode(KY037Digital, INPUT);

  defineInitialStateDay();

  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  pinMode(yellowLedPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  lastTime = millis();
  sumBH = 0.0;
  sumHum = 0.0;
  sumTemp = 0.0;
  sumSound = 0.0;

  noTone(buzzerPin);

  // Cabeçalho CSV
  Serial.println("tipo,timestamp,sensor,valor1,valor2,valor3");
}

// ============ LOOP PRINCIPAL ============
void loop() {
  unsigned long currentTime = millis();

  // Alterna estado do dia após 12h
  if (currentTime - counterStateDay >= twelveHours) {
    stateDay = !stateDay;
    counterStateDay = currentTime;
  } 

  // Processar métricas de 24h e resetar
  if (currentTime >= twentyFourHours && 
      (currentTime - counterStateDay) >= twentyFourHours) {
    
    calculateAverage24h();
    
    unsigned long totalTime24h = twentyFourHours;
    
    // Calcula porcentagens
    float luxPercOut = calculatePercentageOutOfRange(luxStats24h.timeOutOfRange, totalTime24h);
    float tempPercOut = calculatePercentageOutOfRange(tempStats24h.timeOutOfRange, totalTime24h);
    float humPercOut = calculatePercentageOutOfRange(humStats24h.timeOutOfRange, totalTime24h);
    float soundPercOut = calculatePercentageOutOfRange(soundStats24h.timeOutOfRange, totalTime24h);

    // CSV: Resumo 24h - Luminosidade
    Serial.print("resumo_24h,");
    Serial.print(currentTime);
    Serial.print(",LUX,");
    Serial.print(luxStats24h.minValue, 2);
    Serial.print(",");
    Serial.print(luxStats24h.maxValue, 2);
    Serial.print(",");
    Serial.print(luxStats24h.avgValue, 2);
    Serial.print(",");
    Serial.println(luxPercOut, 2);

    // CSV: Resumo 24h - Temperatura
    Serial.print("resumo_24h,");
    Serial.print(currentTime);
    Serial.print(",TEMP,");
    Serial.print(tempStats24h.minValue, 2);
    Serial.print(",");
    Serial.print(tempStats24h.maxValue, 2);
    Serial.print(",");
    Serial.print(tempStats24h.avgValue, 2);
    Serial.print(",");
    Serial.println(tempPercOut, 2);

    // CSV: Resumo 24h - Umidade
    Serial.print("resumo_24h,");
    Serial.print(currentTime);
    Serial.print(",HUM,");
    Serial.print(humStats24h.minValue, 2);
    Serial.print(",");
    Serial.print(humStats24h.maxValue, 2);
    Serial.print(",");
    Serial.print(humStats24h.avgValue, 2);
    Serial.print(",");
    Serial.println(humPercOut, 2);

    // CSV: Resumo 24h - Som
    Serial.print("resumo_24h,");
    Serial.print(currentTime);
    Serial.print(",SOM,");
    Serial.print(soundStats24h.minValue, 2);
    Serial.print(",");
    Serial.print(soundStats24h.maxValue, 2);
    Serial.print(",");
    Serial.print(soundStats24h.avgValue, 2);
    Serial.print(",");
    Serial.println(soundPercOut, 2);

    resetAccumulatedData();
    counterStateDay = currentTime;
  }

  handleButton();

  // Leitura dos sensores a cada 10s
  if (currentTime - lastTime >= readingInterval) {
    lastTime += readingInterval; 
    counter++;

    // Leituras
    float lux = lightMeter.readLightLevel();
    float humidity = dht.readHumidity();
    float temp = dht.readTemperature();
    float soundScore = readSoundSensor();

    // Atualiza estatísticas
    calculate24hStats(lux, humidity, temp, soundScore);

    // Acumula para médias de 1min
    sumBH += lux;
    sumHum += humidity;
    sumTemp += temp;
    sumSound += soundScore;
   
    // Verifica alertas
    checkBH(lux);
    checkHumidity(humidity);
    checkTemperature(temp);
    checkSound(soundScore);

    // CSV: Leituras instantâneas
    Serial.print("leitura,");
    Serial.print(currentTime);
    Serial.print(",LUX,");
    Serial.print(lux, 2);
    Serial.println(",,");

    Serial.print("leitura,");
    Serial.print(currentTime);
    Serial.print(",TEMP,");
    Serial.print(temp, 2);
    Serial.println(",,");

    Serial.print("leitura,");
    Serial.print(currentTime);
    Serial.print(",HUM,");
    Serial.print(humidity, 2);
    Serial.println(",,");

    Serial.print("leitura,");
    Serial.print(currentTime);
    Serial.print(",SOM,");
    Serial.print(soundScore, 2);
    Serial.println(",,");

    // Média em 1min (6 leituras)
    if (counter >= 6) {
      averageBH = sumBH / 6.0;
      averageHum = sumHum / 6.0;
      averageTemp = sumTemp / 6.0;
      averageSound = sumSound / 6.0;

      counter = 0;
      sumBH = 0.0;
      sumHum = 0.0;
      sumTemp = 0.0;
      sumSound = 0.0;

      // CSV: Médias de 1min
      Serial.print("media_1min,");
      Serial.print(currentTime);
      Serial.print(",LUX,");
      Serial.print(averageBH, 2);
      Serial.println(",,");

      Serial.print("media_1min,");
      Serial.print(currentTime);
      Serial.print(",TEMP,");
      Serial.print(averageTemp, 2);
      Serial.println(",,");

      Serial.print("media_1min,");
      Serial.print(currentTime);
      Serial.print(",HUM,");
      Serial.print(averageHum, 2);
      Serial.println(",,");

      Serial.print("media_1min,");
      Serial.print(currentTime);
      Serial.print(",SOM,");
      Serial.print(averageSound, 2);
      Serial.println(",,");
    }

    activateAlerts();
  }    

  handleBuzzer();
}