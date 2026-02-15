#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

typedef struct 
{
    long total;
    long free;
} mem_data;

typedef struct 
{
    long total[2];    
    long idle[2];
    int record;     // 1 || 0: it tells which index should be modified
    double usage;
} cpu_data;

int read_mem_data(mem_data *);
int read_cpu_data(cpu_data *);
double get_cpu_percentage(cpu_data *);

void display_data(mem_data *, cpu_data*);

int main() {
    mem_data data = {0};
    cpu_data data2 = {0};
    data2.record = 0;
    
    // First CPU sample
    read_cpu_data(&data2);
    
    // Sleep for 100ms to get meaningful CPU usage
    struct timespec sleep_time = {0, 100000000};  // 0 seconds, 100 million nanoseconds (100ms)
    nanosleep(&sleep_time, NULL);
    
    // Read memory and second CPU sample
    read_mem_data(&data);
    read_cpu_data(&data2);
    
    data2.usage = get_cpu_percentage(&data2);
    display_data(&data, &data2);
    
    
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
    while(fgets(buffer, sizeof(buffer), fp) && found != 2) {    //searches line by line
        if (sscanf(buffer, "%63[^:]: %ld", key, &value) == 2) {
            if(strcmp("MemTotal", key) == 0) {
                data->total = value;
                found++;
            }
            else if(strcmp("MemFree", key) == 0) {
                data->free = value;
                found++;
            }
        }
    }
    fclose(fp);
    return (found == 2) ? 1:0;  //1 if it found everything, 0 otherwise
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

void display_data(mem_data *mem, cpu_data *cpu) {
    printf("MemTotal: %ld kb\n", mem->total);
    printf("MemFree: %ld kb\n", mem->free);

    printf("cpuPercentage: %lf\n", cpu->usage);
}