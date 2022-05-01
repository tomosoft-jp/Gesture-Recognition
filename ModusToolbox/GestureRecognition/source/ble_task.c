/*******************************************************************************
 * File Name:   ble_task.c
 *
 * Description: This file contains the task that handles ble events and
 * notifications.
 *
 * Related Document: See README.md
 *
 *******************************************************************************
 * Copyright 2021-2022, Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
 *
 * This software, including source code, documentation and related
 * materials ("Software") is owned by Cypress Semiconductor Corporation
 * or one of its affiliates ("Cypress") and is protected by and subject to
 * worldwide patent protection (United States and foreign),
 * United States copyright laws and international treaty provisions.
 * Therefore, you may use this Software only as provided in the license
 * agreement accompanying the software package from which you
 * obtained this Software ("EULA").
 * If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
 * non-transferable license to copy, modify, and compile the Software
 * source code solely for use in connection with Cypress's
 * integrated circuit products.  Any reproduction, modification, translation,
 * compilation, or representation of this Software except as specified
 * above is prohibited without the express written permission of Cypress.
 *
 * Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
 * reserves the right to make changes to the Software without notice. Cypress
 * does not assume any liability arising out of the application or use of the
 * Software or any product or circuit described in the Software. Cypress does
 * not authorize its products for use in any products where a malfunction or
 * failure of the Cypress product may reasonably be expected to result in
 * significant property damage, injury or death ("High Risk Product"). By
 * including Cypress's product in a High Risk Product, the manufacturer
 * of such system or application assumes all risk of such use and in doing
 * so agrees to indemnify Cypress against all liability.
 *******************************************************************************/

#include <stdlib.h>
#include "cybsp.h"
#include "cyhal.h"
#include "cycfg.h"
#include "cy_retarget_io.h"
#include "cycfg_gap.h"
#include "cycfg_gatt_db.h"
#include "cycfg_bt_settings.h"
#include "wiced_bt_stack.h"
#include "ble_task.h"
#include "wiced_bt_gatt.h"
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <string.h>
#include <timers.h>

/*******************************************************************************
 * Function Prototypes
 *******************************************************************************/

static void ble_app_init(void);
static wiced_result_t ble_app_management_cb(wiced_bt_management_evt_t event,
		wiced_bt_management_evt_data_t *p_event_data);
static wiced_bt_gatt_status_t ble_app_gatt_event_handler(
		wiced_bt_gatt_evt_t event, wiced_bt_gatt_event_data_t *p_data);
static wiced_bt_gatt_status_t ble_app_gatt_read_handler(uint16_t conn_id,
		wiced_bt_gatt_opcode_t opcode, wiced_bt_gatt_read_t *p_read_req,
		uint16_t req_len);
static wiced_bt_gatt_status_t ble_app_gatt_write_handler(uint16_t conn_id,
		wiced_bt_gatt_opcode_t opcode, wiced_bt_gatt_write_req_t *p_write_req,
		uint16_t req_len);
static wiced_bt_gatt_status_t ble_app_gatt_req_write_value(uint16_t attr_handle,
		uint8_t *p_val, uint16_t len);

static wiced_bt_gatt_status_t ble_app_gatt_req_read_by_type_handler(
		uint16_t conn_id, wiced_bt_gatt_opcode_t opcode,
		wiced_bt_gatt_read_by_type_t *p_read_req, uint16_t req_len);

static wiced_bt_gatt_status_t ble_app_gatt_req_read_multi_handler(
		uint16_t conn_id, wiced_bt_gatt_opcode_t opcode,
		wiced_bt_gatt_read_multiple_req_t *p_read_req, uint16_t req_len);
static gatt_db_lookup_table_t* ble_app_find_by_handle(uint16_t handle);
static void ble_print_bd_address(wiced_bt_device_address_t bdadr);
void ble_app_send_notification(void);
void ble_app_send_notification_classification(void);
/*******************************************************************************
 * Global variable
 ******************************************************************************/
extern char classification;     // tomo Stationary:2 Vertical:3 Horizontal:1
bool button1_flg; // tomo button1 status 1:True

/* Queue handle for ble app data */
QueueHandle_t ble_capsense_data_q;

