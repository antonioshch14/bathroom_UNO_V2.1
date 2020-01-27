#include <SoftwareSerial.h>
#include <SoftwareWire.h> // am2320
#include <AM2320.h>
#define RX 2 //esp settings
#define TX 3 //esp settings
int LED1=6;
int LED2=9;
int LED3=10;
int relay=12;
int button=A0;
int light = 11; //switching mirror light
int espReset=8; 
int humanSensor=A1;
int humadLED=A2;
int humidGotFromServer = 0;
AM2320 sensor(4,5); // AM2320 sensor attached SDA to digital PIN 4 and SCL to digital PIN 5
SoftwareSerial esp8266(RX,TX);
int long overallTime=6*60L*1000L;
bool buttonStart, fanStart,countDown,buttonStartEnd;
int buttonPressedCount;
char dataSent[500];
float sensorData[7][4];
int session; // counts how many times the data from sensors was read befor sending
int long SeccountT;
int long LEDTime=overallTime/3;
int unsigned long timers[14]; //0-bouncing of pushed button , 1- bouncing of released button, 2- reset of counting type and direction of counting, 
                          // 3- second for continious counting, 4- count down of fan, 5 reading sensors, 6- sending data, 7-econd for steps counting,
                          // 8- for blocking sensors read while button pushing, 9-human sensor for activation of fan, 10-counter of humidWarning alarm,
                          // 11-ESP reset , 12-start while pause; 13- light on

//-------------------------humid Array
int maxHumStored=100; //how many entry to store
int humid[100]; //storage of read humidity
bool humidWarning; //triger of flashing LED and prolong rotation of fan
int humidStored;
 bool coutnFull;
 int HumidAver;
 //----------------------------------------------------------------
 bool flashingLED;
 bool LEDFlashOn; // in the loop of flaching to define state of LEDs
 int timeToReadSensor=9940;
 int long timeToSEndData=11000;//was 60 000
 int humidWarningThreshod=3;
 bool humidWarningPause; 
 int unsigned long humidWaningEnd=1800000;
 int unsigned long humanSensorDelay=180000;
 bool humanBodyDeteckted;
 bool humanSensorOn;
 bool lightOff;
 int unsigned long lightOffDelay = 30000;
bool const test=false;
bool addHumidToArray; 
bool buttonPresedWhilePause;
 
void setup() {
  Serial.begin(9600); 
  esp8266.begin(9600);
  pinMode(LED1,OUTPUT);
  pinMode(LED2,OUTPUT);
  pinMode(LED3,OUTPUT);
pinMode(button,INPUT);
digitalWrite(button,HIGH);
pinMode(relay,OUTPUT);
pinMode(espReset,OUTPUT);
digitalWrite(espReset,HIGH);
pinMode(humanSensor,INPUT);
pinMode(humadLED,OUTPUT);
digitalWrite(relay, HIGH);
pinMode(light, OUTPUT);
digitalWrite(light, HIGH);

if (test){
          overallTime=1*60L*1000L;
          timeToSEndData=6000;
          timeToReadSensor=994;
          humidWaningEnd=60000UL;
          LEDTime=overallTime/3;
          humanSensorDelay=5000;
          humidWarningThreshod=2;
          maxHumStored=5; //how many entry to store
          }
}

