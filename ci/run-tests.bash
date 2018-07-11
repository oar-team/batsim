#!/usr/bin/env nix-shell
#! nix-shell -i bash ./default.nix

# Run a redis server if needed
redis_launched_here=0
r=$(ps faux | grep redis-server | grep -v grep | wc -l)
if [ $r -eq 0 ]
then
    echo "Running a Redis server..."
    redis-server>/dev/null &
    redis_launched_here=1

    while ! nc -z localhost 6379; do
      sleep 1
    done
fi

# Add built batsim in PATH
export PATH=$(realpath ./build):${PATH}

# Execute the tests
cd build
ctest
failed=$?

# Stop the redis server if it has been launched by this script
if [ $redis_launched_here -eq 1 ]
then
    echo "Stopping the Redis server..."
    killall redis-server
fi

exit ${failed}
