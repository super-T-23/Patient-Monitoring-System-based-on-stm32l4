
#include "wifi.h"
#include <stdlib.h>   // for rand(). Can be removed if valid sensor data is sent instead
#include "main.h"
#include "math.h"
#include "../../Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_accelero.h"
#include "../../Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_tsensor.h"
#include "../../Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_gyro.h"
#include "../../Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_magneto.h"

#include "../../Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_psensor.h"
#include "../../Drivers/BSP/B-L475E-IOT01/stm32l475e_iot01_hsensor.h"
#include "stdio.h"
#include "string.h"
#define TEMP_THRESHOLD 32
#define ACC_THRESHOLD 6
#define GYRO_THRESHOLD 5000
#define HUMID_THRESHOLD 50
#define PRESSURE_SENSOR 1020



static void UART1_Init(void);
static void MX_GPIO_Init(void);

float temp_data;
int flagn=0;
int mode=1;
int send=0;
int fatal=0;

UART_HandleTypeDef huart1;
#define MAX_LENGTH 400  // adjust it depending on the max size of the packet you expect to send or receive
#define WIFI_READ_TIMEOUT 10000
#define WIFI_WRITE_TIMEOUT 10000

const char* WiFi_SSID = "12345678";               // Replacce mySSID with WiFi SSID for your router / Hotspot
const char* WiFi_password = "12345678";   // Replace myPassword with WiFi password for your router / Hotspot
const WIFI_Ecn_t WiFi_security = WIFI_ECN_WPA2_PSK; // WiFi security your router / Hotspot. No need to change it unless you use something other than WPA2 PSK
const uint16_t SOURCE_PORT = 1234;  // source port, which can be almost any 16 bit number

uint8_t ipaddr[4] = {192, 168, 137, 1}; // IP address of your laptop wireless lan adapter, which is the one you successfully used to test Packet Sender above.
                                    // If using IoT platform, this will be overwritten by DNS lookup, so the values of x and y doesn't matter
                                            //(it should still be filled in with numbers 0-255 to avoid compilation errors)

    const uint16_t DEST_PORT = 2028;        // 'server' port number - this is the port Packet Sender listens to (as you set in Packer Sender)

SPI_HandleTypeDef hspi3;

