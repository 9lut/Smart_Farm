#include <LiquidCrystal_I2C.h> 
#include <DHT.h>         //เรียกใช้คลังโปรแกรม DHT.h
#include <Keypad_I2C.h>
#include <Keypad.h>
#include <Wire.h> 
#include <TimerOne.h>  // Timer1 library
#include <avr/wdt.h>

//initial sensor pin
#define DHTPIN 5          //กรณีมีการต่อสัญญาณของเซนเซอร์ DHT11 เข้ากับขา 5
#define DHTTYPE DHT11     //DHT 11

//setting I2C Address
#define I2CADDR 0x20 

//setting keypad
const byte ROWS = 4;
const byte COLS = 4;

char hexaKeys[ROWS][COLS] = {
 {'1','2','3','A'},
 {'4','5','6','B'},
 {'7','8','9','C'},
 {'*','0','#','D'}
};

byte rowPins[ROWS] = {7, 6, 5, 4};
byte colPins[COLS] = {3, 2, 1, 0}; 

Keypad_I2C keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS, I2CADDR); 

//initial analog sensor
int Soil = A0; 
int Water = A1; 
int Light = A2;

//initial dht sensor
DHT dht(DHTPIN, DHT11);   //สั่งให้ใช้มอดูล DHT11 กับขา 5 ของบอร์ด Arduino

//initial LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2); //Module IIC/I2C Interface บางรุ่นอาจจะใช้ 0x3f 

//initial sensor delay time 
unsigned long soilPreviousMillis = 0;
unsigned long tempPreviousMillis = 0;
unsigned long waterPreviousMillis = 0;
unsigned long lightPreviousMillis = 0;
unsigned long pumpPreviousMillis = 0;
unsigned long pressAPreviousMillis = 0;
unsigned long debouncePreviousMillis = 0;
unsigned long senttime = 0;
unsigned long Active = 0;
const unsigned long interval = 1000;  // 1 วินาที

//initial state for PressA
int checkState = 0; 

//initial Min&Max Soil Moisture
int Min_Soilmoisture = 50; 
int Max_Soilmoisture = 77;

//initial water level value
int water_low = 0;

//initial Light active
int Light_Active = 30;
int Fan_Active = 30;

//state sensor on node-red
bool water1_on = false;

//initial sensor Action
bool isSoilCheckActive = false;
bool isTempCheckActive = false;
bool isWaterCheckActive = false;
bool isLightCheckActive = false;
bool isPumpCheckActive = false;
bool isPressA = true;

// Time variables
int Hour = 0, Minute = 0, Second = 0;
bool digit = false;

void setup() {
  Wire.begin();                    //เรียกการเชื่อมต่อ Wire
  keypad.begin(makeKeymap(hexaKeys));  //เรียกการเชื่อมต่อ Keymap
  Serial.begin(115200);
  lcd.init();
  lcd.backlight(); // เปิด backlight
  lcd.home();
  wdt_enable(WDTO_8S);
  Timer1.initialize(1000000); 

  dht.begin();      //สั่งให้ DHT11 ทำงาน
  pinMode(Soil, INPUT);
  pinMode(Water, INPUT);
  pinMode(Light, INPUT);

}



//Check water sensor and print to lcd
void check_water(float level) {
  lcd.clear();
  lcd.setCursor(0, 0); 
  lcd.print("Water level");
  lcd.setCursor(0, 1);
  lcd.print("Value: ");
  lcd.print(level);  //แสดง output ของค่าความชื้นสัมพัทธ์
  lcd.print(" %"); 
}

//Check light sensor and print to lcd
void check_light(float light) {
  lcd.clear();
  lcd.setCursor(0, 0); 
  lcd.print("Light intensity");
  lcd.setCursor(0, 1); 
  lcd.print("Value: ");
  lcd.print(light);  //แสดง output ของค่าความชื้นสัมพัทธ์
  lcd.print(" %"); 
}

