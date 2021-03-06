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

#include <boost/bind.hpp>
#include <stdint.h> // boost/atomic/atomic.hpp:202:16: error: ‘uintptr_t’ was not declared in this scope
#include "ping.h"
#include "common/log.h"
#include "common/service_locator.h"
#include "worker_manager.h"

namespace master {

void Pinger::Stop()
{
    stopped_ = true;
    timer_.StopWaiting();
}

void Pinger::PingWorkers()
{
    std::vector< WorkerPtr > workers;
    auto workerManager = common::GetService< IWorkerManager >();
    workerManager->GetWorkers( workers );
    for( auto &worker : workers )
    {
        PingWorker( worker );
    }
    ++numPings_;
}

void Pinger::Run()
{
    while( !stopped_ )
    {
        timer_.Wait( pingDelay_ * 1000 );
        PingWorkers();
        CheckDropedPingResponses();
    }
}

void Pinger::CheckDropedPingResponses()
{
    if ( numPings_ < maxDroped_ + 1 )
        return;

    auto workerManager = common::GetService< IWorkerManager >();
    workerManager->CheckDropedPingResponses();
    numPings_ = 0;
}

void Pinger::OnWorkerIPResolve( WorkerPtr &worker, const std::string &ip )
{
    auto workerManager = common::GetService< IWorkerManager >();
    workerManager->SetWorkerIP( worker, ip );
}

void PingerBoost::StartPing()
{
    io_service_.post( boost::bind( &Pinger::Run, this ) );
}

void PingerBoost::PingWorker( WorkerPtr &worker )
{
    auto it = endpoints_.find( worker->GetHost() );
    if ( it == endpoints_.end() )
    {
        common::Config &cfg = common::Config::Instance();
        bool ipv6_only = cfg.Get<bool>( "ipv6_only" );
        udp::resolver::query query( ipv6_only ? udp::v6() : udp::v4(), worker->GetHost(), port_ );

        boost::system::error_code error;
        udp::resolver::iterator iterator = resolver_.resolve( query, error ), end;
        if ( error || iterator == end )
        {
            PLOG_DBG( "PingerBoost::PingWorker address not resolved: " << worker->GetHost() );
            return;
        }

        auto p = endpoints_.emplace( worker->GetHost(), *iterator );
        it = p.first;
    }

    if ( worker->GetIP().empty() )
    {
        OnWorkerIPResolve( worker, it->second.address().to_string() );
    }

    const std::string &node_ip = it->second.address().to_string();

    common::Marshaller marshaller;
    marshaller( "host", node_ip );

    std::string msg;
    protocol_->Serialize( msg, "ping", marshaller );
    //PLOG_DBG( msg );
    //PLOG_DBG( node_ip );

    try
    {
        socket_.send_to( boost::asio::buffer( msg ), it->second );
    }
    catch( boost::system::system_error &e )
    {
        PLOG_ERR( "PingerBoost::PingWorker: send_to failed: " << e.what() << ", host : " << node_ip );
    }
}

} // namespace master
