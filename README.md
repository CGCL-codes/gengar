# Gengar:Distributed Hybrid Memory Pool

&#160; &#160; &#160; &#160; Gengar, a distributed shared hybrid memory pool with RDMA support. Gengar allows applications to access remote DRAM/NVM in a large and global memory space through a client/server model. We exploit the semantics of RDMA primitives at the client side to identify frequently-accessed (hot) data in the server node, and cache it in a small DRAM buffer. Also, we manage the DRAM buffer with different strategies to maximize its efficiency. To full exploit the high performance RDMA hardware, we redesign the RDMA communication protocol and RPC to multiplex registered Memory Regions, and to overlap RDMA write operations with the execution of applications.

Gengar has achieved following functions:

 * **NVM Allocation and Release**: The application on the Gengar's client controller can request and release NVM memory on the remote server directly through the dhmp_malloc and dhmp_free API.

 * **DRAM Allocation and Release**: For Gengar servers, DRAM is a scarce resource. Therefore, the client cannot apply for DRAM resources through the API, and must send a DRAM request request to the remote server through the sniffing package. The server will decide whether to agree to the client's DRAM application based on the memory pressure condition.

 * **Remote Read/Write**: When a client allocates NVM or DRAM resources from a remote server, it can directly access remote memory resources through the dhmp_read and dhmp_write API interface. Gengar implements remote data access with one-sided RDMA Read/Write Verbs.


Gengar WebUI Setup, Compiling, Configuration and How to use
------------
**1.External Dependencies**  
&#160; &#160; &#160; &#160; Before run the WebUI of Gengar, it's essential that you have already install dependencies listing below.
* jdk 8+
* maven 3+
* [GateOne](http://liftoff.github.io/GateOne/About/index.html)

**2.Setup**
* Modify the configuration in Gengar/vis/src/main/resources/application.properties
* set the web app port and socket port
```javascript
# set the web app port
server.port=8080		//default
# set the socket port
netty.server.port=3333	//default
```

* Modify the websocket ip addr and port(the same to the webUI ip and port) configuration 
* in Gengar/vis/src/main/resources/static/pages/memory-usage/memory-usage-table.js
```javascript
var wsUrl = "ip:port";
var gateOneUrl = "https://your-gateone-server/";
```

**3.Running**
* The first mode of running
```javascript
[user @node1 ~]# cd Gengar/vis/
[user @node1 vis]# mvn package
[user @node1 vis]# cd target
[user @node1 target]# java -jar vis-1.0.0.jar
```

* The second mode of running
```javascript
# use the maven running
[user @node1 ~]# cd Gengar/vis/
[user @node1 vis]# mvn spring-boot:run
```

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
[user @node1 ~]# tar -xvf dhmp.tar
[user @node1 ~]# cd dhmp
[user @node1 dhmp]# mkdir build && cd build
[user @node1 build]# cmake .. && make 	//will generate three executable files(client\watcher\server) on the ~/dhmp/bin
```

* Update configuration to your Gengar's server configuration through ~/dhmp/bin/config.xml
```javascript
[Log Level]
	<log_level>4</log_level>			//ERROR:0; WARN:1; INFO:2; DEBUG:3; TRACE:4;

[Server Configuration]
	<nic_name>ib0</nic_name>			//RDMA Card Name,through ifconfig look up
	<addr>xxx.xxx.xxx.xxx</addr>			//Server's RDMA Card Address
	<port>39300</port>				//Server's listen port
	<rdelay>200</rdelay>				//express the read latency of server's NVM is 200ns
	<wdelay>800</wdelay>				//express the write latency of server's NVM is 800ns
	<knum>1</knum>					//express the CPU parallelism of server is 1
```

**3. Running**

Make sure your server configuration is correct.

First, running all your server according to the dhmp/bin/config.xml configuration.
```javascript
[user @server ~]# cd dhmp/bin
[user @server ~]# ./server
```

Second, running the watcher.
```javascript
[user @watcher ~]# cd dhmp/bin
[user @watcher ~]# ./watcher
```

Finally, running the client.
```javascript
[user @client ~]# cd dhmp/bin
# comand line format:./client size readnum writenum
# for example, the client will allocate the object size is 65536
# and will exec the remote read num is 10000
# and will exec the remote write num is 20000
[user @client ~]# ./client 65536 10000 20000
```

**4. How to use Gengar API**
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

## Support or Contact
If you have any questions, please contact ZhuoHui Duan(zhduan@hust.edu.cn), Haikun Liu (hkliu@hust.edu.cn) and Xiaofei Liao (xfliao@hust.edu.cn).
