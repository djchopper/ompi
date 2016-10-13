/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * Copyright (c) 2015	     Politecnico di Milano.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orte_config.h"
#include "orte/constants.h"
#include "orte/types.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "orte/util/show_help.h"
#include "orte/mca/state/state.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/runtime/orte_globals.h"
#include "orte/util/name_fns.h"
#include "orte/mca/ras/base/ras_private.h"
#include "orte/mca/rmaps/base/base.h"
#include "ras_bbque.h"
#include "ras_bbque_protocol.h"

/*
 * Hard-coded constants
 */
#define ORTE_RAS_BBQUE_ENV_PORT  "BBQUE_PORT"
#define ORTE_RAS_BBQUE_ENV_IP    "BBQUE_IP" 

/*
 * Local variables
 */
static int cmd_received;
static int socket_fd;
static opal_event_t recv_ev;
static orte_job_t *received_job=NULL;
static opal_list_t nodes;

/*
 * Local function prototypes
 */
static int  orte_ras_bbque_init(void);
static int  orte_ras_bbque_recv_data(int fd, short args, void *cbdata);
static int  orte_ras_bbque_allocate(orte_job_t *jdata, opal_list_t *nodes);
static int  orte_ras_bbque_finalize(void);
static int  orte_ras_bbque_recv_nodes_reply(void);
static void orte_ras_bbque_launch_job(void);
static int  orte_ras_bbque_parse_new_cmd(void);
static int  orte_ras_bbque_send_node_request(void);
static int  orte_ras_bbque_send_terminate(void);

/*
 * Global module
 */
orte_ras_base_module_t orte_ras_bbque_module = {
    orte_ras_bbque_init,
    orte_ras_bbque_allocate,
    NULL,
    orte_ras_bbque_finalize
};

/**
 * Get the IP address and the port from the environemnts variables and try to
 * connect to the BarbequeRTRM instance. In case of success, instantiates the
 * events in order to recv/send bytes in the socket.
 */
