#include <errno.h>
#include <error.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timex.h>
#include <time.h>
#include <unistd.h>

#include <ubloxcfg/ff_epoch.h>
#include <ubloxcfg/ff_parser.h>
#include <ubloxcfg/ff_rx.h>
#include <ubloxcfg/ff_stuff.h>
#include <ubloxcfg/ff_ubx.h>
#include <ubloxcfg/ubloxcfg.h>

#include "config.h"
#include "gnss.h"
#include "gnss-config.h"
#include "log.h"
#include "utils.h"

#define NUM_SAT_MIN 3

#define GNSS_CONNECT_MAX_TRY 5

#define GNSS_TIMEOUT_MS 1000
#define GNSS_RECONFIGURE_MAX_TRY 5
#define SEC_IN_WEEK 604800

#define GPS_EPOCH_TO_TAI 315964819

#define GAL_EPOCH_TO_GPS 619315200
#define GAL_EPOCH_TO_TAI GAL_EPOCH_TO_GPS + GPS_EPOCH_TO_TAI

#define BDS_EPOCH_TO_GPS 820108814
#define BDS_EPOCH_TO_TAI BDS_EPOCH_TO_GPS + GPS_EPOCH_TO_TAI

#define GLO_EPOCH_TO_TAI 315954019

#define ARRAY_SIZE(_A) (sizeof(_A) / sizeof((_A)[0]))

/** Survey In min duration (s) */
#define SVIN_MIN_DUR 1200
/** Survey In max duration allowed (s) */
#define SVIN_MAX_DUR SVIN_MIN_DUR + 600


#ifndef FLAG
#define FLAG(field, flag) ( ((field) & (flag)) == (flag) )
#endif

enum AntennaStatus {
	ANT_STATUS_INIT,
	ANT_STATUS_DONT_KNOW,
	ANT_STATUS_OK,
	ANT_STATUS_SHORT,
	ANT_STATUS_OPEN,
	ANT_STATUS_UNDEFINED
};

enum AntennaPower {
	ANT_POWER_OFF,
	ANT_POWER_ON,
	ANT_POWER_DONTKNOW,
	ANT_POWER_IDLE,
	ANT_POWER_UNDEFINED
};

enum SurveyInState {
	SURVEY_IN_KO,
	SURVEY_IN_UNKNOWN,
	SURVEY_IN_IN_PROGRESS,
	SURVEY_IN_COMPLETED
};

static const char *fix_log[11] = {
	"unknown",
	"no fix",
	"DR only",
	"time",
	"2D",
	"3D",
	"3D_DR",
	"RTK_FLOAT",
	"RTK_FIXED",
	"RTK_FLOAT_DR",
	"RTK_FIXED_DR",
};

static void * gnss_thread(void * p_data);

static int gnss_get_satellites(EPOCH_t *epoch)
{
	if (epoch->haveNumSv) {
		return epoch->numSv;
	}
	return 0;
}

/**
 * @brief Convert time from epoch to UTC time
 *
 * @param epoch
 * @return time_t
 */
static time_t gnss_get_utc_time(EPOCH_t *epoch)
{
	struct tm t = {
		// Year - 1900
		.tm_year = epoch->year - 1900,
		// Month, where 0 = jan. libublox starts indexing by 1
		.tm_mon = epoch->month - 1,
		// Day of the month
		.tm_mday = epoch->day,
		.tm_hour = epoch->hour,
		.tm_min = epoch->minute,
		.tm_sec = (int) round(epoch->second),
		// Is DST on? 1 = yes, 0 = no, -1 = unknown
		.tm_isdst = -1
	};
	time_t time = mktime(&t);
	/* Temporary solution to get UTC time as mktime converts
	* considering time provided is a local time
	*/
# ifdef	__USE_MISC
	time =  time + localtime(&time)->tm_gmtoff;
# else
	time =  time + localtime(&time)->__tm_gmtoff;
# endif
	return time;
}

