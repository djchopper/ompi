/*
 * Copyright (c) 2016      Politecnico di Milano. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORTE_RAS_BBQUE_PROTOCOL_H
#define ORTE_RAS_BBQUE_PROTOCOL_H

/*
 * BarbequeRTRM protocol constants
 */

#define ORTE_RAS_BBQUE_NONE -1

/* Commands */
#define ORTE_RAS_BBQUE_NODES_REQUEST 0
#define ORTE_RAS_BBQUE_NODES_REPLY 1
#define ORTE_RAS_BBQUE_TERMINATE 2

#include <stdint.h>

/**
 * The command message that will be send
 * before each other message (it can be
 * considered as the header of the messages)
 * @note Direction: both
 */
struct orte_ras_bbque_cmd_s {
    uint32_t jobid;     /* The jobid  */
    uint8_t cmd_type;   /* Command number */
    uint8_t flags;      /* Flags */
};
typedef struct orte_ras_bbque_cmd_s orte_ras_bbque_cmd_t;

/**
 * The request message for the job. 
 * @note Direction: OMPI -> BBQUE
 */
struct orte_ras_bbque_job_s {
    uint32_t jobid;
    uint32_t slots_requested;
};
typedef struct orte_ras_bbque_job_s orte_ras_bbque_job_t;


/**
 * A resource item identifying a resource assigned to Open MPI.
 * These messages are sent in sequence until more_items != 0. 
 * @note Direction: BBQUE -> OMPI
 */

struct orte_ras_bbque_res_item_t {
	uint32_t jobid;
	char hostname[256];					/* orte_node_t / name */
	int32_t slots_available;			/* orte_node_t / slots */
	uint8_t more_items;					/* boolean flag */
};
typedef struct orte_ras_bbque_res_item_t orte_ras_bbque_res_item_t;


#endif