void loop() {
int unsigned long left; // counter of a time that lesft fo timers
int buttonPressed=digitalRead(button);
//--------------------------------button pushed-------------------------------------
if (buttonPressed==LOW && timer(0,100))
  {
  if (!buttonStart && !fanStart){ buttonStart=true;
                                  buttonPressedCount=1;
                                  fanStart=true;
                                  timers[3]=millis()-201;
                                  }
  if (buttonStart)
     {if (timer(3,200))          //for continous stetting
       { if(!countDown && buttonPressedCount>=6)
                                  {countDown=true;  //to define whether conting up or down
                                  timers[3]=millis()+2000; // delay in case of turnover
                                  }
        else if (countDown && buttonPressedCount<=0)
                                        {countDown=false;
                                        timers[3]=millis()+2000;
                                        fanStart=false;
                                        Serial.println("fan off 1 executed");} // delay in case of turnover
         else if (!countDown) { buttonPressedCount++;
                                LEDcontrol(buttonPressedCount,2,false);
                                SeccountT=overallTime*buttonPressedCount/6;
                               }
         else if (countDown)  {buttonPressedCount--;
                                LEDcontrol(buttonPressedCount,2,false);
                                SeccountT=overallTime*buttonPressedCount/6;}
                                // Serial.print("+++ btc=");Serial.print(buttonPressedCount); Serial.print("ScT=");Serial.println(SeccountT);
        }
      }
  else if (!buttonStart)                            //for  setting by steps
      {if(timer(7,50))  { if (!countDown )
                                {buttonPressedCount++;
                                 LEDcontrol(buttonPressedCount,2,false);
                                 SeccountT=overallTime*buttonPressedCount/6;
                                 // Serial.print("*** btc=");Serial.print(buttonPressedCount); Serial.print("  ScT=");Serial.println(SeccountT);
                                 }
                         else if (countDown )
                              {buttonPressedCount--;
                              LEDcontrol(buttonPressedCount,2,false);
                              SeccountT=overallTime*buttonPressedCount/6;
                              // Serial.print("--- btc=");Serial.print(buttonPressedCount); Serial.print("  ScT=");Serial.println(SeccountT);
                              }
                          if ( buttonPressedCount>=6){countDown=true; }
                          else if (buttonPressedCount<=0){countDown=false;
                          Serial.println("fan off 2 executed");
                          timers[7]=millis()+2000;
                          fanStart=false;}
                          buttonStart=true; // to initiate continious counting in case of holding button 
                          }}
         timers[2]=millis(); //resetting of timer for type of counting
         timers[1]=millis();  // resetting of timer for released button
         timers[8]=millis(); buttonStartEnd=false; //resetting of timer for blocking sensors data read and send
         timers[12]=millis(); if (humidWarning) buttonPresedWhilePause=true; // repeating start while pause
    }
  //--------------------------------button released-------------------------------------------------
else if(buttonPressed==HIGH && timer(1,100))
  {if (buttonStart){buttonStart=false;}  // turn off counitious counting and start of step counting
 if (humidWarningPause && buttonPresedWhilePause && timer(12,2000)) {humidWarningPause=false; //for starting fan while pause 
                                            buttonPresedWhilePause=false;
                                            timers[10]=millis()+humidWaningEnd-SeccountT;
                                            Serial.print("SeccountT= ");Serial.println(SeccountT);
                                            Serial.println("buttonPresedWhilePause");}// setting time for fan in case of starting in warning pause
   if (timer(2,5000)){countDown=false;   // reset of counting type
                      buttonStart=false;
                      buttonPresedWhilePause=false;}   //to restart the continous setting
   timers[0]=millis();                        // resetting of timer for pushed button
   }
   //----------------------------------fan operation-------------------------------------------------
 if (!humidWarningPause && fanStart){digitalWrite(relay,LOW);}
 else {digitalWrite(relay,HIGH);}
 //-------------------------------start of humid-----------------------------------------------------
 if (!buttonStart && timer(4,1000))
  {   if (!humidWarning){if (fanStart && !buttonStart)
                          {SeccountT-=1000; 
                           LEDcontrol(SeccountT,LEDTime,false);
                           buttonPressedCount=6*SeccountT/overallTime;     
                           if (SeccountT<=0){fanStart=false;} //fan stop
                          }
                        }
      else if (humidWarning) {if (fanStart){ if (!humidWarningPause){ left= (humidWaningEnd + timers[10]-millis()); 
                                                  //Serial.print("left= ");Serial.println(left);
                                                 // Serial.print("humidWaningEnd= ");Serial.println(humidWaningEnd);
                                                  //Serial.print("timers[10] ");Serial.println(timers[10]);
                                                 // Serial.print("millis()");Serial.println(millis());
                                                  LEDcontrol(left,humidWaningEnd/3,true);
                                                  SeccountT=2000;}
                                              else  { left= (humidWaningEnd/3 + timers[10]-millis()); //flashing while pause is on
                                                  LEDcontrol(left,humidWaningEnd/9,true);} 
                                            }                         
                         else if (!fanStart) {LEDcontrol(3,1,true);} //flashing alert befor button has pushed
                              }
  }
  if (fanStart){ // fan pause setting
              if (!humidWarningPause && humidWarning){if (timer(10,humidWaningEnd)) {humidWarningPause=true;
                                                                                     addHumidToArray=true;}} //add every pause loop one entry with hight humid to array
              if (humidWarningPause && humidWarning){if (timer(10,humidWaningEnd/3)) {humidWarningPause=false;}}
              }
// ------------------------------------end of humid----------------------------------------------------------------
  if (timer(8,5000))buttonStartEnd=true; 
  if (buttonStartEnd) // do not read sensor data while pressing button
      {
        if ( timer(5,timeToReadSensor)){getSensorData(session); 
                             session++;}
        if (session>0 && timer(6,timeToSEndData)) { 
                       humidArray(session);
                      dataSending(session-1); 
                      session=0; 
                      timers[5]=millis();}
      }
//-------------------------human sensor------------------------------------------------------------------------------------

if (!test) humanSensorOn=digitalRead(humanSensor);
else {humanSensorOn=false;}
if (humanSensorOn) { digitalWrite(humadLED, HIGH); timers[9] = millis(); humanBodyDeteckted = true; }
else digitalWrite(humadLED,LOW);
if (humidWarning && !fanStart && timer(9,humanSensorDelay))fanStart=true;
//------------------------end of human sensor-----------------------------------------------------------------------------
if (humanBodyDeteckted) {
	timers[13] = millis(); 
	lightOff = false;
	digitalWrite(light, HIGH);
	}
if (!lightOff)if (timer(13, lightOffDelay)) {
	digitalWrite(light, LOW);
	lightOff = true;
}
//---------------------end of light off----------------------------------------------------------------------------
if (esp8266.available()) ReadDataSerial();
if (timer(11,130000))ResetESP();
}
//=================================================================================================================================================
void ResetESP (){digitalWrite(espReset,LOW);delay(500);digitalWrite(espReset,HIGH);}
void ReadDataSerial(){ //reads data from ESP serial and checks for validity then sens responce by G:xxx], if gets re:from100 to 299 than LEDOK ON
bool startServRespRead=false;
String message = esp8266.readStringUntil('\r');
	Serial.println(message);
	unsigned long value;
	int index;
	int humid;
	if (get_field_value(message, "humid:", &value, &index)) {
		humid = value / pow(10, index);
		Serial.print("Humid recognised: ");                         // print the content
		Serial.println(humid);
		humidGotFromServer = humid; 
		timers[11] = millis();//postpond esp reset
	}
	                   
}
bool get_field_value(String Message, String field, unsigned long* value, int* index) {
	int fieldBegin = Message.indexOf(field) + field.length();
	int check_field = Message.indexOf(field);
	int ii = 0;
	*value = 0;
	*index = 0;
	bool indFloat = false;
	if (check_field != -1) {
		int filedEnd = Message.indexOf(';', fieldBegin);
		if (filedEnd == -1) { return false; }
		int i = 1;
		char ch = Message[filedEnd - i];
		while (ch != ' ' && ch != ':') {
			if (isDigit(ch)) {
				int val = ch - 48;
				if (!indFloat)ii = i - 1;
				else ii = i - 2;
				*value = *value + ((val * pow(10, ii)));
			}
			else if (ch == '.') { *index = i - 1; indFloat = true; }
			i++;
			if (i > (filedEnd - fieldBegin + 1) || i > 10)break;
			ch = Message[filedEnd - i];
		}

	}
	else return false;
	return true;
}

