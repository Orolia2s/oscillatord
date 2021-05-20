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

#define TIMEOUT_USEC 1000


static bool gnss_data_valid(EPOCH_t *epoch)
{
	if (epoch->haveFix) {
		if (epoch->fix >= EPOCH_FIX_TIME)
			return true;
	}
	return false;
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
	if ((gnss->rx == NULL || !rxOpen(gnss->rx))) {
		free(gnss->rx);
		printf("rx init failed\n");
		return -1;
	}
	gnss->session_open = true;
	return 0;
}

enum gnss_state gnss_get_data(struct gnss *gnss)
{
	uint32_t nMsgs = 0;
	int timeout_step = 0;
	bool valid = false;

	EPOCH_t coll;
	EPOCH_t epoch;
	epochInit(&coll);

	while (timeout_step < 2000)
	{
		PARSER_MSG_t *msg = rxGetNextMessage(gnss->rx);
		if (msg != NULL)
		{
			if(epochCollect(&coll, msg, &epoch))
			{
				if(epoch.haveFix) {
					switch(epoch.fix) {
						case EPOCH_FIX_NOFIX: gnss->data.fix.mode = MODE_NO_FIX; break;
						case EPOCH_FIX_DRONLY: gnss->data.fix.mode = MODE_NO_FIX; break;
						case EPOCH_FIX_S2D: gnss->data.fix.mode = MODE_2D; break;
						case EPOCH_FIX_S3D: gnss->data.fix.mode = MODE_3D; break;
						case EPOCH_FIX_S3D_DR: gnss->data.fix.mode = MODE_3D; break;
						case EPOCH_FIX_TIME: gnss->data.fix.mode = MODE_NO_FIX; break;
					}
					break;
				}
				if(epoch.haveLeapSeconds) {
					printf("Got leap seconds from GNSS, LS is %d\n", epoch.leapSeconds);
				}
				if(epoch.haveLeapSecondEvent) {
					printf("Got LeapSecond event! LsChange is %d\n", epoch.lsChange);
				}
			}
		} else {
			usleep(5 * 1000);
			timeout_step++;
		}
	}
	printf("EPOCH data is:\n");
	printf("Fix: %d, Fix Ok %s, LepSecKnow %s, Hours %d, Minutes %d Seconds %f\n",
		epoch.fix,
		epoch.fixOk ? "TRUE" : "FALSE",
		epoch.leapSecKnown ? "TRUE" : "FALSE",
		epoch.hour,
		epoch.minute,
		epoch.second
	);
	struct tm t = {
		// Year - 1900
		.tm_year = epoch.year - 1900,
		// Month, where 0 = jan. libublox starts indexing by 1
		.tm_mon = epoch.month - 1,
		// Day of the month
		.tm_mday = epoch.day,
		.tm_hour = epoch.hour,
		.tm_min = epoch.minute,
		.tm_sec = epoch.second,
		// Is DST on? 1 = yes, 0 = no, -1 = unknown
		.tm_isdst = -1
	};

	gnss->time = mktime(&t);
	/* Temporary solution to get UTC time as mktime converts
	* considering time provided is a local time
	*/
# ifdef	__USE_MISC
	gnss->time =  gnss->time + localtime(&gnss->time)->tm_gmtoff;
# else
	gnss->time =  gnss->time + localtime(&gnss->time)->__tm_gmtoff;
# endif
	printf("TM time is %s", asctime(&t));
	printf("GNSS Time is now %lu\n", gnss->time);
	valid = gnss_data_valid(&epoch);
	return valid ? GNSS_VALID : GNSS_INVALID;
}

void gnss_cleanup(struct gnss *gnss)
{
	if (!gnss->session_open)
		return;

	debug("Closing gnss session\n");
	rxClose(gnss->rx);
	free(gnss->rx);
}
