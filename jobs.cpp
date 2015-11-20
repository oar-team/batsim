/**
 * @file jobs.cpp
 */

#include "jobs.hpp"

using namespace std;

Jobs::Jobs()
{

}

Jobs::~Jobs()
{
    for (auto mit : _jobs)
    {
        delete mit.second;
    }
}

void Jobs::load_from_json(const std::string &filename)
{
    // TODO
}

Job *Jobs::operator[](int job_id)
{
    auto it = _jobs.find(job_id);
    return it->second;
}

const Job *Jobs::operator[](int job_id) const
{
    auto it = _jobs.find(job_id);
    return it->second;
}

bool Jobs::exists(int job_id) const
{
    auto it = _jobs.find(job_id);
    return it != _jobs.end();
}

const std::map<int, Job* > &Jobs::jobs() const
{
    return _jobs;
}