static int orte_ras_bbque_init(void){
    
    short bbque_port;
    char *bbque_addr;
    struct sockaddr_in addr;
    
    /*Check if the environment variables needed to use BBQ are set*/
    if (0 == (bbque_port = atoi(getenv(ORTE_RAS_BBQUE_ENV_PORT))))
    {
        orte_show_help("help-ras-bbque.txt", "bbque-env-var-missing", true, 
                       ORTE_RAS_BBQUE_ENV_PORT);
        return ORTE_ERROR;
    }
    
    if (NULL == (bbque_addr = getenv(ORTE_RAS_BBQUE_ENV_IP)))
    {
        orte_show_help("help-ras-bbque.txt", "bbque-env-var-missing", true, 
                       ORTE_RAS_BBQUE_ENV_IP);
        return ORTE_ERROR;
    }   
    
    /*If so, create a socket and connect to BBQ*/
    
    if(0 > (socket_fd=socket(AF_INET,SOCK_STREAM,0)))
    {
            ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
            return ORTE_ERR_OUT_OF_RESOURCE;
    }
    
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(bbque_addr);
    addr.sin_port = htons(bbque_port);
    
    if(0>connect(socket_fd,(struct sockaddr *)&addr, sizeof(addr)))
    {
        orte_show_help("help-ras-bbque.txt", "bbque-unable-to-connect", true, 
                       strerror(errno));
        return ORTE_ERROR;
    }
    
    /*Tell OMPI framework to call recv_data when it receives something on socket_fd*/
    opal_event_set(orte_event_base, &recv_ev, socket_fd, OPAL_EV_READ, 
                   orte_ras_bbque_recv_data, NULL);
    opal_event_add(&recv_ev, 0);
    
    OBJ_CONSTRUCT(&nodes, opal_list_t);
    
    cmd_received = ORTE_RAS_BBQUE_NONE;
    
    opal_output_verbose(2, orte_ras_base_framework.framework_output,
                    "%s ras:bbque: Module initialized.",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    
    return ORTE_SUCCESS;
}


/*
 * This function is called by the OMPI event management system when some data is
 * received on the socket bound during the initialization of the module. 
 * It basically loops and reads the received commands until it reaches the final
 * message, or an error occurs.
 */
static int orte_ras_bbque_recv_data(int fd, short args, void *cbdata)
{   

    /*cmd_received state drives the execution flow*/
    
    switch(cmd_received) {
    
        case ORTE_RAS_BBQUE_NONE:
        	/* No command received yet, we wait for it */
            if(orte_ras_bbque_parse_new_cmd())
            {
                return ORTE_ERROR;
            }
            break;
        case ORTE_RAS_BBQUE_NODES_REPLY:
            /* We are receiving the node list from BBQ... */
            if(orte_ras_bbque_recv_nodes_reply())
            {
                return ORTE_ERROR;
            }
            break;
        default:
            orte_show_help("help-ras-bbque.txt", "bbque-unknown-command", true, 
                       cmd_received);
        return ORTE_ERROR;

    }

    opal_event_add(&recv_ev, 0);
    return ORTE_SUCCESS;
}

/**
 * This function is called internally by OMPI when mpirun command is executed in
 * order to request a nodes allocation.
 */
static int orte_ras_bbque_allocate(orte_job_t *jdata, opal_list_t *nodes)
{
    received_job=jdata;
    
    orte_ras_bbque_send_node_request();
    
    /*
     * Since we have to wait for BBQUE to send us the nodes list,
     * we notify OMPI that the allocation phase is not over yet.
     */
    return ORTE_ERR_ALLOCATION_PENDING;
}


/**
 * Send the termination command to Barbeque, close the sockets and free the
 * memory.
 */
static int orte_ras_bbque_finalize(void)
{
    orte_ras_bbque_send_terminate();
    
    opal_event_del(&recv_ev);
    
    if (received_job != NULL) {
        OBJ_DESTRUCT(received_job);
    }
    shutdown(socket_fd, 2);
    close(socket_fd);
    
    return ORTE_SUCCESS;
}

/**
 * This function is called after receiving the header of nodes reply and it is
 * in charge of queue the nodes. If the message is the last node provided by
 * Barbeque, launch the job.
 */
static int orte_ras_bbque_recv_nodes_reply(void)
{
    int bytes;
    orte_node_t *temp;
    orte_ras_bbque_res_item_t response_item;
    
    bytes = recv(socket_fd,&response_item,sizeof(orte_ras_bbque_res_item_t), MSG_WAITALL);
    if(bytes != sizeof(orte_ras_bbque_res_item_t))
    {   
            orte_show_help("help-ras-bbque.txt", "bbque-protocol-error", true, 
                       sizeof(orte_ras_bbque_res_item_t), bytes);
        return ORTE_ERROR;
    }
    
    temp = OBJ_NEW(orte_node_t);
    
    temp->name = strdup(response_item.hostname);
    temp->slots_inuse = 0;
    temp->slots_max   = 0;
    temp->slots = response_item.slots_available;
    temp->state = ORTE_NODE_STATE_UP;

    opal_list_append(&nodes, &temp->super);
    
    if (response_item.more_items == 0) {
        orte_ras_bbque_launch_job();
        cmd_received = ORTE_RAS_BBQUE_NONE;
    } else {
    	cmd_received = ORTE_RAS_BBQUE_NODES_REPLY;
    }
    
    return ORTE_SUCCESS;
}

/**
 * When the allocation is complete we can launch the job and settings some
 * values for other frameworks.
 */
static void orte_ras_bbque_launch_job(void)
{                    
    /* Insert received nodes into orte list for this job */
    orte_ras_base_node_insert(&nodes, received_job);

    opal_output_verbose(2, orte_ras_base_framework.framework_output,
                "%s ras:bbq: Job allocation complete.",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    OBJ_DESTRUCT(&nodes);

    /* default to no-oversubscribe-allowed for managed systems */
    /* so bbque never allocates more process than slots available. */
    if (!(ORTE_MAPPING_SUBSCRIBE_GIVEN & ORTE_GET_MAPPING_DIRECTIVE(orte_rmaps_base.mapping))) {
        ORTE_SET_MAPPING_DIRECTIVE(orte_rmaps_base.mapping, ORTE_MAPPING_NO_OVERSUBSCRIBE);
    }
    /* flag that the allocation is managed (do not search in hostfile, take the ip from ras) */
    orte_managed_allocation = true;

    ORTE_ACTIVATE_JOB_STATE(received_job, ORTE_JOB_STATE_ALLOCATION_COMPLETE);
}

/**
 * This function is called when the "state machine" is in the initial state, thus
 * no message is received so far or the previous allocation is concluded.
 */
static int orte_ras_bbque_parse_new_cmd(void){
    int bytes;
    orte_ras_bbque_cmd_t response_cmd;
    
    bytes = recv(socket_fd,&response_cmd,sizeof(orte_ras_bbque_cmd_t),  MSG_WAITALL);
    if(bytes != sizeof(orte_ras_bbque_cmd_t))
    {
            orte_show_help("help-ras-bbque.txt", "bbque-protocol-error", true, 
                       sizeof(orte_ras_bbque_cmd_t), bytes);
        return ORTE_ERROR;
    }

    switch(response_cmd.cmd_type)
    {
        case ORTE_RAS_BBQUE_NODES_REPLY:
        {
            opal_output_verbose(5, orte_ras_base_framework.framework_output,
                "%s ras:bbque: BBQUE sent command:ORTE_RAS_BBQUE_NODES_REPLY. Expecting node data. ",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
            cmd_received=ORTE_RAS_BBQUE_NODES_REPLY;
            break;
        }
        default:
            orte_show_help("help-ras-bbque.txt", "bbque-unknown-command", true, 
                       cmd_received);
            return ORTE_ERROR;
    }
    return ORTE_SUCCESS;
}

/**
 * Send the node request to BarbequeRTRM to request the allocation of the job
 */
static int orte_ras_bbque_send_node_request(void)
{
    orte_ras_bbque_job_t job;
    orte_ras_bbque_cmd_t command;
    orte_app_context_t *app;
    int i;
    
    command.flags = 0;

    command.cmd_type = ORTE_RAS_BBQUE_NODES_REQUEST;
    command.jobid = received_job->jobid;
    
    opal_output_verbose(5, orte_ras_base_framework.framework_output,
                    "%s ras:bbque: Sending the node request to BarbequeRTRM.",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

   
    if(0 > write(socket_fd,&command,sizeof(orte_ras_bbque_cmd_t)))
    {
        orte_show_help("help-ras-bbque.txt", "bbque-send-error", true, 
                       strerror(errno));
        return ORTE_ERROR;
    }

    job.jobid=received_job->jobid;
    
    /*
     * Sums up the num_procs parameters of all the elements in apps array of the job. 
     * To be done in order to tell BBQ how many nodes have been requested via -np option in mpirun command.
     */
    job.slots_requested = 0;
    for (i=0; i < received_job->apps->size; i++) {
        app = (orte_app_context_t*) opal_pointer_array_get_item(received_job->apps, i);
        if (NULL == app) {
            continue;
        }
        job.slots_requested += app->num_procs;
    }

    if(0 > write(socket_fd,&job,sizeof(orte_ras_bbque_job_t)))
    {
        orte_show_help("help-ras-bbque.txt", "bbque-send-error", true, 
                       strerror(errno));
        return ORTE_ERROR;
    }

    opal_output_verbose(2, orte_ras_base_framework.framework_output,
        "%s ras:bbque: Requested %u slots for job %u.",
        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),job.slots_requested,job.jobid);
    
    return ORTE_SUCCESS;
}

/**
 * Send the terminate command to Barbeque to alert it that the application is
 * shutting down.
 */
static int orte_ras_bbque_send_terminate(void)
{
    orte_ras_bbque_cmd_t command;
    
    command.cmd_type=ORTE_RAS_BBQUE_TERMINATE;
    command.jobid=received_job->jobid;
    
    opal_output_verbose(0, orte_ras_base_framework.framework_output,
                "%s ras:bbque: Sending the terminate command to Barbeque.",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    if(0 > write(socket_fd,&command,sizeof(orte_ras_bbque_cmd_t)))
    {
        orte_show_help("help-ras-bbque.txt", "bbque-send-error", true, 
                       strerror(errno));
        return ORTE_ERROR;
    }
    
    return ORTE_SUCCESS;
}
