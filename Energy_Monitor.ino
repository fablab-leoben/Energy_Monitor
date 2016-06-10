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
    return true;
}

#define WAIT_CURRENT_TIME 2000 /* milliseconds */
#define WAIT_PUBLISH_TIME 60000 /* milliseconds */

typedef enum {
    INITIALIZE_SENSORS,
    WAIT_ONLINE,
    ALIVE,
    PUBLISH,
    SET_TIMER,
    WAIT_CURRENT,
    WAIT_PUBLISH,
    READ_CHANNEL_1,
    READ_CHANNEL_2,
    READ_SENSORS,
    CREATE_STRING
} FSM_state_t;

void setup() {
    
}

void loop()
{
    static FSM_state_t myState = WAIT_ONLINE;
    static uint32_t saveTime;
    static String data;
    static float temp;
    static float pres;
    static float hum;
    static double kwh_ch1;
    static double kwh_ch2;
    static unsigned long lastReadTime;
    static double wattage_ch1;
    static double wattage_ch2;
    static unsigned long upTime;
    static float ACVoltage = 230.00;
    static double currentReading_ch1;
    static double currentReading_ch2;
    static double lastReading;

    switch (myState) {
        
        case INITIALIZE_SENSORS:
            if(!current.initialize(0,0,0,0)){
                //Serial.println("Initialize failed");
            }
    

            if (!bme.begin()) {
                //Serial.println("Could not find a valid BME280 sensor, check wiring!");
            while (1);
            }
            //Particle.publish("Sensors initialized");
            myState = ALIVE;
            break;
    

        case WAIT_ONLINE: // stay in this state  until we're connected to the big old cloud in the sky
            if (Particle.connected()){
                myState = INITIALIZE_SENSORS;
            }
            break;

        case ALIVE: // we're alive! shout about it
            Particle.publish("Came online at", String(millis()) );
            myState = SET_TIMER; // next time through loop(), we'll set up our custom timer
            break;

        case PUBLISH: // it's been TIME_TO_WAIT milliseconds -- make some noise!
            if ( !Particle.connected() ) { // first though ... did we get disconnected somehow?
                myState = WAIT_ONLINE;
            } else {
                Particle.publish("PING!! at ", String(millis()) );
                //Particle.publish("CH2", String::format("{\"KWh\":%.2f,\"W\":%.2f,\"Temp\":%.2f,\"Hum\":%.2f,\"Pres\":%.2f}", kwh_ch2, wattage_ch2, temp, hum, pres), 60, PRIVATE);
                sendInflux(data);

                myState = SET_TIMER;
            }
            break;

        case SET_TIMER: // record the current time for the WAIT state to reference back to
            saveTime = millis();
            //Particle.publish("Timer set");
            myState = WAIT_CURRENT;
            break;

        case WAIT_CURRENT: // stay in this state until TIME_TO_WAIT milliseconds have gone by since the SET_TIMER state
            if ( millis() > (saveTime + WAIT_CURRENT_TIME) ) {
                
                // Time's up! But we'll drop out of loop() for and get it done next time through
                // This allows all the background network stuff the best chance of keeping up with business
                //Particle.publish("Ready to read current");
                myState = READ_CHANNEL_1; 
            }
            break;
            
        case WAIT_PUBLISH: // stay in this state until TIME_TO_WAIT milliseconds have gone by since the SET_TIMER state
            if ( millis() > (saveTime + WAIT_PUBLISH_TIME) ) {
                
                // Time's up! But we'll drop out of loop() for and get it done next time through
                // This allows all the background network stuff the best chance of keeping up with business
                myState = CREATE_STRING; 
            } else {
                myState = WAIT_CURRENT;
            }
            break;
            
        case READ_CHANNEL_1: 
            if( !current.deviceStatusReady){
                Particle.publish("not ready");
                myState = WAIT_CURRENT;
            } else {
                double c1 = current.readChannelCurrent(1);
                if(c1 != current.failedCommand){
                    //Particle.publish("hello 2");
                    //Publish most recent current reading
                    currentReading_ch1 = c1;
           
                    lastReading = currentReading_ch1;
                    // Calculate Kilowatt hours
                    // Calculate Wattage
                    double wattage_ch1 = currentReading_ch1*ACVoltage;
                    //Calculate hours
                    upTime = millis();
                
                    double hours_ch1 = (upTime - lastReadTime) / (60.00 * 60.00 * 1000.00);
                
                    //Calculate Kilowatt hours
                    kwh_ch1 = kwh_ch1 + ((wattage_ch1 * hours_ch1)/1000);
                    //Particle.publish(String(c1));
                    myState = READ_CHANNEL_2;
                    
                } else{
                    Particle.publish("Reading of current channel 1 failed");
                    myState = WAIT_CURRENT;
                }
            }
            break;
            
        case READ_CHANNEL_2:
            if( !current.deviceStatusReady){
                myState = WAIT_CURRENT;
            } else {
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
                    //Particle.publish(String(wattage_ch2));
                    myState = READ_SENSORS;
                }else{
                    Particle.publish("Reading of current channel 2 failed");
                    myState = WAIT_CURRENT;
                }
            }
        
        case READ_SENSORS:
            temp = bme.readTemperature();
            pres = bme.readPressure() / 100.0F;
            hum = bme.readHumidity();
            //Particle.publish(String(hum));
            myState = WAIT_PUBLISH;
            break;
        
        case CREATE_STRING:
            data = "sensors,Location=Bachgasse Temp=" + String(temp, 2) + ",Hum=" + String(hum, 2) + ",Pres=" + String(pres) + ",kWh=" + String(kwh_ch2, 2) + ",P=" + String(wattage_ch2);
            myState = PUBLISH;
            break;
        
        default:
            myState = WAIT_ONLINE;
    }
}