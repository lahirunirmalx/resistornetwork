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
#define MAX_R2R_BITS 24   /* max bits for R-2R ladder */

/* E24 series base values */
static const double E24_BASE[] = {
    1.0, 1.1, 1.2, 1.3, 1.5, 1.6, 1.8, 2.0, 2.2, 2.4, 2.7, 3.0,
    3.3, 3.6, 3.9, 4.3, 4.7, 5.1, 5.6, 6.2, 6.8, 7.5, 8.2, 9.1
};
#define E24_COUNT 24
#define E24_DECADES 7   /* 1 to 1M */

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

/* Hex color values for visual display */
static const char *color_hex[] = {
    "#000000", "#8B4513", "#FF0000", "#FFA500", "#FFFF00",
    "#008000", "#0000FF", "#8B00FF", "#808080", "#FFFFFF"
};
#define GOLD_HEX "#FFD700"
#define SILVER_HEX "#C0C0C0"

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
 * Create color tags in a text buffer for colored output.
 */
static void create_color_tags(GtkTextBuffer *buffer)
{
    int i;
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buffer);
    
    for (i = 0; i < 10; i++) {
        if (!gtk_text_tag_table_lookup(table, color_names[i])) {
            gtk_text_buffer_create_tag(buffer, color_names[i],
                                       "background", color_hex[i],
                                       "foreground", (i == 0 || i == 6 || i == 7) ? "#FFFFFF" : "#000000",
                                       NULL);
        }
    }
    if (!gtk_text_tag_table_lookup(table, "Gold")) {
        gtk_text_buffer_create_tag(buffer, "Gold",
                                   "background", GOLD_HEX,
                                   "foreground", "#000000",
                                   NULL);
    }
    if (!gtk_text_tag_table_lookup(table, "Silver")) {
        gtk_text_buffer_create_tag(buffer, "Silver",
                                   "background", SILVER_HEX,
                                   "foreground", "#000000",
                                   NULL);
    }
}

/*
 * Insert a colored box with text into the buffer.
 */
static void insert_color_box(GtkTextBuffer *buffer, GtkTextIter *iter, const char *color_name)
{
    char box_text[32];
    snprintf(box_text, sizeof(box_text), " %s ", color_name);
    gtk_text_buffer_insert_with_tags_by_name(buffer, iter, box_text, -1, color_name, NULL);
}

/*
 * Insert 4-band color code with visual boxes.
 */
static void insert_4band_visual(GtkTextBuffer *buffer, GtkTextIter *iter, double ohms)
{
    double normalized;
    int sig2, exp10, d1, d2;
    
    if (ohms <= 0) {
        gtk_text_buffer_insert(buffer, iter, "(invalid)", -1);
        return;
    }

    exp10 = (int)floor(log10(ohms)) - 1;
    if (exp10 < 0) exp10 = 0;
    if (exp10 > 9) exp10 = 9;
    normalized = ohms / pow(10, exp10);
    sig2 = (int)round(normalized);
    if (sig2 >= 100) { sig2 /= 10; exp10++; }
    if (sig2 < 10) { sig2 *= 10; exp10--; }
    if (exp10 < 0) exp10 = 0;
    if (exp10 > 9) exp10 = 9;
    d1 = sig2 / 10;
    d2 = sig2 % 10;

    gtk_text_buffer_insert(buffer, iter, "4-band: ", -1);
    insert_color_box(buffer, iter, color_names[d1]);
    insert_color_box(buffer, iter, color_names[d2]);
    insert_color_box(buffer, iter, multiplier_colors[exp10]);
    insert_color_box(buffer, iter, "Gold");
}

/*
 * Insert 5-band color code with visual boxes.
 */
static void insert_5band_visual(GtkTextBuffer *buffer, GtkTextIter *iter, double ohms)
{
    double normalized;
    int sig3, exp10, d1, d2, d3;
    
    if (ohms <= 0) {
        gtk_text_buffer_insert(buffer, iter, "(invalid)", -1);
        return;
    }

    exp10 = (int)floor(log10(ohms)) - 2;
    if (exp10 < 0) exp10 = 0;
    if (exp10 > 9) exp10 = 9;
    normalized = ohms / pow(10, exp10);
    sig3 = (int)round(normalized);
    if (sig3 >= 1000) { sig3 /= 10; exp10++; }
    if (sig3 < 100) { sig3 *= 10; exp10--; }
    if (exp10 < 0) exp10 = 0;
    if (exp10 > 9) exp10 = 9;
    d1 = sig3 / 100;
    d2 = (sig3 / 10) % 10;
    d3 = sig3 % 10;

    gtk_text_buffer_insert(buffer, iter, "5-band: ", -1);
    insert_color_box(buffer, iter, color_names[d1]);
    insert_color_box(buffer, iter, color_names[d2]);
    insert_color_box(buffer, iter, color_names[d3]);
    insert_color_box(buffer, iter, multiplier_colors[exp10]);
    insert_color_box(buffer, iter, "Brown");
}

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

