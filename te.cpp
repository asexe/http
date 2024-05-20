#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>
#include <thread>
#include <fstream>
#include <sstream>
#include "ThreadPool.hpp"
#include "EpollServer.hpp"
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <ctime>
#include <dirent.h>
#include <filesystem>
// #include "File.hpp"
const int MAX_EVENTS = 10;
int server_fd;

// 获取硬件支持的线程数，如果没有，就使用默认值4
size_t thread_count = std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 4;

std::string captureAfterKey(const std::string &input)
{
    std::size_t echoPos = input.find("/echo/");
    if (echoPos == std::string::npos)
    {
        // 如果没有找到 /echo/，返回空字符串
        return "";
    }
    // 从 /echo/ 后面开始查找空格
    std::size_t spacePos = input.find(' ', echoPos + 6); // /echo/ 长度为6
    if (spacePos == std::string::npos)
    {
        // 如果没有找到空格，取从 /echo/ 后面到字符串末尾的部分
        return input.substr(echoPos + 6);
    }
    else
    {
        // 如果找到了空格，取空格前的部分
        return input.substr(echoPos + 6, spacePos - echoPos - 6);
    }
}

std::string extractUserAgent(const std::string &request)
{
    std::size_t userAgentPos = request.find("User-Agent: ");
    if (userAgentPos == std::string::npos)
    {
        // 如果没有找到 User-Agent 头，返回空字符串
        return "";
    }
    // 找到 User-Agent 头，现在找到该行的结束位置
    std::size_t endOfLinePos = request.find("\r\n", userAgentPos);
    if (endOfLinePos == std::string::npos)
    {
        // 如果没有找到行结束，返回空字符串
        return "";
    }
    // 提取 User-Agent 头的值
    return request.substr(userAgentPos + sizeof("User-Agent: ") - 1, endOfLinePos - userAgentPos - sizeof("User-Agent: ") + 1);
}

/*
std::string handlePostRequest(const std::string& request, const std::string& directory, int client_fd) {
    std::string response;
    std::string filename;
    std::string fileContent;

    // 查找 POST 请求正文的开始
    size_t postHeaderEnd = request.find("\r\n\r\n") + 4;
    if (postHeaderEnd != std::string::npos) {
        // 获取 POST 请求正文内容
        fileContent = request.substr(postHeaderEnd);
        std::cout << fileContent  +"\n";

        // 提取文件名，假设它紧跟在 "POST /files/" 之后
        size_t filenameStart = request.find("POST /files/") + 11;
        size_t filenameEnd = request.find(" ", filenameStart); // 假设文件名之后有一个空格
        if (filenameEnd != std::string::npos) {
            filename = request.substr(filenameStart, filenameEnd - filenameStart);
            std::cout << filename + "rnm" + "\n";

            // 构造完整的文件路径
            std::string filePath;
            if (directory.empty()) {
                // 如果 directory 为空，则下载文件到 downloaded 文件夹
                filePath = "downloaded/" + filename;
            } else {
                // 否则，上传文件到 uploaded 文件夹
                filePath = directory + "/" + filename;
            }

            // 保存文件
            std::ofstream outFile(filePath, std::ios::binary);
            if (outFile) {
                outFile << fileContent;
                outFile.close(); // 确保文件已关闭

                // 使用 sendfile() 来发送文件
                int fd = open(filePath.c_str(), O_RDONLY);
                if (fd < 0) {
                    response = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n";
                    std::cerr << "fd < 0\n";
                } else {
                    if (client_fd < 0) {
                        response = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n";
                        std::cerr << "client_fd < 0\n";
                    } else {
                        // 发送文件
                        off_t offset = 0;
                        struct stat fileStat;
                        fstat(fd, &fileStat);
                        ssize_t sent = sendfile(client_fd, fd, &offset, fileStat.st_size);
                        if (sent < 0) {
                            response = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n";
                            std::cerr << "sent < 0\n";
                        } else {
                            response = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: "
                                     + std::to_string(sent) + "\r\n\r\n";
                        }
                        close(client_fd);
                    }
                    close(fd);
                }
            } else {
                response = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n";
                std::cerr << "There are not out files exist\n";
            }
        } else {
            response = "HTTP/1.1 400 Bad Request: Invalid filename\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n";
        }
    } else {
        response = "HTTP/1.1 400 Bad Request: Invalid POST request format\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n";
    }

    return response;
}
*/
std::string extractFileName(const std::string &postContent)
{
    // 查找 "filename=\"" 字符串
    size_t filenamePos = postContent.find("filename=\"");
    if (filenamePos != std::string::npos)
    {
        // 找到文件名开始的位置
        size_t filenameStart = filenamePos + 10; // "filename=\"" 的长度为 10
        // 找到文件名结束的位置
        size_t filenameEnd = postContent.find("\"", filenameStart);
        if (filenameEnd != std::string::npos)
        {
            // 提取文件名
            std::string filename = postContent.substr(filenameStart, filenameEnd - filenameStart);
            return filename;
        }
    }
    return "";
}

