
/**********************************  INCLUDES  ***********************************/
#include <AccelStepper.h>    //Libreria para controlar los motores paso a paso
/********************************************************************************/

/**********************************  DEFINES  ***********************************/
#define PIN_LED 49
#define PIN_BTN_CONF 51
#define HALFSTEP 8 //Half-step mode (8 step control signal sequence)

//Define para los pines del MOTOR PASO A PASO 1
#define motorPin1  2     // IN1 on the ULN2003 driver 1
#define motorPin2  3     // IN2 on the ULN2003 driver 1
#define motorPin3  4     // IN3 on the ULN2003 driver 1
#define motorPin4  5     // IN4 on the ULN2003 driver 1
/********************************************************************************/

/*********************************  CONSTANTES  *********************************/
//Cada movimiento del motor paso a paso para que de 13 vueltas en los 360º
const int pasos = 315;

//COMANDOS DE EJECUCION EN ESP8266
const String botonConfirmacionPulsado = "[PULSABTN-1]";
const String botonEmergenciaPulsado = "[PULSABTN-2]";

//COMANDO RECIBIDOS DE ATmega2560
const String moverPastillero = "MPAS";
/********************************************************************************/

/*********************************  VARIABLES  **********************************/
// Initialize with pin sequence IN1-IN3-IN2-IN4 for using the AccelStepper with 28BYJ-48
AccelStepper stepper1(HALFSTEP, motorPin1, motorPin3, motorPin2, motorPin4);

//Variables de control
bool movPas1;
bool movPas2;
bool onlyOne;

String stringRecibido;
/********************************************************************************/

void setup() {
  //Inicializar puertos y salidas
  Serial.begin(115200); //Atmega2560
  Serial3.begin(115200); //ESP8266

  //Esperar que se conecte el puerto serie. Necesario solo para el puerto USB nativo
  while (!Serial3) { }

  //Pin modes
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BTN_CONF, INPUT);

  //Inicializar pines
  digitalWrite(PIN_LED, LOW);

  //Inicializar motor paso a paso 1
  stepper1.setMaxSpeed(1000.0);
  stepper1.setAcceleration(100.0);
  stepper1.setSpeed(100);

  //Inicializacion de variables de control
  movPas1 = false;
  movPas2 = false;
  onlyOne = true;
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

      //EVENTO DE MOVER PASTILLERO
      if (code.equals(moverPastillero)) {
        String numPastillas = split(stringRecibido, '-', 2);
        Serial.println("Pastillas: " + numPastillas);
        
        //Mover PASTILLERO 1
        //Comando completo "[MPAS-1-3]"
        if (value.toInt() == 1) {
          movPas1 = true;
          stepper1.move(pasos * numPastillas.toInt());
          digitalWrite(PIN_LED, HIGH);
        }
        //Mover PASTILLERO 2
        //Comando completo "[MPAS-2-3]"
        else if (value.toInt() == 2) {
          movPas2 = true;
        }
      }
      else {
        Serial.println("Wrong command");
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
    } else {
      stepper1.run();
    }
  }
}


/* Eventos surgidos en el hardware accionable de la caja de medicamentos */
void eventosHardware(){
  if (digitalRead(PIN_BTN_CONF) == 1 && onlyOne) {
    Serial3.print(botonConfirmacionPulsado);
    onlyOne = false;
  }
  else if (digitalRead(PIN_BTN_CONF) == 0 && !onlyOne) {
    onlyOne = true;
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