//Check soilmoisture sensor and print to lcd
void check_soil(float Moisture) {
  lcd.clear();
  lcd.setCursor(0, 0); 
  lcd.print("Soil Moisture");
  lcd.setCursor(0, 1);
  lcd.print("Value: ");
  lcd.print(Moisture);  //แสดง output ของค่าความชื้นในดิน
  lcd.print(" %"); 
}

//Check temperature&humidity sensor and print to lcd
void check_temp(float h, float t) {
  lcd.clear();
  lcd.setCursor(0, 0); 
  lcd.print("Humidity:");
  lcd.print(h);                    //แสดง output ของค่าความชื้นสัมพัทธ์
  lcd.print(" %");                 //หน่วยแสดงค่าความชื้นสัมพัทธ์
  lcd.setCursor(0, 1); 
  lcd.print("Temp:");
  lcd.print(t);                    //แสดง output ของอุณหภูมิ
  lcd.print((char)223);            //ใช้คำสั่ง char เพื่อให้มีเครื่องหมายหน้าหน่วยองศาเซลเซียส
  lcd.print("C");                  //หน่วยองศาเซลเซียส  
}

// ในฟังก์ชัน ISR (Interrupt Service Routine) ของ Timer1
void timerISR() {
  // ถ้า Minute == 0 และ Hour == 0 นั่นคือเวลาหมด
  if (Minute == 0 && Hour == 0 && Second == 0) {
    Timer1.stop();          // หยุด Timer
    water1_on = false;
    Serial.println("PUMP1OFF");
  } else {
    // นับถอยหลัง
    Second--;
    if (Second < 0){
        if (Minute == 0) {
            if (Hour > 0) {
                Hour--;    // ลดชั่วโมง
                Minute = 59;  // ตั้งนาทีเป็น 59
            }
        } else {
            Minute--;  // ลดนาที
        }
        Second = 59;  // ตั้งวินาทีเป็น 59
    }
  }
}



//initial setting sensor state 
bool isSettingSoil_min = false;  
bool isSettingSoil_max = false;  
bool isSettingTemp = false;  
bool isSettingWater = false;  
bool isSettingLight = false;  

//initial input buffer for Soil Moisture
char inputBuffer[4];           // เก็บค่าตัวเลขที่ป้อนเข้ามา (3 หลัก + 1 สำหรับ '\0')
int inputIndex = 0;            // ตำแหน่งของข้อมูลที่กำลังป้อน

//function clear input buffer
void clearInputBuffer() {
  // ล้างข้อมูลใน inputBuffer และรีเซ็ต inputIndex
  memset(inputBuffer, 0, sizeof(inputBuffer));  // เคลียร์ buffer ทั้งหมด
  inputIndex = 0;  // รีเซ็ตตำแหน่งการป้อนค่า
}

