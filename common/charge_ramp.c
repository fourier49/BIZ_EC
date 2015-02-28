/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Charge input current limit ramp module for Chrome EC */

#include "charge_manager.h"
#include "charge_ramp.h"
#include "common.h"
#include "console.h"
#include "task.h"
#include "timer.h"
#include "usb_pd.h"
#include "usb_pd_config.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)

/* Number of times to ramp current searching for limit before stable charging */
#define RAMP_COUNT          3

/*
 * Time to delay for detecting the charger type (must be long enough for BC1.2
 * driver to get supplier information and notify charge manager).
 */
#define CHARGE_DETECT_DELAY (2*SECOND)

/* Maximum allowable time charger can be unplugged to be considered an OCP */
#define OC_RECOVER_MAX_TIME (SECOND)

/* Delay for running state machine when board is not consuming full current */
#define CURRENT_DRAW_DELAY  (5*SECOND)

/* Current ramp increment */
#define RAMP_CURR_INCR_MA   64
#define RAMP_CURR_DELAY     (500*MSEC)

/* How much to backoff the input current limit when limit has been found */
#define RAMP_ICL_BACKOFF    (2*RAMP_CURR_INCR_MA)

/* Interval at which VBUS voltage is monitored in stable state */
#define STABLE_VBUS_MONITOR_INTERVAL (SECOND)

/* Time to delay for stablizing the charging current */
#define STABLIZE_DELAY (5*SECOND)

enum chg_ramp_state {
	CHG_RAMP_DISCONNECTED,
	CHG_RAMP_CHARGE_DETECT,
	CHG_RAMP_OVERCURRENT_DETECT,
	CHG_RAMP_RAMP,
	CHG_RAMP_STABILIZE,
	CHG_RAMP_STABLE,
};
static enum chg_ramp_state ramp_st;

struct oc_info {
	timestamp_t ts;
	uint64_t recover;
	int sup;
	int icl;
};

/* OCP info for each over-current */
static struct oc_info oc_info[PD_PORT_COUNT][RAMP_COUNT];
static int oc_info_idx[PD_PORT_COUNT];
#define ACTIVE_OC_INFO (oc_info[active_port][oc_info_idx[active_port]])

/* Active charging information */
static int active_port = CHARGE_PORT_NONE;
static int active_sup;
static int active_icl;
static timestamp_t reg_time;

static int stablize_port;
static int stablize_sup;

/* Maximum/minimum input current limit for active charger */
static int max_icl;
static int min_icl;


void chg_ramp_charge_supplier_change(int port, int supplier, int current,
				     timestamp_t registration_time)
{
	/*
	 * If the last active port was a valid port and the port
	 * has changed, then this may have been an over-current.
	 */
	if (active_port != CHARGE_PORT_NONE &&
	    port != active_port) {
		if (oc_info_idx[active_port] == RAMP_COUNT - 1)
			oc_info_idx[active_port] = 0;
		else
			oc_info_idx[active_port]++;
		ACTIVE_OC_INFO.ts = get_time();
		ACTIVE_OC_INFO.sup = active_sup;
		ACTIVE_OC_INFO.icl = active_icl;
	}

	/* Set new active port, set ramp state, and wake ramp task */
	active_port = port;
	active_sup = supplier;
	min_icl = current;
	max_icl = board_get_ramp_current_limit(active_sup);
	reg_time = registration_time;
	if (ramp_st != CHG_RAMP_STABILIZE) {
		ramp_st = (active_port == CHARGE_PORT_NONE) ?
			  CHG_RAMP_DISCONNECTED : CHG_RAMP_CHARGE_DETECT;
		CPRINTS("Ramp reset: st%d\n", ramp_st);
		task_wake(TASK_ID_CHG_RAMP);
	}
}

int chg_ramp_get_current_limit(void)
{
	/*
	 * If we are ramping or stable, then use the active input
	 * current limit. Otherwise, use the minimum input current
	 * limit.
	 */
	switch (ramp_st) {
	case CHG_RAMP_RAMP:
	case CHG_RAMP_STABILIZE:
	case CHG_RAMP_STABLE:
		return active_icl;
	default:
		return min_icl;
	}
}

