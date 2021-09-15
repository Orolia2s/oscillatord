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
#include "log.h"
#include "f9_defvalsets.h"
#include "utils.h"

#define GNSS_TIMEOUT_MS 1500
#define GNSS_RECONFIGURE_MAX_TRY 5

#define ARRAY_SIZE(_A) (sizeof(_A) / sizeof((_A)[0]))

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

static int gnss_get_fix(int fix)
{
	switch(fix) {
		case EPOCH_FIX_NOFIX: return MODE_NO_FIX;
		case EPOCH_FIX_DRONLY: return MODE_NO_FIX;
		case EPOCH_FIX_S2D: return MODE_2D;
		case EPOCH_FIX_S3D: return MODE_3D;
		case EPOCH_FIX_S3D_DR: return MODE_3D;
		case EPOCH_FIX_TIME: return MODE_NO_FIX;
	}
	return -1;
}

static time_t gnss_get_time(EPOCH_t *epoch)
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


static int gnss_get_leap_seconds(EPOCH_t *epoch)
{
	if (epoch->haveLeapSeconds) {
		return epoch->leapSeconds;
	}
	return 0;
}


static int gnss_get_leap_notify(EPOCH_t *epoch)
{
	if (epoch->haveLeapSecondEvent) {
		if ((0 != epoch->lsChange) && (0 < epoch->timeToLsEvent) &&
			((60 * 60 * 23) > epoch->timeToLsEvent)) {
			if (1 == epoch->lsChange) {
				return LEAP_ADDSECOND;
			} else if (-1 == epoch->lsChange) {
				return LEAP_DELSECOND;
			}
		} else {
			return LEAP_NOWARNING;
		}
	}
	return LEAP_NOWARNING;
}

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
	log_debug("GNSS data: Fix %s (%d), Fix ok: %s, antenna status: %d, valid %d,"
		" time %lld, leapm_seconds %d, leap_notify %d, lsChange %d "
		"timeToLsChange %d",
		fix_log[session->fix],
		session->fix,
		session->fixOk ? "True" : "False",
		session->antenna_status,
		session->valid,
		session->last_fixtime.tv_sec,
		session->context->leap_seconds,
		session->context->leap_notify,
		session->context->lsChange,
		session->context->timeToLsEvent
	);
}

// Copied from GPSD
/* Latch the fact that we've saved a fix.
 * And add in the device fudge */
static void ntp_latch(struct gps_device_t *device, struct timedelta_t *td)
{

    /* this should be an invariant of the way this function is called */
    if (0 >= device->last_fixtime.tv_sec) {
        return;
    }

    (void)clock_gettime(CLOCK_REALTIME, &td->clock);
    /* structure copy of time from GPS */
    td->real = device->last_fixtime;

    /* thread-safe update */
    pps_thread_fixin(&device->pps_thread, td);
}


struct gnss * gnss_init(const struct config *config, struct gps_device_t *session, int fd_clock)
{
	struct gnss *gnss;
	int ret;
	size_t i;
	bool do_reconfiguration;

	RX_ARGS_t args = RX_ARGS_DEFAULT();
	args.autobaud = true;
	args.detect = true;

