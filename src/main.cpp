#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>
#include <vector>
#include <PubSubClient.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>
#include <WiFiClientSecure.h>
#include<ArduinoJson.h>
#include<ESP32Servo.h>

// Define OLED parameters
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define BUZZER 5
#define LED_1 15
#define PB_CANCEL 14
#define PB_OK 33  // blue
#define PB_UP 32  //yellow
#define PB_DOWN 35  //black
#define DHTPIN 12
#define LDR 34 
#define SERVO_PIN 13


#define NTP_SERVER     "pool.ntp.org"

#define UTC_OFFSET     0
#define UTC_OFFSET_DST 0
#define MQTT_SERVER "3.74.203.170"

// Declare objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;


// Global variables
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;
int sample_send_period = 30;
int sample_interval = 5;
int ldr_val_amount = sample_send_period/sample_interval;
int current_sample_min = 0;
int current_sample_sec = 0;
int current_sample_hr = 0;
float temp = 0;
float intensity = 0;
int theta_offset =30;
float control_factor = 0.75;
float t_med = 30;

int sample = 0;
std::vector<int> samples(ldr_val_amount, 0);

bool alarm_enabled[]= {false,false};
int n_alarms = 2;
int alarm_hours[] = {0,1};
int alarm_minutes[] ={1,10};
bool alarm_triggered[] = {false,false};
bool alarm_snooze[]={false,false};

int n_notes = 8;
int C = 262;
int D = 280;
int E = 325;
int G = 345;
int H = 368;
int K = 393;
int J = 421;
int F = 443;

int notes[] = { C,D,E,G,H,K,J,F};

int current_mode = 0;
int max_modes = 6;
String modes[]={"1-Set Time Zone","2-Set Alarm 1","3-Set Alarm 2","4-View Alarms","5-Delete Alarm 1","6-Delete Alarm 2"};

struct TimeZone {
    const char* name;
    int utcOffset;  // Offset in seconds
};

// List of available time zones
TimeZone timeZones[] = {
    {"UTC", 0},
    {"New York (EST)", -5 * 3600},
    {"London (GMT)", 0},
    {"Berlin (CET)", 1 * 3600},
    {"Colombo (IST)", (int)(5.5 * 3600)},  // Explicit cast to int
    {"Tokyo (JST)", 9 * 3600},
};


// Store selected timezone index
int selectedZone = 0;

// Function declarations (prototypes)
void print_line(String text, int column, int row, int text_size);
void print_time_now(void);
void update_time(void);
void set_timezone(void);
void view_alarms(void);
void delete_alarm(void);
void ring_alarm(void);
void update_time_with_check_alarm(void);
int wait_for_button_press(void);
void go_to_menu(void);
void set_time(void);
void set_alarm(int alarm);
void run_mode(int mode);
void check_temp(void);
void view_alarm(void);
void set_time_zone(void);
void LDRSUM(void);
float LDRAVG(void);
void MQTT_PUB(float val, String cat);
void control_servo(void);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER, UTC_OFFSET, 60000);

WiFiClient espClient;
PubSubClient client(espClient);

Servo myServo;    //servo

//fuction to reconnect to mqtt broker
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client_" + String(random(1000, 9999));
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe("esp32/config257");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

// function which takes mqtt messages from node red
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.println(topic);

  const size_t capacity = JSON_OBJECT_SIZE(2) + 2 * JSON_ARRAY_SIZE(1) + 20;
  DynamicJsonDocument doc(capacity);

  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.print("JSON parse failed: ");
    Serial.println(error.c_str());
    return;
  }

  if (doc.containsKey("sample_interval")) {
    int new_interval = doc["sample_interval"];
    if (new_interval > 0) {
      sample_interval = new_interval;
      Serial.print("Updated sample_interval to: ");
      Serial.println(sample_interval);
    }
  }
  if (doc.containsKey("sample_send_period")) {
    int new_period = doc["sample_send_period"];
    if (new_period > 0) {
      sample_send_period = new_period;
      Serial.print("Updated sample_send_period to: ");
      Serial.println(sample_send_period);
    }
  }
  if (doc.containsKey("theta_offset")) {
    Serial.println("theta_offset_rec");
    int new_offset = doc["theta_offset"];
    if (new_offset > 0) {
      theta_offset = new_offset;
      Serial.print("Updated theta_offset to: ");
      Serial.println(theta_offset);
      control_servo();
    }
  }
  if (doc.containsKey("control_factor")) {
    Serial.println("c_fac_rec");
    float new_control_factor = doc["control_factor"];
    if (new_control_factor > 0) {
      control_factor = new_control_factor;
      Serial.print("Updated control_factor to: ");
      Serial.println(control_factor);
      control_servo();
    }
  }
  if (doc.containsKey("t_med")) {
    int new_t_med = doc["t_med"];
    if (t_med > 10) {
      t_med = new_t_med;
      Serial.print("Updated t_med to: ");
      Serial.println(t_med);
      control_servo();
    }
  } 
}