/* ========================================================================
 * FORMATTING HELPERS
 * ======================================================================== */

/*
 * Format resistance value with appropriate suffix (Ω, KΩ, MΩ)
 */
static void format_resistance(double ohms, char *buf, size_t bufsize)
{
    if (ohms >= 1e6)
        snprintf(buf, bufsize, "%.1fMΩ", ohms / 1e6);
    else if (ohms >= 1e3)
        snprintf(buf, bufsize, "%.1fKΩ", ohms / 1e3);
    else
        snprintf(buf, bufsize, "%.1fΩ", ohms);
}

/*
 * Format LSB voltage with appropriate unit (V, mV, µV, nV)
 */
static void format_lsb(double volts, char *buf, size_t bufsize)
{
    if (volts >= 1.0)
        snprintf(buf, bufsize, "%.3fV", volts);
    else if (volts >= 0.001)
        snprintf(buf, bufsize, "%.2fmV", volts * 1000);
    else if (volts >= 0.000001)
        snprintf(buf, bufsize, "%.2fµV", volts * 1e6);
    else
        snprintf(buf, bufsize, "%.2fnV", volts * 1e9);
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

    /* Get text buffer and create color tags */
    {
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview_output));
        GtkTextIter iter;
        char line[512];
        int p;
        double seen[MAX_RESISTORS_PER_NET];
        int num_seen;
        
        create_color_tags(buffer);
        gtk_text_buffer_set_text(buffer, "", -1);
        gtk_text_buffer_get_end_iter(buffer, &iter);

        /* Header */
        snprintf(line, sizeof(line),
            "\n-- Networks within %.2f%% tolerance of %.2f Ω --\n"
            "   Found %d combinations, showing top %d sorted by error\n\n",
            tolPerc, target,
            num_results, num_results < MAX_RESULTS ? num_results : MAX_RESULTS);
        gtk_text_buffer_insert(buffer, &iter, line, -1);

        for (i = 0; i < num_results && i < MAX_RESULTS; i++) {
            /* Show rank for top 5 */
            if (i < TOP_N_CODES) {
                snprintf(line, sizeof(line), "#%d ", i + 1);
                gtk_text_buffer_insert(buffer, &iter, line, -1);
            }
            
            snprintf(line, sizeof(line),
                "%s = %.2f Ω (%d resistor%s, error %.2f%%)\n",
                results[i].expr,
                results[i].R,
                results[i].n,
                results[i].n > 1 ? "s" : "",
                results[i].error * 100);
            gtk_text_buffer_insert(buffer, &iter, line, -1);

            /* Show color codes with visual boxes for top 5 results */
            if (i < TOP_N_CODES) {
                int already_shown;
                num_seen = 0;
                gtk_text_buffer_insert(buffer, &iter, "    Component resistor codes:\n", -1);
                
                for (p = 0; p < results[i].num_parts; p++) {
                    int s;
                    already_shown = 0;
                    for (s = 0; s < num_seen; s++) {
                        if (fabs(seen[s] - results[i].parts[p]) < 0.01) {
                            already_shown = 1;
                            break;
                        }
                    }
                    if (!already_shown) {
                        snprintf(line, sizeof(line), "      %.2f Ω: ", results[i].parts[p]);
                        gtk_text_buffer_insert(buffer, &iter, line, -1);
                        insert_4band_visual(buffer, &iter, results[i].parts[p]);
                        gtk_text_buffer_insert(buffer, &iter, "\n              ", -1);
                        insert_5band_visual(buffer, &iter, results[i].parts[p]);
                        snprintf(line, sizeof(line), " | SMD: %s\n", get_smd_code(results[i].parts[p]));
                        gtk_text_buffer_insert(buffer, &iter, line, -1);
                        if (num_seen < MAX_RESISTORS_PER_NET)
                            seen[num_seen++] = results[i].parts[p];
                    }
                }
            }
            gtk_text_buffer_insert(buffer, &iter, "\n", -1);
            found = 1;
        }

        if (num_results > MAX_RESULTS) {
            snprintf(line, sizeof(line), "... and %d more results\n\n",
                     num_results - MAX_RESULTS);
            gtk_text_buffer_insert(buffer, &iter, line, -1);
        }

        if (!found)
            gtk_text_buffer_insert(buffer, &iter, "No network found within the specified tolerance.\n", -1);

        /* Add color code legend with visual boxes */
        gtk_text_buffer_insert(buffer, &iter, "\n-- Color Code Reference --\n", -1);
        gtk_text_buffer_insert(buffer, &iter, "Digits: ", -1);
        for (i = 0; i < 10; i++) {
            snprintf(line, sizeof(line), "%d=", i);
            gtk_text_buffer_insert(buffer, &iter, line, -1);
            insert_color_box(buffer, &iter, color_names[i]);
            gtk_text_buffer_insert(buffer, &iter, " ", -1);
        }
        gtk_text_buffer_insert(buffer, &iter, "\nTolerance: ", -1);
        insert_color_box(buffer, &iter, "Gold");
        gtk_text_buffer_insert(buffer, &iter, "=5% ", -1);
        insert_color_box(buffer, &iter, "Brown");
        gtk_text_buffer_insert(buffer, &iter, "=1% ", -1);
        insert_color_box(buffer, &iter, "Silver");
        gtk_text_buffer_insert(buffer, &iter, "=10%\n", -1);
    }
    
    free(results);