/**
 * @brief Parse UBX-NAV-TIMELS msg to get leap second data
 *
 * @param session gps device data of the session
 * @param msg msg received from the receiver
 */
static void gnss_parse_ubx_nav_timels(struct gps_device_t *session, PARSER_MSG_t *msg)
{
	UBX_NAV_TIMELS_V0_GROUP0_t nav_timels_msg;
	if (msg->size == UBX_NAV_TIMELS_V0_SIZE) {
		memcpy(&nav_timels_msg, &msg->data[UBX_HEAD_SIZE], sizeof(nav_timels_msg));

		session->context->leap_seconds =
			FLAG(nav_timels_msg.valid, UBX_NAV_TIMELS_V0_VALID_CURRLSVALID) ?
			nav_timels_msg.currLs :
			0;
		session->context->lsset = FLAG(nav_timels_msg.valid, UBX_NAV_TIMELS_V0_VALID_CURRLSVALID);

		if (FLAG(nav_timels_msg.valid, UBX_NAV_TIMELS_V0_VALID_TIMETOLSEVENTVALID))
		{
			session->context->timeToLsEvent = nav_timels_msg.timeToLsEvent;
			session->context->lsChange = nav_timels_msg.lsChange;

			if ((0 != session->context->lsChange) &&
				(0 < session->context->timeToLsEvent) &&
				((60 * 60 * 23) > session->context->timeToLsEvent)) {
				if (1 == session->context->lsChange) {
					session->context->leap_notify = LEAP_ADDSECOND;
				} else if (-1 == session->context->lsChange) {
					session->context->leap_notify = LEAP_DELSECOND;
				}
			} else {
				session->context->leap_notify = LEAP_NOWARNING;
			}
			return;
		}
	}
	session->context->timeToLsEvent = 0;
	session->context->lsChange = 0;
	session->context->leap_notify = LEAP_NOWARNING;
};

/**
 * @brief Parse UBX-TIM-TP msg to get time from a constellation or UTC time and compute TAR
 *
 * @param session gps device data of the session
 * @param msg msg received from the receiver
 */
static void gnss_parse_ubx_tim_tp(struct gps_device_t *session, PARSER_MSG_t *msg) {
	if (msg->size == (int) UBX_TIM_TP_V0_SIZE) {
		UBX_TIME_TP_V0_GROUP0_t gr0;
		memcpy(&gr0, &msg->data[UBX_HEAD_SIZE], sizeof(gr0));
		log_trace("UBX-TIM-TP: towMS %lu, towSubMs %lu, qErr %ld, week %ld, flags %x; refInfo %x",
			gr0.towMs,
			gr0.towSubMS,
			gr0.qErr,
			gr0.week,
			gr0.flags,
			gr0.refInfo
		);

		int offset = 0;
		if (UBX_TIM_TP_V0_FLAGS_TIMEBASE_GET(gr0.flags) == UBX_TIM_TP_V0_FLAGS_TIMEBASE_GNSS) {
			switch(UBX_TIM_TP_V0_REFINFO_GET(gr0.refInfo)) {
				case UBX_TIM_TP_V0_REFINFO_GPS:
					offset = GPS_EPOCH_TO_TAI; 
					break;
				case UBX_TIM_TP_V0_REFINFO_BDS:
					offset = BDS_EPOCH_TO_TAI;
					break;
				case UBX_TIM_TP_V0_REFINFO_GAL:
					offset = GAL_EPOCH_TO_TAI;
					break;
				case UBX_TIM_TP_V0_REFINFO_GLO:
					if (session->context->lsset) {
						offset = GLO_EPOCH_TO_TAI + session->context->leap_seconds;
					} else {
						log_warn("Cannot compute TAI time from GLONASS without leap second information. Waiting for leap second data");
						return;
					}
					break;
				default:
					log_error("Unhandled Constellations %d", UBX_TIM_TP_V0_REFINFO_GET(gr0.refInfo));
					return;
			}
		} else if (UBX_TIM_TP_V0_FLAGS_TIMEBASE_GET(gr0.flags) == UBX_TIM_TP_V0_FLAGS_TIMEBASE_UTC) {
			if (session->context->lsset) {
				offset = GPS_EPOCH_TO_TAI + session->context->leap_seconds;
			} else {
				log_warn("Cannot compute TAI time from UTC without leap second information. Waiting for leap second data");
				return;
			}
		}

		session->tai_time = (int) round(
			((double) gr0.towMs / 1000)
			+ ((double) gr0.week * SEC_IN_WEEK)
			+ offset
			- 1 // UBX-TIM-TP gives time at next pulse
		);
		/* Update quantization error and store quantization of last epoch */
		session->context->qErr_last_epoch = session->context->qErr;
		session->context->qErr = gr0.qErr;
		session->tai_time_set = true;
		return;
	}
}

