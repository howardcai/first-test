#!/bin/bash
#
# Startup flags for chroot-like behavior when running the dma-agent-builder environment. By default, it will
# pull the 'latest' image from the registry. Override by setting VERSION in your environment.

IMAGE="datapath-builder"
VERSION=${VERSION:-latest}
RELEASE_TIER=${RELEASE_TIER:-"dev"}
ARTIFACTORY_PATH='artifactory.f5net.com/velocity-platform-docker-'${RELEASE_TIER}
IMAGE_REMOTE=${ARTIFACTORY_PATH}/${IMAGE}

docker pull ${IMAGE_REMOTE}:${VERSION}

docker run --interactive --tty --rm \
    --name ${IMAGE}-${USER}-dsk-lib \
    --hostname ${USER}-dsk-lib \
    --user $(stat -c "%u:%g" $HOME) \
    --security-opt seccomp:unconfined \
    --privileged \
    --cap-add=NET_ADMIN \
    --workdir ${PWD} \
    --device /dev/fuse \
    --device /dev/hugepages \
    --device /dev/net/tun:/dev/net/tun \
    --volume ${HOME}:${HOME} \
    --volume /etc/passwd:/etc/passwd \
    --volume /etc/group:/etc/group \
    --volume /etc/shadow:/etc/shadow \
    --volume /etc/sudoers:/etc/sudoers \
    --volume /var/hugepages:/var/hugepages \
    --volume /config/:/config \
    ${IMAGE_REMOTE}:${VERSION} "$@"

