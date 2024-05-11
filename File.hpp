#ifndef ZERO_COPY_SERVER_HPP
#define ZERO_COPY_SERVER_HPP

#include <arpa/inet.h>
#include <string>
#include <mutex>
#include <atomic>
#include <sys/sendfile.h>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>

// 文件系统操作类，用于处理文件的读取和写入，以及互斥锁保护
class FileSystem {
private:
    std::string upload_directory;
    std::string download_directory;
    std::mutex file_mutex; // 用于保护文件操作的互斥锁

public:
    FileSystem(const std::string& upload_dir, const std::string& download_dir)
        : upload_directory(upload_dir), download_directory(download_dir) {}

    // 上传文件
    bool uploadFile(const std::string& filename, const std::string& file_content) {
        std::lock_guard<std::mutex> lock(file_mutex);
        std::ofstream outFile(upload_directory + "/" + filename, std::ios::binary);
        if (outFile) {
            outFile << file_content;
            return true;
        }
        return false;
    }

    // 实现零复制传输
    bool zeroCopyFile(const int out_fd, const std::string& filename) {
        auto [fd, size] = getFileDescriptor(download_directory + "/" + filename);
        if (fd < 0) {
            return false;
        }

        // 使用 sendfile() 实现零复制
        off_t offset = 0;
        while (size > 0) {
            ssize_t bytes_sent = sendfile(out_fd, fd, &offset, size);
            if (bytes_sent <= 0) {
                break;
            }
            size -= bytes_sent;
        }

        close(fd);
        return true;
    }

    std::pair<int, off_t> getFileDescriptor(const std::string& filepath) {
        int fd = open(filepath.c_str(), O_RDONLY);
        if (fd != -1) {
            // 获取文件大小
            off_t size = lseek(fd, 0, SEEK_END);
            lseek(fd, 0, SEEK_SET); // 将文件指针重新定位到文件开头
            return {fd, size};
        }
        return {-1, 0};
    }
};

// 服务器类，用于处理网络通信和文件传输
class ZeroCopyServer {
private:
    int server_fd;
    struct sockaddr_in server_addr;
    FileSystem file_system;

    // 处理客户端请求
    void handle_client(int client_fd, const std::string& directory);

public:
    ZeroCopyServer(int port, const std::string& upload_dir, const std::string& download_dir);
    ~ZeroCopyServer();
    void start();
};

#endif // ZERO_COPY_SERVER_HPP