#include <WiFi.h>
#include <FirebaseESP32.h>
#include <DHT.h>
#include <ThingSpeak.h>

//wifi
#define WIFI_SSID "Thanglee"
#define WIFI_PASSWORD "06062002"

//thingspeak
#define SECRET_CH_ID 2235175
#define SECRET_WRITE_APIKEY "M6IJG6P8LPJSZ0KF"
unsigned long myChannelNumber = SECRET_CH_ID;
const char * myWriteAPIKey = SECRET_WRITE_APIKEY;

WiFiClient  client;

//firebase
#define DATABASE_URL "project-vixuly-default-rtdb.firebaseio.com"
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

//pin define
#define DHTPIN 18 //digital
#define MQ7_A 35 // analog
#define ledPower 13 //digital
#define dustPin 34 // annalog

#define DHTTYPE DHT11 
DHT dht(DHTPIN, DHTTYPE); 

//khai bao bien
volatile float pmValue, coValue, totalPm, totalCo, pmAvg, coAvg, aqi,e=2.71828;
unsigned long sysTime = 0;
unsigned long coMeasureTimer = 0;
unsigned long pmMeasureTimer = 0;
unsigned long loopTimer = 0;

unsigned long coMeasureDelay = 5000; // Set your desired delay in milliseconds
unsigned long lastCOMeasureTime = 0;

unsigned long pmMeasureDelay = 5000; // Set your desired delay in milliseconds
unsigned long lastPMMeasureTime = 0;

unsigned long dhtMeasureDelay = 5000; // Set your desired delay in milliseconds
unsigned long lastDHTMeasureTime = 0;

volatile int count;
String myStatus = "";
String myStatus2 = "";
//pm25
int delayTime = 280;
int delayTime2 = 40;
float offtime = 9680;

int dustVal = 0;
float voltage = 0;
float dustdensity = 0;
float t = 0, h = 0;


//khai bao ham
void logWiFi();
void measurePm2_5();
void measure_Co();
void measure_CO2();
void measure_dht11();
void calAqi();

void setup() {
  Serial.begin(9600);  // khởi tạo giao tiếp với tốc độ truyền dữ liệu 9600bit/s
  pinMode(ledPower, OUTPUT); // đặt chế độ ledPower là OUTPUT
  pinMode(dustPin, INPUT);  //
  pinMode(MQ7_A, INPUT);

   logWiFi();
  //firebase
  ThingSpeak.begin(client);  // khởi tạo kết nối tới dịch vụ firebase với tham số client

  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);  // in ra thông báo trên cổng Serial với phiên bản của Firebase Client.
  config.database_url = DATABASE_URL;  
  config.signer.test_mode = true;

  Firebase.reconnectWiFi(true);  
  Firebase.begin(&config, &auth);

}

void loop() {
  unsigned long currentMillis = millis();
  if((currentMillis - sysTime) > 5000){
    measure_CO();
    measurePm2_5();
    measure_dht11();
    coMeasureTimer = currentMillis;
    count ++;

    if (count >= 50){
      calAqi();
      ThingSpeak.setField(5,aqi);
      ThingSpeak.setStatus(myStatus2);
    int x2 = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey); // ghi du lieu len thinkspeak
    if(x2 == 200){ //
              Serial.println("Channel update successful2.");
            }
            else{
              Serial.println("Problem updating channel. HTTP error code2 " + String(x2));
            }  
      totalCo = 0;
      totalPm = 0;
      count = 0;
  }
  }
  if(currentMillis-pmMeasureTimer>=5000){
    ThingSpeak.setField(2,dustdensity);
     ThingSpeak.setField(1,coValue);
     ThingSpeak.setField(3,t);
     ThingSpeak.setField(4,h);
     ThingSpeak.setStatus(myStatus);
     int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if(x == 200){
    Serial.println("Channel update successful-1.");
  }
  else{
    Serial.println("Problem updating channel. HTTP error code-1 " + String(x));
  }
  pmMeasureTimer = currentMillis;
  }
}

void logWiFi()
{
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(300);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();
}

void measure_CO(){
  unsigned long currentMillis = millis();
  if(currentMillis - lastCOMeasureTime >= coMeasureDelay){
    lastCOMeasureTime = currentMillis;
    float A_value = analogRead(MQ7_A);  // doc tin hieu analog tu cam bien MQ7
    float Volt = A_value/1024*5;  //tinh dien ap tu gia tri analog
    coValue = 3.027*pow(e,(1.0698*Volt));

    Serial.print("coValue: ");
    Serial.print(coValue);
    Serial.println("ppm");
    totalCo += coValue;
    Firebase.setInt(fbdo,"/air/co",coValue);
  }
}

void measurePm2_5(){
  unsigned long currentMillis = millis();

  if ( currentMillis - lastPMMeasureTime >= pmMeasureDelay){
    lastPMMeasureTime = currentMillis;
    digitalWrite(ledPower, LOW); // tat nguon cho den
    delayMicroseconds(delayTime); //cho khoang thoi gian
    dustVal=analogRead(dustPin);  // doc tin hieu
    delayMicroseconds(delayTime2); //
    digitalWrite(ledPower, HIGH); // bat lai nguon dien
    delayMicroseconds(offtime);

    voltage = dustVal*0.0049; // tinh gia tri dien ap
    dustdensity = (0.172*voltage-0.1)*1000.0; 

    if (dustdensity < 0)
    dustdensity =0;
    if(dustdensity > 500)
    dustdensity = 500;

    Serial.print("voltage: ");
    Serial.print(voltage);
    Serial.println("V");
    Serial.print("dustdensity: ");
     Serial.print (dustdensity);
    Serial.println("ug/m3");
    totalPm +=dustdensity;
    Firebase.setFloat(fbdo,"/air/pm",dustdensity);
  }
}

void measure_dht11(){
  unsigned long currentMillis = millis();

  if ( currentMillis - lastDHTMeasureTime >= dhtMeasureDelay){
    lastDHTMeasureTime = currentMillis;

   h = dht.readHumidity();  // doc gia tri do am 
   t = dht.readTemperature();  // Đọc nhiệt độ theo độ C

    if (isnan(h) || isnan(t)) {  // kiem tra xem du lieu doc co hop le khong
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
    Serial.print("Nhiet do: ");
    Serial.print(t);
    Serial.print("*C  ");
    Serial.print("Do am: ");
    Serial.print(h);
    Serial.println("%  ");

    Firebase.setFloat(fbdo,"/air/Nhiet do",t);
    Firebase.setFloat(fbdo,"/air/Do am",h);
    Serial.println("-------------------------------------");
  }

}
void calAqi(){
  pmAvg = totalPm/count;
  coAvg = totalCo/count;
  float pmAqi = pmAvg*2.103004+25.553648;
  float coAqi = coAvg*16.89655-59.51724;
  delay(10);
  if(pmAqi > coAqi) aqi = pmAqi;
  else aqi = coAqi;
  Serial.print("AQI: ");
  Serial.print(aqi);
  Firebase.setFloat(fbdo,"/air/aqi",aqi);
  }












