#define _GNU_SOURCE

/**
 * @file temp.c
 * @version 1.6.0
 * @author Jeffrey
 * @date 13 May 2026
 *
 * @brief A utility to read and display system hardware temperatures for Linux.
 *
 * PROJECT ROADMAP & COMPLIANCE STATUS
 * -----------------------------------
 * [MET] Restored complete source code to resolve EOF and missing semicolon errors.
 * [MET] Implemented standard script flags: --dry-run, --debug, and --license.
 * [MET] Maintained strict ANSI C89 and BCC compatibility with bounded string functions.
 * [PENDING] Implement support for passing output values to external automation scripts.
 * [PENDING] Design a test suite or verification script for supported hardware platforms.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>

#define APP_NAME "System Temperature Monitor"
#define APP_VERSION "1.6.0"

#define PATH_BUFFER_SIZE 4096
#define LINE_BUFFER_SIZE 256

/* Global configuration state for the current execution */
int config_use_fahrenheit = 0;
int config_use_lmsensors = 0;
int config_dry_run = 0;
int config_debug = 0;

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
    printf("  -l, --lm-sensors    Additionally run 'sensors' command for comparison.\n");
    printf("  -h, --help          Display this help message and exit.\n");
    printf("  -a, --about         Display application information and exit.\n");
    printf("  -v, --version       Display the current version and exit.\n");
    printf("  -d, --debug         Enable verbose diagnostic output.\n");
    printf("  -t, --dry-run       Simulate execution without reading sensor files.\n");
    printf("      --license       Display the Modified MIT License and exit.\n\n");
}

void print_license(void) {
    printf("Modified MIT License\n\n");
    printf("Copyright (c) 2025-2026 Jeffrey\n\n");
    printf("Permission is hereby granted, free of charge, to any person obtaining a copy\n");
    printf("of this software and associated documentation files (the \"Software\"), to deal\n");
    printf("in the Software without restriction, including without limitation the rights\n");
    printf("to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n");
    printf("copies of the Software, and to permit persons to whom the Software is\n");
    printf("furnished to do so, subject to the following conditions:\n\n");
    printf("The above copyright notice and this permission notice shall be included in all\n");
    printf("copies or substantial portions of the Software.\n\n");
    printf("THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n");
    printf("IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n");
    printf("FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n");
    printf("AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n");
    printf("LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n");
    printf("OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n");
    printf("SOFTWARE.\n");
}

long celsius_to_fahrenheit_milli(long milli_c) {
    return (milli_c * 9) / 5 + 32000;
}

int read_temp_value(const char *sensor_path, long *temp_milli_out) {
    FILE *fp;
    long temp_milli_c;
    int result;

    if (config_debug) {
        printf("[DEBUG] Attempting to read sensor file: %s\n", sensor_path);
    }

    if (config_dry_run) {
        if (config_debug) {
            printf("[DEBUG] Dry run active. Skipping file read.\n");
        }
        *temp_milli_out = 45000; /* Return dummy 45.0C for dry-run simulation */
        return 1;
    }

    fp = fopen(sensor_path, "r");
    if (fp == NULL) {
        if (config_debug) {
            printf("[DEBUG] Failed to open sensor file: %s\n", sensor_path);
        }
        return 0;
    }

    result = 0;
    if (fscanf(fp, "%ld", &temp_milli_c) == 1) {
        *temp_milli_out = temp_milli_c;
        result = 1;
        if (config_debug) {
            printf("[DEBUG] Successfully read raw value: %ld\n", temp_milli_c);
        }
    } else if (config_debug) {
        printf("[DEBUG] Failed to parse integer from: %s\n", sensor_path);
    }

    fclose(fp);
    return result;
}

