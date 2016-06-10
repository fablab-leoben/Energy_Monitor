// This #include statement was automatically added by the Particle IDE.
#include "Current_Monitor/Current_Monitor.h"

// This #include statement was automatically added by the Particle IDE.
#include "CE_BME280/CE_BME280.h"

#define INFLUXDB_HOST   "server"
#define INFLUXDB_PORT   8086
#define INFLUXDB_DB     "12345"

#define SEALEVELPRESSURE_HPA (1013.25)

SYSTEM_THREAD(ENABLED) 

CE_BME280 bme; // I2C

TCPClient client;

CurrentMonitor current;

//cloud variables
double currentReading;
double currentReading_ch2;
double kwh;
double kwh_ch2;
int firmware;
int maxCurrent;
int numberOfChannels;
int sensorType;
double ACVoltageD;

float temp, pres, hum = 0;

unsigned long previousMillis = 0;
unsigned long previousMillis2 = 0;
unsigned long interval = 30000;

//local variables
float ACVoltage = 230.00;
unsigned long upTime = 0;
unsigned long lastReadTime;
int acVoltageStorageIndex = 0;
int kwhStorageStartIndex = 4;
int tripLimitIndex = 12;
double tripLimit = 200.00;
double lastReading = 0;

double wattage_ch1 = 0;
double wattage_ch2 = 0;


//Cloud Functions
int setACVoltagte(String voltage);
int clearKWH(String channel);
int setTripLimit(String limit);

bool sendInflux(String payload) {
    client.connect(INFLUXDB_HOST, INFLUXDB_PORT);
    if(client.connected())
    {
        //Particle.publish("connected");
    }
    
    client.println("POST /write?db=" + String(INFLUXDB_DB) + " HTTP/1.1");
    client.println("Host: " + String(INFLUXDB_HOST));
    client.println("User-Agent: Photon/1.0");
    client.println("Connection: close");
    client.println("Content-Type: application/json");
    client.printlnf("Content-Length: %d", payload.length());
    client.println();
    client.print(payload);
    client.println(); 
    client.flush();
    client.stop();
    
}

void setup() {
    Serial.begin(19200);
    if(!current.initialize(0,0,0,0)){
        Serial.println("Initialize failed");
    }
    firmware = current.firmwareVersion;
    maxCurrent = current.maxCurrent;
    numberOfChannels = current.numberOfChannels;
    sensorType = current.sensorType;
    ACVoltageD = (double)ACVoltage;
    Serial.println(ACVoltageD);
    getInfoFromStorage();
    Particle.variable("Firmware", firmware);
    Particle.variable("Max_Current", maxCurrent);
    Particle.variable("Sensor_type", sensorType);
    Particle.variable("Channels", numberOfChannels);
    Particle.variable("Current", currentReading);
    Particle.variable("KWH_Readings", kwh);
    Particle.variable("ACVoltage", ACVoltageD);
    Particle.function("SetACVoltage", setACVoltagte);
    Particle.function("ClearKWHs", clearKWH);
    Particle.function("Trip", setTripLimit);
    
    Serial.println(F("BME280 test"));

    if (!bme.begin()) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
    }
    delay(1000);
    //kwh = readInflux();
    
}

