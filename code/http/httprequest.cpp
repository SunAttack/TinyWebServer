#include "httprequest.h"
using namespace std;

// 存储界面路径
const unordered_set<string> HttpRequest::DEFAULT_HTML{
            "/index", "/register", "/login",
             "/welcome", "/video", "/picture", };
// 登录、注册
const unordered_map<string, int> HttpRequest::DEFAULT_HTML_TAG {
            {"/register.html", 0}, {"/login.html", 1},  };

void HttpRequest::Init() {
    state_ = REQUEST_LINE;
    method_ = path_ = version_ = body_ = "";
    header_.clear();
    post_.clear();
}

bool HttpRequest::IsKeepAlive() const {
    if(header_.count("Connection") == 1) {
        return header_.find("Connection")->second == "keep-alive" && version_ == "1.1";
    }
    return false;
}

// 对Buffer进行首行、协议头HEADER、空格、正文BODY解析
bool HttpRequest::parse(Buffer& buff) {
    const char CRLF[] = "\r\n";      // 行结束符标志(回车换行)
    if(buff.ReadableBytes() <= 0) { // 没有可读的字节
        return false;
    }
    // 读取数据
    while(buff.ReadableBytes() && state_ != FINISH) {
        // std::search(first1, last1, first2, last2)
        // C++ STL <algorithm> 中的函数，用来查找 [first2, last2) 在 [first1, last1) 中第一次出现的位置。
        // 返回该子序列的起始位置（指向 \r 的指针）。
        const char* lineEnd = search(buff.Peek(), buff.BeginWriteConst(), CRLF, CRLF + 2);
        // 转化为string类型
        std::string line(buff.Peek(), lineEnd);
        /*
            有限状态机，从请求行开始，每处理完后会自动转入到下一个状态    
        */
        switch(state_)
        {
        case REQUEST_LINE:
            if(ParseRequestLine_(line) == false) {
                return false;
            }
            ParsePath_();   // 解析路径
            break;    
        case HEADERS:
            ParseHeader_(line);
            if(buff.ReadableBytes() <= 2) { // 换行\r\n + 请求数据 肯定要大于 > 2
                state_ = FINISH;
            }
            break;
        case BODY:
            ParseBody_(line);
            break;
        default:
            LOG_ERROR("HttpRequest::parse state is default");
            break;
        }
        // 改动
        if (state_ != FINISH) buff.RetrieveUntil(lineEnd + 2); // 读指针跳过回车换行
    }
    buff.RetrieveAll(); // 清空好习惯
    LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), path_.c_str(), version_.c_str());
    return true;
}



// 处理请求行
bool HttpRequest::ParseRequestLine_(const string& line) {
    // 使用正则表达式解析HTTP请求行：方法 路径 HTTP版本
    // ([^ ]*)：第一个捕获组，匹配任意非空格字符（请求方法）
    // ([^ ]*)：第二个捕获组，匹配路径
    // HTTP/：字面匹配 "HTTP/"
    // ([^ ]*)$：第三个捕获组，匹配版本号直到行尾  
    regex patten("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$"); 
    smatch subMatch;

    // 检查请求行是否匹配正则表达式
    if(regex_match(line, subMatch, patten)) {
        // 提取匹配结果：[0]是整个匹配，[1]-[3]是三个捕获组    
        method_ = subMatch[1];
        path_ = subMatch[2];
        version_ = subMatch[3];
        
        // 状态转换为解析头部
        state_ = HEADERS; 
        return true;
    }

    // 解析失败，记录错误日志
    LOG_ERROR("RequestLine Error");
    return false;
}

// 解析路径
void HttpRequest::ParsePath_() {
    if(path_ == "/") {  
        path_ = "/index.html"; 
    }
    else {
        for(auto &item: DEFAULT_HTML) {
            if(item == path_) {
                path_ += ".html";
                break;
            }
        }
    }
}

void HttpRequest::ParseHeader_(const string& line) {
    // 使用正则表达式解析HTTP头部字段：字段名: 值
    // ^：匹配行的开始
    // ([^:]*)：第一个捕获组，匹配任意非冒号字符（字段名）
    // : ?：匹配冒号和可选的空格
    // (.*)$：第二个捕获组，匹配剩余的所有字符（字段值）
    regex patten("^([^:]*): ?(.*)$");
    smatch subMatch;
    if(regex_match(line, subMatch, patten)) {
        header_[subMatch[1]] = subMatch[2];
    }
    else {
        // 遇到空行（即"\r\n"），表示头部结束，状态转换为解析BODY
        state_ = BODY;  
    }
}

