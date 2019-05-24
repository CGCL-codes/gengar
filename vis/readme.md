
## 环境搭建
* 安装 jdk 8+
* 安装 maven 3+
* 安装 [GateOne](http://liftoff.github.io/GateOne/About/index.html)

## 修改配置
* 修改vis\src\main\resources\application.properties里面的配置文件，如下：
```
# web app 端口
server.port=8080
# socket监听的端口
netty.server.port=3333
```

* 修改vis/src/main/resources/static/pages/memory-usage/memory-usage-table.js里面的websocket的ip地址和端口号代码，必须和在浏览器访问web页面的ip和端口一致：
* 同时也修改 gataone 的访问地址：
```javascript 1.8
var wsUrl = "ip:port";
var gateOneUrl = "https://your-gateone-server/";
```

## 运行方式
### 第一种：打包成jar包运行
1.进入vis项目的根目录，然后运行`mvn package`命令把工程打包成jar；
```cmd
vis\: mvn package
```
2.运行jar包：maven默认打包在target目录，进入vis\target目录，然后运行`java -jar vis.jar`
```cmd
vis\target\: java -jar vis-1.0.0.jar
```

### 第二种：使用maven运行
进入vis项目的根目录，然后运行`mvn spring-boot:run`命令。
```cmd
vis\: mvn spring-boot:run
```

