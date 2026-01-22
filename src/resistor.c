/*
 * resistor.c - Resistor Network Calculator
 *
 * Finds series/parallel combinations of standard resistors
 * to achieve a target resistance within specified tolerance.
 *
 * SPDX-License-Identifier: MIT
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef PLATFORM_MACOS
#include <CoreFoundation/CoreFoundation.h>
#endif

#define MAX_N 5           /* maximum resistors in a network */
#define MAX_NETWORKS 10000
#define MAX_EXPR 256
#define MAX_RESULTS 50    /* max results to display */
#define TOP_N_CODES 5     /* show color codes for top N results */

/* DATADIR is set by cmake at compile time */
#ifndef DATADIR
#define DATADIR "."
#endif

#define MAX_RESISTORS_PER_NET 8  /* max individual resistors tracked */

typedef struct {
    double R;                      /* equivalent resistance (ohms) */
    int n;                         /* number of resistors used */
    char expr[MAX_EXPR];           /* text expression of the network */
    double parts[MAX_RESISTORS_PER_NET]; /* individual resistor values */
    int num_parts;                 /* count of parts */
} Network;

typedef struct {
    double R;                      /* equivalent resistance */
    double error;                  /* relative error (0-1) */
    int n;                         /* number of resistors */
    char expr[MAX_EXPR];           /* expression */
    double parts[MAX_RESISTORS_PER_NET]; /* individual resistor values */
    int num_parts;                 /* count of parts */
} Result;

/* Comparison function for qsort - sort by error ascending */
static int compare_results(const void *a, const void *b)
{
    const Result *ra = (const Result *)a;
    const Result *rb = (const Result *)b;
    if (ra->error < rb->error) return -1;
    if (ra->error > rb->error) return 1;
    /* Secondary sort: fewer resistors first */
    return ra->n - rb->n;
}

static GtkBuilder *builder = NULL;

/* ========================================================================
 * RESISTOR COLOR CODE TABLES
 * ======================================================================== */

static const char *color_names[] = {
    "Black", "Brown", "Red", "Orange", "Yellow",
    "Green", "Blue", "Violet", "Grey", "White"
};

static const char *multiplier_colors[] = {
    "Black",   /* 10^0 = 1 */
    "Brown",   /* 10^1 = 10 */
    "Red",     /* 10^2 = 100 */
    "Orange",  /* 10^3 = 1K */
    "Yellow",  /* 10^4 = 10K */
    "Green",   /* 10^5 = 100K */
    "Blue",    /* 10^6 = 1M */
    "Violet",  /* 10^7 = 10M */
    "Grey",    /* 10^8 = 100M */
    "White"    /* 10^9 = 1G */
};

/*
 * Generate 4-band color code.
 * Format: [digit1][digit2][multiplier][tolerance]
 * Tolerance assumed 5% (Gold) for standard resistors.
 *
 * Returns: static buffer with color code string
 */
static const char *get_4band_code(double ohms)
{
    static char buf[128];
    double normalized;
    int sig2;  /* 2 significant digits */
    int exp10; /* power of 10 multiplier */
    int d1, d2;

    if (ohms <= 0) {
        snprintf(buf, sizeof(buf), "(invalid)");
        return buf;
    }

    /* Normalize to 2 significant digits: find sig2 * 10^exp10 = ohms */
    exp10 = (int)floor(log10(ohms)) - 1;
    if (exp10 < 0) exp10 = 0;  /* sub-ohm: clamp */
    if (exp10 > 9) exp10 = 9;  /* over 10G: clamp */

    normalized = ohms / pow(10, exp10);
    sig2 = (int)round(normalized);

    /* Handle rounding edge cases */
    if (sig2 >= 100) {
        sig2 /= 10;
        exp10++;
    }
    if (sig2 < 10) {
        sig2 *= 10;
        exp10--;
    }
    if (exp10 < 0) exp10 = 0;
    if (exp10 > 9) exp10 = 9;

    d1 = sig2 / 10;
    d2 = sig2 % 10;

    snprintf(buf, sizeof(buf), "%s-%s-%s-Gold",
             color_names[d1], color_names[d2], multiplier_colors[exp10]);
    return buf;
}

/*
 * Generate 5-band color code.
 * Format: [digit1][digit2][digit3][multiplier][tolerance]
 * Tolerance assumed 1% (Brown) for precision resistors.
 */
