package com.cgcl.common.netty.client;

import io.netty.bootstrap.Bootstrap;
import io.netty.channel.Channel;
import io.netty.channel.ChannelFuture;
import io.netty.channel.EventLoopGroup;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.nio.NioSocketChannel;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;

import javax.annotation.PostConstruct;
import javax.annotation.PreDestroy;
import java.io.BufferedReader;
import java.io.InputStreamReader;

/**
 * <p>
 *
 * </p>
 *
 * @author Liu Cong
 * @since Created in 2019/3/5
 */
//@Component
@Slf4j
public class MemoryInfoReceiveClient {

    private EventLoopGroup group = new NioEventLoopGroup();

    @Value("${netty.client.host}")
    private String host;
    @Value("${netty.client.port}")
    private Integer port;

    @PostConstruct
    public void start() {
        try {
            Bootstrap bootstrap = new Bootstrap()
                    .group(group)
                    .channel(NioSocketChannel.class)
                    .handler(new MemoryInfoReceiveClientHandler());
            ChannelFuture future = bootstrap.connect(host, port).sync();
            if (future.isSuccess()) {
                log.info("启动 Netty Client");
            }
        } catch (Exception e) {
            group.shutdownGracefully();
            e.printStackTrace();
        } finally {
            group.shutdownGracefully();
        }
    }

    @PreDestroy
    public void destory() throws InterruptedException {
        group.shutdownGracefully().sync();
        log.info("关闭 Netty Client");
    }

    public static void main(String[] args) {
        EventLoopGroup group = new NioEventLoopGroup();
        String host = "localhost";
        Integer port = 3333;
        try {
            Bootstrap bootstrap = new Bootstrap()
                    .group(group)
                    .channel(NioSocketChannel.class)
                    .handler(new MemoryInfoReceiveClientInitializer());
            ChannelFuture future = bootstrap.connect(host, port).sync();
            if (future.isSuccess()) {
                log.info("启动 Netty Client");
            }
            Channel channel = future.channel();
            BufferedReader in = new BufferedReader(new InputStreamReader(System.in));
            while (true) {
                channel.writeAndFlush(in.readLine() + "\r\n");
            }
        } catch (Exception e) {
            group.shutdownGracefully();
            e.printStackTrace();
        } finally {
            group.shutdownGracefully();
        }
    }
}
