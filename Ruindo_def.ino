//============PINS=============
#define pinSensorAnalogico A4
#define pinSensorDigital 7
#define pinLED LED_BUILTIN

//============CONSTS=============
int contadorRuido = 0; 

//============STRUCTS=============
struct LeituraSensorA {
  int vpp;
  float db;
};

struct LeituraSensorD {
  int dVal;
  bool alerta;
};

//============CONFIGS/DEFS=============
void setup() {
  pinMode(pinSensorDigital, INPUT); //Seta porta digital Input
  pinMode(pinLED, OUTPUT); //Seta porta digital Input
  Serial.begin(9600);
  Serial.println("vpp,db,status"); //HEAD do CSV
}

// -------------------------------
// Leitura ANALÓGICA VPP
// -------------------------------

LeituraSensorA lerSensorAnalogico(){

  const int N = 200;   // AMOSTRAS
  int minimo = 1023;
  int maximo = 0;

  // mede pico-a-pico 
  for (int i = 0; i < N; i++) {
    int val = analogRead(pinSensorAnalogico);
    if (val > maximo) maximo = val;
    if (val < minimo) minimo = val;
  }

  int vpp = maximo - minimo;   // pico-a-pico
  float dbAprox = 20 * log10(vpp) + 37; // conversão APROX , 37 é o offset

  LeituraSensorA dadosA;
  dadosA.vpp = vpp;
  dadosA.db = dbAprox;
  return dadosA;


}

// -------------------------------  
// LEITURA/CONTROLE DIGITAL
// -------------------------------

LeituraSensorD writeSensorDigital(float dbAprox){

  int digitalVal = digitalRead(pinSensorDigital); //digital
  bool alerta = false;

  //70 DB É O LIMIAR
  if (dbAprox > 70) {
    digitalVal = HIGH;  // Som alto 
    alerta = true;
    
  } else {
    digitalVal = LOW;   // Som baixo 
  }

  LeituraSensorD dadosD;
  dadosD.dVal = digitalVal;
  dadosD.alerta = alerta;

  return  dadosD;
}

// -------------------------------  
//LOGICA DE CONTROLE LED
// -------------------------------
void ledControl(int status){
  digitalWrite(pinLED, status); //APAGA OU ACENDE LED
  }

// -------------------------------  
//CRIANDO CSV
// ------------------------------- 
void printCSV(int vpp, float dbAprox, const String& status){
  Serial.print(vpp);
  Serial.print(",");
  Serial.print(dbAprox, 2);
  Serial.print(",");
  Serial.println(status);
}

bool alertaCritico(){
  //PEGA OS VALORES
  LeituraSensorA dadosA = lerSensorAnalogico();
  LeituraSensorD  dadosD = writeSensorDigital(dadosA.db);
  //CONTROLA LED
  ledControl(dadosD.dVal); // TODO: VOCE PODE TIRAR ESSE AQ E DEIXA PARA SO ACENDER AO FIM DOS 10 S
  if (dadosD.dVal = 1){
    contadorRuido = contadorRuido + 1;
  }
  return contadorRuido;
}



void loop() {

  String status =  "OK";
  LeituraSensorA dadosA = lerSensorAnalogico();
  LeituraSensorD  dadosD = writeSensorDigital(dadosA.db);
  //CONTROLA LED
  ledControl(dadosD.dVal);

  if (contadorRuido >= 5){
    String status =  "ALERTA";
  }

  //MSG 
  printCSV(dadosA.vpp,dadosA.db,status);


  delay(10*1000);

}
