
/**********************************  INCLUDES  ***********************************/
#include <ESP8266WiFi.h>    //WiFI ESP8266
#include "RestClient.h"     //Peticiones REST contra el servidor NodeJS
#include <ArduinoJson.h>    //Parser JSON
#include <NTPClient.h>      //Network Time Protocol Cliente. Obtener fecha y hora
#include <WiFiUdp.h>        //User Datagram Protocol. Necesario para NTPClient
/********************************************************************************/

/*********************************  CONSTANTES  *********************************/
//Serial Caja de Medicamentos
const String serial = "yyyyy-yyyyy-yyyyy-yyyyy-yyyyy";

//Datos de conexion al WiFi
const char* ssid = "JAZZTEL_U9rS";
const char* password = "ku7j7br5phzx";

//Datos de conexion a la API REST
const char* host = "156.35.98.12";
const String routeBase = "/api";
const String routeGetHorarios = "/horarios";
const String routeUpdateHorario = "/update";
const String routeNotificacion = "/notify";

//Segundos para la siguiente llamada a la API de consulta de horarios
const int llamadasCada = 60;

//Segundos de margen para permitir la toma de la medicacion
const int margenToma = 300;       //5 min

const size_t bufferSize = + 120;

//COMANDOS DE EJECUCION EN ATmega2560
//Comando completo "[MPAS-1-3]" donde 1 es el numero de pastillero y 3 la cantidad
//de pastillas a tomar (numero de pasos del respectivo pastillero)
const String moverPastillero1 = "[MPAS-1";
const String moverPastillero2 = "[MPAS-2";

//COMANDO RECIBIDOS DE ATmega2560
const String botonPulsado = "PULSABTN";
const String irDeteccion = "IRDETECCION";
/********************************************************************************/

/*********************************  VARIABLES  **********************************/
//Get fecha y hora actual de Espana
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);

//Memoria dinamica para el Buffer del parser JSON
DynamicJsonBuffer jsonBuffer(bufferSize);

long controMinutoApi;
String comandoEvento;

//Variables de API REST
RestClient client = RestClient(host, 8081); //Crear instancia para realizar API REST
String route;
String response;
int statusCode;

//Horarios programados para ahora
//Objetivo: Controlar los minutos siguientes a la programacion para comprobar
//  que se toma la medicacion y actuar en consecuencia
//  - Tomada: Sensor IR y boton de confirmacion
//  - No tomada: Notificacion de no toma
//i = 0 => id(id de mongo), unixtimetoma, pastillero=1, pastillas(numero)
//i = 1 => id(id de mongo), unixtimetoma, pastillero=2, pastillas(numero)
String horarios[2][4];
int numHorarios;

boolean necesariaToma;
boolean btnConfirmacion;
boolean irToma;
/********************************************************************************/

void setup() {
  Serial.begin(115200);

  //Esperar que se conecte el puerto serie. Necesario solo para el puerto USB nativo
  while (!Serial) { }

  //Conectar a WiFi
  conexionWiFi();

  //Inicializar fecha y hora de NTP
  timeClient.begin();
  timeClient.update();

  //Control de llamadas a la API REST (uso de UNIX time, segundos desde 1970)
  controMinutoApi = timeClient.getEpochTime();

  //Inicializaicion de variables
  necesariaToma = false;
  btnConfirmacion = false;
  irToma = false;
}

/********************************************************************************/

void loop() {
  //Eventos surgidos del Serial1 (ATmega2560)
  serial1Event();

  //Rutina llamas a la API para comprobar horarios de medicacion
  //Comprobaciones cada constante "llamadasCada"
  rutinaApiHorarios();

  //Comprobacion de toma de la medicacion en el margen que marca la constante
  //"margenToma" (si la variable necesariaToma, asi lo indica)
  comprobacionToma();
}

/********************************************************************************/


