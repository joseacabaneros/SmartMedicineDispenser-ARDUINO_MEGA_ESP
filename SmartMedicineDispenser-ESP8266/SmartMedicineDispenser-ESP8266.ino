
/**********************************  INCLUDES  ***********************************/
#include <ESP8266WiFi.h>    //WiFI ESP8266
#include "RestClient.h"     //Peticiones REST contra el servidor NodeJS
#include <ArduinoJson.h>    //Parser JSON
#include <NTPClient.h>      //Network Time Protocol Cliente. Obtener fecha y hora
#include <WiFiUdp.h>        //User Datagram Protocol. Necesario para NTPClient
/********************************************************************************/

/*********************************  CONSTANTES  *********************************/
//Serial Caja de Medicamentos
const String serial = "NZHH3-DHK2W-8SX7I-SESB3-7RLDP";
const int numPastilleros = 2;

//Datos de conexion al WiFi
/*
const char* ssid = "JAZZTEL_U9rS";
const char* password = "ku7j7br5phzx";
*/
const char* ssid = "AndroidAP";
const char* password = "abcd1234";

//Datos de conexion a la API REST
const char* host = "smart-medicine-dispenser.herokuapp.com";
//const char* host = "156.35.98.12";
const String routeBase = "/api";
const String routeGetHorarios = "/schedule";
const String routeUpdateHorario = "/update/action";
const String routeUpdatePosicion = "/update/position";
const String routeNotificacionNoTomada = "/notification/notake";
const String routeNotificacionEmergencia = "/notification/emergency";

//Segundos para la siguiente llamada a la API de consulta de horarios
const int llamadasCada = 60;

//COMANDOS DE EJECUCION EN ATmega2560
//Comando completo "[MPAS-A-3]" donde A es el pastillero y 3 la cantidad
//de pastillas a tomar (numero de pasos del respectivo pastillero)
const String moverPastilleroA = "[MPAS-A";
const String moverPastilleroB = "[MPAS-B";
const String wifiOk = "[WIFIOK-1]";     //Comando de confirmacion de conexion WiFi
//Comando para solicitar la temperatura y la humedad del dispensador
const String solicitarTempHum = "[SOLICTEMHUM-1]";
//Comando completo "[LEDNOT-0]" donde 0 es que debe apagarse y 1 encenderse
const String ledNotificacion = "[LEDNOT-";
//Comando completo "[ZUMNOT-0]" donde 0 es que debe apagarse y 1 encenderse
const String zumabdorNotificacion = "[ZUMNOT-";

//COMANDO RECIBIDOS DE ATmega2560
const String motorPastillasDispensadas = "MOTORDISPENSADO";
const String botonPulsado = "PULSABTN";
const String irDeteccion = "IRDETECCION";
const String tempHumeCalor = "TEMPHUMCALOR";
const String gasDeteccion = "GASDETECCION";
const String vibDeteccion = "VIBDETECCION";
/********************************************************************************/

/*********************************  VARIABLES  **********************************/
//Get fecha y hora actual de Espana
WiFiUDP ntpUDP;
//+2 horas (horario verano) +1 hora (no horario verano)
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 7200, 60000);

long controMinutoApi;
long timeProgramacionRecibida;
String comandoEvento;

//Variables de API REST
RestClient client = RestClient(host); //Crear instancia para realizar API REST
//RestClient client = RestClient(host, 8081); //Crear instancia para realizar API REST

//Horarios programados para ahora
//Objetivo: Controlar los minutos siguientes a la programacion para comprobar
//  que se toma la medicacion y actuar en consecuencia
//  - Tomada: Sensor IR y/o boton de confirmacion [configurables]
//  - No tomada: Notificacion de no toma
//i = 0 => id(id de mongo), unixtimetoma, pastillero=A, pastillas
//i = 1 => id(id de mongo), unixtimetoma, pastillero=B, pastillas
String horarios[numPastilleros][4];
int numHorarios;
//Configuraciones horarios (tiempoEspera) y se usa las variables btnConfirmacion e irToma 
//para la configuracion del btn de confirmacion y del ir respectivamente
int tiempoEspera = 300;       //Segundos de margen para permitir la toma de la medicacion

