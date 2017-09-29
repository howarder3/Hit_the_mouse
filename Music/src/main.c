#include "stm32l476xx.h"
#include <stdlib.h>

#define led_A 4
#define led_B 5
#define led_C 6
#define led_D 7
#define led_E 8
#define led_F 9
#define led_G 10
#define led_H 11
#define led_I 12

//遊戲相關設定
#define perfect 200
#define good 2000
#define total_btn 97
#define max_score 1000

extern void max7219_init();
extern void max7219_send(unsigned char address, unsigned char data);
extern void Delay_1ms(); //1ms

int current_score = 0;
int add_score=0;
int cnt=0;
int answer_time;
int question_btn; //0-8
int question_light = 12; //4-12
int mouse= 70;
int all_mouse = 70;
int sleep_time;

void run(int led_1,int led_2,int led_3,int delay_time);


//question_light = question_btn + 4;

//Delay
void Delay(int time)
{
	for(int i=0;i<time;i++)
		Delay_1ms();
}

//GPIO初始化
void init_GPIO()
{
	RCC->AHB2ENR = RCC->AHB2ENR|0xF; //PA,PB,PC
	//Set PA 5~12 as output mode
	GPIOA->MODER = 0xA96955F7;

	GPIOA->OTYPER &= ~(0xFFFF); //otype PP
	//GPIOA->OTYPER |= 0x0600; //otype PP

	GPIOA->PUPDR &= ~(0xFFFFFFFF); //PUpd nopull
	//GPIOA->PUPDR = 0xEBFFF7; //PUpd nopull

	GPIOA->OSPEEDR &= ~(0xFFFFFFFF); //speed low
	//GPIOA->OSPEEDR = 0xEBFFF7; //speed low

	GPIOA->AFR[1] = (GPIOA->AFR[1] & ~(0x00000FF0)) | 0x00000770; // AF7 for pin

	//Set PB 0-8 as input mode
	GPIOB->MODER = 0xFF140000;

	//set PB 0-15 is Pull-up output
	GPIOB->PUPDR = GPIOB->PUPDR | 0x55555555;
	//Set PC 0~2 as output mode
	GPIOC->MODER = GPIOC->MODER & 0xFFFFFFD5;
}

void init_UART() {
/*USART_init*/
	RCC->APB2ENR |=   RCC_APB2ENR_USART1EN;

	USART1->CR1 &= ~(USART_CR1_M|USART_CR1_PS|USART_CR1_PCE|USART_CR1_TE|USART_CR1_RE|USART_CR1_OVER8);
	USART1->CR1 |= (USART_CR1_TE|USART_CR1_RE);

	USART1->CR2 &= ~USART_CR2_STOP; /*!< 0x00003000 */

	USART1->CR3 &= ~(USART_CR3_RTSE|USART_CR3_CTSE|USART_CR3_ONEBIT);

	USART1->BRR &= ~(0xFFFF);
	USART1->BRR = (4000000/9600L);

	USART1->CR2 &= ~(USART_CR2_LINEN | USART_CR2_CLKEN);
	USART1->CR3 &= ~(USART_CR3_SCEN | USART_CR3_HDSEL | USART_CR3_IREN);
	// Enable UART
	USART1->CR1 |= (USART_CR1_UE);

}

void Timer_init()
{
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
	TIM2->ARR = (uint32_t)8;//Reload value
	TIM2->PSC = (uint32_t)40000;//Prescalser 1911 Do
	TIM2->EGR = TIM_EGR_UG;//Reinitialize the counter. CNT takes the auto-reload value.
	TIM2->CR1 |= TIM_CR1_CEN;//start timer
}

void Timer_start(int sound_value)
{
	TIM2->PSC = (uint32_t)sound_value;//Prescalser 1911 Do
	int i;
	for(i=0 ; i < 20000; i++)
	{
		int timerValue = TIM2->CNT;//polling the counter value
		if(timerValue < 4)
		{
			GPIOA->BSRR = (1<<1);
		}
		else
		{
			GPIOA->BRR = (1<<1);
		}
	}
}

