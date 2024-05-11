#ifndef ZERO_COPY_SERVER_HPP
#define ZERO_COPY_SERVER_HPP

#include <arpa/inet.h>
#include <string>
#include <mutex>
#include <atomic>
#include <sys/sendfile.h>
#include <fstream>

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
    bool uploadFile(const std::string& filename, const std::string& file_content);

    // 下载文件
    std::pair<int, off_t> downloadFileDescriptor(const std::string& filename);

    // 获取文件描述符和文件大小
    std::pair<int, off_t> getFileDescriptor(const std::string& filepath);
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