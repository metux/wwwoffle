#!/bin/bash

if ! ln -sf /usr/share/automake-1.11/config.sub ; then
    echo "autoconf 1.11 doesnt seem to be installed correctly"
    exit 1
fi

if ! ln -sf /usr/share/automake-1.11/config.guess ; then
    echo "autoconf 1.11 doesnt seem to be installed correctly"
    exit 1
fi

autoreconf -fi
