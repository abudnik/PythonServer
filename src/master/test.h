#ifndef __TEST_H
#define __TEST_H

#include <fstream>
#include "job_manager.h"


namespace master {

void TestSingleJob( const std::string &filePath )
{
    // read job description from file
    std::ifstream file( filePath.c_str() );
    if ( !file.is_open() )
    {
        PLOG_ERR( "TestSingleJob: couldn't open " << filePath );
        return;
    }
    std::string jobDescr, line;
    while( getline( file, line ) )
        jobDescr += line;

    IJobManager *jobManager = common::GetService< IJobManager >();
    JobPtr job( jobManager->CreateJob( jobDescr, true ) );
    if ( job )
    {
        // add job to job queue
        jobManager->PushJob( job );
    }
}

void TestMetaJob( const std::string &filePath )
{
    // read meta job description from file
    std::ifstream file( filePath.c_str() );
    if ( !file.is_open() )
    {
        PLOG_ERR( "TestMetaJob: couldn't open " << filePath );
        return;
    }
    std::string metaDescr, line;
    while( getline( file, line ) )
        metaDescr += line + '\n';

    std::list< JobPtr > jobs;
    IJobManager *jobManager = common::GetService< IJobManager >();
    jobManager->CreateMetaJob( metaDescr, jobs, true );
    jobManager->PushJobs( jobs );
}

void RunTests( const std::string &jobsDir )
{
    std::string filePath = jobsDir + "/test.all";
    std::ifstream file( filePath.c_str() );
    if ( !file.is_open() )
    {
        PLOG_ERR( "RunTests: couldn't open " << filePath );
        return;
    }
    int i = 0;
    std::string line;
    while( getline( file, line ) )
    {
        size_t found = line.rfind( '.' );
        if ( found == std::string::npos )
        {
            PLOG_ERR( "RunTests: couldn't extract job file extension, line=" << i++ );
            continue;
        }
        std::string ext = line.substr( found + 1 );

        filePath = jobsDir + '/' + line;

        if ( ext == "job" )
            TestSingleJob( filePath );
        else
        if ( ext == "meta" )
            TestMetaJob( filePath );
        else
            PLOG_ERR( "RunTests: unknown file extension, line=" << i );

        ++i;
    }
}

} // namespace master

#endif
