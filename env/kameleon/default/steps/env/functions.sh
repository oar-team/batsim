## functions

function fail {
    echo $@ 1>&2
    false
}

export -f fail

function __download {
    src=$1
    dst=$2
    # If dst is unset or a directory, infers dst pathname from src
    if [ -z "$dst" -o "${dst: -1}" == "/" ]; then
        dst="$dst${src##*/}"
        dst="${dst%%\?*}"
    fi
    echo -n "Downloading: $src..."
    # Put cURL first because it accept URIs (like file://...)
    if which curl >/dev/null; then
        echo " (cURL)"
        curl -S --fail -# -L --retry 999 --retry-max-time 0 "$src" -o "$dst" 2>&1
    elif which wget >/dev/null; then
        echo " (wget)"
        wget --retry-connrefused --progress=bar:force "$src" -O "$dst" 2>&1
    elif which python >/dev/null; then
        echo " (python)"
        python -c <<EOF
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

urllib.urlretrieve('$src', '$dst', reporthook=reporthook)
print('\n')
EOF
        true
    else
        fail "No way to download $src"
    fi
}

export -f __download

function __download_recipe_build() {
    set -e
    recipe=$1
    version=${2:-latest}
    do_checksum=${3:-true}
    do_checksign=${4:-false}
    do_cache=${5:-false}
    builds_url=${6:-http://kameleon.imag.fr/builds}
    dest_dir="${7:-$recipe}"
    mkdir -p $dest_dir
    pushd $dest_dir > /dev/null
    echo "Downloading $recipe ($version):"
    __download $builds_url/${recipe}_$version.manifest
    if [ "$do_checksign" == "true" ]; then
        __download $builds_url/${recipe}_$version.manifest.sign
        gpg --verify ${recipe}_$version.manifest{.sign,} || fail "Cannot verify signature"
    fi
    for f in $(< ${recipe}_$version.manifest); do
        if [[ $f =~ ^$recipe-cache_ ]] && [ "$do_cache" != "true" ]; then
            continue
        fi
        if [[ $f =~ \.sha[[:digit:]]+sum$ ]]; then
            if [ "$do_checksum" == "true" ]; then
                __download $builds_url/$f
                ${f##*.} -c $f || fail "Cannot verify checksum"
                if [ "$do_checksign" == "true" ]; then
                    __download $builds_url/$f.sign
                    gpg --verify $f{.sign,} || fail "Cannot verify signature"
                fi
            fi
        else
            __download $builds_url/$f
            echo -n "Link to version-less filename: "
            ln -fv $f ${f%_*}.${f#*.}
        fi
    done
    set +e
}

export -f __download_recipe_build

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
