// Bibliotecas necessáiras
#include "WiFi.h"
#include <FirebaseESP32.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Definição de váriaveis
#define DS18B20 21
#define Sensor_Umidade 35
#define local "/motor/bool"
#define MOTOR 22
#define LED 5
#define POSICAO 4

// Definições necessárias usar o procolo com o sensor
OneWire ourWire(DS18B20);
DallasTemperature sensors(&ourWire); 

// Variaveis do WiFi
const char* ssid = "REDE_WIFI"; // Nome do Wifi
const char* pass = "SENHA_WIFI";  // Senha do Wifi

// Variaveis que serão enviadas para o banco de dados
float tem;
int hum;
int pos;

// Variaveis utilizadas para determinar a data e hora
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
const long gmtOffset_sec = -4 * 60 * 60;

// Variaveis do Firebase
#define FIREBASE_HOST "URL_DO_BANCO_DE_DADOS.firebaseio.com" // Database URL
#define FIREBASE_AUTH "APIKEY_DO_APP_FIREBASE" // apiKey
FirebaseData firebaseData;
FirebaseJson json;
String epochTime;
String path;

// Setup
void setup() {
  // Inicialização do monitor serial
  Serial.begin(115200);
  // Inicialização do sensor
  sensors.begin();
  // Definição de saida para ligar o motor
  pinMode(MOTOR, OUTPUT);
  // LED de sinalização e para ativar o ENABLE do A4988
  pinMode(LED, OUTPUT);
  //Sensor de curto, para saber se o balde está na posição correta
  pinMode(POSICAO, INPUT);
  // Inicialização do wifi
  Serial.println("Conectando a rede:");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  while(WiFi.status() != WL_CONNECTED){
    delay(250);
    Serial.print(".");
  }
  Serial.println("Conectado!");
  Serial.println(WiFi.localIP());;

  // Inicializando o NPT
  timeClient.begin();
  timeClient.setTimeOffset(gmtOffset_sec);

  // Inicializando o Firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
}

// Loop
void loop() {
  // Ligar o LED para sinalizar que está tudo certo, e desativar o ENABLE
  digitalWrite(LED, HIGH);  
  // Validazação do WIFI
  while(WiFi.status() != WL_CONNECTED){
    delay(250);
    Serial.print(". - . ");
  }
  
  // Atualização do time
  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }
  // Leitura para ver se o balde está na posição correta
  pos=digitalRead(POSICAO);
  // Caso esteja na posição errada, o motor é ativado até que chegue na posição correta
  if(pos == LOW){
      // Ativando o ENABLE, energizando o motor
      while(pos == LOW){
        digitalWrite(LED, LOW);
        // Passos para a rotação do motor de passo
        for(int i=0; i<500; i++){
                  digitalWrite(MOTOR, HIGH);
                  delayMicroseconds(500);
                  digitalWrite(MOTOR, LOW);
                  delayMicroseconds(500);
        }
        // Leitura para ver se o balde está na posição correta
      pos=digitalRead(POSICAO); 
      }
    }else{
        // Informação descrita no monitor serial
        Serial.println("Posição correta");
        // Desativando o ENABLE, desernegizando o motor
        digitalWrite(LED, HIGH);
      }

  // Comnado para ler a temperatura
  sensors.requestTemperatures();
  // Armazenando temperatura, umidade e data/hora nas variaveis
  tem = sensors.getTempCByIndex(0);
  hum = 4095-(analogRead(Sensor_Umidade));
  epochTime = timeClient.getEpochTime();
  // Local onde será armazenado no FIREBASE
  path = "/data/T-" + epochTime;

  // Validação se não está tendo erro na leitura
  if( isnan(tem) || isnan(hum) ){
    Serial.println("----------------  ERROR ----------------");
    Serial.println("Error: sensor");
    Serial.println("--------------------------------");
    
    // Enviar erro
    sendError(epochTime, 111);
  } else {
    
    // Adiciona os valores das variaveis no FIREBASE
    json.clear();
    json.add("tem", tem);
    json.add("hum", hum);
    json.add("time", timeClient.getFormattedDate());

    // Confirmação que os dados foram enviados
    if (Firebase.setJSON(firebaseData, path, json)) {
      Serial.println("ENVIADO COM SUCESSO");
      // Pausa de 5 minutos
      delay(1000*60*5);
        
    } else {
        Serial.println("FALHA");
        Serial.println("MOTIVO: " + firebaseData.errorReason());
        Serial.println("------------------------------------");
        Serial.println();
    }
  }

  // Para rotacionar o balde a partir da leitura do FIREBASE
  if (Firebase.ready()){
    Serial.println("Efetuando a Leitura");
    // Varaivel que indica se precisa ou não rotacionar
    bool bVal;
    // Adiciona o valor que está no FIREBASE na variavel
    Firebase.getBool(firebaseData, local, &bVal);
    // Condição para não rotacionar o motor
    if (bVal){
         digitalWrite(LED, HIGH);
         Serial.println("Motor precisa estar desligado");
      }
    // Condição para rotacionar o motor
    else{
        // Altera o valor que está salvo no FIREBASE
        Firebase.setBool(firebaseData, local, "true");
        // Ativando o ENABLE, energizando o motor
        digitalWrite(LED, LOW);
        Serial.println("Motor Precisa ligar");
        // Passos para a rotação do motor de passo
        for(int j=0; j<3; j++){
        for(unsigned int i=0; i<50000; i++){
                digitalWrite(MOTOR, HIGH);
                delayMicroseconds(500);
                digitalWrite(MOTOR, LOW);
                delayMicroseconds(500);
          }
        }
      }
    Serial.println("Finalizando a Leitura");
  }
  // Pausa de 30 segundos
  delay(30000);
}

// Função de erro no FIREBASE
void sendError(String epochTime, int codeError) {
  String pathError = "error/T-" + epochTime;
  FirebaseJson jsonError;
  jsonError.clear();
  jsonError.add("Código", codeError);
  jsonError.add("epochTime", epochTime);

  switch (codeError) {
    case 111:
      jsonError.add("msj", "NaN valor na temperatura ou umidade");
      break;
    default:
      Serial.println("Erro não encontrado");
      break;
  }

  if (Firebase.setJSON(firebaseData, pathError, jsonError)) {
      Serial.println("Erro enviado com sucesso");
      // Pausa de 1 minuto
      delay(1000*60*1);
        
    } else {
        Serial.println("Falha ao enviar erro");
        Serial.println("Razão: " + firebaseData.errorReason());
        Serial.println("------------------------------------");
        Serial.println();
        delay(1000);
    }
}