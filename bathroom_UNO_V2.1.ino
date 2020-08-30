#include <SoftwareSerial.h>
#include <SoftwareWire.h> // am2320
#include <AM2320.h>
#define RX 2 //esp settings
#define TX 3 //esp settings
#define LED1 6
#define LED2 9
#define LED3 10
#define relay 12
#define button A0
#define light  11 //switching mirror light
#define deodorant  A5 //relay of the deodorant
#define deodorantDelay 600
#define espReset 8
#define humanSensor A1
#define humadLED A2
#define maxHumStored 100 //how many entry to store
#define lightOffDelay 300000
//#define test
#ifdef test
#define overallTime 60000
#define timeToSEndData 10000
#define timeToReadSensor 994
#define humidWaningEnd 60000
#define humidWarningThreshod  2
#define LOG(X) Serial.println(X);
#define LOGL(X)  Serial.print(X);
#else
#define overallTime 360000
#define timeToSEndData 5000
#define timeToReadSensor 940
#define humidWaningEnd 1800000
#define humidWarningThreshod  3
#define LOG(X) 
#define LOGL(X) 
#endif

AM2320 sensor(4, 5); // AM2320 sensor attached SDA to digital PIN 4 and SCL to digital PIN 5
//SoftwareSerial esp8266(RX, TX);
#define humidReadArraylength 12
void deodorantSpray();
class task {
public:
	unsigned long period;
	bool ignor = false;
	void reLoop() {
		taskLoop = millis();
	};
	bool check() {
		if (!ignor) {
			if (millis() - taskLoop > period) {
				taskLoop = millis();
				return true;
			}
		}
		return false;
	}
	void StartLoop(unsigned long shift) {
		taskLoop = millis() + shift;
	}
	task(unsigned long t) {
		period = t;
	}
private:
	unsigned long taskLoop;

};
task task_checkFan(1000);
task task_lighOn(lightOffDelay);
task task_readSensors(timeToReadSensor);
task task_sednData(timeToSEndData);
task task_addToHumidArrayDuringHumWarn(humidWaningEnd/2);
task task_LED(1000);
struct _fan {
	const unsigned long _timeToWork;//working time for normal operation
	int _gradeFanToWork;//one of three 1,2,3
	unsigned long _timeToWorkHumid; //working time for humid warning
	unsigned long _timeToWorkWeb;
	int _pauseRatio;// relevant time for pause
	unsigned long startOfFanWork; //time when a fan start to work
	unsigned long startOfFanWorkWeb;
	unsigned long _startPause;
	unsigned long _timePause;
	bool buttonStart;
	bool humidStart;
	bool webStart;
	bool fanWork;
	bool startPuase;
	bool deodorantActivated;
	_fan(unsigned long timeToWork, unsigned long timeToWorkOthers, int pauseRatio) :
		_timeToWork(timeToWork), _timeToWorkHumid(timeToWorkOthers), _pauseRatio(pauseRatio) {}
	bool checkToWork() {
		unsigned long current = millis();
		fanWork = false;//each loop prove that fan has to be ON
		if (buttonStart && startOfFanWork + _timeToWork * _gradeFanToWork < current) buttonStart = false;
		if (webStart && startOfFanWorkWeb + _timeToWorkWeb < current) webStart = false;
		if (humidStart && startOfFanWork + _timeToWorkHumid < current && !webStart && !buttonStart && !startPuase) setPauseHumid();
		if (startPuase && _startPause + _timePause < current) humidStartOfFan();
		if (buttonStart) fanWork = true;
		if (!humidStart) startPuase = false;// force pause off if humid warning off
		if (humidStart && !startPuase) 	fanWork = true;
		if (webStart) fanWork = true;
		if (fanWork) { 
			digitalWrite(relay, HIGH);
			return true;
		} else {
			digitalWrite(relay, LOW);
			if (deodorantActivated) { // spray deodorant after fan off once deodorantActivated is true
				deodorantSpray();
				deodorantActivated = false;
			}
			return false;
		}
	}
	void setPauseHumid()
	{
		startPuase = true;
		_startPause = millis();
		_timePause = _timeToWorkHumid / _pauseRatio;
	}
	void buttonStartFunc(int level) {
		if (level >= 0 && level<4) _gradeFanToWork = level;
		else _gradeFanToWork = 3;
		buttonStart = true;
		startOfFanWork = millis();
		digitalWrite(relay, HIGH);
	}
	void webStartFunc(unsigned long& timeToWork) {
		webStart = true;
		startOfFanWorkWeb = millis();
		_timeToWorkWeb = timeToWork;
	}
	void humidStartOfFan () {
		humidStart = true;
		startPuase = false;
		startOfFanWork = millis();
	}
	int getButtonLevel() {
		if (buttonStart) {
			return _gradeFanToWork;
		}
		else return 0;
	}
	unsigned long leftTime(unsigned long& setTime) {
		if (buttonStart) {
			setTime = _timeToWork * _gradeFanToWork;
			if (startOfFanWork + _timeToWork * _gradeFanToWork > millis()) return startOfFanWork + _timeToWork * _gradeFanToWork - millis();
			return 0;
			}
		if (startPuase) {
			setTime = _timePause;
			if (_startPause + _timePause > millis()) return _startPause + _timePause - millis();
			return 0;
		}
		if (humidStart) {
			setTime = _timeToWorkHumid;
			if (startOfFanWork + _timeToWorkHumid > millis()) return startOfFanWork + _timeToWorkHumid - millis();
			return 0;
		}
		if (webStart) {
			setTime = _timeToWorkWeb;
			if (startOfFanWorkWeb + _timeToWorkWeb > millis()) return startOfFanWorkWeb + _timeToWorkWeb - millis();
			return 0;
		}
		return 0;
	}
};
_fan fan(overallTime, humidWaningEnd, 4);
struct _sensor {
	float temp;
	float humidity;
	bool dodydSensor;
	bool humanBodyDeteckted;
};
_sensor Sensor;
struct _humid {
private:
	int humid[maxHumStored]; //storage of read humidity
	int humidStored=1;
	float humidReadArray[humidReadArraylength];
	int sessionsOfHumidRead;
public:
	bool coutnFull;
	float humidAverRead=0;
	//bool humidWarning; //triger of flashing LED and prolong rotation of fan
	int HumidAver=0;
	void addToHumidReadArray() {
		if (sessionsOfHumidRead < humidReadArraylength) {
			sessionsOfHumidRead++;
		}
		humidReadArray[sessionsOfHumidRead-1] = Sensor.humidity;
		
	}
	void evaluateHumid() {
		int i, sum = 0;
		for (i = 0; i < sessionsOfHumidRead; i++) {
			sum += humidReadArray[i]; 
		} // count average read for a minut from sensor
		humidAverRead = sum / sessionsOfHumidRead;
		if (HumidAver == 0) {//initial feal
			HumidAver = (int)humidAverRead;
			humid[0] = HumidAver;
		}
		if (humidAverRead - HumidAver > humidWarningThreshod) {
			if(!fan.humidStart)fan.humidStartOfFan();
			if (task_addToHumidArrayDuringHumWarn.ignor) {// add to humid array bit by bit to avoid conctant work in case of sharp humidity rise
				task_addToHumidArrayDuringHumWarn.reLoop();
				task_addToHumidArrayDuringHumWarn.ignor = false;
			}
			if (task_addToHumidArrayDuringHumWarn.check())humidArray();
		}
		else {
			task_addToHumidArrayDuringHumWarn.ignor = true; //deactivate add humidity
			humidArray();
			fan.humidStart = false;
			//timers[10] = millis(); // resetting timers in case of rising humid while fan on (without humid warning)
		}
		LOG("humidAverRead=" + String(humidAverRead) + "  sessionsOfHumidRead=" + String(sessionsOfHumidRead)+ "  humidStored=" +String(humidStored)+" HumidAver="+String(HumidAver))
		sessionsOfHumidRead = 0;
	}
	void populateHumidArray(float h) {
		if (!coutnFull && h){
			for (int i = 0; i< maxHumStored; i++) humid[i] = (int)h;
			coutnFull = true;
			LOG("the humidity array filled with humidity got from server " + String(h))
		}
	}
private:
	void inline humidArray() {
		int lastEntry;
		if (!coutnFull) lastEntry = humidStored;
		else lastEntry = maxHumStored;
		int sumOverall = 0;
		humid[humidStored] = (int)humidAverRead;  // for initial start before 100 have counted
		for (int i = 0; i < lastEntry; i++) { sumOverall += humid[i]; }
		HumidAver = sumOverall / lastEntry;
		LOG("HumidAver = "+ String(HumidAver))
		humidStored++;
		if (humidStored == (maxHumStored - 1)) {
			coutnFull = true;
			humidStored = 0;
		} //return to the beginning of the array when its full

	}
};
_humid humid;

