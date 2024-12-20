#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <dirent.h>

#define PORT 8080
#define BUFFER_SIZE 1024

GtkWidget *progress_bar;
GtkWidget *status_label;
GtkWidget *selected_files_label;
GtkWidget *selected_files_title;
GtkWidget *dropdown;

char *receiver_ip = "127.0.0.1"; // Fixed receiver IP
GList *selected_files = NULL;   // List of selected files

// Struct for passing label and message
typedef struct {
    GtkWidget *label;
    const char *message;
} LabelUpdateData;

// Helper function to update label text
gboolean update_label(gpointer data) {
    LabelUpdateData *update_data = (LabelUpdateData *)data;
    gtk_label_set_text(GTK_LABEL(update_data->label), update_data->message);
    free(update_data);
    return FALSE;
}

// Helper function to update progress bar
gboolean update_progress_bar(gpointer fraction_pointer) {
    double fraction = *(double *)fraction_pointer;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), fraction);
    char percentage[20];
    snprintf(percentage, sizeof(percentage), "%.0f%%", fraction * 100);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), percentage);
    return FALSE;
}

// Helper function to reset progress bar and status label
void reset_progress_and_status() {
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), "0%");
    gtk_label_set_text(GTK_LABEL(status_label), " ");
}

// Helper function to convert GList to a comma-separated string
char *g_list_to_comma_string(GList *list) {
    GString *result = g_string_new(NULL);
    for (GList *l = list; l != NULL; l = l->next) {
        if (result->len > 0) {
            g_string_append(result, ", ");
        }
        g_string_append(result, (char *)l->data);
    }
    char *final_string = g_string_free(result, FALSE);
    return final_string;
}

// Helper function to update selected files label
gboolean update_selected_files_label(gpointer file_list) {
    gtk_label_set_text(GTK_LABEL(selected_files_label), (char *)file_list);
    free(file_list);
    return FALSE;
}

// Function to transfer a single file
void transfer_file(const char *file_path) {
    int sockfd, file_fd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    off_t file_size, total_sent = 0;

    // Open file
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "./sender_directory/%s", file_path);
    file_fd = open(full_path, O_RDONLY);
    if (file_fd < 0) {
        perror("Failed to open file");
        return;
    }

    // Get file size
    file_size = lseek(file_fd, 0, SEEK_END);
    lseek(file_fd, 0, SEEK_SET);

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        close(file_fd);
        return;
    }

    // Setup server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, receiver_ip, &server_addr.sin_addr);

    // Connect to receiver
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sockfd);
        close(file_fd);
        return;
    }

    // Transfer file
    while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
        send(sockfd, buffer, bytes_read, 0);
        total_sent += bytes_read;

        // Update progress bar
        double fraction = (double)total_sent / file_size;
        gdk_threads_add_idle(update_progress_bar, &fraction);
    }

    // Send EOF marker
    send(sockfd, "EOF", 3, 0);

    // Close file and socket
    close(file_fd);
    close(sockfd);
}

// Function to handle file transfer in a separate thread
void *transfer_files(void *arg) {
    if (selected_files == NULL) {
        LabelUpdateData *data = malloc(sizeof(LabelUpdateData));
        data->label = status_label;
        data->message = "No files selected!";
        gdk_threads_add_idle(update_label, data);
        return NULL;
    }

    LabelUpdateData *start_data = malloc(sizeof(LabelUpdateData));
    start_data->label = status_label;
    start_data->message = "Transferring files...";
    gdk_threads_add_idle(update_label, start_data);

    // Transfer each selected file
    for (GList *file = selected_files; file != NULL; file = file->next) {
        transfer_file((char *)file->data);
    }

    LabelUpdateData *complete_data = malloc(sizeof(LabelUpdateData));
    complete_data->label = status_label;
    complete_data->message = "All files transferred successfully!";
    gdk_threads_add_idle(update_label, complete_data);

    // Free file list
    g_list_free_full(selected_files, free);
    selected_files = NULL;

    // Open receiver folder
    system("xdg-open ./receiver_directory");

    return NULL;
}