//function check Keypad 
void check_keypad() {
  char key = keypad.getKey();
  unsigned long currentMillis = millis();

  if (key != NO_KEY && currentMillis - debouncePreviousMillis >= 200) {
    isSoilCheckActive = false;
    isTempCheckActive = false;
    isWaterCheckActive = false;
    isLightCheckActive = false;
    isPumpCheckActive = false;
    isPressA = false;

    if (isSettingSoil_min) {  // ตรวจสอบทั้งสองสถานะ
      if (key >= '0' && key <= '9') {  // ถ้าป้อนตัวเลข
        if (inputIndex < 3) {  // รับค่าได้สูงสุด 4 หลัก
          inputBuffer[inputIndex] = key;
          inputIndex++;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Soil_Moisture");
          lcd.setCursor(0, 1);
          lcd.print("Minimum:");
          lcd.print(inputBuffer);  // แสดงค่าที่ป้อนบน LCD
          lcd.print("%");
        }
      } else if (key == '#') {  // กด '#' เพื่อยืนยันค่าที่ป้อน
        inputBuffer[inputIndex] = '\0';  // สิ้นสุดสตริง
        int inputValue = atoi(inputBuffer);  // แปลงค่าที่ป้อนเป็น integer

        if (inputValue >= 0 && inputValue <= 100) {  // ตรวจสอบว่าค่าอยู่ในช่วง 0-100
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Soil_Moisture");
          lcd.setCursor(0, 1);
          lcd.print("Minimum:");
          lcd.print(inputValue);  // แสดงค่าที่รับเข้ามา
          lcd.print("%"); 
          Min_Soilmoisture = inputValue;
          Serial.print("Min_soil:");
          Serial.println(Min_Soilmoisture);
          isSettingSoil_min = false;  // ออกจากโหมดป้อนค่า
          clearInputBuffer();  // ล้างข้อมูลหลังจากรับค่าเสร็จ
          delay(2000);
          isSettingSoil_max = true;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Soil_Moisture");
          lcd.setCursor(0, 1);
          lcd.print("Maximum:");
          lcd.print("%");
        } else {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Error: Must");
          lcd.setCursor(0, 1);
          lcd.print("be 0-100");
          delay(2000);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Soil_Moisture");
          lcd.setCursor(0, 1);
          lcd.print("Minimum:");
          lcd.print("%");
          clearInputBuffer();  // ล้างข้อมูลเพื่อเตรียมป้อนค่าใหม่
        }
      }
    }else if(isSettingSoil_max){
      if (key >= '0' && key <= '9') {  // ถ้าป้อนตัวเลข
        if (inputIndex < 3) {  // รับค่าได้สูงสุด 4 หลัก
          inputBuffer[inputIndex] = key;
          inputIndex++;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Soil_Moisture");
          lcd.setCursor(0, 1);
          lcd.print("Maximum:");
          lcd.print(inputBuffer);  // แสดงค่าที่ป้อนบน LCD
          lcd.print("%");
        }
      } else if (key == '#') {  // กด '#' เพื่อยืนยันค่าที่ป้อน
        inputBuffer[inputIndex] = '\0';  // สิ้นสุดสตริง
        int inputValue = atoi(inputBuffer);  // แปลงค่าที่ป้อนเป็น integer

        if (inputValue >= 0 && inputValue <= 100) {  // ตรวจสอบว่าค่าอยู่ในช่วง 0-100
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Soil_Moisture");
          lcd.setCursor(0, 1);
          lcd.print("Maximum:");
          lcd.print(inputValue);  // แสดงค่าที่รับเข้ามา
          lcd.print("%"); 
          Max_Soilmoisture = inputValue;
          Serial.print("Max_soil:");
          Serial.println(Max_Soilmoisture);
          isSettingSoil_max = false;  // ออกจากโหมดป้อนค่า
          clearInputBuffer();  // ล้างข้อมูลหลังจากรับค่าเสร็จ
          delay(2000);
          isPressA = true;
        } else {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Error: Must");
          lcd.setCursor(0, 1);
          lcd.print("be 0-100");
          delay(2000);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Soil_Moisture");
          lcd.setCursor(0, 1);
          lcd.print("Maximum:");
          lcd.print("%");
          clearInputBuffer();  // ล้างข้อมูลเพื่อเตรียมป้อนค่าใหม่
        }
      }
    }else if (isSettingWater){
    if (key == 'A') {  // Toggle between Hour and Minute
      digit = !digit;
    } else if (key == 'C') {  // Increment Hour or Minute
      if (!digit) {
        Hour = (Hour + 1) % 24;  // Increment Hour (0-23)
      } else {
        Minute = (Minute + 1) % 60;  // Increment Minute (0-59)
      }
    } else if (key == 'D') {  // Decrement Hour or Minute
      if (!digit) {
        Hour = (Hour - 1 + 24) % 24;  // Decrement Hour
      } else {
        Minute = (Minute - 1 + 60) % 60;  // Decrement Minute
      }
    } else if (key == '#') {
      if (Hour == 0 && Minute == 0){
        Second = 0;
      }
      Timer1.attachInterrupt(timerISR); // Attach the ISR function
      Timer1.start();
      Serial.println("PUMP1ON");
      water1_on = true;
      isPressA = true;
      isSettingWater = false;  // Exit setting mode
    }    
    // การแสดงผลบน LCD
    lcd.setCursor(0, 0);
    lcd.print("Water Time");
    lcd.setCursor(0, 1);

    // แสดง Hour หรือ Minute ขึ้นอยู่กับสถานะแก้ไข (digit)
    if (!digit) {  // กำลังแก้ไข Hour
      lcd.print("  ");
      delay(500);
      lcd.setCursor(0, 1);
      lcd.print(Hour < 10 ? "0" : ""); 
      lcd.print(Hour);  // แสดงค่า Hour
      lcd.print(" hr:");
    } else {  // กำลังแก้ไข Minute
        lcd.setCursor(6, 1);
        lcd.print("  ");
        delay(500);
        lcd.setCursor(6, 1);
        lcd.print(Minute < 10 ? "0" : ""); 
        lcd.print(Minute);  // แสดงค่า Minute
        lcd.print(" mins"); 
      }
  }else if(isSettingLight){
      if (key >= '0' && key <= '9') {  // ถ้าป้อนตัวเลข
        if (inputIndex < 3) {  // รับค่าได้สูงสุด 4 หลัก
          inputBuffer[inputIndex] = key;
          inputIndex++;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Light intensity");
          lcd.setCursor(0, 1);
          lcd.print("less than: ");
          lcd.print(inputBuffer);  // แสดงค่าที่ป้อนบน LCD
          lcd.print("%");
        }
      } else if (key == '#') {  // กด '#' เพื่อยืนยันค่าที่ป้อน
        inputBuffer[inputIndex] = '\0';  // สิ้นสุดสตริง
        int inputValue = atoi(inputBuffer);  // แปลงค่าที่ป้อนเป็น integer

        if (inputValue >= 0 && inputValue <= 100) {  // ตรวจสอบว่าค่าอยู่ในช่วง 0-100
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Light intensity");
          lcd.setCursor(0, 1);
          lcd.print("less than: ");
          lcd.print(inputValue);  // แสดงค่าที่รับเข้ามา
          lcd.print("%"); 
          Light_Active = inputValue;
          Serial.print("Light_Active:");
          Serial.print(Light_Active);
          isSettingLight = false;  // ออกจากโหมดป้อนค่า
          clearInputBuffer();  // ล้างข้อมูลหลังจากรับค่าเสร็จ
          delay(2000);
          isPressA = true;
        } else {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Error: Must");
          lcd.setCursor(0, 1);
          lcd.print("be 0-100");
          delay(2000);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Light intensity");
          lcd.setCursor(0, 1);
          lcd.print("less than: ");
          clearInputBuffer();  // ล้างข้อมูลเพื่อเตรียมป้อนค่าใหม่
        }
      }
      }else if(isSettingTemp){
      if (key >= '0' && key <= '9') {  // ถ้าป้อนตัวเลข
        if (inputIndex < 3) {  // รับค่าได้สูงสุด 4 หลัก
          inputBuffer[inputIndex] = key;
          inputIndex++;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Temperature");
          lcd.setCursor(0, 1);
          lcd.print("more than: ");
          lcd.print(inputBuffer);  // แสดงค่าที่ป้อนบน LCD
          lcd.print((char)223);            //ใช้คำสั่ง char เพื่อให้มีเครื่องหมายหน้าหน่วยองศาเซลเซียส
          lcd.print("C"); 
        }
      } else if (key == '#') {  // กด '#' เพื่อยืนยันค่าที่ป้อน
        inputBuffer[inputIndex] = '\0';  // สิ้นสุดสตริง
        int inputValue = atoi(inputBuffer);  // แปลงค่าที่ป้อนเป็น integer

        if (inputValue >= 0 && inputValue <= 50) {  // ตรวจสอบว่าค่าอยู่ในช่วง 0-100
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Temperature");
          lcd.setCursor(0, 1);
          lcd.print("more than: ");
          lcd.print(inputValue);  // แสดงค่าที่รับเข้ามา
          lcd.print((char)223);            //ใช้คำสั่ง char เพื่อให้มีเครื่องหมายหน้าหน่วยองศาเซลเซียส
          lcd.print("C"); 
          Fan_Active = inputValue;
          Serial.print("Fan_Active:");
          Serial.print(Fan_Active);
          isSettingTemp = false;  // ออกจากโหมดป้อนค่า
          clearInputBuffer();  // ล้างข้อมูลหลังจากรับค่าเสร็จ
          delay(2000);
          isPressA = true;
        } else {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Error: Must");
          lcd.setCursor(0, 1);
          lcd.print("be 0-50");
          delay(2000);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Temperature");
          lcd.setCursor(0, 1);
          lcd.print("more than: ");
          lcd.print((char)223);            //ใช้คำสั่ง char เพื่อให้มีเครื่องหมายหน้าหน่วยองศาเซลเซียส
          lcd.print("C"); 
          clearInputBuffer();  // ล้างข้อมูลเพื่อเตรียมป้อนค่าใหม่
        }
      }
      }else {  // ไม่ได้อยู่ในโหมดการป้อนค่า
      switch (key) {
        case '1':
          isSoilCheckActive = true;
          break;

        case '2':
          isTempCheckActive = true;
          break;

        case '3':
          isWaterCheckActive = true;
          break;

        case '4':
          isLightCheckActive = true;
          break;

        case '5':  // เปลี่ยนจาก if เป็น case '5'
          isSettingSoil_min = true;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Soil_Moisture");
          lcd.setCursor(0, 1);
          lcd.print("Minimum:");
          lcd.print("%");
          clearInputBuffer();  // ล้างข้อมูลก่อนป้อนค่าใหม่
          break;
        
        case '6':
          isSettingWater = true;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Water Time");
          lcd.setCursor(0, 1);
          lcd.setCursor(0, 1);
          lcd.print(Hour < 10 ? "0" : ""); lcd.print(Hour);  // Display Hour
          lcd.print(" hr");
          lcd.print(":");
          lcd.print(Minute < 10 ? "0" : ""); lcd.print(Minute);  // Display Minute
          lcd.print(" mins");
          break;

        case '7':
          isSettingLight = true;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Light intensity");
          lcd.setCursor(0, 1);
          lcd.print("less than: ");
          clearInputBuffer();  // ล้างข้อมูลก่อนป้อนค่าใหม่
          break;

        case '8':
          isSettingTemp = true;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Temperature");
          lcd.setCursor(0, 1);
          lcd.print("more than: ");
          lcd.print((char)223);            //ใช้คำสั่ง char เพื่อให้มีเครื่องหมายหน้าหน่วยองศาเซลเซียส
          lcd.print("C"); 
          clearInputBuffer();  // ล้างข้อมูลก่อนป้อนค่าใหม่
          break;

        case '9':
          isPumpCheckActive = true;
          break;

        case 'A':
          isPressA = true;
          break;

        default:
          lcd.clear();
          lcd.setCursor(0, 0); 
          lcd.print("Error :");      
          lcd.setCursor(0, 1); 
          lcd.print("Invalid Input");
          break;
      }
    }
    debouncePreviousMillis = currentMillis;  // รีเซ็ตดีบาวซ์
  }
}

