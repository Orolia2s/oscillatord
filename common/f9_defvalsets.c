#include "f9_defvalsets.h"

const char default_configuration[284][128] = {
    "UART1     115200  UBX                  UBX",
    "UART2      38400  RTCM3                RTCM3",
    "SPI            -  UBX,NMEA,RTCM3       UBX,NMEA,RTCM3",
    "I2C            -  UBX,NMEA,RTCM3       UBX,NMEA,RTCM3",
    "USB            -  UBX,NMEA,RTCM3       UBX,NMEA,RTCM3",
    "NMEA-PUBX-SVSTATUS         0   0   0   0   0",
    "NMEA-PUBX-POSITION         0   0   0   0   0",
    "NMEA-PUBX-TIME             0   0   0   0   0",
    "NMEA-STANDARD-DTM          0   0   0   0   0",
    "NMEA-STANDARD-GBS          0   0   0   0   0",
    "NMEA-STANDARD-GGA          0   0   0   0   0",
    "NMEA-STANDARD-GLL          0   0   0   0   0",
    "NMEA-STANDARD-GNS          0   0   0   0   0",
    "NMEA-STANDARD-GRS          0   0   0   0   0",
    "NMEA-STANDARD-GSA          0   0   0   0   0",
    "NMEA-STANDARD-GST          0   0   0   0   0",
    "NMEA-STANDARD-GSV          0   0   0   0   0",
    "NMEA-STANDARD-RMC          0   0   0   0   0",
    "NMEA-STANDARD-VLW          0   0   0   0   0",
    "NMEA-STANDARD-VTG          0   0   0   0   0",
    "NMEA-STANDARD-ZDA          0   0   0   0   0",
    "RTCM-3X-TYPE1005           0   0   0   0   0",
    "RTCM-3X-TYPE1077           0   0   0   0   0",
    "RTCM-3X-TYPE1087           0   0   0   0   0",
    "RTCM-3X-TYPE1097           0   0   0   0   0",
    "RTCM-3X-TYPE1127           0   0   0   0   0",
    "RTCM-3X-TYPE1230           0   0   0   0   0",
    "RTCM-3X-TYPE4072_1         0   0   0   0   0",
    "UBX-LOG-INFO               0   0   0   0   0",
    "UBX-MON-COMMS              0   0   0   0   0",
    "UBX-MON-HW                 0   0   0   0   0",
    "UBX-MON-HW2                0   0   0   0   0",
    "UBX-MON-HW3                0   0   0   0   0",
    "UBX-MON-IO                 0   0   0   0   0",
    "UBX-MON-MSGPP              0   0   0   0   0",
    "UBX-MON-RF                 1   1   1   1   1",
    "UBX-MON-RXBUF              0   0   0   0   0",
    "UBX-MON-RXR                0   0   0   0   0",
    "UBX-MON-TEMP               0   0   0   0   0",
    "UBX-MON-TXBUF              0   0   0   0   0",
    "UBX-NAV-CLOCK              0   0   0   0   0",
    "UBX-NAV-COV                0   0   0   0   0",
    "UBX-NAV-DOP                0   0   0   0   0",
    "UBX-NAV-EOE                1   1   1   1   1",
    "UBX-NAV-GEOFENCE           0   0   0   0   0",
    "UBX-NAV-ODO                0   0   0   0   0",
    "UBX-NAV-ORB                0   0   0   0   0",
    "UBX-NAV-POSECEF            0   0   0   0   0",
    "UBX-NAV-POSLLH             0   0   0   0   0",
    "UBX-NAV-PVT                1   1   1   1   1",
    "UBX-NAV-SAT                0   0   0   0   0",
    "UBX-NAV-SBAS               0   0   0   0   0",
    "UBX-NAV-SIG                0   0   0   0   0",
    "UBX-NAV-STATUS             0   0   0   0   0",
    "UBX-NAV-SVIN               0   0   0   0   0",
    "UBX-NAV-TIMEBDS            0   0   0   0   0",
    "UBX-NAV-TIMEGAL            0   0   0   0   0",
    "UBX-NAV-TIMEGLO            0   0   0   0   0",
    "UBX-NAV-TIMEGPS            0   0   0   0   0",
    "UBX-NAV-TIMELS             1   1   1   1   1",
    "UBX-NAV-TIMEUTC            1   1   1   1   1",
    "UBX-NAV-VELECEF            0   0   0   0   0",
    "UBX-NAV-VELNED             0   0   0   0   0",
    "UBX-RXM-MEASX              0   0   0   0   0",
    "UBX-RXM-RAWX               0   0   0   0   0",
    "UBX-RXM-RLM                0   0   0   0   0",
    "UBX-RXM-RTCM               0   0   0   0   0",
    "UBX-RXM-SFRBX              0   0   0   0   0",
    "UBX-TIM-TM2                0   0   0   0   0",
    "UBX-TIM-TP                 1   1   1   1   1",
    "UBX-TIM-VRFY               0   0   0   0   0",
    "UBX-TIM-SVIN               1   1   1   1   1",
    "CFG-GEOFENCE-CONFLVL               L000",
    "CFG-GEOFENCE-USE_PIO               false",
    "CFG-GEOFENCE-PINPOL                LOW_IN",
    "CFG-GEOFENCE-PIN                   3",
    "CFG-GEOFENCE-USE_FENCE1            false",
    "CFG-GEOFENCE-FENCE1_LAT            0",
    "CFG-GEOFENCE-FENCE1_LON            0",
    "CFG-GEOFENCE-FENCE1_RAD            0",
    "CFG-GEOFENCE-USE_FENCE2            false",
    "CFG-GEOFENCE-FENCE2_LAT            0",
    "CFG-GEOFENCE-FENCE2_LON            0",
    "CFG-GEOFENCE-FENCE2_RAD            0",
    "CFG-GEOFENCE-USE_FENCE3            false",
    "CFG-GEOFENCE-FENCE3_LAT            0",
    "CFG-GEOFENCE-FENCE3_LON            0",
    "CFG-GEOFENCE-FENCE3_RAD            0",
    "CFG-GEOFENCE-USE_FENCE4            false",
    "CFG-GEOFENCE-FENCE4_LAT            0",
    "CFG-GEOFENCE-FENCE4_LON            0",
    "CFG-GEOFENCE-FENCE4_RAD            0",
    "CFG-HW-ANT_CFG_VOLTCTRL            true",
    "CFG-HW-ANT_CFG_SHORTDET            true",
    "CFG-HW-ANT_CFG_SHORTDET_POL        true",
    "CFG-HW-ANT_CFG_OPENDET             true",
    "CFG-HW-ANT_CFG_OPENDET_POL         true",
    "CFG-HW-ANT_CFG_PWRDOWN             true",
    "CFG-HW-ANT_CFG_PWRDOWN_POL         true",
    "CFG-HW-ANT_CFG_RECOVER             true",
    "CFG-HW-ANT_SUP_SWITCH_PIN          16",
    "CFG-HW-ANT_SUP_SHORT_PIN           15",
    "CFG-HW-ANT_SUP_OPEN_PIN            14",
    "CFG-I2C-ADDRESS                    132",
    "CFG-I2C-EXTENDEDTIMEOUT            false",
    "CFG-I2C-ENABLED                    true",
    "CFG-INFMSG-UBX_I2C                 0x00",
    "CFG-INFMSG-UBX_UART1               0x00",
    "CFG-INFMSG-UBX_UART2               0x00",
    "CFG-INFMSG-UBX_USB                 0x00",
    "CFG-INFMSG-UBX_SPI                 0x00",
    "CFG-INFMSG-NMEA_I2C                ERROR|WARNING|NOTICE",
    "CFG-INFMSG-NMEA_UART1              ERROR|WARNING|NOTICE",
    "CFG-INFMSG-NMEA_UART2              ERROR|WARNING|NOTICE",
    "CFG-INFMSG-NMEA_USB                ERROR|WARNING|NOTICE",
    "CFG-INFMSG-NMEA_SPI                ERROR|WARNING|NOTICE",
    "CFG-ITFM-BBTHRESHOLD               3",
    "CFG-ITFM-CWTHRESHOLD               15",
    "CFG-ITFM-ENABLE                    false",
    "CFG-ITFM-ANTSETTING                UNKNOWN",
    "CFG-ITFM-ENABLE_AUX                false",
    "CFG-LOGFILTER-RECORD_ENA           false",
    "CFG-LOGFILTER-ONCE_PER_WAKE_UP_ENA false",
    "CFG-LOGFILTER-APPLY_ALL_FILTERS    false",
    "CFG-LOGFILTER-MIN_INTERVAL         0",
    "CFG-LOGFILTER-TIME_THRS            0",
    "CFG-LOGFILTER-SPEED_THRS           0",
    "CFG-LOGFILTER-POSITION_THRS        0",
    "CFG-MOT-GNSSSPEED_THRS             0",
    "CFG-MOT-GNSSDIST_THRS              0",
    "CFG-NAVSPG-FIXMODE                 AUTO",
    "CFG-NAVSPG-INIFIX3D                false",
    "CFG-NAVSPG-WKNROLLOVER             2014",
    "CFG-NAVSPG-USE_PPP                 true",
    "CFG-NAVSPG-UTCSTANDARD             AUTO",
    "CFG-NAVSPG-DYNMODEL                STAT",
    "CFG-NAVSPG-ACKAIDING               false",
    "CFG-NAVSPG-USE_USRDAT              false",
    "CFG-NAVSPG-USRDAT_MAJA             6378137",
    "CFG-NAVSPG-USRDAT_FLAT             298.25722356300002502393908798694610595703125",
    "CFG-NAVSPG-USRDAT_DX               0",
    "CFG-NAVSPG-USRDAT_DY               0",
    "CFG-NAVSPG-USRDAT_DZ               0",
    "CFG-NAVSPG-USRDAT_ROTX             0",
    "CFG-NAVSPG-USRDAT_ROTY             0",
    "CFG-NAVSPG-USRDAT_ROTZ             0",
    "CFG-NAVSPG-USRDAT_SCALE            0",
    "CFG-NAVSPG-INFIL_MINSVS            1",
    "CFG-NAVSPG-INFIL_MAXSVS            32",
    "CFG-NAVSPG-INFIL_MINCNO            9",
    "CFG-NAVSPG-INFIL_MINELEV           5",
    "CFG-NAVSPG-INFIL_NCNOTHRS          0",
    "CFG-NAVSPG-INFIL_CNOTHRS           0",
    "CFG-NAVSPG-OUTFIL_PDOP             250",
    "CFG-NAVSPG-OUTFIL_TDOP             250",
    "CFG-NAVSPG-OUTFIL_PACC             100",
    "CFG-NAVSPG-OUTFIL_TACC             350",
    "CFG-NAVSPG-OUTFIL_FACC             150",
    "CFG-NAVSPG-CONSTR_ALT              0",
    "CFG-NAVSPG-CONSTR_ALTVAR           10000",
    "CFG-NAVSPG-CONSTR_DGNSSTO          60",
    "CFG-NMEA-PROTVER                   V41",
    "CFG-NMEA-MAXSVS                    UNLIM",
    "CFG-NMEA-COMPAT                    false",
    "CFG-NMEA-CONSIDER                  true",
    "CFG-NMEA-LIMIT82                   false",
    "CFG-NMEA-HIGHPREC                  false",
    "CFG-NMEA-SVNUMBERING               STRICT",
    "CFG-NMEA-FILT_GPS                  false",
    "CFG-NMEA-FILT_SBAS                 false",
    "CFG-NMEA-FILT_GAL                  false",
    "CFG-NMEA-FILT_QZSS                 false",
    "CFG-NMEA-FILT_GLO                  false",
    "CFG-NMEA-FILT_BDS                  false",
    "CFG-NMEA-OUT_INVFIX                false",
    "CFG-NMEA-OUT_MSKFIX                false",
    "CFG-NMEA-OUT_INVTIME               false",
    "CFG-NMEA-OUT_INVDATE               false",
    "CFG-NMEA-OUT_ONLYGPS               false",
    "CFG-NMEA-OUT_FROZENCOG             false",
    "CFG-NMEA-MAINTALKERID              AUTO",
    "CFG-NMEA-GSVTALKERID               GNSS",
    "CFG-NMEA-BDSTALKERID               0",
    "CFG-ODO-USE_ODO                    false",
    "CFG-ODO-USE_COG                    false",
    "CFG-ODO-OUTLPVEL                   false",
    "CFG-ODO-OUTLPCOG                   false",
    "CFG-ODO-PROFILE                    RUN",
    "CFG-ODO-COGMAXSPEED                10",
    "CFG-ODO-COGMAXPOSACC               50",
    "CFG-ODO-VELLPGAIN                  153",
    "CFG-ODO-COGLPGAIN                  76",
    "CFG-RATE-MEAS                      1000",
    "CFG-RATE-NAV                       1",
    "CFG-RATE-TIMEREF                   GPS",
    "CFG-RINV-DUMP                      false",
    "CFG-RINV-BINARY                    false",
    "CFG-RINV-DATA_SIZE                 22",
    "CFG-RINV-CHUNK0                    0x203a656369746f4e",
    "CFG-RINV-CHUNK1                    0x2061746164206f6e",
    "CFG-RINV-CHUNK2                    0x0000216465766173",
    "CFG-RINV-CHUNK3                    0x0000000000000000",
    "CFG-SBAS-USE_TESTMODE              false",
    "CFG-SBAS-USE_RANGING               true",
    "CFG-SBAS-USE_DIFFCORR              true",
    "CFG-SBAS-USE_INTEGRITY             false",
    "CFG-SBAS-PRNSCANMASK               PRN120|PRN123|PRN127|PRN128|PRN129|PRN133|PRN135|PRN136|PRN137|PRN138",
    "CFG-SIGNAL-GPS_ENA                 true",
    "CFG-SIGNAL-GPS_L1CA_ENA            true",
    "CFG-SIGNAL-GPS_L2C_ENA             true",
    "CFG-SIGNAL-SBAS_ENA                true",
    "CFG-SIGNAL-SBAS_L1CA_ENA           false",
    "CFG-SIGNAL-GAL_ENA                 true",
    "CFG-SIGNAL-GAL_E1_ENA              true",
    "CFG-SIGNAL-GAL_E5B_ENA             true",
    "CFG-SIGNAL-BDS_ENA                 true",
    "CFG-SIGNAL-BDS_B1_ENA              true",
    "CFG-SIGNAL-BDS_B2_ENA              true",
    "CFG-SIGNAL-QZSS_ENA                true",
    "CFG-SIGNAL-QZSS_L1CA_ENA           true",
    "CFG-SIGNAL-QZSS_L1S_ENA            false",
    "CFG-SIGNAL-QZSS_L2C_ENA            true",
    "CFG-SIGNAL-GLO_ENA                 true",
    "CFG-SIGNAL-GLO_L1_ENA              true",
    "CFG-SIGNAL-GLO_L2_ENA              true",
    "CFG-SPI-MAXFF                      50",
    "CFG-SPI-CPOLARITY                  false",
    "CFG-SPI-CPHASE                     false",
    "CFG-SPI-EXTENDEDTIMEOUT            false",
    "CFG-SPI-ENABLED                    false",
    "CFG-TMODE-MODE                     SURVEY_IN",
    "CFG-TMODE-POS_TYPE                 ECEF",
    "CFG-TMODE-ECEF_X                   0",
    "CFG-TMODE-ECEF_Y                   0",
    "CFG-TMODE-ECEF_Z                   0",
    "CFG-TMODE-ECEF_X_HP                0",
    "CFG-TMODE-ECEF_Y_HP                0",
    "CFG-TMODE-ECEF_Z_HP                0",
    "CFG-TMODE-LAT                      0",
    "CFG-TMODE-LON                      0",
    "CFG-TMODE-HEIGHT                   0",
    "CFG-TMODE-LAT_HP                   0",
    "CFG-TMODE-LON_HP                   0",
    "CFG-TMODE-HEIGHT_HP                0",
    "CFG-TMODE-FIXED_POS_ACC            0",
    "CFG-TMODE-SVIN_MIN_DUR             1200",
    "CFG-TMODE-SVIN_ACC_LIMIT           90000",
    "CFG-TP-PULSE_DEF                   PERIOD",
    "CFG-TP-PULSE_LENGTH_DEF            LENGTH",
    "CFG-TP-ANT_CABLEDELAY              50",
    "CFG-TP-PERIOD_TP1                  1",
    "CFG-TP-PERIOD_LOCK_TP1             1000000",
    "CFG-TP-FREQ_TP1                    1",
    "CFG-TP-FREQ_LOCK_TP1               1",
    "CFG-TP-LEN_TP1                     0",
    "CFG-TP-LEN_LOCK_TP1                100000",
    "CFG-TP-DUTY_TP1                    0",
    "CFG-TP-DUTY_LOCK_TP1               100000",
    "CFG-TP-USER_DELAY_TP1              0",
    "CFG-TP-TP1_ENA                     true",
    "CFG-TP-SYNC_GNSS_TP1               true",
    "CFG-TP-USE_LOCKED_TP1              true",
    "CFG-TP-ALIGN_TO_TOW_TP1            true",
    "CFG-TP-POL_TP1                     true",
    "CFG-TP-TIMEGRID_TP1                GPS",
    "CFG-TXREADY-ENABLED                false",
    "CFG-TXREADY-POLARITY               false",
    "CFG-TXREADY-PIN                    0",
    "CFG-TXREADY-THRESHOLD              0",
    "CFG-TXREADY-INTERFACE              I2C",
    "CFG-UART1-STOPBITS                 ONE",
    "CFG-UART1-DATABITS                 EIGHT",
    "CFG-UART1-PARITY                   NONE",
    "CFG-UART1-ENABLED                  true",
    "CFG-UART2-STOPBITS                 ONE",
    "CFG-UART2-DATABITS                 EIGHT",
    "CFG-UART2-PARITY                   NONE",
    "CFG-UART2-ENABLED                  true",
    "CFG-UART2-REMAP                    false",
    "CFG-USB-ENABLED                    true",
    "CFG-USB-SELFPOW                    true",
    "CFG-USB-VENDOR_ID                  5446",
    "CFG-USB-PRODUCT_ID                 425",
    "CFG-USB-POWER                      0"
};



