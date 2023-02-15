#include <stdlib.h>
#include <unistd.h>
#include <ubloxcfg/ff_epoch.h>
#include <ubloxcfg/ff_rx.h>
#include <ubloxcfg/ff_ubx.h>

#include "gnss.h"
#include "gnss-config.h"
#include "log.h"
#include "f9_defvalsets.h"

#define GNSS_TIMEOUT_MS 1500
#define GNSS_TEST_TIMEOUT 600
#define ARRAY_SIZE(_A) (sizeof(_A) / sizeof((_A)[0]))

static bool parse_receiver_version(char* textToCheck, int* major, int* minor)
{
	return textToCheck && sscanf(textToCheck, "%*s %i.%i", major, minor) == 2;
}

int main(int argc, char *argv[])
{
    char path[256] = "";
    bool gnss_path_valid;
    bool receiver_configured = false;

	/* Set log level */
	log_set_level(1);

    snprintf(path, sizeof(path), "%s", argv[1]);

	log_info("Checking input:");

    if (path == NULL) {
        log_error("\t- GNSS Path does not exists");
        gnss_path_valid = false;
    }

	log_info("\t-GNSS serial path is: \"%s\", checking...",path);
	if (access(path, F_OK) != -1) 
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
        RX_ARGS_t args = RX_ARGS_DEFAULT();
        args.autobaud = true;
        args.detect = true;
        log_info("Path is %s", path);
        RX_t * rx = rxInit(path, &args);

        if (!rxOpen(rx)) {
            free(rx);
            log_error("\t- Gnss rx init failed");
            return false;
        }

        /* Fetch receiver version */
        char verStr[100];
        int major, minor;

        if (rxGetVerStr(rx, verStr, sizeof(verStr))) 
        {
            if (parse_receiver_version(verStr, &major, &minor))
                log_debug("Receiver version successfully detected ! Major is %d, Minor is %d ", major, minor);
            else
                log_warn("Receiver version parsing failed");
        }
        else
            log_warn("Receiver version get command failed");

        /* Get configuration */
        int nAllKvCfg;
        UBLOXCFG_KEYVAL_t *allKvCfg = get_default_value_from_config(&nAllKvCfg, major, minor);

        log_info("Configuring receiver with ART parameters...\n");
        bool res = rxSetConfig(rx, allKvCfg, nAllKvCfg, true, true, true);
        if (res)
        {       
            rxClose(rx);
            if (!rxOpen(rx)) 
            {
                free(rx);
                log_warn("GNSS rx init failed");
            }

            log_info("Successfully reconfigured GNSS receiver");
            log_debug("Performing Hardware reset");
            if (!rxReset(rx, RX_RESET_HARD))
            {
                free(allKvCfg);
                log_warn("GNSS receiver reset error");
            }
            else
            {
                log_info("Hardware reset performed");
                receiver_configured = true;
            }
        }
        else
        {
            log_warn("GNSS receiver configuration failed");
        }

        log_info("Gnss receiver configuration routine done");
        rxClose(rx);
        free(rx);
        free(allKvCfg);
    }
    else
    {
        log_warn("GNSS Config Aborted");
    }
}
