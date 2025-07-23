#Check Input
if [ "$#" != 4 ]; then
    echo "Need: $0 -p <path> -c <command>"
    exit 1
fi

re='^[a-zA-Z]+$'
if [ "$1" != "-p" ]; then
    echo "error: wrong flag"
    exit 1
fi
if ! [[ "$2" =~ $re ]]; then
    echo "error: argument not a string"
    exit 1
fi
if [ "$3" != "-c" ]; then
    echo "error: wrong flag"
    exit 1
fi

if ! [[ "$4" =~ $re ]]; then
    echo "error:argument not a string"
    exit 1
fi

if ! [ -e "$2" ]; then
    echo "Path does not exist"
    exit 1
fi

if [ -d "$2" ]; then #its a directory
    if [ "$4" != "purge" ]; then
        echo "Only command avalailable for directories is purge"
        exit 1
    fi
fi

if [[ "$4" == "purge" ]]; then
    if [ -d "$2" ]; then #its a directory
        rm -rf "$2"
        echo "Deleting "$2"..."
    elif [ -f "$2" ]; then
        rm "$2"
        echo "Deleteing "$2"..."
    fi
    echo "Purge complete"
fi