/* Verificar el evento en el puerto Serial1 (ATmega2560) */
void serial1Event() {
  while (Serial.available()) {
    char inChar = Serial.read(); //Leer datos del puerto Serial
    comandoEvento += inChar;

    //Encuentra el comando en los datos recibidos (el comando debe estar entre corchetes)
    if (inChar == ']') {
      //Salida de los datos de lectura al puerto Serial
      /*Serial.println(comandoEvento);*/

      comandoEvento = comandoEvento.substring(1);         //Borrar '[' del codigo
      comandoEvento.remove(comandoEvento.length() - 1);   //Borrar ']' del valor

      String code = split(comandoEvento, '-', 0);
      String value = split(comandoEvento, '-', 1);

      Serial.println("EVENT ATmega2ESP");
      Serial.println("Code: " + code);
      Serial.println("Value: " + value);

      //EVENTO DE PULSACION DE BOTON
      //Siempre y cuando sea necesaria toma de medicacion
      if (code.equals(botonPulsado) && necesariaToma && !btnConfirmacion) {
        //Boton de CONFIRMACION de toma pulsado (1)
        if (value.toInt() == 1) {
          Serial.println("Confirmacion btn toma");
          btnConfirmacion = true;

          Serial.println("Numero horarios: " + numHorarios);

          //Actualizar boton de confirmacion de los horarios
          for (int i = 0; i < numHorarios; i = i + 1) {
            route = routeBase + routeUpdateHorario;
            String body = "id=" + horarios[i][0] + "&accion=" + "tomadaBtn";
            client.setHeader("Content-Type: application/x-www-form-urlencoded");
            statusCode = client.post(route.c_str(), body.c_str(), &response);

            //Resetear statusCode
            statusCode = 0;
          }
        }
        //Boton de EMERGENCIA de toma pulsado (2)
        else if (value.toInt() == 2) {

        }
      }
      //EVENTO DE DETECCION DE SENSOR IR
      //Siempre y cuando sea necesaria toma de medicacion
      else if (code.equals(irDeteccion) && necesariaToma) {
        //IR de DETECCION de toma (1)
        if (value.toInt() == 1) {
          Serial.println("Deteccion ir toma");
          irToma = true;


        }
      }
      else {
        Serial.println("COMANDO ERRONEO!!");
      }

      comandoEvento = "";
    }
  }
}


/* Rutina de llamadas a la API REST para comprobar si toca tomar medicacion */
void rutinaApiHorarios() {
  if (controMinutoApi <= timeClient.getEpochTime()) {
    //Sumar "llamadasCada" para realizar la siguiente llamada a la API horarios
    controMinutoApi = timeClient.getEpochTime() + llamadasCada;
    /*Serial.println(controMinutoApi);*/

    route = routeBase + routeGetHorarios  + "/" + serial;
    /*Serial.println("Requesting http://" + host + route);*/
    statusCode = client.get(route.c_str(), &response);
    /*Serial.println(statusCode + " " + response);*/

    //Si el codigo de la respuesta es 200, toma de medicacion
    if (statusCode == 200) {
      JsonObject& root = jsonBuffer.parseObject(response);

      //Comprobar si se ha parseado el JSON correctamente
      if (!root.success()) {
        /*Serial.println("parseObject() failed");*/
        return;
      }

      JsonArray& data = root["Horarios programados"]; //Datos de horarios
      numHorarios = root["Numero horarios"];      //Numero de horarios

      for (int i = 0; i < numHorarios; i = i + 1) {
        String id = data[i]["id"];
        int unixTimeTomaInt = data[i]["unixtimetoma"];
        String unixTimeToma = String(unixTimeTomaInt);
        String pastillero = data[i]["pastillero"];
        String pastillas = data[i]["pastillas"];

        //Control del margen de toma de medicacion (const margenToma)
        necesariaToma = true;
        btnConfirmacion = false;
        irToma = false;

        horarios[i][0] = id;
        horarios[i][1] = unixTimeToma;
        horarios[i][2] = pastillero;
        horarios[i][3] = pastillas;

        if (pastillero.toInt() == 1) {
          //Comando completo "[MPAS-1-3]"
          Serial.println(moverPastillero1 + "-" + pastillas + "]");    //CODIGO DE EJECUCION EN ATMEGA
        }
        else if (pastillero.toInt() == 2) {
          //Comando completo "[MPAS-2-3]"
          Serial.println(moverPastillero2 + "-" + pastillas + "]");    //CODIGO DE EJECUCION EN ATMEGA
        }
      }
    }
  }
}

/* Comprobacion de toma de la medicacion en el margen que marca la constante "margenToma" */
void comprobacionToma() {
  if (necesariaToma) {
    //Si tanto el boton de confirmacion como el sensor IR han sido pulsado/detectado,
    //no es necesaria notificacion ya que se presupone que se ha tomado la medicacion
    if (btnConfirmacion && irToma) {
      resetearNecesariaToma();
    }
    else {
      for (int i = 0; i < numHorarios; i = i + 1) {
        //Sobrepasado tiempo de margen de toma de medicacion => notificacion
        if (timeClient.getEpochTime() > (horarios[i][1].toInt() + margenToma)) {
          resetearNecesariaToma();
          Serial.println("ENVIAR NOTIFICACION A USUARIOS DE NO TOMA (API-REST)");
        }
      }
    }
  }
}

/* Resetear variables de necesaria toma */
void resetearNecesariaToma() {
  necesariaToma = false;
  btnConfirmacion = false;
  irToma = false;
}

/* Conexion al WiFi */
void conexionWiFi() {
  WiFi.begin(ssid, password);

  //Esperando la conexion
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    /*Serial.print(".");*/
  }

  /*Serial.println("");
    Serial.println("Conectado a " + ssid);*/
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

