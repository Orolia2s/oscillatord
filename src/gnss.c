#include <errno.h>
#include <unistd.h>
#include <time.h>

#include "log.h"
#include "config.h"
#include "gnss.h"

#include <ubloxcfg/ubloxcfg.h>
#include <ubloxcfg/ff_rx.h>
#include <ubloxcfg/ff_parser.h>
#include <ubloxcfg/ff_epoch.h>
#include <ubloxcfg/ff_stuff.h>

static void * gnss_thread(void * p_data);

static bool gnss_data_valid(EPOCH_t *epoch)
{
	if (epoch->haveFix) {
		if (epoch->fix >= EPOCH_FIX_TIME)
			return true;
	}
	return false;
}

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

time_t gnss_get_time(EPOCH_t *epoch)
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
		.tm_sec = epoch->second,
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

int gnss_init(const struct config *config, struct gnss *gnss)
{
	RX_ARGS_t args = RX_ARGS_DEFAULT();
	args.autobaud = false;

	gnss->session_open = false;

	const char *gnss_device_tty = config_get(config, "gnss-device-tty");
	if (gnss_device_tty == NULL) {
		err("device-tty not defined in config %s", config->path);
		return -1;
	}
	
	gnss->rx = rxInit(gnss_device_tty, &args);
	if (gnss->rx == NULL || !rxOpen(gnss->rx)) {
		free(gnss->rx);
		printf("rx init failed\n");
		return -1;
	}
	gnss->session_open = true;
	gnss->stop = false;

	int ret = pthread_create(
		&gnss->thread,
		NULL,
		gnss_thread,
		gnss
	);

	return 0;
}

struct gnss_data gnss_get_data(struct gnss *gnss)
{
	struct gnss_data data;
	pthread_mutex_lock(&gnss->mutex_data);
	data = gnss->data;
	pthread_mutex_unlock(&gnss->mutex_data);
	return data;
}

static void * gnss_thread(void * p_data)
{
	struct gnss *gnss = (struct gnss*) p_data;

	bool valid = false;
	bool stop;

	EPOCH_t coll;
	EPOCH_t epoch;
	epochInit(&coll);

	stop = gnss->stop;

	while (!stop)
	{
		PARSER_MSG_t *msg = rxGetNextMessage(gnss->rx);
		if (msg != NULL)
		{
			if(epochCollect(&coll, msg, &epoch))
			{
				pthread_mutex_lock(&gnss->mutex_data);
				if(epoch.haveFix) {
					gnss->data.fix = gnss_get_fix(epoch.fix);
					gnss->data.time = gnss_get_time(&epoch);
					gnss->data.valid = gnss_data_valid(&epoch);
				}
				pthread_mutex_unlock(&gnss->mutex_data);
			}
		} else {
			usleep(5 * 1000);
		}
		pthread_mutex_lock(&gnss->mutex_data);
		stop = gnss->stop;
		pthread_mutex_unlock(&gnss->mutex_data);
	}

	rxClose(gnss->rx);
	
	if (!gnss->session_open)
		return;

	info("Closing gnss session\n");
	free(gnss->rx);
	return NULL;
}
