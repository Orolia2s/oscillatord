/**
 * @file monitoring.c
 * @brief monitoring handler
 * @date 2022-01-10
 *
 * @copyright Copyright (c) 2022-2023
 *
 * This program exposes a socket other processes can connect to and
 * request data from, as well as requesting a calibration.
 * Implementation is based on Eli's work: https://eli.thegreenplace.net/2017/concurrent-servers-part-3-event-driven/
 */
#include <assert.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <json-c/json.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "eeprom_config.h"
#include "monitoring.h"
#include "log.h"

/** The socket will not be polled for more than 2 seconds at a time */
#define SOCKET_TIMEOUT_MS 2000

/** Maximum number of pending connections queued up. */
#define N_BACKLOG 64

/**
 * Maximum file descriptor the socket can handle.
 *
 * One peer state will be allocated on the stack for each file descriptor (FD).
 * This number isn't exactily the maximum number of peers this program
 * will be able to handle, but this number minus 1 will be the highest
 * FD handled.
*/
#define MAXFDS 16 * 1024

/** Number of chars allocated on the stack for each peer */
#define SENDBUF_SIZE 1024

typedef enum { INITIAL_ACK, WAIT_FOR_MSG, IN_MSG } ProcessingState;

/** Data stored for each peer. */
typedef struct {
	ProcessingState state;
	char recv_buf[SENDBUF_SIZE];
	int buf_end;
	int buf_ptr;
} peer_state_t;

/**
 * Global table of peers, with file descriptors as keys and peers as values.
 *
 * Each peer is globally identified by the file descriptor (fd) it's connected
 * on. As long as the peer is connected, the fd is unique to it. When a peer
 * disconnects, a new peer may connect and get the same fd. on_peer_connected
 * should initialize the state properly to remove any trace of the old peer on
 * the same fd.
*/
peer_state_t global_state[MAXFDS];

/**
 * File descriptor status.
 *
 * Callbacks (on_XXX functions) return this status to the main loop; the status
 * instructs the loop about the next steps for the fd for which the callback was
 * invoked.
 * want_read=true means we want to keep monitoring this fd for reading.
 * want_write=true means we want to keep monitoring this fd for writing.
 * When both are false it means the fd is no longer needed and can be closed.
*/
typedef struct {
	bool want_read;
	bool want_write;
} fd_status_t;

// These constants make creating fd_status_t values less verbose.
const fd_status_t fd_status_R    = {.want_read = true,  .want_write = false};
const fd_status_t fd_status_W    = {.want_read = false, .want_write = true};
const fd_status_t fd_status_RW   = {.want_read = true,  .want_write = true};
const fd_status_t fd_status_NORW = {.want_read = false, .want_write = false};

static void * monitoring_thread(void * p_data);

const char *clock_class_string[CLOCK_CLASS_NUM] = {
	"Uncalibrated",
	"Calibrating",
	"Holdover",
	"Lock"
};

/**
 * @brief Create, bind and listen socket
 *
 * Will use TCP over IPv4
 * @param address address to bind to
 * @param portnum port to bind to
 * @return socket fd on success, -1 if error
 */
static int listen_inet_socket(const char* address, unsigned short portnum) {
	if (!address) {
		log_error("Monitoring address (provided address is NULL)");
		return -1;
	}

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		log_error("opening socket");
		return -1;
	}

	// This helps avoid spurious EADDRINUSE when the previous instance of this
	// server died.
	int opt = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		log_error("setsockopt");
		close(sockfd);
		return -1;
	}

	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(address);
	serv_addr.sin_port = htons(portnum);

	if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		log_error("on binding");
		close(sockfd);
		return -1;
	}

	if (listen(sockfd, N_BACKLOG) < 0) {
		log_error("on listen");
		close(sockfd);
		return -1;
	}

	return sockfd;
}

/**
 * @brief Make socket non blocking to allow multiple connections
 *
 * @param sockfd socket file descriptor
 */
static void make_socket_non_blocking(int sockfd) {
	int flags = fcntl(sockfd, F_GETFL, 0);
	if (flags == -1) {
		log_error("fcntl F_GETFL");
	}

	if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
		log_error("fcntl F_SETFL O_NONBLOCK");
	}
	return;
}