static const char *get_5band_code(double ohms)
{
    static char buf[128];
    double normalized;
    int sig3;  /* 3 significant digits */
    int exp10;
    int d1, d2, d3;

    if (ohms <= 0) {
        snprintf(buf, sizeof(buf), "(invalid)");
        return buf;
    }

    /* Normalize to 3 significant digits */
    exp10 = (int)floor(log10(ohms)) - 2;
    if (exp10 < 0) exp10 = 0;
    if (exp10 > 9) exp10 = 9;

    normalized = ohms / pow(10, exp10);
    sig3 = (int)round(normalized);

    /* Handle rounding edge cases */
    if (sig3 >= 1000) {
        sig3 /= 10;
        exp10++;
    }
    if (sig3 < 100) {
        sig3 *= 10;
        exp10--;
    }
    if (exp10 < 0) exp10 = 0;
    if (exp10 > 9) exp10 = 9;

    d1 = sig3 / 100;
    d2 = (sig3 / 10) % 10;
    d3 = sig3 % 10;

    snprintf(buf, sizeof(buf), "%s-%s-%s-%s-Brown",
             color_names[d1], color_names[d2], color_names[d3],
             multiplier_colors[exp10]);
    return buf;
}

/*
 * Generate SMD code (3-digit format).
 * 3-digit: XYZ = XY × 10^Z (e.g., 103 = 10K)
 * Also includes R notation for sub-10 ohm (e.g., 4R7 = 4.7Ω)
 */
static const char *get_smd_code(double ohms)
{
    static char buf[64];
    double normalized;
    int sig, exp10;

    if (ohms <= 0) {
        snprintf(buf, sizeof(buf), "(invalid)");
        return buf;
    }

    /* Handle sub-10 ohm with R notation */
    if (ohms < 10.0) {
        int whole = (int)ohms;
        int frac = (int)round((ohms - whole) * 10) % 10;
        snprintf(buf, sizeof(buf), "%dR%d", whole, frac);
        return buf;
    }

    /* 3-digit code: 2 significant figures */
    exp10 = (int)floor(log10(ohms)) - 1;
    if (exp10 < 0) exp10 = 0;
    if (exp10 > 9) exp10 = 9;

    normalized = ohms / pow(10, exp10);
    sig = (int)round(normalized);
    if (sig >= 100) { sig /= 10; exp10++; }
    if (sig < 10) { sig *= 10; exp10--; }
    if (exp10 < 0) exp10 = 0;

    snprintf(buf, sizeof(buf), "%d%d", sig, exp10);
    return buf;
}

/*
 * Format color code info for a single resistor value.
 */
static void append_single_color_code(GString *str, double ohms)
{
    g_string_append_printf(str, "      %.2f Ω: 4-band: %s | 5-band: %s | SMD: %s\n",
                           ohms, get_4band_code(ohms), get_5band_code(ohms), get_smd_code(ohms));
}

/*
 * Format color codes for all individual resistors in a result.
 * Shows codes for each unique resistor value used.
 */
static void append_result_color_codes(GString *str, const Result *res)
{
    int p;
    double seen[MAX_RESISTORS_PER_NET];
    int num_seen = 0;
    int already_shown;

    g_string_append(str, "    Component resistor codes:\n");
    
    for (p = 0; p < res->num_parts; p++) {
        int s;
        /* Check if we've already shown this value */
        already_shown = 0;
        for (s = 0; s < num_seen; s++) {
            if (fabs(seen[s] - res->parts[p]) < 0.01) {
                already_shown = 1;
                break;
            }
        }
        if (!already_shown) {
            append_single_color_code(str, res->parts[p]);
            if (num_seen < MAX_RESISTORS_PER_NET)
                seen[num_seen++] = res->parts[p];
        }
    }
}

/* ========================================================================
 * RESISTOR VALUE PARSING
 * ======================================================================== */

/*
 * Parse resistor value from label string.
 * Handles: "1 Ω", "7.5 Ω", "1K Ω", "1M Ω"
 */
static double parse_resistor_value(const char *label)
{
    double value = 0;
    char suffix[10] = "";

    if (sscanf(label, "%lf%9s", &value, suffix) >= 1) {
        if (strchr(suffix, 'K') || strchr(suffix, 'k'))
            value *= 1000;
        else if (strchr(suffix, 'M') || strchr(suffix, 'm'))
            value *= 1000000;
    }
    return value;
}

/* ========================================================================
 * NETWORK CALCULATION
 * ======================================================================== */

/*
 * Main calculation - builds all possible series/parallel networks
 * and finds those within tolerance of target.
 */
