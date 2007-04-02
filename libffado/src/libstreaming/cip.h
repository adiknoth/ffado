#ifndef _IEC61883_CIP_PRIVATE_H
#define _IEC61883_CIP_PRIVATE_H

#include <libraw1394/raw1394.h>
#include <endian.h>
#include <stdint.h>

#define IEC61883_FMT_DV 0x00
#define IEC61883_FMT_AMDTP 0x10
#define IEC61883_FMT_MPEG2 0x20

#define CIP_TRANSFER_DELAY 9000


#ifdef __cplusplus
extern "C" {
#endif

#if __BYTE_ORDER == __BIG_ENDIAN


struct iec61883_packet {
	/* First quadlet */
	uint8_t dbs      : 8;
	uint8_t eoh0     : 2;
	uint8_t sid      : 6;

	uint8_t dbc      : 8;
	uint8_t fn       : 2;
	uint8_t qpc      : 3;
	uint8_t sph      : 1;
	uint8_t reserved : 2;

	/* Second quadlet */
	uint8_t fdf      : 8;
	uint8_t eoh1     : 2;
	uint8_t fmt      : 6;

	uint16_t syt      : 16;

	unsigned char data[0];
};

#elif __BYTE_ORDER == __LITTLE_ENDIAN

struct iec61883_packet {
	/* First quadlet */
	uint8_t sid      : 6;
	uint8_t eoh0     : 2;
	uint8_t dbs      : 8;

	uint8_t reserved : 2;
	uint8_t sph      : 1;
	uint8_t qpc      : 3;
	uint8_t fn       : 2;
	uint8_t dbc      : 8;

	/* Second quadlet */
	uint8_t fmt      : 6;
	uint8_t eoh1     : 2;
	uint8_t fdf      : 8;

	uint16_t syt      : 16;

	unsigned char data[0];
};

#else

#error Unknown bitfield type

#endif

/*
 * The TAG value is present in the isochronous header (first quadlet). It
 * provides a high level label for the format of data carried by the
 * isochronous packet.
 */

#define IEC61883_TAG_WITHOUT_CIP 0 /* CIP header NOT included */
#define IEC61883_TAG_WITH_CIP    1 /* CIP header included. */
#define IEC61883_TAG_RESERVED1   2 /* Reserved */
#define IEC61883_TAG_RESERVED2   3 /* Reserved */

#define IEC61883_FDF_NODATA   0xFF
		
/* AM824 format definitions. */
#define IEC61883_FDF_AM824 0x00
#define IEC61883_FDF_AM824_CONTROLLED 0x04
#define IEC61883_FDF_SFC_MASK 0x03

#define IEC61883_AM824_LABEL              0x40
#define IEC61883_AM824_LABEL_RAW_24BITS   0x40
#define IEC61883_AM824_LABEL_RAW_20BITS   0x41
#define IEC61883_AM824_LABEL_RAW_16BITS   0x42
#define IEC61883_AM824_LABEL_RAW_RESERVED 0x43

#define IEC61883_AM824_VBL_24BITS   0x0
#define IEC61883_AM824_VBL_20BITS   0x1
#define IEC61883_AM824_VBL_16BITS   0x2
#define IEC61883_AM824_VBL_RESERVED 0x3

/* IEC-60958 format definitions. */
#define IEC60958_LABEL   0x0
#define IEC60958_PAC_B   0x3 /* Preamble Code 'B': Start of channel 1, at
			      * the start of a data block. */
#define IEC60958_PAC_RSV 0x2 /* Preamble Code 'RESERVED' */
#define IEC60958_PAC_M   0x1 /* Preamble Code 'M': Start of channel 1 that
			      *	is not at the start of a data block. */
#define IEC60958_PAC_W   0x0 /* Preamble Code 'W': start of channel 2. */
#define IEC60958_DATA_VALID   0 /* When cleared means data is valid. */
#define IEC60958_DATA_INVALID 1 /* When set means data is not suitable for an ADC. */

struct iec61883_fraction {
	int integer;
	int numerator;
	int denominator;
};

struct iec61883_cip {
	struct iec61883_fraction cycle_offset;
	struct iec61883_fraction ticks_per_syt_offset;
	struct iec61883_fraction ready_samples;
	struct iec61883_fraction samples_per_cycle;
	int dbc, dbs;
	int cycle_count;
	int cycle_count2;
	int mode;
	int syt_interval;
	int dimension;
	int rate;
	int fdf;
	int format;
};

void
iec61883_cip_init(struct iec61883_cip *cip, int format, int fdf,
		int rate, int dbs, int syt_interval);
void 
iec61883_cip_set_transmission_mode(struct iec61883_cip *ptz, int mode);

int 
iec61883_cip_get_max_packet_size(struct iec61883_cip *ptz);

int
iec61883_cip_fill_header(int node_id, struct iec61883_cip *cip,
		struct iec61883_packet *packet);

int
iec61883_cip_fill_header_nodata(int node_id, struct iec61883_cip *cip,
		struct iec61883_packet *packet);

#ifdef __cplusplus
}
#endif

#endif
