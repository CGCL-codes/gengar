package com.cgcl.web.controller;

import org.springframework.stereotype.Controller;
import org.springframework.web.bind.annotation.RequestMapping;

/**
 * <p>
 *
 * </p>
 *
 * @author Liu Cong
 * @since Created in 2019/2/28
 */
@Controller
public class MemoryUsageController {

//    @RequestMapping("/memoryUsage")
//    public String memoryUsage(){
//        return "memory-usage/memory-usage";
//    }

    @RequestMapping("/memory")
    public String memoryUsageTable(){
        return "memory-usage/memory-usage-table";
    }
}