void setup() {
	Serial.begin(9600);
	//esp8266.begin(9600);
	pinMode(LED1, OUTPUT);
	pinMode(LED2, OUTPUT);
	pinMode(LED3, OUTPUT);
	pinMode(deodorant, OUTPUT);
	pinMode(button, INPUT);
	digitalWrite(button, HIGH);
	pinMode(relay, OUTPUT);
	pinMode(espReset, OUTPUT);
	digitalWrite(espReset, HIGH);
	pinMode(humanSensor, INPUT);
	pinMode(humadLED, OUTPUT);
	digitalWrite(relay, LOW);
	pinMode(light, OUTPUT);
	digitalWrite(light, LOW);
}
void setTimeButton() {
	unsigned long start = millis()+1000;
	int increment = 1;
	while (start  > millis()) {
		delay(100);
		if (!digitalRead(button)) {
			start = millis() + 3000;
			fan.buttonStartFunc(fan.getButtonLevel() + increment);
			LEDcontrol(fan.getButtonLevel(),1, false);
			if (fan.getButtonLevel() == 3) {
				increment = -1;
				delay(1500);
			}
			else if (fan.getButtonLevel() == 0) {
				increment = 1;
				delay(1500);
			}
			else delay(150);
		}
	}
	if(fan.getButtonLevel()) fan.deodorantActivated = true;
	else fan.deodorantActivated = false;
}
void loop() {
	if (!digitalRead(button)) setTimeButton();
	if (task_checkFan.check()) fan.checkToWork();
	if (digitalRead(humanSensor)) {
		digitalWrite(humadLED, HIGH);
		digitalWrite(light, HIGH);
		task_lighOn.reLoop();
	}
	else if (task_lighOn.check()) {
		digitalWrite(humadLED, LOW);
		digitalWrite(light, LOW);
	}
	if (task_readSensors.check())getSensorData();
	if (task_sednData.check()) {
		humid.evaluateHumid();
		dataSending();
	}
	if(task_LED.check()) 
	{
		unsigned long setTime;
		unsigned long leftTime = fan.leftTime(setTime);
		if (leftTime)	LEDcontrol(leftTime, setTime/3, fan.humidStart);
		else LEDcontrol(0, 0, false);
	}
	//if (esp8266.available()) ReadDataSerial();
	if (Serial.available()) ReadDataSerial();
	//if (timer(11, 130000))ResetESP();
}
//=================================================================================================================================================
void deodorantSpray() {
	digitalWrite(deodorant, HIGH);
	delay(deodorantDelay);
	digitalWrite(deodorant, LOW);
}
void ResetESP() { digitalWrite(espReset, LOW); delay(500); digitalWrite(espReset, HIGH); }
void ReadDataSerial() { //reads data from ESP serial and checks for validity then sens responce by G:xxx], if gets re:from100 to 299 than LEDOK ON
	//String message = esp8266.readStringUntil('\r');
	String message = Serial.readStringUntil('\r');
	LOG(message)
	unsigned long ULValue;
	float floalValue;
	if (get_field_value(message, "humid:", &ULValue, &floalValue)) {
		LOGL("Humid recognised: ")                         // print the content
			LOG(floalValue)
			humid.populateHumidArray(floalValue);
	}
	else if (get_field_value(message, "fanStart:", &ULValue, &floalValue)) fan.webStartFunc(ULValue);
	else if (message.indexOf( "fanStop:")!= -1)fan.webStart=false;
	else if (message.indexOf("airvickPush:") != -1) deodorantSpray();
	else if (message.indexOf("lightOn:") != -1) {digitalWrite(light, HIGH);}
	else if (message.indexOf("lightOff:") != -1) { Sensor.humanBodyDeteckted = false; digitalWrite(light, LOW); }

}