void HttpRequest::ParseBody_(const string& line) {
    body_ = line;
    ParsePost_();
    state_ = FINISH;    // 状态转换为下一个状态
    LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

// 处理post请求
void HttpRequest::ParsePost_() {
    // application/x-www-form-urlencoded这个格式的核心规则( body )是：
    // 键值对：key1=value1&key2=value2
    if(method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded") {
        // 解析application/x-www-form-urlencoded格式的 POST请求体正文BODY
        ParseFromUrlencoded_();

        // // 如果是登录/注册的path(URL)
        if(DEFAULT_HTML_TAG.count(path_)) { 
            int tag = DEFAULT_HTML_TAG.find(path_)->second; 
            LOG_DEBUG("Tag:%d", tag);
            if(tag == 0 || tag == 1) {
                bool isLogin = (tag == 1);  // 为1则是登录
                if(UserVerify(post_["username"], post_["password"], isLogin)) {
                    path_ = "/welcome.html";
                } 
                else {
                    path_ = "/error.html";
                }
            }
        }
    }   
}

// 16进制字符转换为对应的数值（0-15）
int HttpRequest::ConverHex(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';        // 数字字符
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;   // 大写字母
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;   // 小写字母
    return 0;  // 非法字符，默认返回0（实际应抛出异常或日志警告）
}
/*
    功能：// 解析application/x-www-form-urlencoded格式的正文BODY
    
    这个格式的核心规则是：
    键值对：key1=value1&key2=value2
    编码规则：
    空格 → + 或 %20
    非 ASCII 字符 → %XX（如中文 %E4%B8%AD）
    保留字符（如 &、=）→ 编码为 %XX

    函数中的关键操作：
    处理 +：将 + 还原为空格（body_[i] = ' '）。
    处理 %XX：通过 ConverHex 函数将十六进制还原为字符（如 %20 → ）。
    分割键值对：通过 = 和 & 分割字符串，存入 post_ 映射。
*/
void HttpRequest::ParseFromUrlencoded_() {
    if (body_.empty()) return;

    std::string decoded; // 存储解码后的字符串
    decoded.reserve(body_.size()); // 预分配空间

    // 第一步：解码URL编码的字符串
    for (size_t i = 0; i < body_.size(); ++i) {
        if (body_[i] == '+') {
            decoded += ' '; // '+'转换为空格
        } else if (body_[i] == '%' && i + 2 < body_.size()) {
            // 解析%XX格式的编码
            // 例如%20 ->十进制32 -> space(char)
            char high = ConverHex(body_[i + 1]);
            char low = ConverHex(body_[i + 2]);
            decoded += static_cast<char>((high << 4) | low);
            i += 2; // 跳过两位十六进制字符
        } else {
            decoded += body_[i]; // 普通字符直接添加
        }
    }

    // 第二步：解析解码后的键值对
    size_t i = 0, j = 0;
    std::string key, value;
    bool has_key = false;

    for (; i <= decoded.size(); ++i) {
        // 处理字符串结束或遇到键值对分隔符
        if (i == decoded.size() || decoded[i] == '&') {
            if (has_key) {
                value = decoded.substr(j, i - j);
                post_[key] = value;
                LOG_DEBUG("Parsed form data: %s = %s", key.c_str(), value.c_str());
                has_key = false;
            }
            j = i + 1; // 移动到下一个键值对
        }
        // 处理键值分隔符
        else if (decoded[i] == '=' && !has_key) {
            key = decoded.substr(j, i - j);
            j = i + 1;
            has_key = true;
        }
    }
    // assert(j - i == 1);
}

// 来验证登录或注册请求是否成功
bool HttpRequest::UserVerify(const string &name, const string &pwd, bool isLogin) {
    // 第一步：检查用户名和密码非空
    if(name == "" || pwd == "") { return false; }
    LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
    
    // 第二步：获取数据库连接（通过 RAII 自动管理资源）
    MYSQL* sql;
    SqlConnRAII(&sql,  SqlConnPool::Instance());
    assert(sql);
    
    bool flag = false;
    if(!isLogin) { flag = true; }
    // 第三步：构造 SQL 查询语句
    char order[256] = { 0 };
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", name.c_str());
    LOG_DEBUG("%s", order);
    // 第四步：执行 SQL 查询语句
    MYSQL_RES *res = nullptr;
    if(mysql_query(sql, order)) { 
        mysql_free_result(res);
        return false; 
    }
    res = mysql_store_result(sql);
    unsigned int j = mysql_num_fields(res);
    MYSQL_FIELD *fields = mysql_fetch_fields(res);
    // 第五步：处理查询结果（是否已有这个用户名）
    while(MYSQL_ROW row = mysql_fetch_row(res)) {
        // 查询到有这个用户名
        LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        string password(row[1]);
        
        if(isLogin) {
            if(pwd == password) { 
                // 登录，且密码正确
                flag = true; 
            }
            else {
                // 登录，但密码不正确
                flag = false;
                LOG_INFO("pwd error!");
            }
        } 
        else { 
            // 注册，但用户名重复
            flag = false; 
            LOG_INFO("user used!");
        }
    }
    mysql_free_result(res);

    /* 注册行为 且 用户名未被使用*/
    if(!isLogin && flag == true) {
        LOG_DEBUG("regirster!");
        bzero(order, 256);
        snprintf(order, 256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        LOG_DEBUG( "%s", order);
        if(mysql_query(sql, order)) { 
            LOG_DEBUG( "Insert error!");
            flag = false; 
        }
        else flag = true;
    }
    // SqlConnPool::Instance()->FreeConn(sql);
    LOG_DEBUG( "UserVerify success!!");
    return flag;
}

std::string HttpRequest::path() const{
    return path_;
}

std::string& HttpRequest::path(){
    return path_;
}
std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

std::string HttpRequest::GetPost(const std::string& key) const {
    assert(key != "");
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}

std::string HttpRequest::GetPost(const char* key) const {
    assert(key != nullptr);
    if(post_.count(key) == 1) {
        return post_.find(key)->second;
    }
    return "";
}
