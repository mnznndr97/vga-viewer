/*
 * console.c
 *
 *  Created on: Nov 12, 2021
 *      Author: mnznn
 */

#include <console.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>
#include <cmsis_extensions.h>

void FormatTime(float seconds) {
	// Assuming that a negative time value is not "correct, so we don't format it
	// If needed, we can always store the sign, abs() the value and later reapply the sign
	if (seconds <= 0.0f) {
		printf("%.2f sec", seconds);
		return;
	}

	static const char *suffix[] = { "sec", "ms", "us", "ns" };
	const int suffixLength = 3; // We should never get to ns

	int i;
	for (i = 0; i < suffixLength - 1 && seconds > 0.0f && seconds < 1.0f; i++) {
		seconds *= 1000.0f;
	}

	printf("%.2f %s", seconds, suffix[i]);
}

void FormatFrequency(float hertz) {
	// Assuming that a negative frequancy value is not "correct, so we don't format it
	// If needed, we can always store the sign, abs() the value and later reapply the sign
	if (hertz <= 0.0f) {
		printf("%.2f Hz", hertz);
		return;
	}

	static const char *suffix[] = { "Hz", "KHz", "MHz", "GHz" };
	const int suffixLength = 3; // We should never get to ns

	float lastValidHertz = hertz;
	int i = 0;
	do {
		hertz /= 1000.0f;
		if (i < suffixLength - 1 && hertz > 1.0f) {
			lastValidHertz = hertz;
			++i;
		} else
			break;
	} while (true);
	printf("%.2f %s", lastValidHertz, suffix[i]);
}
