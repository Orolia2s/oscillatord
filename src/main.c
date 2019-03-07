#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>
#include <inttypes.h>

#include <error.h>

#include <oscillator-disciplining/oscillator-disciplining.h>

int main (int argc, char *argv[])
{
	struct od *od;
	struct od_input input;
	struct od_output output;
	int ret;

	od = od_new(CLOCK_MONOTONIC);
	if (od == NULL)
		error(EXIT_FAILURE, errno, "od_new");

	input = (struct od_input) {
		.phase_error = (struct timespec) {
			.tv_sec = 0,
			.tv_nsec = 500,
		},
		.valid = true, /* TODO hardcoded until we can get the GNSS status */
	};
	ret = od_process(od, &input, &output);
	if (ret < 0)
		error(EXIT_FAILURE, -ret, "od_process");

	printf("input:"
			"\tphase_error = %ld.%09ld\n"
			"\tvalid = %s\n",
			input.phase_error.tv_sec,
			input.phase_error.tv_nsec,
			input.valid ? "true" : "false");
	printf("output: setpoint = %"PRIu32"\n", output.setpoint);

	od_destroy(&od);

	return EXIT_SUCCESS;
}
