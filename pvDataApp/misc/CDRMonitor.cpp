/* CDRMonitor.cpp */
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvDataCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdexcept>

#include <epicsThread.h>

#include "noDefaultMethods.h"
#include "lock.h"
#include "pvType.h"
#include "linkedList.h"
#include "CDRMonitor.h"

namespace epics { namespace pvData {

static
epicsThreadOnceId monitorInit = EPICS_THREAD_ONCE_INIT;

// Must use a pointer w/ lazy init due to lack of
// initialization order guarantees
CDRMonitor* CDRMonitor::theone = 0;

CDRMonitor&
CDRMonitor::get()
{
    epicsThreadOnce(&monitorInit, &CDRMonitor::init, 0);
    assert(theone);
    return *theone;
}

void
CDRMonitor::init(void *)
{
    //BUG: No idea how to handle allocation failure at this stage.
    theone=new CDRMonitor;
}

CDRMonitor::CDRMonitor()
    :firstNode(0)
{}

CDRCount
CDRMonitor::current()
{
    CDRCount total;
    for(CDRNode *cur=first(); !!cur; cur=cur->next())
    {
        total+=cur->get();
    }
    return total;
}

void
CDRMonitor::show(FILE *fd)
{
    for(CDRNode *cur=first(); !!cur; cur=cur->next())
    {
        cur->show(fd);
    }
}

void
CDRNode::show(FILE *fd)
{
    Lock x(&guard);
    if(!current.cons && !current.dtys && !current.refs)
        return;
    fprintf(fd,"%s:  totalConstruct %lu totalDestruct %lu",
            nodeName.c_str(), (unsigned long)current.cons,
            (unsigned long)current.dtys);
    ssize_t alive=current.cons;
    alive-=current.dtys;
    if(current.refs)
        fprintf(fd," totalReference %ld", current.refs);
    if(alive)
        fprintf(fd," ACTIVE %ld\n", (long)alive);
    else
        fprintf(fd,"\n");
}

void
onceNode(void* raw)
{
    CDRNodeInstance* inst=static_cast<CDRNodeInstance*>(raw);
    inst->node=new CDRNode(inst->name);
}

}}