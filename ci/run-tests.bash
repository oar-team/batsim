#!/usr/bin/env nix-shell
#! nix-shell -i bash ./default.nix
set -eux

# Run a redis server if needed
redis_launched_here=0
r=$(ps faux | grep redis-server | grep -v grep | wc -l)
if [ $r -eq 0 ]
then
    echo "Running a Redis server..."
    redis-server>/dev/null &
    REDIS_PID=$!
    redis_launched_here=1

    while ! nc -z localhost 6379; do
      sleep 1
    done
fi

BUILD_DIR=$(realpath $(dirname $(realpath $0))/../build)

# Add built batsim in PATH
export PATH=${BUILD_DIR}:${PATH}

# Print which files are executed
echo "batsim realpath: $(realpath $(which batsim))"
echo "batsched realpath: $(realpath $(which batsched))"

# Execute the tests. Command can be overriden in the CTEST_COMMAND env var.
cd $BUILD_DIR
${CTEST_COMMAND:-ctest --output-on-failure}
failed=$?

# Stop the redis server if it has been launched by this script
if [ $redis_launched_here -eq 1 ]
then
    echo "Stopping the Redis server..."
    kill $REDIS_PID
fi

exit ${failed}