void humidArray(int sensorRead)  {
	int i,ii, sum=0;
   float humidAverRead;
   
    for (i=0; i<sensorRead; i++){sum+=sensorData[i][1];} // count average read for a minut from sensor
   humidAverRead=sum/sensorRead;
   if (HumidAver ==0)HumidAver=humidAverRead;
   if (humidAverRead-HumidAver>humidWarningThreshod){humidWarning=true;}
   else { countHumid(humidAverRead);
          addHumidToArray=false; //reset adding humid to array after once added
		  if (humidWarning) { digitalWrite(LED1, LOW); digitalWrite(LED2, LOW); digitalWrite(LED3, LOW); }//solving issue with occasional LED
          humidWarning=false;
          humidWarningPause=false;
          timers[10]=millis(); // resetting timers in case of rising humid while fan on (without humid warning)
         }
    if (addHumidToArray && !test){for (ii=0;ii<3;ii++){countHumid(humidAverRead);} // onec add data to humid array in case of pause
                         addHumidToArray=false;}
    if (humidWarningPause && humidAverRead-HumidAver<=humidWarningThreshod)humidWarningPause=false;
    if (humidWarning && !fanStart)timers[10]=millis();  
    
  
  }
void countHumid(float humidAverRead){
  humidStored++;
  int sumOverall=0;
  int ii;
 humid[humidStored]=(int)humidAverRead;  // for initial start before 100 have counted
  if (!coutnFull) {for (ii=1;ii<=humidStored;ii++){sumOverall+=humid[ii];}
		  if (humidGotFromServer > 0) {
			  HumidAver = humidGotFromServer;//assign humid value for server after restart
			  for (int iii = 0; iii < maxHumStored; iii++) humid[iii]= humidGotFromServer;
			  coutnFull = true;
			  Serial.println("the humidity array filled with humidity got from server " + String(humidGotFromServer));
		  }
			else HumidAver=sumOverall/humidStored;
}
          else {for (ii=1;ii< maxHumStored;ii++){sumOverall+=humid[ii];}
                    HumidAver=sumOverall/(maxHumStored-1);
               }
 if (humidStored==(maxHumStored-1)){coutnFull=true;
                                     humidStored=0;} //return to the beginning of the array when its full

}
void LEDcontrol(int long t,int long s, bool flashing){
  int long brightness;
  if (flashing && LEDFlashOn){digitalWrite(LED1,LOW);digitalWrite(LED2,LOW);digitalWrite(LED3,LOW); LEDFlashOn=false; return;}
  if (flashing && !LEDFlashOn)LEDFlashOn=true;

            if (t> 2*s){
                brightness=((t-s*2)*255)/s;
                analogWrite(LED1,brightness);
                analogWrite(LED2,255);
                analogWrite(LED3,255);
                Serial.print("1 br=");Serial.println(brightness);
                }
            else if (t>s && t<= 2*s){
                brightness=((t-s)*255)/s;
                analogWrite(LED1,0);
                analogWrite(LED2,brightness);
                analogWrite(LED3,255);
               Serial.print("2 br=");Serial.println(brightness);
                }
             else if (t>0 && t<=s) {brightness=(t*255)/s;
                analogWrite(LED1,0);
               analogWrite(LED2,0);
               analogWrite(LED3,brightness);
               Serial.print("3 br=");Serial.println(brightness);
               }
             else analogWrite(LED3,0);
          
}
  
