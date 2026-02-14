#include <stdio.h>
#include <string.h>
#include <stdbool.h>

typedef struct 
{
    long total;
    long free;
} mem_data;

typedef struct 
{
    double total[2];    
    double idle[2];
    int record;     // 1 || 0: it tells which index should be modified
    double usage;
} cpu_data;

int read_mem_data(mem_data *);
int read_cpu_data(cpu_data *);

void display_data(mem_data *);

int main() {
    mem_data data = {0};
    read_mem_data(&data);
    display_data(&data);
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

int read_cpu_data(cpu_data *data) {
    FILE *fp;
    char buffer[1024];

    fp = fopen("/proc/stat", "r");
    if(!fp) {
        perror("fopen /proc/stat failed");
        return 0;
    }

    char key[64];
    double v[10];
    while(fgets(buffer, sizeof(buffer), fp)) {
       if(sscanf(buffer, "%63[^:]: %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld", 
        v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9]) == 11) {
            if(strcmp(key, "cpu") == 0) {
                double value = 0;
                for(int i = 0; i < 10; i++) {
                    value += v[i];
                }
                data->total[data->record] = value;
                data->idle[data->record] = v[3];
                break;
            }
        } 
    }
    fclose(fp);
    return 1;
}

void display_data(mem_data *data) {
    printf("MemTotal: %ld kb\n", data->total);
    printf("MemFree: %ld kb\n", data->free);
}