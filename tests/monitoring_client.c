/*
 * Test checking how monitoring socket works and which data it displays
 */
#include <arpa/inet.h>
#include <assert.h>
#include <getopt.h>
#include <json-c/json.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "log.h"
#include "monitoring.h"

static void print_help(void)
{
	printf("usage: monitoring_client [-h -r REQUEST_TYPE] -a ADDRESS -p PORT\n");
	printf("- -a ADDRESS: Adress socket should bind to\n");
	printf("- -p PORT: Port socket should bind to\n");
	printf("- -r REQUEST_TYPE: send a request to oscillatord. Accepted values are:\n");
	printf("\t- calibration: request a calibration of the algorithm\n");
	printf("\t- gnss_start: start gnss receiver\n");
	printf("\t- gnss_stop: stop gnss receiver.\n");
	printf("- -h: prints help\n");
	return;
}

/* Send json formatted request and returns json response */
static struct json_object *json_send_and_receive(int sockfd, int request)
{
	int ret;

	struct json_object *json_req = json_object_new_object();
	json_object_object_add(json_req, "request", json_object_new_int(request));

	const char *req = json_object_to_json_string(json_req);

	ret = send(sockfd, req, strlen(req), 0);
	if (ret == -1)
	{
		log_error("Error sending request: %d", ret);
		log_error("FAIL");
		return NULL;
	}

	char *resp = (char *)malloc(sizeof(char) * 1024);
	ret = recv(sockfd, resp, 1024, 0);
	if (-1 == ret)
	{
		log_error("Error receiving response: %d", ret);
		log_error("FAIL");
		return NULL;
	}

	return json_tokener_parse(resp);
}

