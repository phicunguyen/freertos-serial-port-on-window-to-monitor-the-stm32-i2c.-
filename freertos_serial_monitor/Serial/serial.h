/**
 * @file inv_hal_serial_api.h
 *
 * @brief register access APIs
 *
 *****************************************************************************/

#ifndef INV_HAL_SERIAL_API_H
#define INV_HAL_SERIAL_API_H
#include <stdio.h>
#include <conio.h>

 /* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/***** #include statements ***************************************************/
struct AdapterTypedef_t;

/***** typedef  ***************************************************/
typedef void(*serial_mon)(struct AdapterTypedef_t *p_adapter);
typedef bool(*serial_connect)(struct AdapterTypedef_t *p_adapter);
typedef void(*user_callback)(uint8_t *pbuf, uint16_t *plen);
/***** local type definitions ************************************************/

struct AdapterTypedef_t {
	char							*p_name;
	serial_mon						ser_mon;
	serial_connect					is_port_open;
	user_callback					callback;

	int32_t 						Handle;
	unsigned int					port_id_sel;
	bool							port_open;
	uint8_t							buffer[512];
	uint16_t						bufindx;
};

/***** public functions ******************************************************/

bool serial_devices_mon(void);
void serial_devices_get(char **p_connection[]);
void serial_devices_create(struct AdapterTypedef_t *p_adapter, user_callback callback, char *portname);
bool serial_devices_open(struct AdapterTypedef_t *p_adapter);
#endif //SI_HAL_SERIAL_API_H