static void on_calculate_clicked(GtkButton *button, gpointer user_data)
{
    GtkWidget *entry_target, *combo_tol, *textview_output, *grid_resistors;
    GList *children, *l;
    double available[100];
    int numAvail = 0;
    double target, tolPerc, tol;
    const char *target_text;
    gchar *tol_text = NULL;
    Network **networks = NULL;
    int count[MAX_N + 1] = {0};
    GString *result;
    int found = 0;
    int i, n, a, b, j_idx;

    (void)button;
    (void)user_data;

    entry_target    = GTK_WIDGET(gtk_builder_get_object(builder, "entry_target"));
    combo_tol       = GTK_WIDGET(gtk_builder_get_object(builder, "combo_tolPerc"));
    textview_output = GTK_WIDGET(gtk_builder_get_object(builder, "textview_output"));
    grid_resistors  = GTK_WIDGET(gtk_builder_get_object(builder, "grid_resistors"));

    /* Collect selected resistor values */
    children = gtk_container_get_children(GTK_CONTAINER(grid_resistors));
    for (l = children; l != NULL; l = l->next) {
        GtkWidget *widget = GTK_WIDGET(l->data);
        if (GTK_IS_CHECK_BUTTON(widget)) {
            if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
                const char *label = gtk_button_get_label(GTK_BUTTON(widget));
                available[numAvail++] = parse_resistor_value(label);
                if (numAvail >= 100)
                    break;
            }
        }
    }
    g_list_free(children);

    target_text = gtk_entry_get_text(GTK_ENTRY(entry_target));
    target = atof(target_text);

    /* FIX: Validate target before division */
    if (target <= 0) {
        GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview_output));
        gtk_text_buffer_set_text(buf, "Error: Target resistance must be greater than 0", -1);
        return;
    }

    if (numAvail == 0) {
        GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview_output));
        gtk_text_buffer_set_text(buf, "Error: Select at least one resistor value", -1);
        return;
    }

    /* FIX: Free allocated string from combo box */
    tol_text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo_tol));
    tolPerc = tol_text ? atof(tol_text) : 5.0;
    g_free(tol_text);
    tol = tolPerc / 100.0;

    /* FIX: Initialize all pointers to NULL before allocation */
    networks = malloc((MAX_N + 1) * sizeof(Network *));
    if (!networks) {
        g_printerr("Memory allocation failed\n");
        return;
    }
    for (i = 0; i <= MAX_N; i++)
        networks[i] = NULL;

    for (i = 0; i <= MAX_N; i++) {
        networks[i] = malloc(MAX_NETWORKS * sizeof(Network));
        if (!networks[i]) {
            g_printerr("Memory allocation failed\n");
            goto cleanup;
        }
    }

    /* Base case: single resistor networks */
    for (i = 0; i < numAvail; i++) {
        if (count[1] < MAX_NETWORKS) {
            networks[1][count[1]].R = available[i];
            networks[1][count[1]].n = 1;
            snprintf(networks[1][count[1]].expr, MAX_EXPR, "%.2f", available[i]);
            /* Track individual resistor value */
            networks[1][count[1]].parts[0] = available[i];
            networks[1][count[1]].num_parts = 1;
            count[1]++;
        }
    }

    /* Build networks with 2..MAX_N resistors */
    for (n = 2; n <= MAX_N; n++) {
        count[n] = 0;
        for (i = 1; i < n; i++) {
            j_idx = n - i;
            /*
             * FIX: Avoid duplicates by only combining when i <= j_idx
             * For i == j_idx, only combine when a <= b
             */
            for (a = 0; a < count[i]; a++) {
                int b_start = (i == j_idx) ? a : 0;
                for (b = b_start; b < count[j_idx]; b++) {
                    char expr[MAX_EXPR];
                    double R_series, R_parallel;
                    int p, total_parts;

                    /* Merge parts arrays */
                    total_parts = networks[i][a].num_parts + networks[j_idx][b].num_parts;
                    if (total_parts > MAX_RESISTORS_PER_NET)
                        total_parts = MAX_RESISTORS_PER_NET;

                    /* Series: R = A + B */
                    if (count[n] < MAX_NETWORKS) {
                        R_series = networks[i][a].R + networks[j_idx][b].R;
                        snprintf(expr, MAX_EXPR, "(%s + %s)",
                                 networks[i][a].expr, networks[j_idx][b].expr);
                        networks[n][count[n]].R = R_series;
                        networks[n][count[n]].n = networks[i][a].n + networks[j_idx][b].n;
                        strncpy(networks[n][count[n]].expr, expr, MAX_EXPR - 1);
                        networks[n][count[n]].expr[MAX_EXPR - 1] = '\0';
                        /* Copy parts from both networks */
                        networks[n][count[n]].num_parts = 0;
                        for (p = 0; p < networks[i][a].num_parts && networks[n][count[n]].num_parts < MAX_RESISTORS_PER_NET; p++)
                            networks[n][count[n]].parts[networks[n][count[n]].num_parts++] = networks[i][a].parts[p];
                        for (p = 0; p < networks[j_idx][b].num_parts && networks[n][count[n]].num_parts < MAX_RESISTORS_PER_NET; p++)
                            networks[n][count[n]].parts[networks[n][count[n]].num_parts++] = networks[j_idx][b].parts[p];
                        count[n]++;
                    }

                    /* Parallel: R = 1/(1/A + 1/B) - use Unicode ∥ symbol */
                    if (networks[i][a].R > 0 && networks[j_idx][b].R > 0 &&
                        count[n] < MAX_NETWORKS) {
                        R_parallel = 1.0 / ((1.0 / networks[i][a].R) +
                                            (1.0 / networks[j_idx][b].R));
                        snprintf(expr, MAX_EXPR, "(%s ∥ %s)",
                                 networks[i][a].expr, networks[j_idx][b].expr);
                        networks[n][count[n]].R = R_parallel;
                        networks[n][count[n]].n = networks[i][a].n + networks[j_idx][b].n;
                        strncpy(networks[n][count[n]].expr, expr, MAX_EXPR - 1);
                        networks[n][count[n]].expr[MAX_EXPR - 1] = '\0';
                        /* Copy parts from both networks */
                        networks[n][count[n]].num_parts = 0;
                        for (p = 0; p < networks[i][a].num_parts && networks[n][count[n]].num_parts < MAX_RESISTORS_PER_NET; p++)
                            networks[n][count[n]].parts[networks[n][count[n]].num_parts++] = networks[i][a].parts[p];
                        for (p = 0; p < networks[j_idx][b].num_parts && networks[n][count[n]].num_parts < MAX_RESISTORS_PER_NET; p++)
                            networks[n][count[n]].parts[networks[n][count[n]].num_parts++] = networks[j_idx][b].parts[p];
                        count[n]++;
                    }
                }
            }
        }
    }

    /* Collect all networks within tolerance into results array */
    Result *results = malloc(MAX_NETWORKS * sizeof(Result));
    int num_results = 0;
    
    if (!results) {
        g_printerr("Memory allocation failed for results\n");
        goto cleanup;
    }

    for (n = 1; n <= MAX_N; n++) {
        for (i = 0; i < count[n]; i++) {
            double relError = fabs(networks[n][i].R - target) / target;
            if (relError <= tol && num_results < MAX_NETWORKS) {
                int p;
                results[num_results].R = networks[n][i].R;
                results[num_results].error = relError;
                results[num_results].n = networks[n][i].n;
                strncpy(results[num_results].expr, networks[n][i].expr, MAX_EXPR - 1);
                results[num_results].expr[MAX_EXPR - 1] = '\0';
                /* Copy individual parts */
                results[num_results].num_parts = networks[n][i].num_parts;
                for (p = 0; p < networks[n][i].num_parts; p++)
                    results[num_results].parts[p] = networks[n][i].parts[p];
                num_results++;
            }
        }
    }

    /* Sort results by error (ascending) */
    if (num_results > 0) {
        qsort(results, num_results, sizeof(Result), compare_results);
    }

    /* Build output string */
    result = g_string_new("");
    g_string_append_printf(result,
        "\n-- Networks within %.2f%% tolerance of %.2f Ω --\n",
        tolPerc, target);
    g_string_append_printf(result,
        "   Found %d combinations, showing top %d sorted by error\n\n",
        num_results, num_results < MAX_RESULTS ? num_results : MAX_RESULTS);

    for (i = 0; i < num_results && i < MAX_RESULTS; i++) {
        /* Show rank for top 5 */
        if (i < TOP_N_CODES) {
            g_string_append_printf(result, "#%d ", i + 1);
        }
        
        g_string_append_printf(result,
            "%s = %.2f Ω (%d resistor%s, error %.2f%%)\n",
            results[i].expr,
            results[i].R,
            results[i].n,
            results[i].n > 1 ? "s" : "",
            results[i].error * 100);

        /* Show color codes for individual resistors in top 5 results */
        if (i < TOP_N_CODES) {
            append_result_color_codes(result, &results[i]);
        }
        g_string_append(result, "\n");
        found = 1;
    }

    if (num_results > MAX_RESULTS) {
        g_string_append_printf(result, "... and %d more results\n\n",
                               num_results - MAX_RESULTS);
    }

    free(results);

    if (!found)
        g_string_append(result, "No network found within the specified tolerance.\n");

    /* Add color code legend */
    g_string_append(result, "\n-- Color Code Reference --\n");
    g_string_append(result, "Digits: Black=0, Brown=1, Red=2, Orange=3, Yellow=4\n");
    g_string_append(result, "        Green=5, Blue=6, Violet=7, Grey=8, White=9\n");
    g_string_append(result, "Tolerance: Gold=5%, Brown=1%, Red=2%, Silver=10%\n");

    gtk_text_buffer_set_text(
        gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview_output)),
        result->str, -1);
    g_string_free(result, TRUE);

