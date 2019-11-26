#include <stdio.h>
#include <conio.h>
#include <windows.h>
#include <strsafe.h>
#include <stdbool.h>

/* FreeRTOS.org includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "serial.h"

/* Demo includes. */
#include "supporting_functions.h"
#define MAX_SERIAL_PORT		10
char *serial_list[MAX_SERIAL_PORT] = { NULL };
struct AdapterTypedef_t adapter = { 0 };

/*
* monitor serial data come from stm32 vcp
*/
void vSerialTask(void *pvParameters) {
	pvParameters = pvParameters;
	while (true) {
		if (adapter.Handle) {
			if (adapter.is_port_open(&adapter)) {
				adapter.ser_mon(&adapter);
			}
		}
		vTaskDelay(1);
	}
}

/*
* convert from ascii to hex
*/
static uint8_t ascii2hex(uint8_t data) {
	if (data >= '0' && data <= '9') {
		return data - '0';
	}
	else if (data >= 'a' && data <= 'f') {
		return data - 'a' + 10;
	}
	else if (data >= 'A' && data <= 'F') {
		return data - 'A' + 10;
	}
	return 0;
}

/*
* register the callback to i2c driver.
* when a i2c stop condition is detected then it will call this function.
*/
static void app_callback(uint8_t *pbuf, uint16_t *plen) {
	uint16_t j = 0;
	printf("\n");
	uint8_t buff[256];

	for (int i = 0; i < *plen; i+=4) {
		buff[j++] = (ascii2hex(pbuf[i]) << 4) | ascii2hex(pbuf[i + 1]);
	}
	for (int i = 0; i < j; i++) {
		printf("%02X", buff[i]);
	}

}

/*
* create a serial connection.
*/
void serial_create(void) {
	char **p_ptr;
	//check if stm32 comport in the window
	if (serial_devices_mon()) {
		int id = 0;
		//get the stm32 comport list
		serial_devices_get(&serial_list);
		p_ptr = serial_list;
		if (*p_ptr != NULL) {
			printf("\nlist of stm32 comports:\n");
			while (*p_ptr) {
				printf("%d: %s\n", id++, *p_ptr++);
			}
			printf("Please select comport: ");
			scanf("%d", &id);
			//create a serial port instance
			serial_devices_create(&adapter, app_callback, serial_list[id]);
			//now open it.
			if (serial_devices_open(&adapter)) {
				printf("Serial port open successfully\n");
			}
		}
	}
}

int main()
{	
	//look for the stm32 comport.
	serial_create();

	//task to monitor the serial port add or remove from window
	//xTaskCreate(vConnectionTask, "connection", 1024, NULL, 1, NULL);
	//task to read the serial data
	xTaskCreate(vSerialTask, "connection", 1024, NULL, 1, NULL);
	vTaskStartScheduler();
	for (;; );
}