/**
 * @brief Parse UBX-TIM-SVIN msg to get information about Survey In process
 *
 * @param session gps device data of the session
 * @param msg msg received from the receiver
 * @return enum survey_in_state
 */
static enum SurveyInState gnss_parse_ubx_tim_svin(struct gps_device_t *session, PARSER_MSG_t *msg) {
	if (msg->size == (int) UBX_TIM_SVIN_V0_SIZE) {
		UBX_TIME_SVIN_V0_GROUP0_t gr0;
		memcpy(&gr0, &msg->data[UBX_HEAD_SIZE], sizeof(gr0));
		log_debug("UBX-TIM-SVIN: dur: %lu, meanX %ld, meanZ %ld, meanZ %ld, meanV %lu, obs %lu, valid %d, active %d",
			gr0.dur,
			gr0.meanX,
			gr0.meanY,
			gr0.meanZ,
			gr0.meanV,
			gr0.obs,
			gr0.valid,
			gr0.active
		);
		session->survey_in_position_error = sqrt(gr0.meanV)/1000;
		if (!gr0.active && gr0.dur > SVIN_MIN_DUR)
			return gr0.valid ? SURVEY_IN_COMPLETED : SURVEY_IN_KO;
		else if (gr0.dur < SVIN_MAX_DUR)
			return SURVEY_IN_IN_PROGRESS;
		else
			return SURVEY_IN_KO;
	}
	return SURVEY_IN_UNKNOWN;
}

/**
 * @brief Parse UBX-MON-RF msg to get antenna status data
 *
 * @param session gps device data of the session
 * @param msg msg received from the receiver
 */
static void gnss_get_antenna_data(struct gps_device_t *session, PARSER_MSG_t *msg)
{
	if (msg->size > (UBX_FRAME_SIZE + 4)) {
		if (msg->size >= (int) UBX_MON_RF_V0_MIN_SIZE) {
			int offs = UBX_HEAD_SIZE;
			UBX_MON_RF_V0_GROUP0_t gr0;
			memcpy(&gr0, &msg->data[offs], sizeof(gr0));
			offs += sizeof(gr0);

			// Reset antenna status and power
			session->antenna_status = 0x5; // Undefined value according to spec
			session->antenna_power = 0x5; // Undefined value according to spec

			UBX_MON_RF_V0_GROUP1_t gr1;
			while (offs <= (msg->size - 2 - (int)sizeof(gr1)))
			{
				memcpy(&gr1, &msg->data[offs], sizeof(gr1));
				// If we have multiple blocks and one is 0X2 (eq OK) and the next is not,
				// Take worst of values
				if (session->antenna_status == 0x5 || (session->antenna_status == 0x2 && gr1.antStatus != 0x2))
					session->antenna_status = gr1.antStatus;
				// Same behaviour but 0x2 means DONT_KNOW
				if (session->antenna_power == 0x5 || (session->antenna_power == 0x2 && gr1.antPower != 0x2))
					session->antenna_power = gr1.antPower;
				offs += sizeof(gr1);
			}
		}
	}
}