cleanup:
    if (networks) {
        for (i = 0; i <= MAX_N; i++)
            free(networks[i]);  /* free(NULL) is safe */
        free(networks);
    }
}

/* ========================================================================
 * UI LOADING
 * ======================================================================== */

/*
 * Get the directory containing the executable.
 * Used to find resources relative to the binary.
 */
static void get_exe_dir(char *buf, size_t bufsize, const char *argv0)
{
#ifdef PLATFORM_MACOS
    /* macOS: Use bundle path */
    CFBundleRef bundle = CFBundleGetMainBundle();
    if (bundle) {
        CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(bundle);
        if (resourcesURL) {
            CFURLGetFileSystemRepresentation(resourcesURL, TRUE,
                                             (UInt8 *)buf, bufsize);
            CFRelease(resourcesURL);
            return;
        }
    }
#endif

    /* Fallback: derive from argv[0] */
    const char *last_slash = strrchr(argv0, '/');
#ifdef PLATFORM_WINDOWS
    const char *last_bslash = strrchr(argv0, '\\');
    if (last_bslash > last_slash)
        last_slash = last_bslash;
#endif
    if (last_slash) {
        size_t len = last_slash - argv0;
        if (len >= bufsize) len = bufsize - 1;
        memcpy(buf, argv0, len);
        buf[len] = '\0';
    } else {
        strncpy(buf, ".", bufsize - 1);
        buf[bufsize - 1] = '\0';
    }
}

