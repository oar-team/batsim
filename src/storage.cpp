#include "storage.hpp"

#include <simgrid.h>

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
    return _redox.get(build_key(key));
}

bool RedisStorage::set(const std::string &key, const std::string &value)
{
    xbt_assert(_is_connected, "Bad RedisStorage::get call: Not connected");
    return _redox.set(build_key(key), value);
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

std::string RedisStorage::build_key(const std::string & user_given_key) const
{
    return _instance_key_prefix + _key_subparts_separator + user_given_key;
}