static void log_gnss_data(struct gps_device_t *session)
{
	log_debug("GNSS data: Fix %s (%d), Fix ok: %s, satellites num %d, survey in error: %0.2f, antenna status: %d, valid %d,"
		" time %lld, leapm_seconds %d, leap_notify %d, lsChange %d, "
		"timeToLsChange %d, lsSet: %s, QErr(n) %d, qErr(n-1) %d",
		fix_log[session->fix],
		session->fix,
		session->fixOk ? "True" : "False",
		session->satellites_count,
		session->survey_in_position_error,
		session->antenna_status,
		session->valid,
		session->last_fix_utc_time.tv_sec,
		session->context->leap_seconds,
		session->context->leap_notify,
		session->context->lsChange,
		session->context->timeToLsEvent,
		session->context->lsset ? "True" : "False",
		session->context->qErr,
		session->context->qErr_last_epoch
	);
}

/**
 * @brief Latch the fact that we've saved a fix and add in the device fudge
 *
 * @param device
 * @param td
 * Copied from GPSD
 */
static void ntp_latch(struct gps_device_t *device, struct timedelta_t *td)
{

    /* this should be an invariant of the way this function is called */
    if (0 >= device->last_fix_utc_time.tv_sec) {
        return;
    }

    (void)clock_gettime(CLOCK_REALTIME, &td->clock);
    /* structure copy of time from GPS */
    td->real = device->last_fix_utc_time;

    /* thread-safe update */
    pps_thread_fixin(&device->pps_thread, td);
}

static bool gnss_set_time_scale(RX_t *rx, uint8_t time_scale) {
	const UBLOXCFG_KEYVAL_t tp_timegrid_tp1 = UBLOXCFG_KEYVAL_ANY(CFG_TP_TIMEGRID_TP1, time_scale);
	return rxSetConfig(rx, &tp_timegrid_tp1, 1, true, false, false);
}

/**
 * @brief Connect to serial device of the GNSS Receiver.
 * Try to connect a maximum of GNSS_CONNECT_MAX_TRY time
 *
 * @param rx pointer to serial communication handler
 * @return boolean indicating connection has been done or not
 */
static bool gnss_connect(RX_t *rx) {
	int tries = 0;
	while (tries < GNSS_CONNECT_MAX_TRY) {
		if (rxOpen(rx))
			return true;
		else
			usleep(5000);
		tries++;
	}
	return false;
}

/**
 * @brief Send default configuration from f9_defvalsets.h to GNSS receiver
 *
 * @param rx pointer to serial communication handler
 * @return boolean indicating receiver has correctly been reset to default configuration
 */
static bool gnss_set_default_configuration(RX_t *rx) {
	bool receiver_configured = false;
	int tries = 0;

	// Get default configuration
	int nAllKvCfg;
	UBLOXCFG_KEYVAL_t *allKvCfg = get_default_value_from_config(&nAllKvCfg);

	/* Check if receiver is already configured */
	receiver_configured = check_gnss_config_in_flash(rx, allKvCfg, nAllKvCfg);
	if (receiver_configured)
		log_info("Receiver already configured to default configuration");
	else
		log_info("Receiver not configured to default configuration, starting reconfiguration");

	while (!receiver_configured) {
		log_info("Configuring receiver with ART parameters...\n");
		bool res = rxSetConfig(rx, allKvCfg, nAllKvCfg, true, true, true);

		if (res) {
			log_info("Successfully reconfigured GNSS receiver");
			log_debug("Performing hardware reset");
			if (!rxReset(rx, RX_RESET_HARD)) {
				free(allKvCfg);
				return false;
			}
			log_info("hardware reset performed");
			receiver_configured = true;
		}

		if (tries < GNSS_RECONFIGURE_MAX_TRY)
			tries++;
		else {
			log_error("Could not reconfigure GNSS receiver from default config\n");
			free(allKvCfg);
			return false;
		}
	}
	free(allKvCfg);
	return true;
}