void setup_mqtt() {
  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(mqtt_callback);
  reconnect();
}

void setup() {
    
    pinMode(BUZZER, OUTPUT);
    pinMode(LED_1, OUTPUT);
    pinMode(PB_CANCEL, INPUT);
    pinMode(PB_OK, INPUT);
    pinMode(PB_UP, INPUT);
    pinMode(PB_DOWN, INPUT);
    pinMode(DHTPIN, INPUT);
    pinMode(LDR, INPUT);


    dhtSensor.setup(DHTPIN, DHTesp::DHT22);

    Serial.begin(9600);

    
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)){
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
    }


    display.display();
    delay(2000);


    WiFi.begin("Wokwi-GUEST", "", 6);
    while (WiFi.status() != WL_CONNECTED) {
      delay(250);
      display.clearDisplay();
      print_line("Connecting to Wi Fi",0,0,2);
    }
    // Clear the buffer
    display.clearDisplay();
    print_line("Connected to Wi-Fi",0,0,2);
    delay(500);

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);

    
    timeClient.begin();
    
    myServo.attach(SERVO_PIN);
    myServo.write(30);   //Initial Position

    setup_mqtt();


    display.clearDisplay();
    print_line("Welcome to Medibox!", 10, 20, 2);
    delay(200);
    display.clearDisplay();
    go_to_menu();
    update_time();

    current_sample_hr = hours;     // Set up initial reference time for ldr value taking
    current_sample_min = minutes;
    current_sample_sec = seconds;
    
}

void loop() {
    
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
    update_time_with_check_alarm();
    if(digitalRead(PB_OK)==LOW){
      delay(200);
      go_to_menu();
    }
    check_temp();
    LDRSUM();
    if(sample == ldr_val_amount){
        float x = LDRAVG();
        Serial.println(x);
        MQTT_PUB(x,"ldr");
        MQTT_PUB(temp,"temp");
        control_servo();
    }

}

void print_line(String text, int column, int row, int text_size){
    display.setTextSize(text_size);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(column, row); // (column, row)
    display.println(text);
    
    display.display();
}


int wait_for_button_press(){
    while (true) {
        if (digitalRead(PB_UP) == LOW){
            delay(200);
            return PB_UP;
        }

        else if (digitalRead(PB_DOWN) == LOW){
            delay(200);
            return PB_DOWN;
        }

        else if (digitalRead(PB_OK) == LOW){
            delay(200);
            return PB_OK;
        }

        else if (digitalRead(PB_CANCEL) == LOW){
            delay(200);
            return PB_CANCEL;
        }
    }
}

void go_to_menu(){
    while (digitalRead(PB_CANCEL) == HIGH){
        display.clearDisplay();
        print_line(modes[current_mode], 0, 0, 2);

        int pressed = wait_for_button_press();
        if (pressed == PB_UP){
            delay(200);
            current_mode += 1;
            current_mode = current_mode % max_modes;
        }
        else if (pressed == PB_DOWN) {
            delay(200);
            current_mode -= 1;
            if (current_mode<0) {
                current_mode = max_modes-1;
            }
        }
        else if (pressed == PB_OK) {
            delay(200);
            Serial.println(current_mode);
            run_mode(current_mode);
        }
        else if (pressed == PB_CANCEL) {
            delay(200);
            break;
        }           
    }
}

void run_mode(int mode) {
    if (mode == 0) {
        set_time_zone();
    }
    else if(mode ==1|mode==2){
        set_alarm(mode-1);
    }
    else if(mode == 3){
        view_alarm();
    }
    else if(mode==4){
        alarm_enabled[0] = false;
    }
    else if(mode==5){
        alarm_enabled[1] = false;
    }
}


