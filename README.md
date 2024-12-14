# FTP_Server-Client

## 📁 **Simple FTP Server-Client**  
A simple FTP server and client implementation for **Linux** in C, developed as part of the **"Securitatea Software"** course at **Universitatea Babeș-Bolyai**.

### 📝 **Collaborators**  
- **Ruse Teodor**  
- **Stan Ariana**

---

## 📚 **Project Description**  
This project contains a basic implementation of an FTP server and client in the C programming language. It supports fundamental FTP operations and provides a simple interface for transferring files and managing directories. The project aims to demonstrate core concepts of FTP protocols, networking, and client-server communication, focusing on understanding file transfer protocols and system-level programming.

The default transfer type mode is **binary**.

---

## ⚙️ **Available FTP Commands**  
This project includes support for the following FTP commands:

- `CWD`  - Change Working Directory  
- `HELP`  - Provide a list of available commands  
- `LIST`  - List files and directories  
- `MKD`   - Create a new directory  
- `MODE`  - Set transfer mode (default is binary)  
- `PASS`  - User password authentication  
- `PASV`  - Set passive mode  
- `PWD`   - Print Working Directory  
- `QUIT`  - Terminate the connection  
- `RETR`  - Retrieve files from the server  
- `RMD`   - Remove directories  

---

## 🖼️ **Build Result Screenshot**  
![build](https://github.com/user-attachments/assets/325c4684-34e6-4a6a-849f-22ba5cbc2059)

> *This screenshot displays the build results and test interactions on the terminal.*

---

### Compile the project
Navigate to the project directory and compile both the server and client.

```bash
cd FTP_Server-Client
gcc -o server server.c -pthread
gcc -o client client.c
```

### Run the FTP Server
```bash
./server
```

### Run the FTP Client
```bash
./client "IP ADDRESS"
```
