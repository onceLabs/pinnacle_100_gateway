/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <dk_buttons_and_leds.h>

#include <zephyr/drivers/modem/hl7800.h>

#include "p100_socket.h"
#include "ble.h"

static K_SEM_DEFINE(lte_connected, 0, 1);

LOG_MODULE_REGISTER(Pinnacle_100_Gateway, LOG_LEVEL_INF);

static struct mdm_hl7800_apn *lte_apn_config;
static struct mdm_hl7800_callback_agent hl7800_evt_agent;

static void modem_event_callback(enum mdm_hl7800_event event, void *event_data)
{
	uint8_t code = ((struct mdm_hl7800_compound_event *)event_data)->code;
	char *s = (char *)event_data;

	switch (event)
	{
	case HL7800_EVENT_NETWORK_STATE_CHANGE:
		switch (code)
		{
		case HL7800_HOME_NETWORK:
		case HL7800_ROAMING:
			dk_set_led_on(DK_LED4);

			dk_set_led_off(DK_LED1);
			dk_set_led_off(DK_LED2);
			dk_set_led_off(DK_LED3);

			k_sem_give(&lte_connected);
			break;

		case HL7800_NOT_REGISTERED:
		case HL7800_REGISTRATION_DENIED:
		case HL7800_UNABLE_TO_CONFIGURE:
		case HL7800_OUT_OF_COVERAGE:
			dk_set_led_on(DK_LED3);
			break;

		case HL7800_SEARCHING:
			dk_set_led_on(DK_LED1);
			break;

		case HL7800_EMERGENCY:
		default:

			break;
		}
		break;

	case HL7800_EVENT_APN_UPDATE:
		/* event data points to static data stored in modem driver.
		 * Store the pointer so we can access the APN elsewhere in our app.
		 */
		lte_apn_config = (struct mdm_hl7800_apn *)event_data;
		break;

	case HL7800_EVENT_RSSI:
		break;

	case HL7800_EVENT_SINR:
		break;

	case HL7800_EVENT_STARTUP_STATE_CHANGE:
		switch (code)
		{
		case HL7800_STARTUP_STATE_READY:
		case HL7800_STARTUP_STATE_WAITING_FOR_ACCESS_CODE:
			break;
		case HL7800_STARTUP_STATE_SIM_NOT_PRESENT:
		case HL7800_STARTUP_STATE_SIMLOCK:
		case HL7800_STARTUP_STATE_UNRECOVERABLE_ERROR:
		case HL7800_STARTUP_STATE_UNKNOWN:
		case HL7800_STARTUP_STATE_INACTIVE_SIM:
		default:
			break;
		}
		break;

	case HL7800_EVENT_SLEEP_STATE_CHANGE:
		break;

	case HL7800_EVENT_RAT:
		break;

	case HL7800_EVENT_BANDS:
		break;

	case HL7800_EVENT_ACTIVE_BANDS:
		break;

	default:
		LOG_ERR("Unknown modem event");
		break;
	}
}

static void modem_configure(void)
{
	hl7800_evt_agent.event_callback = modem_event_callback;
	mdm_hl7800_register_event_callback(&hl7800_evt_agent);
}

void main(void)
{
	int ret = 0;

	if (dk_leds_init() != 0)
	{
		LOG_ERR("Failed to initialize the LEDs Library");
	}

	dk_set_led_on(DK_LED1);
	modem_configure();

	LOG_INF("Connecting to LTE network");

	k_sem_take(&lte_connected, K_FOREVER);

	LOG_INF("Connected to LTE network");
	p100_socket_init();
	ble_init();
}
