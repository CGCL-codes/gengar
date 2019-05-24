package com.cgcl.web.entity;

import com.cgcl.common.util.JsonUtils;
import lombok.Data;
import org.apache.catalina.LifecycleState;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;

/**
 * <p>
 *
 * </p>
 *
 * @author Liu Cong
 * @since Created in 2019/3/11
 */
@Data
public class MemoryInfo implements Serializable {
    private static final long serialVersionUID = 1L;

    private String id;

    private Long dramUsed;

    private Long dramUnused;

    private Long dramTotal;

    private Long nvmUsed;

    private Long nvmUnused;

    private Long nvmTotal;

    List<AppMemUsage> apps = new ArrayList<>();

    @Override
    public String toString() {
        return JsonUtils.toJson(this);
    }
}
