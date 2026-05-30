#include <stdio.h>
#include <string.h>

#include "commands.h"
#include "common.h"
#include "controller.h"
#include "error_codes.h"
#include "parser.h"
#include "repl.h"

int repl_print_prompt(void){
    fputs("domotics> ", stdout);
    fflush(stdout);
    return OK;
}

int repl_handle_line(controller *ctrl, const char *line){
    parsed_command cmd;
    int rc;

    if(ctrl == NULL || line == NULL){
        return ERR_INVALID_PARAMETERS;
    }

    rc =parse_command_line(line, &cmd);
    if(rc!=OK){
        return rc;
    }

    return execute_parsed_command(ctrl, &cmd);
}

int repl_read_and_execute(controller *ctrl){
    char line[LINE_MAX];
    int rc;
    if(ctrl ==NULL){
        return ERR_INVALID_PARAMETERS;
    }

    if(fgets(line, sizeof(line), stdin)==NULL){
        ctrl->running=0;
        return OK;
    }

    rc= repl_handle_line(ctrl, line);
    
    return rc;
}