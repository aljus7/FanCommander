// fanctrl_daemon.cpp
// Compile: g++ -o fanctrl_daemon fanctrl_daemon.cpp -std=c++17

#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/stat.h>

namespace fs = std::filesystem;

std::string SERIAL_PORT = "/dev/ttyUSB0";  // ← change or read from config
const std::string BASE_DIR = "/run/fanctrl/";

std::atomic<int> rpm1{0}, rpm2{0};
std::atomic<bool> running{true};

int serial_fd = -1;

bool open_serial() {
    // Read config file if it exists
    std::string config_file = "fanCtrlDrv.conf";
    std::ifstream config(config_file);
    if (config.is_open()) {
        std::getline(config, SERIAL_PORT);
        config.close();
    }
    
    serial_fd = open(SERIAL_PORT.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_fd == -1) return false;

    struct termios tty;
    tcgetattr(serial_fd, &tty);
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = 5;
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    tcsetattr(serial_fd, TCSANOW, &tty);
    return true;
}

void send_command(const std::string& cmd) {
    if (serial_fd == -1) return;
    write(serial_fd, cmd.c_str(), cmd.length());
    fsync(serial_fd);
}

void serial_reader() {
    char buf[256];
    std::string line;

    while (running) {
        ssize_t n = read(serial_fd, buf, sizeof(buf)-1);
        if (n > 0) {
            buf[n] = '\0';
            line += buf;

            size_t pos;
            while ((pos = line.find('\n')) != std::string::npos) {
                std::string msg = line.substr(0, pos);
                line.erase(0, pos + 1);

                // Very simple parsing - improve as needed
                if (msg.find("RPM:") != std::string::npos) {
                    try {
                        size_t val_pos = msg.find_last_of(' ');
                        if (val_pos != std::string::npos) {
                            int val = std::stoi(msg.substr(val_pos + 1));
                            if (rpm1 < 100) rpm1 = val;  // rough alternation
                            else            rpm2 = val;
                        }
                    } catch(...) {}
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void write_file(const std::string& path, int value) {
    std::ofstream f(path);
    if (f.is_open()) f << value << "\n";
}

int read_pwm_file(const std::string& path) {
    std::ifstream f(path);
    int val = -1;
    if (f.is_open()) f >> val;
    return val;
}

int main() {
    // Create directory (usually needs root)
    fs::create_directories(BASE_DIR);
    chmod(BASE_DIR.c_str(), 0777);  // optional - careful in production

    // Create empty files
    std::string files[] = {"fan1_pwm", "fan1_rpm", "fan2_pwm", "fan2_rpm"};
    for (const auto& name : files) {
        std::string p = BASE_DIR + name;
        if (!fs::exists(p)) {
            std::ofstream f(p);
            f << (name.find("rpm") != std::string::npos ? "0" : "128") << "\n";
        }
    }

    if (!open_serial()) {
        std::cerr << "Cannot open " << SERIAL_PORT << "\n";
        return 1;
    }

    std::cout << "Connected to " << SERIAL_PORT << "\n";

    std::thread reader(serial_reader);

    // Initial safe values
    send_command("p1 128\n");
    send_command("p2 128\n");

    while (running) {
        // Poll pwm files and send commands
        int p1 = read_pwm_file(BASE_DIR + "fan1_pwm");
        if (p1 >= 0 && p1 <= 255) {
            send_command("p1 " + std::to_string(p1) + "\n");
        }

        int p2 = read_pwm_file(BASE_DIR + "fan2_pwm");
        if (p2 >= 0 && p2 <= 255) {
            send_command("p2 " + std::to_string(p2) + "\n");
        }

        // Update rpm files
        write_file(BASE_DIR + "fan1_rpm", rpm1);
        write_file(BASE_DIR + "fan2_rpm", rpm2);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    reader.join();
    if (serial_fd != -1) close(serial_fd);
    return 0;
}