void set_time_zone(){
    int j = 0;
    while (true) {
        display.clearDisplay();
        print_line("Select time Zone: " ,0,0,2 );
        print_line(String(timeZones[j].name) ,0,35,2 );
        
        int pressed = wait_for_button_press();
        if (pressed == PB_UP) {
            delay(200);
            j += 1;
            j = j % 6;
        }
        else if (pressed == PB_DOWN) {
            delay(200);
            j -= 1;
            if (j < 0) {
                j = 5;
            }
        }
        else if (pressed == PB_OK) {
            delay(200);
            selectedZone = j;
            break;
        }
        else if (pressed == PB_CANCEL) {
            delay(200);
            break;
        }
    }
}

void update_time() {
    timeClient.update();

  // Get UNIX timestamp from NTPClient
    unsigned long rawTime = timeClient.getEpochTime();

  // Apply the selected timezone offset
    rawTime += timeZones[selectedZone].utcOffset;

  // Convert to struct tm for hour, minute, etc.
    struct tm *localTime = localtime((time_t*)&rawTime);

    hours = localTime->tm_hour;
    minutes = localTime->tm_min;
    seconds = localTime->tm_sec;
    days = localTime->tm_mday;
}

void print_time_now() {
    display.clearDisplay();
    print_line(String(hours), 0, 0, 2);
    print_line(":", 20, 0, 2);
    print_line(String(minutes), 30, 0, 2);
    print_line(":", 50, 0, 2);
    print_line(String(seconds), 60, 0, 2);
    display.display();
}

void set_alarm(int alarm){
  int temp_hour = alarm_hours[alarm] ;
    
    while (true) {
        display.clearDisplay();
        print_line("Enter hour: " + String(temp_hour), 0, 0, 2);
        
        int pressed = wait_for_button_press();
        if (pressed == PB_UP) {
            delay(200);
            temp_hour += 1;
            temp_hour = temp_hour % 24;
        }
        else if (pressed == PB_DOWN) {
            delay(200);
            temp_hour -= 1;
            if (temp_hour < 0) {
                temp_hour = 23;
            }
        }
        else if (pressed == PB_OK) {
            delay(200);
            alarm_hours[alarm] = temp_hour;
            break;
        }
        else if (pressed == PB_CANCEL) {
            delay(200);
            break;
        }
    }

    int temp_minute = alarm_minutes[alarm];
    while (true) {
        display.clearDisplay();
        print_line("Enter minute: " + String(temp_minute), 0, 0, 2);
    
        int pressed = wait_for_button_press();
        if (pressed == PB_UP) {
            delay(200);
            temp_minute += 1;
            temp_minute = temp_minute % 60;
        }
        else if (pressed == PB_DOWN) {
            delay(200);
            temp_minute -= 1;
            if (temp_minute < 0) {
                temp_minute = 59;
            }
        } 
        else if (pressed == PB_OK) {
            delay(200);
            alarm_minutes[alarm] = temp_minute;
            alarm_enabled[alarm]=true;
            break;
        }
        else if (pressed == PB_CANCEL) {
            delay(200);
            break;
        }
    }
    display.clearDisplay();
    print_line("Alarm is set", 0, 0, 2);
    alarm_enabled[alarm]=true;
}

void ring_alarm(int i){
    display.clearDisplay();
    print_line("Medicine!",0,0,2);
  
    digitalWrite(LED_1, HIGH);
  
    bool break_happened = false;
    
    while(break_happened == false && digitalRead(PB_CANCEL)==HIGH){
      for(int j = 0;j<n_notes;j++){
          if(digitalRead(PB_CANCEL)==LOW){
              delay(200);
              break_happened = true;
              alarm_snooze[i] = false;
              break;
          }
          if(digitalRead(PB_OK)==LOW){
              delay(200);
              alarm_snooze[i] = true;
              break_happened = true;

              alarm_minutes[i] = alarm_minutes[i]+5;
              if(alarm_minutes[i]>= 60){
                  alarm_minutes[i] = alarm_minutes[i]-60;
                  alarm_hours[i] += alarm_hours[i];
              }
              Serial.println("new alarm"+String(alarm_hours[i])+String(alarm_minutes[i]));
              break;
          }
  
          tone(BUZZER,notes[j]);
          delay(500);
          noTone(BUZZER);
          delay(2);
      }
    }
    digitalWrite(LED_1, LOW);
}
  