cleanup:
    if (networks) {
        for (i = 0; i <= MAX_N; i++)
            free(networks[i]);  /* free(NULL) is safe */
        free(networks);
    }
}

/* ========================================================================
 * R-2R LADDER CALCULATOR
 * ======================================================================== */

/*
 * Initialize the R value dropdown with E24 series across decades.
 */
static void init_r2r_dropdowns(void)
{
    GtkComboBoxText *combo_r, *combo_bits;
    int d, i, bit;
    char label[64];
    char r_str[32], r2_str[32];
    double r_val, multiplier;
    int default_idx = 0;

    combo_r = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "combo_r_value"));
    combo_bits = GTK_COMBO_BOX_TEXT(gtk_builder_get_object(builder, "combo_bits"));

    if (!combo_r || !combo_bits)
        return;

    /* Populate R values: E24 series across all decades */
    for (d = 0; d < E24_DECADES; d++) {
        multiplier = pow(10, d);
        for (i = 0; i < E24_COUNT; i++) {
            r_val = E24_BASE[i] * multiplier;
            format_resistance(r_val, r_str, sizeof(r_str));
            format_resistance(r_val * 2, r2_str, sizeof(r2_str));
            snprintf(label, sizeof(label), "%s → 2R = %s", r_str, r2_str);
            gtk_combo_box_text_append(combo_r, NULL, label);
            
            /* Default to 10K (index where r_val == 10000) */
            if (fabs(r_val - 10000) < 1)
                default_idx = d * E24_COUNT + i;
        }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_r), default_idx);

    /* Populate bits: 2 to 24 */
    for (bit = 2; bit <= MAX_R2R_BITS; bit++) {
        snprintf(label, sizeof(label), "%d-bit", bit);
        gtk_combo_box_text_append(combo_bits, NULL, label);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_bits), 6);  /* Default 8-bit */
}

/*
 * Get the R value from the dropdown index.
 */
static double get_r_value_from_index(int idx)
{
    int decade = idx / E24_COUNT;
    int base_idx = idx % E24_COUNT;
    return E24_BASE[base_idx] * pow(10, decade);
}

/*
 * R-2R Ladder generation callback.
 */
