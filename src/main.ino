#include <ESP32Servo.h>

//Define os Pinos
#define BUTTON_PIN_2 12     //TROCAR PELA TAG
#define BUTTON_PIN_IN 13    //Botão Interno
#define BUTTON_PIN_OUT 14   //Botão Externo
#define BUZZER_PIN 2        //Buzzer
#define LED_PIN 0           //LED
#define SERVO_PIN_1 4       //Servo Interno
#define SERVO_PIN_2 21      //Servo Externo
#define ECHO_PIN 17         //Echo - Sensor de Presença
#define TRIGGER_PIN 16      //Trig - Sensor de Presença
Servo servoIN;
Servo servoOUT;

//LOG CLASS

//Enumeração dos Métodos
enum metods {
  ADMIN = 0,
  TAG = 1,
  WIRELESS = 2
};

//Classe
class Log{
  private:
  String Data;
  String Login;
  int Method;

  public:
  //Constructor
  Log(String login, int method) {
    this->Login = login;
    this->Method = method;
    this->Data = "Log Data"; 
  }

  //Getters 
  String getData() {
    return this->Data;
  }

  String getLogin() {
    return this->Login;
  }

  int getMethod() {
    return this->Method;
  }
};

//Lista de Logs
//TO DO

//Define as Funções
void Lights();
void Doors();
Log createLogger();

void setup() {
  Serial.begin(115200);

  //Define as Entradas e Saidas e coloca o buzzer e o led como desligado
  pinMode(BUTTON_PIN_IN, INPUT);
  pinMode(BUTTON_PIN_2, INPUT);
  pinMode(BUTTON_PIN_OUT, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(TRIGGER_PIN, OUTPUT);
  
  servoIN.attach(SERVO_PIN_1);
  servoOUT.attach(SERVO_PIN_2);
  
  digitalWrite(BUZZER_PIN, LOW);  // Buzzer off
  digitalWrite(LED_PIN, LOW);     // LED off
}

void loop() { 
  Lights();
  Doors();
}

void Lights(){
  //TO DO
}

void Doors(){
  //TO DO
}

Log createLogger() {
  //TO DO
  Log result("Teste", 0);  // Create a Log object with "Teste" as Login and 0 as Method
  return result;
}