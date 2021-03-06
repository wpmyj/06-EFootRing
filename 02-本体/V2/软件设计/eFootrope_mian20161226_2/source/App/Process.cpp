/**
  ******************************************************************************
	*文件：Process.cpp
	*作者：孟浩
	*版本：2.0
	*日期：2015-03
	*概要：
	*备注：
	*
  ******************************************************************************  
	*/ 

/* 头文件包含 --------------------------------------------------------------------*/
#include "Process.h"
#ifdef __cplusplus
 extern "C" {
#endif
	 
#include <stdlib.h>
#ifdef __cplusplus
 }
#endif

#define TIM2_PERIOD_MS	10
#define TIM3_PERIOD_MS	30
#define TIM4_PERIOD_MS	100
/* 类的实现---------------------------------------------------------------------*/

 Process::Process()
{
	enable_SWD_disable_Jtag();								//	关闭JTAG打开SWD	
	
	initIOs();												//	初始化IO
	initADs();												//	初始化AD通道
	initUsarts();											//	初始化串口
	initTimers();											//	初始化定时器
	
	SysTick_DelayMs(1000);	
	testLed->setOn();
//	shockCtr->setOn();
	//beepCtr->setOn();
	initAllData();											//  初始化所有数据
	SysTick_DelayMs(100);
//	shockCtr->setOff();
	testLed->setOff();
	beepCtr->setOff();
	redLed->setOn();
}

void Process::openPeriph(void)
{
	SysTick_DelayMs(10);									//  延时 10ms
	zigbeePort->open();    										//  开启通信端口1
	
	t2->start();											//  开启定时器2
	t3->start();											//  开启定时器3
	t4->start();											//  开启定时器4

}

/**
  * @brief  TIM2定时器中断函数，处理
  * @param  None
  * @retval None
	* @定时时间：10ms
  */
void Process::runOnTime2(void)
{
	getPowerValue();

}

/**
  * @brief  TIM3定时器中断函数,更新电阻值与电源电压
  * @param  None
  * @retval None
	* @定时时间：30ms
  */
#define	OVER_TIME	1000
void Process::runOnTime3(void)
{
	static uint16_t	overTimerCount=0;
	if(zigbeeControl->reciveData())							//  接收串口数据
	{
		overTimerCount=0;									//	超时计数复位
		watchDogTimer=3;                  //收到数据，就喂狗

		receiveMainData();										//	接收数据
		disposal();      //处理数据

	}
	else
	{
		overTimerCount++;									//	超时计数
		if(overTimerCount>=OVER_TIME)
		{
			overTimerCount=0;
			resetTerminal();    //没有收到数据，不应该复位的

		}
	}

}

/**
  * @brief  TIM4定时器中断函数,定时休眠
  * @param  None
  * @retval None
	* @定时时间：100ms
  */
void Process::runOnTime4(void)
{
#ifdef USE_SLEEP
	watchDogTimer++;
	if(watchDogTimer>50)
	{
		watchDogTimer=0;
//add you code 休眠之前执行一些操作
		SysTick_DelayMs(500);
		PWR_EnterSTANDBYMode();
	//	PWR_EnterSTOPMode(PWR_Regulator_LowPower,PWR_STOPEntry_WFI);   //进入睡眠
		
	}
	else if(watchDogTimer<2)
	{
		//唤醒后的初始化
	}
	
#endif
}


/**
  * @brief  处理主控数据
  * @param  None
  * @retval None
  */
void Process::receiveMainData(void)
{
	uint8_t* dataFromMain;								//	保存接收自主控的数据
	uint16_t roomID1,roomID2,roomID3;
	uint16_t	terminalID1,terminalID2;
	
	dataFromMain = zigbeeControl->getReciveData();			//获取串口收到的数据
	
	controlMode		=	dataFromMain[3]&0x03;				//	获取控制模式
	controlStatus	=	(dataFromMain[3]>>2)&0x03;			//	获取控制状态
	
	roomID1		=	dataFromMain[4];					//	获取房间号最高位
	roomID2		=	dataFromMain[5];					//	获取房间号中间位
	roomID3		=	dataFromMain[6];					//	获取房间号最低位
	terminalID1	=	dataFromMain[7];					//	获取本体号最高位
	terminalID2	=	dataFromMain[8];					//	获取本体号最低位
	
	RoomID 		=	roomID1*100 + roomID2*10 + roomID3;	//	计算接收到的房间号
	TerminalID	=	terminalID1*10 + terminalID2;		//	计算接收到的本体号
	

	

	
}



/**
  * @brief  处理控制数据，并驱动执行器
  * @param  None
  * @retval None
  */