/**
 * @brief Create gnss struct handler for thread
 *
 * @param config config structure of the program
 * @param session device session structure
 * @param fd_clock file pointer to PHC
 * @return struct gnss*
 */
struct gnss * gnss_init(const struct config *config, struct gps_device_t *session, int fd_clock)
{
	struct gnss *gnss;
	const char * preferred_constellation;
	int ret;
	bool do_reconfiguration;
	bool config_set = false;

	RX_ARGS_t args = RX_ARGS_DEFAULT();
	args.autobaud = true;
	args.detect = true;

	const char *gnss_device_tty = config_get(config, "gnss-device-tty");
	if (gnss_device_tty == NULL) {
		log_error("device-tty not defined in config %s", config->path);
		return NULL;
	}

	if (strchr(gnss_device_tty, '@')) {
		args.autobaud = false;
	}

	if (session == NULL) {
		log_error("No gps session provided");
		return NULL;
	}

	gnss = (struct gnss *) malloc(sizeof(struct gnss));
	if (gnss == NULL) {
		log_error("could not allocate memory for gnss");
		return NULL;
	}

	gnss->fd_clock = fd_clock;
	gnss->session = session;
	/* Init Antenna Status and Power to undefined values according to UBX Protocol */
	gnss->session->antenna_status = ANT_STATUS_UNDEFINED;
	gnss->session->antenna_power = ANT_POWER_UNDEFINED;
	gnss->rx = rxInit(gnss_device_tty, &args);
	gnss->action = GNSS_ACTION_NONE;
	/* Init Survey In Error to undefined values */
	gnss->session->survey_in_position_error =-1.0;
	if (gnss->rx == NULL)
		goto err_rxInit;

	if (!gnss_connect(gnss->rx))
		goto err_gnss_connect;

	/* Check if GNSS receiver should reset to default configuraiton */
	do_reconfiguration = config_get_bool_default(
		config,
		"gnss-receiver-reconfigure",
		false);
	if (do_reconfiguration && !gnss_set_default_configuration(gnss->rx))
		goto err_gnss_connect;

	gnss->stop = false;

	/** Set preferred time scale */
	preferred_constellation = config_get(config, "gnss-preferred-time-scale");
	if (preferred_constellation == NULL) {
		log_info("No preferred timescale, assuming GNSS receiver is correctly set");
	} else {
		if (strlen(preferred_constellation) != 3)
			log_warn("Unknown preferred time scale, assuming GNSS receiver is correctly set");
		else if (strncmp(preferred_constellation, "GPS", 3) == 0)
			config_set = gnss_set_time_scale(gnss->rx, UBLOXCFG_CFG_TP_TIMEGRID_TP1_GPS);
		else if (strncmp(preferred_constellation, "GAL", 3) == 0)
			config_set = gnss_set_time_scale(gnss->rx, UBLOXCFG_CFG_TP_TIMEGRID_TP1_GAL);
		else if (strncmp(preferred_constellation, "GLO", 3) == 0)
			config_set = gnss_set_time_scale(gnss->rx, UBLOXCFG_CFG_TP_TIMEGRID_TP1_GLO);
		else if (strncmp(preferred_constellation, "BDS", 3) == 0)
			config_set = gnss_set_time_scale(gnss->rx, UBLOXCFG_CFG_TP_TIMEGRID_TP1_BDS);
		else if (strncmp(preferred_constellation, "UTC", 3) == 0)
			config_set = gnss_set_time_scale(gnss->rx, UBLOXCFG_CFG_TP_TIMEGRID_TP1_UTC);

		if (config_set) {
			log_info("Preferred time scale set to %s", preferred_constellation);
		} else {
			log_warn("Preferred time scale has not been set, assuming GNSS receiver is correctly set");
		}
	}

