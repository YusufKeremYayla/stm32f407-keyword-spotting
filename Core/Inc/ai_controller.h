/*
 * ai_controller.h
 *
 *  Created on: 8 Eki 2026
 *      Author: yusuf
 */

#ifndef INC_AI_CONTROLLER_H_
#define INC_AI_CONTROLLER_H_

#ifndef AI_CONTROLLER_H
#define AI_CONTROLLER_H

#include "circular_buffer.h"
#include "main.h" // (GPIO) tanıyabilmesi için

#ifdef __cplusplus
extern "C" {
#endif

// 1 saniye = 16000 ses (Edge Impulse: window 1000 ms, 16 kHz)
#define AI_PENCERE		16000

// 1: kaydı UART'a yaz (teşhis / Edge Impulse verisi), 0: modeli çalıştır
#define AI_VERI_TOPLAMA		0

// Yapay zekayı tetikleyecek tek fonksiyonumuz
void AI_Process_Buffer(Circular_Buffer_t *kuyruk);

#ifdef __cplusplus
}
#endif

#endif /* AI_CONTROLLER_H */

#endif /* INC_AI_CONTROLLER_H_ */
