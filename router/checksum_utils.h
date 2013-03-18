#ifndef CHECKSUM_UTILS_H
#define CHECKSUM_UTILS_H

uint16_t get_checksum_16(const void *_data, int len); /* in icmp_error.c */

uint32_t get_checksum_32(const void *_data, int len); /* in icmp_error.c */

#endif
