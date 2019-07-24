FROM artifactory.f5net.com/velocity-platform-docker/platform-base-centos:7.5.2

RUN yum -y update

RUN yum -y install yum-utils

RUN yum -y groupinstall development

RUN yum -y install https://centos7.iuscommunity.org/ius-release.rpm


RUN update-ca-trust enable

COPY --from=artifactory.f5net.com/f5-proto-docker/f5-ca-bundle /f5-ca-bundle.crt /etc/pki/ca-trust/source/anchors/

RUN update-ca-trust extract

WORKDIR /root

COPY . .


CMD /bin/bash
