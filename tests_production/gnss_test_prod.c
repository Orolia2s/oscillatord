#include <stdlib.h>
#include <unistd.h>
#include <ubloxcfg/ff_epoch.h>
#include <ubloxcfg/ff_rx.h>
#include <ubloxcfg/ff_ubx.h>

#include "gnss.h"
#include "gnss-config.h"
#include "gnss_serial_test.h"
#include "log.h"
#include "f9_defvalsets.h"

#define GNSS_TIMEOUT_MS 1500
#define GNSS_TEST_TIMEOUT 600
#define GNSS_RECONFIGURE_MAX_TRY 5
#define ARRAY_SIZE(_A) (sizeof(_A) / sizeof((_A)[0]))

static bool parse_receiver_version(char* textToCheck, int* major, int* minor)
{
	return textToCheck && sscanf(textToCheck, "%*s %i.%i", major, minor) == 2;
}

int main(int argc, char *argv[])
{
    char path[256] = "";
    bool gnss_path_valid;
    bool gnss_test_valid;

	/* Set log level */
	log_set_level(0);

    snprintf(path, sizeof(path), "%s", argv[1]);

	log_info("Checking input:");

    if (path == NULL) {
        log_error("\t- GNSS Path does not exists");
        gnss_path_valid = false;
    }

	log_info("\t-GNSS serial path is: \"%s\", checking...",file_path);
	if (access(file_path, F_OK) != -1) 
	{
		gnss_path_valid = true;
        log_info("\t\tGNSS serial path exists !");
    } 
	else 
	{
		gnss_path_valid = false;
        log_info("\t\tGNSS serial path doesn't exists !");
    }

    if (gnss_path_valid)
    {
        EPOCH_t coll;
        EPOCH_t epoch;

        epochInit(&coll);

        time_t timeout = time(NULL) + GNSS_TEST_TIMEOUT;
        while ((!got_gnss_fix || !got_mon_rf_message || !got_nav_timels_message || !got_tim_tp_message) && time(NULL) < timeout)
        {
            PARSER_MSG_t *msg = rxGetNextMessageTimeout(rx, GNSS_TIMEOUT_MS);
            if (msg != NULL)
            {
                if(epochCollect(&coll, msg, &epoch))
                {
                    if(epoch.haveFix && epoch.fixOk && (epoch.fix >= EPOCH_FIX_S3D || epoch.fix == EPOCH_FIX_TIME) && !got_gnss_fix) {
                        log_info("\t- Got fix !");
                        got_gnss_fix = true;
                    }
                } 
                else 
                {
                    uint8_t clsId = UBX_CLSID(msg->data);
                    uint8_t msgId = UBX_MSGID(msg->data);
                    if (clsId == UBX_MON_CLSID && msgId == UBX_MON_RF_MSGID && !got_mon_rf_message) {
                        log_info("\t- Got UBX-MON-RF message");
                        got_mon_rf_message = true;
                    } else if (clsId == UBX_NAV_CLSID && msgId == UBX_NAV_TIMELS_MSGID && !got_nav_timels_message) {
                        log_info("\t- Got UBX-NAV-TIMELS message");
                        got_nav_timels_message = true;
                    } else if (clsId == UBX_TIM_CLSID && msgId == UBX_TIM_TP_MSGID && !got_tim_tp_message) {
                        log_info("\t- Got UBX-TIM-TP message");
                        got_tim_tp_message = true;
                    }
                }
            }
            else 
            {
                log_warn("GNSS: UART Timeout !");
                usleep(5 * 1000);
            }
        }

        rxClose(rx);
        free(rx);

        if (got_gnss_fix && got_mon_rf_message && got_nav_timels_message && got_tim_tp_message
        ) {
            log_info("GNSS Test Passed");
            return true;
        }
        else
        {
            log_warn("GNSS Test Failed");
        }
    else
    {
        log_warn("GNSS config aborted");
    }

}