int button_press(int pin){
	int debounce_done = 0;
	if(!(GPIOB->IDR&1<<(pin-4))) {
		debounce_done=1;
	}
	return debounce_done;
}

//調整系統頻率
void SystemClock_Config(void){
    // set clock src back to MSI
    RCC->CFGR = (RCC->CFGR & 0xFFFFFFFC) | RCC_CFGR_SW_MSI;
    // enable HSI as input clock
    RCC->CR |= RCC_CR_HSION;
    while((RCC->CR & RCC_CR_HSIRDY) == 0);//check HSI16 ready
    // disable PLL PLLON to 0
    RCC->CR = RCC->CR & (~RCC_CR_PLLON_Msk);
    // wait until PLLRDY is cleared
    while((RCC->CR & RCC_CR_PLLRDY) != 0);

    RCC->PLLCFGR = 0x07000A12;//PLLM=2 PLLN=10 PLLR=8 HSI16
    RCC->CR |= RCC_CR_PLLON;
    while((RCC->CR & RCC_CR_PLLRDY) == 0);
	RCC->CFGR = 0x0F; //pre-scaler = 0 ,set clock source as PLL
	SysTick->CTRL	=0x00000007;
	SysTick->LOAD	=0x00002710; // 10M  0x000F4240 0.1s   0x00989680 1s
}

//顯示分數
void display(int data)
{
	if(data == 0)
	{
		max7219_send(1,0);
		for(int i=2 ; i<=8 ; i++) //從第一個數字開始印
			max7219_send(i,15);
	}
	else
	{
		for(int i=1 ; i<=8 ; i++) //從第一個數字開始印
		{
			if(data==0)
				max7219_send(i,15);
			else
				max7219_send(i,data%10);
			data = data/10;
		}
	}
}

//增加分數
int add_score_func(int current_score,int add_score)
{
	for(int i=1;i <= add_score ;i++)
	{
		display(current_score + i);
	}
	current_score += add_score;
	return current_score;
}

void light_all(){
	GPIOA->BSRR |= (1<<led_A);
	GPIOA->BSRR |= (1<<led_B);
	GPIOA->BSRR |= (1<<led_C);
	GPIOA->BSRR |= (1<<led_D);
	GPIOA->BSRR |= (1<<led_E);
	GPIOB->BSRR |= (1<<led_F);
	GPIOB->BSRR |= (1<<led_G);
	GPIOA->BSRR |= (1<<led_H);
	GPIOA->BSRR |= (1<<led_I);
}

void light(int led){
	if(led==9 || led==10)
		GPIOB->BSRR |= (1<<led);
	else
		GPIOA->BSRR |= (1<<led);
}

void dark_all(){
	GPIOA->BRR |= (1<<led_A);
	GPIOA->BRR |= (1<<led_B);
	GPIOA->BRR |= (1<<led_C);
	GPIOA->BRR |= (1<<led_D);
	GPIOA->BRR |= (1<<led_E);
	GPIOB->BRR |= (1<<led_F);
	GPIOB->BRR |= (1<<led_G);
	GPIOA->BRR |= (1<<led_H);
	GPIOA->BRR |= (1<<led_I);
}

void dark(int led){
	if(led==9 || led==10)
		GPIOB->BRR |= (1<<led);
	else
		GPIOA->BRR |= (1<<led);
}

void LED_all()
{
	light_all();
	Delay(300);
	dark_all();
	Delay(300);
}

//繞圈圈
void LED_circle()
{
	int delay_circle=200;
	light(4);
	Delay(delay_circle);
	dark(4);
	Delay(delay_circle);
	light(5);
	Delay(delay_circle);
	dark(5);
	Delay(delay_circle);
	light(6);
	Delay(delay_circle);
	dark(6);
	Delay(delay_circle);
	light(9);
	Delay(delay_circle);
	dark(9);
	Delay(delay_circle);
	light(12);
	Delay(delay_circle);
	dark(12);
	Delay(delay_circle);
	light(11);
	Delay(delay_circle);
	dark(11);
	Delay(delay_circle);
	light(10);
	Delay(delay_circle);
	dark(10);
	Delay(delay_circle);
	light(7);
	Delay(delay_circle);
	dark(7);
	Delay(delay_circle);
}

