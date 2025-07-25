#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>    // 正则表达式
#include <errno.h>     
#include <mysql/mysql.h>  //mysql
#include <cassert>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"

class HttpRequest {
public:
    enum PARSE_STATE {
        REQUEST_LINE,
        HEADERS,
        BODY,
        FINISH,        
    };
    
    HttpRequest() { Init(); }
    ~HttpRequest() = default;
    void Init();
    // 关键函数
    bool parse(Buffer& buff);   
    // 接口
    bool IsKeepAlive() const;
    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string GetPost(const std::string& key) const;
    std::string GetPost(const char* key) const;

private:
    bool ParseRequestLine_(const std::string& line);    // 处理请求行
    void ParsePath_();                                  // 处理请求路径
    void ParseHeader_(const std::string& line);         // 处理请求头

    void ParseBody_(const std::string& line);           // 处理请求体
    void ParsePost_();                                  // 处理Post事件
    static int ConverHex(char ch);  // 16进制转换为10进制
    void ParseFromUrlencoded_();    // 解析application/x-www-form-urlencoded格式的请求体

    static bool UserVerify(const std::string& name, const std::string& pwd, bool isLogin);  // 来验证登录或注册请求是否成功

private:
    PARSE_STATE state_;
    std::string method_, path_, version_, body_;    // 方法、URL、版本号、正文BODY
    std::unordered_map<std::string, std::string> header_;   // 协议头
    std::unordered_map<std::string, std::string> post_;     // 正文BODY处理后存储

    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
};

#endif

/*
    GET LOG:
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());


    POST LOG:
    // 解析application/x-www-form-urlencoded格式的正文BODY
    LOG_DEBUG("Parsed form data: %s = %s", key.c_str(), value.c_str());
    // 0是注册 1是登录
    LOG_DEBUG("Tag:%d", tag);   
    // UserVerify
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    LOG_DEBUG("%s", order); order是MYSQL指令
        // MYSQL中有这个username
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
            // 登录，但密码不正确
            =LOG_INFO("pwd error!");
            // 注册，但用户名重复
            LOG_INFO("user used!");
        // 注册行为 且 用户名未被使用
        LOG_DEBUG("regirster!");
        LOG_DEBUG( "%s", order);
    LOG_DEBUG( "UserVerify success!!");
    // 正文BODY
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());    
    // 
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());


    ERROR LOG:
    LOG_ERROR("HttpRequest::parse state is default");
    LOG_ERROR("RequestLine Error");
    LOG_DEBUG( "Insert error!");

*/
