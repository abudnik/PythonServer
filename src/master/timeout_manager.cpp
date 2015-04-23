/*
===========================================================================

This software is licensed under the Apache 2 license, quoted below.

Copyright (C) 2013 Andrey Budnik <budnik27@gmail.com>

Licensed under the Apache License, Version 2.0 (the "License"); you may not
use this file except in compliance with the License. You may obtain a copy of
the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
License for the specific language governing permissions and limitations under
the License.

===========================================================================
*/

#include "timeout_manager.h"
#include "scheduler.h"
#include "job_manager.h"
#include "worker_manager.h"
#include "common/service_locator.h"

namespace master {

void TimeoutManager::TaskTimeoutHandler::HandleTimeout()
{
    IScheduler *scheduler = common::GetService< IScheduler >();
    scheduler->OnTaskTimeout( workerTask_, hostIP_ );
}

void TimeoutManager::JobTimeoutHandler::HandleTimeout()
{
    IScheduler *scheduler = common::GetService< IScheduler >();
    scheduler->OnJobTimeout( jobId_ );
}

void TimeoutManager::JobQueueTimeoutHandler::HandleTimeout()
{
    IJobManager *jobManager = common::GetService< IJobManager >();
    jobManager->DeleteJob( jobId_ );
}

void TimeoutManager::StopTaskTimeoutHandler::HandleTimeout()
{
    IWorkerManager *workerManager = common::GetService< IWorkerManager >();
    workerManager->AddCommand( command_, hostIP_ );
}


void TimeoutManager::Start()
{
    io_service_.post( boost::bind( &TimeoutManager::Run, this ) );
}

void TimeoutManager::Stop()
{
    stopped_ = true;
    timer_.StopWaiting();
}

void TimeoutManager::Run()
{
    while( !stopped_ )
    {
        timer_.Wait( 1000 );
        CheckTimeouts();
    }
}

void TimeoutManager::CheckTimeouts()
{
    namespace pt = boost::posix_time;
    std::unique_lock< std::mutex > lock( jobsMut_ );
    auto it = jobs_.begin();
    const pt::ptime now = pt::second_clock::local_time();
    for( ; it != jobs_.end(); )
    {
        const pt::ptime &jobSendTime = it->first;
        if ( now < jobSendTime ) // skip earlier sended jobs
            break;

        Callback callback( it->second );
        callback();
        jobs_.erase( it++ );
    }
}

void TimeoutManager::PushJobQueue( int64_t jobId, int queueTimeout )
{
    if ( queueTimeout < 0 )
        return;

    namespace pt = boost::posix_time;
    const pt::ptime now = pt::second_clock::local_time();
    const pt::ptime deadlineQueue = now + pt::seconds( queueTimeout );

    auto handlerQueue = std::make_shared< JobQueueTimeoutHandler >();
    handlerQueue->jobId_ = jobId;
    Callback callbackQueue(
        std::bind( &JobQueueTimeoutHandler::HandleTimeout, handlerQueue )
    );

    std::unique_lock< std::mutex > lock( jobsMut_ );
    jobs_.insert( std::pair< pt::ptime, Callback >(
                      deadlineQueue,
                      callbackQueue
                )
    );
}

void TimeoutManager::PushJob( int64_t jobId, int jobTimeout )
{
    if ( jobTimeout < 0 )
        return;

    namespace pt = boost::posix_time;
    const pt::ptime now = pt::second_clock::local_time();
    const pt::ptime deadline = now + pt::seconds( jobTimeout );

    auto handler = std::make_shared< JobTimeoutHandler >();
    handler->jobId_ = jobId;
    Callback callback(
        std::bind( &JobTimeoutHandler::HandleTimeout, handler )
    );

    std::unique_lock< std::mutex >lock( jobsMut_ );
    jobs_.insert( std::pair< pt::ptime, Callback >(
                      deadline,
                      callback
                )
    );
}

void TimeoutManager::PushTask( const WorkerTask &task, const std::string &hostIP, int timeout )
{
    if ( timeout < 0 )
        return;

    namespace pt = boost::posix_time;
    const pt::ptime now = pt::second_clock::local_time();
    const pt::ptime deadline = now + pt::seconds( timeout );

    auto handler = std::make_shared< TaskTimeoutHandler >();
    handler->workerTask_ = task;
    handler->hostIP_ = hostIP;
    Callback callback(
        std::bind( &TaskTimeoutHandler::HandleTimeout, handler )
    );

    std::unique_lock< std::mutex > lock( jobsMut_ );
    jobs_.insert( std::pair< pt::ptime, Callback >(
                      deadline,
                      callback
                )
    );
}

void TimeoutManager::PushCommand( CommandPtr &command, const std::string &hostIP, int delay )
{
    if ( delay < 0 )
        return;

    namespace pt = boost::posix_time;
    const pt::ptime now = pt::second_clock::local_time();
    const pt::ptime deadline = now + pt::seconds( delay );

    auto handler = std::make_shared< StopTaskTimeoutHandler >();
    handler->command_ = command;
    handler->hostIP_ = hostIP;
    Callback callback(
        std::bind( &StopTaskTimeoutHandler::HandleTimeout, handler )
    );

    std::unique_lock< std::mutex > lock( jobsMut_ );
    jobs_.insert( std::pair< pt::ptime, Callback >(
                      deadline,
                      callback
                )
    );
}

} // namespace master
