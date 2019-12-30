#include <ESP8266WiFi.h>
#include <PubSubClient.h>  // MQTT 
#include <phi_interfaces.h>  

const char *ssid =  "Smartbox";    // cannot be longer than 32 characters!
const char *pass =  "topsecret ";   //


// Update these with values suitable for your network.
IPAddress server(192, 168, 8, 92);

WiFiClient wclient;
PubSubClient client(wclient, server);
int lanstat=0;


//#define LEDPWM     15
#define Encoder1ChnA  5
#define Encoder1ChnB  4
#define EncoderDetent 20
#define EncoderPush 0
#define ZeroCross  14
#define POut 2 
//#define MaxVal 100
#define Channels 4
//#define Step MaxVal(ch)/EncoderDetent


char mapping1[]={'U','D'}; // This is a rotary encoder so it returns U for up and D for down on the dial.
phi_rotary_encoders my_encoder1(mapping1, Encoder1ChnA, Encoder1ChnB, EncoderDetent);
multiple_button_input* dial1=&my_encoder1;

char mapping[]={'P'}; // This is a list of names for each button.
byte pins[]={EncoderPush}; // The digital pins connected to the 6 buttons.
phi_button_groups my_btns(mapping, pins, 1);
multiple_button_input* pad1=&my_btns;

int channel=0;
long dimmervalue=0;
int val[Channels]={0,0,0,60};
char* items[]={"DimmedLight","RGBLight","",""};
int  itemtype[Channels]={0,12,10,11}; //0-onBoard Dimmer, 1-Ext Dimmer, 2-Switch, 12-RGBVol, 10-RGBColour, 11-RGBSaturation  
char inprefix[]="/myhome/in/";
char outprefix[]="/myhome/out/";
String subscribestr="/myhome/#";
int curchan=0;
int imp=0;


int MaxVal(short ch)
{
  if (itemtype[ch]==10) return 365;
  return 100;
  }
long k=0;
long t=0;
 //uint32_t savedPS;
void zero()
{
 // if (!digitalRead(ZeroCross)) exit;  //falled
 detachInterrupt(ZeroCross);  timer1_write(43000);
//noInterrupts();
//#define TIM_DIV1   0 //80MHz (80 ticks/us - 104857.588 us max)
//#define TIM_DIV16 1 //5MHz (5 ticks/us - 1677721.4 us max)  <----
//#define TIM_DIV265  3 //312.5Khz (1 tick = 3.2us - 26843542.4 us max)
  if (dimmervalue==1) digitalWrite(POut,0); 
  else
  {
  digitalWrite(POut,1);
  imp=0;
  if (dimmervalue>0) 
              {
              k=micros();
              timer1_write(dimmervalue);
 //             savedPS = xt_rsil(0); // this routine will allow level 2 and above 
              
              }
  } 
  //interrupts();
}

void thandler()
{
  
   // xt_wsr_ps(savedPS); // restore the state  
   // digitalWrite(POut,0);
    
  
  if ((imp==0) && dimmervalue)
     {  
      t=micros();
      digitalWrite(POut,0);
      timer1_write(2000); ///2000
      imp=1;
     }
  else   
  {   
  digitalWrite(POut,1);
  imp=0;
  
attachInterrupt(ZeroCross,zero,ONHIGH);
  }
 
}

void UpdateChannel(int chan,int pub)
{
      char buf[64];
      Serial.print("Channel: ");
      Serial.println(chan);
        
      Serial.print("Value: ");
      Serial.println(val[chan]);

      while (strlen(items[chan])==0) chan--;
     
  
  switch (itemtype[chan])
   {
   
   int  intbuf[3]; 
   case 0:      
   if (val[chan]>0) 
      {
///      analogWrite(LEDPWM,map(val[chan],0,MaxVal(0),0,PWMRANGE));
      dimmervalue=map(val[chan],0,MaxVal(0),43000,1);
      Serial.println(dimmervalue);
      snprintf(buf,sizeof(buf),"%d",val[chan]);
      }
      else 
      {
      digitalWrite(POut,1);
      dimmervalue=0;
      snprintf(buf,sizeof(buf),"%d",0);
      }
   
    
   break;
   case 10:
   case 11:
   case 12:
   for (short i=0;i<3;i++) intbuf[itemtype[chan+i]-10]=val[chan+i];
   for (short i=0;i<3;i++) if (intbuf[i]<0) intbuf[i]=0;
   snprintf (buf,sizeof(buf),"%d,%d,%d",intbuf[0],intbuf[1],intbuf[2]);     
   }

//Serial.println(dimmervalue); 

  if (pub)
  { 
    String topic=outprefix+String(items[chan]);
    
     client.publish(topic, buf); 
      
      Serial.print("Publish: ");
      Serial.print(topic);
      Serial.print(":");
      Serial.println(buf);
   
    }
}

