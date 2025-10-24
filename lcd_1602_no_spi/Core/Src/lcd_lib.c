/*
 * lcd_lib.c
 *
 *  Created on: Oct 11, 2025
 *      Author: eevgs
 */
#include "lcd_lib.h"

// Private helper functions

static void pulse_enable(void) {
	HAL_Delay(1);
	HAL_GPIO_WritePin(GPIOA, EN_Pin, 1);
	HAL_Delay(1);
	HAL_GPIO_WritePin(GPIOA, EN_Pin, 0);
	HAL_Delay(1);
}

static void write_nibble(uint8_t nibble) {
	HAL_GPIO_WritePin(GPIOA, D7_Pin, (nibble >> 3) & 1);
	HAL_GPIO_WritePin(GPIOA, D6_Pin, (nibble >> 2) & 1);
	HAL_GPIO_WritePin(GPIOA, D5_Pin, (nibble >> 1) & 1);
	HAL_GPIO_WritePin(GPIOA, D4_Pin, (nibble >> 0) & 1);
	pulse_enable();
}

static void write_bits(uint16_t bits) {
	HAL_GPIO_WritePin(GPIOA, RS_Pin, (bits >> 9) & 1);
	HAL_GPIO_WritePin(GPIOA, RW_Pin, (bits >> 8) & 1);
	write_nibble(bits >> 4);
	write_nibble(bits);
}

static void set_data_pins_mode(uint32_t mode, uint32_t pull) {
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = D7_Pin | D6_Pin | D5_Pin | D4_Pin;
	GPIO_InitStruct.Mode = mode;
	GPIO_InitStruct.Pull = pull;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

// Public API functions
void LCD_Init(void) {
	HAL_Delay(50);
	write_nibble(0b0010);      // Set 4-bit mode
	write_bits(0b00101000);    // 4-bit, 2 lines, 5x8 font
	write_bits(0b00001100);    // Display on, cursor off
	write_bits(0b00000110);    // Entry mode: increment, no shift
}

void LCD_Write_char(char c) {
	write_bits((0b10 << 8) | c);
}

void LCD_Write_string(char *str) {
	while (*str) {
		LCD_Write_char(*str++);
	}
}

void LCD_Switch_line(void) {
	// Switch data pins to input mode
	set_data_pins_mode(GPIO_MODE_INPUT, GPIO_PULLDOWN);

	// Read current address (RS=0, RW=1)
	HAL_GPIO_WritePin(GPIOA, RS_Pin, 0);
	HAL_GPIO_WritePin(GPIOA, RW_Pin, 1);

	// Read upper nibble (contains address bit 6)
	pulse_enable();
	uint8_t is_line2 = HAL_GPIO_ReadPin(GPIOA, D6_Pin);

	// Read lower nibble (complete the read cycle)
	pulse_enable();

	// Switch data pins back to output mode
	set_data_pins_mode(GPIO_MODE_OUTPUT_PP, GPIO_NOPULL);

	// Toggle between lines
	write_bits(is_line2 ? 0b10000000 : 0b11000000);
}

