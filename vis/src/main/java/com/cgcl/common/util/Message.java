package com.cgcl.common.util;

import java.io.Serializable;
import java.util.HashMap;
import java.util.Map;

/**
 * <p>
 * HTTP 返回信息的封装
 * </p>
 *
 * @author Liu Cong
 * @since Created in 2019/3/11
 */
public class Message implements Serializable {
    private static final long serialVersionUID = 1L;

    //状态码
    private int code;
    //提示信息
    private String message;

    //用户要返回给浏览器的数据
    private Map<String, Object> extend = new HashMap<>();

    public static Message success() {
        Message result = new Message();
        result.setCode(100);
        result.setMessage("success");
        return result;
    }

    public static Message fail() {
        Message result = new Message();
        result.setCode(200);
        result.setMessage("error");
        return result;
    }

    public Message add(String key, Object value) {
        this.getExtend().put(key, value);
        return this;
    }

    public int getCode() {
        return code;
    }

    public void setCode(int code) {
        this.code = code;
    }

    public String getMessage() {
        return message;
    }

    public void setMessage(String message) {
        this.message = message;
    }

    public Map<String, Object> getExtend() {
        return extend;
    }

    public void setExtend(Map<String, Object> extend) {
        this.extend = extend;
    }

    @Override
    public String toString() {
        return JsonUtils.toJson(this);
    }
}
