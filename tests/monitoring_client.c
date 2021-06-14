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
	struct json_object *json_resp;

	json_object_object_get_ex(obj, "phase_error", &json_resp);
	int phase_error = json_object_get_int(json_resp);
	json_object_object_get_ex(obj, "status", &json_resp);
	const char *status = json_object_get_string(json_resp);
	log_info("Current status: %s, phase error %d", status, phase_error);
	free(obj);

	/* REQUEST STATUS */
	obj = json_send_and_receive(sockfd, REQUEST_NONE);
	json_object_object_get_ex(obj, "phase_error", &json_resp);
	int phase_error2 = json_object_get_int(json_resp);
	json_object_object_get_ex(obj, "status", &json_resp);
	const char *status2 = json_object_get_string(json_resp);
	log_info("Current status: %s, phase error %d", status2, phase_error2);
	free(obj);

	/* REQUEST CALIBRATION */
	// obj = json_send_and_receive(sockfd, REQUEST_CALIBRATION);
	close(sockfd);
	log_info("PASSED !");

	return 0;
}
