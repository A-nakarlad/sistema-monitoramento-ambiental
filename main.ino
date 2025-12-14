#include <BH1750.h>
#include <Wire.h>
#include "DHT.h"

#define DHTPIN 19
#define DHTTYPE DHT11 
#define KY037Analog 34
#define KY037Digital 35

#define readingInterval 10000 //intervalo de leituras dos sensores
#define alertTime 50000 //tempo máximo para alerta amarelo
#define maximumAlertTime 60000 //tempo para alerta vermelho
#define buzzerInterval 30000 //intervalo do toque do buzzer
#define silenceDuration 600000 //período sem toque do buzzer

//faixas dos sensores DHT
#define tempMin 22.0
#define tempMax 26.0
#define humidityMin 40.0
#define humidityMax 60.0

BH1750 lightMeter;
DHT dht(DHTPIN, DHTTYPE);


//pinout 
const int redLedPin = 15;
const int greenLedPin = 4;
const int yellowLedPin = 2;
const int buzzerPin = 18;
const int buttonPin = 23;

const unsigned long twelveHours = 12 * 60 * 60 * 1000UL;
const unsigned long twentyFourHours = 24UL * 60 * 60 * 1000UL;  

//contadores
unsigned long lastTime = 0; //última leitura dos sensores
int counter = 0; //contador de amostras em 1min
unsigned long counterStateDay = 0; //conta 12hrs

//controle do estado do dia
bool stateDay = false; //noite = false, dia = true

//BH variables
unsigned long startOutsideRangeBH = 0; //quando a luz saiu da faixa
float sumBH = 0.0;
float averageBH = 0.0;
bool onAlertBH = false; //alerta amarelo
bool onMaximumAlertBH = false; //alerta vermelho

//DHT variables - humidity
float sumHum = 0.0;
float averageHum = 0.0;
int consecutiveCounterHum = 0;
bool onAlertHum = false;
bool onMaximumAlertHum = false;
unsigned long startOutsideRangeHum = 0;

//DHT variables - temperature
float sumTemp = 0.0;
float averageTemp = 0.0;
int consecutiveCounterTemp = 0;
bool onAlertTemp = false;
bool onMaximumAlertTemp = false;
unsigned long startOutsideRangeTemp = 0;

bool redAlert = true;

//controles do buzzer
unsigned long lastBuzzerTime = 0; //ultimo toque 
unsigned long silenceStartTime = 0; //quando foi silenciado
bool buzzerSilenced = false; //se está silenciado          
bool buzzerOn = false; //se está tocando               
unsigned long buzzerOnStartTime = 0; //quando começou a tocar
const int buzzerBeepDuration = 200; //duração do toque

//controles do botão
int buttonState = HIGH; //estado atual
int lastButtonState = HIGH; //estado anterioi
unsigned long lastDebounceTime = 0; //utima vez que mudou
const unsigned long debounceDelay = 50;

//métricas em 24hrs
struct SensorStats {
  float minValue;
  float maxValue;
  float avgValue;
  float sumValue;
  int readingCount;
};

SensorStats luxStats24h = {9999.0, -9999.0, 0.0, 0.0, 0}; 
SensorStats tempStats24h = {9999.0, -9999.0, 0.0, 0.0, 0};
SensorStats humStats24h = {9999.0, -9999.0, 0.0, 0.0, 0};

