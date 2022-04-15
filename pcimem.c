/*
 * pcimem.c: Simple program to read/write from/to a pci device from userspace.
 *
 *  Copyright (C) 2010, Bill Farrow (bfarrow@beyondelectronics.us)
 *
 *  Based on the devmem2.c code
 *  Copyright (C) 2000, Jan-Derk Bakker (J.D.Bakker@its.tudelft.nl)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/mman.h>

#ifdef __SIZEOF_INT128__
#define SUPPORT128
#define uint128_t __uint128_t
#endif

#define PRINT_ERROR \
	do { \
		fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", \
		__LINE__, __FILE__, errno, strerror(errno)); exit(1); \
	} while(0)


int main(int argc, char **argv) {
	int fd;
	void *map_base, *virt_addr;
    uint64_t writeval = 0;
#ifdef SUPPORT128
	uint128_t read_result, prev_read_result = 0;
#else
    uint64_t read_result, prev_read_result = 0;
#endif
	char *filename;
	off_t target, target_base;
	int access_type = 'w';
	int items_count = 1;
	int verbose = 0;
	int read_result_dupped = 0;
	int num_bytes;
	int i;
	int map_size = 4096UL;

	if(argc < 3) {
		// pcimem /sys/bus/pci/devices/0001\:00\:07.0/resource0 0x100 w 0x00
		// argv[0]  [1]                                         [2]   [3] [4]
		fprintf(stderr, "\nUsage:\t%s { sysfile } { offset } [ type*count [ data ] ]\n"
			"\tsys file: sysfs file for the pci resource to act on\n"
			"\toffset  : offset into pci memory region to act upon\n"
			"\ttype    : access operation type : [b]yte, [h]alfword, [w]ord, [d]ouble-word\n"
			"\t*count  : number of items to read:  w*100 will dump 100 words\n"
			"\tdata    : data to be written\n\n",
			argv[0]);
		exit(1);
	}
	filename = argv[1];
	target = strtoul(argv[2], 0, 0);

	if(argc > 3) {
		access_type = tolower(argv[3][0]);
		if (argv[3][1] == '*')
			items_count = strtoul(argv[3]+2, 0, 0);
	}

        switch(access_type) {
		case 'b':
			num_bytes = 1;
			break;
		case 'h':
			num_bytes = 2;
			break;
		case 'w':
			num_bytes = 4;
			break;
        case 'd':
			num_bytes = 8;
			break;
#ifdef SUPPORT128
        case 'q':
            num_bytes = 16;
            break;
#endif
		default:
			fprintf(stderr, "Illegal data type '%c'.\n", access_type);
			exit(2);
	}

    if((fd = open(filename, O_RDWR | O_SYNC)) == -1) PRINT_ERROR;
    printf("%s opened.\n", filename);
    printf("Target offset is 0x%x, page size is %ld\n", (int) target, sysconf(_SC_PAGE_SIZE));
    fflush(stdout);

    target_base = target & ~(sysconf(_SC_PAGE_SIZE)-1);
    if (target + items_count*num_bytes - target_base > map_size)
	map_size = target + items_count*num_bytes - target_base;

    /* Map one page */
    printf("mmap(%d, %d, 0x%x, 0x%x, %d, 0x%x)\n", 0, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (int) target);

    map_base = mmap(0, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, target_base);
    if(map_base == (void *) -1) PRINT_ERROR;
    printf("PCI Memory mapped to address 0x%08lx.\n", (unsigned long) map_base);
    fflush(stdout);

    for (i = 0; i < items_count; i++) {

        virt_addr = map_base + target + i*num_bytes - target_base;
        switch(num_bytes) {
		case 1:
			read_result = *((uint8_t *) virt_addr);
			break;
		case 2:
			read_result = *((uint16_t *) virt_addr);
			break;
		case 4:
			read_result = *((uint32_t *) virt_addr);
			break;
        case 8:
			read_result = *((uint64_t *) virt_addr);
			break;
#ifdef SUPPORT128
        case 16:
            read_result = *((uint128_t *) virt_addr);
			break;
#endif
	}

    	if (verbose) {
            printf("Value at offset 0x%X (%p): ", (int) target + i*num_bytes, 
                                    virt_addr);
            if (num_bytes < 16) {
                printf("0x%0*zX\n", num_bytes*2, (uint64_t)read_result);
            }
            else {
                printf("0x%0*zX %0*zX\n", num_bytes, (uint64_t)(read_result >> 64),
                                        num_bytes, (uint64_t)read_result);
            }
        } 
        else {
            if (read_result != prev_read_result || i == 0) {
                printf("0x%04X: ", (int)(target + i*num_bytes));
                if (num_bytes < 16) {
                    printf("0x%0*zX\n", num_bytes*2, (uint64_t)read_result);
                }
                else {
                    printf("0x%0*zX %0*zX\n", num_bytes, (uint64_t)(read_result >> 64),
                                            num_bytes, (uint64_t)read_result);
                }
                read_result_dupped = 0;
            } else {
                if (!read_result_dupped)
                    printf("...\n");
                read_result_dupped = 1;
            }
        }
	
	prev_read_result = read_result;

    }

    fflush(stdout);

	if(argc > 4) {
		writeval = strtoull(argv[4], NULL, 0);
		switch(num_bytes) {
			case 1:
				*((uint8_t *) virt_addr) = writeval;
				read_result = *((uint8_t *) virt_addr);
				break;
			case 2:
				*((uint16_t *) virt_addr) = writeval;
				read_result = *((uint16_t *) virt_addr);
				break;
			case 4:
				*((uint32_t *) virt_addr) = writeval;
				read_result = *((uint32_t *) virt_addr);
				break;
			case 8:
				*((uint64_t *) virt_addr) = writeval;
				read_result = *((uint64_t *) virt_addr);
				break;
#ifdef SUPPORT128
            case 16:
                *((uint128_t *) virt_addr) = (((uint128_t)writeval) << 64)|((uint128_t)writeval);
				read_result = *((uint128_t *) virt_addr);
				break;
#endif
		}
        if (num_bytes < 16) {
            printf("Written 0x%0*zX,\n", num_bytes*2, writeval);
            printf("Readbck 0x%0*zX.\n", num_bytes*2, (uint64_t)read_result);
        }
        else {
            printf("Written 0x%0*zX %0*zX,\n", num_bytes, writeval, num_bytes, writeval);
            printf("Readbck 0x%0*zX %0*zX.\n", num_bytes, (uint64_t)(read_result >> 64),
                                                num_bytes, (uint64_t)read_result);
        }
		fflush(stdout);
	}

	if(munmap(map_base, map_size) == -1) PRINT_ERROR;
    close(fd);
    return 0;
}