// Populate dropdown with video files
void populate_dropdown(GtkComboBoxText *dropdown) {
    DIR *d;
    struct dirent *dir;
    d = opendir("./sender_directory");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (strstr(dir->d_name, ".mp4") || strstr(dir->d_name, ".avi") || strstr(dir->d_name, ".mkv")) {
                gtk_combo_box_text_append_text(dropdown, dir->d_name);
            }
        }
        closedir(d);
    }
}

// Callback for dropdown selection
void on_dropdown_changed(GtkComboBoxText *combo, gpointer user_data) {
    const char *selected = gtk_combo_box_text_get_active_text(combo);
    if (selected) {
        reset_progress_and_status(); // Reset progress bar and status label
        selected_files = g_list_append(selected_files, strdup(selected));
        char *file_list = g_list_to_comma_string(selected_files);
        gdk_threads_add_idle(update_selected_files_label, strdup(file_list));
        free(file_list);
    }
}

// Callback for send button
void on_send_clicked(GtkButton *button, gpointer user_data) {
    pthread_t thread;
    pthread_create(&thread, NULL, transfer_files, NULL);
    pthread_detach(thread);
}

// Main function
int main(int argc, char *argv[]) {
    GtkWidget *window, *grid, *send_button;

    gtk_init(&argc, &argv);

    // Create main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Sender");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 500);
    gtk_container_set_border_width(GTK_CONTAINER(window), 20);

    // Create grid layout
    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_add(GTK_CONTAINER(window), grid);
    
    // Apply CSS styling
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        "window { background-color: #F7D4B5; }", -1, NULL);
    GtkStyleContext *context = gtk_widget_get_style_context(window);
    gtk_style_context_add_provider(context,
        GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);

    // Dropdown for file selection
    dropdown = gtk_combo_box_text_new_with_entry();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dropdown), "Select files...");
    gtk_widget_set_margin_bottom(dropdown, 20); // Bottom margin
    g_signal_connect(dropdown, "changed", G_CALLBACK(on_dropdown_changed), NULL);
    gtk_grid_attach(GTK_GRID(grid), dropdown, 0, 0, 1, 1);

    // "Selected Files:" title
    selected_files_title = gtk_label_new("Selected Files:");
    gtk_widget_set_halign(selected_files_title, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(selected_files_title), 0);
    gtk_label_set_markup(GTK_LABEL(selected_files_title), "<b>Selected Files:</b>");
    gtk_grid_attach(GTK_GRID(grid), selected_files_title, 0, 1, 1, 1);

    // Selected files label
    selected_files_label = gtk_label_new("No files selected.");
    gtk_widget_set_margin_bottom(selected_files_label, 30); // Bottom margin
    gtk_grid_attach(GTK_GRID(grid), selected_files_label, 0, 2, 1, 1);

    // Send button
    send_button = gtk_button_new_with_label("Send Files");
    g_signal_connect(send_button, "clicked", G_CALLBACK(on_send_clicked), NULL);
    gtk_widget_set_margin_bottom(send_button, 20); // Bottom margin
    gtk_grid_attach(GTK_GRID(grid), send_button, 0, 3, 1, 1);

    // Progress bar
    progress_bar = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(progress_bar), TRUE);
    gtk_widget_set_hexpand(progress_bar, TRUE);
    gtk_grid_attach(GTK_GRID(grid), progress_bar, 0, 4, 1, 1);

    // Status label
    status_label = gtk_label_new(" ");
    gtk_grid_attach(GTK_GRID(grid), status_label, 0, 5, 1, 1);

    // Populate dropdown
    populate_dropdown(GTK_COMBO_BOX_TEXT(dropdown));

    // Connect close signal
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}

