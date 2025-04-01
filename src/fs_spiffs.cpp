#include <Arduino.h>
#include "SPIFFS.h"
#include "fs_spiffs.h"

void initSpiffs(){

  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
}

void printVersion(){

  File file = SPIFFS.open("/version.txt");
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }
  Serial.println("File Content:");
  while(file.available()){
    Serial.write(file.read());
  }
  Serial.println(); 
  file.close();
}

/*
Use : 
  String version = readTextFile("/version.txt");
  Serial.println(version);
*/
String readTextFile(const char* path) {
  
  String content;
  File file = SPIFFS.open(path);
  if (!file) {
    Serial.println("Failed to open file");
    return "";
  }

  while (file.available()) {
    content += (char)file.read();
  }
  file.close();
  return content;
}
