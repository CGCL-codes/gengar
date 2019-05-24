package com.cgcl;

import com.cgcl.common.util.JsonUtils;
import com.cgcl.web.entity.MemoryInfo;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.test.context.junit4.SpringRunner;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

@RunWith(SpringRunner.class)
@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.DEFINED_PORT)
public class ApplicationTests {

    @Test
    public void contextLoads() {
        String json = "{ " +
                "\"server1\": { \"dram free size\": 100, \"dram total size\": 1100, \"nvm free size\": 200, \"nvm total size\": 1200 }, " +
                "\"server2\": { \"dram free size\": 300, \"dram total size\": 1500, \"nvm free size\": 400, \"nvm total size\": 1600 }, " +
                "\"server3\": { \"dram free size\": 500, \"dram total size\": 1900, \"nvm free size\": 600, \"nvm total size\": 2000 }, " +
                "\"server4\": { \"dram free size\": 700, \"dram total size\": 2300, \"nvm free size\": 800, \"nvm total size\": 2400 } " +
                "}";
        Map map = JsonUtils.parse(json.trim(), Map.class);
        for (Object item : map.keySet()) {
            String key = (String) item;
            Map value = (Map) map.get(key);
            MemoryInfo memoryInfo = new MemoryInfo();
            memoryInfo.setId(key);
            memoryInfo.setDramUnused(Long.parseLong(value.get("dram free size").toString()));
            memoryInfo.setDramTotal(Long.parseLong(value.get("dram total size").toString()));
            memoryInfo.setDramUsed(memoryInfo.getDramTotal() - memoryInfo.getDramUnused());
            memoryInfo.setNvmUnused(Long.parseLong(value.get("nvm free size").toString()));
            memoryInfo.setNvmTotal(Long.parseLong(value.get("nvm total size").toString()));
            memoryInfo.setNvmUsed(memoryInfo.getNvmTotal() - memoryInfo.getNvmUnused());
            System.out.println(memoryInfo);
        }
    }

}