/**
 * @brief Indicate a peer is connected
 *
 * @param sa socket address and port
 * @param salen socket name length
 */
static void report_peer_connected(const struct sockaddr_in* sa, socklen_t salen) {
	char hostbuf[NI_MAXHOST];
	char portbuf[NI_MAXSERV];
	if (getnameinfo((struct sockaddr*)sa, salen, hostbuf, NI_MAXHOST, portbuf,
					NI_MAXSERV, 0) == 0) {
		log_debug("peer (%s, %s) connected", hostbuf, portbuf);
	} else {
		log_debug("peer (unknown) connected");
	}
}

/**
 * @brief Initialize receive buffer once peer is connected
 *
 * @param sockfd socket file descriptor
 * @param peer_addr
 * @param peer_addr_len
 * @return fd_status_t
 */
static fd_status_t on_peer_connected(int sockfd, const struct sockaddr_in* peer_addr,
									socklen_t peer_addr_len) {
	assert(sockfd < MAXFDS);
	report_peer_connected(peer_addr, peer_addr_len);

	// Initialize state to send back a '*' to the peer immediately.
	peer_state_t* peerstate = &global_state[sockfd];
	peerstate->state = WAIT_FOR_MSG;
	memset(peerstate->recv_buf, 0, 1024);
	peerstate->buf_ptr = 0;
	peerstate->buf_end = 0;

	// Signal that this socket is ready for read now.
	return fd_status_R;
}

/**
 * @brief Callback when ready to receive data from client
 *
 * @param sockfd socket file descriptor
 * @return fd_status_t
 */
static fd_status_t on_peer_ready_recv(int sockfd) {
	assert(sockfd < MAXFDS);
	peer_state_t* peerstate = &global_state[sockfd];

	if (peerstate->state == INITIAL_ACK ||
		peerstate->buf_ptr < peerstate->buf_end) {
		// Until the initial ACK has been sent to the peer, there's nothing we
		// want to receive. Also, wait until all data staged for sending is sent to
		// receive more data.
		return fd_status_R;
	}

	char buf[1024];
	int nbytes = recv(sockfd, buf, sizeof buf, 0);
	if (nbytes == 0) {
		// The peer disconnected.
		return fd_status_NORW;
	} else if (nbytes < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
		// The socket is not *really* ready for recv; wait until it is.
		return fd_status_R;
		} else {
			log_error("recv");
			return fd_status_NORW;
		}
	}
	bool ready_to_send = false;

	/** Store each byte received and try to parse it as a json
	 * if we succeed to parse it as a json then we can analyse
	 * and send the response
	 */
	for (int i = 0; i < nbytes; ++i) {
		switch (peerstate->state) {
		case INITIAL_ACK:
			assert(0 && "can't reach here");
			break;
		case WAIT_FOR_MSG:
			if (buf[i] == '{') {
				peerstate->state = IN_MSG;
				peerstate->recv_buf[0] = buf[i];
				peerstate->buf_end++;
			}
			break;
		case IN_MSG:
			if (json_tokener_parse(peerstate->recv_buf)) {
				peerstate->state = WAIT_FOR_MSG;
				ready_to_send = true;
			} else {
				assert(peerstate->buf_end < SENDBUF_SIZE);
				peerstate->recv_buf[peerstate->buf_end++] = buf[i];
			}
			break;
		}
	}

	if (json_tokener_parse(peerstate->recv_buf)) {
		peerstate->state = WAIT_FOR_MSG;
		ready_to_send = true;
	}

	// Report reading readiness iff there's nothing to analyse from the peer as a
	// result of the latest recv.
	return (fd_status_t){.want_read = !ready_to_send,
						.want_write = ready_to_send};
}

static void json_add_float_array(struct json_object *json, char * array_name, float * array, int length) {
	char array_str[256];
	sprintf(array_str, "[");
	for (int i = 0; i < length; i ++) {
		char item[32];
		sprintf(item, "%.2f", array[i]);
		strcat(array_str, item);
		if (i < length - 1) {
			strcat(array_str, ", ");
		} else {
			strcat(array_str, "]");
		}
	}
	json_object_object_add(
		json,
		array_name,
		json_object_new_string(array_str)
	);
	return;
}

