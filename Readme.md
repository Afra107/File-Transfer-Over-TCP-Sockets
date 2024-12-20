# Video File Transfer Over TCP Sockets with GUI

## Description
This repository implements a system for transferring video files over TCP sockets using GNU C on a Linux operating system. It features a **Graphical User Interface (GUI)** for both the sender and receiver, designed using GTK, to facilitate the transfer of video files between two directories (`sender_directory` and `receiver_directory`) while displaying real-time status updates.

---

## Features

### Sender:
1. **File Selection**:
   - GUI includes a dropdown menu to select video files from the `sender_directory`.
2. **Transfer Status**:
   - Displays the progress of file transfer as a percentage using a progress bar.
   - Updates the status label to indicate the current operation.
3. **Completion Action**:
   - Opens the `receiver_directory` folder upon successful transfer.

### Receiver:
1. **File Reception**:
   - Automatically receives video file segments from the sender.
   - Writes the received data into new video files in the `receiver_directory`.
2. **Status Updates**:
   - Displays transfer progress in bytes via a progress bar.
   - Updates the status label during operations.
3. **File List**:
   - Shows a list of all successfully received files in the GUI.

---

## Design Diagram

![image](https://github.com/user-attachments/assets/bf5852e1-753e-481e-a24f-629cab86c9c9)

---

## Requirements
- **Operating System**: Linux
- **Compiler**: GCC
- **Libraries**: GTK+ for GUI development (`libgtk-3-dev` for compilation).
- **Directories**:
  - `sender_directory`: Contains video files to be sent.
  - `receiver_directory`: Destination for received files.

---

## Setup

1. **Install GTK**:
   ```bash
   sudo apt-get install libgtk-3-dev
   ```

2. **Create Required Directories**:
   ```bash
   mkdir sender_directory receiver_directory
   ```

3. **Compile the Code**:
   - Sender:
     ```bash
     gcc sender.c -o sender `pkg-config --cflags --libs gtk+-3.0` -lpthread
     ```
   - Receiver:
     ```bash
     gcc receiver.c -o receiver `pkg-config --cflags --libs gtk+-3.0` -lpthread
     ```

4. **Run the Applications**:
   - Start the receiver:
     ```bash
     ./receiver
     ```
   - Start the sender:
     ```bash
     ./sender
     ```

---

## Usage

### Sender:
1. Launch the sender application.
2. Select a video file from the dropdown menu.
3. Click **Send Files** to start the transfer.
4. Monitor the progress bar and status updates.

### Receiver:
1. Launch the receiver application.
2. Wait for the sender to connect and transfer files.
3. Monitor the progress bar and file list for updates.

---

## Functionality

### TCP Socket Operations:
- **Sender**:
  - Opens the video file and reads its contents.
  - Splits the file into segments and transmits them via a TCP connection.
  - Sends an EOF marker to indicate the end of the file.
  - Closes the socket after completing the transfer.
- **Receiver**:
  - Accepts incoming TCP connections.
  - Writes received segments into a new video file.
  - Identifies the EOF marker to terminate reception.
  - Closes the socket and file after receiving.

---

## GUI Components
- **Dropdown Menu**: For file selection (Sender).
- **Progress Bar**: Displays transfer progress in percentage (Sender) or bytes (Receiver).
- **Status Label**: Provides real-time updates on operations.
- **File List**: Shows received files (Receiver).

---

