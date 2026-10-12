#include <Arduino.h>
#include <Wire.h>
// USB4000 recovery V2: I2C EEPROM 0x51, bounded surgical firmware restoration.
// Source: verified double dump + manufacturer's v3.0.2. No access to EEPROM 0x53 writes.
static const int SDA_PIN=21, SCL_PIN=22;
static const uint8_t DEV=0x51, BLOCK=16;
static const uint32_t EEPROM_LEN=32768;
static const uint16_t PATCH_MIN=0x4000, PATCH_MAX=0x43B0;
static const char TARGET_SHA[]="0400a96eaa746c4cede0439f796e943c0ad1f8730c8a792d28e64e3d4acc5f49";
static bool busReady=false, armed=false;
static bool readBlock(uint8_t addr,uint16_t off,uint8_t *b) {
  Wire.beginTransmission(addr);
  Wire.write((uint8_t)(off>>8));Wire.write((uint8_t)off);
  if(Wire.endTransmission(false)!=0)return false;
  if(Wire.requestFrom((uint16_t)addr,(uint8_t)BLOCK,true)!=BLOCK)return false;
  for(uint8_t i=0;i<BLOCK;i++){if(!Wire.available())return false;b[i]=Wire.read();}
  return true;
}
static int nibble(char c) {
  if(c>='0'&&c<='9')return c-'0';
  if(c>='A'&&c<='F')return c-'A'+10;
  if(c>='a'&&c<='f')return c-'a'+10;
  return -1;
}
static bool unhex(const String& h,uint8_t* b) {
  if(h.length()!=BLOCK*2)return false;
  for(uint8_t i=0;i<BLOCK;i++){
    int a=nibble(h[2*i]),c=nibble(h[2*i+1]);
    if(a<0||c<0)return false;
    b[i]=(a<<4)|c;
  }
  return true;
}
static void scan(){
  const uint8_t addresses[]={0x4f,0x51,0x53};
  for(uint8_t addr:addresses){
    Wire.beginTransmission(addr);
    uint8_t st=Wire.endTransmission(true);
    Serial.printf("ADDR %02X %s STATUS=%u\n",addr,st==0?"ACK":"NOACK",st);
  }
  Serial.println("SCAN_END");
}
static void dump(uint8_t addr){
  if(addr!=0x51&&addr!=0x53){Serial.println("ERR INVALID_DUMP_ADDR");return;}
  uint8_t b[BLOCK];
  Serial.printf("BEGIN %02X %lu\n",addr,(unsigned long)EEPROM_LEN);
  for(uint32_t off=0;off<EEPROM_LEN;off+=BLOCK){
    if(!readBlock(addr,(uint16_t)off,b)){
      Serial.printf("ERR I2C 0x%02X OFFSET=%lu\n",addr,(unsigned long)off);return;
    }
    Serial.printf("DATA %04lX ",(unsigned long)off);
    for(uint8_t j=0;j<BLOCK;j++)Serial.printf("%02X",b[j]);
    Serial.println();
  }
  Serial.printf("END %02X %lu\n",addr,(unsigned long)EEPROM_LEN);
}
static void patch(String cmd) {
  if(!busReady||!armed){Serial.println("PATCH_ERR_NOT_ARMED");return;}
  int a=cmd.indexOf(' ',6),b=cmd.indexOf(' ',a+1);
  if(a<0||b<0){Serial.println("PATCH_ERR_FORMAT");return;}
  String addrText=cmd.substring(6,a);
  String beforeText=cmd.substring(a+1,b);
  String afterText=cmd.substring(b+1);
  char *endp=nullptr;
  unsigned long off=strtoul(addrText.c_str(),&endp,16);
  if(!endp||*endp!='\0'||addrText.length()!=4||
     off<PATCH_MIN||off>PATCH_MAX||off%BLOCK!=0){
    Serial.println("PATCH_ERR_RANGE");return;
  }
  uint8_t before[BLOCK],after[BLOCK],current[BLOCK],check[BLOCK];
  if(!unhex(beforeText,before)||!unhex(afterText,after)){
    Serial.println("PATCH_ERR_HEX");return;
  }
  if(!readBlock(DEV,(uint16_t)off,current)){
    Serial.println("PATCH_ERR_PREREAD");return;
  }
  if(memcmp(current,after,BLOCK)==0){
    Serial.printf("PATCH_ALREADY %04lX\n",off);return;
  }
  if(memcmp(current,before,BLOCK)!=0){
    Serial.printf("PATCH_ERR_PRECONDITION %04lX\n",off);return;
  }
  Wire.beginTransmission(DEV);
  Wire.write((uint8_t)(off>>8));Wire.write((uint8_t)off);
  for(uint8_t i=0;i<BLOCK;i++)Wire.write(after[i]);
  uint8_t result=Wire.endTransmission(true);
  if(result!=0){Serial.printf("PATCH_ERR_I2C %04lX %u\n",off,result);return;}
  delay(8);
  if(!readBlock(DEV,(uint16_t)off,check)||memcmp(check,after,BLOCK)!=0){
    Serial.printf("PATCH_ERR_VERIFY %04lX\n",off);return;
  }
  Serial.printf("PATCH_OK %04lX\n",off);
}
void setup(){
  Serial.begin(115200);Serial.setTimeout(1200);delay(250);
  busReady=Wire.begin(SDA_PIN,SCL_PIN,100000);
  Serial.printf("USB4000_I2C_V2 READY=%u SDA=%d SCL=%d READ_ONLY_DEFAULT=1\n",busReady?1:0,SDA_PIN,SCL_PIN);
}
void loop(){
  if(!Serial.available()){delay(2);return;}
  String cmd=Serial.readStringUntil('\n');cmd.trim();
  String upper=cmd;upper.toUpperCase();
  if(upper=="PING"){
    Serial.printf("PONG USB4000_I2C_V2 BUS=%u ARMED=%u\n",busReady?1:0,armed?1:0);
  }else if(upper=="DISARM"){
    armed=false;Serial.println("DISARMED");
  }else if(!busReady){
    Serial.println("ERR_BUS_NOT_READY");
  }else if(upper=="SCAN"){
    scan();
  }else if(upper=="DUMP 0X51"||upper=="DUMP 0X53"){
    dump(upper=="DUMP 0X51"?0x51:0x53);
  }else if(cmd.startsWith("ARM ")){
    String token=cmd.substring(4);
    if(token==TARGET_SHA){armed=true;Serial.println("ARMED_TARGET_SHA_VERIFIED");}
    else{armed=false;Serial.println("ERR_ARM_SHA_MISMATCH");}
  }else if(upper.startsWith("PATCH ")){
    patch(upper);
  }else{
    Serial.println("ERR COMMANDS PING SCAN DUMP ARM PATCH DISARM");
  }
}
