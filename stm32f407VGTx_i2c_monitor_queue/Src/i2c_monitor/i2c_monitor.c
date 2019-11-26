/*
 * i2c_monitor.c
 *
 *  Created on: Nov 20, 2019
 *      Author: pnguyen
 */
#include "i2c_monitor.h"
#include "usbd_cdc_if.h"
#include <stdio.h>


/***************** defines go here ************************/
#define IsSdaHi(i2c)		(HAL_GPIO_ReadPin(i2c->sda_port,i2c->sda_pin)==GPIO_PIN_SET)
#define IsSdaLo(i2c)		(HAL_GPIO_ReadPin(i2c->sda_port,i2c->sda_pin)==GPIO_PIN_RESET)
#define IsSclHi(i2c)		(HAL_GPIO_ReadPin(i2c->scl_port,i2c->scl_pin)==GPIO_PIN_SET)
#define IsSclLo(i2c)		(HAL_GPIO_ReadPin(i2c->scl_port,i2c->scl_pin)==GPIO_PIN_RESET)

/***************** local functions go here ************************/
static void i2c_mon_sda(struct i2c_monitor_t *i2c, GPIO_TypeDef *port, uint16_t pin);
static void i2c_mon_scl(struct i2c_monitor_t *i2c, GPIO_TypeDef *port, uint16_t pin);

/*
 * get the i2c timestamp for every transaction
 *
 */
#if 0
static void get_timestamp(struct i2c_monitor_t *i2c) {
	uint32_t sys_tick;
	uint8_t size = sizeof(uint32_t);
	sys_tick = HAL_GetTick();
	for (int i=0; i < size; i++) {
		i2c->i2c_buffer[i2c->i2c_buffer_idx++] = (sys_tick >> ((size-i - 1)*8) & 0xFF);
	}
}
#endif
/* user sda pin configure */
static void i2c_mon_sda(struct i2c_monitor_t *i2c, GPIO_TypeDef *port, uint16_t pin) {
	i2c->sda_port = port;
	i2c->sda_pin = pin;
}

/* user scl pin configure */
static void i2c_mon_scl(struct i2c_monitor_t *i2c, GPIO_TypeDef *port, uint16_t pin) {
	i2c->scl_port = port;
	i2c->scl_pin = pin;
}

/*
 * indicate end of i2c transaction.
 */
static void s_add_i2c_end_header(struct i2c_monitor_t *i2c) {
	i2c->i2c_buffer[i2c->i2c_buffer_idx++] = ']';
}

/*
 * indcate start of the i2c transaction.
 */
static void s_add_i2c_start_header(struct i2c_monitor_t *i2c) {
	i2c->i2c_buffer[i2c->i2c_buffer_idx++] = '[';
}


/*
 * convert hex to ascii
 */
static void s_hex2ascii(struct i2c_monitor_t *i2c, uint8_t data) {
	uint8_t val;
	if (data >= 0 && data <= 9) {
		val = data + '0';
	} else if (data >= 'a' && data <= 'f') {
		val = data -  10 + 0x41;
	} else {
		val = data -  10 + 0x61;
	}
	i2c->i2c_buffer[i2c->i2c_buffer_idx++] = val;
}

/*
 * convert from hex to ascii before store the data to the buffer
 */
static void s_add_i2c_data(struct i2c_monitor_t *i2c, uint16_t data) {
	s_hex2ascii(i2c, (data  >> 4) & 0xf);
	s_hex2ascii(i2c, (data >>  0) & 0xf);
	s_hex2ascii(i2c, (data >>  12) & 0xf);
	s_hex2ascii(i2c, (data >>  8) & 0xf);
}

/*
 * i2c monitor callback:
 *    monitor sda and scl when interrupt occurs.
 *    sda - interrupt occurs on both edge
 *    scl - interrupt occurs on rising edge.
 */
static void i2c_interrupt_monitor(struct i2c_monitor_t *i2c, uint16_t GPIO_Pin){

	if ((i2c->sda_pin == GPIO_Pin) || (i2c->scl_pin == GPIO_Pin) ) {
		switch(i2c->i2c_state) {
			case I2C_STATUS_IDLE:
				// start condition when sda hi->lo
				if(GPIO_Pin==i2c->sda_pin && IsSdaLo(i2c) && IsSclHi(i2c)) {
					//i2c err
					if (i2c->i2c_done) {
						//add_i2c_data(i2c, I2C_BIT__ERR);
					}
					i2c->i2c_state = I2C_STATUS_START;
					i2c->i2c_bit_count = 0;
					i2c->i2c_data_bit = 0;
					i2c->i2c_buffer_idx = 0;
					i2c->i2c_done = true;
					s_add_i2c_start_header(i2c);
					//get_timestamp(i2c);
				}
				break;
			case I2C_STATUS_START:
			case I2C_STATUS_RESTART:
			case I2C_STATUS_DATA:
				// if scl rising edge detect then log the data.
				if(GPIO_Pin==i2c->scl_pin)	{
					if(i2c->i2c_bit_count >= 8) {
						i2c->i2c_data_bit |= IsSdaLo(i2c) ? 0x100 : 0;
						s_add_i2c_data(i2c, i2c->i2c_data_bit);
						i2c->i2c_bit_count = 0;
						i2c->i2c_data_bit = 0;
					}
					else {
						i2c->i2c_data_bit = (i2c->i2c_data_bit << 1) + (IsSdaHi(i2c) ? 1 : 0);
						i2c->i2c_bit_count++;
					}
				}
				// if sda is 0 and scl is 1 //then restarted condition detected
				else if(GPIO_Pin == i2c->sda_pin && IsSdaLo(i2c) && IsSclHi(i2c))  {
					i2c->i2c_state = I2C_STATUS_RESTART;
					i2c->i2c_bit_count 	= 0;
					i2c->i2c_data_bit 	= 0;
				}
				// if sda is hi and scl is lo then stopped condition detected
				else if((GPIO_Pin == i2c->sda_pin) && IsSclHi(i2c) && IsSdaHi(i2c))	{
					//get the time stamp
					//get_timestamp(i2c);
					s_add_i2c_end_header(i2c);
					//let the main application know that i2c data is available
					//to process.
					if (i2c->i2c_app_cb) {
						i2c->i2c_app_cb(i2c->i2c_buffer, &i2c->i2c_buffer_idx);
					}
					//reset all the variables
					i2c->i2c_bit_count	= 0;
					i2c->i2c_buffer_idx = 0;
					i2c->i2c_done		= false;
					i2c->i2c_state = I2C_STATUS_IDLE;
				}
				break;
			}
		}
}

/* create an instance of i2c */
void i2c_mon_create(struct i2c_monitor_t *i2c, i2c_app_callback callback) {

	i2c->sda_pin_set 			= i2c_mon_sda;
	i2c->scl_pin_set 			= i2c_mon_scl;
	i2c->i2c_int_cb				= i2c_interrupt_monitor;
	i2c->i2c_app_cb				= callback;
	i2c->i2c_buffer_idx			= 0;

	i2c->i2c_state				= I2C_STATUS_IDLE;
	i2c->i2c_bit_count			= 0;
	i2c->i2c_data_bit			= 0;
	i2c->i2c_done				= false;

}