/*
 * Try to load UI file from multiple locations:
 * 1. DATADIR (set at compile time, e.g., /usr/share/resistorcal)
 * 2. Current directory (for development)
 * 3. Alongside the executable
 * 4. macOS: Inside app bundle Resources
 */
static gboolean load_ui(GtkBuilder *bldr, const char *argv0)
{
    char path[1024];
    char exe_dir[1024];
    const char *locations[] = {
        DATADIR "/ui.glade",
        "ui.glade",
        "data/ui.glade",
        NULL
    };
    int i;

    /* Try standard locations first */
    for (i = 0; locations[i] != NULL; i++) {
        if (gtk_builder_add_from_file(bldr, locations[i], NULL) != 0)
            return TRUE;
    }

    /* Get directory of executable */
    get_exe_dir(exe_dir, sizeof(exe_dir), argv0);

    /* Try in exe directory (Windows portable, macOS bundle) */
    snprintf(path, sizeof(path), "%s/ui.glade", exe_dir);
    if (gtk_builder_add_from_file(bldr, path, NULL) != 0)
        return TRUE;

    /* Try Linux install location relative to exe */
    snprintf(path, sizeof(path), "%s/../share/resistorcal/ui.glade", exe_dir);
    if (gtk_builder_add_from_file(bldr, path, NULL) != 0)
        return TRUE;

    return FALSE;
}

/* ========================================================================
 * MAIN
 * ======================================================================== */

int main(int argc, char *argv[])
{
    GtkWidget *window, *btn;

    gtk_init(&argc, &argv);

    builder = gtk_builder_new();
    if (!load_ui(builder, argv[0])) {
        g_printerr("Error: Cannot find ui.glade\n");
        g_printerr("Searched in: %s, current dir, data/\n", DATADIR);
        return 1;
    }

    window = GTK_WIDGET(gtk_builder_get_object(builder, "window1"));
    if (!window) {
        g_printerr("Error: Cannot find window1 in UI file\n");
        g_object_unref(builder);
        return 1;
    }

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_builder_connect_signals(builder, NULL);

    btn = GTK_WIDGET(gtk_builder_get_object(builder, "button_calculate"));
    if (btn)
        g_signal_connect(btn, "clicked", G_CALLBACK(on_calculate_clicked), NULL);

    gtk_widget_show_all(window);
    gtk_main();

    g_object_unref(builder);
    return 0;
}
