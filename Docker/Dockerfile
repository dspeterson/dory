FROM centos:7
MAINTAINER ben@perimeterx.com

ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8

RUN yum -y group mark convert
RUN yum -y groupinstall "Development Tools"
RUN yum -y install git libasan snappy-devel boost-devel xerces-c-devel rpm-build wget unzip socat zlib-devel
RUN wget "http://downloads.sourceforge.net/project/scons/scons/2.3.6/scons-2.3.6-1.noarch.rpm?r=http%3A%2F%2Fsourceforge.net%2Fprojects%2Fscons%2Ffiles%2Fscons%2F2.3.6%2F&ts=1439720375&use_mirror=skylineservers" -O scons.rpm && \
    rpm -i scons.rpm

RUN cd /root && \
    git clone https://github.com/dspeterson/dory.git && \
    cd dory && \
    cd src/dory && \
    scons -Q --up --release dory && \
    mkdir -p /opt/dory/bin/ && \
    cp /root/dory/out/release/dory/dory /opt/dory/bin/

RUN mkdir -p /etc/dory
ADD dory_conf.xml /etc/dory/
ADD start.sh /etc/dory/

EXPOSE 9090

CMD sh /etc/dory/start.sh
