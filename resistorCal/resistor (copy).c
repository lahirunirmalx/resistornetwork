#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_N 5           // maximum number of resistors allowed in the network
#define MAX_NETWORKS 10000 // maximum networks per count (you can adjust this as needed)
#define MAX_EXPR 256      // maximum length of the expression string

// Structure to hold a network configuration.
typedef struct {
    double R;            // equivalent resistance (ohms)
    int n;               // number of resistors used in this network
    char expr[MAX_EXPR]; // expression describing the network
} Network;

int main(void) {
    // List of available resistor values (in ohms)
    double available[] = {
         1, 7.5, 10, 18, 22, 24, 39, 47, 68, 75,
       100, 120, 130, 150, 200, 220, 330, 360, 470, 560,
       820, 1000, 1200, 1800, 2200, 2700, 3000, 3600, 3900, 4700,
       5600, 6800, 8200, 10000, 12000, 18000, 22000, 33000, 39000, 47000,
       56000, 68000, 100000, 120000, 130000, 150000, 180000, 220000, 270000, 330000,
       390000, 470000, 510000, 560000, 680000, 750000, 1000000, 2700000, 3000000, 3300000,
       4700000, 10000000
    };
    int numAvail = sizeof(available) / sizeof(available[0]);

    double target, tolPerc, tol;
    printf("Enter target resistance (ohms): ");
    if (scanf("%lf", &target) != 1) {
        fprintf(stderr, "Invalid input.\n");
        return 1;
    }
    printf("Enter tolerance (percentage, e.g., 5 for 5%%): ");
    if (scanf("%lf", &tolPerc) != 1) {
        fprintf(stderr, "Invalid input.\n");
        return 1;
    }
    tol = tolPerc / 100.0;

    // Dynamically allocate an array of pointers for each network count (1 to MAX_N)
    Network **networks = malloc((MAX_N+1) * sizeof(Network *));
    if (!networks) {
        fprintf(stderr, "Memory allocation error.\n");
        return 1;
    }
    for (int i = 0; i <= MAX_N; i++) {
        networks[i] = malloc(MAX_NETWORKS * sizeof(Network));
        if (!networks[i]) {
            fprintf(stderr, "Memory allocation error.\n");
            return 1;
        }
    }
    int count[MAX_N+1] = {0}; // count[i] = number of networks using i resistors

    // For networks using 1 resistor: simply each available resistor.
    for (int i = 0; i < numAvail; i++) {
        if (count[1] < MAX_NETWORKS) {
            networks[1][count[1]].R = available[i];
            networks[1][count[1]].n = 1;
            // Store the resistor value as a string.
            snprintf(networks[1][count[1]].expr, MAX_EXPR, "%.2f", available[i]);
            count[1]++;
        }
    }

    // For networks with 2 to MAX_N resistors, combine smaller networks.
    for (int n = 2; n <= MAX_N; n++) {
        count[n] = 0;
        // Partition the network into two sub-networks of sizes i and n-i.
        for (int i = 1; i < n; i++) {
            int j = n - i;
            for (int a = 0; a < count[i]; a++) {
                for (int b = 0; b < count[j]; b++) {
                    char expr[MAX_EXPR];

                    // Series combination: R = A + B
                    if (count[n] < MAX_NETWORKS) {
                        double R_series = networks[i][a].R + networks[j][b].R;
                        snprintf(expr, MAX_EXPR, "(%s + %s)", networks[i][a].expr, networks[j][b].expr);
                        networks[n][count[n]].R = R_series;
                        networks[n][count[n]].n = networks[i][a].n + networks[j][b].n;
                        strncpy(networks[n][count[n]].expr, expr, MAX_EXPR);
                        count[n]++;
                    }

                    // Parallel combination: R = 1 / (1/A + 1/B)
                    if (networks[i][a].R > 0 && networks[j][b].R > 0 && count[n] < MAX_NETWORKS) {
                        double R_parallel = 1.0 / ((1.0 / networks[i][a].R) + (1.0 / networks[j][b].R));
                        snprintf(expr, MAX_EXPR, "(%s || %s)", networks[i][a].expr, networks[j][b].expr);
                        networks[n][count[n]].R = R_parallel;
                        networks[n][count[n]].n = networks[i][a].n + networks[j][b].n;
                        strncpy(networks[n][count[n]].expr, expr, MAX_EXPR);
                        count[n]++;
                    }
                }
            }
        }
    }

    // Search through all networks (from 1 to MAX_N resistors) and print those within tolerance.
    int found = 0;
    printf("\n-- Networks within %.2f%% tolerance of %.2f ohm --\n", tolPerc, target);
    for (int n = 1; n <= MAX_N; n++) {
        for (int i = 0; i < count[n]; i++) {
            double relError = fabs(networks[n][i].R - target) / target;
            if (relError <= tol) {
                printf("Using %d resistor%s: %s = %.2f ohm (error %.2f%%)\n",
                       networks[n][i].n,
                       networks[n][i].n > 1 ? "s" : "",
                       networks[n][i].expr,
                       networks[n][i].R,
                       relError * 100);
                found = 1;
            }
        }
    }

    if (!found) {
        printf("No network found within the specified tolerance.\n");
    }

    // Free allocated memory.
    for (int i = 0; i <= MAX_N; i++) {
        free(networks[i]);
    }
    free(networks);

    return 0;
}
