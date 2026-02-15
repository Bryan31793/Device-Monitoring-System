#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>

typedef struct 
{
    long total;
    long free;
    long available;
    long cached;
} mem_data;

typedef struct 
{
    long total[2];    
    long idle[2];
    int record;     // 1 || 0: it tells which index should be modified
    double usage;
} cpu_data;

typedef struct 
{
    // I/O Operations
    long reads[3];              // reads completed
    long reads_merged[3];       // reads merged
    long writes[3];             // writes completed  
    long writes_merged[3];      // writes merged
    
    // Throughput
    long sectors_read[3];       // sectors read
    long sectors_written[3];    // sectors written
    
    // Latency/Timing
    long time_reading[3];       // time spent reading (ms)
    long time_writing[3];       // time spent writing (ms)
    long time_io[3];            // time spent doing I/O (ms)
    long weighted_time_io[3];   // weighted time doing I/O (ms)
    
    // Queue depth
    long io_in_progress[3];     // I/Os currently in progress
    
    int record;                 // 0 or 1: which index to modify
    
    // Calculated metrics (useful for ML)
    double read_throughput_kb;   // KB/s
    double write_throughput_kb;  // KB/s
    double read_iops;            // operations per second
    double write_iops;           // operations per second
    double avg_read_latency;     // ms per read
    double avg_write_latency;    // ms per write
    double io_utilization;       // percentage
    double avg_queue_depth;
} disk_data;

int read_mem_data(mem_data *);
int read_cpu_data(cpu_data *);
double get_cpu_percentage(cpu_data *);
int read_disk_data(disk_data *);
void get_disk_information(disk_data *, double);

void display_data(mem_data *, cpu_data*, disk_data *);

int main() {
    mem_data data = {0};
    cpu_data data2 = {0};
    disk_data data3 = {0};
    data2.record = 0;
    data3.record = 0;
    
    struct timespec start, end;
    
    // First samples (baseline)
    read_cpu_data(&data2);
    read_disk_data(&data3);
    
    while(true) {
        // Mark start time
        clock_gettime(CLOCK_MONOTONIC, &start);
        
        // Sleep for 1 second
        sleep(1);
        
        // Mark end time
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        // Take second samples
        read_mem_data(&data);
        read_cpu_data(&data2);
        read_disk_data(&data3);
        
        // Calculate elapsed time
        double elapsed = (end.tv_sec - start.tv_sec) + 
                        (end.tv_nsec - start.tv_nsec) / 1000000000.0;
        
        // Calculate metrics
        data2.usage = get_cpu_percentage(&data2);
        get_disk_information(&data3, elapsed);
        
        // Clear screen and display
        printf("\033[H\033[J");
        display_data(&data, &data2, &data3);
        printf("\nRefresh interval: %.3f seconds\n", elapsed);
    }
    
    return 0;
}

int read_mem_data(mem_data *data) {
    FILE *fp;
    char buffer[1024];

    fp = fopen("/proc/meminfo", "r");   //calls /proc/meminfo and stores output in fp
    if(!fp) {
        perror("fopen /proc/meminfo failed");
        return 0;
    }

    char key[64];
    long value;
    int found = 0;
    //read till find : but don't include it
    while(fgets(buffer, sizeof(buffer), fp) && found != 4) {    //searches line by line
        if (sscanf(buffer, "%63[^:]: %ld", key, &value) == 2) {
            if(strcmp("MemTotal", key) == 0) {
                data->total = value;
                found++;
            }
            else if(strcmp("MemFree", key) == 0) {
                data->free = value;
                found++;
            }
            else if(strcmp("MemAvailable", key) == 0) {
                data->available = value;
                found++;
            }
            else if(strcmp("Cached", key) == 0) {
                data->cached = value;
                found++;
            }
        }
    }
    fclose(fp);
    return (found == 4) ? 1:0;  //1 if it found everything, 0 otherwise
}

int read_cpu_data(cpu_data *stats) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        perror("fopen /proc/stat");
        return 0;
    }
    
    char line[256];
    if (fgets(line, sizeof(line), fp)) {
        long user, nice, system, idle, iowait, irq, softirq, steal;
        
        if (sscanf(line, "cpu %ld %ld %ld %ld %ld %ld %ld %ld",
                   &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) >= 4) {
            stats->total[stats->record] = user + nice + system + idle + iowait + irq + softirq + steal;
            stats->idle[stats->record] = idle + iowait;
            stats->record = (stats->record == 0) ? 1:0;
            fclose(fp);
            return 1;
        }
    }
    
    fclose(fp);
    return 0;
}

double get_cpu_percentage(cpu_data *data) {
    double cpu_total = data->total[0] - data->total[1];
    double idle_total = data->idle[0] - data->idle[1];

    if(cpu_total != 0)
        return (1.0 - idle_total / cpu_total) * 100.0;
    else
        return 0.0;
}

