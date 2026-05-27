#include <stdio.h>

#include "controller.h"
#include "error_codes.h"

int main(void){
    controller ctrl;
    int rc;
    int cleanup_rc;

    rc= controller_init(&ctrl);
    if(rc!=OK){
        fprintf(stderr, "controller_init feiled with code %d\n",rc);
        return rc;
    }

    rc = controller_run(&ctrl);
    if(rc!=OK){
        fprintf(stderr, "controller_run failed with code %d\n", rc);
    }

    cleanup_rc =  controller_destroy(&ctrl);
    if(cleanup_rc!=OK){
        fprintf(stderr, "controller_destroy failed with code %d\n", cleanup_rc);
        if(rc==OK){
            rc = cleanup_rc;
        }
    }

    return rc;
}