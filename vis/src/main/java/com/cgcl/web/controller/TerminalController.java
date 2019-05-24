package com.cgcl.web.controller;

import org.springframework.stereotype.Controller;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;

/**
 * <p>
 *
 * </p>
 *
 * @author Liu Cong
 * @since Created in 2019/3/15
 */
@Controller
@RequestMapping("terminal")
public class TerminalController {

    @GetMapping
    public String openTerminal() {
        return "terminal/terminal";
    }
}