const char default_configuration_v220[284][128] = {
    "UART1     115200  UBX                  UBX",
    "UART2      38400  RTCM3                RTCM3",
    "SPI            -  UBX,NMEA,RTCM3       UBX,NMEA,RTCM3",
    "I2C            -  UBX,NMEA,RTCM3       UBX,NMEA,RTCM3",
    "USB            -  UBX,NMEA,RTCM3       UBX,NMEA,RTCM3",
    "NMEA-PUBX-SVSTATUS         0   0   0   0   0",
    "NMEA-PUBX-POSITION         0   0   0   0   0",
    "NMEA-PUBX-TIME             0   0   0   0   0",
    "NMEA-STANDARD-DTM          0   0   0   0   0",
    "NMEA-STANDARD-GBS          0   0   0   0   0",
    "NMEA-STANDARD-GGA          0   0   0   0   0",
    "NMEA-STANDARD-GLL          0   0   0   0   0",
    "NMEA-STANDARD-GNS          0   0   0   0   0",
    "NMEA-STANDARD-GRS          0   0   0   0   0",
    "NMEA-STANDARD-GSA          0   0   0   0   0",
    "NMEA-STANDARD-GST          0   0   0   0   0",
    "NMEA-STANDARD-GSV          0   0   0   0   0",
    "NMEA-STANDARD-RLM          0   0   0   0   0",
    "NMEA-STANDARD-RMC          0   0   0   0   0",
    "NMEA-STANDARD-VLW          0   0   0   0   0",
    "NMEA-STANDARD-VTG          0   0   0   0   0",
    "NMEA-STANDARD-ZDA          0   0   0   0   0",
    "RTCM-3X-TYPE1005           0   0   0   0   0",
    "RTCM-3X-TYPE1077           0   0   0   0   0",
    "RTCM-3X-TYPE1087           0   0   0   0   0",
    "RTCM-3X-TYPE1097           0   0   0   0   0",
    "RTCM-3X-TYPE1127           0   0   0   0   0",
    "RTCM-3X-TYPE1230           0   0   0   0   0",
    "RTCM-3X-TYPE4072_1         0   0   0   0   0",
    "UBX-LOG-INFO               0   0   0   0   0",
    "UBX-MON-COMMS              0   0   0   0   0",
    "UBX-MON-HW                 0   0   0   0   0",
    "UBX-MON-HW2                0   0   0   0   0",
    "UBX-MON-HW3                0   0   0   0   0",
    "UBX-MON-IO                 0   0   0   0   0",
    "UBX-MON-MSGPP              0   0   0   0   0",
    "UBX-MON-RF                 1   1   1   1   1",
    "UBX-MON-RXBUF              0   0   0   0   0",
    "UBX-MON-RXR                0   0   0   0   0",
    "UBX-MON-TEMP               0   0   0   0   0",
    "UBX-MON-TXBUF              0   0   0   0   0",
    "UBX-NAV-CLOCK              0   0   0   0   0",
    "UBX-NAV-COV                0   0   0   0   0",
    "UBX-NAV-DOP                0   0   0   0   0",
    "UBX-NAV-EOE                1   1   1   1   1",
    "UBX-NAV-GEOFENCE           0   0   0   0   0",
    "UBX-NAV-ODO                0   0   0   0   0",
    "UBX-NAV-ORB                0   0   0   0   0",
    "UBX-NAV-POSECEF            0   0   0   0   0",
    "UBX-NAV-POSLLH             0   0   0   0   0",
    "UBX-NAV-PVT                1   1   1   1   1",
    "UBX-NAV-SAT                0   0   0   0   0",
    "UBX-NAV-SBAS               0   0   0   0   0",
    "UBX-NAV-SIG                0   0   0   0   0",
    "UBX-NAV-STATUS             0   0   0   0   0",
    "UBX-NAV-SVIN               0   0   0   0   0",
    "UBX-NAV-TIMEBDS            0   0   0   0   0",
    "UBX-NAV-TIMEGAL            0   0   0   0   0",
    "UBX-NAV-TIMEGLO            0   0   0   0   0",
    "UBX-NAV-TIMEGPS            0   0   0   0   0",
    "UBX-NAV-TIMELS             1   1   1   1   1",
    "UBX-NAV-TIMEUTC            1   1   1   1   1",
    "UBX-NAV-VELECEF            0   0   0   0   0",
    "UBX-NAV-VELNED             0   0   0   0   0",
    "UBX-RXM-MEASX              0   0   0   0   0",
    "UBX-RXM-RAWX               0   0   0   0   0",
    "UBX-RXM-RLM                0   0   0   0   0",
    "UBX-RXM-RTCM               0   0   0   0   0",
    "UBX-RXM-SFRBX              0   0   0   0   0",
    "UBX-TIM-TM2                0   0   0   0   0",
    "UBX-TIM-TP                 1   1   1   1   1",
    "UBX-TIM-VRFY               0   0   0   0   0",
    "UBX-TIM-SVIN               1   1   1   1   1",
    "CFG-GEOFENCE-CONFLVL               L000",
    "CFG-GEOFENCE-USE_PIO               false",
    "CFG-GEOFENCE-PINPOL                LOW_IN",
    "CFG-GEOFENCE-PIN                   3",
    "CFG-GEOFENCE-USE_FENCE1            false",
    "CFG-GEOFENCE-FENCE1_LAT            0",
    "CFG-GEOFENCE-FENCE1_LON            0",
    "CFG-GEOFENCE-FENCE1_RAD            0",
    "CFG-GEOFENCE-USE_FENCE2            false",
    "CFG-GEOFENCE-FENCE2_LAT            0",
    "CFG-GEOFENCE-FENCE2_LON            0",
    "CFG-GEOFENCE-FENCE2_RAD            0",
    "CFG-GEOFENCE-USE_FENCE3            false",
    "CFG-GEOFENCE-FENCE3_LAT            0",
    "CFG-GEOFENCE-FENCE3_LON            0",
    "CFG-GEOFENCE-FENCE3_RAD            0",
    "CFG-GEOFENCE-USE_FENCE4            false",
    "CFG-GEOFENCE-FENCE4_LAT            0",
    "CFG-GEOFENCE-FENCE4_LON            0",
    "CFG-GEOFENCE-FENCE4_RAD            0",
    "CFG-HW-ANT_CFG_VOLTCTRL            true",
    "CFG-HW-ANT_CFG_SHORTDET            true",
    "CFG-HW-ANT_CFG_SHORTDET_POL        true",
    "CFG-HW-ANT_CFG_OPENDET             true",
    "CFG-HW-ANT_CFG_OPENDET_POL         true",
    "CFG-HW-ANT_CFG_PWRDOWN             true",
    "CFG-HW-ANT_CFG_PWRDOWN_POL         true",
    "CFG-HW-ANT_CFG_RECOVER             true",
    "CFG-HW-ANT_SUP_SWITCH_PIN          16",
    "CFG-HW-ANT_SUP_SHORT_PIN           15",
    "CFG-HW-ANT_SUP_OPEN_PIN            14",
    "CFG-I2C-ADDRESS                    132",
    "CFG-I2C-EXTENDEDTIMEOUT            false",
    "CFG-I2C-ENABLED                    true",
    "CFG-INFMSG-UBX_I2C                 0x00",
    "CFG-INFMSG-UBX_UART1               0x00",
    "CFG-INFMSG-UBX_UART2               0x00",
    "CFG-INFMSG-UBX_USB                 0x00",
    "CFG-INFMSG-UBX_SPI                 0x00",
    "CFG-INFMSG-NMEA_I2C                ERROR|WARNING|NOTICE",
    "CFG-INFMSG-NMEA_UART1              ERROR|WARNING|NOTICE",
    "CFG-INFMSG-NMEA_UART2              ERROR|WARNING|NOTICE",
    "CFG-INFMSG-NMEA_USB                ERROR|WARNING|NOTICE",
    "CFG-INFMSG-NMEA_SPI                ERROR|WARNING|NOTICE",
    "CFG-ITFM-BBTHRESHOLD               3",
    "CFG-ITFM-CWTHRESHOLD               15",
    "CFG-ITFM-ENABLE                    false",
    "CFG-ITFM-ANTSETTING                UNKNOWN",
    "CFG-ITFM-ENABLE_AUX                false",
    "CFG-LOGFILTER-RECORD_ENA           false",
    "CFG-LOGFILTER-ONCE_PER_WAKE_UP_ENA false",
    "CFG-LOGFILTER-APPLY_ALL_FILTERS    false",
    "CFG-LOGFILTER-MIN_INTERVAL         0",
    "CFG-LOGFILTER-TIME_THRS            0",
    "CFG-LOGFILTER-SPEED_THRS           0",
    "CFG-LOGFILTER-POSITION_THRS        0",
    "CFG-MOT-GNSSSPEED_THRS             0",
    "CFG-MOT-GNSSDIST_THRS              0",
    "CFG-NAVSPG-FIXMODE                 AUTO",
    "CFG-NAVSPG-INIFIX3D                false",
    "CFG-NAVSPG-WKNROLLOVER             2185",
    "CFG-NAVSPG-USE_PPP                 true",
    "CFG-NAVSPG-UTCSTANDARD             AUTO",
    "CFG-NAVSPG-DYNMODEL                STAT",
    "CFG-NAVSPG-ACKAIDING               false",
    "CFG-NAVSPG-USE_USRDAT              false",
    "CFG-NAVSPG-USRDAT_MAJA             6378137",
    "CFG-NAVSPG-USRDAT_FLAT             298.25722356300002502393908798694610595703125",
    "CFG-NAVSPG-USRDAT_DX               0",
    "CFG-NAVSPG-USRDAT_DY               0",
    "CFG-NAVSPG-USRDAT_DZ               0",
    "CFG-NAVSPG-USRDAT_ROTX             0",
    "CFG-NAVSPG-USRDAT_ROTY             0",
    "CFG-NAVSPG-USRDAT_ROTZ             0",
    "CFG-NAVSPG-USRDAT_SCALE            0",
    "CFG-NAVSPG-INFIL_MINSVS            1",
    "CFG-NAVSPG-INFIL_MAXSVS            32",
    "CFG-NAVSPG-INFIL_MINCNO            9",
    "CFG-NAVSPG-INFIL_MINELEV           5",
    "CFG-NAVSPG-INFIL_NCNOTHRS          0",
    "CFG-NAVSPG-INFIL_CNOTHRS           0",
    "CFG-NAVSPG-OUTFIL_PDOP             250",
    "CFG-NAVSPG-OUTFIL_TDOP             250",
    "CFG-NAVSPG-OUTFIL_PACC             100",
    "CFG-NAVSPG-OUTFIL_TACC             350",
    "CFG-NAVSPG-OUTFIL_FACC             150",
    "CFG-NAVSPG-CONSTR_ALT              0",
    "CFG-NAVSPG-CONSTR_ALTVAR           10000",
    "CFG-NAVSPG-CONSTR_DGNSSTO          60",
    "CFG-NMEA-PROTVER                   V411",
    "CFG-NMEA-MAXSVS                    UNLIM",
    "CFG-NMEA-COMPAT                    false",
    "CFG-NMEA-CONSIDER                  true",
    "CFG-NMEA-LIMIT82                   false",
    "CFG-NMEA-HIGHPREC                  false",
    "CFG-NMEA-SVNUMBERING               STRICT",
    "CFG-NMEA-FILT_GPS                  false",
    "CFG-NMEA-FILT_SBAS                 false",
    "CFG-NMEA-FILT_GAL                  false",
    "CFG-NMEA-FILT_QZSS                 false",
    "CFG-NMEA-FILT_GLO                  false",
    "CFG-NMEA-FILT_BDS                  false",
    "CFG-NMEA-OUT_INVFIX                false",
    "CFG-NMEA-OUT_MSKFIX                false",
    "CFG-NMEA-OUT_INVTIME               false",
    "CFG-NMEA-OUT_INVDATE               false",
    "CFG-NMEA-OUT_ONLYGPS               false",
    "CFG-NMEA-OUT_FROZENCOG             false",
    "CFG-NMEA-MAINTALKERID              AUTO",
    "CFG-NMEA-GSVTALKERID               GNSS",
    "CFG-NMEA-BDSTALKERID               0",
    "CFG-ODO-USE_ODO                    false",
    "CFG-ODO-USE_COG                    false",
    "CFG-ODO-OUTLPVEL                   false",
    "CFG-ODO-OUTLPCOG                   false",
    "CFG-ODO-PROFILE                    RUN",
    "CFG-ODO-COGMAXSPEED                10",
    "CFG-ODO-COGMAXPOSACC               50",
    "CFG-ODO-VELLPGAIN                  153",
    "CFG-ODO-COGLPGAIN                  76",
    "CFG-RATE-MEAS                      1000",
    "CFG-RATE-NAV                       1",
    "CFG-RATE-TIMEREF                   GPS",
    "CFG-RINV-DUMP                      false",
    "CFG-RINV-BINARY                    false",
    "CFG-RINV-DATA_SIZE                 22",
    "CFG-RINV-CHUNK0                    0x203a656369746f4e",
    "CFG-RINV-CHUNK1                    0x2061746164206f6e",
    "CFG-RINV-CHUNK2                    0x0000216465766173",
    "CFG-RINV-CHUNK3                    0x0000000000000000",
    "CFG-SBAS-USE_TESTMODE              false",
    "CFG-SBAS-USE_RANGING               true",
    "CFG-SBAS-USE_DIFFCORR              true",
    "CFG-SBAS-USE_INTEGRITY             false",
    "CFG-SBAS-PRNSCANMASK               PRN120|PRN123|PRN127|PRN128|PRN129|PRN133|PRN135|PRN136|PRN137|PRN138",
    "CFG-SIGNAL-GPS_ENA                 true",
    "CFG-SIGNAL-GPS_L1CA_ENA            true",
    "CFG-SIGNAL-GPS_L2C_ENA             true",
    "CFG-SIGNAL-SBAS_ENA                true",
    "CFG-SIGNAL-SBAS_L1CA_ENA           false",
    "CFG-SIGNAL-GAL_ENA                 true",
    "CFG-SIGNAL-GAL_E1_ENA              true",
    "CFG-SIGNAL-GAL_E5B_ENA             true",
    "CFG-SIGNAL-BDS_ENA                 true",
    "CFG-SIGNAL-BDS_B1_ENA              true",
    "CFG-SIGNAL-BDS_B2_ENA              true",
    "CFG-SIGNAL-QZSS_ENA                true",
    "CFG-SIGNAL-QZSS_L1CA_ENA           true",
    "CFG-SIGNAL-QZSS_L1S_ENA            false",
    "CFG-SIGNAL-QZSS_L2C_ENA            true",
    "CFG-SIGNAL-GLO_ENA                 true",
    "CFG-SIGNAL-GLO_L1_ENA              true",
    "CFG-SIGNAL-GLO_L2_ENA              true",
    "CFG-SPI-MAXFF                      50",
    "CFG-SPI-CPOLARITY                  false",
    "CFG-SPI-CPHASE                     false",
    "CFG-SPI-EXTENDEDTIMEOUT            false",
    "CFG-SPI-ENABLED                    false",
    "CFG-TMODE-MODE                     SURVEY_IN",
    "CFG-TMODE-POS_TYPE                 ECEF",
    "CFG-TMODE-ECEF_X                   0",
    "CFG-TMODE-ECEF_Y                   0",
    "CFG-TMODE-ECEF_Z                   0",
    "CFG-TMODE-ECEF_X_HP                0",
    "CFG-TMODE-ECEF_Y_HP                0",
    "CFG-TMODE-ECEF_Z_HP                0",
    "CFG-TMODE-LAT                      0",
    "CFG-TMODE-LON                      0",
    "CFG-TMODE-HEIGHT                   0",
    "CFG-TMODE-LAT_HP                   0",
    "CFG-TMODE-LON_HP                   0",
    "CFG-TMODE-HEIGHT_HP                0",
    "CFG-TMODE-FIXED_POS_ACC            0",
    "CFG-TMODE-SVIN_MIN_DUR             1200",
    "CFG-TMODE-SVIN_ACC_LIMIT           90000",
    "CFG-TP-PULSE_DEF                   PERIOD",
    "CFG-TP-PULSE_LENGTH_DEF            LENGTH",
    "CFG-TP-ANT_CABLEDELAY              50",
    "CFG-TP-PERIOD_TP1                  1",
    "CFG-TP-PERIOD_LOCK_TP1             1000000",
    "CFG-TP-FREQ_TP1                    1",
    "CFG-TP-FREQ_LOCK_TP1               1",
    "CFG-TP-LEN_TP1                     0",
    "CFG-TP-LEN_LOCK_TP1                100000",
    "CFG-TP-DUTY_TP1                    0",
    "CFG-TP-DUTY_LOCK_TP1               100000",
    "CFG-TP-USER_DELAY_TP1              0",
    "CFG-TP-TP1_ENA                     true",
    "CFG-TP-SYNC_GNSS_TP1               true",
    "CFG-TP-USE_LOCKED_TP1              true",
    "CFG-TP-ALIGN_TO_TOW_TP1            true",
    "CFG-TP-POL_TP1                     true",
    "CFG-TP-TIMEGRID_TP1                GPS",
    "CFG-TXREADY-ENABLED                false",
    "CFG-TXREADY-POLARITY               false",
    "CFG-TXREADY-PIN                    0",
    "CFG-TXREADY-THRESHOLD              0",
    "CFG-TXREADY-INTERFACE              I2C",
    "CFG-UART1-STOPBITS                 ONE",
    "CFG-UART1-DATABITS                 EIGHT",
    "CFG-UART1-PARITY                   NONE",
    "CFG-UART1-ENABLED                  true",
    "CFG-UART2-STOPBITS                 ONE",
    "CFG-UART2-DATABITS                 EIGHT",
    "CFG-UART2-PARITY                   NONE",
    "CFG-UART2-ENABLED                  true",
    "CFG-UART2-REMAP                    false",
    "CFG-USB-ENABLED                    true",
    "CFG-USB-SELFPOW                    true",
    "CFG-USB-VENDOR_ID                  5446",
    "CFG-USB-PRODUCT_ID                 425",
    "CFG-USB-POWER                      0"
};
