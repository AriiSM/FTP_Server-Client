# FTP_Server-Client

## 📁 **Simple FTP Server-Client**  
A simple FTP server and client implementation for **Linux** in C, developed as part of the **"Securitatea Software"** course at **Universitatea Babeș-Bolyai**.

### 📝 **Collaborators**  
- **Stan Ariana - Client**
- **Ruse Teodor - Server**  

---

## 📚 **Project Description**  
This project contains a basic implementation of an FTP server and client in the C programming language. It supports fundamental FTP operations and provides a simple interface for transferring files and managing directories. The project aims to demonstrate core concepts of FTP protocols, networking, and client-server communication, focusing on understanding file transfer protocols and system-level programming.

The default transfer type mode is **binary**.

---

## ⚙️ **Available FTP Commands**  
This project includes support for the following FTP commands:

- `HELP`  - List available commands  
- `USER`  - Authenticate username  
- `PASS`  - Authenticate password  
- `PASV`  - Enable passive mode  
- `PWD`   - Display current directory  
- `CWD`   - Change directory  
- `MKD`   - Create a directory  
- `LIST`  - List files and directories  
- `RMD`   - Remove a directory  
- `TYPE`  - Set file transfer type  
- `RETR`  - Download a file  
- `STOR`  - Upload a file  
- `QUIT`  - Disconnect from the server  

---

## 🖼️ **Build Result Screenshot**  
![build](https://github.com/user-attachments/assets/325c4684-34e6-4a6a-849f-22ba5cbc2059)

> *This screenshot displays the build results and test interactions on the terminal.*

---

### **Important Notes**  
- The server must have a directory named `server_data` in the same directory as the executable. (It might not be created automatically.)  
- The client must have a directory named `data` in the same directory as the executable.  
- The project must be compiled before running.  

### **Run the Project**  
Navigate to the project directory and compile both the server and client.  

### **Run the FTP Server**  
```bash
sudo ./server
```

### Run the FTP Client
```bash
./client "IP ADDRESS"
```
