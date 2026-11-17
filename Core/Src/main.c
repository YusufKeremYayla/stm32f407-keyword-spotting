/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Sesli komut (go / stop) - STM32F407 Discovery
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "mp45dt02.h"
#include "circular_buffer.h"
#include "ai_controller.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
	DURUM_BOS,		// bekliyor: butona basılmadı
	DURUM_DINLE,	// butona basıldı: son 300 ms'yi tutuyor, konuşmanın başlamasını bekliyor
	DURUM_KAYIT		// konuşma başladı: 1 saniye dolana kadar kaydediyor
} Durum_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BEKLE_MS		300		// butona basma/bırakma tıkları bu sürede biter: kaydetme
#define ON_KAYIT		4800	// kelimeden önce saklanan ses: 300 ms × 16 ses/ms
#define SES_ESIGI		2000	// 1 ms'nin tepesi bundan büyükse ses var (sessizlik ~800'ün altında ölçüldü)
#define SES_SURESI		10		// üst üste 10 ms eşik aşılırsa konuşma başladı say (tek tık sayılmasın)
#define ZAMAN_ASIMI_MS	3000	// 3 saniye içinde konuşulmazsa vazgeç
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CRC_HandleTypeDef hcrc;

I2S_HandleTypeDef hi2s2;
DMA_HandleTypeDef hdma_spi2_rx;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
MP45DT02_t			mikrofon;
Circular_Buffer_t	kuyruk;

volatile Durum_t	durum       = DURUM_BOS;
volatile uint32_t	son_basis   = 0;		// butona basılan an (ms)
volatile uint16_t	ses_sayaci  = 0;		// üst üste kaç ms ses eşiği aşıldı
volatile bool		ses_gelmedi = false;	// 3 saniye içinde konuşulmadı
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2S2_Init(void);
static void MX_CRC_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// printf → USART2 (PA2 = TX, 115200 8N1). PA9 (USART1) Discovery'de C49 kondansatörü yüzünden çalışmıyor.
int _write(int file, char *ptr, int len)
{
	(void)file;
	HAL_UART_Transmit(&huart2, (uint8_t *)ptr, (uint16_t)len, HAL_MAX_DELAY);
	return len;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2S2_Init();
  MX_CRC_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
	Circular_Buffer_Init(&kuyruk);

	if (!MP45DT02_Initialization(&mikrofon, &hi2s2))
	{
		printf("Mikrofon baslatilamadi! saat = %lu Hz, birler = %% %u\r\n",
				(unsigned long)mikrofon.clock_hz, mikrofon.ones_percent);
	}
	else
	{
		printf("Mikrofon hazir. saat = %lu Hz, birler = %% %u\r\n",
				(unsigned long)mikrofon.clock_hz, mikrofon.ones_percent);
	}

	printf("Butona bas, turuncu yaninca konus.\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  if (Circular_Buffer_Count(&kuyruk) >= AI_PENCERE)
	  	{
	  		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_RESET);		// turuncu söndü
	  		AI_Process_Buffer(&kuyruk);
	  	}

	  	if (ses_gelmedi)
	  	{
	  		ses_gelmedi = false;
	  		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_RESET);		// turuncu söndü
	  		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_SET);		// mavi
	  		printf("3 saniye icinde ses gelmedi.\r\n");
	  	}
	    }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CRC Initialization Function
  * @param None
  * @retval None
  */
static void MX_CRC_Init(void)
{

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  hcrc.Instance = CRC;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CRC_Init 2 */

  /* USER CODE END CRC_Init 2 */

}

