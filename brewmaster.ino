//--------------------------------------------------------------------------------------
#define project_name "BREW MASTER Project by Black.Stone"
#define project_version "1.2"
// Licence: Beerware
// By: Milan Zelenka
//--------------------------------------------------------------------------------------
#include <avr/pgmspace.h>
#include <ClickEncoder.h>
#include <LiquidCrystal.h>
#include <TimerOne.h>
#include <OneWire.h>
#include <DallasTemperature.h>

/* Uncomment for DEBUG */
#define DEBUG

/* Useful macros from DateTime.h */
#define SECS_PER_MIN  (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24L)
#define numberOfSeconds(_time_) (_time_/1000 % SECS_PER_MIN)  
#define numberOfMinutes(_time_) ((_time_/1000 / SECS_PER_MIN) % SECS_PER_MIN) 
#define numberOfHours(_time_) (( _time_/1000 % SECS_PER_DAY) / SECS_PER_HOUR)

// heating interval [msec]
#define heatInterval 30000

// pTempInterval - interval (in sec) to messure temperature history
#define pTempInterval 60

// Heat Finish in auto  matic mode - degrees (celsius degrees * 10) before reaching target temperature to stop heating 
#define heatFinishDelta 50

// Heat Finish in automatic mode - power increment (in percent) if pTemp is low in heat finishing time
#define heatFinishPowerIncrement 10

// Heat Finish in automatic mode - interval (in milis) to next step heating in heat finishing time
#define heatFinishStepInterval 30000

// Temp Refresh Interval (in milis)
#define tempRefreshInterval 300

// Pin Settings
#define lcdBckLightPin 13
#define lcdDb7Pin 12
#define lcdDb6Pin 11
#define lcdDb5Pin 10
#define lcdDb4Pin 9
#define lcdEnablePin 8
#define lcdRsPin 7
#define mixRelayPinOnOff 6
#define mixRelayPinSpeed 5
#define tempSensorPin 4
#define buzzerPin 3
#define heatRelayPin 2


String inCmd = "";
unsigned long lastReadTime, getMillis;
long timerValue = 0, timerLast = 0, timerStep = 0, enc2Last = 1;
int heatPower = 0, targetTemp = 30, tempStep = 0, heatFinishStep = 0, pTempStep = 0, tempValue1 = 0, tempValue2 = 0, pTemp = 0, tempHistory[pTempInterval];
boolean heatOn = false, lcdBckLightOn = false, manualModeOn = true, heatFinish = false;
byte mixOn = 0, timerConfigOn = 0; //timerConfigOn = {0 - countdown or standby, 1 - set mode, 2 - alarm mode}
ClickEncoder *enc1, *enc2;
char enc1Step = 0, enc1Last = -1, enc2Step = 0;

LiquidCrystal lcd(lcdRsPin, lcdEnablePin, lcdDb4Pin, lcdDb5Pin, lcdDb6Pin, lcdDb7Pin);

OneWire oneWire(tempSensorPin);
DallasTemperature tempSensors(&oneWire);


/**
 * FUNCTIONS
*/

void timerIsr() {
  enc1->service();
  enc2->service();
}

#if defined(DEBUG)
int freeRam () 
{
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
} // int freeRam

void doCmd(String cmd)
{
  cmd.trim();
  if (cmd.equals("free")) {
      Serial.print(F("Free memory: "));
      Serial.println(freeRam());
  } 
  else
  if (cmd.equals("version")) {
      Serial.print(F(project_name));
      Serial.print(F(" "));
      Serial.println(F(project_version));
  } else { 
      Serial.println(F("Bad command."));
  }
} // void doCmd
#endif

void buzzer(boolean on) {
  if (on) { analogWrite(buzzerPin, 40); } else { analogWrite(buzzerPin, 0); }
}


void tempRefresh() {
  tempSensors.setWaitForConversion(false);
  tempSensors.requestTemperatures();
  tempSensors.setWaitForConversion(true);
  tempValue1 = tempSensors.getTempCByIndex(0)*10;
  tempValue2 = tempSensors.getTempCByIndex(1)*10;
}

void turnHeat(boolean on = !heatOn) {
#if defined(DEBUG)
  Serial.print(F("Heat: "));
  Serial.println(on ? F("ON") : F("OFF"));
#endif
  digitalWrite(heatRelayPin, !on);
  heatOn = on;
}