/* Holds the connection ID */
volatile uint16_t ble_connection_id = 0;
/* Holds the capsense data */
ble_capsense_data_t ble_capsense_data;

/**
 * @brief Typdef for function used to free allocated buffer to stack
 */
typedef void (*pfn_free_buffer_t)(uint8_t*);

/*******************************************************************************
 * Function Name: task_ble
 ********************************************************************************
 * Summary:
 *  Task that handles BLE Initilization and updates gaatt nnotification data.
 *
 * Parameters:
 *  void *param : Task parameter defined during task creation (unused)
 *
 *******************************************************************************/
void task_ble(void *param) {
	BaseType_t rtos_api_result = pdFAIL;
	wiced_result_t result = WICED_BT_SUCCESS;

	/* Suppress warning for unused parameter */
	(void) param;

	/* Configure platform specific settings for the BT device */
	cybt_platform_config_init(&cybsp_bt_platform_cfg);

	/* Register call back and configuration with stack */
	result = wiced_bt_stack_init(ble_app_management_cb, &wiced_bt_cfg_settings);

	/* Check if stack initialization was successful */
	if ( CY_RSLT_SUCCESS == result) {
		printf("Bluetooth stack initialization successful!\r\n");
	} else {
		printf("Bluetooth stack initialization failed!\r\n");
		CY_ASSERT(0);
	}
	/* Repeatedly running part of the task */
	for (;;) {
		/* Block until a command has been received over queue */
		rtos_api_result = xQueueReceive(ble_capsense_data_q, &ble_capsense_data,
				1000 / portTICK_PERIOD_MS);  // tomo
		//                                                              portMAX_DELAY);
		/* Command has been received from queue */
		if (pdPASS == rtos_api_result) {
			ble_app_send_notification();

		} else {
			// printf("task_ble classification: %d\n", classification);  // tomo
			ble_app_send_notification_classification();
		}
	}
}

/*******************************************************************************
 * Function Name: ble_app_management_cb
 ********************************************************************************
 * Summary:
 * This function handles the BT stack events.
 *
 * Parameters:
 *  wiced_bt_management_evt_t event: event code
 *  wiced_bt_management_evt_data_t *p_event_data : Data corresponding to the event
 *
 * Return:
 *  wiced_result_t : status
 *
 *******************************************************************************/

wiced_result_t ble_app_management_cb(wiced_bt_management_evt_t event,
		wiced_bt_management_evt_data_t *p_event_data) {
	wiced_result_t status = WICED_BT_SUCCESS;
	wiced_bt_device_address_t bda = { 0 };
	printf("App management cback: 0x%x\r\n", event);
	switch (event) {
	case BTM_ENABLED_EVT:
		/* Bluetooth Controller and Host Stack Enabled */
		if (WICED_BT_SUCCESS == p_event_data->enabled.status) {
			wiced_bt_dev_read_local_addr(bda);
			printf("Bluetooth local device address: ");
			ble_print_bd_address(bda);

			/* Perform application-specific initialization */
			ble_app_init();
		}
		break;
	case BTM_PIN_REQUEST_EVT:
	case BTM_PASSKEY_REQUEST_EVT:
		status = WICED_BT_ERROR;
		break;
	case BTM_DISABLED_EVT:
		break;
	case BTM_BLE_CONNECTION_PARAM_UPDATE:
		ble_print_bd_address(p_event_data->ble_connection_param_update.bd_addr);
		status = WICED_SUCCESS;
		break;
	case BTM_BLE_ADVERT_STATE_CHANGED_EVT:

		/* Advertisement State Changed */
		printf("Advertisement state change: 0x%x\r\n",
				p_event_data->ble_advert_state_changed);
		break;

	default:
		break;
	}

	return status;
}

/*******************************************************************************
 * Function Name: ble_app_gatt_conn_status_cb
 ********************************************************************************
 * Summary:
 * This function is invoked on GATT connection event
 *
 * Parameters:
 *  wiced_bt_gatt_connection_status_t *p_conn_status : GATT connection status
 *
 * Return:
 *  wiced_bt_gatt_status_t status : Status
 *
 *******************************************************************************/

