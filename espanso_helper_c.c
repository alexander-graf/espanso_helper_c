#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <pwd.h>
#include <ctype.h>
#include <gtk/gtk.h>

#define MAX_INPUT_LENGTH 4096
#define MAX_PATH_LENGTH 4096
#define ESPANSO_RELATIVE_PATH "/.config/espanso/match"
#define YAML_FILE "c_yaml.yml"

typedef struct {
    GtkWidget *window;
    GtkWidget *trigger_entry;
    GtkWidget *replace_text;
    GtkWidget *status_label;
} AppWidgets;

// Function prototypes for existing functions
char* get_home_directory(void);
char* get_config_path(void);
int file_exists(const char *path);
void ensure_directory_exists(const char *path);
int is_valid_trigger(const char *trigger);
void ensure_yaml_file_exists(const char *dir, const char *filename);
char* escape_yaml_string(const char *input);
void append_to_yaml(const char *config_dir, const char *trigger, const char *replace);

// Function to get home directory
char* get_home_directory(void) {
    char* home = getenv("HOME");
    if (home != NULL) {
        return strdup(home);
    }
    
    // Fallback to password database if HOME is not set
    struct passwd *pw = getpwuid(getuid());
    if (pw == NULL) {
        fprintf(stderr, "Error: Could not determine home directory\n");
        exit(1);
    }
    return strdup(pw->pw_dir);
}

// Function to build full config path
char* get_config_path(void) {
    char* home = get_home_directory();
    char* config_path = malloc(MAX_PATH_LENGTH);
    if (config_path == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(home);
        exit(1);
    }
    
    snprintf(config_path, MAX_PATH_LENGTH, "%s%s", home, ESPANSO_RELATIVE_PATH);
    free(home);
    return config_path;
}

// Function to check if a file exists
int file_exists(const char *path) {
    if (path == NULL) return 0;
    return access(path, F_OK) == 0;
}

// Function to create directory if it doesn't exist
void ensure_directory_exists(const char *path) {
    if (path == NULL) {
        fprintf(stderr, "Error: Invalid path\n");
        exit(1);
    }
    
    // Create parent directories recursively
    char *path_copy = strdup(path);
    char *p = path_copy;
    
    // Skip leading '/'
    if (*p == '/') p++;
    
    while (*p != '\0') {
        if (*p == '/') {
            *p = '\0';
            if (access(path_copy, F_OK) != 0) {
                if (mkdir(path_copy, 0700) != 0 && errno != EEXIST) {
                    fprintf(stderr, "Error creating directory %s: %s\n", path_copy, strerror(errno));
                    free(path_copy);
                    exit(1);
                }
            }
            *p = '/';
        }
        p++;
    }
    
    if (access(path_copy, F_OK) != 0) {
        if (mkdir(path_copy, 0700) != 0 && errno != EEXIST) {
            fprintf(stderr, "Error creating directory %s: %s\n", path_copy, strerror(errno));
            free(path_copy);
            exit(1);
        }
    }
    
    free(path_copy);
}

// Function to validate trigger
int is_valid_trigger(const char *trigger) {
    if (trigger == NULL || strlen(trigger) == 0) {
        return 0;
    }
    
    // Check for basic sanity (no control characters, reasonable length)
    for (size_t i = 0; trigger[i] != '\0'; i++) {
        if (iscntrl(trigger[i])) {
            return 0;
        }
    }
    
    return strlen(trigger) < MAX_INPUT_LENGTH;
}

// Function to ensure YAML file exists with proper header
void ensure_yaml_file_exists(const char *dir, const char *filename) {
    if (dir == NULL || filename == NULL) {
        fprintf(stderr, "Error: Invalid directory or filename\n");
        exit(1);
    }
    
    char filepath[MAX_PATH_LENGTH];
    ssize_t written = snprintf(filepath, sizeof(filepath), "%s/%s", dir, filename);
    if (written < 0 || (size_t)written >= sizeof(filepath)) {
        fprintf(stderr, "Error: Path too long\n");
        exit(1);
    }
    
    if (!file_exists(filepath)) {
        FILE *file = fopen(filepath, "w");
        if (file != NULL) {
            fprintf(file, "matches:\n");
            fclose(file);
        } else {
            fprintf(stderr, "Error creating YAML file %s: %s\n", filepath, strerror(errno));
            exit(1);
        }
    }
}

// Function to escape special characters in YAML
char* escape_yaml_string(const char *input) {
    if (input == NULL) return NULL;
    
    char *output = malloc(strlen(input) * 2 + 1);
    if (output == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(1);
    }
    
    int j = 0;
    for (int i = 0; input[i] != '\0'; i++) {
        switch (input[i]) {
            case '"':
                output[j++] = '\\';
                output[j++] = '"';
                break;
            case '\\':
                output[j++] = '\\';
                output[j++] = '\\';
                break;
            case '\n':
                output[j++] = '\\';
                output[j++] = 'n';
                break;
            default:
                output[j++] = input[i];
        }
    }
    output[j] = '\0';
    return output;
}

