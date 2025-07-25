#include "httpresponse.h"

using namespace std;

const unordered_map<string, string> HttpResponse::SUFFIX_TYPE = {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "},
};

const unordered_map<int, string> HttpResponse::CODE_STATUS = {
    { 200, "OK" },          // // 请求成功。一般用于GET与POST请求
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
};

const unordered_map<int, string> HttpResponse::CODE_PATH = {
    { 400, "/400.html" },   // 客户端请求的语法错误，服务器无法理解
    { 403, "/403.html" },   // 有的页面通常需要用户有一定的权限才能访问，如未登录
    { 404, "/404.html" },   // 当你发送请求的 URL 在服务器中找不到该资源，就会出现 404
};

HttpResponse::HttpResponse() {
    code_ = -1;
    path_ = srcDir_ = "";
    isKeepAlive_ = false;
    mmFile_ = nullptr; 
    mmFileStat_ = { 0 };
};

HttpResponse::~HttpResponse() {
    UnmapFile();
}

void HttpResponse::UnmapFile() {
    if(mmFile_) {
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}

void HttpResponse::Init(const string& srcDir, string& path, bool isKeepAlive, int code){
    assert(srcDir != "");
    
    code_ = code;
    isKeepAlive_ = isKeepAlive;
    path_ = path;
    srcDir_ = srcDir;

    if(mmFile_) { UnmapFile(); }
    mmFile_ = nullptr; 
    mmFileStat_ = { 0 };
}

// 判断 HTTP 请求的目标资源是否有效，并据此设置响应状态码（code_）
void HttpResponse::MakeResponse(Buffer& buff) {
    if(stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode)) {
        // 判断目标文件是否存在。stat 返回 -1 表示文件不存在或出错。
        // 判断该路径是否是一个目录（而不是文件）。静态资源请求一般只允许访问文件，如果是目录也返回 404。
        // 如果资源不存在或者是一个目录，则设置状态码为 404 Not Found
        code_ = 404; // 当你发送请求的 URL 在服务器中找不到该资源，就会出现 404
    }
    else if(!(mmFileStat_.st_mode & S_IROTH)) {
        // 如果文件存在但没有权限读取，则设置状态码为 403 Forbidden
        code_ = 403; // 有的页面通常需要用户有一定的权限才能访问，如未登录
    }
    else if(code_ == -1) { 
        code_ = 200; 
    }
    ErrorHtml_();           // 如果code=400/403/404，确保/400.html等文件一定存在
    AddStateLine_(buff);    // 版本号 + 状态码 + 状态码解释
    AddHeader_(buff);       // 协议头header（只有Connection、keep-alive、Content-type）
    AddContent_(buff);      // 正常情况下补充Content-length；无资源和mmap失败时，还额外补充自定义的BODY
}

// 如果code=400/403/404，// 确保/400.html等文件一定存在
void HttpResponse::ErrorHtml_() {
    if(CODE_PATH.count(code_) == 1) {
        path_ = CODE_PATH.find(code_)->second;
        assert(stat((srcDir_ + path_).data(), &mmFileStat_) == 0);
    }
}
// 版本号 + 状态码 + 状态码解释
void HttpResponse::AddStateLine_(Buffer& buff) {
    string status;
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    }
    else {
        code_ = 400;
        status = CODE_STATUS.find(400)->second;
    }
    buff.Append("HTTP/1.1 " + to_string(code_) + " " + status + "\r\n");
}
// 协议头header（只有Connection、keep-alive、Content-type）
void HttpResponse::AddHeader_(Buffer& buff) {
    buff.Append("Connection: ");
    if(isKeepAlive_) {
        buff.Append("keep-alive\r\n");
        // 这个连接能被重用最多 6 次，保持 120 秒，如果超时或次数超过就关闭
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    } else{
        buff.Append("close\r\n");
    }
    buff.Append("Content-type: " + GetFileType_() + "\r\n");
}
// 判断path_文件类型 
string HttpResponse::GetFileType_() {
    // 从字符串末尾向开头搜索，返回匹配字符的最后一个（即最右侧）出现位置
    string::size_type idx = path_.find_last_of('.');
    if(idx == string::npos) {   // 最大值 find函数在找不到指定值得情况下会返回string::npos
        return "text/plain";    // 纯文本格式
    }
    string suffix = path_.substr(idx);  // 获取最后一个.后的扩展名
    if(SUFFIX_TYPE.count(suffix) == 1) {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}
// 正常情况下补充Content-length；无资源和mmap失败时，还额外补充自定义的BODY
void HttpResponse::AddContent_(Buffer& buff) {
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    if(srcFd < 0) { 
        ErrorContent(buff, "File NotFound!");
        return; 
    }

    /*
        将文件映射到内存提高文件的访问速度
        
        参数                含义
        NULL	            让系统自动选择映射的内存地址
        mmFileStat_.st_size	映射的大小，即整个文件大小
        PROT_READ	        映射区域是只读的
        MAP_PRIVATE	        私有映射：写时复制（copy-on-write），不会影响原始文件
        srcFd	            被映射的文件描述符（之前 open() 得到的）
        0	                从文件的起始偏移 0 开始映射

        返回值：一个指向映射内存的地址
    */
    LOG_DEBUG("file path %s", (srcDir_ + path_).data());
    int* mmRet = (int*)mmap(NULL, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0);
    if(mmRet == MAP_FAILED) {
        ErrorContent(buff, "File NotFound!");
        return;
    }
    mmFile_ = (char*)mmRet;
    close(srcFd);
    buff.Append("Content-length: " + to_string(mmFileStat_.st_size) + "\r\n\r\n");
    // 是不是漏掉了正文，应该有一个buff.Append(mmFile_, mmFileStat_.st_size)
    // 没漏，正文封装进了HttpConn::process的iov_[1]
}

void HttpResponse::ErrorContent(Buffer& buff, string message) 
{
    string body;
    string status;
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    if(CODE_STATUS.count(code_) == 1) {
        status = CODE_STATUS.find(code_)->second;
    } else {
        status = "Bad Request";
    }
    body += to_string(code_) + " : " + status  + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>TinyWebServer</em></body></html>";

    buff.Append("Content-length: " + to_string(body.size()) + "\r\n\r\n");
    buff.Append(body);
}

char* HttpResponse::File() {
    return mmFile_;
}

size_t HttpResponse::FileLen() const {
    return mmFileStat_.st_size;
}