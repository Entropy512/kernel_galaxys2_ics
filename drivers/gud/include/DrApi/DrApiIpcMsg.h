/** @addtogroup MC_RTM
 * @{
 * IPC messages.
 *
 * <!-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 */

#ifndef RTMIPCMSG_H_
#define RTMIPCMSG_H_


//------------------------------------------------------------------------------
/** Possible message types/event types of the system. */
typedef enum {
    MSG_NULL = 0,  // Used for initializing state machines
    /***************/
    MSG_RQ                  = 1,  /**< Request; client -> server;  */
    MSG_RS                  = 2,  /**< Response; server -> client */
    MSG_RD                  = 3,  /**< Ready; server -> IPCH */
    MSG_NOT                 = 4,  /**< Notification; client -> IPCH; */
    MSG_CLOSE_TRUSTLET      = 5,  /**< Close Trustlet; MSH -> IPCH; IPCH -> all servers */
    MSG_CLOSE_TRUSTLET_ACK  = 6,  /**< Close Trustlet Ack; servers -> IPCH */
    MSG_MAP                 = 7,  /**< Map; driver <-> IPCH; */
    MSG_ERR_NOT             = 8,  /**< Error Notification;  EXCH/SIQH -> IPCH; */
    MSG_CLOSE_DRIVER        = 9,  /**< Close driver; MSH -> IPCH; IPCH -> driver */
    MSG_CLOSE_DRIVER_ACK    = 10, /**< Close driver Ack; driver -> IPCH; IPCH -> MSH */
    MSG_GET_DRIVER_VERSION  = 11, /**< GetDriverVersion; client <--> IPCH */
    MSG_GET_DRAPI_VERSION   = 12, /**< GetDrApiVersion; driver <--> IPCH */
} message_t;


#endif /** RTMIPCMSG_H_ */

/** @} */
