#ifndef FS_SPIFFS_H
#define FS_SPIFFS_H

void initSpiffs();
String readTextFile(const char* path);
void printVersion();

#endif