/**
 * @brief Add disciplining_parameters to json response
 *
 * @param resp
 * @param disciplining_parameters
 */
static void json_add_disciplining_disciplining_parameters(struct json_object *resp, struct disciplining_parameters *dsc_params)
{
	struct json_object *disc_parameters_json = json_object_new_object();
	struct json_object *calibration_parameters_json = json_object_new_object();
	struct json_object *temp_table = json_object_new_object();
	struct disciplining_config *dsc_config = &dsc_params->dsc_config;

	json_object_object_add(
		calibration_parameters_json,
		"ctrl_nodes_length",
		json_object_new_int(dsc_config->ctrl_nodes_length)
	);
	if (dsc_config->ctrl_nodes_length > 0) {
		json_add_float_array(
			calibration_parameters_json,
			"ctrl_load_nodes",
			dsc_config->ctrl_load_nodes,
			dsc_config->ctrl_nodes_length
		);
		json_add_float_array(
			calibration_parameters_json,
			"ctrl_drift_coeffs",
			dsc_config->ctrl_drift_coeffs,
			dsc_config->ctrl_nodes_length
		);
	}
	json_object_object_add(
		calibration_parameters_json,
		"coarse_equilibrium",
		json_object_new_int64(dsc_config->coarse_equilibrium)
	);
	json_object_object_add(
		calibration_parameters_json,
		"calibration_date",
		json_object_new_int64(dsc_config->calibration_date)
	);
	json_object_object_add(
		calibration_parameters_json,
		"calibration_valid",
		json_object_new_string(dsc_config->calibration_valid ? "True" : "False")
	);

	json_object_object_add(
		calibration_parameters_json,
		"ctrl_nodes_length_factory",
		json_object_new_int(dsc_config->ctrl_nodes_length_factory)
	);
	if (dsc_config->ctrl_nodes_length > 0) {
		json_add_float_array(
			calibration_parameters_json,
			"ctrl_load_nodes_factory",
			dsc_config->ctrl_load_nodes_factory,
			dsc_config->ctrl_nodes_length_factory
		);
		json_add_float_array(
			calibration_parameters_json,
			"ctrl_drift_coeffs_factory",
			dsc_config->ctrl_drift_coeffs_factory,
			dsc_config->ctrl_nodes_length_factory
		);
	}
	json_object_object_add(
		calibration_parameters_json,
		"coarse_equilibrium_factory",
		json_object_new_int64(dsc_config->coarse_equilibrium_factory)
	);

	json_object_object_add(
		calibration_parameters_json,
		"estimated_equilibrium_ES",
		json_object_new_int64(dsc_config->estimated_equilibrium_ES)
	);
	json_object_object_add(disc_parameters_json, "calibration_parameters", calibration_parameters_json);

	for (int i = 0; i < MEAN_TEMPERATURE_ARRAY_MAX; i++) {
		if (dsc_params->temp_table.mean_fine_over_temperature[i] != 0) {
			char temperature_range[64];
			sprintf(
				temperature_range,
				"[%.2f, %.2f[",
				(i + STEPS_BY_DEGREE * MIN_TEMPERATURE) / STEPS_BY_DEGREE,
				(i + 1 + STEPS_BY_DEGREE * MIN_TEMPERATURE) / STEPS_BY_DEGREE
			);
			char mean_value[32];
			sprintf(
				mean_value,
				"%.1f",
				(float) dsc_params->temp_table.mean_fine_over_temperature[i] / 10
			);
			json_object_object_add(
				temp_table,
				temperature_range,
				json_object_new_string(mean_value)
			);
		}
	}
	json_object_object_add(disc_parameters_json, "temperature_table", temp_table);
	json_object_object_add(resp, "disciplining_parameters", disc_parameters_json);
	return;
}

/**
 * @brief Handle request received by setting monitoring request
 * and add action request in json response
 *
 * @param request_type
 * @param mon_request
 * @param resp
 */