void turnMix(boolean on = -1) {
#if defined(DEBUG)
  Serial.print(F("Mix: "));
  Serial.println(mixOn);
#endif
  if (on >=0 && on <= 2) { mixOn = on; } else {
    if (mixOn >=2) { mixOn=0; } else { mixOn++; }
  }
  digitalWrite(mixRelayPinOnOff, !(bitRead(mixOn, 0) | bitRead(mixOn, 1)));
  digitalWrite(mixRelayPinSpeed, !bitRead(mixOn, 1));
}

void turnBckLight(boolean on = !lcdBckLightOn) {
#if defined(DEBUG)
  Serial.print(F("Bcklight: "));
  Serial.println(on ? F("ON") : F("OFF"));
#endif
  digitalWrite(lcdBckLightPin, on);
  lcdBckLightOn = on;
}

void turnManualMode(boolean on = !manualModeOn) {
#if defined(DEBUG)
  Serial.print(F("Mode: "));
  Serial.println(on ? F("MAN") : F("AUTO"));
#endif
  manualModeOn = on;
}

void turnTimerConfig(boolean on = -1) {
#if defined(DEBUG)
  Serial.print(F("TimerConfig: "));
//  Serial.println(on ? F("ON") : F("OFF"));
#endif

  if (on >=0 && on <= 2) { timerConfigOn = on; } else {
    if (timerConfigOn >=1) {
      if (timerConfigOn == 2) {
        buzzer(false);
        lcd.display();
      }
      timerConfigOn=0;
    } else {
      timerConfigOn++;
    }
  }

  lcdTimerSet();
}


void lcdWelcome() {
#if defined(DEBUG)
  doCmd("version");
  doCmd("free");
#endif
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(F("----------------"));
  lcd.setCursor(0,1);
  lcd.print(F(" * BREWMASTER *"));
  lcd.setCursor(-4,2);
  lcd.print(F(" by Black.Stone")); 
  lcd.setCursor(-4,3);
  lcd.print(F("----------------"));
}

void lcdMode() {
  lcd.setCursor(-3,3);
  lcd.print((manualModeOn) ? "MAN " : "AUTO");
}

void lcdMix() {
  lcd.setCursor(2,3);
  lcd.print((mixOn > 0) ? ((mixOn > 1) ? "MIX" : "mix") : "   ");

}

void lcdHeat() {
  lcd.setCursor(7,3);
  lcd.print((heatOn) ? "HEAT" : "    ");
}

void lcdTemp() {
  lcd.setCursor(-1,2);
  lcd.print((float)(tempValue1)/10,1);
  lcd.setCursor(5,2);
  lcd.print((float)(tempValue2)/10,1);
}

void lcdPower() {
  lcd.setCursor(1,0);
  switch (heatPower) {
    case 0:
      lcd.print(F("OFF  "));
      break;
    case 100:
      lcd.print(F("FULL "));
      break;
    default:
      lcd.print(heatPower);
      lcd.print(F("%  "));
      break;
  } //switch
}

void lcdTargetTemp() {
  lcd.setCursor(1,0);
  lcd.print(targetTemp);
  lcd.print((char)223);
  lcd.print(F("C "));
}
   
void lcdPTemp() {
  lcd.setCursor(8,0);
  lcd.print((pTemp>0) ? "+" : "-");
  lcd.print((float)(abs(pTemp))/10,1);
  lcd.print((char)223);
  lcd.print("/m");
}

void lcdTimer() {
  lcd.setCursor(4,1);
  byte h = numberOfHours(timerValue);
  lcd.print((h<10) ? "0" : "");
  lcd.print(h);
  lcd.print(":");
  byte m = numberOfMinutes(timerValue);
  lcd.print((m<10) ? "0" : "");
  lcd.print(m);
  lcd.print(":");
  byte s = numberOfSeconds(timerValue);
  lcd.print((s<10) ? "0" : "");
  lcd.print(s);
}

void lcdTimerSet() {
  if (timerConfigOn != 0) {
    lcd.setCursor(3,1);
    lcd.print(">");
    lcd.setCursor(12,1);
    lcd.print("<");
  } else {
    lcd.setCursor(3,1);
    lcd.print(" ");
    lcd.setCursor(12,1);
    lcd.print(" ");
  }
}

void lcdStatic() {
  lcd.clear();
  lcd.setCursor(0,1);
  lcd.print(F(" -------------- "));
  lcd.setCursor(-4,2);
  lcd.print(F("T:")); 
  lcd.setCursor(10,2);
  lcd.print((char)223);
  lcd.print(F("C")); 
  lcdMode();
  lcdMix();
  lcdHeat();
  lcdTemp();
  lcdPTemp();
  lcdPower();
  lcdTimer();
  lcdTimerSet();
}
/**
 * SETUP
*/

