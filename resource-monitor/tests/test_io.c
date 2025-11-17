#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static void usage(const char *prog) {
    fprintf(stderr,
            "Uso: %s [--file caminho] [--size-mb MB] [--block-kb KB]\n",
            prog);
}

static double elapsed_seconds(struct timespec start, struct timespec end) {
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_nsec - start.tv_nsec) / 1e9;
}

int main(int argc, char **argv) {
    const char *filepath = "io_workload.dat";
    size_t size_mb = 256;
    size_t block_kb = 1024;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) {
            filepath = argv[++i];
        } else if (strcmp(argv[i], "--size-mb") == 0 && i + 1 < argc) {
            size_mb = (size_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--block-kb") == 0 && i + 1 < argc) {
            block_kb = (size_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    if (size_mb == 0 || block_kb == 0) {
        fprintf(stderr, "size-mb e block-kb devem ser maiores que zero.\n");
        return 1;
    }

    size_t total_bytes = size_mb * 1024UL * 1024UL;
    size_t block_bytes = block_kb * 1024UL;

    char *buffer = malloc(block_bytes);
    if (buffer == NULL) {
        perror("malloc");
        return 1;
    }
    for (size_t i = 0; i < block_bytes; i++) {
        buffer[i] = (char)(i & 0xFF);
    }

    int fd = open(filepath, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) {
        perror("open");
        free(buffer);
        return 1;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    size_t written = 0;
    while (written < total_bytes) {
        size_t chunk = block_bytes;
        if (chunk > total_bytes - written) {
            chunk = total_bytes - written;
        }
        ssize_t ret = write(fd, buffer, chunk);
        if (ret < 0) {
            perror("write");
            close(fd);
            free(buffer);
            return 1;
        }
        written += (size_t)ret;
    }

    fsync(fd);
    clock_gettime(CLOCK_MONOTONIC, &end);
    close(fd);

    double write_time = elapsed_seconds(start, end);
    double write_bw = (double)written / (1024.0 * 1024.0) / write_time;

    fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        perror("open(read)");
        free(buffer);
        return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &start);
    size_t read_bytes = 0;
    while (read_bytes < total_bytes) {
        size_t chunk = block_bytes;
        if (chunk > total_bytes - read_bytes) {
            chunk = total_bytes - read_bytes;
        }
        ssize_t ret = read(fd, buffer, chunk);
        if (ret < 0) {
            perror("read");
            close(fd);
            free(buffer);
            return 1;
        }
        if (ret == 0) {
            break;
        }
        read_bytes += (size_t)ret;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    close(fd);

    double read_time = elapsed_seconds(start, end);
    double read_bw = (double)read_bytes / (1024.0 * 1024.0) / read_time;

    printf("io_test file=%s size_mb=%zu block_kb=%zu write_bw=%.2fMB/s read_bw=%.2fMB/s\n",
           filepath, size_mb, block_kb, write_bw, read_bw);
    fflush(stdout);

    free(buffer);
    return 0;
}