void rotary_loop()
{
  char temp;
  //int t;
  temp=dial1->getKey(); // Use the phi_interfaces to access the same keypad
  switch (temp) {
  case 'U': val[channel]+=MaxVal(channel)/EncoderDetent;
  
  break;
  case 'D': val[channel]-=MaxVal(channel)/EncoderDetent;
 
  }
  
  
  if (temp!=NO_KEY) 
  {  
   // Logic_DimmableLight(LEDPWM);
    Serial.print("Key: ");  Serial.println(temp);
    
    Serial.print(t);Serial.print("-");Serial.print(k);Serial.print("=");Serial.println(t-k);
    
     if (val[channel]>MaxVal(channel)) val[channel]=MaxVal(channel);
     
     if (val[channel]<0) 
            {
            val[channel]=0;
 
            }
     UpdateChannel(channel,1);
   Serial.println(val[channel] );
    
  }
  temp=my_btns.getKey(); // Use phi_button_groups object to access the group of buttons

  
  
  if (temp=='P') 
      {
        switch (pad1->get_status()) {
        case buttons_pressed:
           curchan=channel;
           if ((val[0] <0)||(Channels==1)) {val[0]=-val[0]; UpdateChannel(0,1);}
              else 
                  channel++;
        break;
        case buttons_held:
        channel=curchan;
        if (val[channel]>0) {val[channel]=-val[channel]; UpdateChannel(channel,1);}
        }
        
      if (channel>=Channels) channel=0;
     // Serial.println(t);
      
      }
}


int getInt(char ** chan)
{
  if (!*chan) return -1;
  int ch = atoi(*chan);
  *chan=strchr(*chan,',');
  
  if (*chan) *chan+=1;
//  Serial.print("Par:"); Serial.println(ch);
  return ch;
  
}

void callback(const MQTT::Publish& pub) { 
 
  //String strPayload = String(pub.payload_string());
 
   const char * strTopic = (pub.topic().c_str());  //const_cast<char*>
    Serial.println(strTopic);
    Serial.println(pub.topic().c_str());
    
   short intopic  = strncmp(strTopic,inprefix,strlen(inprefix));
   short outtopic = strncmp(strTopic,outprefix,strlen(outprefix));
   char subtopic[20]="";
   char * t;
   if (t=strrchr (strTopic,'/')) strncpy(subtopic,t+1 , 20);
  Serial.print(strTopic);
  Serial.print("=");
  char * strPayload  = const_cast<char*> (pub.payload_string().c_str());
  Serial.println(strPayload);
   
   
   if (!intopic || !outtopic)
   {
   int Par1 = getInt(&strPayload);
   int Par2 = getInt(&strPayload);
   int Par3 = getInt(&strPayload);
   
   int rgb = 0;
   short k;
     
    for (short ch=0;ch<Channels;ch++)
    
       if (!(k=strcmp(subtopic,items[ch])) || (rgb>0)) 
       { 

        switch (itemtype[ch]){
         case 0: 
         case 1:val[ch]=Par1;
         //if (!intopic) 
         UpdateChannel(ch,0);
         break;
         case 10:val[ch]=Par1;
         if (!k) rgb=3;
         break;
         case 11:val[ch]=Par2;
         if (!k) rgb=3;
         break;
         case 12:val[ch]=Par3;
         if (!k) rgb=3;
                 }
         if (rgb) 
                  {
                  rgb--;
                   if (/*!intopic && */!rgb) UpdateChannel(ch-2,0);
                  }             
       } 
   }   
}


unsigned long lastMqtt = 0;


void setup() {
  Serial.begin(115200);
  Serial.println("start");



 
  pinMode(POut,OUTPUT);
  pinMode(ZeroCross,INPUT);

 


attachInterrupt(ZeroCross,zero,ONHIGH); ///RISING - really on change
timer1_isr_init();
timer1_attachInterrupt(thandler);
timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE); //16
  
  //interrupts();
  //analogWriteFreq(500); 
  //analogWriteRange(255); 
  //pinMode(LEDPWM, OUTPUT);
 }

void loop() {

rotary_loop();

  switch (lanstat) {  
  case 0:  
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting to ");
    Serial.print(ssid);
    Serial.println("...");
    WiFi.begin(ssid, pass);
    lanstat=1;
  }
  break;
  
  case 1:
  
    if (WiFi.waitForConnectResult() == WL_CONNECTED)
     {lanstat=2;
    Serial.println("WiFi connected");
   } 
  break;

  case 2:
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (client.connect("arduinoClient"+WiFi.macAddress())) {
  client.publish("outTopic","hello world");
  client.set_callback(callback);
 for (short ch=0;ch<Channels;ch++) if (strlen(items[ch])>0) UpdateChannel(ch,1);
  client.subscribe(subscribestr);
      }
    }
  }   

    if (client.connected())
      client.loop();
  }
}



