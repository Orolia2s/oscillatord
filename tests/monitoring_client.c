#include <arpa/inet.h>
#include <assert.h>
#include <json-c/json.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "log.h"

enum monitoring_request {
	REQUEST_NONE,
	REQUEST_CALIBRATION,
};

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
	
	
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
	{
		log_error("Could not connect to socket !");
		log_error("FAIL");
		return -1;
	}

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = 2958;
	server_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
	
	//Initiate a connection to the server
	int ret = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (ret == -1)
	{
		log_error("Could not connect to socket !");
		log_error("FAIL");
		return -1;
	}

	/* REQUEST PHASE ERROR */
	struct json_object *obj = json_send_and_receive(sockfd, REQUEST_NONE);
	struct json_object *layer_1;
	struct json_object *layer_2;

	log_info(json_object_to_json_string(obj));
	
	// Disciplining
	json_object_object_get_ex(obj, "disciplining", &layer_1);
	if (layer_1 != NULL) {
		json_object_object_get_ex(layer_1, "phase_error", &layer_2);
		int phase_error = json_object_get_int(layer_2);
		json_object_object_get_ex(layer_1, "status", &layer_2);
		const char *status = json_object_get_string(layer_2);
		log_info("Disciplining detected");
		log_info("\t- Current status: %s", status);
		log_info("\t- Phase error: %d", phase_error);
	}

	// Disciplining
	json_object_object_get_ex(obj, "oscillator", &layer_1);
	if (layer_1 != NULL) {
		json_object_object_get_ex(layer_1, "model", &layer_2);
		char * model = json_object_get_string(layer_2);
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

	// GNSS
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
		log_info("GNSS detected");
		log_info("\t- fix: %u", fix);
		log_info("\t- fixOk: %s", fixOk ? "True" : "False");
		log_info("\t- antenna_status: %u", antenna_status);
		log_info("\t- antenna_power: %u", antenna_power);
		log_info("\t- lsChange: %u", lsChange);
		log_info("\t- leap_seconds: %u", leap_seconds);
	}
	free(obj);
	/* REQUEST CALIBRATION */
	// obj = json_send_and_receive(sockfd, REQUEST_CALIBRATION);
	close(sockfd);
	log_info("PASSED !");

	return 0;
}