int main(int argc, char *argv[]) {
	int c;
	int request = REQUEST_NONE;
	int socket_port = -1;
	char *socket_addr = NULL;

	while ((c = getopt(argc, argv, "a:p:r:h")) != -1)
	switch (c)
	{
		case 'a':
			socket_addr = optarg;
			break;
		case 'p':
			socket_port = atoi(optarg);
			break;
		case 'r':
		if (strcmp(optarg, "calibration") == 0)
			request = REQUEST_CALIBRATION;
		else if (strcmp(optarg, "gnss_start") == 0)
			request = REQUEST_GNSS_START;
		else if (strcmp(optarg, "gnss_stop") == 0)
			request = REQUEST_GNSS_STOP;
		else {
			log_error("Unknown request %s", optarg);
			return -1;
		}
		log_info("Action requested: %s", optarg);
		break;
	case 'h':
		print_help();
		return 0;
	case '?':
		if (optopt == 'r')
			fprintf (stderr, "Option -%c requires request type.\n", optopt);
		else
			fprintf (stderr,
					"Unknown option character `\\x%x'.\n",
					optopt);
		return -1;
	default:
		abort();
	}

	if (socket_addr == NULL || socket_port <= 0) {
		log_error("Bad address / port");
		print_help();
		return -1;
	}

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
	{
		log_error("Could not connect to socket !");
		log_error("Try running with sudo");
		log_error("FAIL");
		return -1;
	}

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(socket_port);
	server_addr.sin_addr.s_addr = inet_addr(socket_addr);
	
	/* Initiate a connection to the server */
	int ret = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (ret == -1)
	{
		log_error("Could not connect to socket !");
		log_error("FAIL");
		return -1;
	}

	/* Request data through socket */
	struct json_object *obj = json_send_and_receive(sockfd, request);
	struct json_object *layer_1;
	struct json_object *layer_2;

	log_info(json_object_to_json_string(obj));
	
	/* Disciplining */
	json_object_object_get_ex(obj, "disciplining", &layer_1);
	if (layer_1 != NULL) {
		json_object_object_get_ex(layer_1, "status", &layer_2);
		const char *status = json_object_get_string(layer_2);
		json_object_object_get_ex(layer_1, "tracking_only", &layer_2);
		const char *tracking_only = json_object_get_string(layer_2);
		json_object_object_get_ex(layer_1, "current_phase_convergence_count", &layer_2);
		int current_phase_convergence_count = json_object_get_int(layer_2);
		json_object_object_get_ex(layer_1, "valid_phase_convergence_threshold", &layer_2);
		int valid_phase_convergence_threshold = json_object_get_int(layer_2);
		json_object_object_get_ex(layer_1, "convergence_progress", &layer_2);
		double convergence_progress = json_object_get_double(layer_2);
		log_info("Disciplining detected");
		log_info("\t- Current status: %s", status);
		log_info("\t- tracking_only: %s", tracking_only);

		if (strcmp(status,"TRACKING") == 0) {
			log_info("\t- tracking convergence progress: %0.2f %% (%d/%d)",convergence_progress,current_phase_convergence_count,valid_phase_convergence_threshold);
		}
		else if (strcmp(status,"LOCK_LOW_RESOLUTION") == 0) {
			log_info("\t- lock low resolution convergence progress: %0.2f %% (%d/%d)",convergence_progress,current_phase_convergence_count,valid_phase_convergence_threshold);
		}
		else if (strcmp(status,"LOCK_HIGH_RESOLUTION") == 0) {
			log_info("\t- lock high resolution convergence progress: %0.2f %% (%d/%d)",convergence_progress,current_phase_convergence_count,valid_phase_convergence_threshold);
		}
	}

	/* Oscillator */
	json_object_object_get_ex(obj, "oscillator", &layer_1);
	if (layer_1 != NULL) {
		json_object_object_get_ex(layer_1, "model", &layer_2);
		const char * model = json_object_get_string(layer_2);
		json_object_object_get_ex(layer_1, "fine_ctrl", &layer_2);
		uint32_t fine_ctrl = json_object_get_int(layer_2);
		json_object_object_get_ex(layer_1, "coarse_ctrl", &layer_2);
		uint32_t coarse_ctrl = json_object_get_int(layer_2);
		json_object_object_get_ex(layer_1, "lock", &layer_2);
		bool lock = json_object_get_boolean(layer_2);
		json_object_object_get_ex(layer_1, "temperature", &layer_2);
		double temperature = json_object_get_double(layer_2);
		log_info("Oscillator detected");
		log_info("\t- model: %s", model);
		log_info("\t- fine_ctrl: %u", fine_ctrl);
		log_info("\t- coarse_ctrl: %u", coarse_ctrl);
		log_info("\t- lock: %s", lock ? "True" : "False");
		log_info("\t- temperature: %f", temperature);
	}

	/* Oscillator */
	json_object_object_get_ex(obj, "clock", &layer_1);
	if (layer_1 != NULL) {
		json_object_object_get_ex(layer_1, "class", &layer_2);
		const char * class = json_object_get_string(layer_2);
		json_object_object_get_ex(layer_1, "offset", &layer_2);
		int32_t offset = json_object_get_int(layer_2);
		log_info("Clock detected");
		log_info("\t- class: %s", class);
		log_info("\t- offset: %d", offset);
	}

	/* GNSS */
	json_object_object_get_ex(obj, "gnss", &layer_1);
	if (layer_1 != NULL) {
		json_object_object_get_ex(layer_1, "fix", &layer_2);
		int fix = json_object_get_int(layer_2);
		json_object_object_get_ex(layer_1, "antenna_status", &layer_2);
		int antenna_status = json_object_get_int(layer_2);
		json_object_object_get_ex(layer_1, "antenna_power", &layer_2);
		int antenna_power = json_object_get_int(layer_2);
		json_object_object_get_ex(layer_1, "lsChange", &layer_2);
		int lsChange = json_object_get_int(layer_2);
		json_object_object_get_ex(layer_1, "leap_seconds", &layer_2);
		int leap_seconds = json_object_get_int(layer_2);
		json_object_object_get_ex(layer_1, "fixOk", &layer_2);
		bool fixOk = json_object_get_boolean(layer_2);
		json_object_object_get_ex(layer_1, "survey_in_error", &layer_2);
		double survey_in_error = json_object_get_double(layer_2);
		log_info("GNSS detected");
		log_info("\t- fix: %u", fix);
		log_info("\t- fixOk: %s", fixOk ? "True" : "False");
		log_info("\t- antenna_status: %u", antenna_status);
		log_info("\t- antenna_power: %u", antenna_power);
		log_info("\t- survey_in_error: %0.2f m", survey_in_error);
		log_info("\t- lsChange: %u", lsChange);
		log_info("\t- leap_seconds: %u", leap_seconds);
	}

	/* ACTION */
	json_object_object_get_ex(obj, "Action requested", &layer_1);
	if (layer_1 != NULL)
		log_info("Action requested: %s", json_object_get_string(layer_1));

	free(obj);

	close(sockfd);
	log_info("PASSED !");

	return 0;
}
