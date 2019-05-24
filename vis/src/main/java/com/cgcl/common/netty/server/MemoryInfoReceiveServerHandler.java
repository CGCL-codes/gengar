package com.cgcl.common.netty.server;

import com.cgcl.common.component.WebSocketServer;
import com.cgcl.common.util.JsonUtils;
import com.cgcl.common.util.Message;
import com.cgcl.web.entity.AppMemUsage;
import com.cgcl.web.entity.MemoryInfo;
import io.netty.channel.ChannelHandler;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;
import lombok.extern.slf4j.Slf4j;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * <p>
 *
 * </p>
 *
 * @author Liu Cong
 * @since Created in 2019/3/5
 */
@Slf4j
@ChannelHandler.Sharable
public class MemoryInfoReceiveServerHandler extends SimpleChannelInboundHandler<String> {

    private static List<MemoryInfo> preMemoryInfos = new ArrayList<>();

    @Override
    protected void channelRead0(ChannelHandlerContext ctx, String msg) throws Exception {
        log.info("接收的消息为：" + msg);
        try {
            preMemoryInfos = formatMsg(msg);
            log.info(Message.success().add("data", preMemoryInfos).toString());
            WebSocketServer.broadCastInfo(Message.success().add("data", preMemoryInfos).toString());
            ctx.writeAndFlush("success"); // 反馈消息给客户端
        } catch (RuntimeException e) {
            log.warn("解析json出错！");
            ctx.writeAndFlush("error, msg=" + msg);
            e.printStackTrace();
//            ctx.close();
        }
    }

    /**
     * 预处理 msg
     *
     * @param msg socket收到的msg
     * @return List<MemoryInfo>
     */
    private List<MemoryInfo> formatMsg(String msg) {
        Map map = JsonUtils.parse(msg.trim(), Map.class);
        List<MemoryInfo> memoryInfos = new ArrayList<>();
        for (Object item : map.keySet()) {
            String key = (String) item;
            Map value = (Map) map.get(key);
            MemoryInfo memoryInfo = new MemoryInfo();
            memoryInfo.setId(key);
            memoryInfo.setApps(getApps(value.get("apps")));
            Map<String, Long> used = getUsed(memoryInfo.getApps());
            memoryInfo.setDramTotal(Long.parseLong(value.get("dram total size").toString()));
            memoryInfo.setDramUsed(used.get("dramUsed"));
            memoryInfo.setDramUnused(memoryInfo.getDramTotal() - used.get("dramUsed"));
            memoryInfo.setNvmTotal(Long.parseLong(value.get("nvm total size").toString()));
            memoryInfo.setNvmUsed(used.get("nvmUsed"));
            memoryInfo.setNvmUnused(memoryInfo.getDramTotal() - used.get("nvmUsed"));
            memoryInfos.add(memoryInfo);
        }
        return memoryInfos;
    }

    private Map<String, Long> getUsed(List<AppMemUsage> apps) {
        Map<String, Long> rs = new HashMap<>();
        Long nvmUsed = 0L, dramUsed = 0L;
        for (AppMemUsage app : apps) {
            nvmUsed += app.getNvmUsed();
            dramUsed += app.getDramUsed();
        }
        rs.put("nvmUsed", nvmUsed);
        rs.put("dramUsed", dramUsed);
        return rs;
    }

    private List<AppMemUsage> getApps(Object apps) {
        List map = (List) apps;
        List<AppMemUsage> rs = new ArrayList<>();
        for (Object item : map) {
            Map app = (Map) item;
            AppMemUsage appMemUsage = new AppMemUsage();
            appMemUsage.setName(String.valueOf(app.get("app name")));
            appMemUsage.setDramUsed(Long.parseLong(app.get("dram used size").toString()));
            appMemUsage.setNvmUsed(Long.parseLong(app.get("nvm used size").toString()));
            rs.add(appMemUsage);
        }
        return rs;
    }

    @Override
    public void channelRegistered(ChannelHandlerContext ctx) throws Exception {
        log.info(ctx.channel().remoteAddress() + ": Channel Registered");
    }

    @Override
    public void channelUnregistered(ChannelHandlerContext ctx) throws Exception {
        log.info(ctx.channel().remoteAddress() + ": Channel Unregistered");
    }

    @Override
    public void channelActive(ChannelHandlerContext ctx) throws Exception {
        log.info(ctx.channel().remoteAddress() + ": Channel Active");
    }

    @Override
    public void channelInactive(ChannelHandlerContext ctx) throws Exception {
        log.info(ctx.channel().remoteAddress() + ": Channel Inactive");
    }

    @Override
    public void channelReadComplete(ChannelHandlerContext ctx) throws Exception {
        log.info(ctx.channel().remoteAddress() + ": Channel Read Complete");
    }

    @Override
    public void exceptionCaught(ChannelHandlerContext ctx, Throwable cause) throws Exception {
        log.error("Memory information receiver occurs errors!");
        cause.printStackTrace();
        ctx.close();
    }

    public static String getPreMemoryInfosWraps() {
        return Message.success().add("data", preMemoryInfos).toString();
    }
}