void chg_ramp_task(void)
{
	int task_wait_time = -1;
	int i;
	/*
	 * Static initializer so that we don't clobber early calls to this
	 * module.
	 */
	static enum chg_ramp_state ramp_st_prev = CHG_RAMP_DISCONNECTED,
				   ramp_st_new = CHG_RAMP_DISCONNECTED;
	int active_icl_new;

	/* Clear last OCP supplier to guarantee we ramp on first connect */
	for (i = 0; i < PD_PORT_COUNT; i++)
		oc_info[i][0].sup = CHARGE_SUPPLIER_NONE;

	while (1) {
		ramp_st_new = ramp_st;
		active_icl_new = active_icl;
		switch (ramp_st) {
		case CHG_RAMP_DISCONNECTED:
			/* Do nothing */
			task_wait_time = -1;
			break;
		case CHG_RAMP_CHARGE_DETECT:
			/* Delay for charge_manager to determine supplier */
			/* On entry to state, store the OC recovery time */
			if (ramp_st_prev != ramp_st)
				ACTIVE_OC_INFO.recover =
					reg_time.val - ACTIVE_OC_INFO.ts.val;

			/*
			 * If we are not drawing full charge, then don't ramp,
			 * just wait in this state, until we are.
			 */
			if (!board_is_consuming_full_charge()) {
				task_wait_time = CURRENT_DRAW_DELAY;
				break;
			}

			ramp_st_new = CHG_RAMP_OVERCURRENT_DETECT;
			task_wait_time = CHARGE_DETECT_DELAY;
			break;
		case CHG_RAMP_OVERCURRENT_DETECT:
			/* Check if we should ramp or go straight to stable */
			task_wait_time = SECOND;

			/* Skip ramp for specific suppliers */
			if (!board_is_ramp_allowed(active_sup)) {
				active_icl_new = min_icl;
				ramp_st_new = CHG_RAMP_STABLE;
				break;
			}

			/*
			 * Compare recent OCP events, if all info matches,
			 * then we don't need to ramp anymore.
			 */
			for (i = 0; i < RAMP_COUNT; i++) {
				if (oc_info[active_port][i].sup != active_sup ||
				    oc_info[active_port][i].recover >
				    OC_RECOVER_MAX_TIME)
					break;
			}

			if (i == RAMP_COUNT) {
				/* Found OC threshold! */
				active_icl_new = ACTIVE_OC_INFO.icl -
						 RAMP_ICL_BACKOFF;
				ramp_st_new = CHG_RAMP_STABLE;
			} else {
				/*
				 * Need to ramp to find OC threshold, start
				 * at the minimum input current limit.
				 */
				active_icl_new = min_icl;
				ramp_st_new = CHG_RAMP_RAMP;
			}
			break;
		case CHG_RAMP_RAMP:
			/* Keep ramping until we find the limit */
			task_wait_time = RAMP_CURR_DELAY;

			/* Pause ramping if we are not drawing full current */
			if (!board_is_consuming_full_charge()) {
				task_wait_time = CURRENT_DRAW_DELAY;
				break;
			}

			/* If VBUS is sagging a lot, then stop ramping */
			if (board_is_vbus_too_low(CHG_RAMP_VBUS_RAMPING)) {
				CPRINTS("VBUS low");
				active_icl_new = MAX(min_icl, active_icl -
							      RAMP_ICL_BACKOFF);
				ramp_st_new = CHG_RAMP_STABILIZE;
				task_wait_time = STABLIZE_DELAY;
				stablize_port = active_port;
				stablize_sup = active_sup;
				break;
			}

			/* Ramp the current limit if we haven't reached max */
			if (active_icl == max_icl)
				ramp_st_new = CHG_RAMP_STABLE;
			else if (active_icl + RAMP_CURR_INCR_MA > max_icl)
				active_icl_new = max_icl;
			else
				active_icl_new = active_icl + RAMP_CURR_INCR_MA;
			break;
		case CHG_RAMP_STABILIZE:
			/* Wait for current to stabilize after ramp is done */
			/* Use default delay for exiting this state */
			task_wait_time = SECOND;
			if (active_port == stablize_port &&
			    active_sup == stablize_sup) {
				ramp_st_new = CHG_RAMP_STABLE;
				break;
			}

			ramp_st_new = active_port == CHARGE_PORT_NONE ?
				      CHG_RAMP_DISCONNECTED :
				      CHG_RAMP_CHARGE_DETECT;
			break;
		case CHG_RAMP_STABLE:
			/* Maintain input current limit */
			/* On entry log charging stats */
#ifdef CONFIG_USB_PD_LOGGING
			if (ramp_st_prev != ramp_st)
				charge_manager_save_log(active_port);
#endif

			/* Keep an eye on VBUS and restart ramping if it dips */
			if (board_is_ramp_allowed(active_sup) &&
			    board_is_vbus_too_low(CHG_RAMP_VBUS_STABLE)) {
				CPRINTS("VBUS low; Re-ramp");
				active_icl_new = min_icl;
				ramp_st_new = CHG_RAMP_RAMP;
			}
			task_wait_time = STABLE_VBUS_MONITOR_INTERVAL;
			break;
		}
		if (ramp_st != ramp_st_new || active_icl != active_icl_new)
			CPRINTS("Ramp p%d st%d %dmA %dmA",
				active_port, ramp_st_new, min_icl,
				active_icl_new);

		ramp_st_prev = ramp_st;
		ramp_st = ramp_st_new;
		active_icl = active_icl_new;

		/* Set the input current limit */
		board_set_charge_limit(chg_ramp_get_current_limit());

		if (ramp_st == CHG_RAMP_STABILIZE)
			/*
			 * When in stabilize state, supplier/port may change
			 * and we don't want to wake up task until we have
			 * slept this amount of time.
			 */
			usleep(task_wait_time);
		else
			task_wait_event(task_wait_time);
	}
}

#ifdef CONFIG_CMD_CHGRAMP
static int command_chgramp(int argc, char **argv)
{
	int i;
	int port;

	ccprintf("Chg Ramp:\nState: %d\nMin ICL: %d\nActive ICL: %d\n",
		 ramp_st, min_icl, active_icl);

	for (port = 0; port < PD_PORT_COUNT; port++) {
		ccprintf("Port %d:\n", port);
		ccprintf("  OC idx:%d\n", oc_info_idx[port]);
		for (i = 0; i < RAMP_COUNT; i++) {
			ccprintf("  OC %d: s%d recover%lu icl%d\n", i,
				 oc_info[port][i].sup,
				 oc_info[port][i].recover,
				 oc_info[port][i].icl);
		}
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(chgramp, command_chgramp,
	"",
	"Dump charge ramp state info",
	NULL);
#endif