bool timer(int tNamber, unsigned long tDelay){
  unsigned long current=millis();
    if (current>(timers[tNamber]+tDelay)){
     timers[tNamber]=current; return true;
     }
    else return false;
}

void getSensorData (int sessions){
	byte state=B00000000;// construction of state
      float temp, humidity;
      bool datamissed=false;
      switch(sensor.Read()) {
    case 2:
      Serial.println("CRC failed");
      datamissed=true;
      break;
    case 1:
      Serial.println("Sensor offline");
      datamissed=true;
      break;
    case 0:
      Serial.print("Humidity: ");
      Serial.print(sensor.h);
      humidity = sensor.h;
      Serial.print("%\t Temperature: ");
      Serial.print(sensor.t);
      temp = sensor.t;
      Serial.println("*C");
      break;
  }
  if (!datamissed){
                    sensorData[sessions][0] = temp;
                    sensorData[sessions][1] = humidity;
                    if (HumidAver <100 ) {sensorData[sessions][2]=HumidAver;}
                    else {sensorData[sessions][2]=10;}
					if (fanStart)bitWrite(state, 0, 1);
					if (humanBodyDeteckted)bitWrite(state, 1, 1);
					if (humidWarningPause)bitWrite(state, 2, 1);
					if (humidWarning)bitWrite(state, 3, 1);
					if (coutnFull)bitWrite(state, 4, 1);
					if (addHumidToArray)bitWrite(state, 5, 1);
					if (buttonPresedWhilePause)bitWrite(state, 6, 1);
					if (!lightOff)bitWrite(state, 7, 1);
					sensorData[sessions][3] =int(state);
				    humanBodyDeteckted=false;//reset body detection after writing to send data
                    }
   else {session--;}
}

  void dataSending(int rows){
  int is; size_t lengthT;
  for (is=0; is<=rows; is++){
  strcat(dataSent,"{10,");
  int is2;
  for (is2=0;is2<4;is2++){ 
  lengthT = String(sensorData[is][is2]).length();
  char temp[4];
  String(sensorData[is][is2]).toCharArray(temp,lengthT+1);
  strcat(dataSent,temp);
  strcat(dataSent,",");}
  lengthT = strlen(dataSent);
  dataSent[lengthT-1]='}';}
  esp8266.println(dataSent); 
  Serial.println(dataSent);
  //Serial.print("dataSent ");
  Serial.println(dataSent); 
  dataSent[0]='\0';
}