wiced_bt_gatt_status_t ble_app_gatt_conn_status_cb(
		wiced_bt_gatt_connection_status_t *p_conn_status) {
	wiced_bt_gatt_status_t status = WICED_BT_GATT_ERROR;
	wiced_result_t result = WICED_BT_ERROR;

	if ( NULL != p_conn_status) {
		if (p_conn_status->connected) {
			/* Device has connected */
			printf("Bluetooth connected with device address:");
			ble_print_bd_address(p_conn_status->bd_addr);
			printf("Bluetooth device connection id: 0x%x\r\n",
					p_conn_status->conn_id);
			/* Store the connection ID */
			ble_connection_id = p_conn_status->conn_id;
		} else {
			/* Device has disconnected */
			printf("Bluetooth disconnected with device address:");
			ble_print_bd_address(p_conn_status->bd_addr);
			printf("Bluetooth device connection id: 0x%x\r\n",
					p_conn_status->conn_id);
			/* Set the connection id to zero to indicate disconnected state */
			ble_connection_id = 0;
			/* Restart the advertisements */
			result = wiced_bt_start_advertisements(
					BTM_BLE_ADVERT_UNDIRECTED_HIGH, 0, NULL);
			/* Failed to start advertisement. Stop program execution */
			if (CY_RSLT_SUCCESS != result) {
				CY_ASSERT(0);
			}

		}
		status = WICED_BT_GATT_SUCCESS;
	}

	return status;
}

/*******************************************************************************
 * Function Name: ble_app_init
 ********************************************************************************
 * Summary:
 * This function handles application level initialization tasks and is called from
 * the BT management callback once the BLE stack enabled event BTM_ENABLED_EVT is
 * triggered. This function is executed in the BTM_ENABLED_EVT management callback.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *
 *******************************************************************************/

void ble_app_init(void) {
	wiced_bt_gatt_status_t status = WICED_BT_GATT_ERROR;
	wiced_result_t result = WICED_BT_ERROR;

	printf("Discover the device with name: \"%s\"\r\n\r\n",
			app_gap_device_name);
	/* Register with BT stack to receive GATT callback */
	status = wiced_bt_gatt_register(ble_app_gatt_event_handler);
	printf("GATT event handler registration status: 0x%x\r\n", status);

	/* Initialize GATT Database */
	status = wiced_bt_gatt_db_init(gatt_database, gatt_database_len, NULL);
	printf("GATT database initiliazation status: 0x%x\r\n", status);

	/* Disable pairing for this application */
	wiced_bt_set_pairable_mode(WICED_FALSE, WICED_FALSE);

	/* Set Advertisement Data */
	wiced_bt_ble_set_raw_advertisement_data(CY_BT_ADV_PACKET_DATA_SIZE,
			cy_bt_adv_packet_data);

	/* Start Undirected LE Advertisements on device startup.
	 * The corresponding parameters are contained in 'app_bt_cfg.c' */
	result = wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH, 0,
	NULL);

	/* Failed to start advertisement. Stop program execution */
	if (WICED_BT_SUCCESS != result) {
		printf("Failed to start advertisement! \n");
		CY_ASSERT(0);
	}
}

/*******************************************************************************
 * Function Name : ble_app_alloc_buffer
 *******************************************************************************
 *
 * Summary:
 * @brief  This Function allocates the buffer of requested length
 *
 * @param len            Length of the buffer
 *
 * @return uint8_t*      pointer to allocated buffer
 ******************************************************************************/
static uint8_t* ble_app_alloc_buffer(uint16_t len) {
	uint8_t *p = (uint8_t*) malloc(len);
	return p;
}

/*******************************************************************************
 * Function Name : ble_app_free_buffer
 *******************************************************************************
 * Summary :
 * @brief  This Function frees the buffer requested
 *
 * @param p_data         pointer to the buffer to be freed
 *
 * @return void
 ******************************************************************************/
static void ble_app_free_buffer(uint8_t *p_data) {
	if (NULL != p_data) {
		free(p_data);
	}
}

