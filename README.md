# Gengar:Distributed Hybrid Memory Pool

&#160; &#160; &#160; &#160; Gengar, a distributed shared hybrid memory pool with RDMA support. Gengar allows applications to access remote DRAM/NVM in a large and global memory space through a client/server model. We exploit the semantics of RDMA primitives at the client side to identify frequently-accessed (hot) data in the server node, and cache it in a small DRAM buffer. Also, we manage the DRAM buffer with different strategies to maximize its efficiency. To full exploit the high performance RDMA hardware, we redesign the RDMA communication protocol and RPC to multiplex registered Memory Regions, and to overlap RDMA write operations with the execution of applications.

Gengar has achieved following functions:

 * **NVM Allocation and Release**: The application on the Gengar's client controller can request and release NVM memory on the remote server directly through the dhmp_malloc and dhmp_free API.

 * **DRAM Allocation and Release**: For Gengar servers, DRAM is a scarce resource. Therefore, the client cannot apply for DRAM resources through the API, and must send a DRAM request request to the remote server through the sniffing package. The server will decide whether to agree to the client's DRAM application based on the memory stress condition.

 * **Remote Read/Write**: When a client allocates NVM or DRAM resources from a remote server, it can directly access remote memory resources through the dhmp_read and dhmp_write API interface. Gengar implements remote data access with one-sided RDMA Read/Write Verbs.

## Citing Gengar

If you use Gengar, please cite our research paper published at ICDCS 2021.

**Zhuohui Duan, Haikun Liu, Haodi Lu, Xiaofei Liao, Hai Jin, Yu Zhang, Bingsheng He, Gengar: An RDMA-based Distributed Hybrid Memory Pool, in: Proceedings of the 41th IEEE International Conference on Distributed Computing Systems(ICDCS), 2021**

