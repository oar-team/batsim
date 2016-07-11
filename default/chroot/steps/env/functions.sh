## functions

function fail {
    echo $@ 1>&2
    false
}

export -f fail

function __download {
    echo "Downloading: $1..."
    if which wget >/dev/null; then
        wget --retry-connrefused --progress=bar:force "$1" -O "$2" 2>&1
    else
        echo "wget is missing, trying with curl..."
        if which curl >/dev/null; then
            curl --fail -# -L --retry 999 --retry-max-time 0 "$1" -o "$2" 2>&1
        else
            echo "curl is missing, trying with python..."
            if which python >/dev/null; then
                python -c "
import sys
import time
if sys.version_info >= (3,):
    import urllib.request as urllib
else:
    import urllib


def reporthook(count, block_size, total_size):
    global start_time
    if count == 0:
        start_time = time.time()
        return
    duration = time.time() - start_time
    progress_size = float(count * block_size)
    if duration != 0:
        if total_size == -1:
            total_size = block_size
            percent = 'Unknown size, '
        else:
            percent = '%.0f%%, ' % float(count * block_size * 100 / total_size)
        speed = int(progress_size / (1024 * duration))
        sys.stdout.write('\r%s%.2f MB, %d KB/s, %d seconds passed'
                         % (percent, progress_size / (1024 * 1024), speed, duration))
        sys.stdout.flush()

urllib.urlretrieve('$1', '$2', reporthook=reporthook)
print('\n')
"
            true
            else
                fail "Cannot download $1"
            fi
        fi
    fi
}

export -f __download

function __find_linux_boot_device() {
    local PDEVICE=`stat -c %04D /boot`
    for file in $(find /dev -type b 2>/dev/null) ; do
        local CURRENT_DEVICE=$(stat -c "%02t%02T" $file)
        if [ $CURRENT_DEVICE = $PDEVICE ]; then
            ROOTDEVICE="$file"
            break;
        fi
    done
    echo "$ROOTDEVICE"
}

export -f __find_linux_boot_device


function __find_free_port() {
  local begin_port=$1
  local end_port=$2

  local port=$begin_port
  local ret=$(nc -z 127.0.0.1 $port && echo in use || echo free)
  while [ $port -le $end_port ] && [ "$ret" == "in use" ]
  do
    local port=$[$port+1]
    local ret=$(nc -z 127.0.0.1 $port && echo in use || echo free)
  done

  # manage loop exits
  if [[ $port -gt $end_port ]]
  then
    fail "No free port available between $begin_port and $end_port"
  fi

  echo $port
}

export -f __find_free_port