boolean necesariaToma;
boolean btnConfirmacion;
boolean irToma;

//Temporales de temperatura, humedad e indice de calor
float tempTempe;
float tempHumed;
float tempCalor;

//Temporales de gas y vibracion
bool tempGas;
bool tempVib;

//Segundos que debe emitir sonido el zumbador de notificacion
int segZumbador = 0;
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
  tempGas = false;
  tempVib = false;

  //Solicitar temperatura, humedad y calor
  Serial.println(solicitarTempHum);                   //CODIGO DE EJECUCION EN ATMEGA
  //Esperar 5 segundos a recoger informacion del temperatura y humedad 
  delay(5000);  
}

/********************************************************************************/

void loop() {
  //Eventos surgidos del Serial1 (ATmega2560)
  serial1Event();

  //Rutina llamas a la API para comprobar horarios de medicacion
  //Comprobaciones cada constante "llamadasCada"
  rutinaApiHorarios();

  //Comprobacion de toma de la medicacion en el margen que marca la configuracion
  //"tiempoEspera" (si la variable necesariaToma, asi lo indica)
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

      /*
      Serial.println("EVENT ATmega2ESP");
      Serial.println("Code: " + code);
      Serial.println("Value: " + value);
      */

      //Evento de MOTOR PASTILLAS YA DISPENSADAS
      if (code.equals(motorPastillasDispensadas)) {
        //MOTOR PASTILLERO A
        if (value == "A") {
          /* Serial.println("Confirmacion motor A pastillas dispensadas"); */

          //Actualizar posicion del pastillero A
          String route = routeBase + routeUpdatePosicion;
          String body = "serial=" + serial + "&pastillero=A&pastillas=" + horarios[0][3];
          
          sendPostToAPI(route, body);
        }
        //MOTOR PASTILLERO B
        else if (value == "B") {
          /* Serial.println("Confirmacion motor B pastillas dispensadas"); */

          //Actualizar posicion del pastillero B
          String route = routeBase + routeUpdatePosicion;
          String body = "serial=" + serial + "&pastillero=B&pastillas=";

          //Si hay programacion para ambos pastilleros A y B
          if(numHorarios == 2){
            body = body + horarios[1][3];
          }else{//Si hay programacion solo para el pastillero B
            body = body + horarios[0][3];
          }
            
          sendPostToAPI(route, body);
        }
      }
      //Evento de PULSACION DE BOTON
      else if (code.equals(botonPulsado)) {
        //Boton de CONFIRMACION de toma pulsado (1)
        //Siempre y cuando sea necesaria toma de medicacion
        if (value.toInt() == 1 && necesariaToma && !btnConfirmacion) {
          /* Serial.println("Confirmacion btn toma"); */
          btnConfirmacion = true;

          //Actualizar boton de confirmacion de los horarios
          for (int i = 0; i < numHorarios; i = i + 1) {
            String route = routeBase + routeUpdateHorario;
            String body = "id=" + horarios[i][0] + "&accion=" + "tomadaBtn";
            sendPostToAPI(route, body);
          }
        }
        //Boton de EMERGENCIA de toma pulsado (2)
        else if (value.toInt() == 2) {
            //Notificacion de emergencia
            String route = routeBase + routeNotificacionEmergencia;
            String body = "serial=" + serial;
            sendPostToAPI(route, body);
        }
        else {
          /* Serial.println("COMANDO ERRONEO O INNECESARIO EN ESTE MOMENTO"); */
        }
      }
      //Evento de DETECCION DE SENSOR IR
      //Siempre y cuando sea necesaria toma de medicacion
      else if (code.equals(irDeteccion) && necesariaToma && !irToma) {
        //IR de DETECCION de toma
        /* Serial.println("Deteccion IR toma"); */
        irToma = true;

        //Actualizar IR de confirmacion de los horarios
        for (int i = 0; i < numHorarios; i = i + 1) {
          String route = routeBase + routeUpdateHorario;
          String body = "id=" + horarios[i][0] + "&accion=" + "tomadaIR";
          sendPostToAPI(route, body);
        }
      }
      //Evento de TEMPERATURA, HUMEDAD Y CALOR
      else if (code.equals(tempHumeCalor)) {
        /* Serial.println("Temperatura, humedad y calor recibidos"); */

        tempTempe = value.toFloat();
        tempHumed = split(comandoEvento, '-', 2).toFloat();
        tempCalor = split(comandoEvento, '-', 3).toFloat();

        /*
        Serial.println("Temperatura: " + String(tempTempe) + " C");
        Serial.println("Humedad: " + String(tempHumed) + "%");
        Serial.println("Indice calor: " + String(tempCalor));
        */
      }
      //Evento de DETECCION DE GAS
      else if (code.equals(gasDeteccion)) {
        /* Serial.println("Deteccion de Gas"); */
        tempGas = true;
      }
      //Evento de DETECCION DE VIBRACION
      else if (code.equals(vibDeteccion)) {
        /* Serial.println("Deteccion de Vibracion"); */
        tempVib = true;
      }
      else {
        /* Serial.println("COMANDO ERRONEO O INNECESARIO EN ESTE MOMENTO"); */
      }

      comandoEvento = "";
    }
  }
}