int main(void)
{
  HAL_Init();
  MX_GPIO_Init();

  BSP_ACCELERO_Init();
  BSP_TSENSOR_Init();
  BSP_GYRO_Init();
  BSP_MAGNETO_Init();
  BSP_HSENSOR_Init();
  BSP_PSENSOR_Init();
  UART1_Init();
  uint8_t req[MAX_LENGTH];  // request packet
  uint8_t resp[MAX_LENGTH]; // response packet
  uint16_t Datalen;
  WIFI_Status_t WiFi_Stat; // WiFi status. Should remain WIFI_STATUS_OK if everything goes well


  float MAG_SENSOR[3]={0};
  int mag1=1;
  float gyrodata=0;
  float gyrodata1=0;
  float offset=0;
  int h=0;
  int led=0;
  int ft=0;
  int ff=0;
  int fp=0;
  int fh=0;
  int fm=0;
  int fg=0;
  //int mode=1;
  int enter=1;
  int flag=0;
  int gyrostart=1;
  uint32_t start=0;
  uint32_t startled=0;



  WiFi_Stat = WIFI_Init();                      // if it gets stuck here, you likely did not include EXTI1_IRQHandler() in stm32l4xx_it.c as mentioned above
  WiFi_Stat &= WIFI_Connect(WiFi_SSID, WiFi_password, WiFi_security); // joining a WiFi network takes several seconds. Don't be too quick to judge that your program has 'hung' :)
  if(WiFi_Stat!=WIFI_STATUS_OK) while(1);                   // halt computations if a WiFi connection could not be established.

  // WiFi_Stat = WIFI_Ping(ipaddr, 3, 200);                 // Optional ping 3 times in 200 ms intervals
  WiFi_Stat = WIFI_OpenClientConnection(1, WIFI_TCP_PROTOCOL, "conn", ipaddr, DEST_PORT, SOURCE_PORT); // Make a TCP connection.
                                                                  // "conn" is just a name and serves no functional purpose

  if(WiFi_Stat!=WIFI_STATUS_OK) while(1);                   // halt computations if a connection could not be established with the server
  while (1)
   {
    uint32_t t=HAL_GetTick();
    uint32_t tled=t;
    char mt[32]={0};
    char mf[32]={0};
    char mp[32]={0};
    char mh[32]={0};
    char mm[32]={0};
    char mg[32]={0};

    float accel_data[3]={0};
    float mag_data[3];

    float humid_data;
    float pre_data;
    float gyro_data[3]={0};
    int16_t accel_data_i16[3] = {0};
    int16_t mag_data_i16[3]={0};
    BSP_ACCELERO_AccGetXYZ(accel_data_i16);  // read accelerometer
    accel_data[0] = (float)accel_data_i16[0] / 100.0f;
    accel_data[1] = (float)accel_data_i16[1] / 100.0f;
    accel_data[2] = (float)accel_data_i16[2] / 100.0f;


    temp_data = BSP_TSENSOR_ReadTemp();   // read temperature sensor


    humid_data=BSP_HSENSOR_ReadHumidity();


    pre_data=BSP_PSENSOR_ReadPressure();

    if (mag1==1)
    {
     BSP_MAGNETO_GetXYZ(mag_data_i16);
     mag_data[0]=(float)mag_data_i16[0]/1000.0f;
     mag_data[1]=(float)mag_data_i16[1]/1000.0f;
     mag_data[2]=(float)mag_data_i16[2]/1000.0f;
     MAG_SENSOR[0]= mag_data[0];
     MAG_SENSOR[1]= mag_data[1];
     MAG_SENSOR[2]= mag_data[2];
     mag1=0;
    }

    if(TEMP_THRESHOLD<=temp_data) ft=1;
    if(ACC_THRESHOLD>=accel_data[2]) ff=1;
    if(humid_data<HUMID_THRESHOLD) fh=1;
    if(pre_data>PRESSURE_SENSOR) fp=1;
    if(gyrodata>GYRO_THRESHOLD) fg=1;
    if(mag_data[0]- MAG_SENSOR[0]>0.1||mag_data[1]- MAG_SENSOR[1]>0.1||mag_data[2]- MAG_SENSOR[2]>0.1) fm=1;
    if(mag_data[0]- MAG_SENSOR[0]<-0.1||mag_data[1]- MAG_SENSOR[1]<-0.1||mag_data[2]- MAG_SENSOR[2]<-0.1) fm=1;

    if(led==0)  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, 0);
    else if(led==3)  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, 1);
    else if(led==1)  {if (tled-startled>100) {HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);startled=tled;}}
    else if(led==2)  if (tled-startled>50) {HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_14);startled=tled;}
    if (led!=0) {HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_11);HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_11);}

    if (t-start>10000)
    	  {
    		  flag=1;
    		  start=t;
    	  }
    else flag=0;

    if(send)
    {
        WiFi_Stat = WIFI_SendData(1, "Patient Calling for Help\r\n", (uint16_t)strlen((char*)"Patient Calling for Help\r\n"), &Datalen, WIFI_WRITE_TIMEOUT);
        send=0;
    }
    if(fatal)
    {
        WiFi_Stat = WIFI_SendData(1, "Patient Sitting Up\r\n", (uint16_t)strlen((char*)"Patient Sitting Up\r\n"), &Datalen, WIFI_WRITE_TIMEOUT);
        fatal=0;
    }

    if (mode==1)//Normal Mode
    {
  	  //if (enter==1){HAL_UART_Transmit(&huart1, (uint8_t*)"Entering Normal Mode\r\n", strlen("Entering Normal rMode\r\n"),0xFFFF);enter=0;led=0;flagn=0;}
  	  if (enter==1){HAL_UART_Transmit(&huart1, (uint8_t*)"Entering Normal Mode\r\n", strlen("Entering Normal Mode\r\n"),0xFFFF);led=0;flagn=0;enter=0;}

  	  if(ft==1&&flag==1) {HAL_UART_Transmit(&huart1, (uint8_t*)"Fever Detected\r\n", strlen("Fever Detected\r\n"),0xFFFF);led= led==2? 2:1;}
  	  if(ff==1&&flag==1) {HAL_UART_Transmit(&huart1, (uint8_t*)"Fall Detected\r\n", strlen("Fall Detected\r\n"),0xFFFF);led= led==2? 2:1;}
  	  if((fp==1||fh==1)&&flag==1)
  	  {
  		  HAL_UART_Transmit(&huart1, (uint8_t*)"Check patient's breath!\r\n", strlen("Check patient's breath!\r\n"),0xFFFF);
  		  ft=0;ff=0;fp=0;fh=0;
  		  mode=2;
  		  enter=1;
  	  }

    }
  //###################################################################################################################
    if (mode==2)//Intensive Care Mode
      {

    	  if (enter==1){HAL_UART_Transmit(&huart1, (uint8_t*)"Entering Intensive Care Mode\r\n", strlen("Entering Intensive Care Mode\r\n"),0xFFFF);led=3;enter=0;mag1=1;}

    	  BSP_GYRO_GetXYZ(gyro_data);

    	  if (h<3)
    	  {
    		  gyrodata1+=sqrt(gyro_data[0]*gyro_data[0]+gyro_data[1]*gyro_data[1]+gyro_data[2]*gyro_data[2]);
    		  h++;
    	  }
    	  else
    	  {
    		  h=0;
    		  if (gyrostart==1) {offset=gyrodata1/3;gyrostart=0;}
    		  gyrodata=gyrodata1/3-offset;
    		  gyrodata1=0;

    	  }


    	  BSP_MAGNETO_GetXYZ(mag_data_i16);
    	  mag_data[0]=(float)mag_data_i16[0]/1000.0f;
    	  mag_data[1]=(float)mag_data_i16[1]/1000.0f;
    	  mag_data[2]=(float)mag_data_i16[2]/1000.0f;

  	  if(ft==1&&flag==1)
  	  {
  		  sprintf(mt, "Fever Detected  %.2f(degreeC)\r\n", temp_data);
  		  HAL_UART_Transmit(&huart1, (uint8_t*)mt, strlen(mt),0xFFFF);
  		  led=3;


  	  }
  	  if(ff==1&&flag==1)
  	  {
  		  sprintf(mf, "Fall Detected  %.2f(g) %.2f(g) %.2f(g)\r\n", accel_data[0],accel_data[1],accel_data[2]);
  		  HAL_UART_Transmit(&huart1, (uint8_t*)mf, strlen(mf),0xFFFF);
  		  //led=1;

  	  }
  	  if(fp==1&&flag==1)
  	  {
  		  sprintf(mp, "Check patient's breath! %.2f(hPa)\r\n",pre_data);
  		  HAL_UART_Transmit(&huart1, (uint8_t*)mp, strlen(mp),0xFFFF);
  		  //led=1;

  	  }
  	  if(fh==1&&flag==1)
  	  {
  		  sprintf(mh, "Check patient's breath! %.2f%(rH)\r\n",humid_data);
  		  HAL_UART_Transmit(&huart1, (uint8_t*)mh, strlen(mh),0xFFFF);
  		  //led=1;

  	  }
  	  if(fg==1&&flag==1)
  	  {
  		  sprintf(mg, "Patient in pain! %.1f(pad/s)\r\n",gyrodata);
  		  HAL_UART_Transmit(&huart1, (uint8_t*)mg, strlen(mg),0xFFFF);
  		  led=2;

  	  }
  	  if(fm==1&&flag==1)
  	  {
  		  sprintf(mm, "Check patient's abnormal orientation! %.2f(Gauss) %.2f(Gauss) %.2f(Gauss)\r\n",mag_data[0],mag_data[1],mag_data[2]);
  		  HAL_UART_Transmit(&huart1, (uint8_t*)mm, strlen(mm),0xFFFF);
  		  led=2;

  	  }
  	  if(flagn==1)
  	  {
  		  ft=0;ff=0;fp=0;fh=0;fg=0;fm=0;
  		  mode=1;
  		  enter=1;
  	  }
      }

   }





}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	static int s=0;
	int e=0;
  switch(GPIO_Pin){
  case GPIO_PIN_1:
      SPI_WIFI_ISR();
      break;
  case GPIO_PIN_13:
	  if (mode==2)
	  {
	  	e=HAL_GetTick();
	  	if (e-s<500) {flagn=1;}
	  	else s=e;
	  }
	  else if (mode==1)
	  {
		  send=1;
	  }
	  break;
  case GPIO_PIN_11:
	  //HAL_UART_Transmit(&huart1, (uint8_t*)"cao\r\n", strlen("cao\r\n"),0xFFFF);
	  fatal=1;


  } // use more cases in the case statement for other EXTI interrupts
}