void Pump1active(float fMoisture, int Min_Soilmoisture ,int Max_Soilmoisture) {
    if(fMoisture < Min_Soilmoisture && !water1_on){
      Serial.println("PUMP1ON");
    }else if (fMoisture > Max_Soilmoisture){
      Serial.println("PUMP1OFF");
    }
  }

void Pump2active(float flevel ,int water_low) {   
    if(flevel < water_low){
      Serial.println("PUMP2ON");
    }else if (flevel > 90 ){
      Serial.println("PUMP2OFF");
    }
  }

void Lightactive(float fLight ,int Light_Active) {
    if(fLight < Light_Active){
      Serial.println("Light_ON");
    }else if (fLight > Light_Active){
      Serial.println("Light_OFF");
    }
  }

void Fanactive(float t ,int Fan_Active) {
    if(t > Fan_Active ){
      Serial.println("Fan_ON");
    }else if (t < 28 ){
      Serial.println("Fan_OFF");
    }
  }


//main loop
void loop() { 

  float h = dht.readHumidity();     //อ่านค่าความชื้นสัมพัทธ์ใส่ตัวแปร h
  float t = dht.readTemperature();  //อ่านค่าอุณหภูมิในหน่วยองศาเซลเซียสใส่ตัวแปร t
  float Moisture = analogRead(Soil);
  float level = analogRead(Water);
  float light = analogRead(Light);

  float fLight = (light/1023) * 100;
  fLight = 100 - fLight;
  float flevel = (level/1023) * 100 ;
  float fMoisture = (Moisture/1023) * 100 ;

  check_keypad();
  

  unsigned long currentMillis = millis();

  if(millis() - senttime >= 7000){
    Serial.print("Humidity:");
    Serial.println(h);
    Serial.print("Temperature:");
    Serial.println(t);
    Serial.print("Soil_Moisture:");
    Serial.println(fMoisture);
    Serial.print("Water_Level:");
    Serial.println(flevel);
    Serial.print("Light:");
    Serial.println(fLight);
    senttime = millis();
    wdt_reset();
  }



  // อ่านค่าจาก soil sensor ไปเรื่อยๆเมื่อ isSoilCheckActive เป็น true
  if (isSoilCheckActive && currentMillis - soilPreviousMillis >= interval) {
    check_soil(fMoisture);
    soilPreviousMillis = currentMillis;
  }

  // อ่านค่าอุณหภูมิและความชื้นไปเรื่อยๆเมื่อ isTempCheckActive เป็น true
  if (isTempCheckActive && currentMillis - tempPreviousMillis >= interval) {
    check_temp(h,t);
    tempPreviousMillis = currentMillis;
  }

  // อ่านค่าระดับน้ำไปเรื่อยๆเมื่อ isWaterCheckActive เป็น true
  if (isWaterCheckActive && currentMillis - waterPreviousMillis >= interval) {
    check_water(flevel);
    waterPreviousMillis = currentMillis;
  }

  if (isPumpCheckActive && currentMillis - pumpPreviousMillis >= interval) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Time remaining");
    lcd.setCursor(0, 1);
    if(Hour == 0 && Minute == 0 && Second == 0 ){
      lcd.print("Pump is OFF");
    }else {
      lcd.print(": ");
      lcd.print(Hour < 10 ? "0" : ""); lcd.print(Hour);  // Display Hour
      lcd.print(":");
      lcd.print(Minute < 10 ? "0" : ""); lcd.print(Minute);  // Display Minute
      lcd.print(":");
      lcd.print(Second < 10 ? "0" : ""); lcd.print(Second);
      lcd.print(" Sec");
    }
    pumpPreviousMillis = currentMillis;
  }

  if (isLightCheckActive && currentMillis - lightPreviousMillis >= interval) {
    check_light(fLight);
    lightPreviousMillis = currentMillis;
  }
  
  if (isPressA && currentMillis - pressAPreviousMillis >= 2000)  
    switch (checkState) {
      case 0:
        check_soil(fMoisture);
        checkState = 1;
        pressAPreviousMillis = currentMillis;
        break;
      case 1:
        check_temp(h,t);
        checkState = 2;
        pressAPreviousMillis = currentMillis;
        break;
      case 2:
        check_water(flevel);
        checkState = 3;
        pressAPreviousMillis = currentMillis;
        break;
      case 3:
        check_light(fLight);
        checkState = 0;
        pressAPreviousMillis = currentMillis;
        break;
    }

    if (millis() - Active >= 60000){
      Pump1active(fMoisture, Min_Soilmoisture, Max_Soilmoisture);
      Pump2active(flevel, water_low);
      Lightactive(fLight, Light_Active);
      Fanactive(t, Fan_Active);
      Active = millis();
    }

    Serial.flush();

}