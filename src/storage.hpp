/**
 * @file storage.hpp
 * @brief Contains data storage (Redis) related classes
 */

#pragma once

#include <string>

#include <redox.hpp>

/**
 * @brief Wrapper class around Redox, a Redis client library.
 * @details This class provides blocking methods that communicate
 * with a Redis server. Furthermore, this class defines an instance
 * key prefix and adds this prefix to every user-given key, in order
 * to make the concurrent executions of Batsim easier.
 */
class RedisStorage
{
public:
    /**
     * @brief Builds a RedisStorage
     */
    RedisStorage();
    /**
     * @brief Destroys a RedisStorage
     */
    ~RedisStorage();

    /**
     * @brief Sets the instance key prefix
     * @param[in] key_prefix The new key prefix
     */
    void set_instance_key_prefix(const std::string & key_prefix);

    /**
     * @brief Connects to a Redis server
     * @param[in] host The server hostname
     * @param[in] port The server port
     * @param connection_callback The callback function to call on connection
     */
    void connect_to_server(const std::string & host = redox::REDIS_DEFAULT_HOST,
                           int port = redox::REDIS_DEFAULT_PORT,
                           std::function< void(int)> connection_callback = nullptr);
    /**
     * @brief Disconnects from the server
     */
    void disconnect();

    /**
     * @brief Gets the value associated with the given key
     * @param[in] key The key
     * @return The value associated with the given key
     */
    std::string get(const std::string & key);

    /**
     * @brief Sets a key-value in the Redis server
     * @param[in] key The key
     * @param[in] value The value to associate with the key
     * @return true if succeeded, false otherwise.
     */
    bool set(const std::string & key,
             const std::string & value);

    /**
     * @brief Deletes a key-value association from the Redis server
     * @param[in] key The key which should be deleted from the Redis server
     * @return true if succeeded, false otherwise.
     */
    bool del(const std::string & key);

    /**
     * @brief Returns the instance key prefix.
     * @return The instance key prefix.
     */
    std::string instance_key_prefix() const;

    /**
     * @brief Returns the key subparts separator.
     * @return The key subparts separator.
     */
    std::string key_subparts_separator() const;

private:
    /**
     * @brief Build a final key from a user-given key.
     * @param[in] user_given_key The user-given key
     * @return The real key corresponding to the user-given key
     */
    std::string build_key(const std::string & user_given_key) const;

private:
    bool _is_connected = false; //!< True if and only if the instance is connected to a Redis server
    redox::Redox _redox; //!< The Redox instance
    std::string _instance_key_prefix = ""; //!< The instance key prefix, which is added before to every user-given key.
    std::string _key_subparts_separator = ":"; //!< The key subparts separator, which is put between the instance key prefix and the user-given key.
};
