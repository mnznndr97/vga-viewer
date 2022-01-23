#include <console.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>

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
	const int suffixLength = 3; // We should never get to Ghz

    int i;
    for (i = 0; i < suffixLength - 1 && hertz > 1000.0f; i++) {
        hertz /= 1000.0f;
    }
	printf("%.2f %s", hertz, suffix[i]);
}
