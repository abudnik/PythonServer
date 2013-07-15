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

#include <iostream>
#include <syslog.h>
#include "log.h"


namespace python_server {

namespace logger
{

bool isDaemon = false;
const char *serviceName = "";


void InitLogger( bool isDaemon, const char *serviceName )
{
	logger::isDaemon = isDaemon;
	logger::serviceName = serviceName;

	if ( isDaemon )
	{
		openlog( serviceName, LOG_CONS, LOG_DAEMON );
	}
}

void ShutdownLogger()
{
	if ( isDaemon )
	{
		closelog();
	}
}

void Log( const char *msg )
{
	if ( isDaemon )
	{
		syslog( LOG_INFO, "%s", msg );
	}
	else
	{
		std::cout << serviceName << ": " <<  msg << std::endl;
	}
}

} // namespace logger

} // namespace python_server
