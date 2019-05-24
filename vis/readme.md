
## 介绍
这是一个基于Spring Boot 2.0 的Web程序。

## 环境搭建
* 安装 jdk 8+
* 安装 maven 3+

## 修改配置
* 修改vis\src\main\resources\application.properties里面的配置文件，如下：
* 请把server.port端口修改成你的机器所开放的端口号；
* netty.server.port 为XXX程序监听的端口号。
```
# web app 端口
server.port=8080
# socket监听的端口
netty.server.port=3333
```


## 编译和运行有两种方式
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

## 使用方式
* 在浏览器地址栏输入部署服务器ip和监听的端口port（该端口为上诉的server.port）
* 例如：http://`ip`:`port`
* 如果出现如下截图，则表示运行成功。
* CertifiedGateoneScreenshot.png截图

 到此，基本的Web UI界面已经部署完毕，如果有Web SSH功能需求，请看下面。

------

## 配置 Web SSH 功能（可选项，如果不配置，不影响Web UI正常运行。）
### 环境搭建
* 安装 [GateOne](http://liftoff.github.io/GateOne/About/index.html)
### 认证GateOne
* 在第一次运行GateOne时，它会创建一个默认的配置文件`/gateone/settings/10server.conf`，在这个配置文件中可以找到其监听的端口号`port`。
* 假设安装机器节点的IP地址为`machine_ip`。
* 那么在浏览器地址栏输入`https://machine_ip:port`。
* 如果出现如下截图，则证明GateOne安装成功。
* WebUI.png（截图）
### 修改vis配置
* 在GateOne已经成功运行起来之后，再在`vis/src/main/resources/static/pages/memory-usage/memory-usage-table.js`文件中配置GateOne的访问地址：
```javascript 1.8
var gateOneUrl = "https://machine_ip:port";
```
* 然后按照上诉的编译和运行方式重新执行

### 使用方式
* 在成功运行之后，浏览器会显示一个主页，主页顶部会有一个`Terminal`按钮，点击该按钮便可进入Web SSH界面；
* Note：如果出现认证失败错误，那么请先在浏览器地址栏输入`https://machine_ip:port`，认证之后，再进行上一步操作。

------
