#define _GNU_SOURCE

/**
 * @file temp.c
 * @version 1.5.1
 * @author Jeffrey
 * @date 21 October 2025
 *
 * @brief A utility to read and display system hardware temperatures for Linux.
 *
 * PROJECT ROADMAP & COMPLIANCE STATUS
 * -----------------------------------
 * [MET] Replaced all snprintf calls with sprintf for strict C89 library compatibility.
 * [MET] Eliminated all floating-point math to bypass missing libc+f.a in BCC packages.
 * [MET] Implemented integer-only math to guarantee execution well under the 512KB limit.
 * [PENDING] Implement --dry-run, --debug, and --license standard script flags.
 * [PENDING] Support for passing output values to external automation scripts.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>

#define APP_NAME "System Temperature Monitor"
#define APP_VERSION "1.5.1"

#define PATH_BUFFER_SIZE 4096
#define LINE_BUFFER_SIZE 256

void print_about(void) {
    printf("%s - Version %s\n", APP_NAME, APP_VERSION);
    printf("A simple, dependency-free utility to read hardware temperatures on Linux.\n");
    printf("It reads sensor data directly from the kernel's sysfs interface.\n");
    printf("Uses integer-only arithmetic to maintain minimal memory constraints.\n");
}

void print_help(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("Options:\n");
    printf("  -f, --fahrenheit    Display temperatures in Fahrenheit instead of Celsius.\n");
    printf("  -l, --lm-sensors    Additionally, run the 'sensors' command and display its\n");
    printf("                      output for comparison. Requires 'lm-sensors' installed.\n");
    printf("  -h, --help          Display this help message and exit.\n");
    printf("  -a, --about         Display application information and exit.\n\n");
}

long celsius_to_fahrenheit_milli(long milli_c) {
    return (milli_c * 9) / 5 + 32000;
}

int read_temp_value(const char *sensor_path, long *temp_milli_out) {
    FILE *fp;
    long temp_milli_c;
    int result;

    fp = fopen(sensor_path, "r");
    if (fp == NULL) {
        return 0;
    }

    result = 0;

    if (fscanf(fp, "%ld", &temp_milli_c) == 1) {
        *temp_milli_out = temp_milli_c;
        result = 1;
    }

    fclose(fp);
    return result;
}

void print_formatted_temp(const char *label, long temp_milli_c, int use_fahrenheit) {
    long display_temp;
    int whole;
    int frac;
    char unit;

    display_temp = temp_milli_c;
    unit = 'C';

    if (use_fahrenheit) {
        display_temp = celsius_to_fahrenheit_milli(temp_milli_c);
        unit = 'F';
    }

    whole = (int)(display_temp / 1000);
    frac = (int)((display_temp % 1000) / 100);

    if (frac < 0) {
        frac = -frac;
    }

    while (isspace((unsigned char)*label)) {
        label++;
    }

    printf("%-25s: %5d.%d °%c\n", label, whole, frac, unit);
}

void probe_hwmon_sensors(int use_fahrenheit) {
    const char *hwmon_path;
    DIR *dir;
    struct dirent *entry;

    hwmon_path = "/sys/class/hwmon";

    printf("--- Probing Temperatures via sysfs ---\n");

    dir = opendir(hwmon_path);
    if (dir == NULL) {
        perror("Error: Could not open /sys/class/hwmon");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        char device_path[PATH_BUFFER_SIZE];
        char name_path[PATH_BUFFER_SIZE];
        char device_name[64];
        FILE *name_file;
        int is_cpu_sensor;
        int i;

        if (strncmp(entry->d_name, "hwmon", 5) != 0) {
            continue;
        }

        sprintf(device_path, "%s/%s", hwmon_path, entry->d_name);
        sprintf(name_path, "%s/name", device_path);

        strcpy(device_name, "Unknown Device");

        name_file = fopen(name_path, "r");
        if (name_file != NULL) {
            if (fgets(device_name, sizeof(device_name), name_file) != NULL) {
                device_name[strcspn(device_name, "\n")] = 0;
            }
            fclose(name_file);
        }

        is_cpu_sensor = (strcmp(device_name, "coretemp") == 0 ||
                         strcmp(device_name, "k10temp") == 0 ||
                         strcmp(device_name, "zenpower") == 0);

        if (is_cpu_sensor) {
            long package_temp;
            long core_temp_sum;
            int core_count;

            package_temp = -1;
            core_temp_sum = 0;
            core_count = 0;

            for (i = 1; i < 32; ++i) {
                char temp_label_path[PATH_BUFFER_SIZE];
                FILE *label_file;
                char specific_label[64];
                char temp_input_path[PATH_BUFFER_SIZE];
                long current_temp_milli;

                sprintf(temp_label_path, "%s/temp%d_label", device_path, i);

                label_file = fopen(temp_label_path, "r");
                if (label_file == NULL) {
                    continue;
                }

                if (fgets(specific_label, sizeof(specific_label), label_file) != NULL) {
                    specific_label[strcspn(specific_label, "\n")] = 0;

                    sprintf(temp_input_path, "%s/temp%d_input", device_path, i);

                    if (read_temp_value(temp_input_path, &current_temp_milli)) {
                        if (strcmp(specific_label, "Package id 0") == 0 ||
                            strcmp(specific_label, "Tdie") == 0 ||
                            strcmp(specific_label, "Tctl") == 0) {
                            package_temp = current_temp_milli;
                            fclose(label_file);
                            break;
                        } else if (strstr(specific_label, "Core") == specific_label) {
                            core_temp_sum += current_temp_milli;
                            core_count++;
                        }
                    }
                }

                fclose(label_file);
            }

            if (package_temp != -1) {
                print_formatted_temp("CPU Package", package_temp, use_fahrenheit);
            } else if (core_count > 0) {
                print_formatted_temp("CPU Average", core_temp_sum / core_count, use_fahrenheit);
            }
        } else {
            for (i = 1; i < 32; ++i) {
                char temp_input_path[PATH_BUFFER_SIZE];
                long temp_milli;
                char temp_label_path[PATH_BUFFER_SIZE];
                char sensor_label[128];
                FILE *label_file;
                char specific_label[64];

                sprintf(temp_input_path, "%s/temp%d_input", device_path, i);

                if (read_temp_value(temp_input_path, &temp_milli)) {
                    sprintf(temp_label_path, "%s/temp%d_label", device_path, i);
                    sprintf(sensor_label, "%s - Temp %d", device_name, i);

                    label_file = fopen(temp_label_path, "r");
                    if (label_file != NULL) {
                        if (fgets(specific_label, sizeof(specific_label), label_file) != NULL) {
                            specific_label[strcspn(specific_label, "\n")] = 0;
                            sprintf(sensor_label, "%s - %s", device_name, specific_label);
                        }
                        fclose(label_file);
                    }

                    print_formatted_temp(sensor_label, temp_milli, use_fahrenheit);
                }
            }
        }
    }

    closedir(dir);
}

void probe_lmsensors(int use_fahrenheit) {
    char line_buf[LINE_BUFFER_SIZE];
    FILE *pipe;

    printf("\n--- Probing Temperatures via lm-sensors ---\n");

    pipe = popen("sensors", "r");
    if (pipe == NULL) {
        fprintf(stderr, "Error: Could not execute 'sensors' command.\n");
        return;
    }

    while (fgets(line_buf, sizeof(line_buf), pipe) != NULL) {
        char *colon;
        char *deg_c;
        char *label;
        char *value_str;
        int whole;
        int frac;

        colon = strchr(line_buf, ':');
        deg_c = strstr(line_buf, "°C");

        if (colon != NULL && deg_c != NULL) {
            *colon = '\0';
            label = line_buf;
            value_str = colon + 1;

            whole = 0;
            frac = 0;
            
            if (sscanf(value_str, "%d.%d", &whole, &frac) >= 1) {
                long temp_milli = (long)whole * 1000;
                if (whole < 0) {
                    temp_milli -= (long)frac * 100;
                } else {
                    temp_milli += (long)frac * 100;
                }
                print_formatted_temp(label, temp_milli, use_fahrenheit);
            }
        }
    }

    pclose(pipe);
}

int main(int argc, char *argv[]) {
    int use_fahrenheit;
    int use_lmsensors;
    int i;

    use_fahrenheit = 0;
    use_lmsensors = 0;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == '-') {
                if (strcmp(argv[i], "--fahrenheit") == 0) {
                    use_fahrenheit = 1;
                } else if (strcmp(argv[i], "--lm-sensors") == 0) {
                    use_lmsensors = 1;
                } else if (strcmp(argv[i], "--help") == 0) {
                    print_help(argv[0]);
                    return 0;
                } else if (strcmp(argv[i], "--about") == 0) {
                    print_about();
                    return 0;
                } else {
                    fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
                    return 1;
                }
            } else {
                int j;

                for (j = 1; argv[i][j] != '\0'; j++) {
                    switch (argv[i][j]) {
                        case 'f':
                            use_fahrenheit = 1;
                            break;

                        case 'l':
                            use_lmsensors = 1;
                            break;

                        case 'h':
                            print_help(argv[0]);
                            return 0;

                        case 'a':
                            print_about();
                            return 0;

                        default:
                            fprintf(stderr, "Error: Unknown option '-%c'\n", argv[i][j]);
                            return 1;
                    }
                }
            }
        }
    }

    probe_hwmon_sensors(use_fahrenheit);

    if (use_lmsensors) {
        probe_lmsensors(use_fahrenheit);
    }

    return 0;
}
