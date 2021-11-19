#include <arpa/inet.h>
#include <json-c/json.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <oscillator-disciplining/oscillator-disciplining.h>
#include "monitoring.h"
#include "log.h"

#define SOCKET_TIMEOUT 2

const char *status_string[5] = {
	"Initialization",
	"Disciplining",
	"Holdover",
	"Calibration",
	"Unknown"
};


static void * monitoring_thread(void * p_data);

struct monitoring* monitoring_init(const struct config *config)
{
	int port;
	int ret;
	int opt = 1;
	struct sockaddr_in server_addr;
	struct monitoring *monitoring;

	const char *address = config_get(config, "socket-address");
	if (address == NULL) {
		log_error("socket-address not defined in config %s", config->path);
		return NULL;
	}

	port = config_get_unsigned_number(config, "socket-port");
	if (port < 0) {
		log_error(
			"Error %d fetching socket-port from config %s",
			port,
			config->path
		);
		return NULL;
	}

	monitoring = (struct monitoring *) malloc(sizeof(struct monitoring));
	if (monitoring == NULL) {
		log_error("Could not allocate memory for monitoring struct");
		return NULL;
	}

	monitoring->stop = false;
	monitoring->disciplining_mode = config_get_bool_default(config, "disciplining", false);

	monitoring->disciplining_status = 4;
	monitoring->ctrl_values.fine_ctrl = -1;
	monitoring->ctrl_values.coarse_ctrl = -1;
	monitoring->ctrl_values.lock = false;
	monitoring->temperature = -400.0;

	monitoring->antenna_power = -1;
	monitoring->antenna_status = -1;
	monitoring->leap_seconds = -1;
	monitoring->phase_error = 0;
	monitoring->fix = -1;
	monitoring->fixOk = false;
	monitoring->lsChange = -10;
	monitoring->satellites_count = -1;
	pthread_mutex_init(&monitoring->mutex, NULL);
	pthread_cond_init(&monitoring->cond, NULL);

	monitoring->sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (monitoring->sockfd == -1) {
		log_error("Error creating monitoring socket");
		free(monitoring);
		return NULL;
	}

	setsockopt(monitoring->sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = inet_addr(address);

	ret = bind(monitoring->sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (ret == -1)
	{
		log_error("Error binding socket to address %s", address);
		free(monitoring);
		return NULL;
	}

	ret = pthread_create(
		&monitoring->thread,
		NULL,
		monitoring_thread,
		monitoring
	);

	log_info(
		"INITIALIZATION: Successfully started monitoring thread, listening on %s:%d",
		address,
		port
	);
	if (ret != 0) {
		log_error("Error creating monitoring thread: %d", ret);
		free(monitoring);
		return NULL;
	}
	return monitoring;
}

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

static void handle_client(struct monitoring *monitoring, int fd)
{
	struct json_object *json_req;
	struct json_object *json_resp;
	int ret;
	bool stop;
	fd_set read_sd;
	FD_ZERO(&read_sd);

	FD_SET(fd, &read_sd);
	pthread_mutex_lock(&monitoring->mutex);
	stop = monitoring->stop;
	pthread_mutex_unlock(&monitoring->mutex);
	struct timeval tv = {5, 0};
	while (!stop) {
		fd_set rsd = read_sd;
		int sel = select(fd + 1, &rsd, 0, 0, &tv);
		if (sel > 0) {
			char req[1024] = {0};
			ret = recv(fd, req, 1024, 0);
			if (ret > 0) {
				struct json_object *obj = json_tokener_parse(req);

				json_object_object_get_ex(obj, "request", &json_req);

				/* Notify main loop about the request */
				pthread_mutex_lock(&monitoring->mutex);
				monitoring->request = (enum monitoring_request) json_object_get_int(json_req);
				if (monitoring->stop) {
					pthread_mutex_unlock(&monitoring->mutex);
					break;
				}

				json_resp = json_object_new_object();
				if (monitoring->request == REQUEST_CALIBRATION) {
					json_object_object_add(json_resp, "calibration",
						json_object_new_string("requested"));
				}

				if (monitoring->disciplining_mode) {
					struct json_object *disciplining = json_object_new_object();
					json_object_object_add(disciplining, "status",
						json_object_new_string(
							status_string[monitoring->disciplining_status]
						)
					);
					json_object_object_add(disciplining, "phase_error",
						json_object_new_int(monitoring->phase_error));
					json_object_object_add(json_resp, "disciplining", disciplining);
				}

				struct json_object *oscillator = json_object_new_object();
				json_object_object_add(oscillator, "model",
					json_object_new_string(monitoring->oscillator_model));
				json_object_object_add(oscillator, "fine_ctrl",
					json_object_new_int(monitoring->ctrl_values.fine_ctrl));
				json_object_object_add(oscillator, "coarse_ctrl",
					json_object_new_int(monitoring->ctrl_values.coarse_ctrl));
				json_object_object_add(oscillator, "lock",
					json_object_new_boolean(monitoring->ctrl_values.lock));
				json_object_object_add(oscillator, "temperature",
					json_object_new_double(monitoring->temperature));

				json_object_object_add(json_resp, "oscillator", oscillator);

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

				json_object_object_add(json_resp, "gnss", gnss);

				monitoring->request = REQUEST_NONE;
				pthread_mutex_unlock(&monitoring->mutex);

				const char *resp = json_object_to_json_string(json_resp);
				ret = send(fd, resp, strlen(resp), 0);
				if (ret == -1)
					log_error("Error sending response: %d", ret);
			} else if (ret == 0) {
				log_debug("Client disconnected, closing connection");
				break;
			} else {
				log_error("Error receving data from client, closing connection");
				break;
			}
		} else {
			log_warn("Socket timeout !");
		}
		pthread_mutex_lock(&monitoring->mutex);
		stop = monitoring->stop;
		pthread_mutex_unlock(&monitoring->mutex);
	}
	close(fd);
	return;
}

static void *monitoring_thread(void * p_data)
{
	struct monitoring *monitoring;
	struct sockaddr_in client_addr;
	socklen_t length;
	int fd, ret;
	bool stop;

	monitoring = (struct monitoring*) p_data;
	stop = monitoring->stop;
	fd_set readfds;

	while (!stop)
	{
		log_trace("Listening on socket...");
		ret = listen(monitoring->sockfd, 1);
		if (ret == -1) {
			log_error("Error listening to socket");
			return NULL;
		}

		FD_ZERO(&readfds);
		FD_SET(monitoring->sockfd, &readfds);
		struct timeval tv = {SOCKET_TIMEOUT, 0};
		int sel = select(monitoring->sockfd + 1, &readfds, 0, 0, &tv);
		if (sel > 0) {
			length = sizeof(client_addr);
			fd = accept(monitoring->sockfd, (struct sockaddr *)&client_addr, &length);
			if (fd == -1) {
				log_error("Error accepting client connection");
				continue;
			}

			if (fd > 0) {
				handle_client(monitoring, fd);
			}
		}
		pthread_mutex_lock(&monitoring->mutex);
		stop = monitoring->stop;
		pthread_mutex_unlock(&monitoring->mutex);
	}
	log_info("Exiting monitoring thread");

	return NULL;
}