void SysTick_Handler(void){
	if(mouse == all_mouse)
		{
			for(int i=0;i<3;i++)
			{
				LED_circle();
			}
				//LED_all();
				while(1)
				{
					long int count=0;
					int breathe;
					int brightness;
					int play=0;
					for(brightness = 0; brightness<12 ; brightness++)
						for(breathe = 0;breathe<5 ; breathe++)
							for(count=0;count< 10;count++)//0.01
							{
								if(count<brightness && play ==0)
									light(led_E);
								else
									dark(led_E);

								if(button_press(led_E)&& play==0){
									play=1;
								}
								Delay(1);
							}
					for(brightness = 12; brightness>0 ; brightness--)
						for(breathe = 0;breathe<5 ; breathe++)//0.1
							for(count=0;count< 10;count++)//0.01
							{
								if(count<brightness && play ==0)
									light(led_E);
								else
									dark(led_E);

								if(button_press(led_E)&&play==0 ){
									play=1;
								}
								Delay(1);
							}
					if(play==1)
					{
						Timer_start(perfect);
						break;
					}
				}
			Delay(3000);
			question_btn = (rand() % 9);
			question_light = question_btn + 4;
			mouse--;
		}
		else if(mouse < 0)
		{

			for(int i=0;i<10;i++)
			{
				LED_all();
			}
				while(1)
				{
					long int count=0;
					int breathe;
					int brightness;
					int play=0;
					for(brightness = 0; brightness<12 ; brightness++)
						for(breathe = 0;breathe<5 ; breathe++)
							for(count=0;count< 10;count++)//0.01
							{
								if(count<brightness && play ==0)
									light(led_E);
								else
									dark(led_E);

								if(button_press(led_E)&& play==0){
									play=1;
								}
								Delay(1);
							}
					for(brightness = 12; brightness>0 ; brightness--)
						for(breathe = 0;breathe<5 ; breathe++)//0.1
							for(count=0;count< 10;count++)//0.01
							{
								if(count<brightness && play ==0)
									light(led_E);
								else
									dark(led_E);

								if(button_press(led_E)&&play==0 ){
									play=1;
								}
								Delay(1);
							}
					if(play==1)
					{
						Timer_start(perfect);
						display(0);
						break;
					}
				}
			Delay(3000);
			question_btn = (rand() % 9);
			question_light = question_btn + 4;
			current_score = 0;

			mouse = all_mouse - 1;
		}
		else
		{
			//display(0);
			cnt++;
			light(question_light);
			add_score = 0;
			if(current_score < 5)
			{
				answer_time = 1000;
				sleep_time = 2000;
			}
			else if(current_score < 15)
			{
				answer_time = 800;
				sleep_time = 1000;
			}
			else if(current_score < 25)
			{
				answer_time = 600;
				sleep_time = 800;
			}
			else if(current_score < 35)
			{
				answer_time = 500;
				sleep_time = 500;
			}
			else if(current_score < 45)
			{
				answer_time = 400;
				sleep_time = 300;
			}
			else
			{
				answer_time = 300;
				sleep_time = 300;
			}


			//display(cnt);
			if(!(GPIOB->IDR & 1<<question_btn)) //按了加分 	//結算此輪遊戲
			{
				if(cnt < answer_time)
				{
					dark(question_light);
					Timer_start(perfect);
					add_score = 1;
					cnt = 1000;
				}
			}

			if(cnt >= answer_time)
			{
				//如果超出判定時間的熄燈
				dark(question_light);
				//更新分數
				current_score = add_score_func(current_score,add_score);
				//少一隻老鼠
				mouse--;

				//下個按鍵初始化
				question_btn = (rand() % 9);
				question_light = question_btn + 4;
				cnt = 0;
				int random_time = ((rand() % 50))*10;
				Delay(sleep_time+random_time);
			}
		}
}

int main()
{
	init_GPIO();
	init_UART();
	max7219_init();
	Timer_init();
	Timer_start(good);
	SystemClock_Config();
	display(0);
	while(1)
	{}
	return 0;
}