static void on_r2r_generate_clicked(GtkButton *button, gpointer user_data)
{
    GtkWidget *combo_r, *combo_bits, *entry_vref, *textview_output;
    GtkTextBuffer *buffer;
    GtkTextIter iter;
    int r_idx, bit_idx, bits;
    double R, R2, vref, lsb;
    int r_count, r2_count, total;
    const char *vref_text;
    char line[512];
    char r_str[32], r2_str[32], lsb_str[32];
    int i, num_samples;
    double max_val;

    (void)button;
    (void)user_data;

    combo_r = GTK_WIDGET(gtk_builder_get_object(builder, "combo_r_value"));
    combo_bits = GTK_WIDGET(gtk_builder_get_object(builder, "combo_bits"));
    entry_vref = GTK_WIDGET(gtk_builder_get_object(builder, "entry_vref"));
    textview_output = GTK_WIDGET(gtk_builder_get_object(builder, "textview_r2r_output"));

    if (!combo_r || !combo_bits || !entry_vref || !textview_output)
        return;

    r_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(combo_r));
    bit_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(combo_bits));
    bits = bit_idx + 2;  /* bits = 2 to 24, index 0 = 2-bit */
    R = get_r_value_from_index(r_idx);
    R2 = R * 2;

    vref_text = gtk_entry_get_text(GTK_ENTRY(entry_vref));
    vref = atof(vref_text);
    if (vref <= 0) vref = 5.0;

    /* Calculate component counts */
    r_count = bits - 1;    /* N-1 R resistors in horizontal chain */
    r2_count = bits + 1;   /* N+1 2R resistors (N bit legs + termination) */
    total = r_count + r2_count;

    max_val = pow(2, bits);
    lsb = vref / max_val;

    format_resistance(R, r_str, sizeof(r_str));
    format_resistance(R2, r2_str, sizeof(r2_str));
    format_lsb(lsb, lsb_str, sizeof(lsb_str));

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview_output));
    create_color_tags(buffer);
    gtk_text_buffer_set_text(buffer, "", -1);
    gtk_text_buffer_get_end_iter(buffer, &iter);

    /* Header */
    snprintf(line, sizeof(line),
        "\n══════════════════════════════════════════════════════════════\n"
        "                    R-2R LADDER DAC (%d-bit)\n"
        "══════════════════════════════════════════════════════════════\n\n",
        bits);
    gtk_text_buffer_insert(buffer, &iter, line, -1);

    /* Component summary */
    snprintf(line, sizeof(line),
        "COMPONENTS\n"
        "──────────────────────────────────────────────────────────────\n"
        "  R value:   %s\n"
        "  2R value:  %s\n"
        "  R count:   %d resistors\n"
        "  2R count:  %d resistors\n"
        "  Total:     %d resistors\n\n",
        r_str, r2_str, r_count, r2_count, total);
    gtk_text_buffer_insert(buffer, &iter, line, -1);

    /* Specifications */
    snprintf(line, sizeof(line),
        "SPECIFICATIONS\n"
        "──────────────────────────────────────────────────────────────\n"
        "  Vref:      %.2fV\n"
        "  LSB step:  %s\n"
        "  Levels:    %.0f (0 to %.0f)\n"
        "  Max Vout:  %.6fV\n\n",
        vref, lsb_str, max_val, max_val - 1, vref * (max_val - 1) / max_val);
    gtk_text_buffer_insert(buffer, &iter, line, -1);

    /* Color codes for R */
    gtk_text_buffer_insert(buffer, &iter, "RESISTOR COLOR CODES\n", -1);
    gtk_text_buffer_insert(buffer, &iter, "──────────────────────────────────────────────────────────────\n", -1);
    snprintf(line, sizeof(line), "  R (%s):  ", r_str);
    gtk_text_buffer_insert(buffer, &iter, line, -1);
    insert_4band_visual(buffer, &iter, R);
    snprintf(line, sizeof(line), " | SMD: %s\n", get_smd_code(R));
    gtk_text_buffer_insert(buffer, &iter, line, -1);

    snprintf(line, sizeof(line), "  2R (%s): ", r2_str);
    gtk_text_buffer_insert(buffer, &iter, line, -1);
    insert_4band_visual(buffer, &iter, R2);
    snprintf(line, sizeof(line), " | SMD: %s\n\n", get_smd_code(R2));
    gtk_text_buffer_insert(buffer, &iter, line, -1);

    /* Voltage table - show representative samples */
    gtk_text_buffer_insert(buffer, &iter, "SAMPLE OUTPUT VOLTAGES\n", -1);
    gtk_text_buffer_insert(buffer, &iter, "──────────────────────────────────────────────────────────────\n", -1);

    if (bits <= 12) {
        gtk_text_buffer_insert(buffer, &iter, "  Binary           Dec      Vout\n", -1);
    } else {
        gtk_text_buffer_insert(buffer, &iter, "  Hex          Decimal         Vout\n", -1);
    }
    gtk_text_buffer_insert(buffer, &iter, "  ─────────────────────────────────\n", -1);

    num_samples = (bits <= 4) ? (int)max_val : 16;
    for (i = 0; i < num_samples; i++) {
        int d;
        double voltage;
        char code_str[32];
        
        if (bits <= 4) {
            d = i;
        } else {
            /* Evenly spaced samples */
            d = (int)(i * (max_val - 1) / 15);
        }
        
        voltage = vref * d / max_val;
        
        if (bits <= 12) {
            /* Binary format for <= 12 bits */
            int b;
            char *p = code_str;
            for (b = bits - 1; b >= 0; b--) {
                *p++ = (d & (1 << b)) ? '1' : '0';
            }
            *p = '\0';
            snprintf(line, sizeof(line), "  %-14s %5d    %.6fV\n", code_str, d, voltage);
        } else {
            /* Hex format for > 12 bits */
            int hex_digits = (bits + 3) / 4;
            snprintf(code_str, sizeof(code_str), "0x%0*X", hex_digits, d);
            snprintf(line, sizeof(line), "  %-10s %10d    %.6fV\n", code_str, d, voltage);
        }
        gtk_text_buffer_insert(buffer, &iter, line, -1);
    }

    /* Ladder diagram (ASCII art) */
    gtk_text_buffer_insert(buffer, &iter, "\nLADDER DIAGRAM\n", -1);
    gtk_text_buffer_insert(buffer, &iter, "──────────────────────────────────────────────────────────────\n", -1);
    gtk_text_buffer_insert(buffer, &iter, "\n", -1);

    /* Draw compact ladder */
    gtk_text_buffer_insert(buffer, &iter, "  Vref ───┬───[2R]───GND (termination)\n", -1);
    gtk_text_buffer_insert(buffer, &iter, "          │\n", -1);

    /* Show first few bits, middle ellipsis, last few bits */
    if (bits <= 6) {
        for (i = bits - 1; i >= 0; i--) {
            snprintf(line, sizeof(line), "         [R]───┬───[2R]───B%d\n", i);
            gtk_text_buffer_insert(buffer, &iter, line, -1);
            if (i > 0)
                gtk_text_buffer_insert(buffer, &iter, "               │\n", -1);
        }
    } else {
        /* Show top 2 bits */
        for (i = bits - 1; i >= bits - 2; i--) {
            snprintf(line, sizeof(line), "         [R]───┬───[2R]───B%d (MSB%s)\n",
                     i, i == bits - 1 ? "" : "-1");
            gtk_text_buffer_insert(buffer, &iter, line, -1);
            gtk_text_buffer_insert(buffer, &iter, "               │\n", -1);
        }
        snprintf(line, sizeof(line), "              ...  (%d more stages)\n", bits - 4);
        gtk_text_buffer_insert(buffer, &iter, line, -1);
        gtk_text_buffer_insert(buffer, &iter, "               │\n", -1);
        /* Show bottom 2 bits */
        for (i = 1; i >= 0; i--) {
            snprintf(line, sizeof(line), "         [R]───┬───[2R]───B%d%s\n",
                     i, i == 0 ? " (LSB)" : "");
            gtk_text_buffer_insert(buffer, &iter, line, -1);
            if (i > 0)
                gtk_text_buffer_insert(buffer, &iter, "               │\n", -1);
        }
    }

    gtk_text_buffer_insert(buffer, &iter, "               │\n", -1);
    gtk_text_buffer_insert(buffer, &iter, "              Vout\n\n", -1);

    /* How it works */
    gtk_text_buffer_insert(buffer, &iter, "HOW R-2R LADDER WORKS\n", -1);
    gtk_text_buffer_insert(buffer, &iter, "──────────────────────────────────────────────────────────────\n", -1);
    gtk_text_buffer_insert(buffer, &iter,
        "  Each bit input (B0-Bn) connects to either Vref or GND.\n"
        "  The ladder network creates a binary-weighted voltage divider:\n"
        "    • MSB (Bn) contributes Vref/2 when high\n"
        "    • Next bit contributes Vref/4\n"
        "    • Each successive bit contributes half the previous\n"
        "    • LSB (B0) contributes Vref/(2^N)\n\n"
        "  Formula: Vout = Vref × (Digital_Value / 2^N)\n\n", -1);
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
    GtkWidget *window, *btn, *btn_r2r;

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

    /* Network calculator button */
    btn = GTK_WIDGET(gtk_builder_get_object(builder, "button_calculate"));
    if (btn)
        g_signal_connect(btn, "clicked", G_CALLBACK(on_calculate_clicked), NULL);

    /* R-2R Ladder: initialize dropdowns and connect button */
    init_r2r_dropdowns();
    btn_r2r = GTK_WIDGET(gtk_builder_get_object(builder, "button_r2r_generate"));
    if (btn_r2r)
        g_signal_connect(btn_r2r, "clicked", G_CALLBACK(on_r2r_generate_clicked), NULL);

    gtk_widget_show_all(window);
    gtk_main();

    g_object_unref(builder);
    return 0;
}
