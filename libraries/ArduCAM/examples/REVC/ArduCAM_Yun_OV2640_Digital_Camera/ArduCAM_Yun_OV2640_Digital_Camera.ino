// ArduCAM demo (C)2014 Lee
// web: http://www.ArduCAM.com
// This program is a demo of how to use most of the functions
// of the library with a supported camera modules, and run on Arduino Yun.
//
// This demo was made for Omnivision OV2640 2MP sensor.
// It will run the ArduCAM as a real 2MP digital camera, provide both preview and JPEG capture.
// The demo sketch will do the following tasks:
// 1. Set the sensor to BMP preview output mode.
// 2. Switch to JPEG mode when shutter buttom pressed.
// 3. Capture and buffer the image to FIFO. 
// 4. Store the image to Micro SD/TF card with JPEG format.
// 5. Resolution can be changed by myCAM.set_JPEG_size() function.
// This program requires the ArduCAM V3.4.0 (or later) library and Rev.C ArduCAM shield
// and use Arduino IDE 1.5.8 compiler or above

#include <UTFT_SPI.h>
#include <FileIO.h>
#include <Process.h>
#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include "memorysaver.h"

#define SD_CS 9 
// set pin 10 as the slave select for SPI:
const int SPI_CS = 10;

ArduCAM myCAM(OV2640,SPI_CS);
UTFT myGLCD(SPI_CS);
boolean isShowFlag = true;

void setup()
{
  uint8_t vid,pid;
  uint8_t temp; 
  
  #if defined (__AVR__)
    Wire.begin(); 
  #endif

  Serial.begin(115200);
  Serial.println("ArduCAM Start!"); 
  // set the SPI_CS as an output:
  pinMode(SPI_CS, OUTPUT);
  Bridge.begin();
  // initialize SPI:
  SPI.begin(); 
  //Check if the ArduCAM SPI bus is OK
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  temp = myCAM.read_reg(ARDUCHIP_TEST1);
  if(temp != 0x55)
  {
  	Serial.println("SPI interface Error!");
  	while(1);
  }
  
  //Change MCU mode
  myCAM.set_mode(MCU2LCD_MODE);

  myGLCD.InitLCD();
  
  //Check if the camera module type is OV2640
  myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
  if((vid != 0x26) || (pid != 0x42))
  	Serial.println("Can't find OV2640 module!");
  else
  	Serial.println("OV2640 detected!");
  	
  //Change to BMP capture mode and initialize the OV2640 module	  	
  myCAM.set_format(BMP);

  myCAM.InitCAM();
  
  //Initialize SD Card
  if (! FileSystem.begin()) 
  {
    //while (1);		//If failed, stop here
    Serial.println("SD Card Error!");
    //while(1);
  }
  else
  {
    Serial.println("SD Card detected!");
  }
  if(!FileSystem.exists("/mnt/sda1/arduino/"))
  {
    if(FileSystem.mkdir("/mnt/sda1/arduino"))
    {
      Serial.println("Make arduino floder done.");
    }
    else
    {
      Serial.println("Make arduino floder failed!");
      //while(1);
    }
  }
  else
  {
    Serial.println("The arduino floder is already existed.");
  }
  if(!FileSystem.exists("/mnt/sda1/DCIM/"))
  {
    if(FileSystem.mkdir("/mnt/sda1/DCIM"))
    {
      Serial.println("Make DCIM floder done.");
    }
    else
    {
      Serial.println("Make DCIM floder failed!");
      //while(1);
    }
  }
  else
  {
    Serial.println("The DCIM floder is already existed.");
  }
  Process p;
  p.runShellCommand("ln -s /mnt/sda1/DCIM /www/DCIM");
  p.running();
}

void loop()
{
  char str[8];
  char path[40];
  char filename[20];
  memset(path,0,40);
  memset(filename,0,20);
  byte buf[256];
  static int i = 0;
  static int k = 0;
  static int n = 0;
  uint8_t temp,temp_last;
  uint8_t start_capture = 0;
  int total_time = 0;

  //Wait trigger from shutter buttom   
  if(myCAM.get_bit(ARDUCHIP_TRIG , SHUTTER_MASK))	
  {
    isShowFlag = false;
    myCAM.set_mode(MCU2LCD_MODE);
    myCAM.set_format(JPEG);
    myCAM.InitCAM();

    myCAM.OV2640_set_JPEG_size(OV2640_640x480);
    //myCAM.OV2640_set_JPEG_size(OV2640_1600x1200);
    //Wait until buttom released
    while(myCAM.get_bit(ARDUCHIP_TRIG, SHUTTER_MASK));
    delay(1000);
    start_capture = 1;
    	
  }
  else
  {
    if(isShowFlag )
    {
  
      if(!myCAM.get_bit(ARDUCHIP_TRIG,VSYNC_MASK))				 			//New Frame is coming
      {
         myCAM.set_mode(MCU2LCD_MODE);    		//Switch to MCU
         myGLCD.resetXY();
         myCAM.set_mode(CAM2LCD_MODE);    		//Switch to CAM
         while(!myCAM.get_bit(ARDUCHIP_TRIG,VSYNC_MASK)); 	//Wait for VSYNC is gone
      }
    }
  }
  if(start_capture)
  {
    //Flush the FIFO 
    myCAM.flush_fifo();	
    //Clear the capture done flag 
    myCAM.clear_fifo_flag();		 
    //Start capture
    myCAM.start_capture();
    Serial.println("Start Capture");     
  }
  
  if(myCAM.get_bit(ARDUCHIP_TRIG ,CAP_DONE_MASK))
  {

    Serial.println("Capture Done!");
    String time = gettime();
    strcat(filename,time.c_str());
    filename[strlen(filename)-1]=0;
    //Serial.println(filename);
    strcat(filename,".jpg");
    strcat(path,"/mnt/sda1/DCIM/");
    strcat(path,filename);
    Serial.println(path);
    //Open the new file  
    File outFile = FileSystem.open(path,FILE_WRITE);
    if (! outFile) 
    { 
      Serial.println("open file failed");
      myCAM.clear_fifo_flag();
      return;
    }
    total_time = millis();
    i = 0;
    temp = myCAM.read_fifo();
    //Write first image data to buffer
    buf[i++] = temp;

    //Read JPEG data from FIFO
    while( (temp != 0xD9) | (temp_last != 0xFF) )
    {
      temp_last = temp;
      temp = myCAM.read_fifo();
      //Write image data to buffer if not full
      if(i < 256)
        buf[i++] = temp;
      else
      {
        //Write 256 bytes image data to file
        outFile.write(buf,256);
        i = 0;
        buf[i++] = temp;
      }
    }
    //Write the remain bytes in the buffer
    if(i > 0)
      outFile.write(buf,i);

    //Close the file 
    outFile.close(); 
    total_time = millis() - total_time;
    Serial.print("Total time used:");
    Serial.print(total_time, DEC);
    Serial.println(" millisecond");    
    //Clear the capture done flag 
    myCAM.clear_fifo_flag();
    //Clear the start capture flag
    start_capture = 0;
    
    myCAM.set_format(BMP);
    myCAM.InitCAM();
    isShowFlag = true;	
  }
}

String gettime()
{
    Process ptime;
    String time="";
    ptime.begin("date");
    ptime.addParameter("+%Y%m%d%H%M%S");
    ptime.run();
    while(ptime.available() > 0) 
    {
      char c = ptime.read();
      time += c;
    }
    return time;
}