static void json_handle_request(struct monitoring *monitoring, int request_type, enum monitoring_request *mon_request, struct json_object *resp)
{
	switch (request_type)
	{
	case REQUEST_CALIBRATION:
		json_object_object_add(resp, "Action requested",
			json_object_new_string("calibration"));
		*mon_request = REQUEST_CALIBRATION;
		break;
	case REQUEST_GNSS_START:
		json_object_object_add(resp, "Action requested",
			json_object_new_string("GNSS start"));
		*mon_request = REQUEST_GNSS_START;
		break;
	case REQUEST_GNSS_STOP:
		json_object_object_add(resp, "Action requested",
			json_object_new_string("GNSS stop"));
		*mon_request = REQUEST_GNSS_STOP;
		break;
	case REQUEST_READ_EEPROM:
	{
		struct disciplining_parameters dsc_params;
		int ret = write_disciplining_parameters_in_eeprom(
			monitoring->devices_path.disciplining_config_path,
			monitoring->devices_path.temperature_table_path,
			&dsc_params
		);
		if (ret != 0) {
			log_error("Monitoring: Could not get disciplining parameters");
		} else {
			json_add_disciplining_disciplining_parameters(resp, &dsc_params);
		}
		break;
	}
	case REQUEST_SAVE_EEPROM:
		json_object_object_add(resp, "Action requested",
			json_object_new_string("Save EEPROM"));
		*mon_request = REQUEST_SAVE_EEPROM;
		break;
	case REQUEST_FAKE_HOLDOVER_START:
		json_object_object_add(resp, "Action requested",
			json_object_new_string("Start fake holdover"));
		*mon_request = REQUEST_FAKE_HOLDOVER_START;
		break;
	case REQUEST_FAKE_HOLDOVER_STOP:
		json_object_object_add(resp, "Action requested",
			json_object_new_string("Stop fake holdover"));
		*mon_request = REQUEST_FAKE_HOLDOVER_STOP;
		break;
	case REQUEST_NONE:
	default:
		json_object_object_add(resp, "Action requested",
			json_object_new_string("None"));
		break;
	}
}

/**
 * @brief Add disciplining data to json response
 *
 * @param resp
 * @param monitoring
 */
static void json_add_disciplining_data(struct json_object *resp, struct monitoring *monitoring)
{
	struct json_object *disciplining = json_object_new_object();
	json_object_object_add(disciplining, "status",
		json_object_new_string(
			status_string[monitoring->disciplining.status]
		)
	);
	json_object_object_add(disciplining, "current_phase_convergence_count",
		json_object_new_int(
			monitoring->disciplining.current_phase_convergence_count
		)
	);
	json_object_object_add(disciplining, "valid_phase_convergence_threshold",
		json_object_new_int(
			monitoring->disciplining.valid_phase_convergence_threshold
		)
	);
	json_object_object_add(disciplining, "convergence_progress",
		json_object_new_double(
			monitoring->disciplining.convergence_progress
		)
	);
	json_object_object_add(disciplining, "ready_for_holdover",
		json_object_new_string(
			monitoring->disciplining.ready_for_holdover ? "true" : "false"
		)
	);
	json_object_object_add(resp, "disciplining", disciplining);

	/* Add clock class data */
	struct json_object *clock = json_object_new_object();
	json_object_object_add(clock, "class",
		json_object_new_string(clock_class_string[monitoring->disciplining.clock_class])
	);
	json_object_object_add(clock, "offset",
		json_object_new_int(monitoring->phase_error));

	json_object_object_add(resp, "clock", clock);
}

/**
 * @brief Add oscillator data to json response
 *
 * @param resp
 * @param monitoring
 */
static void json_add_oscillator_data(struct json_object *resp, struct monitoring *monitoring)
{
	struct json_object *oscillator = json_object_new_object();
	json_object_object_add(oscillator, "model",
		json_object_new_string(monitoring->oscillator_model));
	json_object_object_add(oscillator, "fine_ctrl",
		json_object_new_int(monitoring->ctrl_values.fine_ctrl));
	json_object_object_add(oscillator, "coarse_ctrl",
		json_object_new_int(monitoring->ctrl_values.coarse_ctrl));
	json_object_object_add(oscillator, "lock",
		json_object_new_boolean(monitoring->osc_attributes.locked));
	json_object_object_add(oscillator, "temperature",
		json_object_new_double(monitoring->osc_attributes.temperature));

	json_object_object_add(resp, "oscillator", oscillator);
}