/*******************************************************************************
 * Function Name: ble_app_gatt_event_handler
 ********************************************************************************
 * Summary:
 * This function handles GATT callback events from the BT stack.
 *
 * Parameters:
 *  wiced_bt_gatt_evt_t event                   : BLE GATT event code
 *  wiced_bt_gatt_event_data_t *p_event_data    : Pointer to BLE GATT event data
 *
 * Return:
 *  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e
 *  in wiced_bt_gatt.h
 *
 ********************************************************************************/
wiced_bt_gatt_status_t ble_app_gatt_event_handler(wiced_bt_gatt_evt_t event,
		wiced_bt_gatt_event_data_t *p_data) {
	wiced_bt_gatt_status_t status = WICED_BT_GATT_ERROR;
	wiced_bt_gatt_attribute_request_t *p_attr_req = &p_data->attribute_request;

	/* Call the appropriate callback function based on the GATT event type, and pass the relevant event
	 * parameters to the callback function */

	switch (event) {
	case GATT_CONNECTION_STATUS_EVT:
		status = ble_app_gatt_conn_status_cb(&p_data->connection_status);
		if (WICED_BT_GATT_SUCCESS != status) {
			printf("GATT connection cb failed: 0x%x\r\n", status);
		}
		break;

	case GATT_ATTRIBUTE_REQUEST_EVT:
		switch (p_attr_req->opcode) {
		case GATT_REQ_READ:
			status = ble_app_gatt_read_handler(p_attr_req->conn_id,
					p_attr_req->opcode, &p_attr_req->data.read_req,
					p_attr_req->len_requested);
			break;

		case GATT_REQ_READ_BY_TYPE:
			status = ble_app_gatt_req_read_by_type_handler(p_attr_req->conn_id,
					p_attr_req->opcode, &p_attr_req->data.read_by_type,
					p_attr_req->len_requested);
			break;
		case GATT_REQ_READ_MULTI:
		case GATT_REQ_READ_MULTI_VAR_LENGTH:
			status = ble_app_gatt_req_read_multi_handler(p_attr_req->conn_id,
					p_attr_req->opcode, &p_attr_req->data.read_multiple_req,
					p_attr_req->len_requested);
			break;

		case GATT_REQ_WRITE:
		case GATT_CMD_WRITE:
		case GATT_CMD_SIGNED_WRITE:
			status = ble_app_gatt_write_handler(p_attr_req->conn_id,
					p_attr_req->opcode, &p_attr_req->data.write_req,
					p_attr_req->len_requested);

			if ((GATT_REQ_WRITE == p_attr_req->opcode)
					&& (WICED_BT_GATT_SUCCESS == status)) {
				wiced_bt_gatt_write_req_t *p_write_request =
						&p_attr_req->data.write_req;
				wiced_bt_gatt_server_send_write_rsp(p_attr_req->conn_id,
						p_attr_req->opcode, p_write_request->handle);
			}
			break;
		case GATT_REQ_MTU:
			printf("GATT req mtu \r\n");
			status = wiced_bt_gatt_server_send_mtu_rsp(p_attr_req->conn_id,
					p_attr_req->data.remote_mtu,
					wiced_bt_cfg_settings.p_ble_cfg->ble_max_rx_pdu_size);
			break;
		}
		break;
	case GATT_GET_RESPONSE_BUFFER_EVT: /* GATT buffer request, typically sized to max of bearer mtu - 1 */
		p_data->buffer_request.buffer.p_app_rsp_buffer = ble_app_alloc_buffer(
				p_data->buffer_request.len_requested);
		p_data->buffer_request.buffer.p_app_ctxt = (void*) ble_app_free_buffer;
		status = WICED_BT_GATT_SUCCESS;
		break;
	case GATT_APP_BUFFER_TRANSMITTED_EVT: /* GATT buffer transmitted event,  check \ref wiced_bt_gatt_buffer_transmitted_t*/
	{
		pfn_free_buffer_t pfn_free =
				(pfn_free_buffer_t) p_data->buffer_xmitted.p_app_ctxt;

		/* If the buffer is dynamic, the context will point to a function to free it. */
		if (pfn_free) {
			pfn_free(p_data->buffer_xmitted.p_app_data);
			status = WICED_BT_GATT_SUCCESS;
		}
	}
		break;
	default:
		break;
	}
	return status;
}

