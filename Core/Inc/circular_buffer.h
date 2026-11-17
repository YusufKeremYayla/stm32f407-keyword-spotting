/*
 * circularbuffer.h
 *
 *  Created on: Feb 24, 2026
 *      Author: yusuf
 */

#ifndef INC_CIRCULAR_BUFFER_H_
#define INC_CIRCULAR_BUFFER_H_

#include "stdbool.h"
#include "stdint.h"
#include "string.h"

#ifdef __cplusplus
extern "C" {
#endif

// 1. SES REVİZYONU: 512 → 16001. 1 saniye = 16000 ses. Is_Fully bir yeri boş bıraktığı için +1.
#define CIRCULAR_BUFFER_SIZE 	16001

// 2. SES REVİZYONU: (X, Y, Z) yerine tek ses sayısı.
typedef int16_t Sensor_Data_t;

typedef struct
{
	Sensor_Data_t buffer[CIRCULAR_BUFFER_SIZE];

	// 3. SES REVİZYONU: volatile. head'i kesme yazıyor, ana döngü okuyor.
	volatile uint16_t head;
	volatile uint16_t tail;

}Circular_Buffer_t;

/*****		Buffer Initalization  	*****/
void 	 Circular_Buffer_Init 		 (Circular_Buffer_t *circularbuffer);

bool  	 Circular_Buffer_Dequeue     (Circular_Buffer_t *circularbuffer , Sensor_Data_t *data);
bool 	 Circular_Buffer_Enqueue     (Circular_Buffer_t *circularbuffer , Sensor_Data_t data);

//Is Buffer Empty ? | Is Buffer Fully ?
bool 	 Circular_Buffer_Is_Empty 	 (Circular_Buffer_t *circularbuffer);
bool 	 Circular_Buffer_Is_Fully	 (Circular_Buffer_t *circularbuffer);

uint16_t Circular_Buffer_Count	     (Circular_Buffer_t *circularbuffer);

#ifdef __cplusplus
}
#endif

#endif /* INC_CIRCULAR_BUFFER_H_ */