std::string handlePostRequest(const std::string &request, const std::string &directory, int client_fd)
{
    std::string response;
    std::string filename;
    std::string fileContent;

    // 查找 POST 请求正文的开始
    size_t postHeaderEnd = request.find("\r\n\r\n") + 4;
    if (postHeaderEnd != std::string::npos)
    {
        // 获取 POST 请求正文内容
        fileContent = request.substr(postHeaderEnd);

        // 提取文件名，通过检查 "Content-Disposition" 头部字段
        size_t contentDispositionPos = request.find("Content-Disposition: form-data;");
        if (contentDispositionPos != std::string::npos)
        {
            // 找到文件名开始的位置
            size_t filenameStart = request.find("filename=\"", contentDispositionPos);
            if (filenameStart != std::string::npos)
            {
                // 提取文件名
                filename = extractFileName(request.substr(filenameStart));
                // std::cout << "Filename: " << filename << std::endl;
            }
        }

        if (!filename.empty())
        {
            // 构造完整的文件路径
            std::string filePath;
            if (directory.empty())
            {
                // 如果 directory 为空，则下载文件到 downloaded 文件夹
                filePath = "downloaded/" + filename;
            }
            else
            {
                // 否则，上传文件到  文件夹
                filePath = directory + "downloaded/" + filename;
            }

            /*if (std::filesystem::exists(filePath)) {
               std::cout << "File exists: " << filePath << std::endl;
           } else {
               std::cout << "File does not exist: " << filePath << std::endl;
           }*/

            // 保存文件
            std::ofstream outFile(filePath, std::ios::binary);
            if (outFile)
            {
                outFile << fileContent;
                outFile.close(); // 确保文件已关闭

                // 返回成功响应
                //response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n";
                //std::cout << "Send!!!\n" << std::endl;
                char report[520] = "HTTP/1.1 200 ok\r\n\r\n";
                int s = send(client_fd, report, strlen(report), 0);
                // 打开并发送HTML文件内容
                int fd = open("index.html", O_RDONLY);
                sendfile(client_fd, fd, NULL, 2500); // 使用零拷贝发送文件内容
                close(fd);
            }
            else
            {
                response = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n";
                std::cerr << "Failed to save file" << std::endl;
            }
        }
        else
        {
            response = "HTTP/1.1 400 Bad Request: Invalid filename\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n";
            std::cerr << "No filename found in request" << std::endl;
        }
    }
    else
    {
        response = "HTTP/1.1 400 Bad Request: Invalid POST request format\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n";
        std::cerr << "Invalid POST request format" << std::endl;
    }

    return response;
}

std::string fileName(const std::string &filePath)
{
    size_t found = filePath.find_last_of("/\\");
    if (found != std::string::npos)
    {
        return filePath.substr(found + 1);
    }
    return filePath;
}

