/*
* Copyright (c) 2005,2006,2007 The Regents of the University of California.
* All rights reserved.
* 
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the University of California, nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
* 
* THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF CALIFORNIA ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "XtrMetadata.h"
#include "XtrEvent.h"
#include <pthread.h>
#include <unistd.h>
#include <memory>

#ifndef _XTR_CONTEXT_H
#define _XTR_CONTEXT_H

#define MAXHOSTNAME 256

using namespace std;

/** High-level API for maintaining a global X-Trace context and reporting events
 *  with logging-like calls. 
 *  
 *  Usage:
 *    - When communication is received, set the context using Context::setContext().
 *    - To record an event, call Context.logEvent(). Or, to add extra fields to the
 *      event report, call Context.createEvent(), add fields to the returned Event
 *      object, and send it using Event.sendReport().
 *    - When calling another service, get the current context using Context::getContext() and
 *      set it as metadata. After receiving a reply, add an edge from both the reply's metadata and 
 *      the current context in the report for the reply.
 *    - Clear the context using Context.unsetContext()
 *
 *  @author Rodrigo Fonseca
 *  @author Nick Lanham (multi-thread support)
 *
 */
 
namespace xtr {
	
class Context {
public:

    /** Get the current context metadata */
    static const Metadata& getContext ();

    /** Set the current context metadata
     *  @param xtr the metadata to set the context to.
     */
    static void setContext (Metadata const& xtr);

    /** Clear the current context. After this, a call to 
     *  Context.getContext().isValid() will return false.
     *  This is important to avoid the
     *  context leaking to other unrelated operations */
    static void unsetContext ();
    

    /** Logs an event to the X-Trace framework.
     *
     *  This does several things:
     *    - creates a new Event object with a new OpId
     *    - adds an edge from the current context to the new Event
     *    - adds agent and label to the event as information
     *    - sets the current context to the Event.getMetadata()
     *    - sends a report from the new Event, if the severity level and threshold
     *      are compatible.
     *
     *  If the current context is not valid, it will create a new context with a random
     *  task id and set it. 
     *
     *  @param agent value of the Agent: key for the created report
     *  @param label value of the Label: key for the created report
     *  @param severity desired severity level for this event. The default value is
     *          OptionSeverity::DEFAULT. This event will be logged only if the
     *          event severity <= reporting context's severityThreshold.
     *  @return XTR_SUCCESS if the event was successfully sent to the logging layer.
     *              Semantics: the current X-Trace metadata will be changed to this
     *                 event's eventId *IFF* this call returns XTR_SUCCESS.
     *          XTR_FAIL_SEVERITY if the severity level of the event was not sufficient
     *              to clear the effective severity threshold of the reporter. 
     *              The effective severity threhsold of the reporter is a combination of
     *              the severity threshold of the reporter and the severity threshold of
     *              the current X-Trace metadata.
     *          XTR_FAIL if the report is not sent for some other reason.
     *  @see Event
     */
    static xtr_result logEvent(const char* agent, const char* label,
                          u_int8_t severity = OptionSeverity::DEFAULT);

    /**
     * The second version of the logEvent call, taking an event object pointer rather than
     * message strings. This event will be, most likely, the result of a previous call to
     * prepareEvent().
     * This call will, atomically, send the event to the reporting infrastructure and
     * advance the current X-Trace context to the just-logged event. If the report is not
     * accepted by the X-Trace reporter, the call returns XTR_FAIL and the context *is not*
     * advanced.
     * 
     * @param e A pointer to an event object to be logged. The calling function maintains
     *          ownership of the pointer.
     * @param severity (default OptionSeverity::DEFAULT) The severity of the logged event.
     * @return XTR_SUCCESS if the report is accepted by the reporter. This also means that
     *                     the Xtr::Context is made to point the just-logged event.
     *         XTR_FAIL_SEVERITY if the report is not sent because of insufficient severity.
     *         XTR_FAIL if the report is not sent for some other reason.
     * @see logEvent(const char*, const char*, u_int8_t)
     */
    static xtr_result logEvent(Event* e,
                          u_int8_t severity = OptionSeverity::DEFAULT);

    /** 
     * DEPRECATED: use prepareEvent and logEvent(const Event const* e)
     *The same as logEvent, but does not send the report, and returns the created
     *  Event object. Use this to add more information to the report.
     *  @param agent value of the Agent: key for the created report
     *  @param label value of the Label: key for the created report
     *  @param severity desired severity level for this report. This event will
     *          be logged only if the event severity <= reporting context's
     *          severityThreshold AT THE TIME OF CREATION. If the event will not
     *          be logged, the current context is not altered, and all other
     *          methods such as addEdge and sendReport will be no-ops.
     *  @return an auto_ptr to an allocated Event object. Remember to call
     *          sendReport() on this object to report it! You don't need to worry
     *          about deleting it unless you will be passing it around to other 
     *          functions.
     *  @see logEvent(), Event
     */
    static auto_ptr<Event> 
    createEvent( const char* agent, const char* label,
                 u_int8_t severity = OptionSeverity::DEFAULT); 

    static auto_ptr<Event>
    prepareEvent( const char* agent, const char* label, 
                  u_int8_t severity = OptionSeverity::DEFAULT);
    
    /** Sets the host name that is automatically added to the reports generated
     *  by logEvent() and createEvent(). The setting overrides the default, which
     *  the class tries to obtain from the underlying OS.
     *  @param name a regular C string with the host name. At most MAXHOSTNAME (256)
     *         characters are copied from this string into the class.
     */
    static void setHost(const char* name);
    
    /** Deprecated. Explicitly indicate the start of a new chain of events. This is used for explicitly
     *  capturing concurrency, and is experimental. */
    static void forkContext ();
private:

    static void ensureKey();
    static bool is_host_set;
    static char host_name[MAXHOSTNAME+1];
    static void _set_host();
public: // so it can be inited
    static pthread_key_t context_key;
};

}; //namespace xtr


#endif //_XTR_CONTEXT_H