void loop(){
    if(current.deviceStatusReady){
        while(current.deviceStatusReady){
            
            unsigned long currentMillis2 = millis();
            if(currentMillis2 - previousMillis2 >= interval) {
            
                //Read current on channel 1
                double c1 = current.readChannelCurrent(1);
                if(c1 != current.failedCommand){
                    //Publish most recent current reading
                    currentReading = c1;
           
                    lastReading = currentReading;
                    // Calculate Kilowatt hours
                    // Calculate Wattage
                    wattage_ch1 = currentReading*ACVoltage;
                    //Calculate hours
                    upTime = millis();
                
                    double hours = (upTime - lastReadTime) / (60.00 * 60.00 * 1000.00);
                
                    //Calculate Kilowatt hours
                    kwh = kwh + ((wattage_ch1 * hours)/1000);
                    //Store kwh in eeprom so we do not loose it on power loss.
                    //EEPROM.put(kwhStorageStartIndex, kwh);
                
                    /*unsigned long currentMillis = millis();
               
                    if(currentMillis - previousMillis >= interval) {
                        bool sent = Particle.publish("CH1", String::format("{\"KWh\":%.2f,\"W\":%.2f}", kwh, wattage), 60, PRIVATE); 
                
                        previousMillis = currentMillis;
                    } */
                
                } else{
                    Serial.println("Reading of current channel 1 failed");
                }
            
                double c2 = current.readChannelCurrent(2);
                if(c2 != current.failedCommand){
                    //Publish most recent current reading
                    currentReading_ch2 = c2;
                    //lastReading = currentReading;
                    // Calculate Kilowatt hours
                    // Calculate Wattage
                    wattage_ch2 = currentReading_ch2 * ACVoltage;
                    //Calculate hours
                    double hours_ch2 = (upTime - lastReadTime)/ (60.00 * 60.00 * 1000.00);
                    lastReadTime = millis();
                    //Calculate Kilowatt hours
                    kwh_ch2 = kwh_ch2 + ((wattage_ch2 * hours_ch2)/1000);
                    //Store kwh in eeprom so we do not loose it on power loss.
                    //EEPROM.put(kwhStorageStartIndex, kwh);
                
                }else{
                    Serial.println("Reading of current channel 2 failed");
                }
                //We read current on the circuit once per second.
             
                temp = bme.readTemperature();
                pres = bme.readPressure() / 100.0F;
                hum = bme.readHumidity();
            
                bool sent = Particle.publish("CH2", String::format("{\"KWh\":%.2f,\"W\":%.2f,\"Temp\":%.2f,\"Hum\":%.2f,\"Pres\":%.2f}", kwh_ch2, wattage_ch2, temp, hum, pres), 60, PRIVATE);
                String data = "sensors,Location=Bachgasse Temp=" + String(temp, 2) + ",Hum=" + String(hum, 2) + ",Pres=" + String(pres) + ",kWh=" + String(kwh_ch2, 2) + ",P=" + String(wattage_ch2);
                //Serial.println(data);
                sendInflux(data);
                //readInflux();
                previousMillis2 = currentMillis2;
            }else{
                return;
            }
            
        }
    }else{
        Serial.println("Device not ready");
        Particle.publish("Device not ready");
        delay(1000);
        
    }
}


int setACVoltagte(String voltage){
    ACVoltage = voltage.toFloat();
    ACVoltageD = (double)ACVoltage;
    EEPROM.put(acVoltageStorageIndex, ACVoltage);
    return 1;
}

int clearKWH(String channelNumber){
    kwh = 0.00;
    EEPROM.put(kwhStorageStartIndex, kwh);
    return 1;
    
}

int setTripLimit(String limit){
    float trip = limit.toFloat();
    tripLimit = (double)trip;
    EEPROM.put(tripLimitIndex, tripLimit);
    
}

void getInfoFromStorage(){
    float tACVoltage;
    EEPROM.get(acVoltageStorageIndex, tACVoltage);
    String emptyCheck = String(tACVoltage);
    if(emptyCheck.equalsIgnoreCase("0.000000")){
        Serial.println("No ACVoltage reading stored");
    }else{
        ACVoltage = tACVoltage;
        ACVoltageD = (double)ACVoltage;
    }

    double tKWHReading;
    EEPROM.get(kwhStorageStartIndex, tKWHReading);
    String eCheck = String(tKWHReading);
    if(eCheck.equalsIgnoreCase("0.000000")){
        Serial.println("No Stored kWH readings for channel 1 \n");
    }else{
        Serial.printf("%.4f stored for Channel %i \n", tKWHReading, 1);
        kwh = tKWHReading;
    }
    
    double tTripLimit;
    EEPROM.get(tripLimitIndex, tTripLimit);
    String eCheck1 = String(tTripLimit);
    if(eCheck1.equalsIgnoreCase("0.000000")){
        Serial.println("No Stored trip limit for channel 1 \n");
    }else{
        Serial.printf("%.4f stored for Channel %i trip limit \n", tTripLimit, 1);
        tripLimit = tTripLimit;
    }
}