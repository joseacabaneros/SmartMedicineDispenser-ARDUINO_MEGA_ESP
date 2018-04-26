
/**********************************  INCLUDES  ***********************************/
#include <AccelStepper.h>    //Libreria para controlar los motores paso a paso
#include <DHT.h>             //Libreria para controlar sensor de temperatura y humedad
/********************************************************************************/

/**********************************  DEFINES  ***********************************/
#define PIN_LED_NOT_TOMA   46
#define PIN_ZUMB_NOT_TOMA  1   //25
#define PIN_LED_WIFI_OK    29
#define PIN_BTN_CONF       50
#define PIN_BTN_EMER       52
#define PIN_IR_TOMA        48
#define PIN_GAS            33
#define PIN_VIB            30

#define HALFSTEP           8      //Half-step mode (8 step control signal sequence)
#define DHTTYPE            DHT11  //Sensor temp&humedad DHT11

#define PIN_DHT            22

//Define para los pines del MOTOR PASO A PASO 'A'
#define motorAPin1         42     // IN1 on the ULN2003 driver A
#define motorAPin2         43     // IN2 on the ULN2003 driver A
#define motorAPin3         44     // IN3 on the ULN2003 driver A
#define motorAPin4         45     // IN4 on the ULN2003 driver A

//Define para los pines del MOTOR PASO A PASO 'B'
#define motorBPin1         38     // IN1 on the ULN2003 driver B
#define motorBPin2         39     // IN2 on the ULN2003 driver B
#define motorBPin3         40     // IN3 on the ULN2003 driver B
#define motorBPin4         41     // IN4 on the ULN2003 driver B
/********************************************************************************/

/*********************************  CONSTANTES  *********************************/
//Cada movimiento del motor paso a paso para que de 13 vueltas en los 360º
const int pasos = 315;

//COMANDOS DE EJECUCION EN ESP8266
//Motor a llegado a su posicion y ha dispensado las pastillas correspondientes
const String motorPastillasDispensadasA = "[MOTORDISPENSADO-A]";
const String motorPastillasDispensadasB = "[MOTORDISPENSADO-B]";
const String botonConfirmacionPulsado = "[PULSABTN-1]";
const String botonEmergenciaPulsado = "[PULSABTN-2]";
const String sensorIrDetectado = "[IRDETECCION-1]";
const String tempHumeCalor = "[TEMPHUMCALOR";
const String sensorGasDetectado = "[GASDETECCION-1]";
const String sensorVibDetectado = "[VIBDETECCION-1]";

//COMANDO RECIBIDOS DE ESP8266
const String moverPastillero = "MPAS";
const String wifiok = "WIFIOK";
const String medicacionTomada = "MEDTOMADA";
//Comando completo "[SOLICTEMHUM-*temp*-*hume*-*calor*]"
const String solicitarTempHum = "SOLICTEMHUM";
/********************************************************************************/

/*********************************  VARIABLES  **********************************/
// Initialize with pin sequence IN1-IN3-IN2-IN4 for using the AccelStepper with 28BYJ-48
AccelStepper stepperA(HALFSTEP, motorAPin1, motorAPin3, motorAPin2, motorAPin4);
AccelStepper stepperB(HALFSTEP, motorBPin1, motorBPin3, motorBPin2, motorBPin4);

// Inicializamos el sensor DHT11 (temperatura y humedad)
DHT dht(PIN_DHT, DHTTYPE);

//Variables de control
bool movPasA;
bool movPasB;
int oldStateBtnConf;
int oldStateIrConf;
int oldStateGas;
int oldStateVib;

String stringRecibido;
/********************************************************************************/

void setup() {
  //Inicializar puertos y salidas
  Serial.begin(115200); //Atmega2560
  Serial3.begin(115200); //ESP8266

  //Esperar que se conecte el puerto serie. Necesario solo para el puerto USB nativo
  while (!Serial3) { }

  //Pin modes
  pinMode(PIN_LED_NOT_TOMA, OUTPUT);
  pinMode(PIN_ZUMB_NOT_TOMA, OUTPUT);
  pinMode(PIN_LED_WIFI_OK, OUTPUT);
  pinMode(PIN_BTN_CONF, INPUT);
  pinMode(PIN_BTN_EMER, INPUT);
  pinMode(PIN_IR_TOMA, INPUT);
  pinMode(PIN_GAS, INPUT);
  pinMode(PIN_VIB, INPUT);
  
  //Inicializar pines
  digitalWrite(PIN_LED_NOT_TOMA, LOW);
  digitalWrite(PIN_ZUMB_NOT_TOMA, LOW);
  digitalWrite(PIN_LED_WIFI_OK, LOW);

  //Inicializar motor paso a paso A
  stepperA.setMaxSpeed(500.0);
  stepperA.setAcceleration(100.0);
  stepperA.setSpeed(200);
  //Inicializar motor paso a paso B
  stepperB.setMaxSpeed(500.0);
  stepperB.setAcceleration(100.0);
  stepperB.setSpeed(200);

  //Inicializar el sensor DHT (temp&humedad)
  dht.begin();

  //Inicializacion de variables de control
  movPasA = false;
  movPasB = false;
  oldStateBtnConf = LOW;
  oldStateIrConf = HIGH;
  oldStateGas = HIGH;
  oldStateVib = HIGH;
}

/********************************************************************************/

void loop() {
  //Eventos surgidos del Serial3 (ESP8266)
  serial3Event();

  //Control de parada de motores
  controlMotores();

  //Eventos surgidos en hardware accionable
  eventosHardware();
}

/********************************************************************************/


