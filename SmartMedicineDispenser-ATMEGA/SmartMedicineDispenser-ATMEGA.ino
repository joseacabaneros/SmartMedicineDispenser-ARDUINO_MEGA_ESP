
/**********************************  INCLUDES  ***********************************/
#include <AccelStepper.h>    //Libreria para controlar los motores paso a paso
/********************************************************************************/

/**********************************  DEFINES  ***********************************/
#define PIN_LED_NOT_TOMA   49
#define PIN_ZUMB_NOT_TOMA  52
#define PIN_LED_WIFI_OK    47
#define PIN_BTN_CONF       51
#define PIN_IR_TOMA        53
#define HALFSTEP           8 //Half-step mode (8 step control signal sequence)

//Define para los pines del MOTOR PASO A PASO 1
#define motor1Pin1         2     // IN1 on the ULN2003 driver 1
#define motor1Pin2         3     // IN2 on the ULN2003 driver 1
#define motor1Pin3         4     // IN3 on the ULN2003 driver 1
#define motor1Pin4         5     // IN4 on the ULN2003 driver 1

//Define para los pines del MOTOR PASO A PASO 2
#define motor2Pin1         8     // IN1 on the ULN2003 driver 2
#define motor2Pin2         9     // IN2 on the ULN2003 driver 2
#define motor2Pin3         10     // IN3 on the ULN2003 driver 2
#define motor2Pin4         11     // IN4 on the ULN2003 driver 2
/********************************************************************************/

/*********************************  CONSTANTES  *********************************/
//Cada movimiento del motor paso a paso para que de 13 vueltas en los 360º
const int pasos = 315;

//COMANDOS DE EJECUCION EN ESP8266
//Motor a llegado a su posicion y ha dispensado las pastillas correspondientes
const String motorPastillasDispensadas1 = "[MOTORDISPENSADO-1]";
const String motorPastillasDispensadas2 = "[MOTORDISPENSADO-2]";
const String botonConfirmacionPulsado = "[PULSABTN-1]";
const String botonEmergenciaPulsado = "[PULSABTN-2]";
const String sensorIrDetectado = "[IRDETECCION-1]";

//COMANDO RECIBIDOS DE ESP8266
const String moverPastillero = "MPAS";
const String wifiok = "WIFIOK";
const String medicacionTomada = "MEDTOMADA";
/********************************************************************************/

/*********************************  VARIABLES  **********************************/
// Initialize with pin sequence IN1-IN3-IN2-IN4 for using the AccelStepper with 28BYJ-48
AccelStepper stepper1(HALFSTEP, motor1Pin1, motor1Pin3, motor1Pin2, motor1Pin4);
AccelStepper stepper2(HALFSTEP, motor2Pin1, motor2Pin3, motor2Pin2, motor2Pin4);

//Variables de control
bool movPas1;
bool movPas2;
int oldStateBtnConf;
int oldStateIrConf;

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
  pinMode(PIN_IR_TOMA, INPUT);

  //Inicializar pines
  digitalWrite(PIN_LED_NOT_TOMA, LOW);
  digitalWrite(PIN_ZUMB_NOT_TOMA, LOW);
  digitalWrite(PIN_LED_WIFI_OK, LOW);

  //Inicializar motor paso a paso 1
  stepper1.setMaxSpeed(500.0);
  stepper1.setAcceleration(100.0);
  stepper1.setSpeed(200);
  //Inicializar motor paso a paso 2
  stepper2.setMaxSpeed(500.0);
  stepper2.setAcceleration(100.0);
  stepper2.setSpeed(200);

  //Inicializacion de variables de control
  movPas1 = false;
  movPas2 = false;
  oldStateBtnConf = LOW;
  oldStateIrConf = HIGH;
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
        
        //Mover PASTILLERO 1
        //Comando completo "[MPAS-1-3]"
        if (value.toInt() == 1) {
          movPas1 = true;
          stepper1.move(pasos * numPastillas.toInt());
        }
        //Mover PASTILLERO 2
        //Comando completo "[MPAS-2-3]"
        else if (value.toInt() == 2) {
          movPas2 = true;
          stepper2.move(pasos * numPastillas.toInt());
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
  if (movPas1) {
    if (stepper1.distanceToGo() == 0) {
      stepper1.stop();
      movPas1 = false;
      Serial3.print(motorPastillasDispensadas1);
    } else {
      stepper1.run();
    }
  }
  if (movPas2) {
    if (stepper2.distanceToGo() == 0) {
      stepper2.stop();
      movPas2 = false;
      Serial3.print(motorPastillasDispensadas2);
    } else {
      stepper2.run();
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