// Function to append new match to YAML file
void append_to_yaml(const char *config_dir, const char *trigger, const char *replace) {
    if (config_dir == NULL || trigger == NULL || replace == NULL) {
        fprintf(stderr, "Error: Invalid parameters for YAML append\n");
        exit(1);
    }
    
    char filepath[MAX_PATH_LENGTH];
    ssize_t written = snprintf(filepath, sizeof(filepath), "%s/%s", config_dir, YAML_FILE);
    if (written < 0 || (size_t)written >= sizeof(filepath)) {
        fprintf(stderr, "Error: Path too long\n");
        exit(1);
    }
    
    FILE *file = fopen(filepath, "a");
    if (file == NULL) {
        fprintf(stderr, "Error opening YAML file %s: %s\n", filepath, strerror(errno));
        exit(1);
    }
    
    char *escaped_trigger = escape_yaml_string(trigger);
    char *escaped_replace = escape_yaml_string(replace);
    
    fprintf(file, "  - trigger: \"%s\"\n", escaped_trigger);
    fprintf(file, "    replace: \"%s\"\n", escaped_replace);
    
    free(escaped_trigger);
    free(escaped_replace);
    fclose(file);
}

// GUI callback functions
static void on_save_clicked(GtkWidget *button, AppWidgets *app) {
    const char *trigger = gtk_entry_get_text(GTK_ENTRY(app->trigger_entry));
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->replace_text));
    GtkTextIter start, end;
    
    // Get text from buffer
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char *replace = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    
    // Validate input
    if (!is_valid_trigger(trigger) || strlen(trigger) == 0) {
        gtk_label_set_text(GTK_LABEL(app->status_label), "Error: Invalid trigger");
        g_free(replace);
        return;
    }
    
    if (strlen(replace) == 0) {
        gtk_label_set_text(GTK_LABEL(app->status_label), "Error: Replacement text cannot be empty");
        g_free(replace);
        return;
    }
    
    // Get config directory path
    char *config_dir = get_config_path();
    
    // Ensure config directory exists
    ensure_directory_exists(config_dir);
    
    // Ensure YAML file exists with header
    ensure_yaml_file_exists(config_dir, YAML_FILE);
    
    // Append new match to YAML file
    append_to_yaml(config_dir, trigger, replace);
    
    // Update status and clear fields
    gtk_label_set_text(GTK_LABEL(app->status_label), "Match added successfully!");
    gtk_entry_set_text(GTK_ENTRY(app->trigger_entry), "");
    gtk_text_buffer_set_text(buffer, "", -1);
    
    free(config_dir);
    g_free(replace);
}

static void on_clear_clicked(GtkWidget *button, AppWidgets *app) {
    gtk_entry_set_text(GTK_ENTRY(app->trigger_entry), "");
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->replace_text));
    gtk_text_buffer_set_text(buffer, "", -1);
    gtk_label_set_text(GTK_LABEL(app->status_label), "");
}

static void activate(GtkApplication *app, gpointer user_data) {
    AppWidgets *widgets = g_new(AppWidgets, 1);
    
    // Create main window
    widgets->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(widgets->window), "Espanso Helper");
    gtk_window_set_default_size(GTK_WINDOW(widgets->window), 500, 400);
    gtk_container_set_border_width(GTK_CONTAINER(widgets->window), 10);
    
    // Create main vertical box
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(widgets->window), vbox);
    
    // Create trigger input
    GtkWidget *trigger_label = gtk_label_new("Trigger:");
    gtk_widget_set_halign(trigger_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), trigger_label, FALSE, FALSE, 2);
    
    widgets->trigger_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(vbox), widgets->trigger_entry, FALSE, FALSE, 2);
    
    // Create replacement text area
    GtkWidget *replace_label = gtk_label_new("Replacement Text:");
    gtk_widget_set_halign(replace_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), replace_label, FALSE, FALSE, 2);
    
    // Create scrolled window for text view
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled_window, -1, 200);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 2);
    
    widgets->replace_text = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(widgets->replace_text), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(scrolled_window), widgets->replace_text);
    
    // Create button box
    GtkWidget *button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_END);
    gtk_box_pack_start(GTK_BOX(vbox), button_box, FALSE, FALSE, 2);
    
    // Create buttons
    GtkWidget *clear_button = gtk_button_new_with_label("Clear");
    GtkWidget *save_button = gtk_button_new_with_label("Save");
    gtk_container_add(GTK_CONTAINER(button_box), clear_button);
    gtk_container_add(GTK_CONTAINER(button_box), save_button);
    
    // Create status label
    widgets->status_label = gtk_label_new("");
    gtk_widget_set_halign(widgets->status_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), widgets->status_label, FALSE, FALSE, 2);
    
    // Connect signals
    g_signal_connect(save_button, "clicked", G_CALLBACK(on_save_clicked), widgets);
    g_signal_connect(clear_button, "clicked", G_CALLBACK(on_clear_clicked), widgets);
    
    gtk_widget_show_all(widgets->window);
}

int main(int argc, char *argv[]) {
    GtkApplication *app;
    int status;
    
    app = gtk_application_new("org.espanso.helper", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    
    return status;
}