/* Rutina de llamadas a la API REST para comprobar si toca tomar medicacion */
void rutinaApiHorarios() {
  if (controMinutoApi <= timeClient.getEpochTime()) {
    /* Serial.println("SOLICITUD API HORARIOS"); */
    
    //Solicitar temperatura, humedad y calor
    Serial.println(solicitarTempHum);                   //CODIGO DE EJECUCION EN ATMEGA
    //Sumar "llamadasCada" para realizar la siguiente llamada a la API horarios
    controMinutoApi = timeClient.getEpochTime() + llamadasCada;
    /*Serial.println(controMinutoApi);*/

    String response;
    String queryparams = "?temp=" + String(tempTempe) + "&hume=" + String(tempHumed) 
        + "&icalor=" + String(tempCalor) + "&gas=" + tempGas + "&vibracion=" + tempVib;
    String route = routeBase + routeGetHorarios  + "/" + serial + queryparams;
    /*Serial.println("Requesting http://" + host + route);*/
    int statusCode = client.get(route.c_str(), &response);
    /* Serial.println(statusCode + " " + response); */

    //Reinicializar temporales de gas y vibracion
    tempGas = false;
    tempVib = false;

    //Si el codigo de la respuesta es 200, toma de medicacion
    if (statusCode == 200) {
      timeProgramacionRecibida = timeClient.getEpochTime();
      //Control del 'tiempo de espera' de toma de medicacion (configuracion 'tiempoEspera')
      necesariaToma = true;
      
      StaticJsonBuffer<2000> jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject(response);

      //Comprobar si se ha parseado el JSON correctamente
      if (!root.success()) {
        /*Serial.println("parseObject() failed");*/
        return;
      }

      JsonArray& data = root["Horarios programados"];                //Datos de horarios
      numHorarios = root["Numero horarios"];                         //Numero de horarios
      JsonObject& configuracion = root["Configuracion horarios"][0]; //Configuracion

      //Configuracion de horarios
      //tiempoEspera
      String tespera = configuracion["tespera"];
      tiempoEspera = tespera.toInt();
      
      //irToma=false, si precisa de ser detectado 
      //irToma=true, si no precisa de ser detectado ("se marca directemente como detectado")
      if(configuracion["ir"] == "true"){
        irToma = false;
      }else{
        irToma = true;
      }
      
      //btnConfirmacion=false, si precisa de ser pulsado 
      //btnConfirmacion=true, si no precisa de ser pulsado ("se marca directemente como pulsado")
      if(configuracion["btn"] == "true"){
        btnConfirmacion = false;
      }else{
        btnConfirmacion = true;
      }

      //Configuracion del Dispensador
      //Led de notificacion
      if(configuracion["led"] == "true"){
          Serial.println(ledNotificacion + "1]");                //CODIGO DE EJECUCION EN ATMEGA
      }
      //Sonido de notificacion
      if(configuracion["zumbador"] == "true"){
        String tzumbador = configuracion["tzumbador"];
        segZumbador = tzumbador.toInt();
        Serial.println(zumabdorNotificacion + "1]");             //CODIGO DE EJECUCION EN ATMEGA
      }
      else{
        segZumbador = 0;
      }

      //Horarios
      for (int i = 0; i < numHorarios; i = i + 1) {
        String id = data[i]["id"];
        /* Serial.println(id); */
        int unixTimeTomaInt = data[i]["unixtimetoma"];
        String unixTimeToma = String(unixTimeTomaInt);
        String pastillero = data[i]["pastillero"];
        String pastillas = data[i]["pastillas"];

        horarios[i][0] = id;
        horarios[i][1] = unixTimeToma;
        horarios[i][2] = pastillero;
        horarios[i][3] = pastillas;

        if (pastillero == "A") {
          //Comando completo "[MPAS-A-3]"
          Serial.println(moverPastilleroA + "-" + pastillas + "]");    //CODIGO DE EJECUCION EN ATMEGA
        }
        else if (pastillero == "B") {
          //Comando completo "[MPAS-B-3]"
          Serial.println(moverPastilleroB + "-" + pastillas + "]");    //CODIGO DE EJECUCION EN ATMEGA
        }
      }
    }
  }
}

