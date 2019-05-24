package com.cgcl.web.entity;

import com.cgcl.common.util.JsonUtils;
import lombok.Data;

import java.io.Serializable;

/**
 * <p>
 *
 * </p>
 *
 * @author Liu Cong
 * @since Created in 2019/3/15
 */
@Data
public class AppMemUsage implements Serializable {

    private static final long serialVersionUID = 1L;

    private String name;
    private Long dramUsed;
    private Long nvmUsed;

    @Override
    public String toString() {
        return JsonUtils.toJson(this);
    }
}