/*******************************************************************************
 * Function Name: ble_app_gatt_req_write_value
 ********************************************************************************
 * Summary:
 * This function handles writing to the attribute handle in the GATT database
 * using the data passed from the BT stack. The value to write is stored in a
 * buffer whose starting address is passed as one of the function parameters
 *
 * Parameters:
 * @param attr_handle  GATT attribute handle
 * @param p_val        Pointer to BLE GATT write request value
 * @param len          length of GATT write request
 * Return:
 *  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e
 *  in wiced_bt_gatt.h
 *
 *******************************************************************************/
static wiced_bt_gatt_status_t ble_app_gatt_req_write_value(uint16_t attr_handle,
		uint8_t *p_val, uint16_t len) {
	wiced_bt_gatt_status_t result = WICED_BT_GATT_INVALID_HANDLE;

	/* Check for a matching handle entry */
	for (int i = 0; i < app_gatt_db_ext_attr_tbl_size; i++) {
		if (app_gatt_db_ext_attr_tbl[i].handle == attr_handle) {

			if (app_gatt_db_ext_attr_tbl[i].max_len >= len) {
				/* Value fits within the supplied buffer; copy over the value */
				app_gatt_db_ext_attr_tbl[i].cur_len = len;
				memset(app_gatt_db_ext_attr_tbl[i].p_data, 0x00,
						app_gatt_db_ext_attr_tbl[i].max_len);
				memcpy(app_gatt_db_ext_attr_tbl[i].p_data, p_val,
						app_gatt_db_ext_attr_tbl[i].cur_len);

				if (0
						== memcmp(app_gatt_db_ext_attr_tbl[i].p_data, p_val,
								app_gatt_db_ext_attr_tbl[i].cur_len)) {
					result = WICED_BT_GATT_SUCCESS;
				}

				/* Add code for any action required when this attribute is written.
				 * In this case, we initilize the characteristic value */

				switch (attr_handle) {
				case HDLD_CAPSENSE_BUTTON_CLIENT_CHAR_CONFIG:
					ble_capsense_data.buttoncount = 2u; /* No.of Capsense button */
					ble_capsense_data.buttonstatus1 = 0u; /* Button status byte1 */
					ble_capsense_data.buttonstatus2 = 0u; /* Button status byte2 */
					break;
				case HDLD_CAPSENSE_SLIDER_CLIENT_CHAR_CONFIG:
					ble_capsense_data.sliderdata = 0u; /* Capsense slider data */
					break;
				}

			} else {
				/* Value to write does not meet size constraints */
				result = WICED_BT_GATT_INVALID_ATTR_LEN;
			}
			break;
		}
	}

	if (WICED_BT_GATT_SUCCESS != result) {
		printf("Write request to invalid handle: 0x%x\r\n", attr_handle);
	}

	return result;
}

/*******************************************************************************
 * Function Name: ble_app_gatt_write_handler
 ********************************************************************************
 * Summary:
 * This function handles Write Requests received from the client device
 *
 * Parameters:
 * @param conn_id         Connection ID
 * @param opcode          BLE GATT request type opcode
 * @param p_write_req     Pointer that contains details of Write
 *                        Request including the attribute handle
 * @param req_len   length of data requested
 * Return:
 *  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e
 *  in wiced_bt_gatt.h
 *
 *******************************************************************************/
static wiced_bt_gatt_status_t ble_app_gatt_write_handler(uint16_t conn_id,
		wiced_bt_gatt_opcode_t opcode, wiced_bt_gatt_write_req_t *p_write_req,
		uint16_t req_len) {
	wiced_bt_gatt_status_t result = WICED_BT_GATT_INVALID_HANDLE;
	gatt_db_lookup_table_t *puAttribute = NULL;

	puAttribute = ble_app_find_by_handle(p_write_req->handle);
	if ( NULL == puAttribute) {
		printf(" Attribute not found, Handle: 0x%04x\r\n", p_write_req->handle);
		wiced_bt_gatt_server_send_error_rsp(conn_id, opcode,
				p_write_req->handle, WICED_BT_GATT_INVALID_HANDLE);
		return WICED_BT_GATT_INVALID_HANDLE;
	}

	result = ble_app_gatt_req_write_value(p_write_req->handle,
			p_write_req->p_val, p_write_req->val_len);

	if (WICED_BT_GATT_SUCCESS == result) {
		ble_app_send_notification();
	}

	return WICED_BT_GATT_SUCCESS;

}