/* Comprobacion de toma de la medicacion en el margen que marca la configuracion "tiempoEspera" */
void comprobacionToma() {
  if (necesariaToma) {
      //Zumbador activo
      if(segZumbador > 0){
        //Si cumlple la condicion, apagar zumbador (fin de los segundos marcados en la configuracion)
        if (timeClient.getEpochTime() > (timeProgramacionRecibida + segZumbador)) {
          Serial.println(zumabdorNotificacion + "0]");                  //CODIGO DE EJECUCION EN ATMEGA
          segZumbador = 0;
        }
      }
    
    //Si el boton de confirmacion y/o el sensor IR ha/n sido pulsado/detectado (y/o por configuracion),
    //no es necesaria notificacion ya que se presupone que se ha tomado la medicacion
    if (btnConfirmacion && irToma) {
      resetearNecesariaToma();
      /* Serial.println("Medicacion tomada correctamente"); */
    }
    else {
      //Sobrepasado tiempo de margen de toma de medicacion -> NOTIFICACION MEDICACION NO TOMADA
      if (timeClient.getEpochTime() > (timeProgramacionRecibida + tiempoEspera)) {
        for (int i = 0; i < numHorarios; i = i + 1) {
          /* Serial.println("Notificacion medicacion no tomada " + horarios[i][2]); */
          
          //Enviar notificacion de medicacion no tomada (una notificacion por pastillero afectado)
          String route = routeBase + routeNotificacionNoTomada;
          String body = "id=" + horarios[i][0];
          sendPostToAPI(route, body);

          //Resetear variables cuando se procese el ultimo horario
          if((i+1) == numHorarios){
            resetearNecesariaToma();
          }
        }
      }
    }
  }
}

/* Resetear variables de necesaria toma */
void resetearNecesariaToma() {
  numHorarios = 0;
  
  necesariaToma = false;
  btnConfirmacion = false;
  irToma = false;

  //Apagar led notificacion y zumbador
  Serial.println(ledNotificacion + "0]");                     //CODIGO DE EJECUCION EN ATMEGA
  Serial.println(zumabdorNotificacion + "0]");                //CODIGO DE EJECUCION EN ATMEGA
  segZumbador = 0;
}

/* Conexion al WiFi */
void conexionWiFi() {
  WiFi.begin(ssid, password);

  //Esperando la conexion
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    /*Serial.print(".");*/
  }

  //Notificar a ATMega que se ha conectado correctamente al WiFi
  Serial.println(wifiOk);                                     //CODIGO DE EJECUCION EN ATMEGA

  /*Serial.println("");
    Serial.println("Conectado a " + ssid);*/
}

/* Enviar peticion POST a la API REST a la routa y con los paramentos que
  marcan los parametros "route" y "body" respectivamente */
void sendPostToAPI(String route, String body) {
  String response;
  client.setHeader("Content-Type: application/x-www-form-urlencoded");
  int statusCode = client.post(route.c_str(), body.c_str(), &response);
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