bool get_field_value(String Message, String field, unsigned long* ulValue=0, float* floatValue=0) {
	int filedFirstLit = Message.indexOf(field);
	if (filedFirstLit == -1) return false;
	int fieldBegin = filedFirstLit + field.length();
	int fieldEnd = Message.indexOf(';', fieldBegin);
	String fieldString = Message.substring(fieldBegin, fieldEnd);
	*floatValue = fieldString.toFloat();
	*ulValue = fieldString.toDouble();
	LOG(*floatValue)
	LOG(*ulValue)
	return true;
}

void inline setDark() {
	digitalWrite(LED1, LOW); digitalWrite(LED2, LOW); digitalWrite(LED3, LOW);
}
void LEDcontrol(unsigned long timeLeft, unsigned long timeOfOneLED, bool flashing) {
	unsigned long brightness;
	static bool LEDon;
	if (!timeLeft && !timeOfOneLED) {
		setDark();  return;
	}
	if (flashing && LEDon) { setDark(); LEDon = false;   return; }
	LOGL("timeLeft=" + String(timeLeft) + " timeOfOneLED=" + String(timeOfOneLED) + " ")
		LOG("timeLeft:" + String(timeLeft) + ";timeOfOneLED:" + String(timeOfOneLED) + ";");
	LEDon = true;
	if (timeLeft > 2 * timeOfOneLED) {
		brightness = ((timeLeft - timeOfOneLED * 2) * 255) / timeOfOneLED;
		analogWrite(LED1, brightness);
		analogWrite(LED2, 255);
		analogWrite(LED3, 255);
		LOG("3 br=" + String(brightness))
	}
	else if (timeLeft > timeOfOneLED && timeLeft <= 2 * timeOfOneLED) {
		brightness = ((timeLeft - timeOfOneLED) * 255) / timeOfOneLED;
		analogWrite(LED1, 0);
		analogWrite(LED2, brightness);
		analogWrite(LED3, 255);
		LOG("2 br=" + String(brightness))
	}
	else if (timeLeft > 0 && timeLeft <= timeOfOneLED) {
		brightness = (timeLeft * 255) / timeOfOneLED;
		analogWrite(LED1, 0);
		analogWrite(LED2, 0);
		analogWrite(LED3, brightness);
		LOG("1 br=" + String(brightness))
	}
	else {
		analogWrite(LED3, 0);
		LEDon = false;
	}

}
bool getSensorData() {
	
	bool datamissed = false;
	switch (sensor.Read()) {
	case 2:
		LOG("CRC failed")
		datamissed = true;
		break;
	case 1:
		LOG("Sensor offline")
		datamissed = true;
		break;
	case 0:
		LOGL("Humidity: ")
		LOG(sensor.h)
		Sensor.humidity = sensor.h;
		LOGL("%\t Temperature: ");
		LOG(sensor.t)
		Sensor.temp = sensor.t;
		LOG("*C")
		break;
	}
	if (!datamissed) {
		humid.addToHumidReadArray();
		return true;		
	}
	return false;
}

void dataSending() {
	byte state = B00000000;// construction of state
	if (fan.deodorantActivated)bitWrite(state, 0, 1);
	if (Sensor.humanBodyDeteckted)bitWrite(state, 1, 1);
	if (fan.buttonStart)bitWrite(state, 2, 1);
	if (fan.fanWork)bitWrite(state, 3, 1);
	if (fan.webStart)bitWrite(state, 4, 1);
	if (fan.humidStart)bitWrite(state, 5, 1);
	if (humid.coutnFull)bitWrite(state, 6, 1);
	if (digitalRead(light))bitWrite(state, 7, 1);
	Sensor.humanBodyDeteckted = false;//reset body detection after writing to send data
	String datatToSend = "{" + String(Sensor.temp) + "," + String(humid.humidAverRead) +"," + String(humid.HumidAver) +","+ String(state,HEX) + "}";//int(state)
	Serial.println(datatToSend);
	LOG(datatToSend)
}