/*******************************************************************************
 * Function Name: ble_app_gatt_read_handler
 ********************************************************************************
 * Summary:This function handles Read Requests received from the client device.
 *
 * Parameters:
 * @param conn_id       Connection ID
 * @param opcode        BLE GATT request type opcode
 * @param p_read_req    Pointer to read request containing the handle to read
 * @param req_len length of data requested
 *
 * Return:
 *  wiced_bt_gatt_status_t: See possible status codes in wiced_bt_gatt_status_e
 *  in wiced_bt_gatt.h
 *
 *******************************************************************************/
static wiced_bt_gatt_status_t ble_app_gatt_read_handler(uint16_t conn_id,
		wiced_bt_gatt_opcode_t opcode, wiced_bt_gatt_read_t *p_read_req,
		uint16_t req_len) {
	gatt_db_lookup_table_t *puAttribute = NULL;
	int attr_len_to_copy;
	uint8_t *from;
	int to_send;

	puAttribute = ble_app_find_by_handle(p_read_req->handle);
	if ( NULL == puAttribute) {
		wiced_bt_gatt_server_send_error_rsp(conn_id, opcode, p_read_req->handle,
				WICED_BT_GATT_INVALID_HANDLE);
		return WICED_BT_GATT_INVALID_HANDLE;
	}
	attr_len_to_copy = puAttribute->cur_len;
	if (p_read_req->offset >= puAttribute->cur_len) {
		wiced_bt_gatt_server_send_error_rsp(conn_id, opcode, p_read_req->handle,
				WICED_BT_GATT_INVALID_OFFSET);
		return WICED_BT_GATT_INVALID_OFFSET;
	}
	to_send = MIN(req_len, attr_len_to_copy - p_read_req->offset);
	from = ((uint8_t*) puAttribute->p_data) + p_read_req->offset;
	wiced_bt_gatt_server_send_read_handle_rsp(conn_id, opcode, to_send, from,
	NULL);
	return wiced_bt_gatt_server_send_read_handle_rsp(conn_id, opcode, to_send,
			from, NULL); /* No need for context, as buff not allocated */;

}

/*******************************************************************************
 * Function Name: ble_app_gatt_req_read_by_type_handler
 ********************************************************************************
 * Summary:@brief  Process read-by-type request from peer device
 *
 * @param conn_id       Connection ID
 * @param opcode        BLE GATT request type opcode
 * @param p_read_req    Pointer to read request containing the handle to read
 * @param req_len length of data requested
 *
 * @return wiced_bt_gatt_status_t  BLE GATT status
 *******************************************************************************/
