#ifndef OSCILLATORD_GNSS_CONFIG_H
#define OSCILLATORD_GNSS_CONFIG_H

#include <ubloxcfg/ff_rx.h>
#include <ubloxcfg/ubloxcfg.h>

bool check_gnss_config_in_ram(RX_t *rx, UBLOXCFG_KEYVAL_t *allKvCfg, int nAllKvCfg);
UBLOXCFG_KEYVAL_t *get_default_value_from_config(int *nKv, int major, int minor);

#endif /* OSCILLATORD_GNSS_CONFIG_H */