	const char *gnss_device_tty = config_get(config, "gnss-device-tty");
	if (gnss_device_tty == NULL) {
		log_error("device-tty not defined in config %s", config->path);
		return NULL;
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
	// Init Antenna Status and Power to undefined values according to UBX Protocol
	gnss->session->antenna_status = ANT_STATUS_UNDEFINED;
	gnss->session->antenna_power = ANT_POWER_UNDEFINED;
	gnss->rx = rxInit(gnss_device_tty, &args);
	if (gnss->rx == NULL || !rxOpen(gnss->rx)) {
		free(gnss->rx);
		printf("rx init failed\n");
		free(gnss);
		return NULL;
	}

	do_reconfiguration = config_get_bool_default(config,
					"gnss-receiver-reconfigure",
					false);
	if (do_reconfiguration) {
		bool receiver_reconfigured = false;
		int tries = 0;
		log_info("configuring receiver with ART parameters\n");
		while (!receiver_reconfigured) {
			for (i = 0; i < ARRAY_SIZE(ubxCfgValsetMsgs); i++)
			{
				ret = rxSendUbxCfg(gnss->rx, ubxCfgValsetMsgs[i].data,
								ubxCfgValsetMsgs[i].size, 2500);
				if (!ret) {
				} else if (i == ARRAY_SIZE(ubxCfgValsetMsgs) - 1) {
					log_info("Successfully reconfigured gnss receiver");
					receiver_reconfigured = true;
				}
			}

			if (tries < GNSS_RECONFIGURE_MAX_TRY) {
				tries++;
			} else {
				log_error("Could not reconfigure GNSS receiver from default config\n");
				free(gnss->rx);
				free(gnss);
				return NULL;
			}
		}
	}
	gnss->stop = false;

	pthread_mutex_init(&gnss->mutex_data, NULL);
	pthread_cond_init(&gnss->cond_time, NULL);

	ret = pthread_create(
		&gnss->thread,
		NULL,
		gnss_thread,
		gnss
	);

	if (ret != 0) {
		rxClose(gnss->rx);
		free(gnss->rx);
		free(gnss);
		error(EXIT_FAILURE, -ret, "gnss_init");
		return NULL;
	}

	return gnss;
}

time_t gnss_get_next_fix_time(struct gnss * gnss)
{
	time_t time;
	pthread_mutex_lock(&gnss->mutex_data);
	pthread_cond_wait(&gnss->cond_time, &gnss->mutex_data);
	time = gnss->session->last_fixtime.tv_sec;
	pthread_mutex_unlock(&gnss->mutex_data);
	return time;

}

bool gnss_get_valid(struct gnss *gnss)
{
	bool valid;
	pthread_mutex_lock(&gnss->mutex_data);
	valid = gnss->session->valid;
	pthread_mutex_unlock(&gnss->mutex_data);
	return valid;
}

static bool gnss_check_ptp_clock_time(struct gnss *gnss)
{
	struct timespec ts;
	time_t gnss_time;
	int ret;
	if (gnss->fd_clock < 0) {
		log_warn("Bad clock file descriptor");
		return -1;
	}
	if (gnss_get_valid(gnss)) {
		gnss_time = gnss_get_next_fix_time(gnss);
		ret = clock_gettime(FD_TO_CLOCKID(gnss->fd_clock), &ts);
		if (ret == 0) {
			log_debug("GNSS TIME %ld", gnss_time);
			log_debug("PHC TIME  %ld", ts.tv_sec);
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

int gnss_set_ptp_clock_time(struct gnss *gnss)
{
	clockid_t clkid;
	struct timespec ts;
	time_t gnss_time;
	int ret;
	bool clock_set = false;
	bool clock_valid = false;
		
	if (gnss->fd_clock < 0) {
		log_warn("Bad clock file descriptor");
		return -1;
	}
	clkid = FD_TO_CLOCKID(gnss->fd_clock);

	while(!clock_valid) {
		if (gnss_get_valid(gnss)) {
			/* Set clock time according to gnss data */
			if (!clock_set) {
				/* Configure PHC time */
				/* Wait to get next gnss time */
				gnss_time = gnss_get_next_fix_time(gnss);
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
					log_error("PHC time correctly set");
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

static int gnss_adjust_ptp_clock_time(struct gnss *gnss, int64_t ns)
{
	int ret = 0;
	clockid_t clkid;

	if (gnss->fd_clock < 0) {
		log_warn("Bad clock file descriptor");
		return -1;
	}
	clkid = FD_TO_CLOCKID(gnss->fd_clock);

	struct timex timex = {
		.modes = ADJ_SETOFFSET | ADJ_NANO,
		.offset = 0,
		.time.tv_sec = ns > 0  || (ns % NS_IN_SECOND == 0.0) ?
			(long long) floor(ns / NS_IN_SECOND):
			(long long) floor(ns / NS_IN_SECOND) - 1,
		.time.tv_usec = ns > 0 || (ns % NS_IN_SECOND == 0.0) ?
			ns % NS_IN_SECOND:
			ns % NS_IN_SECOND + NS_IN_SECOND,
	};

	log_info("Adusjting ptp_clock time: adjustment: %"PRIi32"ns", ns);
	log_debug("adjust tv_sec %lld tv_usec %lld", timex.time.tv_sec, timex.time.tv_usec);
	ret = clock_adjtime(clkid, &timex);
	log_debug("ret is %d", ret);
	return ret;

}

static void * gnss_thread(void * p_data)
{
	EPOCH_t coll;
	EPOCH_t epoch;
	struct gnss *gnss = (struct gnss*) p_data;
	struct gps_device_t * session;
	bool stop;
	int ret;

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
				if(epoch.haveFix) {
					session->last_fixtime.tv_sec = gnss_get_time(&epoch);
					session->fix = epoch.fix;
					session->fixOk = epoch.fixOk;
					session->context->leap_seconds = gnss_get_leap_seconds(&epoch);
					session->context->leap_notify = gnss_get_leap_notify(&epoch);
					session->context->timeToLsEvent = epoch.timeToLsEvent;
					session->context->lsChange = epoch.lsChange;
					if (session->fix > MODE_NO_FIX)
						session->fixcnt++;
					else
						session->fixcnt = 0;


					struct timedelta_t td;
					ntp_latch(session, &td);
					log_gnss_data(session);
					pthread_cond_signal(&gnss->cond_time);
					if (session->context->leap_notify && session->context->timeToLsEvent == 1) {
						/* Leap second will occur in one second */
						log_warn("Leap second event detected");
						ret = gnss_adjust_ptp_clock_time(gnss, - session->context->lsChange * NS_IN_SECOND);
						if (ret != 0)
							log_error("Error occured while adjusting ptp clock time due to leap second event");
						else
							log_info("Leap second event correctly handled");
					}
				}

			// Analyze msg to parse UBX-MON-RF to get antenna status
			} else {
				uint8_t clsId = UBX_CLSID(msg->data);
				uint8_t msgId = UBX_MSGID(msg->data);
				if (clsId == UBX_MON_CLSID && msgId == UBX_MON_RF_MSGID) {
					gnss_get_antenna_data(session, msg);
					session->valid = session->fix >= EPOCH_FIX_S2D && session->fixOk && (session->antenna_status == ANT_STATUS_OK || session->antenna_status == ANT_STATUS_SHORT || session->antenna_status == ANT_STATUS_OPEN);
				}
			}
			pthread_mutex_unlock(&gnss->mutex_data);
		} else {
			log_warn("UART GNSS Timeout !");
			pthread_mutex_lock(&gnss->mutex_data);
			gnss->session->valid = false;
			pthread_mutex_unlock(&gnss->mutex_data);
			usleep(5 * 1000);
		}
		pthread_mutex_lock(&gnss->mutex_data);
		stop = gnss->stop;
		pthread_mutex_unlock(&gnss->mutex_data);
	}

	log_debug("Closing gnss session");
	rxClose(gnss->rx);
	free(gnss->rx);
	return NULL;
}

void gnss_stop(struct gnss *gnss)
{
	pthread_mutex_lock(&gnss->mutex_data);
	gnss->stop = true;
	pthread_mutex_unlock(&gnss->mutex_data);

	pthread_join(gnss->thread, NULL);
	return;
}