// 新增函数，用于获取文件的详细信息
std::string getFileInfo(const std::string &filePath)
{
    struct stat fileStat;
    if (stat(filePath.c_str(), &fileStat) == 0)
    {
        time_t modTime = fileStat.st_mtime; // 获取文件修改时间
        struct tm *timeInfo = localtime(&modTime);
        char timeBuffer[100];
        strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", timeInfo);

        std::string size = std::to_string(fileStat.st_size) + " bytes";

        return "{\"name\": \"" + fileName(filePath) + "\", \"size\": \"" + size + "\", \"uploaded\": \"" + std::string(timeBuffer) + "\"}";
    }
    else
    {
        return "{}";
    }
}
std::string listFiles(const std::string &directory)
{
    std::vector<std::string> files;
    std::string fileListContent;

    std::string targetDirectory = directory.empty() ? "downloaded" : directory;
    DIR *dir = opendir(targetDirectory.c_str());
    if (dir == NULL)
    {
        return "[]"; // 如果目录打开失败，返回空数组
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL)
    {
        std::string file = ent->d_name;
        if (file != "." && file != "..")
        {
            files.push_back(file);
        }
    }
    closedir(dir);

    // 如果没有文件，直接返回空数组
    if (files.empty())
    {
        return "[]";
    }

    // 开始构建 JSON 数组字符串
    fileListContent = "[\n";
    for (size_t i = 0; i < files.size(); ++i)
    {
        std::string fileInfo = getFileInfo((targetDirectory + "/" + files[i]));
        fileListContent += "  " + fileInfo;
        if (i < files.size() - 1)
        {
            fileListContent += ",\n"; // 除了最后一个元素外，每个元素后都添加逗号和换行符
        }
    }
    fileListContent += "\n]"; // 添加结束括号

    return fileListContent;
}
void NF(int client_fd)
{
    std::string reportt;
    struct stat fileStat;
    int fd = open("404/404.html", O_RDONLY);
    if (fd == -1)
    {
        // handle error
        return;
    }
    fstat(fd, &fileStat);
    off_t len = fileStat.st_size;
    reportt = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: " + std::to_string(len) + "\r\n\r\n";
    // send HTTP header
    send(client_fd, reportt.c_str(), reportt.length(), 0);
    // send file content
    sendfile(client_fd, fd, NULL, len);
    close(fd);
    // printf("I am here");
}

// 发送 CSS 文件的函数
void sendCSS(int client_fd, const std::string &css_path)
{
    std::ifstream css_file(css_path);
    if (!css_file.is_open())
    {
        // 文件打开失败，发送 404 Not Found 错误
        std::string response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, response.c_str(), response.length(), 0);
        return;
    }

    // 获取 CSS 文件的大小
    struct stat file_stat;
    stat(css_path.c_str(), &file_stat);
    off_t len_css = file_stat.st_size;

    // 构造 HTTP 头部
    std::string header = "HTTP/1.1 200 OK\r\nContent-Type: text/css\r\nContent-Length: " + std::to_string(len_css) + "\r\n\r\n";
    send(client_fd, header.c_str(), header.length(), 0);

    // 发送 CSS 文件内容
    char buffer[1024];
    while (!css_file.eof())
    {
        css_file.read(buffer, sizeof(buffer));
        send(client_fd, buffer, css_file.gcount(), 0);
    }

    css_file.close();
}

// 发送 JavaScript 文件的函数
void sendJS(int client_fd, const std::string &js_path)
{
    std::ifstream js_file(js_path);
    if (!js_file.is_open())
    {
        // 文件打开失败，发送 404 Not Found 错误
        std::string response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, response.c_str(), response.length(), 0);
        return;
    }

    // 获取 JavaScript 文件的大小
    struct stat file_stat;
    stat(js_path.c_str(), &file_stat);
    off_t len_js = file_stat.st_size;

    // 构造 HTTP 头部
    std::string header = "HTTP/1.1 200 OK\r\nContent-Type: text/javascript\r\nContent-Length: " + std::to_string(len_js) + "\r\n\r\n";
    send(client_fd, header.c_str(), header.length(), 0);

    // 发送 JavaScript 文件内容
    char buffer[1024];
    while (!js_file.eof())
    {
        js_file.read(buffer, sizeof(buffer));
        send(client_fd, buffer, js_file.gcount(), 0);
    }

    js_file.close();
}
void sendimg(int client_fd, const std::string &js_path)
{
    std::ifstream js_file(js_path);
    if (!js_file.is_open())
    {
        // 文件打开失败，发送 404 Not Found 错误
        std::string response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_fd, response.c_str(), response.length(), 0);
        return;
    }

    // 获取 img 文件的大小
    struct stat file_stat;
    stat(js_path.c_str(), &file_stat);
    off_t len_js = file_stat.st_size;

    // 构造 HTTP 头部
    std::string header = "HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: " + std::to_string(len_js) + "\r\n\r\n";
    send(client_fd, header.c_str(), header.length(), 0);

    // 发送 img 文件内容
    char buffer[1024];
    while (!js_file.eof())
    {
        js_file.read(buffer, sizeof(buffer));
        send(client_fd, buffer, js_file.gcount(), 0);
    }

    js_file.close();
}
void sendfd(int client_fd, const std::string& fd_name, const std::string& directory) {
    std::string filePath;
    if (directory.empty()) {
        // 如果 directory 为空，则文件从 downloaded 文件夹上传
        filePath = "downloaded/" + fd_name;
    } else {
        // 否则，文件从指定目录的 downloaded 文件夹上传
        filePath = directory + "/downloaded/" + fd_name;
    }

    // 确保文件存在
    if (!std::filesystem::exists(filePath)) {
        std::cerr << "File does not exist: " << filePath << std::endl;
        NF(client_fd); // 发送 404 Not Found 页面
        return;
    }

    // 打开文件
    int fd = open(filePath.c_str(), O_RDONLY);
    if (fd < 0) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return;
    }

    // 获取文件状态
    struct stat fileStat;
    if (fstat(fd, &fileStat) != 0) {
        std::cerr << "Failed to get file status for: " << filePath << std::endl;
        close(fd);
        return;
    }

    // 构建 HTTP 响应头
    std::string contentType = "application/octet-stream"; // 默认内容类型
    // 可以根据文件类型设置不同的内容类型
    // if (fd_name.find(".html") != std::string::npos) {
    //     contentType = "text/html";
    // }

    std::string header = "HTTP/1.1 200 OK\r\nContent-Type: " + contentType +
                         "\r\nContent-Length: " + std::to_string(fileStat.st_size) +
                         "\r\n\r\n";

    // 发送 HTTP 头部
    send(client_fd, header.c_str(), header.length(), 0);

    // 发送文件内容
    off_t offset = 0;
    if (sendfile(client_fd, fd, &offset, fileStat.st_size) < 0) {
        std::cerr << "Failed to send file: " << filePath << std::endl;
    }

    // 关闭文件描述符
    close(fd);
}

