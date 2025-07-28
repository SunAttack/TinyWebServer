# readme

## buffer

**buffer**
从陈硕的 Blog----Muduo 设计与实现之一：Buffer 类的设计来看，此处Buffer省略了很多功能，同时又多了一些莫名其妙的东西
https://www.cnblogs.com/Solstice/archive/2011/04/17/2018801.html

但程设计讲究“先能用，再优化”

## log

日志文件的创建那里感觉不太对

## test

目前只有buffer、log、threadpool的测试

## HTTP

完成了HTTP的请求和响应，以及“连接”.


## timer

完成了时间堆（小根堆+定时器），管理大量定时事件的一种高效数据结构.


## webserver

这是整个WebServer最顶层的接口，每连接进来一个用户，就有一个新的套接字创建并加入epoller中，并且给该用户创建一个时间结点，加入时间堆中，同时将该用户加入users哈希表中（key:fd,value:HttpConn)


## 准备工作

重新安装MYSQL
sudo apt-get update
sudo apt-get install mysql-server
sudo apt-get install libmysqlclient-dev

bash
sudo mysql -u root -p

mysql
SHOW DATABASES;
CREATE DATABASE webserver_db;
CREATE USER 'webserver_user'@'%' IDENTIFIED BY '123456';
SELECT User FROM mysql.user;
GRANT ALL PRIVILEGES ON webserver_db.* TO 'webserver_user'@'%';

bash（测试连接）
mysql -u webserver_user -p -P 3306 -h localhost

mysql
USE webserver_db;
CREATE TABLE user(
    username char(50) NULL,
    password char(50) NULL
)ENGINE=InnoDB;
INSERT INTO user(username, password) VALUES('name', 'password');

修改JehanRio/TinyWebServer的main.cpp如下：
3306, "webserver_user", "123456", "webserver_db", /* Mysql配置 */

项目启动bash
make
./bin/server
会自动打开一个浏览器

压力测试
sudo apt-get install libtirpc-dev
sudo apt install libnsl-dev
#include <rpc/types.h>改为#include <tirpc/rpc/types.h>
WEBBENCH-1.5的makefile里面添加
CFLAGS+= -I/usr/include/tirpc
LIBS+= -ltirpc -lnsl
两个终端
①需要先启动服务器./bin/server
②另一个终端
./webbench-1.5/webbench -c 100 -t 10 http://localhost:1316/
./webbench-1.5/webbench -c 1000 -t 10 http://localhost:1316/
./webbench-1.5/webbench -c 5000 -t 10 http://localhost:1316/
./webbench-1.5/webbench -c 10000 -t 10 http://localhost:1316/
