/*
 * Copyright (c) 2021 Justin Liu
 * Author: Justin Liu <rssnsj@gmail.com>
 * https://github.com/rssnsj/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#ifndef DEFAULT_GPIO
	#define DEFAULT_GPIO 7
#endif

static unsigned gpio_no = DEFAULT_GPIO;
static unsigned hold_ms = 1000;

static int write_to_file(const char *file, const char *str)
{
	int fd;

	if ((fd = open(file, O_WRONLY)) < 0) {
		fprintf(stderr, "*** open('%s'): %s.\n", file, strerror(errno));
		return -1;
	}
	if (write(fd, str, strlen(str)) < 0) {
		fprintf(stderr, "*** write('%s', '%s'): %s.\n", file, str, strerror(errno));
		close(fd);
		return -1;
	}
	close(fd);

	return 0;
}

static void pwm_gpio(const char *file, unsigned operate_us, unsigned cycle_us, unsigned duty_us)
{
	int fd, i;

	if ((fd = open(file, O_WRONLY)) < 0) {
		fprintf(stderr, "*** open('%s'): %s.\n", file, strerror(errno));
		return;
	}

	for (i = 0; i < operate_us / cycle_us; i++) {
		char buf[20];

		sprintf(buf, "1\n");
		write(fd, buf, strlen(buf));
		usleep(duty_us);

		sprintf(buf, "0\n");
		write(fd, buf, strlen(buf));
		usleep(cycle_us - duty_us);
	}

	close(fd);
}

static void print_help(int argc, char *argv[])
{
	printf("GPIO relay control CLI.\n");
	printf("Usage:\n");
	printf("  %s [options]\n", argv[0]);
	printf("  -d <gpio>           GPIO ID (default: %u)\n", gpio_no);
	printf("  -t <msec>           delayed time in ms (default: %u)\n", hold_ms);
	printf("  -h                  print this help\n");
}

int main(int argc, char *argv[])
{
	char the_path[64], the_data[64];
	struct stat sb;
	int rc;

	while ((rc = getopt(argc, argv, "d:t:h")) != -1) {
		switch (rc) {
		case 'd':
			gpio_no = strtoul(optarg, NULL, 10);
			break;
		case 't':
			hold_ms = strtoul(optarg, NULL, 10);
			break;
		case 'h':
			print_help(argc, argv);
			exit(0);
			break;
		case '?':
			print_help(argc, argv);
			exit(1);
		}
	}

	sprintf(the_path, "/sys/class/gpio/gpio%u/direction", gpio_no);
	/* Initialize the GPIO */
	if (stat(the_path, &sb) != 0) {
		sprintf(the_data, "%u\n", gpio_no);
		rc = write_to_file("/sys/class/gpio/export", the_data);
		if (rc < 0)
			return 1;
	}
	rc = write_to_file(the_path, "low\n");

	/* Operate the values */
	sprintf(the_path, "/sys/class/gpio/gpio%u/value", gpio_no);
	pwm_gpio(the_path, 500*1000, 20000, 2500);
	usleep(hold_ms*1000*1000);
	pwm_gpio(the_path, 500*1000, 20000, 1500);

	return 0;
}

