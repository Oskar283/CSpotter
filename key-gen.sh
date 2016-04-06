#!/bin/sh

# Check if key exists
if ! [ -f ~/.ssh/id_rsas.pub ] ; then

    # Get email
    echo "Type your email address: "

    read email

    while ! echo "${email}" | grep '^[a-zA-Z0-9]*@[a-zA-Z0-9]*\.[a-zA-Z0-9]*$' > /dev/null  ; do
        echo "Bad email address, try gain: "
        read email
    done

    # Get preferred key location
    echo "Enter file in which to save the key (${HOME}/.ssh/id_rsa): "

    read keyfile
    keyfile="$(readlink -f ${keyfile})"

    while true ; do
        case "${keyfile}" in
            "$HOME"*)
                break ;;
            "")
                keyfile="${HOME}/.ssh/id_rsa"
                break ;;
            *)
                echo "Bad file name, try gain: "
                read keyfile ;;
        esac
    done
    

    # Generate key
    ssh-keygen -t rsa -b 4096 -C "${email}" -f "${keyfile}"
else
    keyfile="${HOME}/.ssh/id_rsa"
fi


if [ ! $(pidof ssh-agent) ] ; then
    # Start ssh-agent in the background
    echo "Starting ssh agent."
    eval "$(ssh-agent -s)"
fi

ssh-add "${keyfile}"

echo "Copy the ssh key in ${keyfile}.pub and distribute it."
