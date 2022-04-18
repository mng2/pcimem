/*
 *  exercise.c: Simple program to write and check (pci) memory from userspace.
 *  Copyright (C) 2022, Mike Ng (mike@kj6aku.net)
 *
 *  Inspired by and cribbed from pcimem.c
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
#include <sys/time.h>

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
    off_t target_base = 0;
    size_t map_size = 512 * 1024 * 1024;
    
    uint64_t writeval = 0;
    uint64_t read_result = 0;
    char *filename;
    int access_type = 'w';

    int i;
    
    struct timeval  time_start, time_end;

    if(argc < 2) {
        // exercise /sys/bus/pci/devices/0001\:00\:07.0/resource0 0
        // argv[0]  [1]                                           [2]
        fprintf(stderr, "\nUsage:\t%s { sysfile } { offset } [ type*count [ data ] ]\n"
            "\tsys file: sysfs file for the pci resource to act on\n"
            "\ttype    : access operation type\n\n",
            argv[0]);
        exit(1);
    }
    filename = argv[1];

    if(argc >= 2) {
        access_type = tolower(argv[2][0]);
    }

    switch(access_type) {
        case '0':
            printf("Will write memory to all '0's...\n");
            break;
        case '1':
            printf("Will write memory to all '1's...\n");
            break;
        default:
            fprintf(stderr, "Illegal access type '%c'.\n", access_type);
            exit(2);
    }

    if((fd = open(filename, O_RDWR | O_SYNC)) == -1) PRINT_ERROR;
    printf("%s opened.\n", filename);
    printf("Target offset is 0, sys page size is %ld\n", sysconf(_SC_PAGE_SIZE));
    fflush(stdout);

    /* Map one page */
    printf("mmap(%d, %d, 0x%x, 0x%x, %d, 0x%x)\n", 0, (unsigned)map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (int) target_base);

    map_base = mmap(0, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, target_base);
    if(map_base == (void *) -1) PRINT_ERROR;
    printf("PCI Memory mapped to address 0x%08lx.\n", (unsigned long) map_base);
    fflush(stdout);

    switch(access_type) {
        case 0:
            writeval = 0;
            break;
        case 1:
            writeval = 0xFFFFFFFFFFFFFFFF;
            break;
        default:
            break;
    }

    // write the stuff

    virt_addr = map_base;
    gettimeofday(&time_start, NULL);
    
    for (i = 0; i < (map_size / 8); i++) {
        *((uint64_t *) virt_addr) = writeval;
        virt_addr += 8;
    }
    
    gettimeofday(&time_end, NULL);
    double time_calc;
    time_calc = (double)(   (time_end.tv_sec + 1.0E-6*time_end.tv_usec) -
                            (time_start.tv_sec + 1.0E-6*time_start.tv_usec));
    printf("Wrote %lu bytes in %1.3f seconds (%1.3f GB/s)\n", map_size, time_calc, 
        map_size/1024./1024./1024./time_calc);

    // readback check

    virt_addr = map_base;
    gettimeofday(&time_start, NULL);
    
    for (i = 0; i < (map_size / 8); i++) {
        read_result = *((uint64_t *) virt_addr);
        if (read_result != writeval)
            printf("Error in readback at location %d, %zu\n", i, read_result);
        virt_addr += 8;
    }
    
    gettimeofday(&time_end, NULL);
    time_calc = (double)(   (time_end.tv_sec + 1.0E-6*time_end.tv_usec) -
                            (time_start.tv_sec + 1.0E-6*time_start.tv_usec));
    printf("Read %lu bytes in %1.3f seconds (%1.3f GB/s)\n", map_size, time_calc, 
        map_size/1024./1024./1024./time_calc);

    if(munmap(map_base, map_size) == -1) PRINT_ERROR;
    close(fd);
    return 0;
}
