package com.cgcl.common.component;

import com.cgcl.common.netty.server.MemoryInfoReceiveServerHandler;
import lombok.extern.slf4j.Slf4j;
import org.springframework.stereotype.Component;

import javax.websocket.*;
import javax.websocket.server.ServerEndpoint;
import java.io.IOException;
import java.util.concurrent.CopyOnWriteArraySet;

/**
 * <p>
 * WebSocket组件
 * </p>
 *
 * @author Liu Cong
 * @since Created in 2019/3/5
 */
@ServerEndpoint("/ws")
@Component
@Slf4j
public class WebSocketServer {

    // 使用concurrent包下的线程安全Set，用来存放每一个客户端对应的Session对象
    private static CopyOnWriteArraySet<Session> sessions = new CopyOnWriteArraySet<>();

    // 建立连接时调用
    @OnOpen
    public void open(Session session) throws IOException {
        sessions.add(session);
        log.info("正在连接中...");
        sendMessage(session, MemoryInfoReceiveServerHandler.getPreMemoryInfosWraps());
    }

    // 关闭连接时调用
    @OnClose
    public void close() {
        log.info("断开连接中...");
    }

    // 接收消息时调用
    @OnMessage
    public void message(String message, Session session) {
        log.info("客户端发送的消息：" + message);
    }

    // 出现错误时调用
    @OnError
    public void error(Session session, Throwable error) {
        log.info("发生错误：{}, Session ID：{}", error.getMessage(), session.getId());
        error.printStackTrace();
    }

    /**
     * 发送消息给指定的 session
     *
     * @param session
     * @param message
     */
    public static void sendMessage(Session session, String message) {
        try {
            session.getBasicRemote().sendText(message);
        } catch (IOException e) {
            log.error("发送消息出错：{}", e.getMessage());
            e.printStackTrace();
        }
    }

    /**
     * 群发消息
     *
     * @param message
     */
    public static void broadCastInfo(String message) {
        for (Session session : sessions) {
            if (session.isOpen()) {
                sendMessage(session, message);
            }
        }
    }
}
