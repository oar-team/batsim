/**
 * @file storage.cpp
 * @brief Contains data storage (Redis) related methods implementation
 */

#include "storage.hpp"

#include <boost/locale.hpp>

#include <xbt.h>

using namespace std;

XBT_LOG_NEW_DEFAULT_CATEGORY(redis, "redis"); //!< Logging

RedisStorage::RedisStorage()
{
    //TODO: wrap redox logging into simgrid?
}

RedisStorage::~RedisStorage()
{
    if (_is_connected)
    {
        disconnect();
    }
}

void RedisStorage::set_instance_key_prefix(const std::string & key_prefix)
{
    _instance_key_prefix = key_prefix;
}

void RedisStorage::connect_to_server(const std::string & host,
                                     int port,
                                     std::function<void (int)> connection_callback)
{
    xbt_assert(!_is_connected, "Bad RedisStorage::connect_to_server call: "
               "Already connected");

    _is_connected = _redox.connect(host, port, connection_callback);
    xbt_assert(_is_connected, "Error: could not connect to Redis server "
               "(host='%s', port=%d)", host.c_str(), port);

}

void RedisStorage::disconnect()
{
    xbt_assert(_is_connected, "Bad RedisStorage::connect_to_server call: "
               "Not connected");

    _redox.disconnect();
}

std::string RedisStorage::get(const std::string & key)
{
    xbt_assert(_is_connected, "Bad RedisStorage::get call: Not connected");

    string real_key = boost::locale::conv::to_utf<char>(build_key(key), "UTF-8");

    try
    {
        return _redox.get(real_key);
    }
    catch (const std::exception & e)
    {
        XBT_ERROR("Couldn't get the value associated to key '%s' in Redis! "
                  "Message: %s", real_key.c_str(), e.what());
        return "";
    }
}

bool RedisStorage::set(const std::string &key, const std::string &value)
{
    string real_key = boost::locale::conv::to_utf<char>(build_key(key), "UTF-8");
    string real_value = boost::locale::conv::to_utf<char>(value, "UTF-8");

    xbt_assert(_is_connected, "Bad RedisStorage::get call: Not connected");
    bool ret = _redox.set(real_key, real_value);
    if (ret)
    {
        XBT_INFO("Set: '%s'='%s'", real_key.c_str(), real_value.c_str());
        xbt_assert(get(key) == value, "Batsim <-> Redis communications are inconsistent!");
    }
    else
    {
        XBT_WARN("Couldn't set: '%s'='%s'", real_key.c_str(), real_value.c_str());
    }

    return ret;
}

bool RedisStorage::del(const std::string &key)
{
    xbt_assert(_is_connected, "Bad RedisStorage::get call: Not connected");
    return _redox.del(build_key(key));
}

std::string RedisStorage::instance_key_prefix() const
{
    return _instance_key_prefix;
}

std::string RedisStorage::key_subparts_separator() const
{
    return _key_subparts_separator;
}

std::string RedisStorage::job_key(const JobIdentifier &job_id)
{
    std::string key = "job_" + job_id.workload_name + '!' + to_string(job_id.job_name);
    return key;
}

std::string RedisStorage::profile_key(const std::string &workload_name,
                                        const std::string &profile_name)
{
    std::string key = "profile_" + workload_name + '!' + profile_name;
    return key;
}



std::string RedisStorage::build_key(const std::string & user_given_key) const
{
    return _instance_key_prefix + _key_subparts_separator + user_given_key;
}