	/* Initialize receiver's survey in flag */
	gnss->session->survey_completed = false;

	/* Check wether receiver's survey in should be bypassed or not */
	gnss->session->bypass_survey = config_get_bool_default(
		config,
		"gnss-bypass-survey",
		false);
	if (gnss->session->bypass_survey) {
		log_warn("GNSS Survey In will be bypassed, true timing performance might not be reached");
		log_warn("Please note that performance may be degraded and holdover might not reached specified limits");
	}

	if (!rxReset(gnss->rx, RX_RESET_GNSS_START)) {
		log_error("Could not start GNSS receiver");
		goto err_gnss_connect;
	}

	pthread_mutex_init(&gnss->mutex_data, NULL);
	pthread_cond_init(&gnss->cond_time, NULL);
	pthread_cond_init(&gnss->cond_data, NULL);

	ret = pthread_create(
		&gnss->thread,
		NULL,
		gnss_thread,
		gnss
	);

	if (ret != 0) {
		rxClose(gnss->rx);
		goto err_gnss_connect;
	}

	return gnss;

err_gnss_connect:
	free(gnss->rx);
	log_error("Could not connect to GNSS serial at %s", gnss_device_tty);
err_rxInit:
	free(gnss);
	error(EXIT_FAILURE, -ret, "gnss_init");
	return NULL;
}

/**
 * @brief Wait for next TAI time retrieved from the device
 *
 * @param gnss thread structure
 * @return time_t
 */
static time_t gnss_get_next_fix_tai_time(struct gnss * gnss)
{
	time_t time;
	pthread_mutex_lock(&gnss->mutex_data);
	pthread_cond_wait(&gnss->cond_time, &gnss->mutex_data);
	time = gnss->session->tai_time;
	pthread_mutex_unlock(&gnss->mutex_data);
	return time;

}

/**
 * @brief Get GNSS data from epoch
 *
 * @param gnss
 * @param valid Output Flags indicating GNSS data are valid (Fix >= 2D + FixOk)
 * @param qErr Output Quantization error from last Epoch
 */
int gnss_get_epoch_data(struct gnss *gnss, bool *valid, bool *survey, int32_t *qErr)
{
	if (!gnss) {
		return -1;
	}

	pthread_mutex_lock(&gnss->mutex_data);
	pthread_cond_wait(&gnss->cond_data, &gnss->mutex_data);
	if (survey != NULL)
		*survey = gnss->session->survey_completed;
	if (valid != NULL)
		*valid = gnss->session->valid;
	if (qErr != NULL)
		*qErr = gnss->session->context->qErr_last_epoch;
	pthread_mutex_unlock(&gnss->mutex_data);
	return 0;
}

/**
 * @brief Check that time set in PHC is the same as the one coming from the GNSS receiver
 *
 * @param gnss
 * @return true
 * @return false
 */
static bool gnss_check_ptp_clock_time(struct gnss *gnss)
{
	struct timespec ts;
	bool valid = false;
	time_t gnss_time;
	int ret;
	if (gnss->fd_clock < 0) {
		log_warn("Bad clock file descriptor");
		return -1;
	}
	if (gnss_get_epoch_data(gnss, &valid, NULL, NULL))
		return -1;
	if (valid) {
		gnss_time = gnss_get_next_fix_tai_time(gnss);
		ret = clock_gettime(FD_TO_CLOCKID(gnss->fd_clock), &ts);
		if (ret == 0) {
			log_debug("GNSS tai time is %ld", gnss_time);
			log_debug("Time set on PHC is %ld", ts.tv_sec);
			if (ts.tv_sec == gnss_time) {
				log_info("PHC time is set to GNSS one");
				return true;
			} else {
				log_error("GNSS time is not the same as PTP clock time");
			}
		} else
			log_error("Could get not PHC time");
	} else {
		log_error("GNSS get valid is false");
	}
	return false;

}