int read_disk_data(disk_data *data) {
    FILE *fp = fopen("/proc/diskstats", "r");
    if (!fp) {
        perror("fopen /proc/diskstats");
        return 0;
    }
    
    char line[256];
    int major, minor;
    char device_name[64];
    long v[11];  // Changed to long for larger values
    
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "%d %d %63s %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld",
                   &major, &minor, device_name,
                   &v[0],   // reads completed
                   &v[1],   // reads merged
                   &v[2],   // sectors read
                   &v[3],   // time reading (ms)
                   &v[4],   // writes completed
                   &v[5],   // writes merged
                   &v[6],   // sectors written
                   &v[7],   // time writing (ms)
                   &v[8],   // I/Os in progress
                   &v[9],   // time doing I/O (ms)
                   &v[10])  // weighted time doing I/O (ms)
                   >= 14) {
            
            if (strcmp(device_name, "nvme0n1") == 0) {
                int idx = data->record;
                
                data->reads[idx] = v[0];
                data->reads_merged[idx] = v[1];
                data->sectors_read[idx] = v[2];
                data->time_reading[idx] = v[3];
                
                data->writes[idx] = v[4];
                data->writes_merged[idx] = v[5];
                data->sectors_written[idx] = v[6];
                data->time_writing[idx] = v[7];
                
                data->io_in_progress[idx] = v[8];
                data->time_io[idx] = v[9];
                data->weighted_time_io[idx] = v[10];
                
                data->record = (data->record == 0) ? 1 : 0;
                fclose(fp);
                return 1;
            }
        }
    }
    
    fclose(fp);
    return 0;
}

void get_disk_information(disk_data *data, double elapsed_seconds) {
    // Calculate deltas
    long delta_reads = data->reads[0] - data->reads[1];
    long delta_writes = data->writes[0] - data->writes[1];
    long delta_sectors_read = data->sectors_read[0] - data->sectors_read[1];
    long delta_sectors_written = data->sectors_written[0] - data->sectors_written[1];
    long delta_time_reading = data->time_reading[0] - data->time_reading[1];
    long delta_time_writing = data->time_writing[0] - data->time_writing[1];
    long delta_time_io = data->time_io[0] - data->time_io[1];
    
    // Store deltas in [2] for reference
    data->reads[2] = delta_reads;
    data->writes[2] = delta_writes;
    data->sectors_read[2] = delta_sectors_read;
    data->sectors_written[2] = delta_sectors_written;
    data->time_io[2] = delta_time_io;
    
    // Calculate derived metrics (these are key for anomaly detection!)
    if (elapsed_seconds > 0) {
        // Throughput
        data->read_throughput_kb = (delta_sectors_read * 512.0 / 1024.0) / elapsed_seconds;
        data->write_throughput_kb = (delta_sectors_written * 512.0 / 1024.0) / elapsed_seconds;
        
        // IOPS
        data->read_iops = delta_reads / elapsed_seconds;
        data->write_iops = delta_writes / elapsed_seconds;
        
        // Average latency per operation
        data->avg_read_latency = (delta_reads > 0) ? 
            (double)delta_time_reading / delta_reads : 0.0;
        data->avg_write_latency = (delta_writes > 0) ? 
            (double)delta_time_writing / delta_writes : 0.0;
        
        // I/O utilization (percentage of time disk was busy)
        data->io_utilization = (delta_time_io / (elapsed_seconds * 1000.0)) * 100.0;
        if (data->io_utilization > 100.0) data->io_utilization = 100.0;
        
        // Average queue depth (simplified)
        data->avg_queue_depth = data->io_in_progress[0];
    }
}

void display_data(mem_data *mem, cpu_data *cpu, disk_data *disk) {
    printf("\n=== SYSTEM MONITORING ===\n");
    
    printf("\nRAM\n");
    printf("  Total: %ld KB\n", mem->total);
    printf("  Free: %ld KB\n", mem->free);
    printf("  Available: %ld KB\n", mem->available);
    printf("  Cached: %ld KB\n", mem->cached);
    printf("  Usage: %.2f%%\n", 
           ((mem->total - mem->available) / (double)mem->total) * 100.0);
    
    printf("\nCPU\n");
    printf("  Usage: %.2f%%\n", cpu->usage);
    
    printf("\nDisk (nvme0n1)\n");
    printf("  Read IOPS: %.2f ops/s\n", disk->read_iops);
    printf("  Write IOPS: %.2f ops/s\n", disk->write_iops);
    printf("  Read Throughput: %.2f KB/s\n", disk->read_throughput_kb);
    printf("  Write Throughput: %.2f KB/s\n", disk->write_throughput_kb);
    printf("  Avg Read Latency: %.2f ms\n", disk->avg_read_latency);
    printf("  Avg Write Latency: %.2f ms\n", disk->avg_write_latency);
    printf("  I/O Utilization: %.2f%%\n", disk->io_utilization);
    printf("  Queue Depth: %.2f\n", disk->avg_queue_depth);
}