/* Verificar el evento en el puerto Serial3 (ESP8266) */
void serial3Event() {
  while (Serial3.available()) {
    char inChar = Serial3.read();  //Leer datos del puerto Serial3
    stringRecibido += inChar;

    //Encuentra el comando en los datos recibidos (el comando debe estar entre corchetes)
    if (inChar == ']') {
      //Salida de los datos de lectura al puerto Serial3
      Serial.println(stringRecibido);

      stringRecibido = stringRecibido.substring(1);         //Borrar '[' del codigo
      stringRecibido.remove(stringRecibido.length() - 1);   //Borrar ']' del valor
      
      String code = split(stringRecibido, '-', 0);
      String value = split(stringRecibido, '-', 1);

      Serial.println("EVENT ESP2ATmega");
      Serial.println("Code: " + code);
      Serial.println("Value: " + value);

      //Evento de MOVER PASTILLERO
      if (code.equals(moverPastillero)) {
        digitalWrite(PIN_LED_NOT_TOMA, HIGH);
        digitalWrite(PIN_ZUMB_NOT_TOMA, HIGH);
        
        String numPastillas = split(stringRecibido, '-', 2);
        Serial.println("Pastillas: " + numPastillas);
        
        //Mover PASTILLERO A
        //Comando completo "[MPAS-A-3]"
        if (value == "A") {
          movPasA = true;
          stepperA.move(pasos * numPastillas.toInt());
        }
        //Mover PASTILLERO B
        //Comando completo "[MPAS-B-3]"
        else if (value == "B") {
          movPasB = true;
          stepperB.move(pasos * numPastillas.toInt());
        }
      }
      //Evento de WIFIOK
      else if (code.equals(wifiok)) {
        digitalWrite(PIN_LED_WIFI_OK, HIGH);
      }
      //Evento de MEDTOMADA (Sensor IR y btn confirmacion ok)
      else if (code.equals(medicacionTomada)) {
        digitalWrite(PIN_LED_NOT_TOMA, LOW);
        digitalWrite(PIN_ZUMB_NOT_TOMA, LOW);
      }
      //Evento de SOLICITARTEMPHUM (solicitud de temperatura, humedad y calor)
      else if (code.equals(solicitarTempHum)) {
        float h = dht.readHumidity();         //Humedad relativa
        float t = dht.readTemperature();      //Temperatura en grados centígrados
        
        // Comprobamos si ha habido algún error en la lectura
        if (isnan(h) || isnan(t)) {
          Serial.println("Error obteniendo los datos del sensor DHT11");
          return;
        }
        
        // Calcular el índice de calor en grados centígrados
        float hic = dht.computeHeatIndex(t, h, false);

        Serial3.print(tempHumeCalor + "-" + String(t) + "-" + String(h) + "-" + String(hic) + "]");
      }
      else {
        Serial.println("COMANDO ERRONEO!");
      }

      stringRecibido = "";
    }
    //String resibido de ESP8266 para ser mostrado en el monitor
    else if(inChar == '\n'){
      //Salida de los datos de lectura al puerto Serial
      Serial.print(stringRecibido);
      stringRecibido = "";
    }
  }
}


/* Control de parada de los motores paso a paso de los pastilleros */
void controlMotores(){
  if (movPasA) {
    if (stepperA.distanceToGo() == 0) {
      stepperA.stop();
      movPasA = false;
      Serial3.print(motorPastillasDispensadasA);
    } else {
      stepperA.run();
    }
  }
  if (movPasB) {
    if (stepperB.distanceToGo() == 0) {
      stepperB.stop();
      movPasB = false;
      Serial3.print(motorPastillasDispensadasB);
    } else {
      stepperB.run();
    }
  }
}


/* Eventos surgidos en el hardware accionable/detectable de la caja de medicamentos */
void eventosHardware(){
  //BOTON DE CONFIRMACION DE TOMA DE MEDICATION
  int newStateBtnConf = digitalRead(PIN_BTN_CONF);
  if (newStateBtnConf == HIGH && oldStateBtnConf == LOW) {
    Serial3.print(botonConfirmacionPulsado);
    oldStateBtnConf = HIGH;
  }
  else if (newStateBtnConf == LOW && oldStateBtnConf == HIGH) {
    oldStateBtnConf = LOW;
  }
  
  //SENSOR IR QUE DETECTA SI LA MEDICACION HA SIDO TOMADA
  int newStateIrConf = digitalRead(PIN_IR_TOMA);
  if (newStateIrConf == LOW && oldStateIrConf == HIGH) {  //Deteccion
    Serial3.print(sensorIrDetectado);
    oldStateIrConf = LOW;
  }
  else if (newStateIrConf == HIGH && oldStateIrConf == LOW) {
    oldStateIrConf = HIGH;
  }

  //SENSOR GAS
  int newStateGas = digitalRead(PIN_GAS);
  if (newStateGas == LOW && oldStateGas == HIGH) {           //Deteccion
    Serial3.print(sensorGasDetectado);
    oldStateGas = LOW;
  }
  else if (newStateGas == HIGH && oldStateGas == LOW) {
    oldStateGas = HIGH;
  }

  //SENSOR VIBRACION
  int newStateVib = digitalRead(PIN_VIB);
  if (newStateVib == LOW && oldStateVib == HIGH) {          //Deteccion
    Serial3.print(sensorVibDetectado);
    oldStateVib = LOW;
  }
  else if (newStateVib == HIGH && oldStateVib == LOW) {
    oldStateVib = HIGH;
  }
}


/* Metodo de utilidad para hacer split de un String */
String split(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}
