#include <stdlib.h>
#include <unistd.h>
#include <ubloxcfg/ff_epoch.h>
#include <ubloxcfg/ff_rx.h>
#include <ubloxcfg/ff_ubx.h>

#include "f9_defvalsets.h"
#include "gnss_serial_test.h"
#include "log.h"

#define GNSS_TIMEOUT_MS 1500
#define GNSS_RECONFIGURE_MAX_TRY 5
#define ARRAY_SIZE(_A) (sizeof(_A) / sizeof((_A)[0]))

bool test_gnss_serial(char * path)
{
    bool got_gnss_fix = false;
    bool got_mon_rf_message = false;
    bool got_nav_timels_message = false;
    bool got_tim_tp_message = false;
    int i = 0;
    int ret = -1;
    int tries = 0;

    if (path == NULL) {
        log_error("\t- GNSS Path does not exists");
        return false;
    }
    RX_ARGS_t args = RX_ARGS_DEFAULT();
    args.autobaud = true;
    args.detect = true;
    log_info("Path is %s", path);
    RX_t * rx = rxInit(path, &args);

    if (!rxOpen(rx)) {
        free(rx);
        log_error("\t- Gnss rx init failed\n");
        return false;
    }

    // Send Default configuration to GNSS receiver
    bool receiver_reconfigured = false;
    tries = 0;
    log_info("configuring receiver with default configuration for ART Card\n");
    while (!receiver_reconfigured) {
        for (i = 0; i < ARRAY_SIZE(ubxCfgValsetMsgs); i++)
        {
            ret = rxSendUbxCfg(rx, ubxCfgValsetMsgs[i].data,
                            ubxCfgValsetMsgs[i].size, 2500);
            if (!ret) {
            } else if (i == ARRAY_SIZE(ubxCfgValsetMsgs) - 1) {
                log_info("Successfully reconfigured gnss receiver");
                log_debug("Performing software reset");
                bool reset = rxReset(rx, RX_RESET_SOFT);
                if (reset) {
                    log_info("Software reset performed");
                    receiver_reconfigured = true;
                }
            }
        }

        if (tries < GNSS_RECONFIGURE_MAX_TRY) {
            tries++;
        } else {
            log_error("Could not reconfigure GNSS receiver from default config\n");
            free(rx);
            return false;
        }
    }

    EPOCH_t coll;
    EPOCH_t epoch;

    epochInit(&coll);

    time_t timeout = time(NULL) + 120;
    while (
        (
            !got_gnss_fix ||
            !got_mon_rf_message ||
            !got_nav_timels_message ||
            !got_tim_tp_message
        ) && time(NULL) < timeout)
    {
        PARSER_MSG_t *msg = rxGetNextMessageTimeout(rx, GNSS_TIMEOUT_MS);
        if (msg != NULL)
        {
            if(epochCollect(&coll, msg, &epoch))
            {
                if(epoch.haveFix && epoch.fixOk && epoch.fix >= EPOCH_FIX_S3D && !got_gnss_fix) {
                    log_info("\t- Got fix !");
                    got_gnss_fix = true;
                }
            } else {
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
        } else {
            log_warn("GNSS: UART Timeout !");
            usleep(5 * 1000);
        }
    }

    rxClose(rx);
    free(rx);
    if (got_gnss_fix &&
        got_mon_rf_message &&
        got_nav_timels_message &&
        got_tim_tp_message
    ) {
        log_info("\t- Passed\n");
        return true;
    }
    log_error("\t- GNSS: did not get all data needed to work with oscillatord software, missing messages are:");
    if (!got_gnss_fix)
        log_error("\t\t- Did not get GNSS Fix");
    if (!got_mon_rf_message)
        log_error("\t\t- Did not get UBX-MON-RF message");
    if (!got_nav_timels_message)
        log_error("\t\t- Did not get UBX-NAV-TIMELS message");
    if (!got_tim_tp_message)
        log_error("\t\t- Did not get UBX-TIM-TP message");
    log_error("Please check antenna cable for fix and reset gnss configuration to default one");
    log_error("\n");
    return false;
}