void SPI3_IRQHandler(void)
{
    HAL_SPI_IRQHandler(&hspi3);
}







 static void UART1_Init(void)
 {
     /* Pin configuration for UART. BSP_COM_Init() can do this automatically */
     __HAL_RCC_GPIOB_CLK_ENABLE();
     GPIO_InitTypeDef GPIO_InitStruct = {0};
     GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
     GPIO_InitStruct.Pin = GPIO_PIN_7|GPIO_PIN_6;
     GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
     GPIO_InitStruct.Pull = GPIO_NOPULL;
     GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
     HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

     /* Configuring UART1 */
     huart1.Instance = USART1;
     huart1.Init.BaudRate = 115200;
     huart1.Init.WordLength = UART_WORDLENGTH_8B;
     huart1.Init.StopBits = UART_STOPBITS_1;
     huart1.Init.Parity = UART_PARITY_NONE;
     huart1.Init.Mode = UART_MODE_TX_RX;
     huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
     huart1.Init.OverSampling = UART_OVERSAMPLING_16;
     huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
     huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
     if (HAL_UART_Init(&huart1) != HAL_OK)
     {
       while(1);
     }
 }

 static void MX_GPIO_Init(void)
 {
   GPIO_InitTypeDef GPIO_InitStruct = {0};

   /* GPIO Ports Clock Enable */
   __HAL_RCC_GPIOB_CLK_ENABLE();

   __HAL_RCC_GPIOC_CLK_ENABLE();

   __HAL_RCC_GPIOD_CLK_ENABLE();// Enable AHB2 Bus for GPIOC

   /*Configure GPIO pin Output Level */
   HAL_GPIO_WritePin(GPIOB, LED2_Pin, GPIO_PIN_RESET);

   /*Configure GPIO pin LED2_Pin */
   GPIO_InitStruct.Pin = LED2_Pin;
   GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	// Configuration of BUTTON_EXTI13_Pin (GPIO-C Pin-13) as AF,
	GPIO_InitStruct.Pin = BUTTON_EXTI13_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	//
	GPIO_InitStruct.Pin = GPIO_PIN_11;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
	// Enable NVIC EXTI line 13
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

 }


