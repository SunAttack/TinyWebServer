#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <unordered_map>
#include <fcntl.h>   // open()、O_RDONLY
#include <unistd.h>  // close()、perror()
#include <sys/stat.h>    // stat
#include <sys/mman.h>    // mmap, munmap

#include "../buffer/buffer.h"
#include "../log/log.h"

class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();
    void UnmapFile();

    // 关键函数
    void MakeResponse(Buffer& buff);
    // 接口
    void Init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);
    char* File();
    size_t FileLen() const;
    int Code() const { return code_; }
    
    void ErrorContent(Buffer& buff, std::string message);

private:
    void ErrorHtml_();
    void AddStateLine_(Buffer &buff);
    void AddHeader_(Buffer &buff);
    std::string GetFileType_();
    void AddContent_(Buffer &buff);

private:
    int code_;              // HTTP状态码
    std::string srcDir_;
    std::string path_;
    bool isKeepAlive_;
    
    char* mmFile_; 
    struct stat mmFileStat_;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;  // 后缀类型集
    static const std::unordered_map<int, std::string> CODE_STATUS;          // 编码状态集
    static const std::unordered_map<int, std::string> CODE_PATH;            // 编码路径集
};


#endif //HTTP_RESPONSE_H