static wiced_bt_gatt_status_t ble_app_gatt_req_read_by_type_handler(
		uint16_t conn_id, wiced_bt_gatt_opcode_t opcode,
		wiced_bt_gatt_read_by_type_t *p_read_req, uint16_t req_len) {
	gatt_db_lookup_table_t *puAttribute = NULL;
	uint16_t attr_handle = p_read_req->s_handle;
	uint8_t *p_rsp = ble_app_alloc_buffer(req_len);
	uint8_t pair_len = 0;
	int used_len = 0;
	if ( NULL == p_rsp) {
		wiced_bt_gatt_server_send_error_rsp(conn_id, opcode, attr_handle,
				WICED_BT_GATT_INSUF_RESOURCE);
		return WICED_BT_GATT_INSUF_RESOURCE;
	}
	/* Read by type returns all attributes of the specified type, between the start and end handles */
	while (WICED_TRUE) {
		attr_handle = wiced_bt_gatt_find_handle_by_type(attr_handle,
				p_read_req->e_handle, &p_read_req->uuid);

		if (0 == attr_handle)
			break;

		puAttribute = ble_app_find_by_handle(attr_handle);
		if ( NULL == puAttribute) {
			wiced_bt_gatt_server_send_error_rsp(conn_id, opcode,
					p_read_req->s_handle, WICED_BT_GATT_ERR_UNLIKELY);
			ble_app_free_buffer(p_rsp);
			return WICED_BT_GATT_INVALID_HANDLE;
		}

		{
			int filled = wiced_bt_gatt_put_read_by_type_rsp_in_stream(
					p_rsp + used_len, req_len - used_len, &pair_len,
					attr_handle, puAttribute->cur_len, puAttribute->p_data);
			if (0 == filled) {
				break;
			}
			used_len += filled;
		}
		/* Increment starting handle for next search to one past current */
		attr_handle++;
	}
	if (!used_len) {
		wiced_bt_gatt_server_send_error_rsp(conn_id, opcode,
				p_read_req->s_handle, WICED_BT_GATT_INVALID_HANDLE);
		ble_app_free_buffer(p_rsp);
		return WICED_BT_GATT_INVALID_HANDLE;
	}
	/* Send the response */
	wiced_bt_gatt_server_send_read_by_type_rsp(conn_id, opcode, pair_len,
			used_len, p_rsp, (void*) ble_app_free_buffer);
	return WICED_BT_GATT_SUCCESS;
}

/*******************************************************************************
 * Function Name: ble_app_gatt_req_read_multi_handler
 ********************************************************************************
 * Summary : @brief  Process write read multi request from peer device
 *
 * @param conn_id       Connection ID
 * @param opcode        BLE GATT request type opcode
 * @param p_read_req    Pointer to read request containing the handle to read
 * @param req_len length of data requested
 *
 * @return wiced_bt_gatt_status_t  BLE GATT status
 ******************************************************************************/
static wiced_bt_gatt_status_t ble_app_gatt_req_read_multi_handler(
		uint16_t conn_id, wiced_bt_gatt_opcode_t opcode,
		wiced_bt_gatt_read_multiple_req_t *p_read_req, uint16_t req_len) {
	gatt_db_lookup_table_t *puAttribute = NULL;
	uint8_t *p_rsp = ble_app_alloc_buffer(req_len);
	int used_len = 0;
	int count;
	uint16_t handle = wiced_bt_gatt_get_handle_from_stream(
			p_read_req->p_handle_stream, 0);
	if ( NULL == p_rsp) {
		wiced_bt_gatt_server_send_error_rsp(conn_id, opcode, handle,
				WICED_BT_GATT_INSUF_RESOURCE);
		return WICED_BT_GATT_INVALID_HANDLE;
	}
	/* Read by type returns all attributes of the specified type, between the start and end handles */
	for (count = 0; count < p_read_req->num_handles; count++) {
		handle = wiced_bt_gatt_get_handle_from_stream(
				p_read_req->p_handle_stream, count);
		puAttribute = ble_app_find_by_handle(handle);
		if ( NULL == puAttribute) {
			wiced_bt_gatt_server_send_error_rsp(conn_id, opcode,
					*p_read_req->p_handle_stream, WICED_BT_GATT_ERR_UNLIKELY);
			ble_app_free_buffer(p_rsp);
			return WICED_BT_GATT_ERR_UNLIKELY;
		} else {
			int filled = wiced_bt_gatt_put_read_multi_rsp_in_stream(opcode,
					p_rsp + used_len, req_len - used_len, puAttribute->handle,
					puAttribute->cur_len, puAttribute->p_data);
			if (!filled) {
				break;
			}
			used_len += filled;
		}
	}
	if (!used_len) {
		wiced_bt_gatt_server_send_error_rsp(conn_id, opcode,
				*p_read_req->p_handle_stream, WICED_BT_GATT_INVALID_HANDLE);
		ble_app_free_buffer(p_rsp);
		return WICED_BT_GATT_INVALID_HANDLE;
	}
	/* Send the response */
	wiced_bt_gatt_server_send_read_multiple_rsp(conn_id, opcode, used_len,
			p_rsp, (void*) ble_app_free_buffer);
	return WICED_BT_GATT_SUCCESS;
}