#define	BEEP_10SEC	3000	//	蜂鸣器10s
#define	SHOCK_3s	3000	//	电击器3s
void Process::disposal()
{
	static	uint8_t lastMode=MODE_APPROACH;

	//static	uint8_t	preKeyState=KEY_DEFAULT;
	static uint16_t operateAlarmCount=0;
	static uint16_t operateShockCount=0;
	
	
	if(lastMode!=controlMode)	//切换了控制模式
	{
		lastMode=controlMode;		
		resetTerminal();
		
		operateAlarmCount=0; //将计数清零
		operateShockCount=0;
		
		
	}
	//没有切换控制模式
	else				
	{
		//<接近模式>
		if(MODE_APPROACH==controlMode)	//	接近模式
		{
			if(controlStatus==STATE_ALARM)  //警告
			{
				operateAlarmCount++;
			 if(operateAlarmCount<=BEEP_10SEC/TIM3_PERIOD_MS)
			 {
					beepCtr->setOn();
				 greenLed->setOn();
			 }
			 else
			 {
					beepCtr->setOff();
				 greenLed->setOff();
			 }
					
			}
			else if(controlStatus==STATE_SHOCK)//电击
			{
				
				operateShockCount++;
				if(operateShockCount<=SHOCK_3s/TIM3_PERIOD_MS)
				{
					shockCtr->setOn();
					redLed->setOn();
				}
				else
				{
					shockCtr->setOff();
					redLed->setOff();
				}	
			}
			else  //默认模式，即不动作
			{
				operateAlarmCount=0; //将计数清零
				operateShockCount=0;	
				//关闭执行器
				beepCtr->setOff();
				shockCtr->setOff();	
				redLed->setOff();		
				greenLed->setOff();
				
			}
		//</接近模式>
		}
		//<定点模式>
		
			
			//add you code
		//</定点模式>
	

	}
	
}

void Process::resetTerminal(void)
{
	beepCtr->setOff();
	shockCtr->setOff();
}



/**
  * @brief  计算电源电压，放大10倍
  * @param  None
  * @retval None
  */
void Process::getPowerValue(void)
{
	uint32_t tempValue=0;

	adValue[NUM_POWER] = AD_Filter[NUM_POWER];
	tempValue  = adValue[NUM_POWER]*33*3>>12;
//	powerValue.u16_data = (uint16_t)tempValue;
	powerVoltage=(uint8_t)tempValue;
}




/**
  * @brief  初始化按键
  * @param  None
  * @retval None
  */
void Process::initIOs(void)
{
	testLed			=	new IoOut(PORT_LED_TEST,PIN_LED_TEST);
	greenLed		=	new IoOut(PORT_LED_GREEN,PIN_LED_GREEN);
	redLed			=	new	IoOut(PORT_LED_RED,PIN_LED_RED);
	
	beepCtr			=	new	IoOut(PORT_BEEP,PIN_BEEP,true);
	shockCtr		=	new IoOut(PORT_SHOCK,PIN_SHOCK,true);
	

}

/**
  * @brief  初始化AD
  * @param  None
  * @retval None
  */
void Process::initADs(void)
{
	//ADC1_DMA_Init();
}

/**
  * @brief  初始化定时器
  * @param  None
  * @retval None
  */
void Process::initTimers(void)
{
	t2=new Timer (TIM2);
	t2->setPriority(STM32_NVIC_TIM2_PrePriority,STM32_NVIC_TIM2_SubPriority);
	t2->setTimeOut(TIM2_PERIOD_MS);

	t3=new Timer (TIM3);
	t3->setPriority(STM32_NVIC_TIM2_PrePriority,STM32_NVIC_TIM3_SubPriority);
	t3->setTimeOut(TIM3_PERIOD_MS);

	t4=new Timer (TIM4);
	t4->setPriority(STM32_NVIC_TIM2_PrePriority,STM32_NVIC_TIM4_SubPriority);
	t4->setTimeOut(TIM4_PERIOD_MS);
}

/**
  * @brief  初始化usart
  * @param  None
  * @retval None
  */
void Process::initUsarts(void)
{
	zigbeePort = new SerialPort(1,38400);
	zigbeeControl = new TerminalControl(zigbeePort);


}

/**
  * @brief  初始化全部数据
  * @param  None
  * @retval None
  */
void Process::initAllData(void)
{
	powerVoltage	=	0;
	controlMode 		=	MODE_APPROACH;					//	默认为接近模式
	controlStatus 	=	STATE_DEFAULT;					//	默认为无按键按下
	RoomID		=	ID_ROOM_INVALID;				//	默认ID为无效
	TerminalID	=	ID_TERMINAL_INVALID;			//	默认ID为无效
	
	watchDogTimer=0;
	

}

/**
  * @brief  重置化全部数据
  * @param  None
  * @retval None
  */
void Process::resetControlData(void)
{
	
}

/**
  * @brief  使能SWD，关闭JTAG
  * @param  None
  * @retval None
  */
void Process::enable_SWD_disable_Jtag(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);		//	使能AFIO时钟
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable,ENABLE);		//	关闭JTAG，引脚作为GPIO用
}
/*--------------------------------- End of Process.cpp -----------------------------*/






