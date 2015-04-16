/*****************************************************
 *
 *	Control program for the PitSchuLight TV-Backlight
 *	(c) Peter Schulten, M�lheim, Germany
 *	peter_(at)_pitschu.de
 *
 *	Die unver�nderte Wiedergabe und Verteilung dieses gesamten Sourcecodes
 *	in beliebiger Form ist gestattet, sofern obiger Hinweis erhalten bleibt.
 *
 * 	Ich stelle diesen Sourcecode kostenlos zur Verf�gung und biete daher weder
 *	Support an noch garantiere ich f�r seine Funktionsf�higkeit. Au�erdem
 *	�bernehme ich keine Haftung f�r die Folgen seiner Nutzung.

 *	Der Sourcecode darf nur zu privaten Zwecken verwendet und modifiziert werden.
 *	Dar�ber hinaus gehende Verwendung bedarf meiner Zustimmung.
 *
 *	History
 *	09.06.2013	pitschu		Start of work
 */

#include "stm32f10x.h"
#include "IRdecoder.h"

volatile static uint8_t  nec_data;       // IR data byte
volatile static uint8_t  nec_addr;       // ID address code
volatile static uint8_t  nec_new ;       // 1 when new data arrived or when repeat code was sent

long			  repTime;			// time, when repeat may start (2 seks after code was sent)

volatile irCode_t	irCode;
volatile int system_time ;


void IRdecoderInit(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  NVIC_InitTypeDef NVIC_InitStructure;
  TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
  TIM_ICInitTypeDef  TIM_ICInitStructure;
  
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC,ENABLE);
  /* TIM1 clock enable */
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE);
  
  /* Setting up TIM 8 Chan 1 pin (C6) */
  GPIO_InitStructure.GPIO_Pin =	GPIO_Pin_6;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOC, &GPIO_InitStructure);
  
  NVIC_InitStructure.NVIC_IRQChannel = TIM8_UP_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 5;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
  
  /* TIM8 configuration: PWM Input mode ------------------------
     The external signal is connected to TIM8 CH1 pin (PC.06),
     The falling edge is used as active edge,
     The TIM8 CCR1 is used to compute the period value (set on falling edge)
     The TIM8 CCR2 is used to compute the duty cycle value (set on rising edge)
     ------------------------------------------------------------ */
  TIM_TimeBaseStructure.TIM_RepetitionCounter = 	0;
  TIM_TimeBaseStructure.TIM_Period = (uint32_t)(250000 * 0.15);
  TIM_TimeBaseStructure.TIM_Prescaler = (uint16_t) ((SystemCoreClock / 1) / 250000) - 1;	// -> 250kHz (4us)
  TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_TimeBaseInit (TIM8, &TIM_TimeBaseStructure);
  
  TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Falling;
  TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI; // connect IC1 to TI1 and TI2
  TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
  TIM_ICInitStructure.TIM_ICFilter = 0x0;
  TIM_PWMIConfig(TIM8, &TIM_ICInitStructure);
  
  /* Select the Input Trigger for slave mode (timer update when TI1FP1) */
  TIM_SelectInputTrigger(TIM8, TIM_TS_TI1FP1);
  TIM_SelectSlaveMode(TIM8, TIM_SlaveMode_Reset);  // TIM8 is reset when TI1FP1 rises
  TIM_SelectMasterSlaveMode(TIM8, TIM_MasterSlaveMode_Enable);
  
  /* TIM enable counter */
  TIM_Cmd(TIM8, ENABLE);
  
  TIM_ClearFlag(TIM8, TIM_FLAG_Update);
  TIM_ITConfig(TIM8, TIM_IT_Update, ENABLE);
  
  irCode.code = NO_CODE;
  irCode.isNew = 0;
}

void TIM8_UP_IRQHandler(void)
{
  static int8_t	bit_cnt;                    // count bits
  static unsigned long tmpData;
  int8_t tmp_cnt = bit_cnt;
  unsigned int bit_time;                  	// bit time from CCR1

  if ( TIM_GetITStatus(TIM8, TIM_IT_Update) != RESET ) {
    TIM_ClearITPendingBit(TIM8, TIM_IT_Update);
    
    if (TIM_GetFlagStatus (TIM8, TIM_FLAG_CC1) == SET) {		// is not a timer overflow
      bit_time = TIM_GetCapture1(TIM8);
      TIM_ClearFlag(TIM8, TIM_FLAG_CC1);
      if (bit_time > NECP_MAX)
	bit_time = 0;
    } else {		// timer overflow (> 200ms since last edge)
      if (irCode.code != NO_CODE && irCode.ticksAutorpt > 0) {	// button released
	irCode.ticksAutorpt = 0;
	irCode.isNew = IR_RELEASED;
      }	else {
	irCode.code = NO_CODE;
	irCode.ticksAutorpt = 0;
	irCode.repcntPressed = 0;
	irCode.isNew = IR_NOTHING;
      }
      bit_time = 0;
      nec_data = 0;
    }
    
    if ((bit_time < NECP_MIN   ) ||   // too short, error
	(bit_time > NECP_REPEAT)) {     // start pulse
      tmp_cnt = 0;
    } else {
      if (tmp_cnt >= 0)	{
	if (bit_time > NECP_ONE) {		// repeat pulse
	  irCode.repcntPressed++;
	  irCode.ticksAutorpt++;
	  
	  if (irCode.ticksAutorpt > AUTO_RPT_INITIAL) {
	    irCode.isNew = IR_AUTORPT;
	    irCode.ticksAutorpt = AUTO_RPT;
	  } else if (irCode.isNew == IR_CHECKED) {
	    irCode.isNew = IR_NOTHING;
	  } ;
	} else {
	  // bit received
	  tmp_cnt++;
	  
	  tmpData >>= 1;
	  
	  if (bit_time > NECP_ZERO)
	    tmpData |= 0x80000000L;
	  
	  if (tmp_cnt == 32) {
	    repTime = system_time + (100 * 1.5); // autorep starts after 1.5 seconds wait
	    
	    nec_addr = (tmpData >> 00) & 0xff;
	    nec_data = (tmpData >> 16) & 0xff;
	    
	    // check for valid data (must match with inverted data byte sent)
	    if (((nec_addr ^ (uint8_t)((tmpData>> 8) & 0xff)) != 0xff) ||
		((nec_data ^ (uint8_t)((tmpData>>24) & 0xff)) != 0xff)) {
	      nec_new = 0;
	      nec_data = 0;
	      nec_addr = 0xff;
	      tmpData = 0;
	    } else {
	      irCode.code = (nec_addr<<8) | nec_data;		// another keywas pressed
	      irCode.ticksAutorpt = 1;
	      irCode.repcntPressed = 0;
	      irCode.isNew = IR_PRESSED;
	    }
	    nec_new = 1;
	    tmp_cnt = -1;
	  }
	}
      }
    }
    bit_cnt  = tmp_cnt;
  }
}