void print_formatted_temp(const char *label, long temp_milli_c) {
    long display_temp;
    int whole;
    int frac;
    char unit;

    display_temp = temp_milli_c;
    unit = 'C';

    if (config_use_fahrenheit) {
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

    printf("%-25s: %5d.%d %c\n", label, whole, frac, unit);
}

void build_path(char *dest, const char *base, const char *suffix) {
    strncpy(dest, base, PATH_BUFFER_SIZE - 1);
    dest[PATH_BUFFER_SIZE - 1] = '\0';
    strncat(dest, suffix, PATH_BUFFER_SIZE - strlen(dest) - 1);
}

void probe_hwmon_sensors(void) {
    const char *hwmon_path = "/sys/class/hwmon";
    DIR *dir;
    struct dirent *entry;

    if (config_debug) {
        printf("[DEBUG] Initiating sysfs hardware monitor probe at %s\n", hwmon_path);
    }

    printf("--- Probing Temperatures via sysfs ---\n");

    dir = opendir(hwmon_path);
    if (dir == NULL) {
        if (config_debug) {
            printf("[DEBUG] Directory %s could not be opened.\n", hwmon_path);
        }
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        char device_path[PATH_BUFFER_SIZE];
        char name_path[PATH_BUFFER_SIZE];
        char device_name[64];
        FILE *name_file;
        int i;

        if (strncmp(entry->d_name, "hwmon", 5) != 0) {
            continue;
        }

        strncpy(device_path, hwmon_path, PATH_BUFFER_SIZE - 1);
        device_path[PATH_BUFFER_SIZE - 1] = '\0';
        strncat(device_path, "/", PATH_BUFFER_SIZE - strlen(device_path) - 1);
        strncat(device_path, entry->d_name, PATH_BUFFER_SIZE - strlen(device_path) - 1);

        build_path(name_path, device_path, "/name");

        strcpy(device_name, "Unknown Device");
        
        if (config_dry_run) {
            strcpy(device_name, "Simulated_Device");
        } else {
            name_file = fopen(name_path, "r");
            if (name_file != NULL) {
                if (fgets(device_name, sizeof(device_name), name_file) != NULL) {
                    device_name[strcspn(device_name, "\n")] = 0;
                }
                fclose(name_file);
            }
        }

        if (config_debug) {
            printf("[DEBUG] Found hwmon device: %s (%s)\n", entry->d_name, device_name);
        }

        /* Probe up to 32 possible temperature inputs per device */
        for (i = 1; i < 32; ++i) {
            char temp_input_path[PATH_BUFFER_SIZE];
            char temp_label_path[PATH_BUFFER_SIZE];
            char sensor_label[160];
            char specific_label[64];
            char suffix[32];
            long current_temp_milli;
            FILE *label_file;

            sprintf(suffix, "/temp%d_input", i);
            build_path(temp_input_path, device_path, suffix);

            /* In dry-run mode, simulate finding exactly two sensors per device */
            if (config_dry_run && i > 2) {
                break;
            }

            if (read_temp_value(temp_input_path, &current_temp_milli)) {
                sprintf(suffix, "/temp%d_label", i);
                build_path(temp_label_path, device_path, suffix);
                
                sprintf(sensor_label, "%s - Temp %d", device_name, i);

                if (config_dry_run) {
                    sprintf(sensor_label, "%.60s - Simulated Core %d", device_name, i);
                } else {
                    label_file = fopen(temp_label_path, "r");
                    if (label_file != NULL) {
                        if (fgets(specific_label, sizeof(specific_label), label_file) != NULL) {
                            specific_label[strcspn(specific_label, "\n")] = 0;
                            sprintf(sensor_label, "%.60s - %.60s", device_name, specific_label);
                        }
                        fclose(label_file);
                    }
                }
                print_formatted_temp(sensor_label, current_temp_milli);
            }
        }
    }
    closedir(dir);
}

void probe_lmsensors(void) {
    char line_buf[LINE_BUFFER_SIZE];
    FILE *pipe;

    if (config_debug) {
        printf("[DEBUG] Initiating external lm-sensors probe.\n");
    }

    printf("\n--- Probing Temperatures via lm-sensors ---\n");
    
    if (config_dry_run) {
        printf("Simulated External Sensor  :    40.0 C\n");
        return;
    }

    pipe = popen("sensors", "r");
    if (pipe == NULL) {
        if (config_debug) {
            printf("[DEBUG] popen() failed for 'sensors' command.\n");
        }
        return;
    }

    while (fgets(line_buf, sizeof(line_buf), pipe) != NULL) {
        char *colon = strchr(line_buf, ':');
        char *deg_c = strstr(line_buf, "C");

        if (colon != NULL && deg_c != NULL) {
            char label[64];
            int whole = 0, frac = 0;
            size_t label_len = (size_t)(colon - line_buf);
            
            if (label_len > 63) label_len = 63;
            strncpy(label, line_buf, label_len);
            label[label_len] = '\0';

            if (sscanf(colon + 1, " %d.%d", &whole, &frac) >= 1) {
                long temp_milli = (long)whole * 1000;
                temp_milli += (whole < 0) ? -(long)frac * 100 : (long)frac * 100;
                print_formatted_temp(label, temp_milli);
            }
        }
    }
    pclose(pipe);
}

int main(int argc, char *argv[]) {
    int i;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "--fahrenheit") == 0 || strcmp(argv[i], "-f") == 0) {
                config_use_fahrenheit = 1;
            } else if (strcmp(argv[i], "--lm-sensors") == 0 || strcmp(argv[i], "-l") == 0) {
                config_use_lmsensors = 1;
            } else if (strcmp(argv[i], "--debug") == 0 || strcmp(argv[i], "-d") == 0) {
                config_debug = 1;
            } else if (strcmp(argv[i], "--dry-run") == 0 || strcmp(argv[i], "-t") == 0) {
                config_dry_run = 1;
            } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
                print_help(argv[0]);
                return 0;
            } else if (strcmp(argv[i], "--about") == 0 || strcmp(argv[i], "-a") == 0) {
                print_about();
                return 0;
            } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
                printf("%s %s\n", APP_NAME, APP_VERSION);
                return 0;
            } else if (strcmp(argv[i], "--license") == 0) {
                print_license();
                return 0;
            } else {
                fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
                fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
                return 1;
            }
        }
    }

    if (config_debug) {
        printf("[DEBUG] Configuration: Fahrenheit=%d, lm-sensors=%d, Dry-Run=%d\n", 
               config_use_fahrenheit, config_use_lmsensors, config_dry_run);
    }

    probe_hwmon_sensors();
    if (config_use_lmsensors) {
        probe_lmsensors();
    }
    
    return 0;
}