/**
  * @brief I2S2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2S2_Init(void)
{

  /* USER CODE BEGIN I2S2_Init 0 */

  /* USER CODE END I2S2_Init 0 */

  /* USER CODE BEGIN I2S2_Init 1 */

  /* USER CODE END I2S2_Init 1 */
  hi2s2.Instance = SPI2;
  hi2s2.Init.Mode = I2S_MODE_MASTER_RX;
  hi2s2.Init.Standard = I2S_STANDARD_LSB;
  hi2s2.Init.DataFormat = I2S_DATAFORMAT_16B;
  hi2s2.Init.MCLKOutput = I2S_MCLKOUTPUT_DISABLE;
  hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_48K;
  hi2s2.Init.CPOL = I2S_CPOL_HIGH;
  hi2s2.Init.ClockSource = I2S_CLOCK_PLL;
  hi2s2.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;
  if (HAL_I2S_Init(&hi2s2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2S2_Init 2 */

  /* USER CODE END I2S2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PD12 PD13 PD14 PD15 */
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/*
 * Mikrofon: her 1 ms'de 16 ses gelir.
 *   DURUM_DINLE : sesleri kuyruğa yaz ama sadece son 300 ms'yi tut. Konuşma başlayınca KAYIT'a geç.
 *   DURUM_KAYIT : kuyruk 16000 olana kadar yaz. Böylece kelimenin önünde ~300 ms, arkasında ~700 ms olur:
 *                 kelime 1 saniyenin ortasına düşer (Edge Impulse'taki kayıtlar gibi).
 */
static void Mikrofon_Kuyruga_Yaz(uint8_t half)
{
	MP45DT02_Read_PCM(&mikrofon, half);

	if (durum == DURUM_BOS)
		return;

	uint32_t gecen = HAL_GetTick() - son_basis;

	if (gecen < BEKLE_MS)
		return;		// buton tıkları: kaydetme

	for (int i = 0; i < MP45DT02_PCM_SIZE; i++)
		Circular_Buffer_Enqueue(&kuyruk, mikrofon.pcm[i]);

	if (durum == DURUM_DINLE)
	{
		// Sadece son 300 ms kalsın: fazlasını en eskiden at
		Sensor_Data_t at;
		while (Circular_Buffer_Count(&kuyruk) > ON_KAYIT)
			Circular_Buffer_Dequeue(&kuyruk, &at);

		// Konuşma başladı mı? (1 ms'nin tepesi eşiği üst üste SES_SURESI kere aştı mı)
		if (mikrofon.level > SES_ESIGI)
			ses_sayaci++;
		else
			ses_sayaci = 0;

		if (ses_sayaci >= SES_SURESI)
		{
			durum = DURUM_KAYIT;		// kelime başladı: önündeki 300 ms kuyrukta, 700 ms daha kaydet
		}
		else if (gecen > ZAMAN_ASIMI_MS)
		{
			Circular_Buffer_Init(&kuyruk);
			durum       = DURUM_BOS;
			ses_gelmedi = true;			// ana döngü mavi yakıp yazacak
		}
		return;
	}

	// DURUM_KAYIT
	if (Circular_Buffer_Count(&kuyruk) >= AI_PENCERE)
		durum = DURUM_BOS;		// 16000 ses = 1 saniye doldu, ana döngü modeli çalıştıracak
}

// DMA ilk yarıyı doldurdu
void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
	if (hi2s->Instance == SPI2)
		Mikrofon_Kuyruga_Yaz(0);
}

// DMA ikinci yarıyı doldurdu
void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s)
{
	if (hi2s->Instance == SPI2)
		Mikrofon_Kuyruga_Yaz(1);
}

// Buton (PA0): dinlemeye başla (bas ve bırak yeter)
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == GPIO_PIN_0)
	{
		// Dinleme/kayıt sürüyorsa, model çalışıyorsa ya da 300 ms içinde titrediyse sayma
		if ((durum == DURUM_BOS) && Circular_Buffer_Is_Empty(&kuyruk) && (HAL_GetTick() - son_basis) > 300)
		{
			son_basis  = HAL_GetTick();
			ses_sayaci = 0;

			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12 | GPIO_PIN_14 | GPIO_PIN_15, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_SET);		// turuncu: konuş

			durum = DURUM_DINLE;
		}
	}
}


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  (void)file;
  (void)line;
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