void update_time_with_check_alarm(void){
    update_time();
    print_time_now();

    if(alarm_enabled[0]== true){
        if(alarm_triggered[0]==false && alarm_hours[0]==hours && alarm_minutes[0]==minutes){
            ring_alarm(0);
            if(alarm_snooze[0] == false){
                alarm_triggered[0]=true;
            }           
        }
    }
    if(alarm_enabled[1]== true){
        if(alarm_triggered[1]==false && alarm_hours[1]==hours && alarm_minutes[1]==minutes){
            ring_alarm(1);
            if(alarm_snooze[1] == false){
                alarm_triggered[1]=true;
            }
        }
    }
}


void check_temp() {
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    temp = data.temperature;
    if (data.temperature > 32) {
        display.clearDisplay();
        print_line("TEMP HIGH", 0, 40, 1);
    }
    else if (data.temperature < 24) {
        display.clearDisplay();
        print_line("TEMP LOW", 0, 40, 1);
    }
    if (data.humidity > 80) {
        display.clearDisplay();
        print_line("HUMIDITY HIGH", 0, 50, 1);
    }
    else if (data.humidity < 65) {
        display.clearDisplay();
        print_line("HUMIDITY LOW", 0, 50, 1);
    }
}

void view_alarm(){
    display.clearDisplay();
    while(digitalRead(PB_CANCEL)==HIGH){
    
    if(alarm_enabled[0]==true){
        print_line("Alarm 1 :"+ String(alarm_hours[0]) + " : " + String(alarm_minutes[0]) , 0, 40, 1);
    }
    if(alarm_enabled[1]==true){
        print_line("Alarm 2 :"+ String(alarm_hours[1]) + " : " + String(alarm_minutes[1]) , 0, 50, 1);
    }
    if(alarm_enabled[0]==false && alarm_enabled[1]==false){
        print_line("No Active Alarms",0,20,2);
    }
    int pressed = wait_for_button_press();
    if(pressed == PB_CANCEL){
        delay(200);
        break;
    }
    }
}

//function for storing ldr values in a list
void LDRSUM(){
    
    if (hours>current_sample_hr && seconds+60-current_sample_sec >= sample_interval && sample <ldr_val_amount){
        samples[sample] = analogRead(LDR);
        Serial.println(samples[sample]);
        sample += 1;
        current_sample_hr = hours;
        current_sample_min = minutes;
        current_sample_sec = seconds;

    }else if (minutes>current_sample_min && seconds+60-current_sample_sec >= sample_interval && sample <ldr_val_amount){
        samples[sample] = analogRead(LDR);
        Serial.println(samples[sample]);
        sample += 1;
        current_sample_min = minutes;
        current_sample_sec = seconds;

    }else if (minutes == current_sample_min && seconds-current_sample_sec >= sample_interval && sample <ldr_val_amount){
        samples[sample] = analogRead(LDR);

        Serial.println(samples[sample]);

        sample += 1;
        current_sample_sec = seconds;
    }
}

//Function for taking average ldr value 
float LDRAVG(){
    int sum = 0;
    for(int i=0;i<ldr_val_amount; i++ ){
        sum += samples[i];
    }
    sample = 0;
    float raw_avg = static_cast<float>(sum) / ldr_val_amount;
    intensity = map(raw_avg, 0, 4063, 100, 0) / 100.0;
    return intensity;
}

//function to publishing to mqtt
void MQTT_PUB(float val,String cat){
    char buffer[10];
    if(cat=="ldr"){
        snprintf(buffer, sizeof(buffer), "%.2f", val);
        if (client.connected()) {
          client.publish("257/ldr", buffer);
          Serial.println("Published to 257/ldr");
        } else {
          Serial.println("MQTT not connected");
        }

    }
    if(cat=="temp"){
        snprintf(buffer, sizeof(buffer), "%.2f", val);
        if (client.connected()) {
          client.publish("257/temp", buffer);
          Serial.println("Published to 257/temp");
        } else {
          Serial.println("MQTT not connected");
        }
    }
}

//function for controlling sevo
void control_servo(){
    if (sample_send_period != 0 && t_med != 0) {
        float ratio = logf((float)sample_interval / sample_send_period);
        // Make sure ratio is positive
        if (ratio < 0) ratio = -ratio;
    
        float theta = theta_offset + (180 - theta_offset) * intensity * control_factor* ratio * (temp / t_med);
        if (theta < 0) theta = 0;         //keeping valid angles
        if (theta > 180) theta = 180;
        myServo.write(theta);

    }
}