/*verifica a luminosidade para determinar se é dia ou noite
espera ocorrer mudança no estado do dia para começar as leituras*/
void defineInitialStateDay () {

  float lux = 0.0;
  unsigned long init_lastTime = millis();
  int init_counter = 0;

  //6 amostras para determinar o estado inicial
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
  
  //aguarda transição de estado
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

        if (averageBH < 20) {
          if (stateDay) {
            stateDay = false;
            break;
          } 
        }else if (averageBH >= 20) { //dia é maior que 20???
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

void setup() {
  Serial.begin(9600);

  Wire.begin();

  lightMeter.begin();
  dht.begin();
  pinMode(pinSensorDigital, INPUT);

  defineInitialStateDay();

  pinMode (redLedPin, OUTPUT);
  pinMode (greenLedPin, OUTPUT);
  pinMode (yellowLedPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  lastTime = millis();
  sumBH = 0.0;

  noTone(buzzerPin);

  Serial.println("tipo,time,sensor,v1,v2,v3");
}

//verificar se a humidade está dentro da faixa ideal
void checkHumidity (float humidity) { 
  bool withinRange = (humidity >= humidityMin && humidity <= humidityMax);

  //se ainda não está em alerta
  if (!onAlertHum) {
    if (!withinRange){ //está fora de faixa
      consecutiveCounterHum++;

      //primeira vez fora de faixa
      if (startOutsideRangeHum == 0) {
        startOutsideRangeHum = millis();
      } 
      //alerta vermelho se passar o tempo máximo
      else if (millis() - startOutsideRangeHum >= maximumAlertTime ) {
        Serial.println("alerta - mais de 3min");
        onMaximumAlertHum = true;
      } 
      //alerta amarelo se passar do tempo
      else if (millis() - startOutsideRangeHum >= alertTime ) {
        Serial.println("alerta - mais de 2min");
        onAlertHum = true;
      } 

      //3 leituras consecutivas fora de faixa
      if (consecutiveCounterHum >= 3) {
        //onAlertHum = true;
        Serial.println("alerta hum");
      }
    } 
    //dentro da faixa
    else { 
      consecutiveCounterHum = 0;
      consecutiveCounterHum = 0;
    }
  } 
  //se já está em alerta
  else {
    //voltou para a faixa
    if (withinRange) { 
      consecutiveCounterHum++;

        //3 leituras consecutivas dentro da faixa
        if (consecutiveCounterHum >= 3) {
        onAlertHum = false;
        consecutiveCounterHum = 0;
        startOutsideRangeTemp = 0;
        Serial.println("hum ok");
      }
    } 
    //continua fora da faixa
    else {
      consecutiveCounterHum = 0;
    }    
  }
}

//verificar se a temperatura está dentro da faixa ideal
void checkTemperature (float temperature) { 
  bool withinRange = (temperature >= tempMin && temperature <= tempMax);

  //ainda não está em alerta
  if (!onAlertTemp) { //está fora de faixa
    if (!withinRange){
      consecutiveCounterTemp++;

      //primeira vez fora da faixa
      if (startOutsideRangeTemp == 0) {
        startOutsideRangeTemp = millis();
      } 
      //alerta vermelho se passou o tempo máximo
      else if (millis() - startOutsideRangeTemp >= maximumAlertTime) {
        Serial.println("alerta - mais de 2min");
        onMaximumAlertTemp = true;
      } 
      //alerta amarelo
      else if (millis() - startOutsideRangeTemp >= alertTime ) {
        Serial.println("alerta - mais de 2min");
        onAlertTemp = true;
      } 

      //3 leituras consecutivas fora da faixa
      if (consecutiveCounterTemp >= 3) {
        //onAlertTemp = true;
        Serial.println("alerta temp");
      }
    } 
    //dentro da faixa
    else {
      consecutiveCounterTemp = 0;
      startOutsideRangeTemp = 0;
    }
  } 
  //já está em alerta
  else {
    if (withinRange) {
      consecutiveCounterTemp++;

        //3 leituras consecutivas dentro da faixa
        if (consecutiveCounterTemp >= 3) {
        onAlertTemp = false;
        consecutiveCounterTemp = 0;
        startOutsideRangeTemp = 0;
        Serial.println("temp ok");
      }
    } 
    //continua fora da faixa
    else {
      consecutiveCounterTemp = 0;
    }    
  }
}

//verificar se a luminosidade está dentro da faixa ideal
void checkBH (float lux) {
  //se é noite
  if (!stateDay){
    if (lux >= 1 && lux <= 3) {
      //primeira vez fora da faixa
      if (startOutsideRangeBH == 0) {
        startOutsideRangeBH = millis();
      } 
      //alerta vermelho se passou o tempo máximo
      else if (onAlertBH && (millis() - startOutsideRangeBH >= maximumAlertTime)) {
        Serial.println("alerta - mais de 2min");
        onMaximumAlertBH = true;
      } 
      //alerta amarelo
      else if (millis() - startOutsideRangeBH >= alertTime ) {
        Serial.println("alerta - mais de 2min");
        onAlertBH = true;
      } 
    } 
    //dentro da faixa
    else {
      if (onAlertBH) {
        onAlertBH = false;
      }
      startOutsideRangeBH = 0;
    }
  } 
  //durante o dia
  else {
    if (lux < 150 || lux > 300) {
      //primeira vez fora da faixa
      if (startOutsideRangeBH == 0) {
        startOutsideRangeBH = millis();
      } 
      //alerta vermelho se passou o tempo máximo
      else if (onAlertBH && (millis() - startOutsideRangeBH >= maximumAlertTime)) {
        Serial.println("alerta - mais de 2min");
        onMaximumAlertBH = true;
      } 
      //alerta amarelo
      else if (millis() - startOutsideRangeBH >= alertTime ) {
        Serial.println("alerta - mais de 2min");
        onAlertBH = true;
      } 
    }    
    else {
      if (onAlertBH) {
        onAlertBH = false;
      }
      startOutsideRangeBH = 0;
    }
  } 
}

//controle dos leds de alertas
void activateAlerts () {
  bool yellowAlert = (onAlertBH || (onAlertTemp || onAlertHum));
  bool maximumAlert = (onMaximumAlertBH || (onMaximumAlertTemp || onMaximumAlertHum));
  int alerts = 0;

  if (onAlertBH) alerts++;
  if (onAlertHum) alerts++;
  if (onAlertTemp) alerts++;

  //qualquer alerta máximo ou dois sensores em alerta
  if (alerts >= 2 || maximumAlert) {
    digitalWrite (yellowLedPin, LOW);
    digitalWrite (redLedPin, HIGH);
    digitalWrite (greenLedPin, LOW);
    redAlert = true;
    Serial.println(redAlert);
  } 
  //qualquer sensor em alerta
  else if (yellowAlert) {
    digitalWrite (yellowLedPin, HIGH);
    digitalWrite (redLedPin, LOW);
    digitalWrite (greenLedPin, LOW);
    redAlert = false;
  } 
  //estado ok
  else {
    digitalWrite (yellowLedPin, LOW);
    digitalWrite (redLedPin, LOW);
    digitalWrite (greenLedPin, HIGH);
    redAlert = false;
  }
}

//controle do buzzer durante alertas vermelhos
void handleBuzzer () {
  //se não está em alerta vermelho
  if (!redAlert) {
    //digitalWrite(buzzerPin, LOW);
    noTone(buzzerPin);
    buzzerSilenced = false;
    return;
  } 
  //se foi silenciado pelo botão
  else if (buzzerSilenced) {
    //verifica se passou o tempo de silencio
    if (millis() - silenceStartTime >= silenceDuration) {
      buzzerSilenced = false;
    } else {
      //digitalWrite(buzzerPin, LOW);
      noTone(buzzerPin);
      return;
    }
  }

  unsigned long currentTime = millis();

  //verifica se é o momento do toque
  if (currentTime - lastBuzzerTime >= buzzerInterval) {
    lastBuzzerTime = currentTime;
    buzzerOn = true;
    buzzerOnStartTime = currentTime;
    //digitalWrite(buzzerPin, HIGH);
    tone(buzzerPin, 2000, buzzerBeepDuration);
  }

  //verifica se completou o toque
  if (buzzerOn && (currentTime - buzzerOnStartTime >= buzzerBeepDuration)) {
    buzzerOn = false;
    //digitalWrite(buzzerPin, LOW);
    noTone(buzzerPin);
  }
}

//gerencia o botçao para silencia o buzzer por 10min
void handleButton () {
  int reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  //aguardar tempo para evitar leituras falsas
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      
      //se foi pressionado
      if (buttonState == LOW) {
        if (redAlert) {
          buzzerSilenced = true;
          silenceStartTime = millis();
          //digitalWrite(buzzerPin, LOW);
          noTone(buzzerPin);
          buzzerOn = false;
        }
      }
    }
  }
  lastButtonState = reading;
}

//reiniciar todas as variáveis e alertas após 24h
void resetAccumulatedData () {
  sumBH = 0.0;
  sumHum = 0.0;
  sumTemp = 0.0;
  counter = 0;
  
  onAlertBH = false;
  onAlertHum = false;
  onAlertTemp = false;
  onMaximumAlertBH = false;
  onMaximumAlertHum = false;
  onMaximumAlertTemp = false;

  startOutsideRangeBH = 0;
  startOutsideRangeHum = 0;
  startOutsideRangeTemp = 0;
  
  consecutiveCounterHum = 0;
  consecutiveCounterTemp = 0;

  luxStats24h.minValue = 9999.0;
  luxStats24h.maxValue = -9999.0;
  luxStats24h.avgValue = 0.0;
  luxStats24h.sumValue = 0.0;
  luxStats24h.readingCount = 0;
  
  tempStats24h.minValue = 9999.0;
  tempStats24h.maxValue = -9999.0;
  tempStats24h.avgValue = 0.0;
  tempStats24h.sumValue = 0.0;
  tempStats24h.readingCount = 0;
  
  humStats24h.minValue = 9999.0;
  humStats24h.maxValue = -9999.0;
  humStats24h.avgValue = 0.0;
  humStats24h.sumValue = 0.0;
  humStats24h.readingCount = 0;
}

//atualizar métricas de 24h
void calculate24hStats (float lux, float hum, float temp) {
  if (lux < luxStats24h.minValue) luxStats24h.minValue = lux;
  else if (lux > luxStats24h.maxValue) luxStats24h.maxValue = lux;
  luxStats24h.sumValue += lux;
  luxStats24h.readingCount++;

  if (hum < humStats24h.minValue) humStats24h.minValue = hum;
  else if (hum > humStats24h.maxValue) humStats24h.maxValue = hum;
  humStats24h.sumValue += sum;
  humStats24h.readingCount++;

  if (temp < tempStats24h.minValue) tempStats24h.minValue = temp;
  else if (temp > tempStats24h.maxValue) tempStats24h.maxValue = temp;
  tempStats24h.sumValue += temp;
  tempStats24h.readingCount++;
}

//calcular médias em 24h
void calculateAverage24h () {
  if (luxStats24h.readingCount > 0) {
    luxStats24h.avgValue = luxStats24h.sumValue / luxStats24h.readingCount;
  }
  if (humStats24h.readingCount > 0) {
    humStats24h.avgValue = humStats24h.sumValue / humStats24h.readingCount;
  }
  if (tempStats24h.readingCount > 0) {
    tempStats24h.avgValue = tempStats24h.sumValue / tempStats24h.readingCount;
  }
}


void loop() {

  unsigned long currentTime = millis();

  //alterna estado do dia após 12h
  if (currentTime - counterStateDay >= twelveHours) {
    stateDay = !stateDay;
  } 

  //processar métricas entre 24h e resetar
  if (currentTime - counterStateDay >= twentyFourHours) {
    calculateAverage24h();

    Serial.print("resumo_24h,");
    Serial.print(currentTime);
    Serial.print(",BH1750,");
    Serial.print(luxStats24h.minValue);
    Serial.print(",");
    Serial.print(luxStats24h.maxValue);
    Serial.print(",");
    Serial.println(luxStats24h.avgValue);

    Serial.print("resumo_24h,");
    Serial.print(currentTime);
    Serial.print(",DHT_TEMP,");
    Serial.print(tempStats24h.minValue);
    Serial.print(",");
    Serial.print(tempStats24h.maxValue);
    Serial.print(",");
    Serial.println(tempStats24h.avgValue);

    Serial.print("resumo_24h,");
    Serial.print(currentTime);
    Serial.print(",DHT_HUM,");
    Serial.print(humStats24h.minValue);
    Serial.print(",");
    Serial.print(humStats24h.maxValue);
    Serial.print(",");
    Serial.println(humStats24h.avgValue);

    resetAccumulatedData();
    counterStateDay = currentTime;
  }

  handleButton();

  //leitura dos sensores a cada 10s
  if (currentTime - lastTime >= readingInterval) {
    lastTime += readingInterval; 
    counter++;

    float lux = lightMeter.readLightLevel();
    float humidity = dht.readHumidity();
    float temp = dht.readTemperature();

    calculate24hStats(lux, humidity, temp);

    sumBH += lux;
    sumHum += humidity;
    sumTemp += temp;
   
    checkBH(lux);
    checkHumidity(humidity);
    checkTemperature(temp);

    //pra salvar no csv
    Serial.print("leitura,");
    Serial.print(currentTime);
    Serial.print(",BH1750,");
    Serial.print(lux);
    Serial.println(",,");

    Serial.print("leitura,");
    Serial.print(currentTime);
    Serial.print(",DHT,");
    Serial.print(tmp);
    Serial.println(",");
    Serial.print(humidity);
    Serial.println(",");

    //média em 1min
    if (counter >= 6 ) {
      averageBH = sumBH / 6.0;
      averageHum = sumHum / 6.0;
      averageTemp = sumTemp / 6.0;

      counter = 0;
      sumBH = 0.0;
      sumHum = 0.0;
      sumTemp = 0.0;

      Serial.print("media_1min ");
      Serial.print(currentTime);
      Serial.print(",BH1750,");
      Serial.print(averageBH);
      Serial.println(",,");

      Serial.print("media_1min,");
      Serial.print(currentTime);
      Serial.print(",DHT,");
      Serial.print(averageTemp);
      Serial.print(",");
      Serial.print(averageHum);
      Serial.println(",");
    }

    activateAlerts();
  }    

  handleBuzzer();
}