/**
 * @brief Add GNSS data to json response
 *
 * @param resp
 * @param monitoring
 */
static void json_add_gnss_data(struct json_object *resp, struct monitoring *monitoring)
{
	struct json_object *gnss = json_object_new_object();
	json_object_object_add(gnss, "fix",
		json_object_new_int(monitoring->fix));
	json_object_object_add(gnss, "fixOk",
		json_object_new_boolean(monitoring->fixOk));
	json_object_object_add(gnss, "antenna_power",
		json_object_new_int(monitoring->antenna_power));
	json_object_object_add(gnss, "antenna_status",
		json_object_new_int(monitoring->antenna_status));
	json_object_object_add(gnss, "lsChange",
		json_object_new_int(monitoring->lsChange));
	json_object_object_add(gnss, "leap_seconds",
		json_object_new_int(monitoring->leap_seconds));
	json_object_object_add(gnss, "satellites_count",
		json_object_new_int(monitoring->satellites_count));
	json_object_object_add(gnss, "survey_in_position_error",
		json_object_new_int(monitoring->survey_in_position_error));

	json_object_object_add(resp, "gnss", gnss);
}

/**
 * @brief Analyse request and send response
 *
 * @param sockfd socket file descriptor
 * @param monitoring monitoring struct pointer
 * @return fd_status_t
 */
static fd_status_t on_peer_ready_send(int sockfd, struct monitoring * monitoring) {
	enum monitoring_request request_type = REQUEST_NONE;
	struct json_object *json_req;
	struct json_object *json_resp;
	int ret;

	assert(sockfd < MAXFDS);
	peer_state_t* peerstate = &global_state[sockfd];

	struct json_object *obj = json_tokener_parse(peerstate->recv_buf);
	memset(peerstate->recv_buf, 0, 1024);

	json_object_object_get_ex(obj, "request", &json_req);

	/* Notify main loop about the request */
	pthread_mutex_lock(&monitoring->mutex);
	request_type = (enum monitoring_request) json_object_get_int(json_req);

	json_resp = json_object_new_object();

	json_handle_request(monitoring, request_type, &monitoring->request, json_resp);

	if (monitoring->disciplining_mode || monitoring->phase_error_supported)
		json_add_disciplining_data(json_resp, monitoring);

	json_add_oscillator_data(json_resp, monitoring);
	json_add_gnss_data(json_resp, monitoring);

	pthread_mutex_unlock(&monitoring->mutex);

	const char *resp = json_object_to_json_string(json_resp);
	json_object_object_del(json_resp, "disciplining");
	json_object_object_del(json_resp, "gnss");
	json_object_object_del(json_resp, "oscillator");
	json_object_object_del(json_resp, "disciplining_parameters");
	ret = send(sockfd, resp, strlen(resp), 0);
	if (ret == -1) {
		log_error("Monitoring: Error sending response: %d", ret);
		return fd_status_W;

	} else {
		// Everything was sent successfully; reset the send queue.
		peerstate->buf_ptr = 0;
		peerstate->buf_end = 0;

		// Special-case state transition in if we were in INITIAL_ACK until now.
		if (peerstate->state == INITIAL_ACK) {
			peerstate->state = WAIT_FOR_MSG;
		}

		return fd_status_R;
	}
}

/**
 * @brief Create monitoring structure from config
 *
 * @param config
 * @return struct monitoring*
 */
struct monitoring* monitoring_init(const struct config *config, struct devices_path *devices_path)
{
	int port;
	int ret;
	struct monitoring *monitoring;

	const char *address = config_get(config, "socket-address");
	if (address == NULL) {
		log_error("Monitoring: socket-address not defined in config %s", config->path);
		return NULL;
	}