The Web Image of Gengar Servers Memory Usage 
------------
![image](https://github.com/coderex2522/gengar/blob/master/images/gengar.jpg)

**DRAM Used**: indicates the total amount of DRAM used by the current server to all remote clients.  
**DRAM Unused**: indicates the remaining DRAM space of the current server.  
**DRAM Total**: indicates the total DRAM space owned by the current server.  
**NVM Used**: indicates the total amount of NVM used by the current server to all remote clients.  
**NVM Unused**: indicates the remaining NVM space of the current server.  
**NVM Total**: indicates the total NVM space owned by the current server.  
**Action Button**: shows the proportion of memory resources used by each client in the occupied server memory space.  

Note:Run Gengar WebUI and Gengar
------------
1. Gengar.tar contains the dhmp dir(Distributed Hybrid Memory Pool=>dhmp project) and vis dir(Gengar WebUI project).
2. You must run the Gengar WebUI before you can run Gengar(dhmp project).
3. dhmp project needs to be built and run on each server and one watcher.
4. Gengar WebUI project needs to be built and run on one machine.

Gengar WebUI Setup, Compiling, Configuration and How to use
------------
**1.External Dependencies**  
&#160; &#160; &#160; &#160; Before run the WebUI of Gengar, it's essential that you have already install dependencies listing below.
* jdk 8+
* maven 3+

**2.Setup**
* Decompression the Gengar.tar into the dhmp dir and vis dir
```javascript
* the dhmp dir use for running the Gengar's client, watcher and server
* the vis dir use for running the Gengar's WebUI
[user @node1 ~]# tar -xvf Gengar.tar
```

* Modify the configuration in Gengar/vis/src/main/resources/application.properties
* Set the web app port to be the same as the port your server is open to.
* Set the socket port to be the same as the watcher port in the configuration file (Gengar/dhmp/bin/config.xml) 
```javascript
# set the web app port
server.port=8080		
# set the socket port, the socket port use for Gengar watcher connecting.
netty.server.port=3333	//default
```

**3.Running**
**There are two ways of running the WebUI.**
* The first way of running by JAVA.
```javascript
[user @node1 ~]# cd Gengar/vis/
[user @node1 vis]# mvn package
[user @node1 vis]# cd target
[user @node1 target]# java -jar vis-1.0.0.jar
```

* The second way of running by Maven.
```javascript
# use the maven running
[user @node1 ~]# cd Gengar/vis/
[user @node1 vis]# mvn spring-boot:run
```

**4.Usage**
* Enter your server ip and listen port in the browser address bar (this port is the `server.port` abovementioned)
* eg：http://`ip`:`port`
* If the screenshot below appears, it means the operation is successful.
![image](https://github.com/coderex2522/gengar/blob/master/images/WebUI.png)

**At this point, the basic Web UI interface has been deployed. If you need Web SSH functionality, please see below.**

**4.Web SSH (Optional)
* Install [GateOne](http://liftoff.github.io/GateOne/About/index.html)
* Certified GateOne: 
* When you run GateOne for the first time, it creates a default configuration file `/gateone/settings/10server.conf`, where you can find the port number `port` it listens on.
* Assume that the IP address of the installed machine node is `machine_ip`.
* Then enter `https://machine_ip:port` in the browser address bar.
* If the screenshot below appears, it proves that the GateOne installation is successful.
![image](https://github.com/coderex2522/gengar/blob/master/images/CertifiedGateoneScreenshot.png)
* After GateOne has been successfully run, configure the address of GateOne in the `vis/src/main/resources/static/pages/terminal/terminal.js
` file:
```javascript 1.8
function initWebShell() {
  GateOne.init({
    url: "https://machine_ip:port"
  });
}
```
* Then re-execute according to the compilation and operation of Step 3.
* After successful operation, the browser will display a home page. There will be a `Terminal` button at the top of the home page, click this button to enter the Web SSH interface.
* Note: If an authentication failure error occurs, please enter `https://machine_ip:port` in the address bar of the browser, and then try again.

Gengar Setup,Compiling,Configuration and How to use
------------
**1.External Dependencies**  
&#160; &#160; &#160; &#160; Before run hybrid memory pool Gengar, it's essential that you have already install dependencies listing below.
* gcc(>=4.6)
* numactl-devel
* libxml2

**2.Compiling**

First, Compiling the Gengar's project. The compile process of client, watcher and server is the same.

```javascript
[user @node1 Gengar]# cd dhmp
[user @node1 dhmp]# mkdir build && cd build
[user @node1 build]# cmake .. && make 	//will generate three executable files(client\watcher\server) on the Gengar/dhmp/bin
```

* Update configuration to your Gengar's server configuration through Gengar/dhmp/bin/config.xml
```javascript
[Client Log Level]
<client>
	<log_level>4</log_level>			//ERROR:0; WARN:1; INFO:2; DEBUG:3; TRACE:4;
</client>

[Watcher Configuration]
<watcher>
	<addr>127.0.0.1</addr>				//connect to the webui. if vis run the machine same as the watcher, this can use the addr "127.0.0.1"
    	<port>3333</port>				//connect to the webui socket port
</watcher>

# Note:Each pair of labels <server> represents a server. Each Server must be running.
[Server Configuration]
<server>
	<nic_name>ib0</nic_name>			//RDMA Card Name,through ifconfig look up
	<addr>xxx.xxx.xxx.xxx</addr>			//Server's RDMA Card Address
	<port>39300</port>				//Server's listen port
	<rdelay>200</rdelay>				//express the read latency of server's NVM is 200ns
	<wdelay>800</wdelay>				//express the write latency of server's NVM is 800ns
	<knum>1</knum>					//express the CPU parallelism of server is 1
</server>
```

**3. Running**

Make sure your server configuration is correct.

First, running all your server according to the Gengar/dhmp/bin/config.xml configuration.
```javascript
[user @server Gengar]# cd dhmp/bin
[user @server bin]# ./server
```

Second, running the watcher.
```javascript
[user @watcher Gengar]# cd dhmp/bin
[user @watcher bin]# ./watcher
```

Finally, you can write client programs using the Gengar API.   
**How to use Gengar API**
```javascript
#include <dhmp.h>
/**
 *	dhmp_malloc: remote alloc the size of length region
 */
void *dhmp_malloc(size_t length);

/**
 *	dhmp_read:read the data from dhmp_addr, write into the local_buf
 */
int dhmp_read(void *dhmp_addr, void * local_buf, size_t count);

/**
 *	dhmp_write:write the data in local buf into the dhmp_addr position
 */
int dhmp_write(void *dhmp_addr, void * local_buf, size_t count);

/**
 *	dhmp_free:release remote memory
 */
void dhmp_free(void *dhmp_addr);
```
**Sample code**
```javascript
#include <stdio.h>
#include <dhmp.h>

int main(int argc,char *argv[])
{
	void *addr;
	int size=65536;
	char *str;

	str=malloc(size);
	snprintf(str, size, "hello world");
	
	dhmp_client_init();

	addr=dhmp_malloc(size);
	
	dhmp_write(addr, str, size);

	dhmp_read(addr, str, size);
	
	dhmp_free(addr);
	
	dhmp_client_destroy();

	return 0;
}
```

**4. How to test Gengar API**

Benchmark: We allocate 30,000 objects of size 512KB and perform 100,000 reads and 100,000 writes to the remote server.
```javascript
[user @client Gengar]# cd dhmp/bin
[user @client bin]# ./test.sh
```
