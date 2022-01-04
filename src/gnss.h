#ifndef OSCILLATORD_GNSS_H
#define OSCILLATORD_GNSS_H

#include <gps.h>

#include <ubloxcfg/ff_rx.h>
#include <pthread.h>
#include <termios.h>

#include "config.h"
#include "ppsthread.h"

#define MAX_DEVICES 4
#define NTPSHMSEGS      (MAX_DEVICES * 2)       /* number of NTP SHM segments */
#define NTP_MIN_FIXES   3  /* # fixes to wait for before shipping NTP time */

#define CLOCKFD 3
#define FD_TO_CLOCKID(fd)	((clockid_t) ((((unsigned int) ~fd) << 3) | CLOCKFD))

typedef struct timespec timespec_t;	/* Unix time as sec, nsec */
struct gps_device_t;

/*
 * Each input source has an associated type.  This is currently used in two
 * ways:
 *
 * (1) To determine if we require that gpsd be the only process opening a
 * device.  We make an exception for PTYs because the master side has to be
 * opened by test code.
 *
 * (2) To determine whether it's safe to send wakeup strings.  These are
 * required on some unusual RS-232 devices (such as the TNT compass and
 * Thales/Ashtech GPSes) but should not be shipped to unidentified USB
 * or Bluetooth devices as we don't even know in advance those are GPSes;
 * they might not cope well.
 *
 * Where it says "case detected but not used" it means that we can identify
 * a source type but no behavior is yet contingent on it.  A "discoverable"
 * device is one for which there is discoverable metadata such as a
 * vendor/product ID.
 *
 * We should never see a block device; that would indicate a serious error
 * in command-line usage or the hotplug system.
 */
typedef enum {source_unknown,
              source_blockdev,  /* block devices can't be GPS sources */
              source_rs232,     /* potential GPS source, not discoverable */
              source_usb,       /* potential GPS source, discoverable */
              source_bluetooth, /* potential GPS source, discoverable */
              source_can,       /* potential GPS source, fixed CAN format */
              source_pty,       /* PTY: we don't require exclusive access */
              source_tcp,       /* TCP/IP stream: case detected but not used */
              source_udp,       /* UDP stream: case detected but not used */
              source_gpsd,      /* Remote gpsd instance over TCP/IP */
              source_pps,       /* PPS-only device, such as /dev/ppsN */
              source_pipe,      /* Unix FIFO; don't use blocking I/O */
} sourcetype_t;

struct gps_context_t {
	int valid;                          /* member validity flags */
#define LEAP_SECOND_VALID       0x01    /* we have or don't need correction */
#define GPS_TIME_VALID          0x02    /* GPS week/tow is valid */
#define CENTURY_VALID           0x04    /* have received ZDA or 4-digit year */
	// struct gpsd_errout_t errout;        /* debug verbosity level and hook */
	bool readonly;                      /* if true, never write to device */
	bool passive;                       // if true, never autoconfigure device
	// if true, remove fix gate to time, for some RTC backed receivers.
	// DANGEROUS
	bool batteryRTC;
	speed_t fixed_port_speed;           // Fixed port speed, if non-zero
	char fixed_port_framing[4];         // Fixed port framing, if non-blank
	/* DGPS status */
	int fixcnt;                         /* count of good fixes seen */
	/* timekeeping */
	time_t start_time;                  /* local time of daemon startup */
	int leap_seconds;                   /* Unix secs to UTC (GPS-UTC offset) */
	unsigned short gps_week;            /* GPS week, usually 10 bits */
	timespec_t gps_tow;                 /* GPS time of week */
	int century;                        /* for NMEA-only devices without ZDA */
	int rollovers;                      /* rollovers since start of run */
	int leap_notify;                    /* notification state from subframe */
#define LEAP_NOWARNING  0x0     /* normal, no leap second warning */
#define LEAP_ADDSECOND  0x1     /* last minute of day has 60 seconds */
#define LEAP_DELSECOND  0x2     /* last minute of day has 59 seconds */
#define LEAP_NOTINSYNC  0x3     /* overload, clock is free running */
	int lsChange;
	int timeToLsEvent;
	bool lsset;
	/* we need the volatile here to tell the C compiler not to
		* 'optimize' as 'dead code' the writes to SHM */
	volatile struct shmTime *shmTime[NTPSHMSEGS];
	bool shmTimeInuse[NTPSHMSEGS];
	void (*pps_hook)(struct gps_device_t *, struct timedelta_t *);
#ifdef SHM_EXPORT_ENABLE
    /* we don't want the compiler to treat writes to shmexport as dead code,
     * and we don't want them reordered either */
    volatile void *shmexport;
    int shmid;                          /* ID of SHM  (for later IPC_RMID) */
#endif
	ssize_t (*serial_write)(struct gps_device_t *,
		const char *buf, const size_t len);
};

struct gps_device_t {
    struct gps_context_t        *context;
    sourcetype_t sourcetype;
    volatile struct shmTime *shm_clock;
    volatile struct shmTime *shm_pps;
    volatile struct pps_thread_t pps_thread;
    int fixcnt;                         /* count of fixes from this device */
	struct timespec last_fix_utc_time;
	int fix;
	bool fixOk;
	bool leap_second_occured;
	int8_t antenna_status;
	int8_t antenna_power;
	bool valid;
	bool tai_time_set;
	int tai_time;
	int satellites_count;
};

struct gnss {
	bool session_open;
	RX_t *rx;
	struct gps_device_t *session;
	pthread_t thread;
	pthread_mutex_t mutex_data;
	pthread_cond_t cond_time;
	int fd_clock;
	bool stop;
};

struct gnss* gnss_init(const struct config *config, struct gps_device_t *session, int fd_clock);
bool gnss_get_valid(struct gnss *gnss);
void gnss_stop(struct gnss *gnss);
int gnss_set_ptp_clock_time(struct gnss *gnss);

#endif