// 新建函数 processRequest 来处理请求
std::string processRequest(const std::string &request, const std::string &directory /*, const std::vector<std::string>& keyword*/, int client_fd)
{
    std::string report;
    size_t start_pos = request.find(" ");
    size_t end_pos = request.find(" ", start_pos + 1);

    if (start_pos != std::string::npos && end_pos != std::string::npos)
    {
        std::string method = request.substr(0, start_pos);
        std::string path = request.substr(start_pos + 1, end_pos - start_pos - 1);
        std::cout << "Received path: " << path << std::endl;

        // 提取 User-Agent 头的值
        std::string userAgent = extractUserAgent(request);

        if (path == "/" || path == "/index")
        {
            // report = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 13\r\n\r\nHello, World!";
            // send(client_fd, report.c_str(), report.length(), 0);
            char report[520] = "HTTP/1.1 200 ok\r\n\r\n";
            int s = send(client_fd, report, strlen(report), 0);
            // 打开并发送HTML文件内容
            int fd = open("index.html", O_RDONLY);
            sendfile(client_fd, fd, NULL, 2500); // 使用零拷贝发送文件内容
            close(fd);
        }
        // 处理 /user-agent 请求
        else if (path == "/user-agent")
        {
            report = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(userAgent.length()) + "\r\n\r\n" + userAgent;
            send(client_fd, report.c_str(), report.length(), 0);
        }
        else if (path == "/404.css")
        {
            sendCSS(client_fd, "404/404.css");
        }
        else if (path == "/404.js")
        {
            sendJS(client_fd, "404/404.js");
        }
        else if (path == "/img/404.png")
        {
            sendimg(client_fd, "404/img/404.png");
        }
        else if (path.substr(0, 12) == "/downloaded/")
        {
            std::cout << "sosososos" << std::endl;
            std::string fd_name = path.substr(12); // 去掉 "/downloaded/"
            sendfd(client_fd, fd_name, directory);
        }
        // 处理 /echo/ 请求
        else if (path.find("/echo/") == 0)
        {
            std::string responseContent = captureAfterKey(request);
            report = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(responseContent.length()) + "\r\n\r\n" + responseContent;
            send(client_fd, report.c_str(), report.length(), 0);
        }
        else if (path == "/list-files")
        {
            report = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n" + listFiles(directory);
            std::cout << listFiles(directory) << std::endl;
            std::cout << report << std::endl;
            send(client_fd, report.c_str(), report.length(), 0);
        }
        else if (method == "POST")
        {
            // 确保路径以 "/files/" 开始
            if (path.find("/files/") == 0)
            {
                report = handlePostRequest(request, directory, client_fd);
                send(client_fd, report.c_str(), report.length(), 0);
            }
            else
            {
                // report = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n";
                // send(client_fd, report.c_str(), report.length(), 0);
                NF(client_fd);
            }
        }else{
            NF(client_fd);
        }
    }
}

