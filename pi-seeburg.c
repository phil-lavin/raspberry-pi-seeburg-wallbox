#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <wiringPi.h>

// Which GPIO pin we're using
#define PIN 2

// How much time a change must be since the last in order to count as a change
#define IGNORE_CHANGE_BELOW_USEC 10000
// What is the minimum time since the last pulse for a pulse to count as "after the gap"
#define MIN_GAP_LEN_USEC 200000
// What is the mimimum time since the last pulse for a pulse to count as a new train
#define MIN_TRAIN_BOUNDARY_USEC 400000 // 0.4 sec
// How often to update the last change value to stop diff overflowing
#define OVERFLOW_PROTECTION_INTERVAL_USEC 60000000 // 60 secs

// Time of last change
struct timeval last_change;
// Which side of "the gap" we're on
int pre_gap = 1;
// Pulse counters
int pre_gap_pulses = 0;
int post_gap_pulses = 0;

// Predefines
unsigned long get_diff(struct timeval now, struct timeval last_change);
void handle_gpio_interrupt(void);
void handle_key_combo(char letter, int number);

int main(void) {
	struct timeval now;
	unsigned long diff;
	char letter;
	int number;

	// Init
	wiringPiSetup();

	// Set pin to output in case it's not
	pinMode(PIN, OUTPUT);

	// Bind to interrupt
	wiringPiISR(PIN, INT_EDGE_BOTH, &handle_gpio_interrupt);

	// The loop...
	for (;;) {
		// Time now
		gettimeofday(&now, NULL);

		// Time difference in usec
		diff = get_diff(now, last_change);

		// Should reset counters?
		if (diff > MIN_TRAIN_BOUNDARY_USEC) {
			// Have we registered a full pulse train (i.e. seen a gap)?
			if (!pre_gap) {
				// 0 base the counts
				pre_gap_pulses--;
				post_gap_pulses--;

				// Calc the key combination...
				letter = 'A' + (2 * post_gap_pulses) + (pre_gap_pulses > 10); // A plus the offset plus 1 more if pre gap pulses > 10
				letter += (letter > 'H'); // Hax for missing I
				number = pre_gap_pulses % 10;

				// Hand off to the handler
				handle_key_combo(letter, number);
			}

			// Reset counters
			if (pre_gap_pulses || post_gap_pulses) {
				pre_gap_pulses = 0;
				post_gap_pulses = 0;
				pre_gap = 1;
			}
		}

		// Should update time to stop diff overflowing?
		if (diff > OVERFLOW_PROTECTION_INTERVAL_USEC) {
			gettimeofday(&last_change, NULL);
		}

		// Waste time but not CPU whilst still allowing us to detect finished pulses
		usleep(10000);
	}

	return 0;
}

// Handler for interrupt
void handle_gpio_interrupt(void) {
	struct timeval now;
	unsigned long diff;

	gettimeofday(&now, NULL);

	// Time difference in usec
	diff = get_diff(now, last_change);

	// Filter jitter
	if (diff > IGNORE_CHANGE_BELOW_USEC) {
		// Should switch to post gap? It's a gap > gap len but less than train boundary. Only when we're doing numbers, though.
		if (pre_gap && diff > MIN_GAP_LEN_USEC && diff < MIN_TRAIN_BOUNDARY_USEC) {
			pre_gap = 0;
		}

		// Increment the right counter
		if (pre_gap) {
			pre_gap_pulses++;
		}
		else {
			post_gap_pulses++;
		}
	}

	// Record when the last change was
	last_change = now;
}

// Handler for the completed key combination
void handle_key_combo(char letter, int number) {
	printf("Combo: %c%d\n\n", letter, number);
}

// Returns the time difference, in usec, between two provided struct timevals 
unsigned long get_diff(struct timeval now, struct timeval last_change) {
	return (now.tv_sec * 1000000 + now.tv_usec) - (last_change.tv_sec * 1000000 + last_change.tv_usec);
}
