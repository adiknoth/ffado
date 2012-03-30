/*
 * Isochronous Cycle Timer register test
 *
 * Copyright 2010 Stefan Richter <stefanr@s5r6.in-berlin.de>
 * You may freely use, modify, and/or redistribute this program.
 *
 * Version imported into FFADO: v20100125, from
 *   http://user.in-berlin.de/~s5r6/linux1394/utils/
 *
 * This is a tool to test the reliability of one particular hardware feature
 * of OHCI-1394 (FireWire) controllers:  The isochronous cycle timer register.
 * Some controllers do not access this register atomically, resulting in the
 * cycle time seemingly jumping backwards occasionally.
 *
 * The firewire-ohci driver contains a workaround for unreliable isochronous
 * cycle timer hardware, but this workaround is only activated for known bad
 * hardware.  You can use this tool to check whether you have an affected
 * controller and firewire-ohci misses the necessary quirks entry.
 *
 * Usage:
 *
 *   - Compile with "gcc test_cycle_time_v20100125.c".
 *
 *   - Run with "sudo ./a.out /dev/fw0".  Use a different /dev/fw* file if you
 *     have multiple controllers in your machine and want to test the second
 *     or following controller.  Be patient, the test runs for 60 seconds.
 *
 *   - If the very last lines of the resulting output contains only
 *     "0 cycleOffset backwards", "0 cycleCount backwards", "0 cycleSeconds
 *     backwards", then either the hardware works correctly or the driver
 *     already uses the workaround to compensate for a cycle timer hardware
 *     bug.
 *     But if the last lines of the output show one or more of the three cycle
 *     timer components having gone backwards, then the hardware is buggy and
 *     the driver does not yet contain the necessary quirks entry.
 *
 * In the latter case, please report your findings at
 * <linux1394-devel@lists.sourceforge.net>.  This mailinglist is open for
 * posting without prior subscription.  Please include the type and identifiers
 * of your FireWire controller(s) in your posting, as obtained by "lspci -nn".
 *
 * Remark:
 *
 * This program optionally accesses /usr/share/misc/oui.db to translate the
 * Globally Unique Identifier of the device that is associated with the chosen
 * /dev/fw* to the company name of the device manufacturer.  The oui.db file
 * is not necessary for this program to work though.  If you want you can
 * generate oui.db this way:
 *     wget -O - http://standards.ieee.org/regauth/oui/oui.txt |
 *     grep -E '(base 16).*\w+.*$' |
 *     sed -e 's/\s*(base 16)\s*'/' /' > oui.db
 *     sudo mv oui.db /usr/share/misc/
 * which will download ~2 MB data and result in a ~0.4 MB large oui.db.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/firewire-cdev.h>

#define TEST_DURATION 60 /* seconds */

static void cooked_ioctl(int fd, int req, void *arg, const char *name)
{
	if (ioctl(fd, req, arg) < 0) {
		fprintf(stderr, "Failed %s ioctl: %m\n", name);
		exit(1);
	}
}

static int rolled_over(__u32 c0, __u32 c1)
{
	return	(c0 >> 25) == 127 &&
		(c1 >> 25) == 0   &&
		(c0 >> 12 & 0x1fff) > 8000 - 3 &&
		(c1 >> 12 & 0x1fff) < 0000 + 3;
}

static void print_ct(__u32 c, __u64 l)
{
	printf("%03d %04d %04d - %lld.%06lld\n",
	       c >> 25, c >> 12 & 0x1fff, c & 0xfff,
	       l / 1000000, l % 1000000);
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s /dev/fw[0-n]*\n", argv[0]);
		return -1;
	}

	int fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Failed to open %s: %m\n", argv[0]);
		return -1;
	}

	__u32 rom[4];
	struct fw_cdev_event_bus_reset reset;
	struct fw_cdev_get_info info = {
		.rom		= (unsigned long)rom,
		.rom_length	= sizeof(rom),
		.bus_reset	= (unsigned long)&reset,
	};
	cooked_ioctl(fd, FW_CDEV_IOC_GET_INFO, &info, "info");
	if (reset.node_id != reset.local_node_id) {
		fprintf(stderr, "Not a local node\n");
		return -1;
	}

	struct fw_cdev_get_cycle_timer ct;
	__u32 c0, c1;
	__u64 l0, l1, end_time;
	int i, j1, j2, j3;
	cooked_ioctl(fd, FW_CDEV_IOC_GET_CYCLE_TIMER, &ct, "cycle timer");
	c1 = ct.cycle_timer;
	l1 = ct.local_time;
	end_time = l1 + TEST_DURATION * 1000000ULL;
	for (i = j1 = j2 = j3 = 0; l1 < end_time; i++) {
		cooked_ioctl(fd, FW_CDEV_IOC_GET_CYCLE_TIMER, &ct, "cycle timer");
		c0 = c1;
		l0 = l1;
		c1 = ct.cycle_timer;
		l1 = ct.local_time;

		if (c1 <= c0 && !rolled_over(c0, c1)) {
			print_ct(c0, l0);
			print_ct(c1, l1);
			printf("\n");
			j1++;
			if ((c1 & 0xfffff000) < (c0 & 0xfffff000))
				j2++;
			if ((c1 & 0xfe000000) < (c0 & 0xfe000000))
				j3++;
		}
	}

	printf("--------------------------------------------------------\n\n");
	fflush(stdout);

	char buf[200];
	sprintf(buf, "grep %06X /usr/share/misc/oui.db", rom[3] >> 8);
	if (system(buf) != 0)
		printf("%06X (unknown Vendor OUI)\n", rom[3] >> 8);

	printf("\n%d cycleOffset backwards out of %d samples (%.2e)\n",
	       j1, i, (double)j1 / (double)i);

	printf("%d cycleCount backwards (%.2e)\n",
	       j2, (double)j2 / (double)i);

	printf("%d cycleSeconds backwards (%.2e)\n",
	       j3, (double)j3 / (double)i);

	return 0;
}