/**
 * @brief Set PHC time to GNSS receiver time
 *
 * @param gnss
 * @return int: 0 on success, -1 on error
 */
int gnss_set_ptp_clock_time(struct gnss *gnss)
{
	clockid_t clkid;
	struct timespec ts;
	time_t gnss_time;
	int ret;
	bool valid = false;
	bool clock_set = false;
	bool clock_valid = false;

	if (!gnss) {
		return -1;
	}

	if (gnss->fd_clock < 0) {
		log_warn("Bad clock file descriptor");
		return -1;
	}
	clkid = FD_TO_CLOCKID(gnss->fd_clock);

	while(!clock_valid && loop) {
		if (gnss_get_epoch_data(gnss, &valid, NULL, NULL))
			return -1;

		if (valid) {
			/* Set clock time according to gnss data */
			if (!clock_set) {
				/* Configure PHC time */
				/* Wait to get next gnss TAI time */
				gnss_time = gnss_get_next_fix_tai_time(gnss);
				/* Then get clock time to preserve nanoseconds */
				ret = clock_gettime(clkid, &ts);
				if (ret == 0) {
					if (ts.tv_sec == gnss_time) {
						log_info("PTP Clock time already set");
						clock_set = true;
					} else {
						ts.tv_sec = gnss_time;
						ret = clock_settime(clkid, &ts);
						if (ret == 0) {
							clock_set = true;
							log_debug("PTP Clock Set");
							sleep(4);
						}
					}
				} else {
					log_warn("Could not get PTP clock time");
					return -1;
				}
			/* PHC time has been set, check time is correctly set */
			} else {
				if (gnss_check_ptp_clock_time(gnss)) {
					log_debug("PHC time correctly set");
					clock_valid = true;
				} else {
					log_warn("PHC time is not valid, resetting it");
					clock_set = false;
				}
			}
		} else {
			sleep(2);
		}
	}
	return 0;
}

/**
 * @brief Thread routine
 *
 * @param p_data
 * @return void*
 */
