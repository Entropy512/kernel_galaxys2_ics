/** @addtogroup DRAPI_COM
 * @{
 *
 * API definitions for use in Mobicore Device Drivers.
 *
 * @file
 * Device Driver Kit Thread functions.
 *
 * <!-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 */

#ifndef __DRAPI_THREAD_H__
#define __DRAPI_THREAD_H__

#include "DrApiCommon.h"
#include "DrApiIpcMsg.h"

//------------------------------------------------------------------------------
/** Definitions */

#define NILTASK             0 /**< returns NILTASK. */
#define NILTHREAD           0 /**< returns NILTHREAD. */

// IMPROVEMENT-2012-01-18-heidera (hollyr, galkag) get these from proper configuration
#define MAX_PRIORITY		(15U)					/**< maximum priority of a task or thread. */
#define DRV_TASK_PRIORITY   9   /**< max Scheduling priority for driver threads. */

typedef word_t  time_t, *time_ptr;          /**< time data type. */
#define TASK_THREAD_QUOTA_SHL       24
#define TASK_TIME_QUOTA_MASK	    ((1 << TASK_THREAD_QUOTA_SHL) - 1)  /**< mask to get/set time quota of a task. */
#define TIME_INFINITE			((time_t)TASK_TIME_QUOTA_MASK)		/**< returns infinite. */

// IMPROVEMENT-2012-01-18-heidera (hollyr, galkag) Remove duplicated codes
#define E_OK			0	/**< no error. */
#define E_INVALID		1	/**< invalid argument. */
#define E_BADTASK		2	/**< current task does not own target task. */
#define E_NOTACTIVATED	3	/**< task does not have been activated. */
#define E_NOTOWNER		4	/**< current task does not own specified task. */
#define E_ACTIVATED		5	/**< task does have been activated. */
#define E_LIMIT			6	/**< limit broken. */
#define E_NOABILITY		7	/**< no permission. */
#define E_STARTED		8	/**< task or thread does have been started. */
#define E_BADMAP		9	/**< invalid mapping. architecture specific error. */
#define E_MAPPED		10	/**< mapping does overlap existing mapping. */
#define E_NOTSTARTED	11	/**< thread does not have been started. */
#define E_TIMEOUT		12	/**< timeout period expired. */
#define E_ABORT			13	/**< operation aborted. */
#define E_MSGTYPE		14	/**< message to send is not of type the receiver is waiting for. */
#define E_MSGLENGTH		15	/**< message to send exceeds message length the receiver is waiting for. */

// Receive error flag to distinguish send from receive errors for IPCs
#define RECV_ERR_FLAG               (1U << 24)
#define RECV_ERR(err)               ((err_t)((err) | RECV_ERR_FLAG))

#define E_IPC_SEND(_code_)          (_code_)
#define E_IPC_RECV(_code_)          (_code_ | RECV_ERR_FLAG)

#define IS_E_IPC(_ret_, _code_)     ((_ret_ & (~RECV_ERR_FLAG)) == _code_)


//------------------------------------------------------------------------------
/**
 * Returns task id for current task.
 *
 * @return taskid
 */
taskid_t  drApiGetTaskid( void );

//------------------------------------------------------------------------------
/**
 * Returns thread id with task id and thread no. set to specified values.
 *
 * @param taskid taskid having the thread.
 * @param threadno Thread number in task.
 *
 * @return MTK return code
 */
threadid_t drApiTaskidGetThreadid(
    taskid_t taskid,
    threadno_t threadno
);

//------------------------------------------------------------------------------
/**
 * Returns thread id from task id and thread no. set to specified values.
 *
 * @param threadno Thread number in current task.
 *
 * @return thread id
 *
 */
threadid_t drApiGetLocalThreadid(
    threadno_t threadno
);

//------------------------------------------------------------------------------
/** Suspend current thread.
 *
 * @param timeout Time to suspend thread.
 *
 * @return MTK return code
 */
drApiResult_t drApiThreadSleep(
	time_t timeout
);

//------------------------------------------------------------------------------
/** Start a thread in driver.
 * @param threadId
 * @param threadEntry
 * @param stackPointer
 * @param priority
 * @param localExceptionHandler  Use exception handler from this thread, use NILTHREAD for exception handler of task
 *
 * @return MTK return code
 */
drApiResult_t drApiStartThread(
    const threadno_t    threadNo,
    const addr_t        threadEntry,
    const stackTop_ptr  stackPointer,
    const uint32_t      priority,
    const threadno_t    localExceptionHandlerThreadNo
);


//------------------------------------------------------------------------------
/** Stop a thread in driver.
 * Stops thread in driver.  if thread no. of thread to stop is set to NILTHREAD,
 * the current thread is stopped. The to stopped thread is detached from any
 * previously attached interrupts. If any thread is waiting for stopped thread
 * to do any ipc, this ipc is aborted.
 *
 * @param threadId
 *
 * @return MTK return code
 */

drApiResult_t drApiStopThread(
	const threadno_t    threadNo
);


//------------------------------------------------------------------------------
/** Attach interrupt to thread.
 *
 * @param intr interrupt number.
 *
 * @return MTK return code
 */
drApiResult_t drApiIntrAttach(
	intrNo_t intr
);

//------------------------------------------------------------------------------
/** Detach interrupt from thread.
 *
 * @param intr interrupt number.
 *
 * @return MTK return code
 */
drApiResult_t drApiIntrDetach(
	intrNo_t intr
);

//------------------------------------------------------------------------------
/** Wait with infinite timeout for interrupt message from kernel.
 *
 * @param intr interrupt number. If ANYINTR is used, the interrupt is returned in the parameter intrRet
 * @param timeout timeout to wait. Allowed values like for MTK signal_wait().
 * @param pIntrRet receives interrupt. Parameter can be NULL if caller does not need this.

 * @return MTK return code
 */
drApiResult_t drApiWaitForIntr(
    const intrNo_t  intr,
    const uint32_t  timeout,
    intrNo_t        *pIntrRet
);

//------------------------------------------------------------------------------
/**
 * Trigger software interrupt in the NWd to notify it.
 *
 * @param intrNo interrupt number.
 *
 * @return MTK return code
 */
drApiResult_t drApiTriggerIntr(
		word_t intrNo
);

//------------------------------------------------------------------------------
/**
 * Send ready message or answer to IPCH and wait for a client request
 *
 * @param ipcPeer Destination to send message to.
 * @param ipcMsg IPC message.
 * @param ipcData Additional IPC data.
 *
 * @retval E_OK         no error.
 */
drApiResult_t drApiIpcCallToIPCH(
    threadid_t       *pIpcPeer,
    message_t        *pIpcMsg,
    uint32_t         *pIpcData
);

//------------------------------------------------------------------------------
/** Set signal.
 * A signal (SIGNAL) will be used by a thread to inform another thread about an event.
 * A signal operation is asynchronous which means that the operation will return
 * immediately without blocking the user. Function uses auto-clear signals, meaning that
 * the signal is cleared automatically when the receiver receives it.
 *
 * It is up to the destination of the signal to pick up and process the information.
 *
 * @param receiver Thread to set the signal for.
 *
 * @return MTK return code
 */
drApiResult_t drApiIpcSignal(
    const threadid_t  receiver
);

//------------------------------------------------------------------------------
/** Signal wait operation.
 * A thread uses the sigWait operation to check if a signal has occurred. If no signal is
 * pending the thread will block until a signal arrives.
 *
 * @return MTK return code
 */
drApiResult_t drApiIpcSigWait( void );

#endif // __DRAPI_THREAD_H__