/*******************************************************************************
 * Function Name: void ble_app_send_notification
 ********************************************************************************
 * Summary: Sends GATT notification.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *
 *******************************************************************************/

void ble_app_send_notification(void) {
	wiced_bt_gatt_status_t status = WICED_BT_GATT_ERROR;

	if ((GATT_CLIENT_CONFIG_NOTIFICATION
			== app_capsense_slider_client_char_config[0])
			&& (0 != ble_connection_id)) {
		/* capsense slider data to be send*/
		app_capsense_slider[0] = ble_capsense_data.sliderdata;
		status = wiced_bt_gatt_server_send_notification(ble_connection_id,
		HDLC_CAPSENSE_SLIDER_VALUE, app_gatt_db_ext_attr_tbl[4].cur_len,
				app_gatt_db_ext_attr_tbl[4].p_data, NULL);

		if (WICED_BT_GATT_SUCCESS != status) {
			printf("Sending CapSense slider notification failed\r\n");
		}

	}
	// tomo
	if ((GATT_CLIENT_CONFIG_NOTIFICATION
			== app_capsense_button_client_char_config[0])
			&& (0 != ble_connection_id)) {
		/* capsense button data to be send*/
		//printf("buttonstatus1: %d\n", ble_capsense_data.buttonstatus1);
		if (ble_capsense_data.buttonstatus1 == 1) {
			button1_flg = true;
		}else{
			button1_flg = false;

		}
	}

}
// tomo stay:0 up:1 down:2 left:3 right:4
// tomo Stationary:2 Vertical:3 Horizontal:1
void ble_app_send_notification_classification(void) {   // tomo
	wiced_bt_gatt_status_t status = WICED_BT_GATT_ERROR;
	char direction;

	if ((GATT_CLIENT_CONFIG_NOTIFICATION
			== app_capsense_button_client_char_config[0])
			&& (0 != ble_connection_id)) {

		if (classification == 2) {
			direction = 0;
		} else if ((classification == 3) && !(button1_flg)) {
			direction = 1;
		} else if ((classification == 3) && (button1_flg)) {
			direction = 2;
		} else if ((classification == 1) && !(button1_flg)) {
			direction = 3;
		} else {
			direction = 4;
		}
		printf("direction: %d\n", direction);

		app_capsense_button[0] = direction;
		app_capsense_button[1] = 0;
		app_capsense_button[2] = 0;
		status = wiced_bt_gatt_server_send_notification(ble_connection_id,
		HDLC_CAPSENSE_BUTTON_VALUE, app_gatt_db_ext_attr_tbl[2].cur_len,
				app_gatt_db_ext_attr_tbl[2].p_data, NULL);

		if (WICED_BT_GATT_SUCCESS != status) {
			printf("Sending CapSense button notification failed\r\n");
		}
	}
}

/*******************************************************************************
 * Function Name: ble_print_bd_address
 ********************************************************************************
 * Summary: This is the utility function that prints the address of the Bluetooth device
 *
 * Parameters:
 *  wiced_bt_device_address_t bdaddr : Bluetooth address
 *
 * Return:
 *  None
 *
 *******************************************************************************/
static void ble_print_bd_address(wiced_bt_device_address_t bdadr) {
	for (uint8_t i = 0; i < BD_ADDR_LEN - 1; i++) {
		printf("%2X:", bdadr[i]);
	}
	printf("%2X\n", bdadr[BD_ADDR_LEN - 1]);
}
/*******************************************************************************
 * Function Name : ble_app_find_by_handle
 * *****************************************************************************
 * Summary : @brief  Find attribute description by handle
 *
 * @param handle    handle to look up
 *
 * @return gatt_db_lookup_table_t   pointer containing handle data
 ******************************************************************************/
static gatt_db_lookup_table_t* ble_app_find_by_handle(uint16_t handle) {
	int i;
	for (i = 0; i < app_gatt_db_ext_attr_tbl_size; i++) {
		if (handle == app_gatt_db_ext_attr_tbl[i].handle) {
			return (&app_gatt_db_ext_attr_tbl[i]);
		}
	}
	return NULL;
}
/* END OF FILE [] */