	port = config_get_unsigned_number(config, "socket-port");
	if (port < 0) {
		log_error(
			"Monitoring: Error %d fetching socket-port from config %s",
			port,
			config->path
		);
		return NULL;
	}

	if (devices_path == NULL) {
		log_error("No struct devices path passed !");
		return NULL;
	}

	monitoring = (struct monitoring *) malloc(sizeof(struct monitoring));
	if (monitoring == NULL) {
		log_error("Monitoring: Could not allocate memory for monitoring struct");
		return NULL;
	}

	monitoring->oscillator_model = config_get(config, "oscillator");
	if (monitoring->oscillator_model == NULL) {
		ret = errno;
		log_error("Monitoring: Configuration \"%s\" doesn't have an oscillator entry.",
				config->path);
		errno = ret;
		return NULL;
	}

	monitoring->stop = false;
	monitoring->disciplining_mode = config_get_bool_default(config, "disciplining", false);
	monitoring->phase_error_supported = false;
	memcpy(&monitoring->devices_path, devices_path, sizeof(struct devices_path));

	monitoring->disciplining.clock_class = CLOCK_CLASS_UNCALIBRATED;
	monitoring->disciplining.status = INIT;
	monitoring->disciplining.current_phase_convergence_count = -1;
	monitoring->disciplining.valid_phase_convergence_threshold = -1;
	monitoring->disciplining.convergence_progress = 0.00;
	monitoring->disciplining.ready_for_holdover = false;
	monitoring->ctrl_values.fine_ctrl = -1;
	monitoring->ctrl_values.coarse_ctrl = -1;
	monitoring->osc_attributes.locked = false;
	monitoring->osc_attributes.temperature = -400.0;

	monitoring->antenna_power = -1;
	monitoring->antenna_status = -1;
	monitoring->leap_seconds = -1;
	monitoring->phase_error = 0;
	monitoring->fix = -1;
	monitoring->fixOk = false;
	monitoring->lsChange = -10;
	monitoring->satellites_count = -1;
	monitoring->survey_in_position_error = -1.0;
	pthread_mutex_init(&monitoring->mutex, NULL);
	pthread_cond_init(&monitoring->cond, NULL);

	monitoring->sockfd = listen_inet_socket(address, port);
	if (monitoring->sockfd == -1) {
		log_error("Monitoring: Error creating monitoring socket");
		free(monitoring);
		return NULL;
	}
	make_socket_non_blocking(monitoring->sockfd);

	ret = pthread_create(
		&monitoring->thread,
		NULL,
		monitoring_thread,
		monitoring
	);

	log_info(
		"Monitoring: INITIALIZATION: Successfully started monitoring thread, listening on %s:%d",
		address,
		port
	);
	if (ret != 0) {
		log_error("Monitoring: Error creating monitoring thread: %d", ret);
		close(monitoring->sockfd);
		free(monitoring);
		return NULL;
	}
	return monitoring;
}

/**
 * @brief Stop monitoring thread
 *
 * @param monitoring
 */
void monitoring_stop(struct monitoring *monitoring)
{
	if (monitoring == NULL)
		return;
	pthread_mutex_lock(&monitoring->mutex);
	monitoring->stop = true;
	pthread_cond_signal(&monitoring->cond);
	pthread_mutex_unlock(&monitoring->mutex);
	pthread_join(monitoring->thread, NULL);

	free(monitoring);
	return;
}

/**
 * @brief Monitoring thread routine
 *
 * @param p_data
 * @return void*
 */
