#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glib.h>

#define MAX_N 5           // maximum number of resistors allowed in a network
#define MAX_NETWORKS 10000 // maximum networks per count
#define MAX_EXPR 256      // maximum length of the expression string

// Structure for a resistor network configuration.
typedef struct {
    double R;            // equivalent resistance (ohms)
    int n;               // number of resistors used
    char expr[MAX_EXPR]; // text expression of the network
} Network;

GtkBuilder *builder = NULL;

/* Helper: parse a resistor value from a string label.
   This expects the label to have a number optionally followed by a suffix:
   e.g., "1 Ω", "7.5 Ω", "1K Ω", "1M Ω". */
double parse_resistor_value(const char *label) {
    double value;
    char suffix[10] = "";
    int n = sscanf(label, "%lf%9s", &value, suffix);
    if(n == 2) {
        /* Check for suffixes: K (kilo) or M (mega) */
        if (strchr(suffix, 'K') || strchr(suffix, 'k'))
            value *= 1000;
        else if (strchr(suffix, 'M') || strchr(suffix, 'm'))
            value *= 1000000;
    }
    return value;
}

/* Callback for the Calculate button click. */
static void on_calculate_clicked(GtkButton *button, gpointer user_data) {
    // Retrieve widgets from the builder (their IDs must match those in Glade)
    GtkWidget *entry_target    = GTK_WIDGET(gtk_builder_get_object(builder, "entry_target"));
    GtkWidget *combo_tol       = GTK_WIDGET(gtk_builder_get_object(builder, "combo_tolPerc"));
    GtkWidget *textview_output = GTK_WIDGET(gtk_builder_get_object(builder, "textview_output"));
    GtkWidget *grid_resistors  = GTK_WIDGET(gtk_builder_get_object(builder, "grid_resistors"));

    /* Build the available resistor array by iterating over all check buttons in the grid.
       Only the check buttons that are active (toggled on) are added.
       Their labels are parsed to extract the numeric value. */
    GList *children = gtk_container_get_children(GTK_CONTAINER(grid_resistors));
    double available[100];
    int numAvail = 0;
    for (GList *l = children; l != NULL; l = l->next) {
        GtkWidget *widget = GTK_WIDGET(l->data);
        if (GTK_IS_CHECK_BUTTON(widget)) {
            gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
            if (active) {
                const char *label = gtk_button_get_label(GTK_BUTTON(widget));
                double value = parse_resistor_value(label);
                available[numAvail++] = value;
            }
        }
    }
    g_list_free(children);

    // Get the target resistance value from the entry (convert from text to double)
    const char *target_text = gtk_entry_get_text(GTK_ENTRY(entry_target));
    double target = atof(target_text);

    // Get the tolerance percentage from the combo box.
    // (Assumes the combo box has items like "1", "2", … "10" representing percentages.)
    const char *tol_text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo_tol));
    double tolPerc = tol_text ? atof(tol_text) : 5; // default to 5% if none selected
    double tol = tolPerc / 100.0;

    /* Now run the resistor network calculation.
       We use the dynamic programming approach as in your original code. */

    // Allocate arrays for networks (indices 1..MAX_N)
    Network **networks = malloc((MAX_N+1) * sizeof(Network *));
    if (!networks) {
        g_printerr("Memory allocation error.\n");
        return;
    }
    for (int i = 0; i <= MAX_N; i++) {
        networks[i] = malloc(MAX_NETWORKS * sizeof(Network));
        if (!networks[i]) {
            g_printerr("Memory allocation error.\n");
            return;
        }
    }
    int count[MAX_N+1] = {0};

    // For networks using 1 resistor: each available resistor becomes a network.
    for (int i = 0; i < numAvail; i++) {
        if (count[1] < MAX_NETWORKS) {
            networks[1][count[1]].R = available[i];
            networks[1][count[1]].n = 1;
            snprintf(networks[1][count[1]].expr, MAX_EXPR, "%.2f", available[i]);
            count[1]++;
        }
    }

    // For networks using 2 to MAX_N resistors, combine smaller networks.
    for (int n = 2; n <= MAX_N; n++) {
        count[n] = 0;
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

    // Build the output string containing networks within the tolerance range.
    GString *result = g_string_new("");
    g_string_append_printf(result, "\n-- Networks within %.2f%% tolerance of %.2f ohm --\n", tolPerc, target);
    int found = 0;
    for (int n = 1; n <= MAX_N; n++) {
        for (int i = 0; i < count[n]; i++) {
            double relError = fabs(networks[n][i].R - target) / target;
            if (relError <= tol) {
                g_string_append_printf(result,
                    "Using %d resistor%s: %s = %.2f ohm (error %.2f%%)\n",
                    networks[n][i].n,
                    networks[n][i].n > 1 ? "s" : "",
                    networks[n][i].expr,
                    networks[n][i].R,
                    relError * 100);
                found = 1;
            }
        }
    }
    if (!found)
        g_string_append(result, "No network found within the specified tolerance.\n");

    // Set the results text into the text view.
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview_output));
    gtk_text_buffer_set_text(buffer, result->str, -1);
    g_string_free(result, TRUE);

    // Free allocated memory for networks.
    for (int i = 0; i <= MAX_N; i++)
        free(networks[i]);
    free(networks);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    // Load the UI from the Glade file.
    builder = gtk_builder_new();
    if (gtk_builder_add_from_file(builder, "ui.glade", NULL) == 0) {
        g_printerr("Error loading UI file.\n");
        return 1;
    }

    // Get the main window (its ID must match that in Glade, here "window1").
    GtkWidget *window = GTK_WIDGET(gtk_builder_get_object(builder, "window1"));
    if (!window) {
        g_printerr("Could not find window1 in UI file.\n");
        return 1;
    }

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    // Connect signals declared in the UI and also manually connect the Calculate button.
    gtk_builder_connect_signals(builder, NULL);
    GtkWidget *button = GTK_WIDGET(gtk_builder_get_object(builder, "button_calculate"));
    if (button)
        g_signal_connect(button, "clicked", G_CALLBACK(on_calculate_clicked), NULL);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
