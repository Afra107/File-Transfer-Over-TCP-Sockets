#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024

GtkWidget *progress_bar;
GtkWidget *status_label;
GtkWidget *received_files_title;
GtkWidget *file_list;

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

// Helper function to reset progress bar
void reset_progress_bar() {
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), "0 bytes");
}

// Helper function to update progress bar
gboolean update_progress_bar(gpointer fraction_pointer) {
    double fraction = *(double *)fraction_pointer;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), fraction);
    char progress_text[30];
    snprintf(progress_text, sizeof(progress_text), "%.0f bytes", fraction);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), progress_text);
    return FALSE;
}

// Helper function to update file list
gboolean update_file_list(gpointer file_name) {
    const char *current_text = gtk_label_get_text(GTK_LABEL(file_list));
    if (strcmp(current_text, "No files received yet.") == 0) {
        // If no files have been received yet, start a new list
        gtk_label_set_text(GTK_LABEL(file_list), (char *)file_name);
    } else {
        // Otherwise, append to the existing list
        char *new_text = g_strdup_printf("%s\n%s", current_text, (char *)file_name);
        gtk_label_set_text(GTK_LABEL(file_list), new_text);
        g_free(new_text);
    }
    free(file_name);
    return FALSE;
}

// Function to handle file reception
void *receive_files(void *arg) {
    int sockfd, new_sock, file_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received;
    off_t total_received = 0, file_size = 1;
    
    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Setup server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind socket
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    listen(sockfd, 5);
    LabelUpdateData *waiting_data = malloc(sizeof(LabelUpdateData));
    waiting_data->label = status_label;
    waiting_data->message = "Waiting for sender to send files...";
    gdk_threads_add_idle(update_label, waiting_data);

    while (1) {
        // Accept a connection
        new_sock = accept(sockfd, (struct sockaddr *)&client_addr, &addr_len);
        if (new_sock < 0) {
            perror("Accept failed");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        // Notify start of transfer
        LabelUpdateData *start_data = malloc(sizeof(LabelUpdateData));
        start_data->label = status_label;
        start_data->message = "Receiving file...";
        gdk_threads_add_idle(update_label, start_data);

     

        // Generate a unique file name
        char received_file_name[256];
        static int file_count = 1;
        snprintf(received_file_name, sizeof(received_file_name), "./receiver_directory/received_file_%d.mp4", file_count++);

        // Open a new file to write received data
        file_fd = open(received_file_name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (file_fd < 0) {
            perror("Failed to create file");
            close(new_sock);
            continue; // Skip to next connection
        }

        total_received = 0;

        // Receive data
        while ((bytes_received = recv(new_sock, buffer, BUFFER_SIZE, 0)) > 0) {
            if (bytes_received == 3 && strncmp(buffer, "EOF", 3) == 0) {
                break; // End of file received
            }
            write(file_fd, buffer, bytes_received);
            total_received += bytes_received;

            // Calculate progress fraction and update progress bar
            double fraction = (double)total_received / file_size;
            gdk_threads_add_idle(update_progress_bar, &fraction);
        }

        // Close file and socket
        close(file_fd);
        close(new_sock);

        // Reset progress bar
        reset_progress_bar();

        // Update file list
        gdk_threads_add_idle(update_file_list, strdup(received_file_name));

        // Notify idle state
        LabelUpdateData *idle_data = malloc(sizeof(LabelUpdateData));
        idle_data->label = status_label;
        idle_data->message = "Waiting for sender to send files...";
        gdk_threads_add_idle(update_label, idle_data);
    }

    close(sockfd);
    return NULL;
}

// Main function
int main(int argc, char *argv[]) {
    GtkWidget *window, *grid;

    gtk_init(&argc, &argv);

    // Create main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Receiver");
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

    // Progress bar
    progress_bar = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(progress_bar), TRUE);
    gtk_widget_set_hexpand(progress_bar, TRUE);
    gtk_grid_attach(GTK_GRID(grid), progress_bar, 0, 0, 1, 1);

    // Status label
    status_label = gtk_label_new("Waiting for sender to send files...");
    gtk_widget_set_margin_bottom(status_label, 30); // Bottom margin
    gtk_grid_attach(GTK_GRID(grid), status_label, 0, 1, 1, 1);

    // "Received Files:" title
    received_files_title = gtk_label_new("Received Files:");
    gtk_widget_set_halign(received_files_title, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(received_files_title), 0);
    gtk_label_set_markup(GTK_LABEL(received_files_title), "<b>Received Files:</b>");
    gtk_grid_attach(GTK_GRID(grid), received_files_title, 0, 2, 1, 1);

    // File list
    file_list = gtk_label_new("No files received yet.");
    gtk_grid_attach(GTK_GRID(grid), file_list, 0, 3, 1, 1);

    // Connect close signal
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Start file reception in a separate thread
    pthread_t thread;
    pthread_create(&thread, NULL, receive_files, NULL);
    pthread_detach(thread);

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}