static void * gnss_thread(void * p_data)
{
	EPOCH_t coll;
	EPOCH_t epoch;
	struct gnss *gnss = (struct gnss*) p_data;
	struct gps_device_t * session;
	enum gnss_action action = GNSS_ACTION_NONE;
	bool stop;

	epochInit(&coll);

	pthread_mutex_lock(&gnss->mutex_data);
	stop = gnss->stop;
	pthread_mutex_unlock(&gnss->mutex_data);

	while (!stop)
	{
		PARSER_MSG_t *msg = rxGetNextMessageTimeout(gnss->rx, GNSS_TIMEOUT_MS);
		if (msg != NULL)
		{
			pthread_mutex_lock(&gnss->mutex_data);
			session = gnss->session;
			// Epoch collect is used to fetch navigation data such as time and leap seconds
			if(epochCollect(&coll, msg, &epoch))
			{
				// if epoch has no fix there will be no Nav solution and 0 satellites
				session->satellites_count = gnss_get_satellites(&epoch);
				if (epoch.haveFix) {
					session->last_fix_utc_time.tv_sec = gnss_get_utc_time(&epoch);
					session->fix = epoch.fix;
					session->fixOk = epoch.fixOk && session->satellites_count > NUM_SAT_MIN;
					session->valid = session->fix >= EPOCH_FIX_TIME && session->fixOk;
					if (!session->valid) {
						if (session->fix < EPOCH_FIX_TIME)
							log_trace("Fix is to low: %d", session->fix);
						if (!session->fixOk)
							log_trace("Fix is not OK");
					}
					struct timedelta_t td;
					ntp_latch(session, &td);
					log_gnss_data(session);
				} else {
					session->fix = MODE_NO_FIX;
					session->fixOk = false;
				}
				pthread_cond_signal(&gnss->cond_data);

				if (session->tai_time_set)
					pthread_cond_signal(&gnss->cond_time);
				else
					log_warn("Could not tai time from gnss, please check GNSS Configuration if this message keeps appearing more than 25 minutes");

			} else {
				// Analyze msg to parse UBX-MON-RF to get antenna status
				uint8_t clsId = UBX_CLSID(msg->data);
				uint8_t msgId = UBX_MSGID(msg->data);
				if (clsId == UBX_MON_CLSID && msgId == UBX_MON_RF_MSGID) {
					gnss_get_antenna_data(session, msg);
					log_trace("GNSS: Antenna status: 0x%x", session->antenna_status);
				// Parse UBX-NAV-TIMELS messages there because library does not do it
				} else if (clsId == UBX_NAV_CLSID && msgId == UBX_NAV_TIMELS_MSGID)
					gnss_parse_ubx_nav_timels(session, msg);
				else if (clsId == UBX_TIM_CLSID && msgId == UBX_TIM_TP_MSGID)
					gnss_parse_ubx_tim_tp(session, msg);
				else if (clsId == UBX_TIM_CLSID && msgId == UBX_TIM_SVIN_MSGID && !session->survey_completed && !gnss->session->bypass_survey) {
					switch (gnss_parse_ubx_tim_svin(session, msg)) {
					case SURVEY_IN_COMPLETED:
						session->survey_completed = true;
						break;
					case SURVEY_IN_IN_PROGRESS:
					case SURVEY_IN_UNKNOWN:
						break;
					case SURVEY_IN_KO:
					default:
						log_error("Survey In did not complete in time. GNSS conditions are not stable enough for optimal timing performance");
						log_error("Please check your antenna setup (antenna on roof is way more precise) to pass survey in.");
						break;
					}

				}
			}
			pthread_mutex_unlock(&gnss->mutex_data);
		} else {
			log_warn("UART GNSS Timeout !");
			pthread_mutex_lock(&gnss->mutex_data);
			gnss->session->valid = false;
			pthread_cond_signal(&gnss->cond_data);
			pthread_mutex_unlock(&gnss->mutex_data);
			usleep(5 * 1000);
		}
		pthread_mutex_lock(&gnss->mutex_data);
		stop = gnss->stop;
		action = gnss->action;
		gnss->action = GNSS_ACTION_NONE;
		pthread_mutex_unlock(&gnss->mutex_data);

		if (action == GNSS_ACTION_START) {
			log_debug("Performing GNSS START");
			if (!rxReset(gnss->rx, RX_RESET_GNSS_START))
				log_error("Could not start GNSS Receiver");
			else
				log_info("GNSS START performed");
		} else if (action == GNSS_ACTION_STOP) {
			log_debug("Performing GNSS STOP");
			if (!rxReset(gnss->rx, RX_RESET_GNSS_STOP))
				log_error("Could not stop GNSS Receiver");
			else
				log_info("GNSS STOP performed");
		}
	}

	log_debug("Closing gnss session");
	rxClose(gnss->rx);
	free(gnss->rx);
	gnss->rx = NULL;
	free(gnss);
	gnss = NULL;
	return NULL;
}

/**
 * @brief Stop gnss thread
 *
 * @param gnss
 */
void gnss_stop(struct gnss *gnss)
{
	if (!gnss)
		return;

	pthread_mutex_lock(&gnss->mutex_data);
	gnss->stop = true;
	pthread_mutex_unlock(&gnss->mutex_data);

	pthread_join(gnss->thread, NULL);
	return;
}

void gnss_set_action(struct gnss *gnss, enum gnss_action action)
{
	if (!gnss)
		return;

	if (action != GNSS_ACTION_START && action != GNSS_ACTION_STOP) {
		log_error("Unknown action %d", action);
		return;
	}

	pthread_mutex_lock(&gnss->mutex_data);
	gnss->action = action;
	pthread_mutex_unlock(&gnss->mutex_data);
	return;
}