static void *monitoring_thread(void * p_data)
{
	struct monitoring *monitoring;
	bool stop;

	monitoring = (struct monitoring*) p_data;
	stop = monitoring->stop;

	int epollfd = epoll_create1(0);
	if (epollfd < 0) {
		log_error("epoll_create1");
		return NULL;
	}

	struct epoll_event accept_event;
	accept_event.data.fd = monitoring->sockfd;
	accept_event.events = EPOLLIN;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, monitoring->sockfd, &accept_event) < 0) {
		log_error("epoll_ctl EPOLL_CTL_ADD");
		return NULL;
	}

	struct epoll_event* events = calloc(MAXFDS, sizeof(struct epoll_event));
	if (events == NULL) {
		log_error("Unable to allocate memory for epoll_events");
		return NULL;
	}

	while (!stop)
	{
		log_trace("Monitoring: Listening on socket...");
		int nready = epoll_wait(epollfd, events, MAXFDS, SOCKET_TIMEOUT_MS);
		for (int i = 0; i < nready; i++) {
			if (events[i].events & EPOLLERR) {
				log_error("received EPOLLERR");
				log_debug("socket %d closing", events[i].data.fd);
				if (epoll_ctl(epollfd, EPOLL_CTL_DEL, events[i].data.fd, NULL) < 0) {
					log_error("epoll_ctl EPOLL_CTL_DEL");
					return NULL;
				}
				close(events[i].data.fd);
				continue;
			}

			if (events[i].data.fd == monitoring->sockfd) {
				// The listening socket is ready; this means a new peer is connecting.

				struct sockaddr_in peer_addr;
				socklen_t peer_addr_len = sizeof(peer_addr);
				int newsockfd = accept(monitoring->sockfd, (struct sockaddr*)&peer_addr,
									&peer_addr_len);
				if (newsockfd < 0) {
					if (errno == EAGAIN || errno == EWOULDBLOCK) {
						// This can happen due to the nonblocking socket mode; in this
						// case don't do anything, but print a notice (since these events
						// are extremely rare and interesting to observe...)
						log_debug("accept returned EAGAIN or EWOULDBLOCK");
					} else {
						log_error("accept");
						return NULL;
					}
				} else {
					make_socket_non_blocking(newsockfd);
					if (newsockfd >= MAXFDS) {
						log_error("socket fd (%d) >= MAXFDS (%d)", newsockfd, MAXFDS);
						return NULL;
					}

					fd_status_t status =
						on_peer_connected(newsockfd, &peer_addr, peer_addr_len);
					struct epoll_event event = {0};
					event.data.fd = newsockfd;
					if (status.want_read) {
						event.events |= EPOLLIN;
					}
					if (status.want_write) {
						event.events |= EPOLLOUT;
					}

					if (epoll_ctl(epollfd, EPOLL_CTL_ADD, newsockfd, &event) < 0) {
						log_error("epoll_ctl EPOLL_CTL_ADD");
						return NULL;
					}
				}
			} else {
				// A peer socket is ready.
				if (events[i].events & EPOLLIN) {
					// Ready for reading.
					int fd = events[i].data.fd;
					fd_status_t status = on_peer_ready_recv(fd);
					struct epoll_event event = {0};
					event.data.fd = fd;
					if (status.want_read) {
						event.events |= EPOLLIN;
					}
					if (status.want_write) {
						event.events |= EPOLLOUT;
					}
					if (event.events == 0) {
						log_debug("socket %d closing", fd);
						if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL) < 0) {
							log_error("epoll_ctl EPOLL_CTL_DEL");
							return NULL;
						}
						close(fd);
					} else if (epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event) < 0) {
						log_error("epoll_ctl EPOLL_CTL_MOD");
						return NULL;
					}
				} else if (events[i].events & EPOLLOUT) {
					// Ready for writing.
					int fd = events[i].data.fd;
					fd_status_t status = on_peer_ready_send(fd, monitoring);
					struct epoll_event event = {0};
					event.data.fd = fd;

					if (status.want_read) {
						event.events |= EPOLLIN;
					}
					if (status.want_write) {
						event.events |= EPOLLOUT;
					}
					if (event.events == 0) {
						log_debug("socket %d closing", fd);
						if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL) < 0) {
							log_error("epoll_ctl EPOLL_CTL_DEL");
							return NULL;
						}
						close(fd);
					} else if (epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event) < 0) {
						log_error("epoll_ctl EPOLL_CTL_MOD");
						return NULL;
					}
				}
			}
		}
		pthread_mutex_lock(&monitoring->mutex);
		stop = monitoring->stop;
		pthread_mutex_unlock(&monitoring->mutex);

	}
	log_info("Monitoring: Exiting thread");

	return NULL;
}