void handle_client(int client_fd, struct sockaddr_in client_addr, const std::string &directory)
{
    char buffer[1024];
    std::string report;
    /*std::vector<std::string> keyword = {"/files/", "/echo/", "/index.html", "/user-agent"};*/
    int bytes_received = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes_received < 0)
    {
        std::cerr << "Error receiving data from client\n";
        close(client_fd);
        return;
    }

    std::string request(buffer, bytes_received);
    report = processRequest(request, directory /*, keyword*/, client_fd);

    // Send the response to the client
    // send(client_fd, report.c_str(), report.length(), 0);
    close(client_fd);
}
EpollServer::EpollServer(int server_fd, const std::string &directory, size_t thread_count)
    : server_fd(server_fd), directory(directory), pool(thread_count)
{
    // 创建epoll实例
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0)
    {
        std::cerr << "Error: epoll_create1 failed" << std::endl;
        exit(EXIT_FAILURE);
    }

    // 设置服务器套接字为非阻塞
    fcntl(server_fd, F_SETFL, O_NONBLOCK);

    // 初始化epoll事件
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;

    // 将服务器套接字添加到epoll监听
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0)
    {
        std::cerr << "Error: epoll_ctl failed" << std::endl;
        exit(EXIT_FAILURE);
    }
}

EpollServer::~EpollServer()
{
    pool.stop();
    close(epoll_fd);
    close(server_fd);
}

void EpollServer::eventLoop()
{
    struct epoll_event events[MAX_EVENTS];
    while (true)
    {
        int numEvents = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < numEvents; ++i)
        {
            if (events[i].data.fd == server_fd)
            {
                struct sockaddr_in client_addr;
                socklen_t client_addr_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
                if (client_fd < 0)
                {
                    std::cerr << "Error: accept failed" << std::endl;
                    continue;
                }

                // 使用线程池来处理新的连接
                pool.enqueue([this, client_fd, client_addr]()
                             { handle_client(client_fd, client_addr, directory); });
            }
        }
    }
}

void EpollServer::start()
{
    eventLoop();
}

int main(int argc, char **argv)
{
    // You can use print statements as follows for debugging, they'll be visible when running tests.
    std::cout << "Logs from your program will appear here!\n";
    std::string directory;
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--directory" && i + 1 < argc)
        {
            directory = argv[++i];
            break;
        }
    }

    if (directory.empty())
    {
        std::cout << "Default directory set to: " << directory << std::endl;
    }

    // Uncomment this block to pass the first stage
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        std::cerr << "Failed to create server socket\n";
        return 1;
    }
    //
    // // Since the tester restarts your program quite often, setting REUSE_PORT
    // // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0)
    {
        std::cerr << "setsockopt failed\n";
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(4221);
    //
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    {
        std::cerr << "Failed to bind to port 4221\n";
        return 1;
    }
    //
    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0)
    {
        std::cerr << "listen failed\n";
        return 1;
    }

    // 输出等待客户端连接的消息
    std::cout << "Waiting for a client to connect...\n";

    // 创建EpollServer实例并启动
    EpollServer server(server_fd, directory, thread_count);
    server.start(); // 启动事件循环

    // 关闭服务器套接字（服务器正常运行时不会执行到这一步）
    close(server_fd);

    return 0;

    /*
    size_t thread_count = std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 4;
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
    std::cout << "Waiting for a client to connect...\n";

        while (true) {
            struct sockaddr_in client_addr;
            int client_addr_len = sizeof(client_addr);
            client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
            if (client_fd < 0) {
                std::cerr << "Error accepting connection\n";
                continue; // Skip to the next iteration if accept fails
            }

            // Create a new thread to handle the client
            pool.enqueue(handle_client, client_fd, client_addr, directory);
            //std::thread client_thread(handle_client, client_fd, client_addr, directory);
            //client_thread.detach(); // Detach the thread to let it run independently
            //EpollServer server(server_fd, directory, thread_count);
        // 启动事件循环
        //server.start();
        }

        // Close the server socket when done (not reached in this example)
        close(server_fd);

        return 0;
        */
}