void setup(void) {
#if defined(DEBUG)
//  Serial.begin(112500);
  Serial.begin(9600);
#endif
  // LCD init
  lcd.begin(16,4);
  lcdWelcome();

  pinMode(heatRelayPin, OUTPUT);
  pinMode(mixRelayPinSpeed, OUTPUT);
  pinMode(mixRelayPinOnOff, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  turnHeat(false);
  turnMix(0);
  turnManualMode(true);
  buzzer(true);
  delay(20);
  buzzer(false);

  enc1 = new ClickEncoder(A4, A3, A5);
  enc1->setAccelerationEnabled(false);

  enc2 = new ClickEncoder(A1, A0, A2);
  enc2->setAccelerationEnabled(false);  
  
  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr); 

  // LCD Backlight
  pinMode(lcdBckLightPin, OUTPUT);
  turnBckLight(true);

  // Temperature sensors init
  tempSensors.begin();
  tempRefresh();
  delay(700);
  tempRefresh();
  lcdStatic();

  // populate 60 sec. history from current temperature
  int avgTemp = (tempValue1+tempValue2)/2;
  for (byte i = 0; i <= pTempInterval; i++) { tempHistory[i] = avgTemp; }
}

/**
 * LOOP
*/
void loop(void) {
#if defined(DEBUG)
  // **** COMMANDLINE ****
  if (Serial.available() > 0) {
    char inChar = Serial.read();
    if (inChar == '\n' && inCmd != "\r") {
      doCmd(inCmd);
      noInterrupts();
      inCmd = "";
      interrupts ();
    } else if (inCmd != "\r") {
      noInterrupts ();
      inCmd += String(inChar);
      interrupts();
    } else {
      noInterrupts();
      inCmd = "";
      interrupts();
    }
  } // if Serial.available
#endif

  // **** HEATING TIMER ****

  getMillis=millis() - lastReadTime;

  if (heatOn && heatPower < 100 && getMillis >= heatInterval / 100 * heatPower) {
#if defined(DEBUG)
    Serial.print(F("Stop heating after "));
    Serial.print(heatInterval / 100 * heatPower);
    Serial.println(F("ms."));
#endif
    turnHeat(false);
    lcdHeat();
  }
  
  if (!heatOn && getMillis < heatInterval / 100 * heatPower) {
    turnHeat(true);
    lcdHeat();
  }
  
  if (heatPower > 0 && getMillis >= heatInterval) {
    lastReadTime = millis();
    turnHeat(true);
    lcdHeat();
  } // if getMillis


  // **** HEATING Power Encoder ****
  
  enc1Step = enc1->getValue();
  if (enc1Step != 0) {
    if (manualModeOn) {
      heatPower += enc1Step;
      if (heatPower > 100) { heatPower = 100; }
      if (heatPower < 0) { heatPower = 0; }
    } else {
      targetTemp += enc1Step;
      if (targetTemp > 100) { targetTemp = 100; }
      if (targetTemp < 10) { targetTemp = 10; }
    }
  }
 
  if (manualModeOn && enc1Last != heatPower) {
    enc1Last = heatPower;
    lcdPower();
  } else if(!manualModeOn && enc1Last != targetTemp) {
    enc1Last = targetTemp;
    lcdTargetTemp();
  }
  
  // **** HEATING Power Button ****
  ClickEncoder::Button b = enc1->getButton();
  switch (b) {
    case ClickEncoder::Clicked:
      turnMix();
      lcdMix();
      break;
    case ClickEncoder::Released:
#if defined(DEBUG)
      Serial.println("HEAT cycle reset...");
#endif
      lastReadTime = 0;
      heatOn = false;
      break;
    case ClickEncoder::DoubleClicked:
      turnManualMode();
      lcdMode();
      break;
  }

  // Encoder - Timer
  enc2Step = enc2->getValue();
  if (timerConfigOn == 1 && enc2Step != 0) {
    timerValue += enc2Step*30000;
    if (timerValue > 360000000) { timerValue = 360000000; }
    if (timerValue < 0) { timerValue = 0; }
    lcdTimer();
  }

  if (enc2Last != timerValue) {
    enc2Last = timerValue;
    lcdTimer();
//    Serial.print(F("Timer: "));
//    Serial.println(timerValue);
  }

  ClickEncoder::Button b1 = enc2->getButton();
  switch (b1) {
    case ClickEncoder::DoubleClicked:
      turnBckLight();
      break;
    case ClickEncoder::Released:
#if defined(DEBUG)
      Serial.println("Timer reset...");
#endif
      timerValue = 0;
      timerConfigOn = 0;
      lcdTimer();
      lcdTimerSet();
      break;
    case ClickEncoder::Clicked:
      turnTimerConfig();
      timerStep = 0;
      break;
  }
  
  long timerNow = millis();
  if (timerValue > 0 && timerConfigOn == 0) {
    timerStep += timerNow - timerLast;
 
    if ((timerValue - timerStep) < 0) {
      timerValue = 0;
      timerStep = 0;
      lcdTimer();
      turnTimerConfig(2); // alarm mode
    } else
    
    if (timerStep > 1000) {
       timerValue = timerValue - timerStep;
       timerStep = 0;
       lcdTimer();
    }
  } //if
  
    //alarm mode
  if (timerValue == 0 && timerConfigOn == 2) {
    timerStep += timerNow - timerLast;
    if (timerStep > 900) {
      buzzer(false);
      lcd.noDisplay();
      timerStep=0;
    } else if (timerStep > 500) {
      buzzer(true);
      lcd.display();
    }
  }

  // TEMP sensors refresh
  tempStep += timerNow - timerLast;
  if (tempStep > tempRefreshInterval) {
    tempRefresh();
    lcdTemp();
    tempStep = 0;
  }


  // CALCULATING TEMPERATURE HISTORY - pTemp

  pTempStep += timerNow - timerLast;
  if (pTempStep > 1000) {
    tempHistory[pTempInterval]=(tempValue1+tempValue2)/2;
    memcpy(&tempHistory[0], &tempHistory[1], sizeof(tempHistory));
    pTemp = (tempHistory[pTempInterval-1]-tempHistory[0]);
    lcdPTemp();
    pTempStep = 0;
  }


  // AUTOMATIC HEAT CONTROL BY TARGET TEMPERATURE
  
  if (!manualModeOn) {
    int deltaTemp = targetTemp*10 - (tempValue1+tempValue2)/2;
    
    if (deltaTemp > heatFinishDelta && heatFinish) {
      heatPower = 100;
      heatFinish = false;
#if defined(DEBUG)
      Serial.print(F("AUTO: deltaTemp = "));
      Serial.print((float)(deltaTemp)/10,1);
      Serial.print(F(" > "));
      Serial.print((float)(heatFinishDelta)/10,1);
      Serial.println(F(" - start heating on 100%"));
#endif
    } else if (deltaTemp < 0 && heatFinish) {
      heatPower = 0;
      heatFinish = false;
#if defined(DEBUG)
      Serial.print(F("AUTO: deltaTemp = "));
      Serial.print((float)(deltaTemp)/10,1);
      Serial.println(F(" < 0 - stop heating"));
#endif
    } else if (deltaTemp >= 0 && deltaTemp <= heatFinishDelta && !heatFinish) {
      heatPower = 0;
      heatFinish = true;
      heatFinishStep = 0;
#if defined(DEBUG)
      Serial.print(F("AUTO: 0 > deltaTemp = "));
      Serial.print((float)(deltaTemp)/10,1);
      Serial.print(F(" < "));
      Serial.print((float)(heatFinishDelta)/10,1);
      Serial.println(F(" - heat finishing, heat power = 0%"));
#endif
    }
    
    if (heatFinish) {
        heatFinishStep += timerNow - timerLast;
        if (heatFinishStep > heatFinishStepInterval) {
#if defined(DEBUG)
            Serial.print(F("temp increment = "));
            Serial.println((float)(pTemp)/10,1);
            Serial.print(F("expected increment: "));
            Serial.print((float)(deltaTemp)/100,2);
            Serial.print(F(" < increment < "));
            Serial.println((float)(2*deltaTemp)/100,2);
#endif
          if (pTemp < deltaTemp/10) {
            heatPower = heatPower + heatFinishPowerIncrement;
            if (heatPower > 100) { heatPower = 100; }
#if defined(DEBUG)
            Serial.print(F("AUTO (heat finish): temp increment is lower than expected, rising heat power, current heat power = "));
            Serial.println(heatPower);
#endif
          } else if (pTemp > 2*deltaTemp/10) {
            heatPower = 0;
#if defined(DEBUG)
            Serial.println(F("AUTO (heat finish): temp increment is higher than expected, stop heating, heat power = 0"));
#endif
          } else {
#if defined(DEBUG)
            Serial.print(F("AUTO (heat finish): temp increment is OK, heat power = "));
            Serial.println(heatPower);
#endif
          }
          heatFinishStep = 0;
        }
    } // fi heatFinish
  } // fi !manualModeOn
  
  timerLast = timerNow;
  
  

} // loop
/* ------------------------------------------------------------------------------- */
